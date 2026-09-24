// One ordered pass builds the inverse occurrence map. Zero means no material;
// slot+1 permits later occurrences to replace canonical/earlier face materials.
StructuredBuffer<uint4> polygons : register(t0,space0);
StructuredBuffer<uint4> corners : register(t1,space0);
RWStructuredBuffer<uint> faceLookup : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint slotCount,faceCount,cornerCount,padding;};
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id!=0)) return;
    for(uint face=0;face<faceCount;++face) faceLookup[face]=0;
    for(uint slot=0;slot<slotCount;++slot) {
        uint4 p=polygons[slot];
        if(p.y<3 || p.x>=cornerCount || p.y>cornerCount-p.x) continue;
        uint face=corners[p.x].w;
        if(face>0 && face<=faceCount) faceLookup[face-1]=slot+1;
    }
}
