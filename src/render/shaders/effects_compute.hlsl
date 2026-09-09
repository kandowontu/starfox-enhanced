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
};
uint indexOf(uint2 p) { return p.y*width+p.x; }
uint tag(uint2 p) {
    uint i=indexOf(p);
    return (layerTags.Load(i & ~3u) >> ((i & 3u)*8)) & 255u;
}
bool model(uint2 p) { uint t=tag(p); return t==0 || t==4; }
bool world(uint2 p) { uint t=tag(p); return t==2 || t==3; }
bool art(uint2 p) { uint t=tag(p); return t==1 || t==2 || t==4; }
uint4 colour(uint2 p) { return uint4(inputImage.Load(int3(p,0))*255+.5); }
uint luminance(uint4 c) { return (c.r*77+c.g*150+c.b*29)/256; }
uint2 bounded(int2 p) { return uint2(clamp(p,int2(0,0),int2(width-1,height-1))); }
static const int bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
bool surfaceAt(int2 p,out float4 sample) {
    sample=0;
    int2 local=p-int2(surfaceX,surfaceY);
    if(any(p<0) || any(p>=int2(width,height)) || any(local<0) || any(local>=int2(surfaceWidth,surfaceHeight))) return false;
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
    if(stage==17) {
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
    } else if(stage==15) {
        int sy=int(p.y)-shadowY;
        uint layer=tag(p);
        if(p.x<shadowWidth && sy>=0 && sy<int(shadowHeight) && (layer==0 || layer==2 || layer==4)) {
            uint i=uint(sy)*shadowWidth+p.x;
            uint shade=(shadowMask.Load(i&~3u)>>((i&3u)*8))&255u;
            result.rgb=c.rgb*(255-shade)/255;
        }
    } else if(stage==14 && (overlayFilter || art(p))) {
        uint factor=filter==2?clamp(scale,2u,6u):max(scale,2u);uint4 v;
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
        int light=luminance(c);
        bool edge=abs(light-int(luminance(colour(min(p+uint2(scale,0),uint2(width-1,height-1))))))>28
            || abs(light-int(luminance(colour(min(p+uint2(0,scale),uint2(width-1,height-1))))))>28;
        int3 value=int3(c.rgb);
        if(effect==1) value=edge?value/6:min(255,((value+21)/43)*43);
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
        else if(effect==12) value=(edge || ((p.x/scale)%3==1 && (p.y/scale)%3==1 && light<180))?18:min(255,((value+31)/64)*64);
        else if(effect==13) { value=(int3(55,8,100)*(255-light)+int3(70,250,245)*light)/255; if(edge) value=int3(255,75,255); }
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
