#include "geometry_fp64.hlsli"
Sf64 precise_value(float hi,float lo) {
    return sf_add(sf_from_float_bits(asuint(hi)),sf_from_float_bits(asuint(lo)));
}
float2 precise_parts(Sf64 value) {
    uint hi=sf_to_float_bits(value);
    return float2(asfloat(hi),asfloat(sf_to_float_bits(sf_sub(value,sf_from_float_bits(hi)))));
}
struct Vertex { float3 coordinate; uint pose; };
struct Pose { float4 row0; float4 row1; float4 row2; float4 translation; float4 vanish; };
struct Result { float4 camera; float4 screen; };
precise float2 sum2(float a,float b) {
    precise float s=a+b,v=s-a;return float2(s,(a-(s-v))+(b-v));
}
precise float2 add2(float2 a,float2 b) {precise float2 s=sum2(a.x,b.x);return sum2(s.x,s.y+a.y+b.y);}
precise float2 product2(float a,float b) {
    precise float ca=4097*a,cb=4097*b,ah=ca-(ca-a),bh=cb-(cb-b),al=a-ah,bl=b-bh;
    precise float p=a*b;return float2(p,((ah*bh-p)+ah*bl+al*bh)+al*bl);
}
precise float2 multiply2(float2 a,float2 b) {precise float2 p=product2(a.x,b.x);return sum2(p.x,p.y+a.x*b.y+a.y*b.x+a.y*b.y);}
precise float2 divide2(float2 a,float2 b) {
    precise float q=a.x/b.x;
    precise float2 r=add2(a,-multiply2(float2(q,0),b));
    precise float2 result=sum2(q,(r.x+r.y)/b.x);
    r=add2(a,-multiply2(result,b));
    return add2(result,float2((r.x+r.y)/b.x,0));
}
precise float2 rounded64(float2 value) {
    // Rotation, translation and fragment addition are distinct double
    // operations in the source. Preserve their rounding before cancellation.
    int exponent=int((asuint(abs(value.x))>>23)&255U)-127;
    if(isfinite(value.x) && exponent>=-74) {
        float quantum=exp2(float(exponent-52));
        value.y=round(value.y/quantum)*quantum;
    }
    return value;
}
[[vk::binding(0,0)]] StructuredBuffer<Vertex> vertices : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<Pose> poses : register(t1,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Result> outputPoints : register(u0,space1);
[[vk::binding(1,1)]] RWStructuredBuffer<Result> outputResiduals : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) { uint count; uint poseCount; uint keepResiduals; uint padding; };
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    Vertex v=vertices[id.x];Result result;
    result.camera=float4(0,0,0,-1);result.screen=result.camera;
    Result tails;tails.camera=float4(0,0,0,-1);tails.screen=0;
    if(v.pose<poseCount) {
        Pose p=poses[v.pose];
        // Preserve operation order across source boundaries; no word rounding
        // or source projection saturation belongs in this continuous path.
        precise float3 camera=v.coordinate.x*p.row0.xyz+v.coordinate.y*p.row1.xyz
            +v.coordinate.z*p.row2.xyz+p.translation.xyz;
        float3 residual=0;
        Sf64 exactCamera[3];bool hasExactCamera=false;
        uint4 exactScreen=0;
        if(p.vanish.w==1 && v.pose+2<poseCount) {
            Pose low=poses[v.pose+2];
            for(uint c=0;c<3;++c) {
                precise float2 value=add2(add2(product2(v.coordinate.x,p.row0[c]),product2(v.coordinate.y,p.row1[c])),product2(v.coordinate.z,p.row2[c]));
                if(p.vanish.z>0) {
                    value=add2(value,float2(0,v.coordinate.x*low.row0[c]+v.coordinate.y*low.row1[c]+v.coordinate.z*low.row2[c]));
                    value=rounded64(value);
                    value=rounded64(add2(value,float2(p.translation[c],low.translation[c])));
                } else {
                    value=add2(value,float2(p.translation[c],low.translation[c]));
                    value=add2(value,float2(0,v.coordinate.x*low.row0[c]+v.coordinate.y*low.row1[c]+v.coordinate.z*low.row2[c]));
                }
                camera[c]=value.x;residual[c]=value.y;
            }
        }
        if(p.vanish.w==2 && poseCount>=6) {
            Pose e=poses[4],el=poses[5];
            float2 factor=float2(e.row2[v.pose],el.row2[v.pose]);
            precise float2 x=rounded64(multiply2(float2(v.coordinate.x,0),factor));
            precise float2 y=rounded64(multiply2(float2(v.coordinate.y,0),factor));
            precise float2 z=rounded64(multiply2(float2(v.coordinate.z,0),factor));
            precise float2 cx=float2(e.row0.x,el.row0.x),sx=float2(e.row1.x,el.row1.x);
            precise float2 cy=float2(e.row0.y,el.row0.y),sy=float2(e.row1.y,el.row1.y);
            precise float2 cz=float2(e.row0.z,el.row0.z),sz=float2(e.row1.z,el.row1.z);
            precise float2 y1=rounded64(add2(rounded64(multiply2(y,cx)),-rounded64(multiply2(z,sx))));
            precise float2 z1=rounded64(add2(rounded64(multiply2(y,sx)),rounded64(multiply2(z,cx))));
            precise float2 x2=rounded64(add2(rounded64(multiply2(x,cy)),rounded64(multiply2(z1,sy))));
            precise float2 z2=rounded64(add2(-rounded64(multiply2(x,sy)),rounded64(multiply2(z1,cy))));
            precise float2 x3=rounded64(add2(rounded64(multiply2(x2,cz)),-rounded64(multiply2(y1,sz))));
            precise float2 y3=rounded64(add2(rounded64(multiply2(x2,sz)),rounded64(multiply2(y1,cz))));
            Pose low=poses[v.pose+2];
            x3=rounded64(add2(x3,float2(p.translation.x,low.translation.x)));
            y3=rounded64(add2(y3,float2(p.translation.y,low.translation.y)));
            z2=rounded64(add2(z2,float2(p.translation.z,low.translation.z)));
            camera=float3(x3.x,y3.x,z2.x);residual=float3(x3.y,y3.y,z2.y);
            Sf64 scale64=precise_value(factor.x,factor.y);
            Sf64 ax=sf_mul(precise_value(v.coordinate.x,0),scale64);
            Sf64 ay=sf_mul(precise_value(v.coordinate.y,0),scale64);
            Sf64 az=sf_mul(precise_value(v.coordinate.z,0),scale64);
            Sf64 c0=precise_value(cx.x,cx.y),s0=precise_value(sx.x,sx.y);
            Sf64 c1=precise_value(cy.x,cy.y),s1=precise_value(sy.x,sy.y);
            Sf64 c2=precise_value(cz.x,cz.y),s2=precise_value(sz.x,sz.y);
            c0=sf_add(c0,precise_value(e.translation.x,0));s0=sf_add(s0,precise_value(el.translation.x,0));
            c1=sf_add(c1,precise_value(e.translation.y,0));s1=sf_add(s1,precise_value(el.translation.y,0));
            c2=sf_add(c2,precise_value(e.translation.z,0));s2=sf_add(s2,precise_value(el.translation.z,0));
            Sf64 by=sf_sub(sf_mul(ay,c0),sf_mul(az,s0));
            Sf64 bz=sf_add(sf_mul(ay,s0),sf_mul(az,c0));
            Sf64 bx=sf_add(sf_mul(ax,c1),sf_mul(bz,s1));
            Sf64 cz64=sf_add(sf_neg(sf_mul(ax,s1)),sf_mul(bz,c1));
            Sf64 cx64=sf_sub(sf_mul(bx,c2),sf_mul(by,s2));
            Sf64 cy64=sf_add(sf_mul(bx,s2),sf_mul(by,c2));
            exactCamera[0]=sf_add(cx64,precise_value(p.translation.x,low.translation.x));
            exactCamera[1]=sf_add(cy64,precise_value(p.translation.y,low.translation.y));
            exactCamera[2]=sf_add(cz64,precise_value(p.translation.z,low.translation.z));hasExactCamera=true;
            float2 px=precise_parts(exactCamera[0]);
            float2 py=precise_parts(exactCamera[1]);
            float2 pz=precise_parts(exactCamera[2]);
            camera=float3(px.x,py.x,pz.x);residual=float3(px.y,py.y,pz.y);
        }
        // Optional per-face destruction record, indexed plus one so ordinary
        // poses retain a zero payload. Normals never inherit vertex scaling.
        if(p.vanish.z>0 && uint(p.vanish.z)<poseCount) {
            uint normalIndex=uint(p.vanish.z)-1;
            Pose n=poses[normalIndex],nl=poses[normalIndex+1];
            float3 source=float3(-n.row0.w,n.row1.w,-n.row2.w);
            for(uint c=0;c<3;++c) {
                precise float direction;
                if(n.vanish.w==1) {
                    int value=(int(source.x)*int(n.row0[c]))>>15;
                    value+=(int(source.y)*int(n.row1[c]))>>15;
                    value+=(int(source.z)*int(n.row2[c]))>>15;
                    direction=float((value<<16)>>16);
                } else {
                    precise float2 value=add2(add2(product2(source.x,n.row0[c]),product2(source.y,n.row1[c])),product2(source.z,n.row2[c]));
                    value=add2(value,float2(0,source.x*nl.row0[c]+source.y*nl.row1[c]+source.z*nl.row2[c]));
                    direction=value.x+value.y;
                }
                if(c==1) direction=-abs(direction);
                int rounded=direction<0?-int(floor(-direction+0.5)):int(floor(direction+0.5));
                float offset=float((rounded*int(n.translation.w))>>2);
                if(hasExactCamera)exactCamera[c]=sf_add(exactCamera[c],precise_value(offset,0));
                precise float2 moved=rounded64(add2(float2(camera[c],residual[c]),float2(offset,0)));
                camera[c]=moved.x;residual[c]=moved.y;
            }
        }
        precise float depth=camera.z==0.0?1.0:camera.z;
        precise float2 screen=p.vanish.xy+camera.xy*p.translation.w/depth;
        float2 screenHigh=screen,screenTail=0;
        if(p.vanish.w>=1 && v.pose+2<poseCount) {
            for(uint c=0;c<2;++c) {
                precise float2 value;
                if(p.vanish.z>0 || p.vanish.w==2) {
                    // Select the operands before the expensive software-double
                    // division. The fallback result was discarded whenever
                    // exact camera coordinates were already available.
                    Sf64 coordinate=precise_value(camera[c],residual[c]);
                    Sf64 divisor=precise_value(depth,camera.z==0?0:residual.z);
                    if(hasExactCamera) {
                        coordinate=exactCamera[c];divisor=exactCamera[2];
                        if(sf_zero(divisor))divisor=precise_value(1,0);
                    }
                    Sf64 exact=sf_div(sf_mul(coordinate,precise_value(p.translation.w,0)),divisor);
                    exact=sf_add(precise_value(p.vanish[c],0),exact);
                    exactScreen[c*2]=exact.lo;exactScreen[c*2+1]=exact.hi;
                    value=precise_parts(exact);
                } else {
                    // Only the compensated path consumes this result. Exact
                    // projection above must not also pay for a discarded
                    // compensated multiply/divide and intermediate rounding.
                    precise float2 numerator=multiply2(float2(camera[c],residual[c]),float2(p.translation.w,0));
                    precise float2 projected=divide2(numerator,float2(depth,camera.z==0?0:residual.z));
                    value=add2(float2(p.vanish[c],0),projected);
                    // The source rounds each binary64 multiply/divide/add.
                    // Compensated float pairs retain an unrounded remainder
                    // which can cross a half-pixel after screen clipping.
                    // Pay for software-double only at a raster tie boundary.
                    if(value.y!=0 && value.x!=0 && frac(value.x*8)==0) {
                        Pose low=poses[v.pose+2];
                        Sf64 exactCamera[3];
                        for(uint axis=0;axis<3;++axis) {
                            Sf64 xterm=sf_mul(sf_from_float_bits(asuint(v.coordinate.x)),
                                precise_value(p.row0[axis],low.row0[axis]));
                            Sf64 yterm=sf_mul(sf_from_float_bits(asuint(v.coordinate.y)),
                                precise_value(p.row1[axis],low.row1[axis]));
                            Sf64 zterm=sf_mul(sf_from_float_bits(asuint(v.coordinate.z)),
                                precise_value(p.row2[axis],low.row2[axis]));
                            Sf64 translation=sf_add(sf_add(sf_from_float_bits(asuint(p.translation[axis])),
                                sf_from_float_bits(asuint(low.translation[axis]))),
                                sf_from_float_bits(asuint(low.vanish[axis])));
                            exactCamera[axis]=sf_add(sf_add(sf_add(xterm,yterm),zterm),translation);
                        }
                        Sf64 divisor=exactCamera[2];
                        if(sf_zero(divisor))divisor=sf_from_float_bits(asuint(1.f));
                        Sf64 exact=sf_add(precise_value(p.vanish[c],0),
                            sf_div(sf_mul(exactCamera[c],precise_value(p.translation.w,0)),divisor));
                        if(sf_valid(exact))value=precise_parts(exact);
                    }
                }
                screen[c]=value.x+value.y;
                if(p.vanish.z>0 || p.vanish.w==2) {
                    value=rounded64(value);
                    screen[c]=value.x+value.y;
                    // At supported 1x/2x/4x pixel-rounding boundaries, retain
                    // the side of the double result when narrowing to float.
                    // Otherwise an exactly representable half-pixel swallows
                    // the tail and changes the source's final integer vertex.
                    if(screen[c]==value.x && value.y!=0 && value.x!=0
                        && frac(value.x*8)==0) {
                        uint bits=asuint(value.x);
                        bool increase=(value.y>0)==(value.x>0);
                        screen[c]=asfloat(increase?bits+1:bits-1);
                    }
                }
                if(keepResiduals!=0) {
                    screenHigh[c]=value.x;screenTail[c]=value.y;
                }
            }
        }
        if(all(isfinite(camera)) && all(isfinite(screen))) {
            result.camera=float4(camera,1);
            result.screen=float4(screen,camera.z,camera.z>=0.0?1.0:0.0);
            tails.camera=float4(residual,1);tails.screen=float4(screenHigh,screenTail);
            // Format 2 carries raw binary64 screen X/Y words, not floats.
            if(p.vanish.z>0 || p.vanish.w==2) {tails.camera.w=2;tails.screen=asfloat(exactScreen);}
            if(keepResiduals==2) {
                for(uint c=0;c<3;++c) {
                    Sf64 exact=precise_value(camera[c],residual[c]);
                    if(hasExactCamera)exact=exactCamera[c];
                    tails.camera[c]=asfloat(exact.lo);
                    tails.screen[c]=asfloat(exact.hi);
                }
                tails.camera.w=3;tails.screen.w=0;
            }
        }
    }
    outputPoints[id.x]=result;
    if(keepResiduals!=0) outputResiduals[id.x]=tails;
}
