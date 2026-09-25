#include "starfox/vr/application.hpp"
#include "starfox/vr/cartridge_save.hpp"
#include "starfox/vr/packet_route.hpp"
#include "starfox/state/files.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>
int main() try {
    {
        using namespace starfox::vr;
        SceneVertex vertex{};
        if(packet_route({},{})!=PacketRoute::empty) throw std::runtime_error("Empty routing inventory");
        const std::pair<uint32_t,PacketRoute> fixtures[]{
            {0x28000005U,PacketRoute::sprite},{0x28000007U,PacketRoute::sprite},
            {0x80000000U,PacketRoute::particle},{0x80000006U,PacketRoute::particle},
            {134217728U|1030U,PacketRoute::text},{68U,PacketRoute::grid},
            {132U,PacketRoute::dust},{132U|2048U,PacketRoute::dust},
            {132U|16384U,PacketRoute::dust},{512U,PacketRoute::cpu_connected_grid},
            {512U|4194304U,PacketRoute::connected_grid},
            {4U,PacketRoute::ordinary},{0x80000008U,PacketRoute::procedural}};
        for(const auto& [flags,expected]:fixtures) {
            vertex.texture[3]=flags;
            const auto one=std::span<const SceneVertex>(&vertex,1);
            if(packet_route(one,{})!=expected || packet_route({},one)!=expected)
                throw std::runtime_error("Triangle/line producer classification mismatch");
        }
        SceneVertex particle{};particle.texture[3]=0x80000000U;
        vertex.texture[3]=512U;
        if(packet_route(std::span(&vertex,1),std::span(&particle,1))!=PacketRoute::procedural)
            throw std::runtime_error("Mixed producer packet hidden by triangle-only classification");
        std::cout<<"Producer inventory includes line-only and connected-grid CPU paths\n";
    }
    starfox::vr::ApplicationHost host;
    unsigned polls=0;
    host.frame_limit=0;host.time_limit=std::chrono::seconds(0);
    host.stop_requested=[&]{++polls;return true;};
    // No arguments, ROM, loader or headset needed when the host already exited.
    if(starfox::vr::run_application(0,nullptr,host)!=0 || polls!=1) return 1;
    std::cout<<"VR host cancellation precedes assets and runtime startup\n";
    {
        starfox::vr::ApplicationHost audit_host;
        char program[]="vr-test", invulnerable[]="--preflight-invulnerable";
        char graphics[]="--graphics";
        char* audit_only[]{program,invulnerable};
        char* graphics_audit[]{program,graphics,invulnerable};
        if(starfox::vr::run_application(2,audit_only,audit_host)!=1
            || starfox::vr::run_application(3,graphics_audit,audit_host)!=1)
            throw std::runtime_error("Diagnostic invulnerability escaped preflight validation");
    }
    const auto directory=std::filesystem::temp_directory_path()/
        ("starfox-vr-save-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(directory)) throw std::runtime_error("Test directory collision");
    struct Cleanup {
        std::filesystem::path directory;
        ~Cleanup() {std::error_code ignored;std::filesystem::remove(directory/"test.srm",ignored);std::filesystem::remove(directory,ignored);}
    } cleanup{directory};
    const auto path=directory/"test.srm";
    std::vector<uint8_t> bytes(65536);bytes[42]=123;
    starfox::vr::CartridgeSave disabled;
    if(disabled.synchronize(bytes)) throw std::runtime_error("Disabled persistence wrote a file");
    starfox::vr::CartridgeSave save(path);
    if(!save.initial().empty() || !save.synchronize(bytes) || save.synchronize(bytes))
        throw std::runtime_error("New/unchanged cartridge persistence failed");
    starfox::vr::CartridgeSave loaded(path);
    if(!std::ranges::equal(loaded.initial(),bytes)) throw std::runtime_error("Save reload mismatch");
    bytes[255]=91;
    if(!loaded.synchronize(bytes) || starfox::state::read_file(path)!=bytes)
        throw std::runtime_error("Changed cartridge save not persisted");
    bool rejected=false;
    try {loaded.synchronize(std::span<const uint8_t>(bytes).first(3));} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected || starfox::state::read_file(path)!=bytes) throw std::runtime_error("Invalid save overwrote disk state");
    const auto previous=bytes;bytes[7]^=1;
    // A directory occupying this test-only destination forces replacement to
    // fail on both Windows and Android/POSIX. Never mark failed bytes as saved.
    std::filesystem::remove(path);std::filesystem::create_directory(path);
    rejected=false;
    try {loaded.synchronize(bytes);} catch(const std::exception&) {rejected=true;}
    if(!rejected || !std::ranges::equal(loaded.initial(),previous) || !std::filesystem::is_directory(path))
        throw std::runtime_error("Failed write advanced the persisted baseline");
    std::filesystem::remove(path);
    if(!loaded.synchronize(bytes) || starfox::state::read_file(path)!=bytes)
        throw std::runtime_error("Save did not recover after write failure");
    starfox::state::write_atomic(path,std::span<const uint8_t>(bytes).first(3));
    rejected=false;
    try {starfox::vr::CartridgeSave invalid(path);} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected || std::filesystem::file_size(path)!=3) throw std::runtime_error("Corrupt save silently replaced");
    std::cout<<"VR cartridge saves: create, reload, change, no-op, failure recovery and corrupt-file preservation passed\n";
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
