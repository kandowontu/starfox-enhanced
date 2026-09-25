#include "starfox/render/dxr_shadows.hpp"
#include <iostream>
#include <stdexcept>
#ifdef STARFOX_DXR
#error This diagnostic must compile without the Windows DXR backend.
#endif
int main() try {
    using namespace starfox::render::shadows;
    DxrShadows backend;
    const auto require=[](bool condition) {if(!condition) throw std::runtime_error("Unsupported DXR contract failed");};
    Scene scene;Camera camera{};Vec3 light{0,1,0};
    std::vector<std::uint8_t> mask{17,23};
    require(!backend.available());
    require(!backend.render(scene,camera,light,std::nullopt,mask));
    require(mask==std::vector<std::uint8_t>{17,23}); // Caller-owned fallback data.
    for(bool deferred:{false,true}) {
        require(!backend.render_resident(scene,camera,light,std::nullopt,nullptr,nullptr,true,deferred));
        require(!backend.resident_output().resource && !backend.resident_output().device);
        require(!backend.prepare_shared_geometry(3).resource);
        require(!backend.export_geometry_handle() && !backend.export_resident_handle()
            && !backend.export_ready_fence_handle());
    }
    require(!backend.readback_resident(mask) && mask.empty());
    starfox::render::RayMaterials materials;std::array<std::uint32_t,256> palette{};
    require(!backend.render_reflections(scene,camera,materials,palette,0,mask) && mask.empty());
    DxrShadows::ReflectionInput reflection{&materials,palette,0};
    require(!backend.render_resident(scene,camera,light,{},nullptr,nullptr,true,true,&reflection));
    std::cout<<"Unsupported DXR declines cleanly; no resources/handles or stale readback\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
