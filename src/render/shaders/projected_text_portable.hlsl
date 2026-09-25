#include "geometry_fp64.hlsli"
#include "raster_jitter.hlsli"
// Two passes: stage 0 projects one immutable text object; stage 1 fills pixels.
// The caller must end the first compute pass before dispatching the second.
// Glyph rows are uint32, sixteen per character; zero rows preserve blank tokens.
[[vk::binding(0,0)]] StructuredBuffer<uint> glyphs:register(t0,space0);
// Four rectangle words followed by width*height packed coverage pixels.
[[vk::binding(0,1)]] RWStructuredBuffer<uint> pixels:register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings:register(b0,space2) {
    uint2 ownerX,ownerY,ownerZ;
    uint width,height;
    uint scale,count;int characterSize;uint colour;
    float eyeX,convergence;uint stage,tag;
    uint referenceWidth,referenceHeight;float2 rasterJitter;
};
Sf64 num(float x) {return sf_from_float_bits(asuint(x));}
Sf64 raw(uint2 x) {return sf_make(x.x,x.y);}
int truncateText(Sf64 x) {
    int exponent=int((x.hi>>20)&2047)-1023;
    if(exponent<0) return 0;
    if(exponent>30) return (x.hi>>31)!=0?-2147483647:2147483647;
    uint magnitude=sf_right(sf_make(x.lo,(x.hi&0xfffff)|0x100000),uint(52-exponent)).lo;
    return (x.hi>>31)!=0?-int(magnitude):int(magnitude);
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(stage==0) {
        if(id.x!=0) return;
        pixels[0]=pixels[1]=pixels[2]=pixels[3]=0;
        Sf64 z=raw(ownerZ);
        if(!sf_valid(z) || (z.hi>>31)!=0 || sf_less(z,num(128)) || characterSize<=0 || count==0) return;
        Sf64 focal=num(float(256*scale));
        int dimension=truncateText(sf_div(sf_mul(num(float(characterSize)),focal),z));
        if(dimension<=0) return;
        Sf64 px=sf_div(sf_mul(raw(ownerX),focal),z);
        if(eyeX!=0) px=sf_add(px,sf_mul(num(float(256*scale)*eyeX),
            sf_sub(sf_div(num(1),num(convergence)),sf_div(num(1),z))));
        int cx=int(referenceWidth/2)+truncateText(px);
        int cy=int(referenceHeight/2)+truncateText(sf_div(sf_mul(raw(ownerY),focal),z));
        pixels[0]=asuint(cx-dimension*int(count)/2);
        pixels[1]=asuint(cy-dimension/2);pixels[2]=uint(dimension);pixels[3]=1;
        return;
    }
    if(id.x>=width*height) return;
    uint pixel=0;
    if(pixels[3]!=0) {
        int x=int((id.x%width)*referenceWidth/width)-asint(pixels[0]);
        int y=int((id.x/width)*referenceHeight/height)-asint(pixels[1]);
        if(any(rasterJitter!=0)) {
            x=jitterFloor(id.x%width,referenceWidth,width,rasterJitter.x,true)-asint(pixels[0]);
            y=jitterFloor(id.x/width,referenceHeight,height,rasterJitter.y,true)-asint(pixels[1]);
        }
        int dimension=int(pixels[2]);
        if(x>=0 && y>=0 && y<dimension && uint(x/dimension)<count) {
            uint glyph=uint(x/dimension),column=uint((x%dimension)*16/dimension);
            uint row=uint(y*16/dimension);
            if((glyphs[glyph*16+row]&(0x8000U>>column))!=0)
                pixel=colour|(tag<<8)|(1U<<26);
        }
    }
    pixels[4+id.x]=pixel;
}
