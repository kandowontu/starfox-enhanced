// AMD FSR1 EASU followed by RCAS. Input/output must be different textures.
// Apply to the opaque scene before native-resolution HUD/menu composition.
#define A_GPU 1
#define A_HLSL 1
#include "../../../third_party/fsr1/ffx_a.h"
#define FSR_EASU_F 1
#define FSR_RCAS_F 1

Texture2D<float4> sourceImage : register(t0, space0);
SamplerState sourceSampler : register(s0, space0); // Clamp-to-edge.
RWTexture2D<float4> outputImage : register(u0, space1);
cbuffer Settings : register(b0, space2) {
    uint sourceWidth,sourceHeight,outputWidth,outputHeight;
    uint stage; float sharpness; uint reserved0,reserved1;
};

AF4 FsrEasuRF(AF2 p) { return sourceImage.GatherRed(sourceSampler,p); }
AF4 FsrEasuGF(AF2 p) { return sourceImage.GatherGreen(sourceSampler,p); }
AF4 FsrEasuBF(AF2 p) { return sourceImage.GatherBlue(sourceSampler,p); }
AF4 FsrRcasLoadF(ASU2 p) {
    return sourceImage.Load(int3(clamp(p,int2(0,0),int2(sourceWidth,sourceHeight)-1),0));
}
void FsrRcasInputF(inout AF1 r,inout AF1 g,inout AF1 b) {}
#include "../../../third_party/fsr1/ffx_fsr1.h"

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=outputWidth || id.y>=outputHeight) return;
    float3 colour;
    if(stage==0) {
        uint4 c0,c1,c2,c3;
        FsrEasuCon(c0,c1,c2,c3,float(sourceWidth),float(sourceHeight),
            float(sourceWidth),float(sourceHeight),float(outputWidth),float(outputHeight));
        FsrEasuF(colour,id.xy,c0,c1,c2,c3);
    } else {
        uint4 constants;
        FsrRcasCon(constants,clamp(sharpness,0.0,2.0));
        FsrRcasF(colour.r,colour.g,colour.b,id.xy,constants);
    }
    outputImage[id.xy]=float4(saturate(colour),1);
}
