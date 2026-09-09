#include "starfox/render/shadow_mask.hpp"
#include <chrono>
#include <iostream>

int main() {
    using namespace starfox::render::shadows;
    Scene scene;
    for(int z=0;z<8;++z) for(int x=-4;x<4;++x) {
        const Vec3 a{x*18.0,-12.0,z*25.0+60};
        scene.add({a,a+Vec3{14,0,0},a+Vec3{7,20,4}});
        scene.add({a,a+Vec3{7,20,4},a+Vec3{0,0,12}});
    }
    scene.build();
    starfox::render::RowWorkers workers;
    workers.set_worker_count(4);
    for(unsigned scale:{1U,2U}) {
        std::vector<std::uint8_t> mask;
        std::vector<double> times;
        for(unsigned run=0;run<12;++run) {
            const auto start=std::chrono::steady_clock::now();
            render_mask(scene,{400*scale,224*scale,256.0*scale,200.0*scale,112.0*scale},
                {-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},mask,&workers);
            const auto end=std::chrono::steady_clock::now();
            if(run>=2)times.push_back(std::chrono::duration<double,std::milli>(end-start).count());
        }
        std::sort(times.begin(),times.end());
        std::uint64_t hash=14695981039346656037ULL;
        for(auto b:mask){hash^=b;hash*=1099511628211ULL;}
        std::cout<<"scale="<<scale<<" median_ms="<<times[5]<<" p90_ms="<<times[9]
            <<" hash="<<hash<<std::endl;
    }
}
