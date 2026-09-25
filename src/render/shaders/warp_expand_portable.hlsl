// Expand ordered draws into independent clipping/material slots. Source face
// IDs may repeat; each occurrence retains its own random texture and UVs.
struct Command {
    int4 bounds;uint4 colour;uint4 texture;int4 uv;
    float4 surface;uint4 flags;
};
StructuredBuffer<uint> ordered : register(t0,space0);
StructuredBuffer<uint2> traversal : register(t1,space0);
StructuredBuffer<uint4> sourcePolygons : register(t2,space0);
StructuredBuffer<uint4> sourceCorners : register(t3,space0);
StructuredBuffer<Command> sourceMaterials : register(t4,space0);
StructuredBuffer<uint4> decoded : register(t5,space0);
StructuredBuffer<uint4> textures : register(t6,space0);
StructuredBuffer<uint2> coordinates : register(t7,space0);
RWStructuredBuffer<uint4> polygons : register(u0,space1);
RWStructuredBuffer<uint4> corners : register(u1,space1);
RWStructuredBuffer<Command> materials : register(u2,space1);
cbuffer Settings : register(b0,space2) {
    uint capacity,faceCount,cornerCount,textureCount;
    uint coordinateCount,colourBase,scrollX,scrollY;
};
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint slot=id.x;
    if(slot>=capacity) return;
    polygons[slot]=0;materials[slot]=(Command)0;
    uint2 list=traversal[0];
    if(list.y!=0 || list.x>capacity || slot>=list.x) return;
    uint face=ordered[slot];
    if(face>=faceCount) return;
    uint4 polygon=sourcePolygons[face],material=decoded[slot];
    // Distinct from an untextured material, including solid black.
    if(material.w==0xfffffffeU) return;
    if(polygon.y>32 || polygon.x>cornerCount || polygon.y>cornerCount-polygon.x) return;
    bool textured=material.w!=0xffffffffU;
    uint4 texture=0;
    if(textured) {
        if(material.w>=textureCount) return;
        texture=textures[material.w];
        if(texture.w>coordinateCount || 4U>coordinateCount-texture.w) return;
    }
    Command command=sourceMaterials[face];
    command.colour.xyz=material.xyz;
    command.flags.y=uint(textured);
    // Source material templates must be untextured (preserving cel/wave
    // flags); selected textures replace scroll, tag and descriptor fields.
    command.colour.w=0; // PixelLayer::three_d
    if(textured) {
        command.texture=uint4(texture.xyz,colourBase);
        command.flags.zw=uint2(scrollX,scrollY);
        command.colour.w=(polygon.w&4U)!=0?1U:((polygon.w&2U)!=0?0U:4U);
    }
    uint first=slot*32U;
    for(uint c=0;c<polygon.y;++c) {
        uint vertex=sourceCorners[polygon.x+c].x;
        uint2 uv=textured?coordinates[texture.w+c%4U]:uint2(0,0);
        // Retain source-face identity independently of the occurrence slot.
        // Zero remains the ordinary (non-warp) corner sentinel.
        corners[first+c]=uint4(vertex,uv,face+1U);
    }
    polygon.x=first;polygon.w=(polygon.w&~1U)|uint(textured);
    polygons[slot]=polygon;materials[slot]=command;
}
