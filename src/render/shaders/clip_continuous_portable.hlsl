// Fractional screen clipping. Near-plane intersections must be processed
// before this stage: status 3 explicitly requests that path, never projects
// a behind-camera polygon as though it were entirely in front.
#ifndef STARFOX_CLIP_CAPACITY
#define STARFOX_CLIP_CAPACITY 128
#endif
#include "geometry_fp64.hlsli"
Sf64 exact_pair(float hi,float lo) {
    return sf_add(sf_from_float_bits(asuint(hi)),sf_from_float_bits(asuint(lo)));
}
float2 exact_parts(Sf64 value) {
    uint hi=sf_to_float_bits(value);
    return float2(asfloat(hi),asfloat(sf_to_float_bits(sf_sub(value,sf_from_float_bits(hi)))));
}
struct Point {float4 camera;float4 screen;};
Sf64 raw_camera(Point p,uint c) {return sf_make(asuint(p.camera[c]),asuint(p.screen[c]));}
Sf64 project_exact(Sf64 coordinate,Sf64 depth,float vanish,float focal) {
    if(sf_zero(depth))depth=exact_pair(1,0);
    return sf_add(exact_pair(vanish,0),sf_div(sf_mul(coordinate,exact_pair(focal,0)),depth));
}
struct Accurate {float4 hi;float4 lo;};
struct ExactVertex {Sf64 v[4];};
bool exact_inside(Sf64 value,float boundary,bool less) {
    Sf64 difference=sf_sub(value,exact_pair(boundary,0));
    bool below=!sf_zero(difference) && (difference.hi&0x80000000U)!=0;
    return less?below:!below;
}
precise float2 sum2(float a,float b) {
    precise float s=a+b,v=s-a,e=(a-(s-v))+(b-v);return float2(s,e);
}
precise float2 add2(float2 a,float2 b) {
    precise float2 s=sum2(a.x,b.x);return sum2(s.x,(a.y+b.y)+s.y);
}
precise float2 product2(float a,float b) {
    precise float ca=4097*a,cb=4097*b,ah=ca-(ca-a),bh=cb-(cb-b),al=a-ah,bl=b-bh;
    precise float p=a*b,e=((ah*bh-p)+ah*bl+al*bh)+al*bl;return float2(p,e);
}
precise float2 multiply2(float2 a,float2 b) {
    precise float2 p=product2(a.x,b.x);
    return sum2(p.x,p.y+(a.x*b.y+a.y*b.x)+a.y*b.y);
}
precise float2 divide2(float2 a,float2 b) {
    precise float q=a.x/b.x;
    precise float2 remainder=add2(a,-multiply2(float2(q,0),b));
    precise float2 result=sum2(q,(remainder.x+remainder.y)/b.x);
    remainder=add2(a,-multiply2(result,b));
    return add2(result,float2((remainder.x+remainder.y)/b.x,0));
}
bool inside(Accurate v,uint axis,float boundary,bool less) {
    bool below=v.hi[axis]<boundary || (v.hi[axis]==boundary && v.lo[axis]<0);
    return less?below:!below;
}
[[vk::binding(0,0)]] StructuredBuffer<Point> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> corners : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint4> polygons : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<uint> visibility : register(t3,space0);
[[vk::binding(4,0)]] StructuredBuffer<float4> projectionParams : register(t4,space0);
[[vk::binding(5,0)]] StructuredBuffer<Point> pointResiduals : register(t5,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<float4> clipped : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint polygonCount;uint pointCount;uint cornerCount;uint visibilityCount;
    int width;int height;uint projectionCount;uint residualCount;
};
// Header is bitwise int4(count,status,0,0), payload is float4(X,Y,U,V).
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=polygonCount) return;
    uint base=id.x*129U;clipped[base]=0;
    uint4 descriptor=polygons[id.x];
    if(descriptor.z>=visibilityCount || descriptor.x>cornerCount
        || descriptor.y>cornerCount-descriptor.x || descriptor.y>32U) {
        clipped[base]=asfloat(int4(0,1,0,0));return;
    }
    bool isLine=(descriptor.w&2U)!=0 && descriptor.y==2;
    if((descriptor.w&5U)==5U && descriptor.y==1 && visibility[descriptor.z]!=0) {
        uint index=corners[descriptor.x].x;
        if(index>=pointCount) {clipped[base]=asfloat(int4(0,1,0,0));return;}
        Point p=points[index];
        if(p.camera.w>=0 && p.screen.w>=0 && p.camera.z>=32 && all(isfinite(p.screen))) {
            clipped[base+1]=float4(p.screen.xy,p.camera.z,0);clipped[base]=asfloat(int4(1,0,2,0));
        }
        return;
    }
    if(visibility[descriptor.z]==0 || (descriptor.y<3 && !isLine)) return;
    Accurate work[STARFOX_CLIP_CAPACITY],scratch[STARFOX_CLIP_CAPACITY];uint size=descriptor.y;
    // Fully interior polygons already carry the producer's source-rounded
    // screen coordinates. Re-projecting their narrowed camera values can
    // cross a raster rounding boundary without any clipping being necessary.
    // Lines keep the existing intersection path, including endpoint ordering.
    bool needsClip=true;
    if(!isLine && residualCount==0 && id.x<projectionCount && projectionParams[id.x].w==1) {
        needsClip=false;
        for(uint j=0;j<size;++j) {
            uint index=corners[descriptor.x+j].x;
            if(index>=pointCount) {clipped[base]=asfloat(int4(0,1,0,0));return;}
            Point candidate=points[index];
            if(candidate.camera.z<0 || candidate.screen.x<0 || candidate.screen.x>=width
                || candidate.screen.y<0 || candidate.screen.y>=height) {
                needsClip=true;break;
            }
        }
    }
    float3 camera[32],cameraTail[32];bool behind=false;uint frontCount=0;
    for(uint i=0;i<size;++i) {
        uint4 corner=corners[descriptor.x+i];
        if(corner.x>=pointCount) {clipped[base]=asfloat(int4(0,1,0,0));return;}
        Point p=points[corner.x];
        if(p.camera.w<0 || p.screen.w<0 || !all(isfinite(p.screen)) || !all(isfinite(p.camera))) {
            clipped[base]=asfloat(int4(0,1,0,0));return;
        }
        camera[i]=p.camera.xyz;cameraTail[i]=0;behind=behind || p.camera.z<0;frontCount+=p.camera.z>=0?1U:0U;
        work[i].hi=float4(p.screen.xy,float(asint(corner.y)),float(asint(corner.z)));work[i].lo=0;
        if(residualCount!=0) {
            if(corner.x>=residualCount || pointResiduals[corner.x].camera.w<0
                || (pointResiduals[corner.x].camera.w!=3 && ((pointResiduals[corner.x].camera.w!=2 && !all(isfinite(pointResiduals[corner.x].screen))) || !all(isfinite(pointResiduals[corner.x].camera))))) {
                clipped[base]=asfloat(int4(0,1,0,0));return;
            }
            if(pointResiduals[corner.x].camera.w==3) {
                if(id.x>=projectionCount) {clipped[base]=asfloat(int4(0,3,0,0));return;}
                Point raw=pointResiduals[corner.x];
                for(uint c=0;c<3;++c) {
                    Sf64 exact=raw_camera(raw,c);
                    if(!sf_valid(exact)) {clipped[base]=asfloat(int4(0,1,0,0));return;}
                    float2 parts=exact_parts(exact);camera[i][c]=parts.x;cameraTail[i][c]=parts.y;
                }
                for(uint c=0;c<2;++c) {
                    float2 parts=exact_parts(project_exact(raw_camera(raw,c),raw_camera(raw,2),projectionParams[id.x][c],projectionParams[id.x].z));
                    work[i].hi[c]=parts.x;work[i].lo[c]=parts.y;
                }
            } else {
            cameraTail[i]=pointResiduals[corner.x].camera.xyz;
            for(uint c=0;c<2;++c) {
                precise float2 value=sum2(pointResiduals[corner.x].screen[c],pointResiduals[corner.x].screen[c+2]);
                if(pointResiduals[corner.x].camera.w==2) {
                    uint4 raw=asuint(pointResiduals[corner.x].screen);
                    Sf64 exact=sf_make(raw[c*2],raw[c*2+1]);
                    if(!sf_valid(exact)) {clipped[base]=asfloat(int4(0,1,0,0));return;}
                    uint hi=sf_to_float_bits(exact);
                    value=float2(asfloat(hi),asfloat(sf_to_float_bits(sf_sub(exact,sf_from_float_bits(hi)))));
                }
                work[i].hi[c]=value.x;work[i].lo[c]=value.y;
            }
            }
        }
        // Model packets opt into compensated projection. A device division
        // can put an exact half-pixel just below its rounding boundary;
        // retain the residual through clipping instead of baking that error in.
        if(needsClip && residualCount==0 && id.x<projectionCount && projectionParams[id.x].w==1) {
            float4 parameters=projectionParams[id.x];
            float depth=p.camera.z==0?1:p.camera.z;
            for(uint c=0;c<2;++c) {
                precise float2 screen=add2(float2(parameters[c],0),divide2(
                    multiply2(float2(p.camera[c],0),float2(parameters.z,0)),float2(depth,0)));
                screen=exact_parts(sf_add(exact_pair(parameters[c],0),sf_div(sf_mul(exact_pair(p.camera[c],0),exact_pair(parameters.z,0)),exact_pair(depth,0))));
                work[i].hi[c]=screen.x;work[i].lo[c]=screen.y;
            }
        }
    }
    if(behind) {
        // MOBJ declines 3D clipping for texture maps. Descriptor bit 0 marks
        // a textured polygon; wholly behind solid polygons also disappear.
        if((descriptor.w&1U)!=0 || frontCount==0) return;
        if(id.x>=projectionCount) {clipped[base]=asfloat(int4(0,3,0,0));return;}
        float4 parameters=projectionParams[id.x]; // vanish X/Y, focal length, reserved
        if(!all(isfinite(parameters))) {clipped[base]=asfloat(int4(0,1,0,0));return;}
        uint nextSize=0;
        if(isLine) {
            scratch[0].hi=float4(camera[0],0);scratch[0].lo=float4(cameraTail[0],0);
            scratch[1].hi=float4(camera[1],0);scratch[1].lo=float4(cameraTail[1],0);nextSize=2;
            float3 a=camera[0],b=camera[1];uint outside=a.z<0?0:1;
            precise float2 amount=divide2(float2(a.z,cameraTail[0].z),add2(float2(a.z,cameraTail[0].z),-float2(b.z,cameraTail[1].z)));
            Sf64 nearA=exact_pair(a.z,cameraTail[0].z),nearB=exact_pair(b.z,cameraTail[1].z);
            Sf64 nearAmount=sf_div(nearA,sf_sub(nearA,nearB));
            scratch[outside].hi=0;scratch[outside].lo=0;
            for(uint c=0;c<2;++c) {
                precise float2 first=float2(a[c],cameraTail[0][c]),last=float2(b[c],cameraTail[1][c]);
                precise float2 value=add2(first,multiply2(add2(last,-first),amount));
                if(parameters.w!=0)value=exact_parts(sf_add(exact_pair(first.x,first.y),sf_mul(sf_sub(exact_pair(last.x,last.y),exact_pair(first.x,first.y)),nearAmount)));
                scratch[outside].hi[c]=value.x;scratch[outside].lo[c]=value.y;
            }
        }
        // Same current/next traversal as clip_near_polygon, retaining order.
        else for(uint i=0;i<size;++i) {
            float3 a=camera[i],b=camera[(i+1)%size];
            float3 at=cameraTail[i],bt=cameraTail[(i+1)%size];
            if(a.z>=0) {scratch[nextSize].hi=float4(a,0);scratch[nextSize++].lo=float4(at,0);}
            if((a.z>=0)!=(b.z>=0)) {
                precise float2 amount=divide2(float2(a.z,at.z),add2(float2(a.z,at.z),-float2(b.z,bt.z)));
                Sf64 nearA=exact_pair(a.z,at.z),nearB=exact_pair(b.z,bt.z);
                Sf64 nearAmount=sf_div(nearA,sf_sub(nearA,nearB));
                Accurate intersection;intersection.hi=0;intersection.lo=0;
                for(uint c=0;c<2;++c) {
                    precise float2 first=float2(a[c],at[c]),last=float2(b[c],bt[c]);
                    precise float2 value=add2(first,multiply2(add2(last,-first),amount));
                    if(parameters.w!=0)value=exact_parts(sf_add(exact_pair(first.x,first.y),sf_mul(sf_sub(exact_pair(last.x,last.y),exact_pair(first.x,first.y)),nearAmount)));
                    intersection.hi[c]=value.x;intersection.lo[c]=value.y;
                }
                scratch[nextSize++]=intersection;
            }
        }
        size=nextSize;
        for(uint i=0;i<size;++i) {
            Accurate vertex=scratch[i];work[i].hi=0;work[i].lo=0;
            float2 depth=float2(vertex.hi.z,vertex.lo.z);
            if(all(depth==0)) depth=float2(1,0);
            for(uint c=0;c<2;++c) {
                precise float2 screen=add2(float2(parameters[c],0),divide2(
                    multiply2(float2(vertex.hi[c],vertex.lo[c]),float2(parameters.z,0)),depth));
                work[i].hi[c]=screen.x;work[i].lo[c]=screen.y;
            }
        }
    }
    // Two floats do not retain all 53 source bits. At a clipped half-pixel,
    // narrowing to that pair before intersecting can choose the opposite
    // scanline even when both input camera coordinates were exact. Reuse the
    // binary64 polygon path for boundary intersections, not just backfaces.
    bool rawPolygon=residualCount!=0 && pointResiduals[corners[descriptor.x].x].camera.w==3;
    bool exactProjection=residualCount!=0 || (id.x<projectionCount && projectionParams[id.x].w!=0);
    bool screenClip=behind;
    if(exactProjection && !isLine) for(uint i=0;i<size;++i)
        screenClip=screenClip || !inside(work[i],0,0,false) || !inside(work[i],0,float(width),true)
            || !inside(work[i],1,0,false) || !inside(work[i],1,float(height),true);
    if(!isLine && ((descriptor.w&8U)!=0 || (exactProjection && screenClip))) {
        Sf64 area=sf_make(0,0);
        // Near-plane clipping emits at most two vertices per input corner.
        // The descriptor is capped at 32 corners, so these pre-screen arrays
        // need only 64 entries.
        Sf64 ax[64],ay[64];
        if(rawPolygon) {
            uint produced=0;
            [loop] for(uint i=0;i<descriptor.y;++i) {
                Point a=pointResiduals[corners[descriptor.x+i].x];
                Point b=pointResiduals[corners[descriptor.x+(i+1)%descriptor.y].x];
                if(a.camera.w!=3 || b.camera.w!=3) {clipped[base]=asfloat(int4(0,1,0,0));return;}
                Sf64 az=raw_camera(a,2),bz=raw_camera(b,2);
                bool frontA=sf_zero(az) || (az.hi&0x80000000U)==0;
                bool frontB=sf_zero(bz) || (bz.hi&0x80000000U)==0;
                [loop] for(uint emit=0;emit<2;++emit) {
                    if((emit==0 && !frontA) || (emit==1 && frontA==frontB))continue;
                    Sf64 z=az,amount=sf_make(0,0);
                    if(emit==1) {amount=sf_div(az,sf_sub(az,bz));z=sf_make(0,0);}
                    for(uint c=0;c<2;++c) {
                        Sf64 coordinate=raw_camera(a,c);
                        if(emit==1)coordinate=sf_add(coordinate,sf_mul(sf_sub(raw_camera(b,c),coordinate),amount));
                        Sf64 screen=project_exact(coordinate,z,projectionParams[id.x][c],projectionParams[id.x].z);
                        if(!sf_valid(screen))return;
                        if(c==0)ax[produced]=screen;else ay[produced]=screen;
                        float2 parts=exact_parts(screen);work[produced].hi[c]=parts.x;work[produced].lo[c]=parts.y;
                    }
                    ++produced;
                }
            }
            if(produced!=size) {clipped[base]=asfloat(int4(0,1,0,0));return;}
        } else for(uint i=0;i<size;++i) {
            ax[i]=exact_pair(work[i].hi.x,work[i].lo.x);
            ay[i]=exact_pair(work[i].hi.y,work[i].lo.y);
            if(behind && id.x<projectionCount) {
                float4 parameters=projectionParams[id.x];
                Sf64 depth64=exact_pair(scratch[i].hi.z,scratch[i].lo.z);
                if(sf_zero(depth64))depth64=exact_pair(1,0);
                ax[i]=sf_add(exact_pair(parameters.x,0),sf_div(sf_mul(exact_pair(scratch[i].hi.x,scratch[i].lo.x),exact_pair(parameters.z,0)),depth64));
                ay[i]=sf_add(exact_pair(parameters.y,0),sf_div(sf_mul(exact_pair(scratch[i].hi.y,scratch[i].lo.y),exact_pair(parameters.z,0)),depth64));
            }
            if(!behind && id.x<projectionCount && projectionParams[id.x].w!=0) {
                float4 parameters=projectionParams[id.x];
                Sf64 depth64=exact_pair(camera[i].z==0?1:camera[i].z,camera[i].z==0?0:cameraTail[i].z);
                ax[i]=sf_add(exact_pair(parameters.x,0),sf_div(sf_mul(exact_pair(camera[i].x,cameraTail[i].x),exact_pair(parameters.z,0)),depth64));
                ay[i]=sf_add(exact_pair(parameters.y,0),sf_div(sf_mul(exact_pair(camera[i].y,cameraTail[i].y),exact_pair(parameters.z,0)),depth64));
            }
            if(!behind && residualCount!=0 && pointResiduals[corners[descriptor.x+i].x].camera.w==2) {
                uint4 raw=asuint(pointResiduals[corners[descriptor.x+i].x].screen);
                ax[i]=sf_make(raw.x,raw.y);ay[i]=sf_make(raw.z,raw.w);
            }
        }
        if((descriptor.w&8U)!=0) for(uint i=0;i<size;++i) {
            uint j=(i+1)%size;
            area=sf_add(area,sf_sub(sf_mul(ax[i],ay[j]),sf_mul(ax[j],ay[i])));
        }
        if((descriptor.w&8U)!=0 && (!sf_valid(area) || sf_zero(area) || (area.hi&0x80000000U)==0))return;
        if(rawPolygon || (exactProjection && screenClip)) {
            ExactVertex vertices64[STARFOX_CLIP_CAPACITY],scratch64[STARFOX_CLIP_CAPACITY];
            [loop] for(uint i=0;i<size;++i) {
                vertices64[i].v[0]=ax[i];vertices64[i].v[1]=ay[i];
                vertices64[i].v[2]=exact_pair(work[i].hi.z,work[i].lo.z);
                vertices64[i].v[3]=exact_pair(work[i].hi.w,work[i].lo.w);
            }
            [loop] for(uint plane=0;plane<4 && size>0;++plane) {
                uint axis=plane/2;bool upper=(plane&1U)!=0;
                float boundary=upper?float(axis==0?width:height):0.f;
                uint nextSize=0;ExactVertex previous=vertices64[size-1];
                bool previousInside=exact_inside(previous.v[axis],boundary,upper);
                [loop] for(uint i=0;i<size;++i) {
                    ExactVertex current=vertices64[i];
                    bool currentInside=exact_inside(current.v[axis],boundary,upper);
                    if(previousInside!=currentInside) {
                        Sf64 denominator=sf_sub(current.v[axis],previous.v[axis]);
                        Sf64 amount=sf_make(0,0);
                        if(!sf_zero(denominator))amount=sf_div(sf_sub(exact_pair(boundary,0),previous.v[axis]),denominator);
                        ExactVertex intersection;
                        [loop] for(uint c=0;c<4;++c)
                            intersection.v[c]=sf_add(previous.v[c],sf_mul(sf_sub(current.v[c],previous.v[c]),amount));
                        if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=asfloat(int4(0,2,0,0));return;}
                        scratch64[nextSize++]=intersection;
                    }
                    if(currentInside) {
                        if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=asfloat(int4(0,2,0,0));return;}
                        scratch64[nextSize++]=current;
                    }
                    previous=current;previousInside=currentInside;
                }
                size=nextSize;[loop] for(uint i=0;i<size;++i)vertices64[i]=scratch64[i];
            }
            [loop] for(uint i=0;i<size;++i) {
                float4 value=0;
                [loop] for(uint c=0;c<4;++c) {
                    if(!sf_valid(vertices64[i].v[c]))return;
                    float2 parts=exact_parts(vertices64[i].v[c]);value[c]=parts.x;
                    if(c<2 && parts.y!=0 && parts.x!=0 && frac(parts.x*8)==0) {
                        uint raw=asuint(parts.x);bool increase=(parts.y>0)==(parts.x>0);
                        value[c]=asfloat(increase?raw+1:raw-1);
                    }
                }
                clipped[base+i+1]=value;
            }
            clipped[base]=asfloat(int4(size,0,0,0));return;
        }
    }
    if(isLine) {
        if(id.x<projectionCount && projectionParams[id.x].w!=0) {
            Sf64 origin[2],delta64[2];
            for(uint c=0;c<2;++c) {
                Sf64 values[2];
                for(uint i=0;i<2;++i) {
                    values[i]=exact_pair(work[i].hi[c],work[i].lo[c]);
                    uint index=corners[descriptor.x+i].x;
                    if(!behind && residualCount!=0 && pointResiduals[index].camera.w==2) {
                        uint4 raw=asuint(pointResiduals[index].screen);
                        values[i]=sf_make(raw[c*2],raw[c*2+1]);
                    }
                }
                origin[c]=values[0];delta64[c]=sf_sub(values[1],values[0]);
            }
            Sf64 enter64=exact_pair(0,0),leave64=exact_pair(1,0);
            for(uint plane=0;plane<4;++plane) {
                uint axis=plane/2;bool upper=(plane&1U)!=0;
                Sf64 direction=delta64[axis],distance=origin[axis];
                if(upper) distance=sf_sub(exact_pair(axis==0?width:height,0),distance);
                else direction=sf_neg(direction);
                if(sf_zero(direction)) {
                    if(!sf_zero(distance) && (distance.hi&0x80000000U)!=0)return;
                } else {
                    Sf64 amount=sf_div(distance,direction);
                    if(!sf_valid(amount))return;
                    bool negative=(direction.hi&0x80000000U)!=0;
                    Sf64 selected=leave64;if(negative)selected=enter64;
                    Sf64 difference=sf_sub(amount,selected);
                    bool greater=!sf_zero(difference) && (difference.hi&0x80000000U)==0;
                    if(negative && greater)enter64=amount;
                    if(!negative && !greater)leave64=amount;
                    difference=sf_sub(enter64,leave64);
                    if(!sf_zero(difference) && (difference.hi&0x80000000U)==0)return;
                }
            }
            float4 endpoints64[2];endpoints64[0]=endpoints64[1]=0;
            for(uint i=0;i<2;++i)for(uint c=0;c<2;++c) {
                Sf64 amount=leave64;if(i==0)amount=enter64;
                Sf64 exact=sf_add(origin[c],sf_mul(delta64[c],amount));
                if(!sf_valid(exact))return;
                float2 value=exact_parts(exact);
                float narrowed=value.x;
                if(value.y!=0 && value.x!=0 && frac(value.x*8)==0) {
                    uint bits=asuint(value.x);bool increase=(value.y>0)==(value.x>0);
                    narrowed=asfloat(increase?bits+1:bits-1);
                }
                endpoints64[i][c]=narrowed;
            }
            clipped[base+1]=endpoints64[0];clipped[base+2]=endpoints64[1];clipped[base]=asfloat(int4(2,0,1,0));return;
        }
        precise float2 enter=float2(0,0),leave=float2(1,0);
        for(uint plane=0;plane<4;++plane) {
            uint axis=plane/2;bool upper=(plane&1U)!=0;
            precise float2 first=float2(work[0].hi[axis],work[0].lo[axis]);
            precise float2 delta=add2(float2(work[1].hi[axis],work[1].lo[axis]),-first);
            precise float2 direction=upper?delta:-delta;
            precise float2 distance=upper?add2(float2(axis==0?width:height,0),-first):first;
            if(all(direction==0)) {
                if(distance.x<0 || (distance.x==0 && distance.y<0)) return;
            } else {
                precise float2 amount=divide2(distance,direction);
                if(id.x<projectionCount && projectionParams[id.x].w!=0)
                    amount=exact_parts(sf_div(exact_pair(distance.x,distance.y),exact_pair(direction.x,direction.y)));
                bool negative=direction.x<0 || (direction.x==0 && direction.y<0);
                precise float2 difference=add2(amount,-(negative?enter:leave));
                bool greater=difference.x>0 || (difference.x==0 && difference.y>0);
                if(negative && greater) enter=amount;
                if(!negative && !greater) leave=amount;
                difference=add2(enter,-leave);
                if(difference.x>0 || (difference.x==0 && difference.y>0)) return;
            }
        }
        float4 endpoints[2];endpoints[0]=endpoints[1]=0;
        for(uint i=0;i<2;++i) for(uint c=0;c<2;++c) {
            precise float2 first=float2(work[0].hi[c],work[0].lo[c]);
            precise float2 delta=add2(float2(work[1].hi[c],work[1].lo[c]),-first);
            precise float2 value=add2(first,multiply2(delta,i==0?enter:leave));endpoints[i][c]=value.x+value.y;
            if(id.x<projectionCount && projectionParams[id.x].w!=0) {
                float2 selected=i==0?enter:leave;
                value=exact_parts(sf_add(exact_pair(first.x,first.y),sf_mul(exact_pair(delta.x,delta.y),exact_pair(selected.x,selected.y))));
                endpoints[i][c]=value.x+value.y;
            }
            if(residualCount!=0 && endpoints[i][c]==value.x && value.y!=0 && value.x!=0 && frac(value.x*8)==0) {
                uint bits=asuint(value.x);bool increase=(value.y>0)==(value.x>0);
                endpoints[i][c]=asfloat(increase?bits+1:bits-1);
            }
        }
        clipped[base+1]=endpoints[0];clipped[base+2]=endpoints[1];clipped[base]=asfloat(int4(2,0,1,0));return;
    }
    for(uint plane=0;plane<4 && size>0;++plane) {
        uint axis=plane<2?0:1;
        float boundary=(plane&1U)==0?0.0:float(axis==0?width:height);
        bool keepLess=(plane&1U)!=0;
        uint nextSize=0;Accurate previous=work[size-1];
        bool previousInside=inside(previous,axis,boundary,keepLess);
        for(uint i=0;i<size;++i) {
            Accurate current=work[i];
            bool currentInside=inside(current,axis,boundary,keepLess);
            if(currentInside!=previousInside) {
                precise float2 previousAxis=float2(previous.hi[axis],previous.lo[axis]);
                precise float2 denominator=add2(float2(current.hi[axis],current.lo[axis]),-previousAxis);
                precise float2 amount=all(denominator==0)?float2(0,0):divide2(add2(float2(boundary,0),-previousAxis),denominator);
                Accurate intersection;
                for(uint component=0;component<4;++component) {
                    precise float2 a=float2(previous.hi[component],previous.lo[component]);
                    precise float2 b=float2(current.hi[component],current.lo[component]);
                    precise float2 result=add2(a,multiply2(add2(b,-a),amount));
                    if(id.x<projectionCount && projectionParams[id.x].w!=0) {
                        Sf64 first64=exact_pair(previous.hi[axis],previous.lo[axis]);
                        Sf64 denominator64=sf_sub(exact_pair(current.hi[axis],current.lo[axis]),first64);
                        Sf64 amount64=sf_make(0,0);
                        if(!sf_zero(denominator64))amount64=sf_div(sf_sub(exact_pair(boundary,0),first64),denominator64);
                        Sf64 a64=exact_pair(a.x,a.y),b64=exact_pair(b.x,b.y);
                        result=exact_parts(sf_add(a64,sf_mul(sf_sub(b64,a64),amount64)));
                    }
                    intersection.hi[component]=result.x;intersection.lo[component]=result.y;
                }
                // The intersected coordinate is known exactly. Retaining a
                // cancellation residual here can amplify UV error when the
                // next plane clips a very short edge at a viewport corner.
                intersection.hi[axis]=boundary;intersection.lo[axis]=0;
                if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=asfloat(int4(0,2,0,0));return;}
                scratch[nextSize++]=intersection;
            }
            if(currentInside) {
                if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=asfloat(int4(0,2,0,0));return;}
                scratch[nextSize++]=current;
            }
            previous=current;previousInside=currentInside;
        }
        size=nextSize;for(uint i=0;i<size;++i) work[i]=scratch[i];
    }
    for(uint i=0;i<size;++i) {
        float4 value=work[i].hi+work[i].lo;
        if(residualCount!=0 || (id.x<projectionCount && projectionParams[id.x].w==1)) for(uint c=0;c<2;++c) {
            if(value[c]==work[i].hi[c] && work[i].lo[c]!=0 && value[c]!=0 && frac(value[c]*8)==0) {
                uint bits=asuint(value[c]);bool increase=(work[i].lo[c]>0)==(value[c]>0);
                value[c]=asfloat(increase?bits+1:bits-1);
            }
        }
        clipped[base+1+i]=value;
    }
    clipped[base]=asfloat(int4(size,0,0,0));
}
