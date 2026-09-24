Texture2D<float4> original : register(t0,space0);
Texture2D<float4> reconstructed : register(t1,space0);
StructuredBuffer<uint> packedPixels : register(t2,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> result : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint width,height,preserveArtwork,pad1;};
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint packed=packedPixels[id.y*width+id.x];
    bool hud=((packed>>8)&255u)==1u && (packed&0x10000000u)==0;
    // Tilemap artwork has no pinhole-camera correspondence. Keep its native
    // samples instead of asking temporal reconstruction to invent movement.
    // Explicit terrain ownership stays in the reconstructed world layer.
    bool artwork=preserveArtwork!=0 && ((packed>>8)&255u)==2u && (packed&0x08000000u)==0;
    result[id.xy]=(hud || artwork)?original.Load(int3(id.xy,0)):reconstructed.Load(int3(id.xy,0));
}
