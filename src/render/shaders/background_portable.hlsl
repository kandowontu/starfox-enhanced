#include "raster_jitter.hlsli"
StructuredBuffer<uint> memory : register(t0,space0);
RWStructuredBuffer<uint> pixels : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,scale,bits;
    uint mapBase,charBase,tilesWide,tilesHigh;
    int scrollX,scrollY,originX,clipLeft;
    int clipRight,mosaic;uint tileEdge,priority;
    uint tag,enabled,transparentBlack,textOutline;
    uint logicalWidth,logicalHeight;float2 rasterJitter;
};
uint readByte(uint address) {
    address&=65535u;
    return (memory[address>>2]>>((address&3u)*8u))&255u;
}
int mosaicAt(int v) {
    uint step=uint(mosaic);
    return v<0?-int((uint(-v)+step-1u)/step)*mosaic:int(uint(v)/step)*mosaic;
}
uint sampleGlyph(int2 p) {
    if(p.x<clipLeft || p.x>=clipRight || p.y<0 || p.y>=int(logicalHeight)) return 0;
    // All tilemap dimensions are powers of two, so unsigned masking also
    // gives the source's positive modulo for negative scroll/mosaic values.
    uint x=uint(mosaicAt(p.x-originX)+scrollX)&(tilesWide*tileEdge-1u);
    uint y=uint(mosaicAt(p.y)+scrollY)&(tilesHigh*tileEdge-1u);
    uint tx=x/tileEdge,ty=y/tileEdge;
    uint entry=((ty>>5)*(tilesWide>>5)+(tx>>5))*1024u+(ty&31u)*32u+(tx&31u);
    uint address=((mapBase+entry)&32767u)*2u;
    uint tile=readByte(address)|(readByte(address+1u)<<8u);
    if(priority && (((tile>>13)&1u)!=(priority==2u?1u:0u))) return 0;
    uint paletteIndex=(tile>>10)&7u;
    x&=tileEdge-1u;y&=tileEdge-1u;
    if(tile&16384u) x=tileEdge-1u-x;
    if(tile&32768u) y=tileEdge-1u-y;
    uint character=((tile&1023u)+(x>>3)+(y>>3)*16u)&1023u;
    x&=7u;y&=7u;
    uint base=charBase*2u+character*(bits*8u)+y*2u;
    uint colour=0,mask=128u>>x;
    for(uint pair=0;pair<bits/2u;++pair) {
        if(readByte(base+pair*16u)&mask) colour|=1u<<(pair*2u);
        if(readByte(base+pair*16u+1u)&mask) colour|=2u<<(pair*2u);
    }
    if(!colour) return 0;
    uint value=bits==8u?colour:paletteIndex*(1u<<bits)+colour;
    if(transparentBlack && !(memory[16384u+value]&32767u)) return 0;
    return value;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint index=id.y*width+id.x;pixels[index]=0;
    int2 p=int2(id.xy*uint2(logicalWidth,logicalHeight)/uint2(width,height));
    if(any(rasterJitter!=0)) p=clamp(int2(jitterFloor(id.x,logicalWidth,width,rasterJitter.x,true),
        jitterFloor(id.y,logicalHeight,height,rasterJitter.y,true)),int2(0,0),int2(logicalWidth,logicalHeight)-1);
    if(!enabled || p.x<clipLeft || p.x>=clipRight) return;
    uint value=sampleGlyph(p);
    if(textOutline) {
        if(value) {
            uint c=memory[16384u+value];
            uint r=c&31u,g=(c>>5u)&31u,b=(c>>10u)&31u;
            if(r+g+b<=24u || 54u*r+183u*g+19u*b<16u*256u) value=254u;
        } else if(sampleGlyph(p+int2(-1,0)) || sampleGlyph(p+int2(1,0))
            || sampleGlyph(p+int2(0,-1)) || sampleGlyph(p+int2(0,1))) value=255u;
    }
    if(!value) return;
    pixels[index]=value|(tag<<8u)|0x04000000u;
}
