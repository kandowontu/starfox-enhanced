// The caller supplies backdropWord(byteAddress) for its immutable RGBA buffer.
float3 backdropMoonSurface(float3 colour,float2 uv,float4 fade) {
    if(fade.z>=0 || uv.y<2) return colour;
    float t=saturate((uv.y-2)/max(.0001f,fade.y));t=t*t*(3-2*t);
    return (colour+(255-colour)*(.32f*(1-t)))*(1-.65f*t);
}
float backdropMoonOpacity(float2 uv,float4 fade) {
    if(fade.z>=0 || uv.y<2) return 1;
    float t=saturate((uv.y-2-fade.x)/max(.0001f,fade.y-fade.x));
    return 1-t*t*(3-2*t);
}
bool backdropInEllipse(float2 p,float4 ellipse) {
    if(ellipse.z<=0 || ellipse.w<=0) return false;
    float2 d=(p-ellipse.xy)/ellipse.zw;
    return dot(d,d)<=1;
}
float2 cityMoonCoordinates(float x,float y,float4 scroll) {
#define SF_CITY_MOON(cx,cy,r,p) \
    { float2 d=(float2(x,y)+scroll.xy-float2(cx,cy))/float(r); \
      if(dot(d,d)<=1) return float2(float(p)*.5f+(d.x+1)*.25f,2+(d.y+1)*.5f); }
#include "../../../include/starfox/render/ex_city_moons.inc"
#undef SF_CITY_MOON
    return float2(-1,-1);
}
float2 facePlanetCoordinates(float x,float y,float4 t) {
    float determinant=1-t.x*t.y;
    if(abs(determinant)<.001f) return float2(-1,-1);
#define SF_FACE_PLANET_REGION(l,top,r,b) \
    if((r-l)==(b-top)) { \
        float2 center=float2((l+r)*.5f,(top+b)*.5f); \
        float sx=(center.x-t.z-t.x*(center.y-t.w))/determinant; \
        float sy=center.y-t.w-t.y*sx; \
        float2 d=float2(x-sx,y-sy); \
        if(dot(d,d)<=(r-l)*(r-l)*.25f) return center+d; \
    }
#include "../../../include/starfox/render/ex_face_planet_regions.inc"
#undef SF_FACE_PLANET_REGION
    float2 source=float2(x+t.x*y+t.z,y+t.y*x+t.w);
#define SF_FACE_PLANET_REGION(l,top,r,b) \
    if((r-l)==(b-top) && source.x>=l && source.x<r && source.y>=top && source.y<b) return float2(0,0);
#include "../../../include/starfox/render/ex_face_planet_regions.inc"
#undef SF_FACE_PLANET_REGION
    return float2(-1,-1);
}
float2 cloudLimbCoordinates(float x,float y,float4 t) {
    float determinant=1-t.x*t.y;
    if(abs(determinant)<.001f) return float2(-1,-1);
    const float4 regions[2]={float4(80,264,128,312),float4(160,320,240,352)};
    for(uint i=0;i<2;++i) {
        float4 r=regions[i];float2 center=(r.xy+r.zw)*.5f;
        float sx=(center.x-t.z-t.x*(center.y-t.w))/determinant;
        float sy=center.y-t.w-t.y*sx;
        if(t.x==0 && t.y==0) {
            float wrapped=sx-512*floor((sx+256)/512);
            if(wrapped>=-128 && wrapped<128) sx=wrapped;
        }
        float2 p=center+float2(x-sx,y-sy);
        if(all(p>=r.xy) && all(p<r.zw)) return p;
    }
    if(t.x==0 && t.y==0) return float2(-1,-1);
    float2 p=float2(x+t.x*y+t.z,y+t.y*x+t.w);
    for(uint i=0;i<2;++i)
        if(all(p>=regions[i].xy) && all(p<regions[i].zw)) return float2(0,0);
    return float2(-1,-1);
}
float3 backdropLimbColour(float3 surface,uint4 ramp[4]) {
    float shade=1+14*(1-saturate(surface.r/255.f));
    uint a=uint(shade),b=min(a+1,15u);
    uint ca=ramp[a/4][a%4],cb=ramp[b/4][b%4];
    return lerp(float3(ca&255,(ca>>8)&255,(ca>>16)&255),
        float3(cb&255,(cb>>8)&255,(cb>>16)&255),shade-float(a));
}
bool backdropCovers(float x,float y,float horizon,float slope,float4 projection,float4 keep0,float4 keep1) {
    if(projection.w==7) return true;
    if(projection.w==6 || projection.w==8) return y<horizon+slope*x;
    if(projection.w==5) return facePlanetCoordinates(x,y,keep1).x>=0;
    if(projection.w==9) return cloudLimbCoordinates(x,y,keep1).x>=0;
    if(projection.w==4) return backdropInEllipse(float2(x+keep1.x*y+keep1.z,y+keep1.y*x+keep1.w),keep0);
    if(projection.w==3) return backdropInEllipse(float2(x,y),keep0);
    return (projection.w!=0 || y<horizon+slope*x)
        && !backdropInEllipse(float2(x,y),keep0) && !backdropInEllipse(float2(x,y),keep1);
}
float2 backdropCoordinates(float x,float y,float horizon,float slope,float scroll,float4 projection) {
    float inverse=rsqrt(1+slope*slope),dy=y-horizon;
    float v=projection.z+inverse*(dy-slope*x)*projection.y;
    return float2((inverse*(x+slope*dy)+scroll)*projection.x,projection.w>=7?saturate(v):v);
}
float2 backdropCoordinates(float x,float y,float horizon,float slope,float scroll) {
    return backdropCoordinates(x,y,horizon,slope,scroll,float4(1/512.f,1/160.f,1.f,0.f));
}
float2 backdropCoordinates(float x,float y,float horizon,float slope,float scroll,float4 projection,float4 keep0,float4 keep1) {
    if(projection.w==8) {
        float2 uv=cityMoonCoordinates(x,y,keep1);
        if(uv.y>=2) return uv;
    }
    if(projection.w==6 || projection.w==7) {
        if(backdropInEllipse(float2(x,y),keep0)) return float2(((x-keep0.x)/keep0.z+1)*.25f,2+((y-keep0.y)/keep0.w+1)*.5f);
        if(backdropInEllipse(float2(x,y),keep1)) return float2(.5f+((x-keep1.x)/keep1.z+1)*.25f,2+((y-keep1.y)/keep1.w+1)*.5f);
    }
    if(projection.w==5) return facePlanetCoordinates(x,y,keep1)/512.f;
    if(projection.w==9) return cloudLimbCoordinates(x,y,keep1)/512.f;
    if(projection.w==4) return float2((x+keep1.x*y+keep1.z-keep0.x+scroll)*projection.x,
        projection.z+(y+keep1.y*x+keep1.w-keep0.y)*projection.y);
    return backdropCoordinates(x,y,horizon,slope,scroll,projection);
}
float3 backdropRgb(uint p) { return float3(p&255u,(p>>8)&255u,(p>>16)&255u); }
float3 backdropRampMoonColour(float3 surface,float2 uv,uint4 ramp[4]) {
    bool blue=uv.x>=.5f;
    uint count=ramp[1].z==1?7u:blue?1u:2u,base=ramp[1].z==2 && blue?10u:7u;
    float shade=float(count)*(1-saturate(surface.r/255.f));
    uint a=uint(shade),b=min(a+1,count),ia=base+a,ib=base+b;
    return lerp(backdropRgb(ramp[ia/4][ia%4]),backdropRgb(ramp[ib/4][ib%4]),shade-float(a));
}
float3 backdropMoonColour(float3 surface,float2 uv,uint bright0,uint bright1,uint dark0,uint dark1,uint phase) {
    uint body=uv.x<.5f?0:1;
    float3 bright=backdropRgb(body==0?bright0:bright1),dark=backdropRgb(body==0?dark0:dark1);
    float x=(uv.x-float(body)*.5f)*4-1;
    float light=saturate((x+.05f)/.4f);light=light*light*(3-2*light);
    return phase!=0?lerp(dark,bright,light)*(.8f+.2f*surface/255.f):surface*bright/255.f;
}
float3 backdropNebulaShade(uint4 ramp[4],uint bank,uint n) {
    if(n>=7)return 0;
    uint i=1+bank*7+n;return backdropRgb(ramp[i/4][i%4]);
}
float3 backdropNebulaColour(float3 c,uint4 ramp[4]) {
    float peak=max(c.r,max(c.g,c.b));
    float chroma=(peak-min(c.r,min(c.g,c.b)))/max(1.f,peak);
    float cool=saturate(.5f+2.f*(c.b-c.r)/max(1.f,peak));
    float shade=7.f*(1.f-saturate(peak/180.f));uint a=min(uint(shade),6u),b=a+1;
    float3 warm=lerp(backdropNebulaShade(ramp,0,a),backdropNebulaShade(ramp,0,b),shade-float(a));
    float3 cold=lerp(backdropNebulaShade(ramp,1,a),backdropNebulaShade(ramp,1,b),shade-float(a));
    return lerp(c,lerp(warm,cold,cool),saturate(chroma*4.f));
}
float3 backdropBilinear(float x,float y,uint w,uint h,uint base) {
    uint x0=min(uint(x),w-1),x1=min(x0+1,w-1),y0=uint(y),y1=min(y0+1,h-1);
    float fx=frac(x),fy=y-float(y0);
    return lerp(lerp(backdropRgb(backdropWord(base+(y0*w+x0)*4)),backdropRgb(backdropWord(base+(y0*w+x1)*4)),fx),
        lerp(backdropRgb(backdropWord(base+(y1*w+x0)*4)),backdropRgb(backdropWord(base+(y1*w+x1)*4)),fx),fy);
}
float3 backdropSample(float u,float v,uint w,uint h,uint base) {
    uint overlap=max(1u,w/32),period=max(1u,w-overlap);
    float x=frac(u)*float(period),y=clamp(v,0.f,1.f)*float(h-1);
    float3 c=backdropBilinear(x,y,w,h,base);
    if(x<float(overlap)) {
        float t=x/float(overlap);t=t*t*(3-2*t);
        c=lerp(backdropBilinear(float(period)+x,y,w,h,base),c,t);
    }
    return c;
}
float3 backdropSample(float u,float v,uint w,uint h,uint base,bool repeatVertical) {
    if(!repeatVertical || h<2) return backdropSample(u,v,w,h,base);
    uint overlap=max(1u,h/32),period=h-overlap;
    float y=frac(v)*float(period);
    float3 c=backdropSample(u,y/float(h-1),w,h,base);
    if(y<float(overlap)) {
        float t=y/float(overlap);t=t*t*(3-2*t);
        c=lerp(backdropSample(u,(float(period)+y)/float(h-1),w,h,base),c,t);
    }
    return c;
}
float3 backdropSample(float u,float v,uint w,uint h,uint base,float mode) {
    if(mode==6 || mode==7 || mode==8) {
        if(v<2) return backdropSample(u,v,w,h/2,base);
        return backdropBilinear(saturate(u)*float(w-1),float(h/2)+saturate(v-2)*float(h/2-1),w,h,base);
    }
    if(mode==3 || mode==4 || mode==5 || mode==9) return backdropBilinear(saturate(u)*float(w-1),saturate(v)*float(h-1),w,h,base);
    return backdropSample(u,v,w,h,base,mode==2);
}
