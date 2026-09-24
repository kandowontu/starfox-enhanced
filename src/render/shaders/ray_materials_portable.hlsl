// Convert resolved raster materials, including occurrence-based EX draws, into
// the 64-byte RayMaterial ABI. No colour/UV resolution or readback on the CPU.
struct Command {int4 bounds;uint4 colour;uint4 texture;int4 uv;float4 surface;uint4 flags;};
StructuredBuffer<uint4> topology : register(t0,space0);
StructuredBuffer<uint4> corners : register(t1,space0);
StructuredBuffer<uint4> polygons : register(t2,space0);
StructuredBuffer<Command> materials : register(t3,space0);
StructuredBuffer<uint> faceLookup : register(t4,space0);
RWStructuredBuffer<uint4> outputMaterials : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint triangleCount,cornerCount,materialCount,texelCount;uint texelBase,outputBase,rejectAll,lookupCount;};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint tri=id.x;if(tri>=triangleCount) return;
    uint destination=outputBase+tri*4;
    for(uint i=0;i<4;++i) outputMaterials[destination+i]=0;
    // reserved=1 explicitly marks rejection; zero colours are legitimate.
    outputMaterials[destination+3].w=1;
    if(rejectAll!=0) return;
    uint4 ids=topology[tri];
    if((ids.w&0x80000000U)!=0) {
        // Source-face topology uses local corner ordinals. Repeated BSP draws
        // overlap exactly; the last emitted occurrence supplies visible colour.
        uint sourceFace=(ids.w&0x7fffffffU)+1U,selected=materialCount;
        if(lookupCount!=0) {
            if(sourceFace>lookupCount) return;
            uint encoded=faceLookup[sourceFace-1];
            if(encoded==0 || encoded>materialCount) return;
            selected=encoded-1;
            uint4 candidate=polygons[selected];
            if(candidate.y<3 || candidate.x>=cornerCount || candidate.y>cornerCount-candidate.x
                || corners[candidate.x].w!=sourceFace) return;
        } else for(uint slot=0;slot<materialCount;++slot) {
            uint4 candidate=polygons[slot];
            if(candidate.y>=3 && candidate.x<cornerCount && candidate.y<=cornerCount-candidate.x
                && corners[candidate.x].w==sourceFace) selected=slot;
        }
        if(selected==materialCount) return;
        uint4 selectedPolygon=polygons[selected];
        if(any(ids.xyz>=selectedPolygon.y)) return;
        ids.xyz+=selectedPolygon.x;ids.w=selected;
    }
    if(ids.w>=materialCount) return;
    uint4 polygon=polygons[ids.w];
    if(polygon.x>cornerCount || polygon.y>cornerCount-polygon.x
        || any(ids.xyz<polygon.x) || any(ids.xyz-polygon.x>=polygon.y)) return;
    Command m=materials[ids.w];uint textured=m.flags.y;
    if(textured>1 || any(m.colour.xy>255) || m.texture.w>255) return;
    if(textured && (m.texture.y>4095 || m.texture.z>4095
        || (m.texture.y&(m.texture.y+1)) || (m.texture.z&(m.texture.z+1))
        || m.texture.x>texelCount || (m.texture.y+1)*(m.texture.z+1)>texelCount-m.texture.x)) return;
    float2 uv[3];
    for(uint c=0;c<3;++c) {
        // Convert before adding signed scroll, avoiding uint wrap in UVs.
        uv[c]=textured?float2(corners[ids[c]].yz)+float2(asint(m.flags.zw)):float2(0,0);
        if(any(abs(uv[c])>1e8)) return;
    }
    outputMaterials[destination]=asuint(float4(uv[0],uv[1]));
    outputMaterials[destination+1]=uint4(asuint(uv[2]),textured,m.colour.z);
    outputMaterials[destination+2]=uint4(m.colour.xy,m.texture.w,ids.w);
    outputMaterials[destination+3]=uint4(textured?uint3(m.texture.x+texelBase,m.texture.yz):uint3(0,0,0),0);
}
