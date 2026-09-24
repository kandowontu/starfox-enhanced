#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/render/gpu_scene.hpp"
#if defined(STARFOX_TEST_PORTABLE_SHADOWS)
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include <SDL3/SDL.h>
#endif
#include <chrono>
#include <iostream>
#include <algorithm>
#include <array>
#include <iomanip>
#if defined(_WIN32) && !defined(STARFOX_TEST_PORTABLE_SHADOWS)
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstring>
#include <stdexcept>
namespace {
Microsoft::WRL::ComPtr<ID3D12Resource> resident_material_fixture(ID3D12Device* device,
    std::span<const starfox::render::RayMaterial> records) {
    using Microsoft::WRL::ComPtr;
    const auto check=[](HRESULT result){if(FAILED(result)) throw std::runtime_error("Resident material fixture failed");};
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=records.size_bytes();
    desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    ComPtr<ID3D12Resource> result,upload;
    check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,
        IID_ID3D12Resource,reinterpret_cast<void**>(result.GetAddressOf())));
    heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
        IID_ID3D12Resource,reinterpret_cast<void**>(upload.GetAddressOf())));
    void* mapped{};check(upload->Map(0,nullptr,&mapped));std::memcpy(mapped,records.data(),records.size_bytes());upload->Unmap(0,nullptr);
    ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(device->CreateCommandQueue(&q,IID_ID3D12CommandQueue,reinterpret_cast<void**>(queue.GetAddressOf())));
    check(device->CreateCommandAllocator(q.Type,IID_ID3D12CommandAllocator,reinterpret_cast<void**>(allocator.GetAddressOf())));
    check(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_ID3D12GraphicsCommandList,reinterpret_cast<void**>(list.GetAddressOf())));
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={result.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};
    list->ResourceBarrier(1,&barrier);list->CopyBufferRegion(result.Get(),0,upload.Get(),0,desc.Width);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
    check(list->Close());ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);
    check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_ID3D12Fence,reinterpret_cast<void**>(fence.GetAddressOf())));
    check(queue->Signal(fence.Get(),1));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event) throw std::runtime_error("Material fixture event unavailable");
    const auto signal=fence->SetEventOnCompletion(1,event);
    const auto wait=SUCCEEDED(signal)?WaitForSingleObject(event,5000):WAIT_FAILED;CloseHandle(event);
    if(wait!=WAIT_OBJECT_0) throw std::runtime_error("Material fixture timed out");
    return result;
}
}
#endif
int main() {
    using namespace starfox::render::shadows;
#if defined(STARFOX_TEST_PORTABLE_SHADOWS)
    if(!SDL_Init(SDL_INIT_VIDEO)) return 3;
    struct SdlLifetime { ~SdlLifetime(){SDL_Quit();} } sdl;
#endif
    Scene scene;
    for (int z=0;z<8;++z) for (int x=-4;x<4;++x) {
        const Vec3 a{x*18.0,-12,z*25.0+60};
        scene.add({a,a+Vec3{14,0,0},a+Vec3{7,20,4}});
        scene.add({a,a+Vec3{7,20,4},a+Vec3{0,0,12}});
    }
    scene.build();
#if defined(STARFOX_TEST_PORTABLE_SHADOWS)
    PortableShadows gpu;
#else
    DxrShadows gpu;
    {
        Scene reflected;
        reflected.add({{-2,-2,3},{2,-2,7},{0,2,5}});
        reflected.add({{5,-3,2},{5,3,2},{5,0,8}});
        starfox::render::RayMaterials materials;
        materials.triangles.resize(2);materials.triangles[0].even=1;materials.triangles[1].even=2;
        std::array<std::uint32_t,256> palette{};
        palette[1]=0xffffffff;palette[2]=0xff332211;
        std::vector<std::uint8_t> result;
        const Camera camera{1,1,1,.5,.5};
        if(!gpu.render_reflections(reflected,camera,materials,palette,0xff998877,result)) {
            std::cerr<<"Reflection dispatch failed: "<<gpu.status()<<'\n';return 40;
        }
        if(result!=std::vector<std::uint8_t>{0x11,0x22,0x33,0xff}) {
            std::cerr<<"Offscreen reflected triangle colour mismatch\n";return 41;
        }
        // Transparent texel zero must reveal the environment, not opaque black.
        materials.triangles[1].textured=1;materials.texels={0};
        if(!gpu.render_reflections(reflected,camera,materials,palette,0xff998877,result)
            || result!=std::vector<std::uint8_t>{0x77,0x88,0x99,0xff}) return 42;
        materials.texels[0]=2;
        if(!gpu.render_reflections(reflected,camera,materials,palette,0xff998877,result)
            || result!=std::vector<std::uint8_t>{0x11,0x22,0x33,0xff}) return 43;
        palette[2]=0xff665544;
        if(!gpu.render_reflections(reflected,camera,materials,palette,0xff998877,result)
            || result!=std::vector<std::uint8_t>{0x44,0x55,0x66,0xff}) return 44;
        Scene moved;
        moved.add({{-2,-2,3},{2,-2,7},{0,2,5}});
        moved.add({{5,17,2},{5,23,2},{5,20,8}});
        if(!gpu.render_reflections(moved,camera,materials,palette,0xff998877,result)
            || result!=std::vector<std::uint8_t>{0x77,0x88,0x99,0xff}) return 45;
        if(!gpu.render_reflections(reflected,Camera{1,1,1,100,.5},materials,palette,0xff998877,result)
            || result!=std::vector<std::uint8_t>{0,0,0,0}) return 46;
        if(!gpu.render_reflections(reflected,Camera{13,9,5,6.5,4.5},materials,palette,0xff998877,result)
            || result.size()!=13*9*4 || result[(4*13+6)*4]!=0x44) return 47;
        const DxrShadows::ReflectionInput input{&materials,palette,0xff998877};
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&input)) return 80;
        auto gpu_records=resident_material_fixture(static_cast<ID3D12Device*>(gpu.resident_output().device),materials.triangles);
        auto resident_input=input;resident_input.resident_materials=gpu_records.Get();
        // Texels stay in the metadata source; CPU triangle records are absent.
        starfox::render::RayMaterials texels_only;texels_only.texels=materials.texels;
        resident_input.materials=&texels_only;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&resident_input)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x44,0x55,0x66,255}) return 77;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&resident_input)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x44,0x55,0x66,255}) return 78;
        auto* material_device=static_cast<ID3D12Device*>(gpu.resident_output().device);
        auto invalid_records=materials.triangles;invalid_records[1].offset=0xffffffffU;
        auto invalid_gpu=resident_material_fixture(material_device,invalid_records);
        resident_input.resident_materials=invalid_gpu.Get();
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&resident_input)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x77,0x88,0x99,255}) return 81;
        invalid_records[0].reserved=1;
        auto rejected_gpu=resident_material_fixture(material_device,invalid_records);
        resident_input.resident_materials=rejected_gpu.Get();
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&resident_input)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,0,0,0}) return 82;
        auto small_gpu=resident_material_fixture(material_device,std::span(materials.triangles).first(1));
        resident_input.resident_materials=small_gpu.Get();
        if(gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&resident_input)
            || gpu.resident_output().resource) return 83;
        // Restore the previous 13x9 fixture before comparing resident output.
        if(!gpu.render_reflections(reflected,Camera{13,9,5,6.5,4.5},materials,palette,0xff998877,result)) return 79;
        const auto expected=result;
        if(!gpu.render_resident(reflected,Camera{13,9,5,6.5,4.5},{0,1,0},{},nullptr,nullptr,true,true,&input)
            || gpu.resident_output().bytes_per_pixel!=4 || gpu.resident_output().row_bytes!=52
            || !gpu.readback_resident(result) || result!=expected) return 48;
        auto metal=input;metal.metallic=true;
        palette[1]=0xff0000ff; // Red conductor preserves red, suppresses green/blue.
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&metal)
            || !gpu.readback_resident(result) || result.size()!=4 || result[0]!=0x44
            || result[1]>=0x55 || result[2]>=0x66) return 49;
        metal.roughness=.65f;
        if(!gpu.render_resident(reflected,Camera{13,9,5,6.5,4.5},{0,1,0},{},nullptr,nullptr,false,false,&metal)
            || !gpu.readback_resident(result)) return 50;
        const auto rough=result;
        if(!gpu.render_resident(reflected,Camera{13,9,5,6.5,4.5},{0,1,0},{},nullptr,nullptr,false,false,&metal)
            || !gpu.readback_resident(result) || result!=rough) return 51;
        metal.roughness=0;
        if(!gpu.render_resident(reflected,Camera{13,9,5,6.5,4.5},{0,1,0},{},nullptr,nullptr,false,false,&metal)
            || !gpu.readback_resident(result) || result==rough) return 52;
        for(unsigned conductor:{2U,3U}) {
            metal.metallic=conductor;
            if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&metal)
                || !gpu.readback_resident(result) || result.size()!=4) return 104;
            const std::array<float,3> f0=conductor==2?std::array<float,3>{1,.766f,.336f}
                :std::array<float,3>{.955f,.638f,.538f};
            for(unsigned c=0;c<3;++c) if(std::abs(int(result[c])-int(std::array<unsigned,3>{0x44,0x55,0x66}[c]*std::sqrt(f0[c])+.5f))>1) return 105;
        }
        metal.metallic=1;metal.roughness=1.01f;
        if(gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&metal)
            || gpu.resident_output().resource) return 53;
        std::array<std::uint32_t,6> cube{0xff102030,0xff405060,0xff708090,0xffa0b0c0,0xffd0e0f0,0xff123456};
        auto environment=input;environment.environment_cube=cube;environment.face_size=1;
        // Target moved out of the reflected ray: the +X surround is visible.
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x30,0x20,0x10,0xff}) return 54;
        cube[0]=0xffabcdef;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0xef,0xcd,0xab,0xff}) return 55;
        // A real object must occlude the environment, including after updates.
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x44,0x55,0x66,0xff}) return 56;
        environment.face_size=2;
        if(gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || gpu.resident_output().resource) return 57;
        environment.face_size=1;environment.environment_rotation={0,0,-1,0,1,0,1,0,0};
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0xf0,0xe0,0xd0,0xff}) return 58;
        // A ray on the +X/+Z boundary must blend both faces in linear light,
        // not clamp to whichever face won the major-axis comparison.
        cube.fill(0xff000000);cube[4]=0xffffffff;
        constexpr float diagonal=.70710678f;
        environment.environment_rotation={diagonal,0,-diagonal,0,1,0,diagonal,0,diagonal};
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || !gpu.readback_resident(result) || result.size()!=4
            || result[0]<179 || result[0]>182 || result[1]!=result[0] || result[2]!=result[0]) return 60;
        environment.environment_rotation={};
        if(gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&environment)
            || gpu.resident_output().resource) return 59;
        // The physical plane must not disappear when no BG2 material exists.
        auto flat_ground=input;
        flat_ground.ground=ReceiverPlane{{2,0,0},{1,0,0}};
        const std::vector<std::uint8_t> fallback_ground{0x77,0x88,0x99,255};
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&flat_ground)
            || !gpu.readback_resident(result) || result!=fallback_ground) return 100;
        // Cube radiance is distant sky, not a transparent finite floor.
        flat_ground.environment_cube=cube;flat_ground.face_size=1;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&flat_ground)
            || !gpu.readback_resident(result) || result!=fallback_ground) return 101;
        flat_ground.ground->point.x=10;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&flat_ground)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x44,0x55,0x66,255}) return 102;
        flat_ground.ground->normal={};
        if(gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&flat_ground)
            || gpu.resident_output().resource) return 103;
        auto ppu=std::make_shared<starfox::simulation::SnesPpuState>();
        starfox::render::GpuBackgroundDraw authored;authored.ppu=ppu;authored.settings.layer=2;
        ppu->cgram.fill(0x7fff);ppu->cgram[0]=0;
        for(unsigned row=0;row<8;++row) ppu->vram[ppu->bg2_character_base*2+row*2]=255;
        palette[0]=0xff000000;palette[1]=0xff00ff00;palette[2]=0xff0000ff;
        auto tiles=input;tiles.background=&authored;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 61;
        authored.settings.transparent_cgram_black=true;ppu->cgram[1]=0;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x77,0x88,0x99,255}) return 74;
        // Changing source CGRAM at the same address must invalidate the upload.
        ppu->cgram[1]=0x7fff;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 75;
        authored.settings.transparent_cgram_black=false;
        for(unsigned row=0;row<8;++row) ppu->vram[ppu->bg2_character_base*2+row*2]=0;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0x77,0x88,0x99,255}) return 76;
        for(unsigned row=0;row<8;++row) ppu->vram[ppu->bg2_character_base*2+row*2]=255;
        // A physical receiver in front of the reflected object wins; moving it
        // behind the object restores the object. Cube/fallback alone cannot do this.
        tiles.ground=ReceiverPlane{{2,0,0},{1,0,0}};
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 69;
        // Central-camera material lookup must undo the eye-space translation.
        for(unsigned row=0;row<8;++row) ppu->vram[ppu->bg2_character_base*2+32+row*2+1]=255;
        ppu->vram[(ppu->bg2_screen_base+1024+14*32+3)*2]=1;
        tiles.background_eye_x=1;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{255,0,0,255}) return 72;
        tiles.background_eye_x=0;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 73;
        tiles.ground->point.x=10;
        if(!gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{255,0,0,255}) return 70;
        tiles.ground->normal={};
        if(gpu.render_resident(reflected,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || gpu.resident_output().resource) return 71;
        tiles.ground.reset();
        // Same PPU object, changed tile bytes: uploads cannot cache by identity.
        for(unsigned row=0;row<8;++row) {
            ppu->vram[ppu->bg2_character_base*2+row*2]=0;
            ppu->vram[ppu->bg2_character_base*2+row*2+1]=255;
        }
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{255,0,0,255}) return 62;
        // +X is beyond the original view: unique background art must not repeat.
        authored.settings.unique_regions.push_back({0,0,512,512,2,2,0});
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,0,0,255}) return 63;
        authored.settings.unique_regions.back().replacement_x_offset=512;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{255,0,0,255}) return 111;
        authored.settings.unique_regions.clear();
        // BG row scroll must address new tiles, not move a cached screenshot.
        for(unsigned row=0;row<8;++row) {
            ppu->vram[ppu->bg2_character_base*2+32+row*2]=255;
            ppu->vram[ppu->bg2_character_base*2+32+row*2+1]=0;
        }
        for(unsigned col=0;col<64;++col) {
            const unsigned word=ppu->bg2_screen_base+(col>>5)*1024+15*32+(col&31);
            ppu->vram[(word*2)&65535]=1;
        }
        ppu->bg2_scanline_scroll_enabled=true;ppu->bg2_scanline_scroll_y.fill(8);
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 65;
        ppu->background_mode=2;ppu->bg2_vertical_offsets_enabled=true;
        ppu->vram[0x2fbf*2]=16;ppu->vram[0x2fbf*2+1]=0x40;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{255,0,0,255}) return 66;
        // At +X the reflected sky is beyond column 32. Continue the fitted
        // roll, rather than pinning it to the final column's 248-pixel offset.
        {
            // Empty atlas rows above authored sky must not leak backdrop
            // colour into an offscreen reflection. Exercise metadata changes
            // on the same PPU/settings addresses, not just a fresh upload.
            auto sky_ppu=std::make_shared<starfox::simulation::SnesPpuState>(*ppu);
            sky_ppu->vram.fill(0);
            sky_ppu->bg2_scanline_scroll_enabled=false;
            for(unsigned col=0;col<32;++col) sky_ppu->vram[(0x2fa0+col)*2+1]=0x40;
            for(unsigned row=0;row<8;++row)
                sky_ppu->vram[sky_ppu->bg2_character_base*2+32+row*2]=255;
            for(unsigned col=0;col<64;++col) {
                const unsigned tile=sky_ppu->bg2_screen_base+2048+(col>>5)*1024+(col&31);
                sky_ppu->vram[(tile*2)&65535]=1; // First sky row at source Y=256.
            }
            auto sky_draw=authored;sky_draw.ppu=sky_ppu;
            auto sky_input=tiles;sky_input.background=&sky_draw;
            const auto check_sky=[&](unsigned bound,const std::vector<std::uint8_t>& expected) {
                sky_draw.settings.sky_source_min=bound;
                return gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&sky_input)
                    && gpu.readback_resident(result) && result==expected;
            };
            const std::vector<std::uint8_t> empty{0x77,0x88,0x99,255},green{0,255,0,255};
            if(!check_sky(0,empty) || !check_sky(256,green)
                || !check_sky(0,empty) || !check_sky(2048,empty)) return 112;
            std::cout<<"DXR authored sky edge: enabled/disabled/invalid bounds and metadata updates passed\n";
        }
        for(unsigned col=0;col<32;++col) {
            const unsigned word=0x4000+col*8,address=(0x2fa0+col)*2;
            ppu->vram[address]=word&255;ppu->vram[address+1]=word>>8;
        }
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 67;
        for(unsigned col=0;col<64;++col) {
            const unsigned word=ppu->bg2_screen_base+(col>>5)*1024+14*32+(col&31);
            ppu->vram[(word*2)&65535]=1;
        }
        // Hardware scroll words wrap at 8192, but the fitted slope must not.
        for(unsigned col=0;col<32;++col) {
            const unsigned word=0x4000|((8184+col*8)&8191),address=(0x2fa0+col)*2;
            ppu->vram[address]=word&255;ppu->vram[address+1]=word>>8;
        }
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 68;
        // Reflected terrain must use the selected enhanced material, not
        // the original green background. Changing modes on the same pointer
        // must invalidate the uploaded metadata as well.
        starfox::render::EnvironmentEffects enhanced_environment;
        enhanced_environment.classes[1]=1;enhanced_environment.classes[2]=1;
        enhanced_environment.modes[0]=6;
        enhanced_environment.motion={0,0,0,0};
        authored.settings.reflection_environment=&enhanced_environment;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result[2]<=result[1]) return 97;
        enhanced_environment.modes[0]=8;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result[0]<=result[2]) return 98;
        starfox::render::BackdropImage reflected_sky;
        reflected_sky.width=4;reflected_sky.height=2;reflected_sky.pixels.assign(8,0xff38220cu);
        enhanced_environment.backdrop=&reflected_sky;enhanced_environment.modes[2]=1;
        enhanced_environment.motion[0]=1000;enhanced_environment.plane[3]=1;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{12,34,56,255}) return 100;
        if(gpu.last_backdrop_upload_bytes()!=32) return 102;
        enhanced_environment.plane[3]=.5f;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{6,17,28,255}) return 101;
        if(gpu.last_backdrop_upload_bytes()!=0) return 103;
        enhanced_environment.plane[3]=1;
        const auto check_backdrop=[&](std::size_t bytes,std::vector<std::uint8_t> expected) {
            return gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
                && gpu.readback_resident(result) && result==expected && gpu.last_backdrop_upload_bytes()==bytes;
        };
        reflected_sky.pixels.assign(8,0xff785028u);
        if(!check_backdrop(32,{40,80,120,255})) return 104;
        enhanced_environment.backdrop_palette={{{0,0,0,.5f},{0,0,0,.5f}}};
        if(!check_backdrop(0,{20,40,60,255})) return 110;
        enhanced_environment.backdrop_palette={{{10.f/255,-20.f/255,30.f/255,.5f},{0,0,0,1}}};
        if(!check_backdrop(0,{30,20,90,255})) return 111;
        reflected_sky.pixels.assign(8,0xffffffffu);
        enhanced_environment.backdrop_projection[3]=6;
        enhanced_environment.backdrop_keep[0]={0,0,100000,100000};
        enhanced_environment.backdrop_ramp[1]=enhanced_environment.backdrop_ramp[2]=0x00785028;
        if(!check_backdrop(32,{40,80,120,255})) return 117;
        enhanced_environment.backdrop_ramp[1]=enhanced_environment.backdrop_ramp[2]=0x00604020;
        if(!check_backdrop(0,{32,64,96,255})) return 118;
        enhanced_environment.backdrop_projection[3]=7;
        if(!check_backdrop(0,{32,64,96,255})) return 123;
        enhanced_environment.backdrop_ramp[6]=1;
        enhanced_environment.backdrop_ramp[7]=0x235ed2;
        if(!check_backdrop(0,{210,94,35,255})) return 125;
        enhanced_environment.backdrop_ramp[6]=2;
        enhanced_environment.backdrop_ramp[7]=0x786878;
        if(!check_backdrop(0,{120,104,120,255})) return 126;
        enhanced_environment.backdrop_ramp[6]=0;
        {
            const auto old_ramp=enhanced_environment.backdrop_ramp;
            const auto old_keep=enhanced_environment.backdrop_keep;
            enhanced_environment.backdrop_projection[3]=9;
            // This fixture reflects toward +X: authored screen coordinate
            // (128 + pi/2*256, 112). Put the crescent there with slight shear.
            enhanced_environment.backdrop_keep[1]={.01f,0,200-402.12386f-1.12f,224};
            enhanced_environment.backdrop_ramp.fill(0x00302010);
            enhanced_environment.backdrop_ramp[0]=0;
            if(!check_backdrop(0,{16,32,48,255})) return 127;
            enhanced_environment.backdrop_ramp[1]=0x00605040;
            if(!check_backdrop(0,{64,80,96,255})) return 128;
            enhanced_environment.backdrop_ramp=old_ramp;
            enhanced_environment.backdrop_keep=old_keep;
            std::cout<<"DXR crescent: live palette changes without texture re-upload passed\n";
        }
        enhanced_environment.backdrop_projection[3]=6;
        enhanced_environment.backdrop_keep[1]={0,0,-1,0};
        if(!check_backdrop(0,{138,108,158,255})) return 120;
        enhanced_environment.backdrop_projection[3]=0;
        enhanced_environment.backdrop_keep={};
        enhanced_environment.backdrop_ramp={};
        reflected_sky.pixels.assign(8,0xff785028u);
        if(!check_backdrop(32,{30,20,90,255})) return 119;
        enhanced_environment.backdrop_palette={{{0,0,0,1},{0,0,0,1}}};
        enhanced_environment.backdrop_ramp.fill(0x00203040);enhanced_environment.backdrop_ramp[0]=1;
        if(!check_backdrop(0,{64,48,32,255})) return 112;
        enhanced_environment.backdrop_ramp={};
        enhanced_environment.backdrop_ramp[0]=2;
        enhanced_environment.backdrop_ramp[1]=0x00605040;
        enhanced_environment.backdrop_ramp[8]=0x0038220c;
        reflected_sky.pixels.assign(8,0xffb40000u);
        if(!check_backdrop(32,{12,34,56,255})) return 113;
        enhanced_environment.backdrop_ramp[8]=0x00605040;
        if(!check_backdrop(0,{64,80,96,255})) return 114;
        reflected_sky.pixels.assign(8,0xff0000b4u);
        if(!check_backdrop(32,{64,80,96,255})) return 115;
        reflected_sky.pixels.assign(8,0xff000000u);
        if(!check_backdrop(32,{0,0,0,255})) return 116;
        reflected_sky.pixels.assign(8,0xffc8c8c8u);
        if(!check_backdrop(32,{200,200,200,255})) return 117;
        std::cout<<"DXR nebula: independent color families, live metadata, black and neutral stars passed\n";
        enhanced_environment.backdrop_ramp={};
        reflected_sky.pixels.assign(8,0xff785028u);
        if(!check_backdrop(32,{40,80,120,255})) return 118;
        reflected_sky.height=4;reflected_sky.pixels.resize(16);
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x)
            reflected_sky.pixels[y*4+x]=0xff000000U|(y*40U);
        const auto saved_projection=enhanced_environment.backdrop_projection;
        enhanced_environment.backdrop_projection={0,0,1.5f,2};
        if(!check_backdrop(64,{60,0,0,255})) return 119;
        enhanced_environment.backdrop_projection={0,0,1.5f,3};
        enhanced_environment.backdrop_keep[0]={0,0,100000,100000};
        if(!check_backdrop(0,{120,0,0,255})) return 121;
        enhanced_environment.backdrop_projection[3]=4;
        enhanced_environment.backdrop_keep[1]={.3f,-.3f,530,30};
        if(!check_backdrop(0,{120,0,0,255})) return 122;
        enhanced_environment.backdrop_keep={};
        enhanced_environment.backdrop_projection={0,0,10,7};
        // The first two rows are the orbital panorama, the last two moons.
        // Very large surface UV must clamp to row one, not select a moon.
        if(!check_backdrop(0,{40,0,0,255})) return 124;
        enhanced_environment.backdrop_projection=saved_projection;
        reflected_sky.height=2;reflected_sky.pixels.assign(8,0xff785028u);
        if(!check_backdrop(32,{40,80,120,255})) return 120;
        reflected_sky.width=8;reflected_sky.height=1;
        enhanced_environment.motion[3]=91;enhanced_environment.plane[2]=512;
        if(!check_backdrop(0,{40,80,120,255})) return 105;
        reflected_sky.width=32;reflected_sky.height=32;reflected_sky.pixels.assign(1024,0xff1e140au);
        if(!check_backdrop(4096,{10,20,30,255})) return 106;
        // A sub-native-pixel camera motion must not quantize a photographic
        // reflection to the original 256-wide tile grid. The gradient yields
        // several distinct colours within less than one native pixel.
        reflected_sky.width=4096;reflected_sky.height=1;reflected_sky.pixels.resize(4096);
        for(unsigned x=0;x<4096;++x) reflected_sky.pixels[x]=0xff000000U|(x&255U);
        unsigned changes=0,last=256;
        for(unsigned step=0;step<16;++step) {
            auto micro_camera=camera;micro_camera.center_x+=double(step)*.0002;
            if(!gpu.render_resident(moved,micro_camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
                || !gpu.readback_resident(result)) return 109;
            if(result[0]!=last) ++changes;
            last=result[0];
        }
        if(changes<6) {std::cerr<<"Photographic reflection snapped: "<<changes<<" distinct subpixel steps\n";return 110;}
        std::cout<<"Photographic reflection: "<<changes<<" distinct sub-native-pixel camera steps\n";
        reflected_sky.width=32;reflected_sky.height=32;reflected_sky.pixels.assign(1024,0xff1e140au);
        authored.settings.reflection_environment=nullptr;
        if(!gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !gpu.readback_resident(result) || result!=std::vector<std::uint8_t>{0,255,0,255}) return 99;
        if(gpu.last_backdrop_upload_bytes()!=0) return 107;
        reflected_sky.pixels.assign(1024,0xff3c2814u);
        authored.settings.reflection_environment=&enhanced_environment;
        if(!check_backdrop(4096,{20,40,60,255}) || !check_backdrop(0,{20,40,60,255})) return 108;
        reflected_sky.seal_for_upload();
        if(!check_backdrop(4096,{20,40,60,255}) || !check_backdrop(0,{20,40,60,255})) return 111;
        reflected_sky.immutable_upload_key=0;
        reflected_sky.pixels.assign(1024,0xff3c2814u);
        reflected_sky.seal_for_upload();
        if(!check_backdrop(4096,{20,40,60,255}) || !check_backdrop(0,{20,40,60,255})) return 112;
        DxrShadows fresh_backdrop;
        if(!fresh_backdrop.available() || !fresh_backdrop.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || !fresh_backdrop.readback_resident(result) || result!=std::vector<std::uint8_t>{20,40,60,255}
            || fresh_backdrop.last_backdrop_upload_bytes()!=4096) return 109;
        authored.settings.reflection_environment=nullptr;
        std::cout<<"DXR backdrop residency: no unchanged image upload; edits, dimensions, growth, toggle and fresh device passed\n";
        std::cout<<"DXR enhanced environment: water/gold replacement and disabled restoration passed\n";
        authored.settings.layer=1;
        if(gpu.render_resident(moved,camera,{0,1,0},{},nullptr,nullptr,false,false,&tiles)
            || gpu.resident_output().resource) return 64;
        // Water is a ray receiver in front of scene geometry, not a flipped
        // framebuffer. A reflected red wall lies outside the primary view.
        Scene water_scene;water_scene.add({{-20,-20,4},{20,-20,4},{0,20,4}});
        starfox::render::RayMaterials water_materials;water_materials.triangles.resize(1);
        water_materials.triangles[0].even=2;palette[2]=0xff0000ff;
        RayWater water;water.reflection_strength=1;
        DxrShadows::ReflectionInput water_input{&water_materials,palette,0xffb08040};
        water_input.ground=ReceiverPlane{{0,2,0},{0,1,0}};water_input.water=&water;
        const Camera water_camera{1,1,1,.5,-.5};
        const auto water_capture=[&] {
            return gpu.render_resident(water_scene,water_camera,{0,1,0},{},nullptr,nullptr,false,false,&water_input)
                && gpu.readback_resident(result) && result.size()==4 && result[3]==254;
        };
        if(!water_capture()) {std::cerr<<"Water receiver missing: "<<gpu.status()<<'\n';return 90;}
        const auto wet=result;
        if(!water_capture() || result!=wet) return 91;
        water.reflection_strength=0;
        if(!water_capture() || result==wet || result[0]>=wet[0]) return 92;
        water.time=3;
        if(!water_capture()) return 93;
        water.reflection_strength=1;water.material=1;
        if(!water_capture()) return 95;
        const auto mirror=result;
        water.time=1234;
        if(!water_capture() || result!=mirror) return 104;
        water.material=2;
        if(!water_capture() || result==mirror) return 96;
        water_input.water=nullptr;
        if(!gpu.render_resident(water_scene,water_camera,{0,1,0},{},nullptr,nullptr,false,false,&water_input)
            || !gpu.readback_resident(result) || result[3]!=255) return 94;
        std::cout<<"DXR water: physical receiver, scene-hit reflection, deterministic waves, reflection Off and legacy model isolation passed\n";
        std::cout<<"DXR reflections: offscreen colour, alpha holes, texture/palette/motion updates, conductor tint, stable roughness, environment updates/occlusion and partial groups passed\n";
    }
#endif
    starfox::render::RowWorkers workers; workers.set_worker_count(4);
    for (unsigned scale:{1U,2U,4U}) {
        const Camera camera{400*scale,224*scale,256.0*scale,200.0*scale,112.0*scale};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> cpu,hardware;
        std::vector<double> cpu_times;
        for(unsigned i=0;i<5;++i) {
            const auto start=std::chrono::steady_clock::now();
            render_mask(scene,camera,{-1,-1,-1},ground,cpu,&workers);
            if(i) cpu_times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(cpu_times.begin(),cpu_times.end());
        std::vector<double> times;
        for (unsigned i=0;i<12;++i) {
            const auto start=std::chrono::steady_clock::now();
            if (!gpu.render(scene,camera,{-1,-1,-1},ground,hardware)) {
                std::cerr << gpu.status() << '\n'; return 1;
            }
            if(i>=2) times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());
        if (hardware.size()!=cpu.size()) return 11;
        const auto shadowed=std::count_if(hardware.begin(),hardware.end(),
            [](std::uint8_t value){return value!=0;});
        // Matching empty masks are not evidence of working ray tracing.
        if(shadowed==0) {std::cerr << "No shadow rays affected the fixture\n";return 12;}
        std::size_t different=0; unsigned largest=0;
        for(std::size_t i=0;i<cpu.size();++i) {
            different+=cpu[i]!=hardware[i];
            largest=std::max(largest,unsigned(std::abs(int(cpu[i])-int(hardware[i]))));
        }
        std::cout << gpu.status() << " scale=" << scale << " median_ms=" << times[times.size()/2]
            << " cpu_median_ms=" << cpu_times[cpu_times.size()/2]
            << " shadowed_pixels=" << shadowed
            << " differing_pixels=" << different << '/' << cpu.size() << " max_delta=" << largest << '\n';
        if (different>cpu.size()/1000) return 2;
    }
    // Benchmark changing geometry as well: static inputs reuse the AS and
    // conceal the work performed for moving ships/enemies during gameplay.
#if !defined(STARFOX_TEST_PORTABLE_SHADOWS)
    {
        const Camera camera{800,448,512,400,224};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        const auto original=scene.triangles();
        const std::vector<Triangle> source(original.begin(),original.end());
        std::vector<double> downloaded_times,resident_times;
        DxrShadows resident;
        for(unsigned frame=0;frame<32;++frame) {
            const Vec3 shift{double(frame%7)*0.125,0,double(frame)*0.0625};
            Scene animated;
            for(const auto& triangle:source)
                animated.add({triangle.a+shift,triangle.b+shift,triangle.c+shift});
            animated.build();
            // Separate producers prevent the second path reusing the first
            // path's just-built geometry and biasing the comparison.
            std::vector<std::uint8_t> downloaded,read;
            auto start=std::chrono::steady_clock::now();
            if(!gpu.render(animated,camera,{-1,-1,-1},ground,downloaded)) return 31;
            const auto download_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            start=std::chrono::steady_clock::now();
            if(!resident.render_resident(animated,camera,{-1,-1,-1},ground)) return 32;
            const auto resident_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(!resident.readback_resident(read) || read!=downloaded) return 33;
            if(frame>=8) {downloaded_times.push_back(download_ms);resident_times.push_back(resident_ms);}
        }
        std::sort(downloaded_times.begin(),downloaded_times.end());
        std::sort(resident_times.begin(),resident_times.end());
        std::cout<<"Animated DXR 2x: download_median_ms="<<downloaded_times[downloaded_times.size()/2]
            <<" resident_median_ms="<<resident_times[resident_times.size()/2]
            <<"; 32 exact mask comparisons passed (resident timing excludes diagnostic download)\n";
    }
#endif
    // Rebuild and clear the same object to catch accidental cross-frame
    // geometry reuse. Exercise a different size, light axis and receiver.
    for(unsigned variant=0;variant<6;++variant) {
        scene.clear();
        if(variant!=5) {
            const double z=40+variant*11;
            scene.add({{-20,-10,z},{20,-10,z},{0,20,z+6}});
            scene.add({{0,20,z+6},{20,-10,z},{-20,-10,z}});
        }
        scene.build();
        const Camera camera{133+variant,79+variant,100,64,35};
        const Vec3 light=variant%2?Vec3{0,-1,0}:Vec3{-1,-1,-1};
        std::optional<ReceiverPlane> ground;
        if(variant%2) ground=ReceiverPlane{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> cpu,hardware;
        render_mask(scene,camera,light,ground,cpu,&workers);
        if(!gpu.render(scene,camera,light,ground,hardware) || hardware.size()!=cpu.size()) return 4;
        std::size_t differences=0;
        for(std::size_t i=0;i<cpu.size();++i) differences+=cpu[i]!=hardware[i];
        if(differences>cpu.size()/1000) return 5;
    }
    std::cout<<"Changing geometry, size, light, winding, receiver and empty scene: passed\n";
    // A nonempty mask could contain only model self-shadowing. Prove that a
    // detached caster also shades the ground outside its screen silhouette.
    scene.clear();
    scene.add({{-20,0,80},{20,0,80},{20,0,120}});
    scene.add({{-20,0,80},{20,0,120},{-20,0,120}});
    scene.build();
    {
        const Camera camera{133,79,100,64,35};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> mask,without_ground;
        if(!gpu.render(scene,camera,{0,-1,0},ground,mask)
            || !gpu.render(scene,camera,{0,-1,0},{},without_ground)
            || mask.size()!=133U*79U || without_ground.size()!=mask.size()
            || mask[60U*133U+64U]!=160U
            || without_ground[60U*133U+64U]!=0U
            || mask[10U*133U+64U]!=0U
            || mask[60U*133U+4U]!=0U) {
            std::cerr<<"Detached caster failed ground-only shadow/sky isolation\n";
            return 15;
        }
    }
    std::cout<<"Detached ground shadow, absent receiver and unshadowed sky: passed\n";
    // Exercise partial workgroups and every packed-row remainder, including
    // one-pixel rows. A later render must not retain another size's padding.
    scene.clear();
    scene.add({{-20,-10,40},{20,-10,40},{0,20,46}});
    scene.add({{0,20,46},{20,-10,40},{-20,-10,40}});
    scene.build();
    for(unsigned width=1;width<=17;++width) for(unsigned height:{1U,3U,9U}) {
        const Camera camera{width,height,12,double(width)/2,double(height)/2};
        std::vector<std::uint8_t> cpu,hardware;
        render_mask(scene,camera,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},cpu,&workers);
        if(!gpu.render(scene,camera,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},hardware)
            || hardware!=cpu) {
            std::cerr << "Partial shadow workgroup mismatch: " << width << 'x' << height << '\n';
            return 13;
        }
    }
    std::cout<<"All 51 partial-workgroup/packed-row fixtures passed\n";
    for(double receiver_depth:{384.,512.,768.}) {
        std::array<std::vector<std::uint8_t>,2> eye_masks;
        for(unsigned eye=0;eye<2;++eye) {
            const double eye_x=eye?4.:-4.;
            Scene eye_scene;
            const Vec3 offset{-eye_x,0,0};
            eye_scene.add({Vec3{-20,-20,256}+offset,Vec3{20,-20,256}+offset,Vec3{20,20,256}+offset});
            eye_scene.add({Vec3{-20,-20,256}+offset,Vec3{20,20,256}+offset,Vec3{-20,20,256}+offset});
            eye_scene.build();
            const Camera camera{400,224,256,200+256*eye_x/512.,112};
            const ReceiverPlane receiver{{-eye_x,0,receiver_depth},{0,0,-1}};
            std::vector<std::uint8_t> reference;
            render_mask(eye_scene,camera,{-1,0,-1},receiver,reference,&workers);
            if(!gpu.render(eye_scene,camera,{-1,0,-1},receiver,eye_masks[eye]) || eye_masks[eye]!=reference) {
                std::cerr<<"Stereo receiver GPU/CPU mismatch\n";return 15;
            }
            // Independent world-space projection of the displaced shadow's
            // centre onto the receiver, away from the visible caster itself.
            const double world_x=receiver_depth-256.;
            const int x=int(200+256*(world_x-eye_x)/receiver_depth+256*eye_x/512.);
            if(!eye_masks[eye][112*400+unsigned(x)]) {
                std::cerr<<"Stereo shadow missing at projected receiver\n";return 16;
            }
        }
        if(receiver_depth==512.) {
            // Only compare the receiver patch; caster silhouettes have their
            // own depth and correctly retain stereo disparity.
            for(unsigned y=105;y<119;++y) for(unsigned x=320;x<337;++x)
                if(eye_masks[0][y*400+x]!=eye_masks[1][y*400+x]) return 17;
        }
    }
    std::cout<<"Stereo shadow receiver near/convergence/far CPU/GPU checks passed\n";
#if !defined(STARFOX_TEST_PORTABLE_SHADOWS)
    // A new backend must build its AS. Compare that independent result with
    // the reused backend while alternating stable and changed input geometry.
    for(unsigned variant=0;variant<12;++variant) {
        Scene casters;
        const double z=80+16*(variant/3%2);
        casters.add({{-20,0,z},{20,0,z},{0,0,z+40}});casters.build();
        const Camera camera{133+variant,79,100,64+double(variant),35};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        const Vec3 light{variant%2?-.4:.4,-1,0};
        DxrShadows fresh;
        std::vector<std::uint8_t> rebuilt,reused;
        if(!fresh.render(casters,camera,light,ground,rebuilt)
            || !gpu.render(casters,camera,light,ground,reused) || rebuilt!=reused) return 23;
    }
    std::cout<<"Cached acceleration structures match fresh builds across geometry/camera/light changes\n";
    for(unsigned variant=0;variant<7;++variant) {
        const Camera camera{133+variant*3,79+variant*2,100,64,35};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> expected,read;
        if(!gpu.render(scene,camera,{-1,-1,-1},ground,expected)
            || gpu.resident_output().resource
            || !gpu.render_resident(scene,camera,{-1,-1,-1},ground)) return 18;
        const auto output=gpu.resident_output();
        if(variant==0) {
            std::cout<<"DXR producer LUID=";
            for(const auto byte:output.adapter_luid) std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
            std::cout<<std::dec<<'\n';
        }
        if(!output.device || !output.resource || output.width!=camera.width
            || output.height!=camera.height || output.row_bytes!=((camera.width+3U)&~3U)) return 19;
#if defined(_WIN32)
        const auto handle=gpu.export_resident_handle();
        if(!handle) return 24;
        ID3D12Resource* imported{};
        const auto opened=static_cast<ID3D12Device*>(output.device)->OpenSharedHandle(handle,
            IID_ID3D12Resource,reinterpret_cast<void**>(&imported));
        CloseHandle(handle);
        if(FAILED(opened) || !imported) return 25;
        const auto description=imported->GetDesc();
        imported->Release();
        if(description.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER
            || description.Width<uint64_t(output.row_bytes)*output.height) return 26;
        const auto ready=gpu.export_ready_fence_handle();
        if(!ready || gpu.resident_output().ready_value<=output.ready_value) return 28;
        CloseHandle(ready);
        const auto stable_value=gpu.resident_output().ready_value;
        const auto repeated=gpu.export_ready_fence_handle();
        if(!repeated || gpu.resident_output().ready_value!=stable_value) return 29;
        CloseHandle(repeated);
#endif
        if(!gpu.readback_resident(read) || read!=expected
            || !gpu.readback_resident(read) || read!=expected) return 20;
        if(!gpu.render_resident(scene,camera,{-1,-1,-1},ground,nullptr,nullptr,true,true)) return 34;
        const auto released=gpu.resident_output();
#if defined(_WIN32)
        const auto folded_fence=gpu.export_ready_fence_handle();
        if(!folded_fence || gpu.resident_output().ready_value!=released.ready_value) return 35;
        CloseHandle(folded_fence);
#endif
        if(!gpu.readback_resident(read) || read!=expected) return 36;
        if(gpu.render_resident(scene,camera,{-1,-1,-1},ground,nullptr,nullptr,false,true)) return 37;
        // Reuse immediately after a deferred dispatch, without a consumer
        // download incidentally waiting for the producer first.
        if(!gpu.render_resident(scene,camera,{-1,-1,-1},ground,nullptr,nullptr,true,true)
            || !gpu.render_resident(scene,camera,{-1,-1,-1},ground,nullptr,nullptr,true,true)
            || !gpu.readback_resident(read) || read!=expected) return 38;
    }
    Scene empty_scene;empty_scene.build();
    std::vector<std::uint8_t> stale{1,2,3};
    if(gpu.render_resident(empty_scene,{133,79,100,64,35},{-1,-1,-1},{})
        || gpu.resident_output().resource || gpu.readback_resident(stale) || !stale.empty()) return 21;
    if(gpu.export_resident_handle() || gpu.export_ready_fence_handle()) return 27;
    if(!gpu.render_resident(scene,{133,79,100,64,35},{-1,-1,-1},{})
        || !gpu.readback_resident(stale) || stale.size()!=133*79) return 22;
    std::cout<<"DXR resident masks: exact repeated downloads, folded external release, resize, invalidation and recovery passed\n";
#endif
#if defined(STARFOX_TEST_PORTABLE_SHADOWS)
    struct Device {
        SDL_GPUDevice* value=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL,false,nullptr);
        ~Device(){if(value) SDL_DestroyGPUDevice(value);}
    } borrowed;
    if(!borrowed.value) return 6;
    PortableShadows resident;
    starfox::render::SdlGpuEffects effects;
    for(unsigned variant=0;variant<5;++variant) {
        scene.clear();
        const double z=45+variant*8;
        scene.add({{-20,-10,z},{20,-10,z},{0,20,z+6}});scene.build();
        const Camera camera{133+variant*3,79+variant*2,100,64,35};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> mask,read;
        if(!gpu.render(scene,camera,{-1,-1,-1},ground,mask)
            || !resident.render_resident(borrowed.value,scene,camera,{-1,-1,-1},ground)) return 7;
        const auto output=resident.output();
        if(output.device!=borrowed.value || !output.buffer || output.width!=camera.width
            || output.height!=camera.height) return 8;
        if(!resident.readback(read) || read!=mask || !resident.readback(read) || read!=mask) return 9;
        starfox::render::Framebuffer frame(camera.width,camera.height+4);
        frame.enable_layer_tags(true);
        for(unsigned y=0;y<frame.height();++y) for(unsigned x=0;x<frame.width();++x)
            frame.set_stored(x,y,1,static_cast<starfox::render::PixelLayer>(x%5));
        std::vector<std::uint8_t> expected(frame.pixels().size()*4,200),direct=expected;
        starfox::render::GpuEffectSettings settings;
        settings.shadow_mask=mask;settings.shadow_width=camera.width;settings.shadow_height=camera.height;
        settings.shadow_offset_y=int(variant)-2;
        if(!effects.apply(borrowed.value,frame,expected,settings)) return 10;
        settings.shadow_mask={};settings.resident_shadow=output;
        if(!effects.apply(borrowed.value,frame,direct,settings) || direct!=expected) return 11;
    }
    scene.clear();scene.build();
    if(resident.render_resident(borrowed.value,scene,{133,79,100,64,35},{-1,-1,-1},{})
        || resident.output().buffer) return 12;
    effects.release_device();resident.release_device();
    if(resident.output().buffer || !SDL_GetGPUShaderFormats(borrowed.value)) return 13;
    scene.add({{-20,-10,45},{20,-10,45},{0,20,51}});scene.build();
    std::vector<std::uint8_t> restored;
    if(!resident.render_resident(borrowed.value,scene,{133,79,100,64,35},{-1,-1,-1},{})
        || !resident.readback(restored)) return 14;
    std::cout<<"Resident shadow masks, blend offsets/tags, invalidation and borrowed lifetime: exact\n";
#endif
}
