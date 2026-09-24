#include "starfox/vr/application.hpp"
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

namespace {
volatile std::sig_atomic_t interrupted=0;
void interrupt(int) {interrupted=1;}
}

int main(int argc,char** argv) try {
    const auto directory=std::filesystem::absolute(argv[0]).parent_path();
    auto bundle=directory/"Starfox-Assets.BIN";
    auto data=directory/"vr-data";
    std::string msu;
    bool enhanced_sky=false;
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--help" || option=="-h") {
            std::cout<<"Star Fox Enhanced PCVR (development)\n"
                "Usage: starfox_pcvr [--bundle Starfox-Assets.BIN] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky]\n"
                "--enhanced-sky: start with Enhanced Sky on (also in 2D Options); unsupported families remain native.\n"
                "Requires a Vulkan-capable GPU and an active OpenXR headset runtime.\n"
                "Default bundle: beside this executable. Saves/settings: vr-data beside this executable.\n"
                "Generate your own BIN with starfox_asset_builder; no cartridge is bundled.\n";
            return 0;
        }
        if(option=="--enhanced-sky") {enhanced_sky=true;continue;}
        if(i+1>=argc || (option!="--bundle" && option!="--data-dir" && option!="--msu")) {
            std::cerr<<"Unknown or incomplete option: "<<option<<". Use --help.\n";return 2;
        }
        if(option=="--bundle") bundle=std::filesystem::absolute(argv[++i]);
        else if(option=="--data-dir") data=std::filesystem::absolute(argv[++i]);
        else msu=argv[++i];
    }
    if(!std::filesystem::is_regular_file(bundle)) {
        std::cerr<<"Missing asset bundle: "<<bundle<<"\n"
            "Use starfox_asset_builder to prepare your own Starfox-Assets.BIN, then place it beside this executable or pass --bundle PATH.\n";
        return 2;
    }
    starfox::vr::ApplicationHost host;
    // The diagnostic host defaults to 120 frames / 30 seconds. A player
    // executable must run until an actual exit, never expire mid-game.
    host.frame_limit=0;host.time_limit=std::chrono::seconds(0);
    host.cartridge_save_path=data/"starfox-ex.srm";
    std::signal(SIGINT,interrupt);
    std::signal(SIGTERM,interrupt);
    host.stop_requested=[] {return interrupted!=0;};
    std::vector<std::string> arguments{argv[0],"--bundle",bundle.string()};
    if(enhanced_sky) arguments.emplace_back("--enhanced-sky");
    if(!msu.empty()) {arguments.emplace_back("--msu");arguments.push_back(msu);}
    std::vector<char*> pointers;
    for(auto& arg:arguments) pointers.push_back(arg.data());
    return starfox::vr::run_application(int(pointers.size()),pointers.data(),host);
} catch(const std::exception& error) {
    std::cerr<<"PCVR could not start: "<<error.what()<<'\n';return 1;
}
