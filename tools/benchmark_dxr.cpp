#include "starfox/render/dxr_shadows.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>
int main() {
    using namespace starfox::render::shadows;
    Scene scene;
    for (int z=0;z<8;++z) for (int x=-4;x<4;++x) {
        const Vec3 a{x*18.0,-12,z*25.0+60};
        scene.add({a,a+Vec3{14,0,0},a+Vec3{7,20,4}});
        scene.add({a,a+Vec3{7,20,4},a+Vec3{0,0,12}});
    }
    scene.build();
    DxrShadows gpu;
    starfox::render::RowWorkers workers; workers.set_worker_count(4);
    for (unsigned scale:{1U,2U,4U}) {
        const Camera camera{400*scale,224*scale,256.0*scale,200.0*scale,112.0*scale};
        const ReceiverPlane ground{{0,25,0},{0,1,0}};
        std::vector<std::uint8_t> cpu,hardware;
        render_mask(scene,camera,{-1,-1,-1},ground,cpu,&workers);
        std::vector<double> times;
        for (unsigned i=0;i<12;++i) {
            const auto start=std::chrono::steady_clock::now();
            if (!gpu.render(scene,camera,{-1,-1,-1},ground,hardware)) {
                std::cerr << gpu.status() << '\n'; return 1;
            }
            if(i>=2) times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());
        std::size_t different=0; unsigned largest=0;
        for(std::size_t i=0;i<cpu.size();++i) {
            different+=cpu[i]!=hardware[i];
            largest=std::max(largest,unsigned(std::abs(int(cpu[i])-int(hardware[i]))));
        }
        std::cout << gpu.status() << " scale=" << scale << " median_ms=" << times[times.size()/2]
            << " differing_pixels=" << different << '/' << cpu.size() << " max_delta=" << largest << '\n';
        if (different>cpu.size()/1000) return 2;
    }
}
