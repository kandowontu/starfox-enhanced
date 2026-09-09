#include "starfox/render/bloom.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>

int main(int argc,char** argv) {
    using namespace starfox::render;
    RowWorkers workers;
    workers.set_worker_count(4);
    for (unsigned scale : {1U,2U,4U}) {
        if (argc>1 && scale!=unsigned(std::atoi(argv[1]))) continue;
        Framebuffer frame{796,224,scale};
        frame.enable_layer_tags(true);
        std::vector<std::uint8_t> original(frame.pixels().size()*4),rgba;
        for (std::size_t i=0;i<original.size();++i) original[i]=std::uint8_t(i*73+i/17);
        for (std::size_t i=0;i<frame.pixels().size();++i) frame.layer_tags()[i]=std::uint8_t((i/13+i/797)%5);
        for (bool threaded : {false,true}) {
            if (argc>2 && threaded!=(std::atoi(argv[2])!=0)) continue;
            BloomPass bloom;
            std::vector<double> times;
            for (unsigned run=0;run<45;++run) {
                rgba=original;
                const auto start=std::chrono::steady_clock::now();
                bloom.apply(3,2,frame,rgba,threaded?&workers:nullptr);
                const auto stop=std::chrono::steady_clock::now();
                if(run>=5) times.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
            }
            std::sort(times.begin(),times.end());
            std::uint64_t hash=14695981039346656037ULL;
            for (auto b:rgba) {hash^=b;hash*=1099511628211ULL;}
            std::cout << "scale=" << scale << " threaded=" << threaded << " median_us="
                << times[20] << " p95_us=" << times[38] << " hash=" << hash << std::endl;
        }
    }
}
