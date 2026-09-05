// Development-only full-system reference. Ares remains an optional ISC
// dependency; this executable and reference game data are never packaged.
#include <sfc/sfc.hpp>
#include "starfox/simulation/wdc65816.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>

namespace sfc = ares::SuperFamicom;
#include "gameplay_audit.hpp"
namespace {
std::function<void(unsigned, unsigned)> cpu_hook;
std::function<unsigned(unsigned, unsigned)> gsu_write_hook;
std::function<void(unsigned, unsigned, unsigned, unsigned,
    std::uint64_t, std::uint64_t, bool)> gsu_hook;
std::uint64_t gsu_clocks{}, gsu_started{};
unsigned gsu_entry{}, gsu_clsr{}, gsu_cfgr{}, gsu_scmr{};
bool gsu_active{};
}
extern "C" void sfc_audit_cpu(unsigned pc, unsigned clocks) {
    if (cpu_hook) cpu_hook(pc, clocks);
}
extern "C" void sfc_audit_gsu_step(unsigned clocks) { gsu_clocks += clocks; }
extern "C" unsigned sfc_audit_gsu_write(unsigned address, unsigned data) {
    return gsu_write_hook ? gsu_write_hook(address, data) : data;
}
extern "C" void sfc_audit_gsu_start(unsigned pc, unsigned clsr, unsigned cfgr, unsigned scmr) {
    if (gsu_active && gsu_hook)
        gsu_hook(gsu_entry, gsu_clsr, gsu_cfgr, gsu_scmr, gsu_started, gsu_clocks, false);
    gsu_started = gsu_clocks;
    gsu_entry = pc; gsu_clsr = clsr; gsu_cfgr = cfgr; gsu_scmr = scmr;
    gsu_active = true;
}
extern "C" void sfc_audit_gsu_stop() {
    if (gsu_active && gsu_hook)
        gsu_hook(gsu_entry, gsu_clsr, gsu_cfgr, gsu_scmr, gsu_started, gsu_clocks, true);
    gsu_active = false;
}

namespace {
std::vector<std::uint8_t> bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + path);
    return {std::istreambuf_iterator<char>(file), {}};
}
struct AuditPlatform : ares::Platform {
    ares::VFS::Pak system_pak = std::make_shared<nall::vfs::directory>();
    ares::VFS::Pak cartridge_pak = std::make_shared<nall::vfs::directory>();
    std::mutex video_mutex;
    std::vector<std::uint32_t> pixels;
    unsigned width{}, height{}, pending_jump{};
    unsigned jump_boundary{};
    std::function<void()> before_jump;

    explicit AuditPlatform(const std::string& rom_path) {
        const std::string root = STARFOX_REFERENCE_ARES_ROOT;
        system_pak->append("ipl.rom", bytes(root + "/mia/Firmware/Super Famicom/ipl.rom"));
        system_pak->append("boards.bml", bytes(root + "/mia/Database/Super Famicom Boards.bml"));
        const auto rom = bytes(rom_path);
        if (rom.size() < 0x8000 || rom[0x7fd8] > 16)
            throw std::runtime_error("Invalid reference cartridge header");
        cartridge_pak->setAttribute("title", "Star Fox development reference");
        cartridge_pak->setAttribute("region", "NTSC");
        cartridge_pak->setAttribute("board", "GSU-RAM");
        // Use the shared NTSC oscillator, not the generic board's separate
        // 21.44 MHz crystal. This is the generic Ares GSU; optional register
        // overrides measure timing sensitivity, not a physical MC1 revision.
        const auto work = rom[0x7fbd] ? 1024U << (rom[0x7fbd] & 7) : 0x8000U;
        const auto save = rom[0x7fd8] ? 1024U << rom[0x7fd8] : 0U;
        cartridge_pak->append("program.rom", rom);
        cartridge_pak->append("save.ram", work + save);
    }
    auto pak(ares::Node::Object node) -> ares::VFS::Pak override {
        return node->name() == "Super Famicom" ? system_pak : cartridge_pak;
    }
    void input(ares::Node::Input::Input input) override {
        if (auto button = input->cast<ares::Node::Input::Button>()) button->setValue(false);
    }
    void audio(ares::Node::Audio::Stream stream) override {
        std::vector<double> samples(stream->channels());
        while (stream->pending()) stream->read(samples.data());
    }
    void log(ares::Node::Debugger::Tracer::Tracer, nall::string_view) override {
        if (!pending_jump || sfc::cpu.r.pc.d != jump_boundary
            || sfc::superfx.regs.sfr.g || sfc::superfx.regs.ramcl) return;
        // TRANSFER_L begins after the previous scene update has returned.
        // A random instruction boundary can still be inside RUNMARIO's wait
        // loop, with the GSU overwriting RAM that GAMESTART is initializing.
        if (before_jump) before_jump();
        auto& r = sfc::cpu.r;
        r.pc = pending_jump; r.p = 0x34; r.e = false; r.b = 0; r.d = 0; r.s = 0x2ff;
        r.a = r.x = r.y = 0; r.wai = r.stp = false;
        pending_jump = 0;
        before_jump = {};
        sfc::cpu.debugger.tracer.instruction->setEnabled(false);
    }
    void video(ares::Node::Video::Screen, const std::uint32_t* data,
        std::uint32_t pitch, std::uint32_t w, std::uint32_t h) override {
        std::lock_guard lock(video_mutex);
        width = w; height = h; pixels.resize(w * h);
        for (unsigned y = 0; y < h; ++y)
            std::copy_n(reinterpret_cast<const std::uint32_t*>(
                reinterpret_cast<const std::uint8_t*>(data) + y * pitch), w, pixels.data() + y * w);
    }
    void capture(const std::string& path) {
        std::lock_guard lock(video_mutex);
        if (pixels.empty()) throw std::runtime_error("No reference video");
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot create " + path);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        for (auto pixel : pixels) { file.put(pixel >> 16); file.put(pixel >> 8); file.put(pixel); }
    }
};
struct SystemGuard {
    ares::Node::System& system;
    ~SystemGuard() { close(); }
    void close() {
        cpu_hook = {}; gsu_hook = {}; gsu_write_hook = {};
        if (system) {
            // The reference's asynchronous colour conversion can still be
            // using PPU settings. Join it before the system unloads them.
            sfc::ppu.screen()->quit();
            system->unload(); system.reset();
        }
        ares::platform = nullptr;
    }
};
}

int main(int argc, char** argv) { try {
    if (argc < 6 || argc > 8) throw std::runtime_error(
        "Usage: full_reference ROM SYMBOLS MAP VIDEO_FRAMES OUTPUT_PREFIX [source|divided-standard|divided-fast [GAMEPLAY_UPDATES]]");
    const std::string policy = argc >= 7 ? argv[6] : "source";
    if (policy != "source" && policy != "divided-standard" && policy != "divided-fast")
        throw std::runtime_error("Unknown GSU register policy: " + policy);
    const auto frames = std::stoul(argv[4]);
    if (frames < 120 || frames > 100000) throw std::runtime_error("VIDEO_FRAMES must be 120..100000");
    const auto gameplay_updates = argc == 8 ? std::stoul(argv[7]) : 0U;
    if (argc == 8 && (gameplay_updates < 1U || gameplay_updates > 10000U))
        throw std::runtime_error("GAMEPLAY_UPDATES must be 1..10000");
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    const auto address = [&](const char* name) { return symbols.find(name).at(0); };
    const auto read = [](unsigned address, unsigned size = 2) {
        unsigned value = 0;
        for (unsigned i = 0; i < size; ++i) value |= unsigned(sfc::cpu.wram[(address + i) & 0x1ffff]) << (8 * i);
        return value;
    };
    const auto write = [](unsigned address, unsigned value, unsigned size = 2) {
        for (unsigned i = 0; i < size; ++i) sfc::cpu.wram[(address + i) & 0x1ffff] = value >> (8 * i);
    };
    starfox::simulation::Wdc65816 host_cpu(rom, &symbols);
    const std::string prefix = argv[5], map = argv[3];
    std::unique_ptr<GameplayAudit> gameplay;
    if (gameplay_updates) gameplay = std::make_unique<GameplayAudit>(rom,symbols,map,prefix,gameplay_updates);
    std::ofstream registers(prefix + "-registers.csv");
    std::ofstream entry(prefix + "-entry.csv");
    if (!registers || !entry) throw std::runtime_error("Cannot create register/entry traces");
    registers << "period,video_frame,cpu_pc,register,requested,effective\n";
    entry << "policy,map,video_frame,cpu_pc,gsu_running,pending_ram_clocks\n";
    unsigned register_video = 0;
    bool booting = true;
    AuditPlatform platform(argv[1]);
    ares::platform = &platform;
    ares::Node::System system;
    // Selecting a PPU implementation is required before loading the bus.
    sfc::option("Pixel Accuracy", "true");
    if (!sfc::load(system, "[Nintendo] Super Famicom (NTSC)"))
        throw std::runtime_error("System load failed");
    SystemGuard guard{system};
    gsu_write_hook = [&](unsigned location, unsigned requested) {
        if (location != 0x3037 && location != 0x3039) return requested;
        auto effective = requested;
        if (policy != "source") {
            if (location == 0x3039) effective = requested & ~1U;
            else if (policy == "divided-standard") effective = requested & ~0x20U;
            else effective = requested | 0x20U;
        }
        registers << (booting ? "boot" : "stage") << ',' << register_video << ','
            << unsigned(sfc::cpu.r.pc.d) << ',' << location << ',' << requested << ',' << effective << '\n';
        return effective;
    };
    sfc::cartridgeSlot.port->allocate(); sfc::cartridgeSlot.port->connect();
    sfc::controllerPort1.port->allocate("Gamepad"); sfc::controllerPort1.port->connect();
    system->power();
    for (register_video = 0; register_video < 600; ++register_video) system->run();
    booting = false;
    sfc::ppu.screen()->refresh();
    platform.capture(prefix + "-boot.ppm");
    std::cout << "Boot PC $" << std::hex << unsigned(sfc::cpu.r.pc.d) << std::dec << '\n';
    std::cout << "GSU register policy: " << policy << " (generic Ares GSU, shared NTSC oscillator)\n";
    if (map != "boot") {
        std::smatch match;
        if (!std::regex_match(map, match, std::regex("LEVEL([1-7])_([1-9][0-9]*)")))
            throw std::runtime_error("MAP must be boot or a numeric LEVEL route/stage symbol");
        const auto route = std::stoul(match[1].str()) - 1;
        const auto target = address(map.c_str()), stage = static_cast<unsigned>(std::stoul(match[2].str()) - 1);
        platform.before_jump = [&, target, route, stage] {
            entry << policy << ',' << map << ',' << register_video << ',' << unsigned(sfc::cpu.r.pc.d)
                << ',' << unsigned(sfc::superfx.regs.sfr.g) << ',' << unsigned(sfc::superfx.regs.ramcl) << '\n';
            write(address("MAPBANK"), target >> 16, 1);
            write(address("MAPPTR"), target & 0x7fff);
            write(address("WHICHROUTE"), route, 1);
            write(address("CURRENTLEVEL"), route, 1);
            if (!symbols.find("ACTUALROUTE").empty()) write(address("ACTUALROUTE"), route, 1);
            write(address("STAGE"), stage);
            std::cout << "Stage entry at video frame " << register_video << ", GSU idle\n";
        };
        platform.pending_jump = address("GAMESTART");
        platform.jump_boundary = address("TRANSFER_L");
        sfc::cpu.debugger.tracer.instruction->setEnabled(true);
    }
    std::ofstream complete(prefix + "-frames.csv"), camera(prefix + "-camera.csv"), launches(prefix + "-gsu.csv");
    if (!complete || !camera || !launches) throw std::runtime_error("Cannot create trace CSV files");
    complete << "video_frame,game_frame,master_clocks,active,view_z,map,player_x,player_y,player_z,framec,framer,draw\n";
    camera << "video_frame,game_frame,field,host,native\n";
    launches << "video_frame,game_frame,entry,clsr,cfgr,scmr,start_clocks,elapsed_clocks,stopped\n";
    unsigned video_index = 0, previous_clocks = 0, camera_calls = 0, camera_differences = 0;
    bool previous_frame = false, camera_pending = false;
    std::string camera_error;
    const auto get_view = address("GETVIEW_L"), do_sounds = address("DOSOUNDS_L"), transfer = address("TRANSFER_L");
    const auto game_frame = address("GAMEFRAME"), all_objects = address("ALLST"), map_pointer = address("MAPPTR");
    const auto player_pointer = address("PLAYPT"), world_x = address("AL_WORLDX"), world_y = address("AL_WORLDY"), world_z = address("AL_WORLDZ");
    const auto view_z = address("VIEWPOSZ"), frame_c = address("FRAMEC"), frame_r = address("FRAMER"), draw = address("M_NUMSHAPES") & 65535;
    const auto player_flags = address("PSHIPFLAGS3");
    unsigned settled_transfer = 0;
    for (unsigned offset = 0; gameplay && offset < 32; ++offset) {
        if (rom.read8(transfer + offset) == 0x9cU
            && rom.read16(transfer + offset + 1) == address("NOIRQBIT3")) {
            if (settled_transfer) throw std::runtime_error("Ambiguous transfer boundary");
            settled_transfer = transfer + offset;
        }
    }
    if (gameplay && !settled_transfer) throw std::runtime_error("Missing settled transfer boundary");
    unsigned draw_flags_clear = 0;
    for (unsigned offset = 0; gameplay && offset < 512; ++offset) {
        const auto pc = address("MARIOSHOWVIEW") + offset;
        // AND #~ASF_HITFLASH; STA AL_SFLAGS,Y. At the store instruction,
        // DL_SFLAGS has already captured the complete submitted flags.
        if (rom.read8(pc) == 0x29 && rom.read8(pc + 1) == 0xfd
            && rom.read8(pc + 2) == 0x99 && rom.read16(pc + 3) == address("AL_SFLAGS")) {
            if (draw_flags_clear) throw std::runtime_error("Ambiguous draw-flags clear boundary");
            draw_flags_clear = pc + 2;
        }
    }
    if (gameplay && !draw_flags_clear) throw std::runtime_error("Missing draw-flags clear boundary");
    std::vector<std::pair<std::string, unsigned>> camera_fields;
    for (const auto name : {"VIEWPOSX", "VIEWPOSY", "VIEWPOSZ", "VIEWROTXW", "VIEWROTYW", "VIEWROTZW", "ARSEBANDX", "ARSEBANDY",
        "WMAT11W", "WMAT12W", "WMAT13W", "WMAT21W", "WMAT22W", "WMAT23W", "WMAT31W", "WMAT32W", "WMAT33W"})
        camera_fields.emplace_back(name, address(name));
    cpu_hook = [&](unsigned pc, unsigned clocks) {
        if (platform.pending_jump) return;
        if (gameplay && pc == draw_flags_clear) gameplay->capture_submitted_flags();
        if (pc == get_view && camera_calls < 200 && camera_error.empty()) {
            for (unsigned i = 0; i < 0x20000; ++i) host_cpu.write8(0x7e0000 + i, sfc::cpu.wram[i]);
            for (unsigned i = 0; i < 0x10000; ++i) host_cpu.write8(0x700000 + i, sfc::superfx.ram.read(i));
            starfox::simulation::Wdc65816Registers registers;
            const auto& r = sfc::cpu.r;
            registers.a = r.a.w; registers.x = r.x.w; registers.y = r.y.w; registers.direct = r.d.w;
            registers.data_bank = r.b; registers.status = r.p; registers.stack = 0x2ff;
            // Exceptions cannot unwind across Ares's coroutine stacks.
            // Report a failed host call after returning to the video loop.
            try {
                host_cpu.call_long(get_view, registers);
                camera_pending = true;
            } catch (const std::exception& error) { camera_error = error.what(); }
        }
        if (pc == do_sounds && camera_pending) {
            camera_pending = false;
            ++camera_calls;
            for (const auto& [name, location] : camera_fields) {
                const auto actual = host_cpu.read16(location), expected = static_cast<std::uint16_t>(read(location));
                camera << video_index << ',' << read(game_frame) << ',' << name << ',' << static_cast<std::int16_t>(actual)
                    << ',' << static_cast<std::int16_t>(expected) << '\n';
                camera_differences += actual != expected;
            }
        }
        if (gameplay && pc == settled_transfer) gameplay->transfer();
        if (pc != transfer) return;
        unsigned active = 0, object = read(all_objects);
        while (object && active < 100) { ++active; object = read(object); }
        const auto player = read(player_pointer);
        complete << video_index << ',' << read(game_frame) << ',' << (previous_frame ? clocks - previous_clocks : 0) << ','
            << active << ',' << static_cast<std::int16_t>(read(view_z)) << ',' << read(map_pointer) << ','
            << static_cast<std::int16_t>(read(player + world_x)) << ',' << static_cast<std::int16_t>(read(player + world_y)) << ','
            << static_cast<std::int16_t>(read(player + world_z)) << ',' << read(frame_c, 1) << ',' << read(frame_r, 1) << ','
            << unsigned(sfc::superfx.ram.read(draw) | (sfc::superfx.ram.read(draw + 1) << 8)) << '\n';
        previous_clocks = clocks; previous_frame = true;
    };
    gsu_hook = [&](unsigned entry, unsigned clsr, unsigned cfgr, unsigned scmr,
        std::uint64_t start, std::uint64_t end, bool stopped) {
        launches << video_index << ',' << read(game_frame) << ',' << entry << ',' << clsr << ',' << cfgr << ',' << scmr
            << ',' << start << ',' << end - start << ',' << stopped << '\n';
    };
    for (unsigned frame = 0; frame < frames; ++frame) {
        video_index = frame;
        register_video = frame;
        // This source flag suppresses some damage, but does not establish a
        // death-free playthrough. Respawns must remain visible in the trace.
        write(player_flags, read(player_flags, 1) | 8, 1);
        system->run();
        if (!camera_error.empty()) break;
        if (gameplay && (!gameplay->error.empty() || gameplay->differences || gameplay->complete())) break;
    }
    sfc::ppu.screen()->quit();
    sfc::ppu.screen()->refresh();
    platform.capture(prefix + "-final.ppm");
    cpu_hook = {}; gsu_hook = {};
    std::cout << camera_calls << " complete camera calls, " << camera_differences << " field differences\n";
    guard.close();
    if (!camera_error.empty()) throw std::runtime_error(camera_error);
    if (gameplay) gameplay->finish();
    return camera_differences || camera_calls == 0 ? 1 : 0;
} catch (const std::exception& error) {
    cpu_hook = {}; gsu_hook = {};
    std::cerr << error.what() << '\n';
    return 1;
} }
