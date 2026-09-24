#include "raster_jitter.hlsli"
// Source grid dots, emitted directly into the resident row-span raster format.
struct Command {
    int left,top,right,bottom;
    uint even,odd,dither,tag;
    uint texture_offset,u_mask,v_mask,colour_base;
    int u,v,du,dv;
    float4 surface;
    uint has_surface,textured,scroll_x,scroll_y;
};
uint sourceYOffset(uint n,uint dx,uint dy) {
    if(n==0 || dy==0) return 0;
    if(dy>dx) return n;
    // Strictly-negative source error: equality delays the first Y step.
    return (n*dy-1)/dx;
}
uint firstStep(uint count,uint dx,uint dy,int offset) {
    if(offset<=0) return 0;
    uint lo=0,hi=count;
    while(lo<hi) {
        uint mid=lo+(hi-lo)/2;
        if(sourceYOffset(mid,dx,dy)<uint(offset)) lo=mid+1;else hi=mid;
    }
    return lo;
}
// Symmetric Bresenham rounds half ties toward the destination. Projected
// endpoints are viewport-clipped (<32767), so these unsigned products fit.
uint trailMinor(uint n,uint major,uint minor) {
    return major==0?0:(2*n*minor+major)/(2*major);
}
uint trailFirst(uint major,uint minor,int offset) {
    if(offset<=0) return 0;
    uint lo=0,hi=major+1;
    while(lo<hi) {
        uint mid=lo+(hi-lo)/2;
        if(trailMinor(mid,major,minor)<uint(offset)) lo=mid+1;else hi=mid;
    }
    return lo;
}
[[vk::binding(0,0)]] StructuredBuffer<int4> points : register(t0,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Command> spans : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint height,scale,colour,tag;
    uint lines; int startX,startY; uint padding;
    uint logicalWidth,logicalHeight,rasterWidth,reserved;
    float2 rasterJitter;uint2 jitterPadding;
};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint components=lines==1?3:1;
    uint pointCount=lines>=2?padding:225;
    if(id.x>=pointCount*components*height) return;
    uint row=id.x%height;
    uint slot=id.x/height,index=slot/components,component=slot%components;
    int4 p=points[lines==3?index*2:index];
    Command c=(Command)0;
    if(p.w!=0) {
        int logicalRow=int(logicalHeight!=0?row*logicalHeight/height:row/scale);
        if(any(rasterJitter!=0)) logicalRow=jitterFloor(row,logicalHeight!=0?logicalHeight:height/scale,height,rasterJitter.y,true);
        int x=p.x;
        int right=x+1;
        bool visible=logicalRow==p.y;
        bool companion=lines==2?(p.w&2)!=0:p.z<512;
        if(companion && logicalRow==p.y+1) {x-=1;visible=true;}
        right=x+1;
        if(lines==1) {
            x=p.x-1;right=x+1;visible=false;
            if(component==0) visible=logicalRow==p.y+2;
            else if(component==2) {--x;--right;visible=p.z<512 && logicalRow==p.y+1;}
            else {
                int2 previous=int2(startX,startY);
                for(int i=int(index)-1;i>=0;--i) if(points[i].w!=0) {
                    previous=points[i].xy-int2(1,0);break;
                }
                int dx=x-previous.x,adx=abs(dx),ady=abs(p.y-previous.y);
                int offset=(logicalRow-p.y)*(p.y<previous.y?1:-1);
                uint count=uint(max(dx,0))+1;
                uint first=firstStep(count,uint(adx),uint(ady),offset);
                uint end=firstStep(count,uint(adx),uint(ady),offset+1);
                visible=offset>=0 && first<end;
                right=x-1-int(first);x=x-1-int(end);
            }
        }
        if(lines==2 && x>=startX && x<startY) visible=false;
        if(lines==3) {
            x=p.x;right=x+2;visible=logicalRow==p.y || logicalRow==p.y+1;
            if((p.w&2)!=0) {
                int2 a=points[index*2+1].xy,b=p.xy;
                uint dx=uint(abs(b.x-a.x)),dy=uint(abs(b.y-a.y));
                int sx=a.x<b.x?1:-1,sy=a.y<b.y?1:-1;
                int offset=(logicalRow-a.y)*sy;
                visible=offset>=0 && uint(offset)<=dy;
                if(dx>=dy) {
                    uint first=trailFirst(dx,dy,offset),end=trailFirst(dx,dy,offset+1);
                    int ax=a.x+sx*int(first),bx=a.x+sx*(int(end)-1);
                    x=min(ax,bx);right=max(ax,bx)+1;visible=visible && first<end;
                } else {
                    x=a.x+sx*int(trailMinor(uint(max(offset,0)),dy,dx));right=x+1;
                }
            }
            if(startY>startX) {x=max(x,startX);right=min(right,startY);}
            visible=visible && x<right;
        }
        if(visible) {
            c.left=x*int(scale);c.right=right*int(scale);
            if(logicalWidth!=0) {
                // ceil signed division: a logical cell owns exactly the output
                // samples whose floor-mapped coordinate falls inside it.
                int a=x*int(rasterWidth),b=right*int(rasterWidth),d=int(logicalWidth);
                c.left=a/d+((a%d)>0?1:0);c.right=b/d+((b%d)>0?1:0);
            }
            if(rasterJitter.x!=0) {
                c.left=jitterCeil(x,logicalWidth!=0?rasterWidth:scale,logicalWidth!=0?logicalWidth:1,rasterJitter.x,true);
                c.right=jitterCeil(right,logicalWidth!=0?rasterWidth:scale,logicalWidth!=0?logicalWidth:1,rasterJitter.x,true);
            }
            c.top=int(row);c.bottom=c.top+1;
            c.even=lines>=2?uint(p.z):colour;c.odd=c.even;c.tag=tag;
        }
    }
    spans[id.x]=c;
}
