#include "geometry_fp64.hlsli"
struct Command {
    int left,top,right,bottom;
    uint even,odd,dither,tag;
    uint texture_offset,u_mask,v_mask,colour_base;
    int u,v,du,dv;
    float4 surface;
    uint has_surface,textured,scroll_x,scroll_y;
};
RWStructuredBuffer<Command> spans:register(u0,space1);
cbuffer Settings:register(b0,space2) {
    uint4 camera_lo,camera_hi; // Exact binary64 x, y, z, focal length
    uint4 view_lo,view_hi; // Exact binary64 vanish x/y, world diameter, scale
    int4 bounds; // effect clip left/right, stored width/height
    uint4 material; // texture masks, palette base, optional override
};
Sf64 cameraAt(uint axis) {return sf_make(camera_lo[axis],camera_hi[axis]);}
Sf64 viewAt(uint axis) {return sf_make(view_lo[axis],view_hi[axis]);}
Sf64 number(int value) {return sf_from_float_bits(asuint(float(value)));}
int truncateExact(Sf64 value) {
    int exponent=int((value.hi>>20)&2047U)-1023;
    if(exponent<0) return 0;
    if(exponent>30) return (value.hi>>31)!=0?-2147483647:2147483647;
    uint magnitude=sf_right(sf_make(value.lo,(value.hi&0xfffffU)|0x100000U),uint(52-exponent)).lo;
    return (value.hi>>31)!=0?-int(magnitude):int(magnitude);
}
int roundExact(Sf64 value) {
    const bool negative=(value.hi>>31)!=0;value.hi&=0x7fffffffU;
    int whole=truncateExact(value);
    if(!sf_less(sf_sub(value,number(whole)),sf_from_float_bits(asuint(0.5)))) ++whole;
    return negative?-whole:whole;
}
groupshared int projectedDimension,projectedLeft,projectedTop,projectedScale;
groupshared uint projectedVisible;
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID,uint3 local:SV_GroupThreadID) {
    // All rows in a group share one projected rectangle. Exact software
    // binary64 division is needed only once per group, not once per row.
    if(local.x==0) {
        projectedVisible=0;
        Sf64 depth=cameraAt(2);
        if(sf_valid(depth) && (depth.hi>>31)==0 && !sf_less(depth,number(128)) && (view_hi.z>>31)==0 && !sf_zero(viewAt(2))) {
            Sf64 focal=cameraAt(3);
            int dimension=clamp(truncateExact(sf_div(sf_mul(viewAt(2),focal),depth)),0,240);
            if(dimension>0) {
                projectedDimension=dimension;
                projectedScale=int(asfloat(sf_to_float_bits(viewAt(3))));
                projectedLeft=roundExact(viewAt(0))+truncateExact(sf_div(sf_mul(cameraAt(0),focal),depth))-dimension/2;
                projectedTop=roundExact(viewAt(1))+truncateExact(sf_div(sf_mul(cameraAt(1),focal),depth))-dimension/2;
                projectedVisible=1;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    if(id.x>=uint(bounds.w)) return;
    Command c=(Command)0;
    if(projectedVisible!=0) {
        int dimension=projectedDimension,scale=projectedScale,left=projectedLeft,top=projectedTop;
        int first=left,last=left+dimension;
        if(bounds.y>bounds.x) {first=max(first,bounds.x);last=min(last,bounds.y);}
        if(first<last && int(id.x)>=top*scale && int(id.x)<(top+dimension)*scale) {
            c.left=first*scale;c.right=last*scale;c.top=int(id.x);c.bottom=c.top+1;
            c.tag=1;c.textured=2;c.u_mask=material.x;c.v_mask=material.y;c.colour_base=material.z;
            c.has_surface=2; // First temporal plane; no lighting metadata.
            c.u=left*scale;c.v=top*scale;c.du=dimension;c.dv=scale;c.scroll_x=material.w;
        }
    }
    spans[id.x]=c;
}
