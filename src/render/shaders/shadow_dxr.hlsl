RaytracingAccelerationStructure scene : register(t0);
ByteAddressBuffer coverage : register(t1);
ByteAddressBuffer triangleVertices : register(t2);
ByteAddressBuffer residentMaterials : register(t3);
ByteAddressBuffer backdropPixels : register(t4);
RWByteAddressBuffer outputMask : register(u0);
cbuffer Settings : register(b0) {
    float4 camera; // width, height, focal length, center x
    float4 options; // center y, ground enabled
    float4 groundPoint;
    float4 groundNormal;
    float4 lights[8];
};

bool covered(uint primitive, float2 bary) {
    uint record = 16 + primitive * 40;
    if (primitive >= coverage.Load(0)) return false;
    if (coverage.Load(record + 36) == 0) return true;
    float2 a = asfloat(coverage.Load2(record));
    float2 b = asfloat(coverage.Load2(record + 8));
    float2 c = asfloat(coverage.Load2(record + 16));
    float2 uv = a * (1 - bary.x - bary.y) + b * bary.x + c * bary.y;
    uint2 mask = coverage.Load2(record + 28);
    uint2 xy = uint2(int2(floor(uv))) & mask;
    uint index = coverage.Load(record + 24) + xy.y * (mask.x + 1) + xy.x;
    return (coverage.Load(coverage.Load(8) + index * 4) >> 24) != 0;
}

uint ray_material_word(uint at) {
    return (coverage.Load(12)&2u)?residentMaterials.Load(at):coverage.Load(16+at);
}
uint2 ray_material_pair(uint at) {return uint2(ray_material_word(at),ray_material_word(at+4));}
uint reflected_colour(uint primitive,float2 bary,uint2 pixel) {
    uint record=primitive*64,index;
    if(ray_material_word(record+60)!=0) return 0;
    uint textured=ray_material_word(record+24);
    if(textured>1) return 0;
    if(textured) {
        float2 a=asfloat(ray_material_pair(record)),b=asfloat(ray_material_pair(record+8)),c=asfloat(ray_material_pair(record+16));
        uint2 mask=ray_material_pair(record+52);uint offset=ray_material_word(record+48);
        uint count=coverage.Load(coverage.Load(4)+1068);
        if(any(mask>4095) || any(mask&(mask+1)) || offset>count || (mask.x+1)*(mask.y+1)>count-offset) return 0;
        if(!all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(c))
            || any(abs(a)>1e8) || any(abs(b)>1e8) || any(abs(c)>1e8)) return 0;
        uint2 xy=uint2(int2(floor(a*(1-bary.x-bary.y)+b*bary.x+c*bary.y)))&mask;
        uint at=offset+xy.y*(mask.x+1)+xy.x;
        index=coverage.Load(coverage.Load(8)+at*4);
        if(!index) return 0;
        index=(index+ray_material_word(record+40))&255;
    } else {
        bool odd=ray_material_word(record+28)!=0 && ((pixel.x+pixel.y)&1)!=0;
        index=ray_material_word(record+(odd?36:32));
    }
    if(index>255) return 0;
    return coverage.Load(coverage.Load(4)+index*4)|0xff000000u;
}
float3 reflection_rgb(uint packed) {
    return float3(packed&255u,(packed>>8)&255u,(packed>>16)&255u)/255.;
}
void reflection_cube_coordinates(float3 direction,out uint face,out float2 uv) {
    float3 a=abs(direction);
    if(a.x>=a.y && a.x>=a.z) {
        face=direction.x>=0?0:1;uv=float2(direction.x>=0?-direction.z:direction.z,-direction.y)/a.x;
    } else if(a.y>=a.z) {
        face=direction.y>=0?2:3;uv=float2(direction.x,direction.y>=0?direction.z:-direction.z)/a.y;
    } else {
        face=direction.z>=0?4:5;uv=float2(direction.z>=0?direction.x:-direction.x,-direction.y)/a.z;
    }
}
float3 reflection_cube_tap(uint base,uint size,uint face,int2 pixel) {
    // Reproject out-of-face taps onto the neighbour, rather than stretching
    // the last texel. This also keeps rough reflection cones seam-free.
    if(any(pixel<0) || any(pixel>=int(size))) {
        float2 uv=(float2(pixel)+.5)/size*2-1;
        float3 direction;
        if(face==0) direction=float3(1,-uv.y,-uv.x);
        else if(face==1) direction=float3(-1,-uv.y,uv.x);
        else if(face==2) direction=float3(uv.x,1,uv.y);
        else if(face==3) direction=float3(uv.x,-1,-uv.y);
        else if(face==4) direction=float3(uv.x,-uv.y,1);
        else direction=float3(-uv.x,-uv.y,-1);
        reflection_cube_coordinates(direction,face,uv);
        pixel=int2(floor((uv*.5+.5)*size));
    }
    pixel=clamp(pixel,0,int(size)-1);
    return reflection_rgb(coverage.Load(base+(face*size*size+pixel.y*size+pixel.x)*4));
}
uint reflection_background_byte(uint base,uint address) {
    address&=65535u;return (coverage.Load(base+64+(address&~3u))>>((address&3u)*8))&255u;
}
uint reflection_background_word(uint base,uint address) {
    return reflection_background_byte(base,address*2)|(reflection_background_byte(base,address*2+1)<<8);
}
#include "environment_material.hlsli"
uint backdropWord(uint address) {return backdropPixels.Load(address);}
#include "backdrop_sample.hlsli"
uint reflection_background_at(uint base,float sampleX,float sampleY) {
    uint environment=coverage.Load(base+60);
    if(environment!=0) {
        uint3 image=coverage.Load3(environment+1072);
        if(image.x!=0 && image.y!=0) {
            float4 motion=asfloat(coverage.Load4(environment+1040)),plane=asfloat(coverage.Load4(environment+1056));
            float4 projection=asfloat(coverage.Load4(environment+1088));
            float4 keep0=asfloat(coverage.Load4(environment+1104)),keep1=asfloat(coverage.Load4(environment+1120));
            if(backdropCovers(sampleX-128,sampleY,motion.x,plane.x,projection,keep0,keep1)) {
                uint4 modes=coverage.Load4(environment+1024);
                float2 uv=backdropMotion(backdropCoordinates(sampleX-128,sampleY,motion.x,plane.x,plane.z,projection,keep0,keep1),modes.w,motion.w);
                bool moonAtlas=projection.w>=6 && projection.w<=8;
                bool cloudLimb=projection.w==9 && uv.y>=320/512.f;
                float4 response=cloudLimb || (moonAtlas && uv.y>=2)?float4(0,0,0,1):
                    asfloat(coverage.Load4(environment+(projection.w!=0 && projection.w!=6 && projection.w!=8 && sampleY>=motion.x+plane.x*(sampleX-128)?1152:1136)));
                float3 sky=backdropStyle(backdropSample(uv.x,uv.y,image.x,image.y,image.z,projection.w),uv,modes.z,modes.w!=0?motion.w:0.f);
                if(cloudLimb) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i) ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropLimbColour(sky,ramp);
                }
                uint moonRamp=coverage.Load(environment+1192);
                if(moonAtlas && uv.y>=2 && (moonRamp==1 || moonRamp==2)) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i) ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropRampMoonColour(sky,uv,ramp);
                } else if(moonAtlas && uv.y>=2) sky=backdropMoonColour(backdropMoonSurface(sky,uv,keep1),uv,
                    coverage.Load(environment+1172),coverage.Load(environment+1176),coverage.Load(environment+1180),
                    coverage.Load(environment+1184),coverage.Load(environment+1188));
                if(coverage.Load(environment+1168)==2) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i)ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropNebulaColour(sky,ramp);
                } else if(coverage.Load(environment+1168)!=0) {
                    uint limits=coverage.Load(environment+1168);
                    float maximum=((limits>>16)&255)!=0?float((limits>>16)&255):245.f;
                    float minimum=((limits>>16)&255)!=0?float((limits>>8)&255):140.f;
                    float shade=1.f+14.f*(1.f-saturate((dot(sky,float3(.299,.587,.114))-minimum)/max(1.f,maximum-minimum)));
                    uint a=uint(shade),b=min(a+1,15u);
                    uint ca=coverage.Load(environment+1168+a*4),cb=coverage.Load(environment+1168+b*4);
                    sky=lerp(float3(ca&255,(ca>>8)&255,(ca>>16)&255),float3(cb&255,(cb>>8)&255,(cb>>16)&255),shade-float(a));
                }
                float3 colour=sky*response.w+255.f*response.rgb;
                float opacity=moonAtlas && projection.w!=8?backdropMoonOpacity(uv,keep1):1;
                if(opacity<1) {
                    float2 backgroundUV=backdropMotion(backdropCoordinates(sampleX-128,sampleY,motion.x,plane.x,plane.z,projection),modes.w,motion.w);
                    float3 behind=backdropStyle(backdropSample(backgroundUV.x,backgroundUV.y,image.x,image.y,image.z,6.f),backgroundUV,modes.z,modes.w!=0?motion.w:0.f);
                    float4 skyResponse=asfloat(coverage.Load4(environment+1136));
                    colour=lerp(behind*skyResponse.w+255.f*skyResponse.rgb,colour,opacity);
                }
                uint3 rgb=uint3(clamp(colour*plane.w,0.f,255.f)+.5);
                return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000u;
            }
        }
    }
    // Native pixel art stays point sampled. Photographic reflections above
    // retain subpixel ray coordinates rather than snapping to a 256-wide grid.
    int x=int(floor(sampleX)),y=int(floor(sampleY));
    uint4 layout=coverage.Load4(base);uint edge=coverage.Load(base+16),flags=coverage.Load(base+28);
    uint mapWidth=layout.z*edge,mapHeight=layout.w*edge;
    y=clamp(y,0,223);
    bool outside=x<0 || x>=256;
    // Tunnel art belongs to the forward corridor only. Other directions sample
    // its wall, not another copy of the opening.
    if((flags&16u) && outside) {x=0;y=112;}
    int sx=(flags&2u)?asint(coverage.Load(base+65600+y*4)):asint(coverage.Load(base+20));
    int sy=(flags&4u)?asint(coverage.Load(base+66496+y*4)):asint(coverage.Load(base+24));
    if(flags&8u) {
        if(flags&64u) {
            float2 fit=asfloat(coverage.Load2(base+48));
            float offset=fit.x+fit.y*float(x+(asint(coverage.Load(base+20))&7))/8;
            // Match round-away-from-zero used by the background's roll fit.
            sy=(offset<0?-int(floor(-offset+.5)):int(floor(offset+.5)))&8191;
        } else {
            int column=int(floor(float(x+(sx&7))/8));
            uint word=reflection_background_word(base,0x2fa0u+uint(clamp(column-1,0,31)));
            if(word&0x4000u) sy=int(word&8191u);
        }
    }
    int unwrapped=x+sx;bool outMap=unwrapped<0 || unwrapped>=int(mapWidth);
    uint black=coverage.Load(base+40),palette=coverage.Load(4);
    if((!(flags&1u) && outMap) || (outside && outMap && y<int(coverage.Load(base+36))))
        return coverage.Load(palette+black*4)|0xff000000u;
    uint sourceX=uint(unwrapped)&(mapWidth-1),sourceY=uint(y+sy)&(mapHeight-1);
    uint skySourceMin=coverage.Load(base+44);
    if(outside && y<144 && !(flags&16u) && (flags&8u) && skySourceMin<mapHeight)
        sourceY=max(sourceY,skySourceMin);
    uint tx=sourceX/edge,ty=sourceY/edge;
    uint tile=reflection_background_word(base,layout.x+((ty>>5)*(layout.z>>5)+(tx>>5))*1024+(ty&31)*32+(tx&31));
    uint px=sourceX&(edge-1),py=sourceY&(edge-1);
    if(tile&16384u) px=edge-1-px;if(tile&32768u) py=edge-1-py;
    uint character=((tile&1023u)+(px>>3)+(py>>3)*16)&1023u;
    uint inkAt=layout.y*2+character*32+(py&7)*2,mask=128u>>(px&7),ink=0;
    if(reflection_background_byte(base,inkAt)&mask) ink|=1;
    if(reflection_background_byte(base,inkAt+1)&mask) ink|=2;
    if(reflection_background_byte(base,inkAt+16)&mask) ink|=4;
    if(reflection_background_byte(base,inkAt+17)&mask) ink|=8;
    // Empty ink reveals the scene backdrop, rather than inventing a black
    // surface. Transparency uses source CGRAM, before display palette effects.
    if(!ink) return asuint(groundPoint.w)|0xff000000u;
    uint colour=((tile>>10)&7u)*16+ink;
    int uniqueX=x+int(uint(sx+int(mapWidth/2))&(mapWidth-1))-int(mapWidth/2);
    if(outside) for(uint r=0;r<coverage.Load(base+32);++r) {
        uint at=base+67392+r*32;int4 bounds=asint(coverage.Load4(at));uint3 colours=coverage.Load3(at+16);
        int replacement=asint(coverage.Load(at+28));bool suppressAll=replacement>=0 && (replacement&0x40000000)!=0;
        if(suppressAll) replacement=replacement&~0x40000000;
        if((suppressAll || uniqueX<0 || uniqueX>=int(mapWidth))
            && int(sourceX)>=bounds.x && int(sourceX)<bounds.z && int(sourceY)>=bounds.y && int(sourceY)<bounds.w
            && colour>=colours.x && colour<=colours.y) {
            colour=colours.z;
            if(replacement!=0) {
                uint rx=uint(int(sourceX)+replacement)&(mapWidth-1);
                uint rtx=rx/edge;
                uint rt=reflection_background_word(base,layout.x+((ty>>5)*(layout.z>>5)+(rtx>>5))*1024+(ty&31)*32+(rtx&31));
                uint rpx=rx&(edge-1),rpy=sourceY&(edge-1);
                if(rt&16384u) rpx=edge-1-rpx;if(rt&32768u) rpy=edge-1-rpy;
                uint rc=((rt&1023u)+(rpx>>3)+(rpy>>3)*16)&1023u;
                uint ra=layout.y*2+rc*32+(rpy&7)*2,rm=128u>>(rpx&7),ri=0;
                if(reflection_background_byte(base,ra)&rm) ri|=1;
                if(reflection_background_byte(base,ra+1)&rm) ri|=2;
                if(reflection_background_byte(base,ra+16)&rm) ri|=4;
                if(reflection_background_byte(base,ra+17)&rm) ri|=8;
                if(ri) colour=((rt>>10)&7u)*16u+ri;
            }
            break;
        }
    }
    if((flags&32u) && !(coverage.Load(coverage.Load(base+56)+colour*4)&32767u))
        return asuint(groundPoint.w)|0xff000000u;
    uint rgba=coverage.Load(palette+colour*4);
    uint enhanced=coverage.Load(base+60);
    if(enhanced!=0) {
        uint kind=coverage.Load(enhanced+colour*4);
        if(kind!=0) {
            float3 rgb=float3(rgba&255u,(rgba>>8)&255u,(rgba>>16)&255u);
            uint3 shaded=uint3(environmentColour(rgb,kind,float(x)-128,float(y),
                coverage.Load4(enhanced+1024),asfloat(coverage.Load4(enhanced+1040)),
                asfloat(coverage.Load(enhanced+1056)))+.5);
            rgba=shaded.x|(shaded.y<<8)|(shaded.z<<16);
        }
    }
    return rgba|0xff000000u;
}
uint reflection_background(uint base,float3 direction) {
    // The authored image covers the original camera's angular field, not an
    // entire hemisphere. Continue its surroundings without duplicating unique art.
    return reflection_background_at(base,128+atan2(direction.x,direction.z)*256,
        112+atan2(direction.y,length(direction.xz))*256);
}
uint reflected_environment(float3 direction) {
    uint settings=coverage.Load(4)+1024,size=coverage.Load(settings+8);
    uint background=coverage.Load(settings+28);
    if(background) return reflection_background(background,direction);
    if(size==0) return asuint(groundPoint.w)|0xff000000u;
    direction=float3(dot(direction,asfloat(coverage.Load3(settings+16))),
        dot(direction,asfloat(coverage.Load3(settings+32))),dot(direction,asfloat(coverage.Load3(settings+48))));
    uint face;float2 uv;reflection_cube_coordinates(direction,face,uv);
    float2 at=(uv*.5+.5)*size-.5;
    int2 lo=int2(floor(at));float2 f=frac(at);
    uint base=coverage.Load(settings+12);
    float3 c00=reflection_cube_tap(base,size,face,lo);
    float3 c10=reflection_cube_tap(base,size,face,lo+int2(1,0));
    float3 c01=reflection_cube_tap(base,size,face,lo+int2(0,1));
    float3 c11=reflection_cube_tap(base,size,face,lo+int2(1,1));
    uint3 rgb=uint3(sqrt(lerp(lerp(c00*c00,c10*c10,f.x),lerp(c01*c01,c11*c11,f.x),f.y))*255+.5);
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000u;
}
uint trace_reflected_ray(RayDesc ray,uint2 id) {
    float groundDistance=ray.TMax;bool groundHit=false;
    if(options.y!=0) {
        float denominator=dot(ray.Direction,groundNormal.xyz);
        if(abs(denominator)>1e-8) {
            float t=dot(groundPoint.xyz-ray.Origin,groundNormal.xyz)/denominator;
            if(t>ray.TMin && t<ray.TMax) {groundDistance=t;groundHit=true;ray.TMax=t;}
        }
    }
    RayQuery<RAY_FLAG_NONE> secondary;
    secondary.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
    while(secondary.Proceed()) if(secondary.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
        && reflected_colour(secondary.CandidatePrimitiveIndex(),secondary.CandidateTriangleBarycentrics(),id)!=0)
        secondary.CommitNonOpaqueTriangleHit();
    if(secondary.CommittedStatus()==COMMITTED_TRIANGLE_HIT)
        return reflected_colour(secondary.CommittedPrimitiveIndex(),secondary.CommittedTriangleBarycentrics(),id);
    if(groundHit) {
        float3 hitPosition=ray.Origin+ray.Direction*groundDistance;
        hitPosition.x+=asfloat(coverage.Load(coverage.Load(4)+1084));
        uint background=coverage.Load(coverage.Load(4)+1052);
        // A physical floor remains opaque even without a tiled material.
        // Zero is the missing-background sentinel, not a tile-data address.
        if(background==0) return asuint(groundPoint.w)|0xff000000u;
        // Project the finite hit onto the authored ground, not the secondary
        // ray's direction. Nearby receivers now show positional parallax.
        if(hitPosition.z>1) return reflection_background_at(background,
            128+256*hitPosition.x/hitPosition.z,112+256*hitPosition.y/hitPosition.z);
        return reflection_background(background,hitPosition);
    }
    return reflected_environment(ray.Direction);
}
uint trace_water(uint2 id,float3 direction,float distance) {
    uint data=coverage.Load(4)+1088;
    float4 settings=asfloat(coverage.Load4(data));
    float4 r0=asfloat(coverage.Load4(data+16)),r1=asfloat(coverage.Load4(data+32)),r2=asfloat(coverage.Load4(data+48));
    float3x3 rotation=float3x3(r0.xyz,r1.xyz,r2.xyz);
    float3 hit=direction*distance;
    float3 position=mul(hit,rotation)+float3(r0.w,r1.w,r2.w);
    float t=settings.x;
    // Long waves plus smaller crossing ripples; anchored in world coordinates,
    // not screen pixels. No stochastic sampling or per-frame noise.
    float dx=.055*cos(position.x*.018+position.z*.011-t*.8)
        +.025*cos(position.x*.047-position.z*.025+t*1.2);
    float dz=.045*cos(position.z*.022-position.x*.009-t*.65)
        -.020*cos(position.x*.047-position.z*.025+t*1.2);
    if(settings.w!=0) {dx=0;dz=0;}
    float3 normal=normalize(mul(rotation,float3(dx,-1,dz)));
    if(dot(normal,direction)>0) normal=-normal;
    float bias=max(.05,distance*1e-5);
    RayDesc ray;ray.Origin=hit+normal*bias;ray.TMin=bias;ray.TMax=65536;
    float3 light=normalize(mul(rotation,float3(-1,-1,-1)));
    ray.Direction=light;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow;
    shadow.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
    while(shadow.Proceed()) if(shadow.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
        && reflected_colour(shadow.CandidatePrimitiveIndex(),shadow.CandidateTriangleBarycentrics(),id)!=0)
        shadow.CommitNonOpaqueTriangleHit();
    float visibility=shadow.CommittedStatus()==COMMITTED_TRIANGLE_HIT?0:1;
    uint background=coverage.Load(coverage.Load(4)+1052);
    float3 authored=reflection_rgb(background?reflection_background_at(background,
        128+256*(hit.x+asfloat(coverage.Load(coverage.Load(4)+1084)))/hit.z,
        112+256*hit.y/hit.z):asuint(groundPoint.w));
    float luminance=dot(authored,float3(.3,.59,.11));
    float3 base=luminance*float3(.20,.58,.85);
    if(settings.w==1) base=luminance*float3(.8,.82,.85);
    if(settings.w==2) base=luminance*float3(1,.875,.58);
    float diffuse=.65+.35*visibility*max(0,dot(normal,light));
    float3 radiance=base*base*diffuse;
    float grazing=pow(1-saturate(dot(-direction,normal)),5);
    float fresnel=settings.w!=0?.95:.02+.98*grazing;
    if(settings.y>0) {
        ray.Direction=reflect(direction,normal);
        float3 reflected=reflection_rgb(trace_reflected_ray(ray,id));
        float3 reflectance=settings.w==2?float3(1,.766,.336)+(1-float3(1,.766,.336))*grazing:float3(1,1,1);
        radiance=lerp(radiance,reflected*reflected*reflectance,saturate(fresnel*settings.y));
    }
    float3 halfway=normalize(light-direction);
    float specular=pow(saturate(dot(normal,halfway)),96)*visibility;
    radiance+=float3(1,.95,.82)*specular*max(authored.r,max(authored.g,authored.b))*.55;
    uint3 rgb=uint3(sqrt(saturate(radiance))*255+.5);
    // 254 denotes a shaded water receiver; 255 remains a model reflection.
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xfe000000u;
}
uint trace_reflection(uint2 id) {
    float3 direction=normalize(float3((id.x+.5-camera.w)/camera.z,(id.y+.5-options.x)/options.z,1));
    RayDesc ray;ray.Origin=0;ray.Direction=direction;ray.TMin=1;ray.TMax=65536;
    bool water=false;
    if(options.y!=0 && asfloat(coverage.Load(coverage.Load(4)+1096))!=0) {
        float denominator=dot(direction,groundNormal.xyz);
        float distance=abs(denominator)>1e-8?dot(groundPoint.xyz,groundNormal.xyz)/denominator:0;
        if(distance>ray.TMin && distance<ray.TMax) {ray.TMax=distance;water=true;}
    }
    RayQuery<RAY_FLAG_NONE> primary;
    primary.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
    while(primary.Proceed()) if(primary.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
        && reflected_colour(primary.CandidatePrimitiveIndex(),primary.CandidateTriangleBarycentrics(),id)!=0)
        primary.CommitNonOpaqueTriangleHit();
    if(primary.CommittedStatus()!=COMMITTED_TRIANGLE_HIT) return water?trace_water(id,direction,ray.TMax):0;
    uint primitive=primary.CommittedPrimitiveIndex();
    uint stride=(uint)groundNormal.w,vertex=primitive*3*stride;
    float3 a=asfloat(triangleVertices.Load3(vertex)),b=asfloat(triangleVertices.Load3(vertex+stride)),c=asfloat(triangleVertices.Load3(vertex+2*stride));
    float3 normal=normalize(cross(b-a,c-a));
    if(dot(normal,direction)>0) normal=-normal;
    float distance=primary.CommittedRayT();
    float bias=max(.01,distance*1e-5);
    ray.Origin=direction*distance+normal*bias;
    ray.Direction=reflect(direction,normal);ray.TMin=bias;ray.TMax=65536;
    uint settings=coverage.Load(4)+1024;
    float roughness=asfloat(coverage.Load(settings));
    uint metallic=coverage.Load(settings+4);
    if(roughness==0 && !metallic) return trace_reflected_ray(ray,id);
    float3 reflected=ray.Direction;
    float3 tangent=normalize(cross(reflected,abs(reflected.y)<.95?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(reflected,tangent),sum=0;
    // Fixed quadrature: no frame/pixel noise or temporal accumulation required.
    const float2 taps[8]={float2(.5,0),float2(-.5,0),float2(0,.5),float2(0,-.5),
        float2(.612,.612),float2(-.612,.612),float2(.612,-.612),float2(-.612,-.612)};
    uint samples=roughness>0?8:1;
    for(uint sample=0;sample<samples;++sample) {
        ray.Direction=normalize(reflected+roughness*roughness*(tangent*taps[sample].x+bitangent*taps[sample].y));
        if(dot(ray.Direction,normal)<=0) ray.Direction=reflected;
        float3 value=reflection_rgb(trace_reflected_ray(ray,id));sum+=value*value;
    }
    float3 radiance=sum/samples;
    if(metallic) {
        float3 base=reflection_rgb(reflected_colour(primitive,primary.CommittedTriangleBarycentrics(),id));
        float3 f0=base*base;
        // Linear-light conductor reflectance; the scene remains ray traced.
        if(metallic==2) f0=float3(1,.766,.336);
        else if(metallic==3) f0=float3(.955,.638,.538);
        float grazing=pow(1-saturate(dot(-direction,normal)),5);
        radiance*=f0+(1-f0)*grazing;
    }
    uint3 rgb=uint3(sqrt(saturate(radiance))*255+.5);
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000u;
}

uint trace_pixel(uint2 id) {
    float3 ray = float3((id.x + .5 - camera.w) / camera.z,
        (id.y + .5 - options.x) / options.z, 1);
    float depth = 65536;
    bool receiverFound = false;
    if (options.y != 0) {
        float denominator = dot(ray, groundNormal.xyz);
        if (abs(denominator) > 1e-10) {
            float groundDepth = dot(groundPoint.xyz, groundNormal.xyz) / denominator;
            if (groundDepth > 1 && groundDepth < depth) {
                depth = groundDepth;
                receiverFound = true;
            }
        }
    }
    RayDesc receiverRay;
    receiverRay.Origin = 0;
    receiverRay.Direction = ray;
    receiverRay.TMin = 1;
    receiverRay.TMax = depth;
    RayQuery<RAY_FLAG_NONE> receiver;
    receiver.TraceRayInline(scene, options.w != 0 ? RAY_FLAG_FORCE_NON_OPAQUE : RAY_FLAG_FORCE_OPAQUE, 255, receiverRay);
    while (receiver.Proceed()) {
        if (receiver.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && covered(receiver.CandidatePrimitiveIndex(), receiver.CandidateTriangleBarycentrics()))
            receiver.CommitNonOpaqueTriangleHit();
    }
    if (receiver.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        depth = receiver.CommittedRayT();
        receiverFound = true;
    }
    uint blocked = 0;
    if (receiverFound) {
        RayDesc shadowRay;
        shadowRay.Origin = ray * depth;
        shadowRay.TMin = max(.1, depth * 1e-5);
        shadowRay.TMax = 65536;
        for (uint sample = 0; sample < 8; ++sample) {
            shadowRay.Direction = lights[sample].xyz;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow;
            shadow.TraceRayInline(scene, options.w != 0 ? RAY_FLAG_FORCE_NON_OPAQUE : RAY_FLAG_FORCE_OPAQUE, 255, shadowRay);
            while (shadow.Proceed()) {
                if (shadow.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && covered(shadow.CandidatePrimitiveIndex(), shadow.CandidateTriangleBarycentrics()))
                    shadow.CommitNonOpaqueTriangleHit();
            }
            if (shadow.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ++blocked;
        }
    }
    return 160 * blocked / 8;
}

// One byte per pixel, with four-byte row alignment for ByteAddressBuffer.
// Keep one ray workload per lane; shared packing avoids serializing four
// pixels' ray queries and does not assume a hardware wave/lane ordering.
groupshared uint shadowValues[64];
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uint local : SV_GroupIndex) {
    uint width = (uint)camera.x;
    bool valid = id.x < width && id.y < (uint)camera.y;
    if((coverage.Load(12)&1u)!=0) {
        if(valid) outputMask.Store((id.y*width+id.x)*4,trace_reflection(id.xy));
        return;
    }
    shadowValues[local] = valid ? trace_pixel(id.xy) : 0;
    GroupMemoryBarrierWithGroupSync();
    if ((local & 3) == 0 && valid) {
        uint packed = shadowValues[local] | (shadowValues[local+1] << 8)
            | (shadowValues[local+2] << 16) | (shadowValues[local+3] << 24);
        outputMask.Store(id.y * ((width+3)&~3U) + id.x, packed);
    }
}
