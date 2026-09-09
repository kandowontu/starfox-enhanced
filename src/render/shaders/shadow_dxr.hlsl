RaytracingAccelerationStructure scene : register(t0);
RWByteAddressBuffer outputMask : register(u0);
cbuffer Settings : register(b0) {
    float4 camera; // width, height, focal length, center x
    float4 options; // center y, ground enabled
    float4 groundPoint;
    float4 groundNormal;
    float4 lights[8];
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint width = (uint)camera.x;
    if (id.x >= width || id.y >= (uint)camera.y) return;
    float3 ray = float3((id.x + .5 - camera.w) / camera.z,
        (id.y + .5 - options.x) / camera.z, 1);
    float depth = 65536;
    bool receiverFound = false;
    if (options.y != 0) {
        float denominator = dot(ray, groundNormal.xyz);
        if (abs(denominator) > 1e-10) {
            float groundDepth = dot(groundPoint.xyz, groundNormal.xyz) / denominator;
            if (groundDepth > 1 && groundDepth < depth) {
                depth = groundDepth;
                receiverFound = true;
            }
        }
    }
    RayDesc receiverRay;
    receiverRay.Origin = 0;
    receiverRay.Direction = ray;
    receiverRay.TMin = 1;
    receiverRay.TMax = depth;
    RayQuery<RAY_FLAG_FORCE_OPAQUE> receiver;
    receiver.TraceRayInline(scene, RAY_FLAG_NONE, 255, receiverRay);
    while (receiver.Proceed()) {}
    if (receiver.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        depth = receiver.CommittedRayT();
        receiverFound = true;
    }
    uint blocked = 0;
    if (receiverFound) {
        RayDesc shadowRay;
        shadowRay.Origin = ray * depth;
        shadowRay.TMin = max(.1, depth * 1e-5);
        shadowRay.TMax = 65536;
        for (uint sample = 0; sample < 8; ++sample) {
            shadowRay.Direction = lights[sample].xyz;
            RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow;
            shadow.TraceRayInline(scene, RAY_FLAG_NONE, 255, shadowRay);
            while (shadow.Proceed()) {}
            if (shadow.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ++blocked;
        }
    }
    outputMask.Store((id.y * width + id.x) * 4, 160 * blocked / 8);
}
