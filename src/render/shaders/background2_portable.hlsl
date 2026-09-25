#include "geometry_fp64.hlsli"
#include "raster_jitter.hlsli"
StructuredBuffer<uint> memory : register(t0,space0);
RWStructuredBuffer<uint> pixels : register(u0,space1);
RWStructuredBuffer<uint> prepared : register(u1,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,scale,phase;
    uint mapBase,charBase,tilesWide,tilesHigh;
    int scrollX,scrollY,originX,clipLeft;
    int clipRight,mosaic;uint tileEdge,priority;
    uint tag,enabled,transparentBlack,flags;
    uint singleRows,regionCount;int registerX,registerY;
    uint terrainFirst,terrainLast,logicalWidth,logicalHeight;
    float2 rasterJitter;uint skySourceMin,jitterPadding;
};
uint readByte(uint address) {address&=65535u;return (memory[address>>2]>>((address&3u)*8u))&255u;}
uint readWord(uint address) {address=(address&32767u)*2u;return readByte(address)|(readByte(address+1u)<<8u);}
int mosaicAt(int v) {
    // Keep the floor division explicit on both shader backends. Signed
    // remainder lowering can otherwise disagree at negative mosaic edges.
    uint step=uint(mosaic);
    return v<0?-int((uint(-v)+step-1u)/step)*mosaic:int(uint(v)/step)*mosaic;
}
int difference(int to,int from) {int d=(to-from)&8191;return d>4095?d-8192:d;}
Sf64 number(int value) {return sf_from_float_bits(asuint(float(value)));}
int truncNumber(Sf64 x) {
    int exponent=int((x.hi>>20)&2047)-1023;
    if(exponent<0) return 0;
    if(exponent>30) return (x.hi>>31)?-2147483647:2147483647;
    uint magnitude=sf_right(sf_make(x.lo,(x.hi&0xfffff)|0x100000),uint(52-exponent)).lo;
    return (x.hi>>31)?-int(magnitude):int(magnitude);
}
int roundNumber(Sf64 x) {
    bool negative=(x.hi>>31)!=0;x.hi&=0x7fffffff;
    int integer=truncNumber(x);
    if(!sf_less(sf_sub(x,number(integer)),sf_from_float_bits(asuint(0.5)))) ++integer;
    return negative?-integer:integer;
}
uint tileAt(uint x,uint y) {
    uint tx=x/tileEdge,ty=y/tileEdge;
    return readWord(mapBase+((ty>>5)*(tilesWide>>5)+(tx>>5))*1024u+(ty&31u)*32u+(tx&31u));
}
uint tileInk(uint tile,uint x,uint y) {
    x&=tileEdge-1;y&=tileEdge-1;
    if(tile&16384u) x=tileEdge-1-x;if(tile&32768u) y=tileEdge-1-y;
    uint character=((tile&1023u)+(x>>3)+(y>>3)*16u)&1023u;
    uint base=charBase*2u+character*32u+(y&7u)*2u,mask=128u>>(x&7u),ink=0;
    if(readByte(base)&mask) ink|=1;if(readByte(base+1)&mask) ink|=2;
    if(readByte(base+16)&mask) ink|=4;if(readByte(base+17)&mask) ink|=8;
    return ink;
}
void prepare() {
    uint darkest=0xffffffffu,black=0;
    for(uint i=0;i<256;++i) {
        uint c=memory[16384u+i],luma=77u*(c&31u)+150u*((c>>5)&31u)+29u*((c>>10)&31u);
        if(luma<darkest) {darkest=luma;black=i;if(!luma) break;}
    }
    uint wall=black;
    if(flags&32u) {
        uint x=uint((flags&8u)?asint(memory[16640u+112]):registerX)&(tilesWide*tileEdge-1);
        uint y=uint(112+((flags&16u)?asint(memory[16864u+112]):registerY))&(tilesHigh*tileEdge-1);
        uint tile=tileAt(x,y),ink=tileInk(tile,x,y);if(ink) wall=((tile>>10)&7u)*16u+ink;
    }
    Sf64 sx=number(0),sy=sx,sxx=sx,sxy=sx;int count=0,prev=0,unwrapped=0;
    int first=-1,last=-1,firstRaw=0,lastRaw=0;
    if(flags&4u) for(int i=0;i<32;++i) {
        uint word=readWord(0x2fa0u+uint(i));if(!(word&0x4000u)) continue;
        int raw=int(word&8191u);unwrapped=count?unwrapped+difference(raw,prev):raw;
        Sf64 x=number(i+1),y=number(unwrapped);
        sx=sf_add(sx,x);sy=sf_add(sy,y);sxx=sf_add(sxx,sf_mul(x,x));sxy=sf_add(sxy,sf_mul(x,y));
        if(!count) {first=i;firstRaw=raw;}last=i;lastRaw=raw;prev=raw;++count;
    }
    Sf64 slope=number(0),intercept=slope;
    if(count) {
        Sf64 n=number(count),denom=sf_sub(sf_mul(n,sxx),sf_mul(sx,sx));
        if(count>1) slope=sf_div(sf_sub(sf_mul(n,sxy),sf_mul(sx,sy)),denom);
        intercept=sf_div(sf_sub(sy,sf_mul(slope,sx)),n);
    }
    prepared[0]=intercept.lo;prepared[1]=intercept.hi;prepared[2]=slope.lo;prepared[3]=slope.hi;
    prepared[4]=uint(count);prepared[5]=black;prepared[6]=wall;
    prepared[7]=asuint(first>=0 && last!=first?difference(lastRaw,firstRaw):0);
    prepared[8]=uint(first>=0 && last!=first?last-first:1);
}
int columnScroll(int screenX,bool expanded) {
    if(!(flags&4u)) return -2147483647;
    int sampleX=clamp(mosaicAt(screenX-originX)+originX,clipLeft,clipRight-1);
    int coordinate=sampleX-originX+(scrollX&7);
    if(expanded && prepared[4]) {
        Sf64 at=sf_div(number(coordinate),number(8));
        return roundNumber(sf_add(sf_make(prepared[0],prepared[1]),sf_mul(sf_make(prepared[2],prepared[3]),at)))&8191;
    }
    int column=coordinate>=0?coordinate/8:-((-coordinate+7)/8);
    if(column>=1 && column<=32) {uint v=readWord(0x2fa0u+uint(column-1));return (v&0x4000u)?int(v&8191u):-2147483647;}
    int anchor=-1,distance=0;
    if(column<=0) {uint v=readWord(0x2fa0u);if(v&0x4000u) {anchor=int(v&8191u);distance=expanded?column-1:min(column+1,0);}}
    if(column>32) {uint v=readWord(0x2fbfu);if(v&0x4000u) {anchor=int(v&8191u);distance=column-32;}}
    return anchor>=0?(anchor+asint(prepared[7])*distance/int(prepared[8]))&8191:-2147483647;
}
void put(uint x,uint y,uint colour,bool covered,bool terrain) {
    uint value=covered?(colour|(tag<<8)|0x04000000u|(terrain?0x08000000u:0)):0;
    uint2 first=(uint2(x,y)*uint2(width,height)+uint2(logicalWidth,logicalHeight)-1)/uint2(logicalWidth,logicalHeight);
    uint2 last=((uint2(x,y)+1)*uint2(width,height)+uint2(logicalWidth,logicalHeight)-1)/uint2(logicalWidth,logicalHeight);
    if(any(rasterJitter!=0)) {
        first=uint2(clamp(int2(jitterCeil(int(x),width,logicalWidth,rasterJitter.x,true),jitterCeil(int(y),height,logicalHeight,rasterJitter.y,true)),0,int2(width,height)));
        last=uint2(clamp(int2(jitterCeil(int(x+1),width,logicalWidth,rasterJitter.x,true),jitterCeil(int(y+1),height,logicalHeight,rasterJitter.y,true)),0,int2(width,height)));
        if(x==0) first.x=0;if(y==0) first.y=0;
        if(x+1==logicalWidth) last.x=width;if(y+1==logicalHeight) last.y=height;
    }
    for(uint by=first.y;by<last.y;++by) for(uint bx=first.x;bx<last.x;++bx) pixels[by*width+bx]=value;
}
void put(uint x,uint y,uint colour,bool covered) {put(x,y,colour,covered,false);}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(!phase) {if(!id.x) prepare();return;}
    uint x=id.x;if(x>=logicalWidth) return;
    bool extend=(flags&1u)!=0,wrapX=(flags&2u)!=0,tunnel=(flags&32u)!=0;
    bool expanded=extend && logicalWidth>256 && (flags&4u),ground=expanded && logicalHeight>192;
    int logicalX=int(x)-originX,sampleX=mosaicAt(tunnel && extend && priority!=2u?clamp(logicalX,0,255):logicalX);
    bool outside=logicalX<0 || logicalX>=256;
    uint mapWidth=tilesWide*tileEdge,mapHeight=tilesHigh*tileEdge;
    int columnY=columnScroll(tunnel && extend?sampleX+originX:int(x),expanded),previous=-1;uint lastGround=0;bool wrapped=false,lastTerrain=false;
    for(uint y=0;y<logicalHeight;++y) {
        put(x,y,0,false);
        if(!enabled || int(x)<clipLeft || int(x)>=clipRight) continue;
        if(tunnel && extend && outside && priority==2u) continue;
        if(tunnel && extend && outside && priority!=2u) put(x,y,prepared[6],true);
        int sampleY=mosaicAt(int(y));
        int rowX=(flags&8u) && sampleY>=0 && sampleY<224?asint(memory[16640u+uint(sampleY)]):scrollX;
        int rowY=(flags&16u)?asint(memory[16864u+uint(clamp(sampleY,0,223))]):scrollY;
        uint sourceY=uint(sampleY+(columnY!=-2147483647?columnY:rowY))&(mapHeight-1);
        if(expanded && y<144 && outside && skySourceMin<mapHeight) sourceY=max(sourceY,skySourceMin);
        if(ground) {
            if(y>=144 && previous>=0 && int(sourceY)<previous && previous-int(sourceY)>int(mapHeight/2)) wrapped=true;
            previous=int(sourceY);
            if(wrapped) {if(lastGround) put(x,y,lastGround,true,lastTerrain);continue;}
        }
        int unwrappedX=sampleX+rowX;
        bool outMap=unwrappedX<0 || unwrappedX>=int(mapWidth);
        if((flags&384u) && extend && outside && (outMap || (flags&256u))) {
            uint cellX=uint(unwrappedX)>>5u;
            int worldY=sampleY+(columnY!=-2147483647?columnY:rowY);
            uint cellY=uint(worldY)>>5u;
            uint seed=cellX*0x9e3779b9u ^ cellY*0x85ebca6bu;
            seed^=seed>>16u;seed*=0x7feb352du;seed^=seed>>15u;
            unwrappedX=int((seed&7u)*32u)+(unwrappedX&31);
            sourceY=((seed>>3u)&3u)*32u+(uint(worldY)&31u);
            if(flags&256u) {uint patch=(seed>>3u)%3u;sourceY=(patch?128u+patch*32u:0u)+(uint(worldY)&31u);}
            outMap=false;
        }
        if(!wrapX && outMap) continue;
        if(singleRows && y<singleRows && outside && outMap) {put(x,y,prepared[5],true);continue;}
        uint sourceX=uint(unwrappedX)&(mapWidth-1);
        if((flags&64u) && (flags&16u) && !tunnel && extend && outside) {
            int waterX=int(uint(128+rowX)&(mapWidth-1))+sampleX-128;sourceX=uint(clamp(waterX,0,int(mapWidth)-1));
        }
        uint tile=tileAt(sourceX,sourceY);
        if(priority && !(tunnel && extend && priority==1u) && (((tile>>13)&1u)!=(priority==2u?1u:0u))) {if(ground && y>=144 && lastGround) put(x,y,lastGround,true,lastTerrain);continue;}
        uint ink=tileInk(tile,sourceX,sourceY);
        if(!ink) {if(ground && y>=144 && lastGround) put(x,y,lastGround,true,lastTerrain);continue;}
        uint colour=((tile>>10)&7u)*16u+ink;
        int uniqueX=sampleX+int(uint(rowX+int(mapWidth/2))&(mapWidth-1))-int(mapWidth/2);
        if((extend && outside) || (flags&512u)) for(uint r=0;r<regionCount;++r) {
            uint base=17088u+r*8u;
            int offset=asint(memory[base+7]);bool suppressAll=offset>=0 && (offset&0x40000000)!=0;
            bool suppressEvery=offset>=0 && (offset&0x20000000)!=0;
            if(suppressAll || suppressEvery) offset=offset&~0x60000000;
            if((suppressEvery || (extend && outside && (suppressAll || uniqueX<0 || uniqueX>=int(mapWidth))))
                && int(sourceX)>=asint(memory[base]) && int(sourceY)>=asint(memory[base+1])
                && int(sourceX)<asint(memory[base+2]) && int(sourceY)<asint(memory[base+3])
                && colour>=memory[base+4] && colour<=memory[base+5]) {
                colour=memory[base+6];
                if(offset!=0) {
                    uint rx=uint(int(sourceX)+offset)&(mapWidth-1);
                    uint rt=tileAt(rx,sourceY),ri=tileInk(rt,rx,sourceY);
                    if(ri) colour=((rt>>10)&7u)*16u+ri;
                }
                break;
            }
        }
        if(transparentBlack && !(memory[16384u+colour]&32767u)) continue;
        bool terrain=!tunnel && sourceY>=terrainFirst && sourceY<terrainLast;
        if(ground) {lastGround=colour;lastTerrain=terrain;}put(x,y,colour,true,terrain);
    }
}
