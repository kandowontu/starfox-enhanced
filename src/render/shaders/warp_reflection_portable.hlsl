// Reflection-only extension. Canonical hidden-face materials precede actual
// draw occurrences, so source-face lookup always prefers the visible draw.
StructuredBuffer<uint> descriptors : register(t0,space0);
StructuredBuffer<uint2> traversal : register(t1,space0);
StructuredBuffer<uint> ordered : register(t2,space0);
RWStructuredBuffer<uint> combinedDescriptors : register(u0,space1);
RWStructuredBuffer<uint2> result : register(u1,space1);
RWStructuredBuffer<uint> combinedOrder : register(u2,space1);
cbuffer Settings : register(b0,space2) {uint capacity,faceCount,seed,padding;};
uint nextWord(inout uint state,inout uint carry) {
    uint word=0;
    for(uint hop=0;hop<32;++hop) {
        uint swapped=((state<<8)|(state>>8))&65535U;
        uint rotated=(carry<<15)|(swapped>>1);
        uint first=rotated+state;
        uint second=(first&65535U)+state+uint(first>65535U);
        carry=uint(second>65535U);state=(second+1U)&65535U;word=state;
        if((word&49152U)!=32768U) return word;
    }
    return word&~32768U;
}
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id!=0)) return;
    result[0]=uint2(0,1);
    for(uint i=0;i<capacity+faceCount;++i) {combinedDescriptors[i]=0xffffffffU;combinedOrder[i]=0xffffffffU;}
    uint2 list=traversal[0];
    if(list.y!=0 || list.x>capacity) return;
    for(uint i=0;i<list.x;++i) if(ordered[i]>=faceCount) return;
    uint state=seed&65535U,carry=0;
    for(uint face=0;face<faceCount;++face) {
        combinedOrder[face]=face;combinedDescriptors[face]=nextWord(state,carry);
    }
    // Never advance or replace the main-view random stream.
    for(uint i=0;i<list.x;++i) {
        combinedOrder[faceCount+i]=ordered[i];combinedDescriptors[faceCount+i]=descriptors[i];
    }
    result[0]=uint2(faceCount+list.x,0);
}
