#if defined(STARFOX_SDL_GPU)
Texture2D<float4> inputImage : register(t0, space0);
Texture2D<float4> bloomInput : register(t1, space0);
Texture2D<float4> bloomCore : register(t2, space0);
Texture2D<float4> filterSource : register(t3, space0);
Texture2D<float4> splitBase : register(t4, space0);
Texture2D<float4> splitGlow : register(t5, space0);
ByteAddressBuffer layerTags : register(t6, space0);
ByteAddressBuffer indexedPixels : register(t7, space0);
ByteAddressBuffer surfaces : register(t8, space0);
ByteAddressBuffer shadowMask : register(t9, space0);
StructuredBuffer<float4> scalefxPixels : register(t10, space0);
ByteAddressBuffer setupPixels : register(t11, space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> outputImage : register(u0, space1);
[[vk::image_format("rgba32f")]] RWTexture2D<float4> bloomOutput : register(u1, space1);
[[vk::image_format("rgba8")]] RWTexture2D<float4> filterOutput : register(u2, space1);
[[vk::image_format("rgba8")]] RWTexture2D<float4> splitOutput : register(u3, space1);
cbuffer Settings : register(b0, space2) {
#else
Texture2D<float4> inputImage : register(t0);
ByteAddressBuffer layerTags : register(t1);
ByteAddressBuffer indexedPixels : register(t2);
ByteAddressBuffer surfaces : register(t3);
Texture2D<float4> bloomInput : register(t4);
Texture2D<float4> bloomCore : register(t5);
Texture2D<float4> filterSource : register(t6);
ByteAddressBuffer shadowMask : register(t7);
Texture2D<float4> splitBase : register(t8);
Texture2D<float4> splitGlow : register(t9);
RWTexture2D<float4> splitOutput : register(u3);
RWTexture2D<float4> outputImage : register(u0);
RWTexture2D<float4> bloomOutput : register(u1);
RWTexture2D<float4> filterOutput : register(u2);
cbuffer Settings : register(b0) {
#endif
    uint width,height,scale,stage;
    uint hdr,chromatic,smoothing,modelEffect;
    uint worldEffect,modelIntensity,worldIntensity,aa;
    uint lighting,surfaceWidth,surfaceHeight,reserved;
    int surfaceX,surfaceY,minimumX,minimumY;
    int maximumX,maximumY,pad0,pad1;
    uint bloomModel,bloomWorld,bloomWidth,bloomHeight;
    uint filter,highlightFilter,overlayFilter,pad3;
    uint shadowWidth,shadowHeight; int shadowY; uint shadowEnabled;
    uint4 windowRows[48];
    uint4 environmentClasses[64];
    uint4 environmentModes;
    float4 environmentMotion;
    float4 environmentPlane;
    float4 backdropProjection;
    float4 backdropKeep0;
    float4 backdropKeep1;
    float4 backdropPaletteSky;
    float4 backdropPaletteSurface;
    uint4 backdropRamp[4];
    float4 scrollFraction;
};
#include "environment_material.hlsli"
#if defined(STARFOX_SDL_GPU)
uint backdropWord(uint address) {return setupPixels.Load(address);}
#include "backdrop_sample.hlsli"
#endif
void touchBox(inout uint3 rgb,int2 at,int4 bounds,uint3 edgeColour) {
    if(at.x<bounds.x || at.y<bounds.y || at.x>bounds.z || at.y>bounds.w) return;
    bool edge=at.x==bounds.x || at.y==bounds.y || at.x==bounds.z || at.y==bounds.w;
    uint alpha=edge?170:76;
    uint3 colour=edge?edgeColour:uint3(18,28,42);
    rgb=(rgb*(255-alpha)+colour*alpha+127)/255;
}
uint indexOf(uint2 p) { return p.y*width+p.x; }
// Exact up to 20-bit distances, enforced by the host. Each limb operation
// fits uint32; no shader int64/float rounding dependency at tangent pixels.
uint2 diskSquare(uint v) {
    uint low=v&65535u,high=v>>16;
    uint base=low*low,cross=low*high;
    uint result=base+(cross<<17);
    return uint2(result,high*high+(cross>>15)+(result<base?1u:0u));
}
bool insideDisk(int2 delta,uint radius) {
    uint2 d=uint2(abs(delta));
    if(any(d>radius)) return false;
    if(radius<=16383u) return d.x*d.x+d.y*d.y<=radius*radius;
    uint2 x=diskSquare(d.x),y=diskSquare(d.y),r=diskSquare(radius);
    uint low=x.x+y.x,high=x.y+y.y+(low<x.x?1u:0u);
    return high<r.y || (high==r.y && low<=r.x);
}
bool hostInk(int2 p) {
    if(any(p<0) || p.x>=int(surfaceWidth) || p.y>=int(surfaceHeight)) return false;
    uint i=uint(p.y)*surfaceWidth+uint(p.x);
    if(i>=6144) return false;
    uint word=i/32;
    return (windowRows[word/4][word%4]&(1u<<(i%32)))!=0;
}
uint tag(uint2 p) {
    uint i=indexOf(p);
    if(reserved==1) return (layerTags.Load(i*4)>>8)&255u;
    return (layerTags.Load(i & ~3u) >> ((i & 3u)*8)) & 255u;
}
bool model(uint2 p) { uint t=tag(p); return t==0 || t==4; }
bool world(uint2 p) { uint t=tag(p); return t==2 || t==3 || t==5; }
bool art(uint2 p) { uint t=tag(p); return t==1 || t==2 || t==4; }
uint4 colour(uint2 p) { return uint4(inputImage.Load(int3(p,0))*255+.5); }
uint luminance(uint4 c) { return (c.r*77+c.g*150+c.b*29)/256; }
uint2 bounded(int2 p) { return uint2(clamp(p,int2(0,0),int2(width-1,height-1))); }
uint4 manipulationSample(int2 at,uint2 original) {
    uint2 n=bounded(at);
    return tag(n)==1 || world(n)!=world(original)?colour(original):colour(n);
}
static const int bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
bool surfaceAt(int2 p,out float4 sample) {
    sample=0;
    int2 local=p-int2(surfaceX,surfaceY);
    if(any(p<0) || any(p>=int2(width,height)) || any(local<0) || any(local>=int2(surfaceWidth,surfaceHeight))) return false;
    if(reserved==1) {
        uint packed=indexedPixels.Load(indexOf(p)*4);
        if(!(packed & 0x01000000u) || ((packed>>16)&255u)!=(packed&255u)) return false;
        sample=asfloat(surfaces.Load4((local.y*surfaceWidth+local.x)*16));return true;
    }
    uint base=(local.y*surfaceWidth+local.x)*20;
    uint flags=surfaces.Load(base+16), i=indexOf(p);
    uint palette=(indexedPixels.Load(i & ~3u)>>((i & 3u)*8)) & 255u;
    if((flags & 0xff00)==0 || (flags & 255)!=palette) return false;
    sample=asfloat(surfaces.Load4(base)); return true;
}
uint4 sourceCell(int2 p) {
    p=clamp(p,int2(0,0),int2(width/scale-1,height/scale-1));
    return uint4(filterSource.Load(int3(p,0))*255+.5);
}
#if STARFOX_ENABLE_XBRZ
#include "xbrz_compute.hlsli"
#endif
#include "edge_corners.hlsli"
uint4 filterSample(uint2 p,uint factor) {
#if defined(STARFOX_SDL_GPU)
    if(filter==5) {
        uint2 extent=uint2(width,height)/scale*3;
        p=min(p,extent-1);
        return uint4(scalefxPixels[p.y*extent.x+p.x]*255+.5);
    }
#endif
#if STARFOX_ENABLE_XBRZ
    if(filter==2) return xbrzSample(p,factor);
#endif
    if(filter==1) {
        int2 cell=int2(p/factor);uint2 local=p%factor;
        uint corner=edgeCorners[factor*(factor-1)*(2*factor-1)/6-1+local.y*factor+local.x];
        uint4 c=sourceCell(cell),u=sourceCell(cell+int2(0,-1)),r=sourceCell(cell+int2(1,0));
        uint4 d=sourceCell(cell+int2(0,1)),l=sourceCell(cell+int2(-1,0)),v=c;
        if(corner==1 && all(l==u) && any(l!=d) && any(u!=r)) v=u;
        else if(corner==2 && all(u==r) && any(u!=l) && any(r!=d)) v=r;
        else if(corner==3 && all(d==l) && any(d!=r) && any(l!=u)) v=l;
        else if(corner==4 && all(r==d) && any(r!=l) && any(d!=u)) v=d;
        return c.a>0 && v.a==0?c:v;
    }
    float2 at=(float2(p)+.5)/factor-.5;
    int2 lo=int2(floor(at));float2 f=saturate((at-lo-.5)*1.5+.5);
    uint4 samples[4]={sourceCell(lo),sourceCell(lo+int2(1,0)),sourceCell(lo+int2(0,1)),sourceCell(lo+1)};
    float weights[4]={(1-f.x)*(1-f.y),f.x*(1-f.y),(1-f.x)*f.y,f.x*f.y};
    float alpha=0;float3 sum=0;
    for(uint i=0;i<4;++i) { float w=weights[i]*samples[i].a;alpha+=w;sum+=w*samples[i].rgb; }
    if(alpha<=.5) return 0;
    float3 value=sum/alpha;
    if(filter==4) { value+=max(0,value-128)*.08;value*=p.y%factor<(factor+1)/2?1:.76; }
    return uint4(uint3(clamp(value+.5,0,255)),uint(alpha+.5));
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint2 p=id.xy;
#if defined(STARFOX_SDL_GPU)
    if(stage==28) {
        if(p.x>=width/scale || p.y>=height/scale) return;
        uint i=uint(pad0)+p.y*(width/scale)+p.x;
        uint index=pad3!=0?(shadowMask.Load((p.y*(width/scale)+p.x)*4)&255)
            :(setupPixels.Load(i&~3u)>>((i&3u)*8))&255;
        uint packed=index!=0 && index<lighting?setupPixels.Load(uint(pad1)+index*4):0;
        if(filter!=0 && index!=0 && index<lighting) packed|=0xff000000u;
        filterOutput[p]=float4(packed&255,(packed>>8)&255,(packed>>16)&255,packed>>24)/255.;return;
    }
#endif
    if(stage==13) {
        if(p.x>=width/scale || p.y>=height/scale) return;
        uint2 base=p*scale;
        if(overlayFilter) {filterOutput[p]=inputImage.Load(int3(p,0));return;}
        bool covered=true;
        for(uint y=0;y<scale && covered;++y) for(uint x=0;x<scale;++x)
            if(!art(base+uint2(x,y))) {covered=false;break;}
        filterOutput[p]=covered?float4(colour(base).rgb/255.0,1):float4(0,0,0,0);return;
    }
    if(stage>=7 && stage<=11) {
        if(p.x>=bloomWidth || p.y>=bloomHeight) return;
        float3 sum=0;
        if(stage==7) {
            uint step=2*scale;
            float strengths[4]={0,.4,.85,1.5};
            uint level=max(bloomModel,bloomWorld);
            for(uint y=p.y*step;y<min((p.y+1)*step,height);++y)
                for(uint x=p.x*step;x<min((p.x+1)*step,width);++x) {
                    uint2 at=uint2(x,y); uint t=tag(at);
                    if(t==1) continue;
                    uint sourceLevel=(t==0 || t==4)?bloomModel:bloomWorld;
                    if(sourceLevel==0) continue;
                    float3 v=colour(at).rgb/255.0;
                    float peak=max(v.r,max(v.g,v.b));peak*=peak;
                    float knee=clamp(peak-.25,0,.3);
                    float contribution=max(peak-.4,knee*knee/.6)/max(peak,.001);
                    contribution*=strengths[sourceLevel]/strengths[level];
                    sum+=v*v*contribution/(step*step);
                }
        } else {
            int radius=stage<=9?1:4;
            bool horizontal=stage==8 || stage==10;
            for(int n=-radius;n<=radius;++n) {
                int2 at=clamp(int2(p)+(horizontal?int2(n,0):int2(0,n)),int2(0,0),int2(bloomWidth-1,bloomHeight-1));
                sum+=bloomInput.Load(int3(at,0)).rgb;
            }
            sum/=radius*2+1;
        }
        bloomOutput[p]=float4(sum,0);return;
    }
    if(p.x>=width || p.y>=height) return;
    uint4 c=colour(p), result=c;
    if(stage==31) {
        if(tag(p)==2 && any(scrollFraction.xy!=0)) {
            float2 at=clamp(float2(p)+scrollFraction.xy*float(scale),0.f,float2(width-1,height-1));
            uint2 a=uint2(at);float2 f=frac(at);float3 sum=0;
            for(uint by=0;by<2;++by) for(uint bx=0;bx<2;++bx) {
                uint2 n=min(a+uint2(bx,by),uint2(width-1,height-1));
                sum+=float3(colour(tag(n)==2?n:p).rgb)*(bx?f.x:1-f.x)*(by?f.y:1-f.y);
            }
            c.rgb=uint3(sum+.5);result=c;
        }
        uint i=indexOf(p);
        uint index=reserved==1?(indexedPixels.Load(i*4)&255u):(indexedPixels.Load(i&~3u)>>((i&3u)*8))&255u;
        bool terrain=tag(p)==5;
        uint kind=terrain?uint(environmentPlane.y):environmentClasses[index/4][index%4];
        if((tag(p)==2 || terrain) && kind!=0) {
            float y=(float(p.y)+.5)/float(scale);
            uint4 modes=environmentModes;float3 base=float3(c.rgb);
            if(terrain) {modes.x=1;if(kind==4) base*=.84f;}
            float3 upgraded=environmentColour(base,kind,(float(p.x)+.5)/float(scale)-float(width)/float(scale)*.5,y,modes,environmentMotion,environmentPlane.x);
            bool water=kind<=5 && (environmentModes.x>=6 || (environmentModes.x==1 && kind==5));
            float x=(float(p.x)+.5)/float(scale)-float(width)/float(scale)*.5;
            float d=y-environmentMotion.x-environmentPlane.x*x;
            if(water && overlayFilter!=0 && d>0) {
                float offset=(environmentModes.x>=7?2.f:1.55f)*d/(1+environmentPlane.x*environmentPlane.x);
                float sx=clamp(float(p.x)+offset*environmentPlane.x*float(scale)+(environmentModes.x>=7?0:sin(y*.12f+environmentMotion.w)*(2*float(scale))),0.f,float(width-1));
                float sy=clamp((y-offset)*float(scale)-.5f,0.f,float(height-1));
                uint x0=uint(sx),y0=uint(sy);float fx=sx-x0,fy=sy-y0;
                float3 reflected=0;float weight=0;
                for(uint dy=0;dy<2;++dy) for(uint dx=0;dx<2;++dx) {
                    uint2 at=uint2(min(x0+dx,width-1),min(y0+dy,height-1));
                    if(tag(at)==1) continue;
                    float w=(dx!=0?fx:1-fx)*(dy!=0?fy:1-fy);weight+=w;
                    reflected+=float3(colour(at).rgb)*w;
                }
                if(weight>0) {
                    float amount=environmentModes.x>=7?.8f:.15f+.20f*clamp(1-d/200.f,0.f,1.f);
                    float3 tint=environmentModes.x==8?float3(1,.875f,.58f):float3(1,1,1);
                    upgraded=upgraded*(1-amount)+(reflected/weight)*tint*amount;
                }
            }
            result.rgb=uint3(upgraded+.5);
        }
#if defined(STARFOX_SDL_GPU)
        if(tag(p)==2 && (kind!=7 || backdropProjection.w==8) && environmentModes.z!=0 && surfaceWidth>0 && surfaceHeight>0) {
            float x=(float(p.x)+.5)/float(scale)-float(width)/float(scale)*.5;
            float y=(float(p.y)+.5)/float(scale);
            float2 uv=backdropMotion(backdropCoordinates(x,y,environmentMotion.x,environmentPlane.x,environmentPlane.z,backdropProjection,backdropKeep0,backdropKeep1),environmentModes.w,environmentMotion.w);
            bool cityMoon=backdropProjection.w==8 && uv.y>=2;
            bool landscape=backdropProjection.w==0 || backdropProjection.w==6 || backdropProjection.w==8;
            bool skyOwned=cityMoon || (kind!=7 && (!landscape || kind==6
                || (kind==0 && y<environmentMotion.x+environmentPlane.x*x)));
            float4 coverageProjection=backdropProjection;
            if(backdropProjection.w==0 && kind==6) coverageProjection.w=1;
            if(skyOwned && ((backdropProjection.w==6 && kind==6) || backdropCovers(x,y,environmentMotion.x,environmentPlane.x,
                    coverageProjection,backdropKeep0,backdropKeep1))) {
                bool moonAtlas=backdropProjection.w>=6 && backdropProjection.w<=8;
                bool cloudLimb=backdropProjection.w==9 && uv.y>=320/512.f;
                float4 response=cloudLimb || (moonAtlas && uv.y>=2)?float4(0,0,0,1):
                    !landscape && y>=environmentMotion.x+environmentPlane.x*x?backdropPaletteSurface:backdropPaletteSky;
                float3 sky=backdropStyle(backdropSample(uv.x,uv.y,
                    surfaceWidth,surfaceHeight,uint(surfaceX),backdropProjection.w),uv,environmentModes.z,environmentModes.w!=0?environmentMotion.w:0.f);
                if(cloudLimb) sky=backdropLimbColour(sky,backdropRamp);
                if(moonAtlas && uv.y>=2 && (backdropRamp[1].z==1 || backdropRamp[1].z==2)) sky=backdropRampMoonColour(sky,uv,backdropRamp);
                else if(moonAtlas && uv.y>=2) sky=backdropMoonColour(backdropMoonSurface(sky,uv,backdropKeep1),uv,
                    backdropRamp[0].y,backdropRamp[0].z,backdropRamp[0].w,backdropRamp[1].x,backdropRamp[1].y);
                if(backdropRamp[0].x==2) sky=backdropNebulaColour(sky,backdropRamp);
                else if(backdropRamp[0].x!=0) {
                    uint limits=backdropRamp[0].x;
                    float maximum=((limits>>16)&255)!=0?float((limits>>16)&255):245.f;
                    float minimum=((limits>>16)&255)!=0?float((limits>>8)&255):140.f;
                    float shade=1.f+14.f*(1.f-saturate((dot(sky,float3(.299,.587,.114))-minimum)/max(1.f,maximum-minimum)));
                    uint a=uint(shade),b=min(a+1,15u);
                    uint ca=backdropRamp[a/4][a%4],cb=backdropRamp[b/4][b%4];
                    sky=lerp(float3(ca&255,(ca>>8)&255,(ca>>16)&255),float3(cb&255,(cb>>8)&255,(cb>>16)&255),shade-float(a));
                }
                float3 colour=sky*response.w+255.f*response.rgb;
                float opacity=moonAtlas && backdropProjection.w!=8?backdropMoonOpacity(uv,backdropKeep1):1;
                if(opacity<1) {
                    float2 backgroundUV=backdropMotion(backdropCoordinates(x,y,environmentMotion.x,environmentPlane.x,environmentPlane.z,backdropProjection),environmentModes.w,environmentMotion.w);
                    float3 behind=backdropStyle(backdropSample(backgroundUV.x,backgroundUV.y,surfaceWidth,surfaceHeight,uint(surfaceX),6.f),backgroundUV,environmentModes.z,environmentModes.w!=0?environmentMotion.w:0.f);
                    colour=lerp(behind*backdropPaletteSky.w+255.f*backdropPaletteSky.rgb,colour,opacity);
                }
                result.rgb=uint3(clamp(colour*environmentPlane.w,0.f,255.f)+.5);
            }
        }
#endif
    } else if(stage==32) {
        bool scenery=world(p),models=(chromatic&1u)!=0,selected=scenery?(chromatic&2u)!=0:models;
        float3 memory=(chromatic&4u)!=0?float3(0,0,0):bloomInput.Load(int3(p,0)).rgb*asfloat(pad0);
        if(tag(p)==1 || (!scenery && !models)) memory=0;
        else {
            memory=max(selected?float3(c.rgb):float3(0,0,0),memory);
            result.rgb=uint3(clamp(float3(c.rgb)+(max(float3(c.rgb),memory)-float3(c.rgb))*float(pad1)/100+.5,0,255));
        }
        bloomOutput[p]=float4(memory,0);
        outputImage[p]=float4(result)/255;return;
    } else if(stage==29) {
#if defined(STARFOX_SDL_GPU)
        uint4 ink=uint4((filter!=0?bloomInput.Load(int3(p,0)):filterSource.Load(int3(p/scale,0)))*255+.5);
        uint i=uint(pad0)+(p.y/scale)*(width/scale)+p.x/scale;
        uint index=pad3!=0?(shadowMask.Load(((p.y/scale)*(width/scale)+p.x/scale)*4)&255)
            :(setupPixels.Load(i&~3u)>>((i&3u)*8))&255;
        bool visible=filter!=0?ink.a!=0:index!=0 && index<lighting;
        if(visible) {
            int3 five=max(int3(0,0,0),int3((ink.rgb*31+127)/255)-int(hdr));
            uint3 rgb=uint3((five<<3)|(five>>2));
            if(filter!=0) result.rgb=(rgb*ink.a+c.rgb*(255-ink.a)+127)/255;
            else result=uint4(rgb,ink.a);
        }
#endif
    } else if(stage==27) {
        int2 at=int2(p/scale);
        bool inside=at.x>=minimumX && at.x<=maximumX && at.y>=minimumY && at.y<=maximumY;
        if(inside && (pad0&4)!=0) {
            uint2 local=uint2(at-int2(minimumX,minimumY));
            inside=local.x<32 && local.y<32 && (windowRows[local.y/4][local.y%4]&(1u<<local.x))!=0;
        }
        if((pad0&1)!=0 && !inside) {
            int3 five=max(int3(0,0,0),int3((result.rgb*31+127)/255)-int(hdr));
            result.rgb=uint3((five<<3)|(five>>2));
        }
        if((pad0&2)!=0) {
            int3 five=max(int3(0,0,0),int3((result.rgb*31+127)/255)-int(chromatic));
            result.rgb=uint3((five<<3)|(five>>2));
        }
    } else if(stage==26) {
        int2 at=int2(p/scale);int w=int(width/scale),h=int(height/scale);
        int dx=w*20/100,dy=h*73/100,u=max(7,h/28);
        uint3 rgb=result.rgb;
        touchBox(rgb,at,int4(dx-u,dy-u*3,dx+u,dy-u),uint3(235,245,255));
        touchBox(rgb,at,int4(dx-u,dy+u,dx+u,dy+u*3),uint3(235,245,255));
        touchBox(rgb,at,int4(dx-u*3,dy-u,dx-u,dy+u),uint3(235,245,255));
        touchBox(rgb,at,int4(dx+u,dy-u,dx+u*3,dy+u),uint3(235,245,255));
        touchBox(rgb,at,int4(dx-u,dy-u,dx+u,dy+u),uint3(150,180,210));
        int2 centre=int2(w*89/100,h*69/100);
        touchBox(rgb,at,int4(centre-u,centre+u),uint3(100,235,120));
        centre=int2(w*77/100,h*81/100);
        touchBox(rgb,at,int4(centre-u,centre+u),uint3(245,105,105));
        centre=int2(w*77/100,h*57/100);
        touchBox(rgb,at,int4(centre-u,centre+u),uint3(100,155,255));
        centre=int2(w*65/100,h*69/100);
        touchBox(rgb,at,int4(centre-u,centre+u),uint3(250,220,95));
        touchBox(rgb,at,int4(7,7,w*30/100,20),uint3(205,215,230));
        touchBox(rgb,at,int4(w*70/100,7,w-8,20),uint3(205,215,230));
        touchBox(rgb,at,int4(w*36/100,h-20,w*47/100,h-7),uint3(205,215,230));
        touchBox(rgb,at,int4(w*53/100,h-20,w*64/100,h-7),uint3(205,215,230));
        result.rgb=rgb;
    } else if(stage==25) {
#if defined(STARFOX_SDL_GPU)
        uint2 at=p/scale;
        int localX=int(at.x)-int((width/scale-256u)/2u);
        if(localX>=minimumX && localX<=maximumX && at.y>=20 && at.y<=222) result.rgb/=4;
        if(at.x<surfaceWidth && at.y<surfaceHeight) {
            uint i=at.y*surfaceWidth+at.x;
            uint ink=(setupPixels.Load(i&~3u)>>((i&3u)*8))&255;
            if(ink!=0) {
                uint c=ink&15;
                uint3 rgb=c==14?uint3(255,255,255):c==10?uint3(255,220,64):uint3(180,200,215);
                result.rgb=rgb*uint(pad0)/15;
            }
        }
#endif
    } else if(stage==24) {
        int2 at=int2(p/scale)-int2(surfaceX,surfaceY);
        if(hostInk(at)) result=uint4(255,255,255,255);
        else if(at.x>=-6 && at.x<int(surfaceWidth)+6 && at.y>=-4 && at.y<int(surfaceHeight)+4) {
            bool border=at.x==-6 || at.x==int(surfaceWidth)+5 || at.y==-4 || at.y==int(surfaceHeight)+3;
            result=uint4(border?255:0,border?255:0,border?255:0,255);
        }
    } else if(stage==23) {
        int2 at=int2(p/scale)-int2(surfaceX,surfaceY);
        if(hostInk(at)) result=uint4(255,255,255,255);
        else if(hostInk(at-int2(1,1))) result=uint4(0,0,0,255);
    } else if(stage==22) {
        int2 logical=int2(p/scale);
        int w=int(width/scale),h=int(height/scale);
        bool expandedY=(pad1&2)!=0;
        int row=expandedY ? clamp(logical.y*191/max(h-1,1),0,191) : logical.y-surfaceY;
        if(row>=0 && row<192) {
            uint packed=windowRows[uint(row)/4][uint(row)%4];
            int left=int(packed&255),right=int((packed>>8)&255);
            int sx=(pad1&1)!=0 ? 16+clamp(logical.x*223/max(w-1,1),0,223) : logical.x-surfaceX;
            bool inside=left<=right ? sx>=left && sx<=right : sx>=left || sx<=right;
            bool a=!inside,b=sx>=16 && sx<=240;
            bool masked=pad0==1 ? a&&b : pad0==2 ? a!=b : pad0==3 ? a==b : a||b;
            if(masked) result=uint4(0,0,0,255);
        }
    } else if(stage==21) {
        uint i=indexOf(p);
        uint palette=reserved==1 ? indexedPixels.Load(i*4)&255u
            : (indexedPixels.Load(i&~3u)>>((i&3u)*8))&255u;
        if(palette<128 || (pad1&4)!=0) {
            int3 fixed=int3(hdr,chromatic,smoothing);
            int3 v=(pad1&1)!=0 ? int3(c.rgb)-fixed : int3(c.rgb)+fixed;
            if((pad1&2)!=0) v/=2;
            result.rgb=uint3(clamp(v,0,255));
        }
    } else if(stage==20) {
        uint2 anchor=(p/scale)*scale;
        uint i=indexOf(anchor);
        uint packed=reserved==1 ? indexedPixels.Load(i*4)
            : (indexedPixels.Load(i&~3u)>>((i&3u)*8))&255u;
        if((packed&255u)<128 && (pad1==0 || !(packed&0x02000000u))) {
            int3 v=max(int3((c.rgb*31u+127u)/255u)-pad0,0);
            result.rgb=uint3((v<<3)|(v>>2));
        }
    } else if(stage==19) {
        int2 delta=int2(p)-int2(surfaceX,surfaceY);
        uint i=indexOf(p);
        uint palette=reserved==1 ? indexedPixels.Load(i*4)&255u
            : (indexedPixels.Load(i&~3u)>>((i&3u)*8))&255u;
        if(int(p.x)>=minimumX && int(p.x)<maximumX
            && int(p.y)>=minimumY && int(p.y)<maximumY
            && insideDisk(delta,uint(pad0))
            && (palette<128 || (pad1&4)!=0)) {
            int3 main=int3((c.rgb*31u+127u)/255u);
            int3 fixed=int3(hdr,chromatic,smoothing);
            int3 v=(pad1&1)!=0 ? main-fixed : main+fixed;
            if((pad1&2)!=0) v/=2;
            v=clamp(v,0,31);
            result.rgb=uint3((v<<3)|(v>>2));
        }
    } else if(stage==18) {
        // The CPU converts interpolated double-precision Y edges to exact
        // stored-pixel bounds; no GPU float rounding can shift a shutter row.
        if(int(p.y)>=surfaceX && int(p.y)<surfaceY) {
            bool closed=int(p.y)<minimumX || int(p.y)>=minimumY;
            int sx=int(p.x/scale)-maximumY;
            bool guard=pad0!=0 ? int(p.x)<maximumX
                : ((!(sx>=15 && sx<=16))!=(sx>=16 && sx<=240));
            if(closed || guard) result=uint4(0,0,0,255);
        }
    } else if(stage==17) {
        float4 sample;
        bool owns=surfaceAt(int2(p),sample);
        splitOutput[p]=owns?float4(c.rgb,255)/255.:float4(0,0,0,0);
        if(owns) {
            const int2 neighbours[8]={int2(-1,0),int2(1,0),int2(0,-1),int2(0,1),
                int2(-1,-1),int2(1,-1),int2(-1,1),int2(1,1)};
            for(uint n=0;n<8;++n) {
                int2 q=int2(p)+neighbours[n];
                if(any(q<0) || any(q>=int2(width,height))) continue;
                if(!surfaceAt(q,sample)) {result=colour(uint2(q));break;}
            }
        }
    } else if(stage==16) {
        uint4 base=uint4(splitBase.Load(int3(p,0))*255+.5);
        uint4 glow=uint4(splitGlow.Load(int3(p,0))*255+.5);
        bool unchanged=all(glow==c);
        uint3 contribution=unchanged?uint3(max(int3(glow.rgb)-int3(base.rgb),0)):uint3(0,0,0);
        result=uint4(unchanged?base.rgb:c.rgb,c.a);
        splitOutput[p]=float4(contribution,255)/255.;
    } else if(stage==30) {
        int sy=int(p.y)-shadowY;float4 surface;
        bool receiver=false;
        if(pad1!=0 && tag(p)==2) {
            uint i=indexOf(p);
            uint index=reserved==1?(indexedPixels.Load(i*4)&255u):(indexedPixels.Load(i&~3u)>>((i&3u)*8))&255u;
            uint kind=environmentClasses[index/4][index%4];
            receiver=kind>=1 && kind<=5 && (environmentModes.x>=6 || (environmentModes.x==1 && kind==5));
        } else if(pad1==0) receiver=model(p) && surfaceAt(int2(p),surface);
        if(receiver && p.x<shadowWidth && sy>=0 && sy<int(shadowHeight)) {
            uint packed=shadowMask.Load((uint(sy)*shadowWidth+p.x)*4);
            uint marker=packed>>24;
            uint alpha=pad1!=0?(marker==254?255:0):(marker==255?uint(pad0)*255/100:0);
            uint3 reflected=uint3(packed&255u,(packed>>8)&255u,(packed>>16)&255u);
            result.rgb=(reflected*alpha+c.rgb*(255-alpha)+127)/255;
        }
    } else if(stage==15) {
        int sy=int(p.y)-shadowY;
        uint layer=tag(p);
        if(p.x<shadowWidth && sy>=0 && sy<int(shadowHeight) && (layer==0 || layer==2 || layer==4 || layer==5)) {
            uint i=uint(sy)*(shadowEnabled==3?((shadowWidth+3u)&~3u):shadowWidth)+p.x;
            uint shade=shadowEnabled==2 ? shadowMask.Load(i*4)
                : (shadowMask.Load(i&~3u)>>((i&3u)*8))&255u;
            result.rgb=c.rgb*(255-shade)/255;
        }
    } else if(stage==14 && (overlayFilter || art(p))) {
        uint factor=filter==5?3:filter==2?clamp(scale,2u,6u):max(scale,2u);uint4 v;
        if(scale!=1) v=filterSample(p*factor/scale,factor);
        else {
            uint3 sum=0;uint alpha=0;
            for(uint y=0;y<factor;++y) for(uint x=0;x<factor;++x) {
                uint4 s=filterSample(p*factor+uint2(x,y),factor);alpha+=s.a;sum+=s.rgb*s.a;
            }
            v=0;
            if(alpha>0) { v=uint4((sum+alpha/2)/alpha,alpha/(factor*factor));if(filter==4 && p.y%2!=0) v.rgb=v.rgb*88/100; }
        }
        if(overlayFilter) result=v;
        else if(v.a>0) result.rgb=highlightFilter?uint3(255,0,255):(v.rgb*v.a+c.rgb*(255-v.a)+127)/255;
    } else if(stage==12 && tag(p)!=1) {
        float2 at=max(0,(float2(p)+.5)/(2*scale)-.5);
        uint2 lo=min(uint2(at),uint2(bloomWidth-1,bloomHeight-1)),hi=min(uint2(at)+1,uint2(bloomWidth-1,bloomHeight-1));
        float2 f=frac(at);
        uint2 taps[4]={lo,uint2(hi.x,lo.y),uint2(lo.x,hi.y),hi};
        float3 v[4];
        for(uint t=0;t<4;++t) v[t]=bloomCore.Load(int3(taps[t],0)).rgb*.6+bloomInput.Load(int3(taps[t],0)).rgb*.8;
        float3 glow=lerp(lerp(v[0],v[1],f.x),lerp(v[2],v[3],f.x),f.y);
        uint level=max(bloomModel,bloomWorld);
        float strength=level==1?.4:level==2?.85:1.5;
        float3 linearColour=c.rgb/255.0;linearColour*=linearColour;
        result.rgb=uint3(sqrt(saturate(linearColour+glow*strength))*255+.5);
    } else if(stage==6 && lighting>0 && int(p.x)>=minimumX && int(p.y)>=minimumY && int(p.x)<maximumX && int(p.y)<maximumY) {
        float4 surface;
        if(surfaceAt(p,surface)) {
            float3 n=surface.xyz*(surface.z>0?-1:1);
            float key=max(0,dot(n,float3(-.474,-.632,-.613))), fill=max(0,dot(n,float3(.422,.211,-.881)));
            float rimBase=1-saturate(-n.z), rim=rimBase*rimBase*key*.1;
            float spec=max(0,dot(n,float3(-.267,-.356,-.895)));
            spec*=spec; spec*=spec; spec*=spec; spec*=.18;
            uint nearer=0;
            int2 offsets[4]={int2(-1,0),int2(1,0),int2(0,-1),int2(0,1)};
            for(uint j=0;j<4;++j) {
                float4 neighbour;
                if(surfaceAt(int2(p)+offsets[j]*int(scale),neighbour)
                    && neighbour.w<surface.w-max(20,abs(surface.w)*.02)) ++nearer;
            }
            float illumination=clamp(.62+key*.58+fill*.16+rim-nearer*.025,.56,1.30);
            float peak=max(c.r,max(c.g,c.b));
            if(peak>0) {
                float diffuse=illumination<=1?illumination:1+(illumination-1)*(1-peak/255);
                float shine=(255-peak*diffuse)*spec*(peak/255);
                float3 lit=c.rgb*diffuse+shine*(.20+.80*c.rgb/peak);
                float strength=lighting==1?.35:lighting==2?.65:1;
                result.rgb=uint3(clamp(c.rgb+(lit-c.rgb)*strength+.5,0,255));
            }
        }
    } else if(stage==1 && tag(p)!=1 && hdr>0) {
        float exposure=hdr==1?1.15:hdr==2?1.35:1.65;
        float contrast=hdr*.2;
        float3 v=c.rgb/255.0;
        float3 lifted=v*exposure/(1+v*(exposure-1));
        float3 shaped=lifted*lifted*(3-2*lifted);
        result.rgb=uint3(255*(lifted*(1-contrast)+shaped*contrast)+.5);
    } else if(stage==2 && model(p) && chromatic>0) {
        float amount=(chromatic==1?.6:chromatic==2?1.2:2.4)*scale;
        float2 delta=(2*(float2(p)+.5)/float2(width,height)-1)*amount;
        for(uint channel=0;channel<3;channel+=2) {
            float2 at=clamp(float2(p)+(channel==0?1:-1)*delta,float2(0,0),float2(width-1,height-1));
            uint2 lo=uint2(at), hi=min(lo+1,uint2(width-1,height-1));
            float2 f=at-lo;
            uint2 taps[4]={lo,uint2(hi.x,lo.y),uint2(lo.x,hi.y),hi};
            float v[4];
            for(uint t=0;t<4;++t) v[t]=model(taps[t])?colour(taps[t])[channel]:c[channel];
            result[channel]=uint(lerp(lerp(v[0],v[1],f.x),lerp(v[2],v[3],f.x),f.y)+.5);
        }
    } else if(stage==3 && model(p) && smoothing>0) {
        int2 offsets[4]={int2(-int(scale),0),int2(scale,0),int2(0,-int(scale)),int2(0,scale)};
        uint3 total=0; uint count=0;
        for(uint n=0;n<4;++n) {
            uint2 neighbour=bounded(int2(p)+offsets[n]);
            if(model(neighbour)) { total+=colour(neighbour).rgb; ++count; }
        }
        if(count>0) result.rgb=(c.rgb*(4-smoothing)+(total+count/2)/count*smoothing+2)/4;
    } else if(stage==4 && tag(p)!=1) {
        bool background=world(p);
        uint effect=background?worldEffect:modelEffect;
        uint intensity=min(background?worldIntensity:modelIntensity,100u);
        if((effect>=43 && effect<=45) || (effect>=48 && effect<=53) || (effect>=58 && effect<=60)) {
            uint order[8]={0,1,2,3,4,5,6,7};
            int block=int(p.x/(scale*8)*scale*8);
            if(effect==45) {
                for(uint a=1;a<8;++a) {
                    uint key=order[a],b=a;
                    uint light=luminance(manipulationSample(int2(block+int(key*scale),p.y),p));
                    while(b>0) {
                        if(luminance(manipulationSample(int2(block+int(order[b-1]*scale),p.y),p))<=light) break;
                        order[b]=order[b-1];--b;
                    }
                    order[b]=key;
                }
            }
            for(uint channel=0;channel<3;++channel) {
                int2 at=int2(p);
                if(effect==43) at=abs(int2(p)*2-int2(width,height)+1);
                else if(effect==44) at.x+=(int(channel)-1)*int(scale)*6;
                else if(effect==45) at.x=block+int(order[(p.x/scale)%8]*scale)+int(p.x%scale);
                else if(effect==48) {
                    int size=int(scale)*16;
                    int2 tile=int2(p)/size,local=int2(p)%size;
                    int rotation=(tile.x*3+tile.y*5)%4;
                    if(rotation==0) at=tile*size+int2(local.y,size-1-local.x);
                    else if(rotation==1) at=tile*size+size-1-local;
                    else if(rotation==2) at=tile*size+int2(size-1-local.y,local.x);
                } else if(effect==49) {
                    uint column=p.x/(scale*3);
                    int drip=int(((column*13)^(column>>1))%32)*int(scale);
                    at.y-=drip*int(p.y)/max(1,int(height)-1);
                } else if(effect==50) {
                    int2 ramp=16-abs(int2(p.y/scale,p.x/scale)%64-32);
                    int2 wave=ramp*(32-abs(ramp))/16;
                    at+=int2(wave.x*int(scale),wave.y*int(scale)/4);
                } else if(effect==51) {
                    int2 d=int2(p)-int2(width,height)/2;
                    int2 n=d*128/max(int2(1,1),int2(width,height));
                    int lens=192+(n.x*n.x+n.y*n.y)/64;
                    at=int2(width,height)/2+d*lens/256;
                } else if(effect==52) {
                    int size=int(scale)*12;
                    at.y=int(p.y)/size*size+(int(p.y)%size)/2+size/4;
                    at.x+=(int(p.y)/size%2?1:-1)*int(scale)*6;
                } else if(effect==53) {
                    int size=int(scale)*24;int2 tile=int2(p)/size;
                    if((tile.x+tile.y)%2) at.x=tile.x*size+size-1-int(p.x)%size;
                    else at.y=tile.y*size+size-1-int(p.y)%size;
                } else if(effect==58) {
                    int2 d=int2(p)-int2(width,height)/2;
                    int turn=clamp(96-(abs(d.x)+abs(d.y))/int(scale),0,96);
                    at+=int2(-d.y*turn/128,d.x*turn/128);
                } else if(effect==59) {
                    int2 d=int2(p)-int2(width,height)/2;
                    int radius=max(abs(d.x),abs(d.y))+min(abs(d.x),abs(d.y))*3/8;
                    int wave=16-abs((radius/int(scale))%64-32);
                    at+=d*wave*int(scale)/max(int(scale),radius);
                } else if(effect==60) {
                    int size=int(scale)*32;
                    int side=(int(p.x)%size+int(p.y)%size<size)?1:-1;
                    at+=side*int(scale)*int2(12,-8);
                }
                result[channel]=(c[channel]*(100-intensity)+manipulationSample(at,p)[channel]*intensity+50)/100;
            }
            outputImage[p]=float4(result)/255;return;
        }
        int light=luminance(c);
        bool edge=abs(light-int(luminance(colour(min(p+uint2(scale,0),uint2(width-1,height-1))))))>28
            || abs(light-int(luminance(colour(min(p+uint2(0,scale),uint2(width-1,height-1))))))>28;
        int3 value=int3(c.rgb);
        if(effect==1 || effect==12) {
            edge=false;int2 offsets[4]={int2(-1,0),int2(1,0),int2(0,-1),int2(0,1)};
            for(uint e=0;e<4;++e) {uint2 n=bounded(int2(p)+offsets[e]*int(scale));if(tag(n)!=1) edge=edge || light-int(luminance(colour(n)))>40;}
        }
        int peak=max(1,max(value.r,max(value.g,value.b)));
        if(effect==1) value=edge?value/4:(value*min(255,((peak+25)/51)*51)+peak/2)/peak;
        else if(effect==2) value=edge?16:light<30?24:235;
        else if(effect==3) value=edge?int3(35,255,255):value/5;
        else if(effect==4) value=light;
        else if(effect==5) value=clamp((value*4+bayer[((p.y/scale)%4)*4+(p.x/scale)%4]*16)/256,0,4)*255/4;
        else if(effect==6) value=edge?int3(130,220,255):int3(8,24,58)+(((p.x/scale)%16==0 || (p.y/scale)%16==0)?12:0);
        else if(effect==7) {
            int3 glow=0;
            for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
                uint2 n=bounded(int2(p)+int2(x,y)*int(scale));
                uint4 v=colour(n); uint peak=max(v.r,max(v.g,v.b));
                if(tag(n)!=1 && world(n)==background && peak>160) glow+=int3(v.rgb)*(int(peak)-160)/95;
            }
            value=min(255,value+glow/12);
        } else if(effect==8) value=min(255,int3(dot(value,int3(101,197,48)),dot(value,int3(89,176,43)),dot(value,int3(70,137,34)))/256);
        else if(effect==9) {
            int3 palette[5]={int3(8,5,40),int3(65,20,150),int3(220,30,70),int3(255,150,15),int3(255,255,210)};
            int band=min(light/64,3), fraction=light-band*64;
            value=(palette[band]*(64-fraction)+palette[band+1]*fraction)/64;
        } else if(effect==10) { value=int3(light/7,min(255,24+light*6/5),light/4); if((p.y/scale)%2) value=value*4/5; }
        else if(effect==11) value=96+value*5/8;
        else if(effect==12) value=edge?value/5:(value*min(255,((peak+31)/64)*64)+peak/2)/peak;
        else if(effect==13) { value=(int3(55,8,100)*(255-light)+int3(70,250,245)*light)/255; if(edge) value=int3(255,75,255); }
        else if(effect==14) value=min(255,((value+25)/51)*51);
        else if(effect==15) value=(int3(8,24,65)*(255-light)+int3(224,250,246)*light)/255;
        else if(effect==16) value=min(255,(value*value*(765-2*value)/65025)*int3(106,100,87)/100+int3(9,4,6));
        else if(effect==17) value=255-value;
        else if(effect==18) value=int3(value.r<128?value.r*2:(255-value.r)*2,value.g<128?value.g*2:(255-value.g)*2,value.b<128?value.b*2:(255-value.b)*2);
        else if(effect==19) value=light*int3(255,176,32)/255;
        else if(effect==20) value=light*int3(55,255,130)/255;
        else if(effect==21) value=(int3(8,22,70)*(255-light)+int3(224,246,240)*light)/255;
        else if(effect==22) value=(int3(30,10,8)*(255-light)+int3(255,190,120)*light)/255;
        else if(effect==23) value=(int3(35,12,65)*(255-light)+int3(250,215,255)*light)/255;
        else if(effect==24) {int3 palette[4]={int3(0,0,0),int3(0,170,170),int3(170,0,170),int3(255,255,255)};value=palette[min(3,light/64)];}
        else if(effect==25) {if((p.y/scale)%2) value=value*3/5;}
        else if(effect==31) {int3 tone=(int3(0,64,80)*(255-light)+int3(255,180,92)*light)/255;value=(value+tone*2)/3;}
        else if(effect==32) {int3 palette[4]={int3(15,56,15),int3(48,98,48),int3(139,172,15),int3(155,188,15)};value=palette[min(3,light/64)];}
        else if(effect==33) {
            int3 wash=0;int weight=0;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                uint2 n=bounded(int2(p)+int2(dx,dy)*int(scale));
                if(tag(n)==1 || world(n)!=background || abs(light-int(luminance(colour(n))))>32) continue;
                int w=dx==0 && dy==0?4:1;weight+=w;wash+=int3(colour(n).rgb)*w;
            }
            wash/=max(1,weight);value=24+min(255,((wash+15)/32)*32)*7/8;
        }
        else if(effect==34) value=edge?235:12+light/10;
        else if(effect==35) {uint2 n=uint2(p.x>=scale?p.x-scale:0,p.y);int neighbour=tag(n)==1?light:int(luminance(colour(n)));value=clamp(128+2*(light-neighbour),0,255);}
        else if(effect==36) {
            int3 overlay=light<128?2*value*light/255:255-2*(255-value)*(255-light)/255;
            int3 silver=(overlay+3*light)/4;value=clamp((silver-112)*7/4+112,0,255);
        }
        else if(effect==37) value=edge?8:clamp(((value*5/4-24+31)/64)*64+16,0,255);
        else if(effect==38) {
            int red=max(0,int(c.r)-int(c.b)),ink=(255-light)*3/4;
            value=clamp(int3(250,237,208)-ink*int3(205,155,65)/255-red*int3(0,110,120)/255,0,255);
        }
        else if(effect==39) {value=(edge?255:24+light*3/4)*int3(32,210,255)/255;if((p.y/scale)%4==3)value=value*2/5;}
        else if(effect==41) value=edge?32:clamp(246-(255-light)*(255-light)/510,0,255);
        else if(effect==40 || effect==42) {
            int counts[4]={0,0,0,0};int3 sums[4]={int3(0,0,0),int3(0,0,0),int3(0,0,0),int3(0,0,0)};
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                int2 centre=effect==40?int2(p/(scale*3)*(scale*3)+scale):int2(p);
                uint2 n=bounded(centre+int2(dx,dy)*int(scale));
                if(tag(n)==1 || world(n)!=background)continue;
                int bin=effect==40?0:int(luminance(colour(n)))/64;
                ++counts[bin];sums[bin]+=int3(colour(n).rgb);
            }
            int best=0;for(int b=1;b<4;++b)if(counts[b]>counts[best])best=b;
            int3 wash=counts[best]>0?sums[best]/max(1,counts[best]):value;
            if(effect==40)value=((p.x/scale)%3==0 || (p.y/scale)%3==0)?wash*2/3:min(255,wash+10);
            else value=clamp((wash-128)*6/5+128,0,255);
        }
        if(effect>=54 && effect<=57) {
            int3 phase=(light*2+int3(0,128,256))%384;
            int3 spectrum=clamp(255-abs(phase-192)*4,0,255);
            int shine=max(0,light-128),specular=shine*shine/64;
            if(effect==54) value=min(255,24+spectrum*3/4+specular/2+(edge?48:0));
            else if(effect==55) value=min(255,int3(c.rgb)/8+light/4+int3(30,65,85)+specular/2+(edge?75:0));
            else if(effect==56) value=min(255,int3(9,7,18)+light/16+specular*3/4+(edge?36:0));
            else value=min(255,125+light/4+spectrum/5+specular/4+(edge?18:0));
        }
        if(effect>=61 && effect<=63) {
            int shine=max(0,light-128),specular=shine*shine/64;
            if(effect==61) value=int3(40,3,12)+light*int3(180,12,42)/255+specular*int3(255,115,145)/255+(edge?int3(50,15,22):int3(0,0,0));
            else if(effect==62) value=int3(6,30,20)+light*int3(65,145,90)/255+specular*int3(120,230,170)/255+(edge?int3(20,35,25):int3(0,0,0));
            else value=25+light*int3(200,190,166)/255+specular/3+(edge?18:0);
            value=min(255,value);
        }
        result.rgb=(c.rgb*(100-intensity)+uint3(value)*intensity+50)/100;
    } else if(stage==5 && aa>0 && p.x>0 && p.y>0 && p.x+1<width && p.y+1<height) {
        uint4 left=colour(p-uint2(1,0)),right=colour(p+uint2(1,0)),up=colour(p-uint2(0,1)),down=colour(p+uint2(0,1));
        int l=luminance(left),r=luminance(right),u=luminance(up),d=luminance(down),v=luminance(c);
        uint low=min(v,min(min(l,r),min(u,d))),high=max(v,max(max(l,r),max(u,d)));
        uint floor=aa==1?20:aa==3?6:12, divisor=aa==1?6:aa==3?12:8,weight=aa==1?6:aa==3?1:2;
        if(high-low>=max(floor,high/divisor)) result.rgb=(c.rgb*weight+(abs(l-r)>=abs(u-d)?up.rgb+down.rgb:left.rgb+right.rgb))/(weight+2);
    }
    outputImage[p]=float4(result)/255;
}
