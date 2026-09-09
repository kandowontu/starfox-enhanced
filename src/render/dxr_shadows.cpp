#include "starfox/render/dxr_shadows.hpp"
#if defined(STARFOX_DXR)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "shadow_dxr_shader.hpp"
#include <cstring>
#include <stdexcept>
#include <sstream>
#endif

namespace starfox::render::shadows {
#if defined(STARFOX_DXR)
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " failed (0x" << std::hex << static_cast<unsigned long>(result) << ')';
        throw std::runtime_error(message.str());
    }
}
struct Buffer { ComPtr<ID3D12Resource> resource; UINT64 capacity{}; };
struct Float4 { float x{}, y{}, z{}, w{}; };
Float4 floats(Vec3 v) { return {float(v.x),float(v.y),float(v.z),0}; }
struct Constants { Float4 camera,options,point,normal; std::array<Float4,8> lights; };
static_assert(sizeof(Constants)==192);
}
struct DxrShadows::Impl {
    bool attempted{}, failed{};
    std::string status{"DXR not initialized"};
    HMODULE d3d{}, dxgi{};
    ComPtr<ID3D12Device5> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    HANDLE event{};
    UINT64 serial{};
    Buffer vertices,instances,constants,blas,tlas,scratch,output,readback;
    ~Impl() {
        // Submitted work is waited before resources are recycled or destroyed.
        if (queue && fence && event) {
            const auto value=++serial;
            if (SUCCEEDED(queue->Signal(fence.Get(),value))
                && SUCCEEDED(fence->SetEventOnCompletion(value,event)))
                WaitForSingleObject(event,5000);
        }
        readback.resource.Reset(); output.resource.Reset(); scratch.resource.Reset();
        tlas.resource.Reset(); blas.resource.Reset(); constants.resource.Reset();
        instances.resource.Reset(); vertices.resource.Reset(); pipeline.Reset(); root.Reset();
        list.Reset(); allocator.Reset(); queue.Reset(); fence.Reset(); device.Reset();
        if (event) CloseHandle(event);
        if (dxgi) FreeLibrary(dxgi);
        if (d3d) FreeLibrary(d3d);
    }
    void initialize() {
        attempted=true;
        d3d=LoadLibraryExW(L"d3d12.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        dxgi=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!d3d || !dxgi) throw std::runtime_error("Direct3D 12 unavailable");
        const auto createDevice=reinterpret_cast<decltype(&D3D12CreateDevice)>(GetProcAddress(d3d,"D3D12CreateDevice"));
        const auto createFactory=reinterpret_cast<decltype(&CreateDXGIFactory2)>(GetProcAddress(dxgi,"CreateDXGIFactory2"));
        const auto serialize=reinterpret_cast<decltype(&D3D12SerializeRootSignature)>(GetProcAddress(d3d,"D3D12SerializeRootSignature"));
        if (!createDevice || !createFactory || !serialize) throw std::runtime_error("Direct3D 12 entry points unavailable");
        ComPtr<IDXGIFactory6> factory;
        check(createFactory(0,IID_PPV_ARGS(factory.GetAddressOf())),"DXGI factory");
        for (UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.GetAddressOf()))==DXGI_ERROR_NOT_FOUND) break;
            if (!adapter) continue;
            DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            ComPtr<ID3D12Device5> candidate;
            if (FAILED(createDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,
                IID_ID3D12Device5, reinterpret_cast<void**>(candidate.GetAddressOf())))) continue;
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 caps{};
            D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_5};
            if (FAILED(candidate->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&caps,sizeof(caps)))
                || caps.RaytracingTier<D3D12_RAYTRACING_TIER_1_1
                || FAILED(candidate->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model)))
                || model.HighestShaderModel<D3D_SHADER_MODEL_6_5) continue;
            device=std::move(candidate);
            char name[256]{};
            WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,sizeof(name),nullptr,nullptr);
            status=std::string("Hardware DXR 1.1: ")+name;
            break;
        }
        if (!device) throw std::runtime_error("No hardware DXR 1.1 device");
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(device->CreateCommandQueue(&queueDesc,IID_ID3D12CommandQueue, reinterpret_cast<void**>(queue.GetAddressOf())),"DXR queue");
        check(device->CreateCommandAllocator(queueDesc.Type,IID_ID3D12CommandAllocator, reinterpret_cast<void**>(allocator.GetAddressOf())),"DXR allocator");
        check(device->CreateCommandList(0,queueDesc.Type,allocator.Get(),nullptr,
            IID_ID3D12GraphicsCommandList4, reinterpret_cast<void**>(list.GetAddressOf())),"DXR command list");
        check(list->Close(),"DXR initial close");
        check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_ID3D12Fence, reinterpret_cast<void**>(fence.GetAddressOf())),"DXR fence");
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!event) throw std::runtime_error("DXR fence event unavailable");
        D3D12_ROOT_PARAMETER parameters[3]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
        for (auto& p:parameters) p.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters=3; rootDesc.pParameters=parameters;
        ComPtr<ID3DBlob> blob,error;
        check(serialize(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,blob.GetAddressOf(),error.GetAddressOf()),"DXR root serialization");
        check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),
            IID_ID3D12RootSignature, reinterpret_cast<void**>(root.GetAddressOf())),"DXR root signature");
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature=root.Get();
        pipelineDesc.CS={shadow_dxr_shader,sizeof(shadow_dxr_shader)};
        check(device->CreateComputePipelineState(&pipelineDesc,IID_ID3D12PipelineState, reinterpret_cast<void**>(pipeline.GetAddressOf())),"DXR shader pipeline");
    }
    void ensure(Buffer& buffer, UINT64 bytes, D3D12_HEAP_TYPE heap,
        D3D12_RESOURCE_STATES state, bool uav=false) {
        if (buffer.resource && buffer.capacity>=bytes) return;
        buffer.resource.Reset(); buffer.capacity=0;
        D3D12_HEAP_PROPERTIES properties{}; properties.Type=heap;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width=(std::max<UINT64>(bytes,256)+255)&~UINT64(255);
        desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
        desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
        check(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,
            IID_ID3D12Resource, reinterpret_cast<void**>(buffer.resource.GetAddressOf())),"DXR buffer allocation");
        buffer.capacity=desc.Width;
    }
    void upload(Buffer& buffer,const void* data,std::size_t size) {
        ensure(buffer,size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* target{}; const D3D12_RANGE empty{0,0};
        check(buffer.resource->Map(0,&empty,&target),"DXR upload map");
        std::memcpy(target,data,size);
        const D3D12_RANGE written{0,size}; buffer.resource->Unmap(0,&written);
    }
    void barrier(ID3D12Resource* resource) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; barrier.UAV.pResource=resource;
        list->ResourceBarrier(1,&barrier);
    }
    void render(const Scene& scene,Camera camera,Vec3 light,std::optional<ReceiverPlane> ground,
        std::vector<std::uint8_t>& mask) {
        const auto count=scene.triangle_count();
        const auto pixels=std::size_t(camera.width)*camera.height;
        if (!pixels || !count) { mask.assign(pixels,0); return; }
        if (count>UINT_MAX/3 || camera.width>16384 || camera.height>16384)
            throw std::runtime_error("DXR scene exceeds supported dimensions");
        std::vector<float> positions;
        positions.reserve(count*9);
        for (const auto& triangle:scene.triangles()) for (auto v:{triangle.a,triangle.b,triangle.c}) {
            positions.push_back(float(v.x)); positions.push_back(float(v.y)); positions.push_back(float(v.z));
        }
        upload(vertices,positions.data(),positions.size()*sizeof(float));
        D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
        geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
        geometry.Triangles.VertexCount=UINT(count*3);
        geometry.Triangles.VertexBuffer={vertices.resource->GetGPUVirtualAddress(),12};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom{};
        bottom.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottom.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottom.NumDescs=1; bottom.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; bottom.pGeometryDescs=&geometry;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomInfo{};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom,&bottomInfo);
        ensure(blas,bottomInfo.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true);
        D3D12_RAYTRACING_INSTANCE_DESC instance{};
        instance.Transform[0][0]=instance.Transform[1][1]=instance.Transform[2][2]=1;
        instance.InstanceMask=255;
        instance.Flags=D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
        instance.AccelerationStructure=blas.resource->GetGPUVirtualAddress();
        upload(instances,&instance,sizeof(instance));
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top{};
        top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        top.NumDescs=1; top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;
        top.InstanceDescs=instances.resource->GetGPUVirtualAddress();
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topInfo{};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&top,&topInfo);
        ensure(tlas,topInfo.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,true);
        ensure(scratch,std::max(bottomInfo.ScratchDataSizeInBytes,topInfo.ScratchDataSizeInBytes),
            D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true);
        ensure(output,pixels*4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,true);
        ensure(readback,pixels*4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Constants settings{};
        settings.camera={float(camera.width),float(camera.height),float(camera.focal_length),float(camera.center_x)};
        settings.options={float(camera.center_y),ground?1.f:0.f,0,0};
        if (ground) { settings.point=floats(ground->point); settings.normal=floats(ground->normal); }
        light=light*(1.0/std::sqrt(dot(light,light)));
        const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
        auto tangent=cross(light,reference); tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
        const auto bitangent=cross(light,tangent);
        for (unsigned i=0;i<8;++i) {
            const auto radius=.015*std::sqrt((i+.5)/8);
            const auto angle=i*2.399963229728653;
            auto direction=light+tangent*(radius*std::cos(angle))+bitangent*(radius*std::sin(angle));
            settings.lights[i]=floats(direction*(1.0/std::sqrt(dot(direction,direction))));
        }
        upload(constants,&settings,sizeof(settings));
        check(allocator->Reset(),"DXR allocator reset");
        check(list->Reset(allocator.Get(),pipeline.Get()),"DXR command reset");
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs=bottom; build.DestAccelerationStructureData=blas.resource->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData=scratch.resource->GetGPUVirtualAddress();
        list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        barrier(blas.resource.Get()); barrier(scratch.resource.Get());
        build.Inputs=top; build.DestAccelerationStructureData=tlas.resource->GetGPUVirtualAddress();
        list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        barrier(tlas.resource.Get());
        list->SetComputeRootSignature(root.Get());
        list->SetComputeRootShaderResourceView(0,tlas.resource->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(1,output.resource->GetGPUVirtualAddress());
        list->SetComputeRootConstantBufferView(2,constants.resource->GetGPUVirtualAddress());
        list->Dispatch((camera.width+7)/8,(camera.height+7)/8,1);
        D3D12_RESOURCE_BARRIER transition{};
        transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition={output.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
        list->ResourceBarrier(1,&transition);
        list->CopyBufferRegion(readback.resource.Get(),0,output.resource.Get(),0,pixels*4);
        std::swap(transition.Transition.StateBefore,transition.Transition.StateAfter);
        list->ResourceBarrier(1,&transition);
        check(list->Close(),"DXR command close");
        ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1,lists);
        const auto value=++serial;
        check(queue->Signal(fence.Get(),value),"DXR signal");
        check(fence->SetEventOnCompletion(value,event),"DXR completion event");
        if (WaitForSingleObject(event,5000)!=WAIT_OBJECT_0) throw std::runtime_error("DXR GPU timeout");
        check(device->GetDeviceRemovedReason(),"DXR device");
        void* data{}; const D3D12_RANGE range{0,pixels*4};
        check(readback.resource->Map(0,&range,&data),"DXR readback map");
        mask.resize(pixels);
        const auto* values=static_cast<const std::uint32_t*>(data);
        for (std::size_t i=0;i<pixels;++i) mask[i]=static_cast<std::uint8_t>(values[i]);
        const D3D12_RANGE empty{0,0}; readback.resource->Unmap(0,&empty);
    }
};
#else
struct DxrShadows::Impl { std::string status{"Hardware DXR unavailable on this build"}; };
#endif
DxrShadows::DxrShadows():impl_(std::make_unique<Impl>()) {}
DxrShadows::~DxrShadows()=default;
const std::string& DxrShadows::status() const { return impl_->status; }
bool DxrShadows::render(const Scene& scene,Camera camera,Vec3 light,
    std::optional<ReceiverPlane> ground,std::vector<std::uint8_t>& mask) {
#if defined(STARFOX_DXR)
    if (impl_->failed) return false;
    if (!std::isfinite(dot(light,light)) || dot(light,light)<1e-20 || camera.focal_length<=0) return false;
    try {
        if (!impl_->attempted) impl_->initialize();
        impl_->render(scene,camera,light,ground,mask);
        return true;
    } catch (const std::exception& error) {
        impl_->status=std::string("CPU shadow fallback: ")+error.what();
        impl_->failed=true;
        return false;
    }
#else
    (void)scene; (void)camera; (void)light; (void)ground; (void)mask;
    return false;
#endif
}
}
