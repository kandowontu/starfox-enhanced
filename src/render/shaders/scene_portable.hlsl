[[vk::binding(0,0)]] StructuredBuffer<uint> frontPixels : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<float4> frontSurfaces : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint> backPixels : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<float4> backSurfaces : register(t3,space0);
[[vk::binding(4,0)]] StructuredBuffer<float> frontDepth : register(t4,space0);
[[vk::binding(5,0)]] StructuredBuffer<float> backDepth : register(t5,space0);
[[vk::binding(6,0)]] StructuredBuffer<float4> frontMotion : register(t6,space0);
[[vk::binding(7,0)]] StructuredBuffer<float4> backMotion : register(t7,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint> pixels : register(u0,space1);
[[vk::binding(1,1)]] RWStructuredBuffer<float4> surfaces : register(u1,space1);
[[vk::binding(2,1)]] RWStructuredBuffer<float> geometryDepth : register(u2,space1);
[[vk::binding(3,1)]] RWStructuredBuffer<float4> motion : register(u3,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count,hasFrontSurfaces,hasBack,hasBackSurfaces;
    uint wantDepth,hasFrontDepth,hasBackDepth,padding;
    uint wantMotion,hasFrontMotion,hasBackMotion,motionPadding;
    uint outputWidth,outputHeight,sourceWidth,sourceHeight;
    uint sourceScale,destinationScale,referenceWidth,referenceHeight;
    int offsetX,offsetY,clipLeft,clipTop;
    int clipRight,clipBottom,mosaicX,mosaicY;
    uint mosaicStep,wantSurfaces,unused1,unused2;
};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    uint front=0,back=0;
    if(motionPadding!=0) {
        uint2 at=uint2(id.x%outputWidth,id.x/outputWidth)*uint2(referenceWidth,referenceHeight)/uint2(outputWidth,outputHeight);
        int2 logical=int2(at/destinationScale);
        bool inside=logical.x>=max(offsetX,clipLeft) && logical.y>=max(offsetY,clipTop)
            && logical.x<min(offsetX+int(sourceWidth/sourceScale),clipRight)
            && logical.y<min(offsetY+int(sourceHeight/sourceScale),clipBottom);
        int2 delta=logical-int2(mosaicX,mosaicY);int step=int(mosaicStep);
        int2 snapped=int2(delta.x<0?-((-delta.x+step-1)/step):delta.x/step,
            delta.y<0?-((-delta.y+step-1)/step):delta.y/step)*step+int2(mosaicX,mosaicY)-int2(offsetX,offsetY);
        if(inside && all(snapped>=0) && all(snapped<int2(sourceWidth,sourceHeight)/int(sourceScale))) {
            uint2 sub=((at%destinationScale*2+1)*sourceScale)/(destinationScale*2);
            uint2 sampleAt=uint2(snapped)*sourceScale+sub;
            front=frontPixels[sampleAt.y*sourceWidth+sampleAt.x];
            // Layer-composite transparency is palette zero, not write coverage.
            front=(front&255u)!=0?(front&0xffffu)|0x04000000u:0;
        }
    } else front=frontPixels[id.x];
    // World billboards deliberately keep their 2D effects/filter tag. That
    // styling classification must not turn explosions into protected HUD.
    if((padding&1u)!=0 && ((front>>8)&255u)==1u) front|=0x10000000u;
    if(hasBack!=0) back=backPixels[id.x];
    // Colour and metadata have independent painter ownership. A later line
    // can paint black without erasing an earlier model's surface sample.
    uint colour=(front&0x04000000U)!=0?front:back;
    uint metadata=0;float4 normal=float4(0,0,1,0);
    if(hasFrontSurfaces!=0 && (front&0x01000000U)!=0) {
        metadata=front&0x01ff0000U;normal=frontSurfaces[id.x];
    } else if(hasBackSurfaces!=0 && (back&0x01000000U)!=0) {
        metadata=back&0x01ff0000U;normal=backSurfaces[id.x];
    }
    if((padding&2u)!=0 && (front&0x04000000u)!=0) {
        metadata=0;normal=float4(0,0,1,0);
    }
    pixels[id.x]=(colour&0x1c00ffffU)|metadata;
    if(wantSurfaces!=0) surfaces[id.x]=normal;
    if(wantDepth!=0) {
        // Unlike effects metadata, geometry belongs to the visible colour.
        // A covering sprite/HUD pixel with unknown depth must not inherit a
        // hidden model's depth. Transparent pixels retain the layer behind.
        float depth=0;
        if((front&0x04000000U)!=0) {
            if(hasFrontDepth!=0) depth=frontDepth[id.x];
        } else if(hasBackDepth!=0) depth=backDepth[id.x];
        geometryDepth[id.x]=depth;
    }
    if(wantMotion!=0) {
        float4 value=0;
        if((front&0x04000000U)!=0) {
            if(hasFrontMotion!=0) value=frontMotion[id.x];
        } else if(hasBackMotion!=0) value=backMotion[id.x];
        motion[id.x]=value;
    }
}
