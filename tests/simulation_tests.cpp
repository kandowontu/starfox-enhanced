#include "starfox/simulation/math.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/path_vm.hpp"
#include "starfox/simulation/particle_system.hpp"
#include "starfox/simulation/prng.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/decrunch.hpp"
#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char** argv) {
    starfox::simulation::OriginalPrng random;
    constexpr std::array<std::uint8_t, 16> expected{
        0x82, 0x72, 0x66, 0xd3, 0xce, 0x92, 0xbd, 0x4c,
        0x93, 0xf3, 0xc0, 0x6d, 0xd2, 0x03, 0x90, 0x80};
    for (const auto value : expected) {
        require(random.next() == value, "original RNG sequence diverged");
    }

    require(starfox::simulation::add16(32'767, 1) == -32'768,
            "16-bit addition did not wrap");
    require(starfox::simulation::subtract16(-32'768, 1) == 32'767,
            "16-bit subtraction did not wrap");
    require(starfox::simulation::arithmetic_shift_right(-3, 1) == -2,
            "arithmetic right shift did not preserve 65816 sign behavior");
    require(starfox::simulation::multiply_q15(16'384, 16'384) == 8'192,
            "Q15 multiplication is wrong");

    std::array<std::uint16_t, 16> palette_words{};
    palette_words[1] = 0x001f;
    palette_words[2] = 0x03e0;
    palette_words[3] = 0x7c00;
    const auto palette = starfox::render::decode_bgr555_palette(palette_words);
    require(palette[1].r == 255 && palette[1].g == 0 && palette[1].b == 0
                && palette[2].g == 255 && palette[3].b == 255,
            "SNES BGR555 palette expansion is wrong");
    const auto half_palette = starfox::render::apply_snes_brightness(palette, 7);
    require(half_palette[1].r == 119 && half_palette[2].g == 119,
            "SNES master brightness scaling is wrong");
    starfox::render::Framebuffer meter_frame{224, 192};
    const starfox::render::SpriteRenderer sprite_renderer;
    sprite_renderer.draw_meters({0, 0, false, true, 10, 20}, meter_frame);
    require(meter_frame.get(200, 4) == 7U * 16U + 2U
                && meter_frame.get(198, 2) == 7U * 16U + 14U,
            "source boss meter geometry was not rendered");

    starfox::simulation::SnesPpuState priority_ppu;
    priority_ppu.main_screen = 0x12U;
    const auto bg2_map_byte = static_cast<std::size_t>(
        priority_ppu.bg2_screen_base) * 2U;
    priority_ppu.vram[bg2_map_byte] = 1U;
    priority_ppu.vram[bg2_map_byte + 1U] = 0x20U;
    const auto bg2_character_byte = static_cast<std::size_t>(
        priority_ppu.bg2_character_base) * 2U + 32U;
    priority_ppu.vram[bg2_character_byte] = 0x80U;
    starfox::render::Framebuffer priority_frame{8, 8};
    priority_frame.clear(42U);
    const starfox::render::BackgroundRenderer background_renderer;
    background_renderer.draw_bg2(priority_ppu, 0, 0, priority_frame,
        starfox::render::TilePriorityPass::low);
    require(priority_frame.get(0, 0) == 42U,
            "high-priority BG2 tile leaked into the low-priority pass");
    background_renderer.draw_bg2(priority_ppu, 0, 0, priority_frame,
        starfox::render::TilePriorityPass::high);
    require(priority_frame.get(0, 0) == 1U,
            "high-priority BG2 tile was not composited in its own pass");

    starfox::simulation::SnesPpuState tall_bg_ppu;
    tall_bg_ppu.main_screen = 0x02U;
    tall_bg_ppu.bg2_screen_size = 2U; // 32x64 tiles: page 1 is below page 0.
    const auto lower_page_byte = (static_cast<std::size_t>(
        tall_bg_ppu.bg2_screen_base) + 0x400U) * 2U;
    tall_bg_ppu.vram[lower_page_byte] = 1U;
    const auto tall_bg_character_byte = static_cast<std::size_t>(
        tall_bg_ppu.bg2_character_base) * 2U + 32U;
    tall_bg_ppu.vram[tall_bg_character_byte] = 0x80U;
    starfox::render::Framebuffer tall_bg_frame{1, 1};
    tall_bg_frame.clear(42U);
    background_renderer.draw_bg2(tall_bg_ppu, 0, 256, tall_bg_frame);
    require(tall_bg_frame.get(0, 0) == 1U,
            "32x64 BG tilemap lower page used the 64x64 page stride");

    starfox::simulation::SnesPpuState mode3_ppu;
    mode3_ppu.background_mode = 3U;
    mode3_ppu.main_screen = 0x01U;
    mode3_ppu.bg1_character_base = 0x1000U;
    mode3_ppu.bg1_screen_base = 0x3000U;
    const auto mode3_map_byte = static_cast<std::size_t>(
        mode3_ppu.bg1_screen_base) * 2U;
    mode3_ppu.vram[mode3_map_byte] = 1U;
    const auto mode3_character_byte = static_cast<std::size_t>(
        mode3_ppu.bg1_character_base) * 2U + 64U;
    mode3_ppu.vram[mode3_character_byte] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 17U] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 32U] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 49U] = 0x80U;
    starfox::render::Framebuffer mode3_frame{8, 8};
    background_renderer.draw_bg1(mode3_ppu, mode3_frame);
    require(mode3_frame.get(0, 0) == 0x99U,
            "Mode 3 BG1 did not decode all eight SNES bitplanes");

    for (std::size_t object = 0; object < 128U; ++object) {
        priority_ppu.oam[object * 4U + 1U] = 224U;
    }
    priority_ppu.oam[1U] = 0U;
    priority_ppu.oam[2U] = 1U;
    priority_ppu.oam[3U] = 0x20U;
    priority_ppu.vram[0xc000U + 32U] = 0x80U;
    priority_frame.clear(42U);
    sprite_renderer.draw_objects(priority_ppu, priority_frame, 1U);
    require(priority_frame.get(0, 0) == 42U,
            "OBJ priority filter rendered a sprite in the wrong pass");
    sprite_renderer.draw_objects(priority_ppu, priority_frame, 2U);
    require(priority_frame.get(0, 0) == 129U,
            "OBJ priority filter omitted the selected source sprite");

    starfox::simulation::ObjectPool objects;
    const auto first = objects.allocate_after();
    const auto second = objects.allocate_after(first);
    const auto head = objects.allocate_after();
    require((objects.active_handles() == std::vector<starfox::simulation::ObjectHandle>{head, first, second}),
            "object insertion order differs from l_add");
    objects.at(second).attached = first;
    require(objects.remove(first), "active object could not be removed");
    require(objects.at(second).attached == 0, "object references were not divorced on removal");
    const auto reused = objects.allocate_after(head);
    require(reused == first, "freed object was not reused from the LIFO free-list head");
    require(objects.active_count() == 3, "object count is wrong after reuse");
    objects.write_path_word(second, 0x80, 0x1234);
    require(objects.read_path_word(second, 0x80) == 0x1234,
            "extended alien-block addressing is wrong");

    std::vector<std::uint8_t> rom_bytes(0x20000);
    const auto to_offset = [](std::uint32_t address) {
        return static_cast<std::size_t>((address >> 16U) & 0x7fU) * 0x8000U
            + static_cast<std::size_t>(address & 0x7fffU);
    };
    const auto put8 = [&](std::uint32_t address, std::uint8_t value) {
        rom_bytes[to_offset(address)] = value;
    };
    const auto put16 = [&](std::uint32_t address, std::uint16_t value) {
        put8(address, static_cast<std::uint8_t>(value));
        put8(address + 1U, static_cast<std::uint8_t>(value >> 8U));
    };
    put16(0x018000 + 3U * 2U, 0x9000); // shape table entry 3
    put16(0x018100 + 4U * 4U, 0x9111); // strategy table entry 4
    put8(0x018100 + 4U * 4U + 2U, 0x02);
    put8(0x018100 + 4U * 4U + 3U, 3);
    auto map = std::uint32_t{0x018200};
    put8(map++, 0); put16(map, 0); map += 2;
    put16(map, 100); map += 2; put16(map, static_cast<std::uint16_t>(-20)); map += 2;
    put16(map, 300); map += 2; put8(map++, 3); put8(map++, 4);
    put8(map++, 50); put8(map++, 64);
    put8(map++, 18); put16(map, 10); map += 2;
    put8(map++, 0); put16(map, 5); map += 2;
    put16(map, 200); map += 2; put16(map, 0); map += 2; put16(map, 400); map += 2;
    put8(map++, 3); put8(map++, 4); put8(map++, 2);

    auto inline_map = std::uint32_t{0x018300};
    put8(inline_map++, 0); put16(inline_map, 0); inline_map += 2;
    put16(inline_map, 100); inline_map += 2; put16(inline_map, 200); inline_map += 2;
    put16(inline_map, 300); inline_map += 2; put8(inline_map++, 3); put8(inline_map++, 4);
    put8(inline_map++, 120);
    put8(inline_map++, 0xa9); put8(inline_map++, 0x7f); // LDA #$7f (A8)
    put8(inline_map++, 0x9d); put16(inline_map, 0x000e); inline_map += 2; // STA al_worldy,x
    put8(inline_map++, 0xc2); put8(inline_map++, 0x20); // REP #$20 (A16)
    put8(inline_map++, 0xac); put16(inline_map, 0x12af); inline_map += 2; // LDY alfreelst
    put8(inline_map++, 0xb9); put16(inline_map, 0x0000); inline_map += 2; // LDA _next,y
    put8(inline_map++, 0x8d); put16(inline_map, 0x12af); inline_map += 2; // STA alfreelst
    put8(inline_map++, 0xbd); put16(inline_map, 0x0000); inline_map += 2; // LDA _next,x
    put8(inline_map++, 0x99); put16(inline_map, 0x0000); inline_map += 2; // STA _next,y
    put8(inline_map++, 0x8a); // TXA
    put8(inline_map++, 0x99); put16(inline_map, 0x0002); inline_map += 2; // STA _prev,y
    put8(inline_map++, 0x98); // TYA
    put8(inline_map++, 0x9d); put16(inline_map, 0x0000); inline_map += 2; // STA _next,x
    put8(inline_map++, 0xa9); put16(inline_map, 0x7777); inline_map += 2;
    put8(inline_map++, 0x99); put16(inline_map, 0x000c); inline_map += 2; // STA al_worldx,y
    put8(inline_map++, 0xa2);
    const auto inline_return_operand = inline_map;
    put16(inline_map, 0); inline_map += 2;
    put8(inline_map++, 0x6b); // RTL
    put16(inline_return_operand, static_cast<std::uint16_t>(inline_map & 0x7fffU));
    put8(inline_map++, 122);
    put16(inline_map, 0x81ff); inline_map += 2; put8(inline_map++, 0x02); // JSL $028200
    put8(inline_map++, 2);

    put8(0x028200, 0xa9); put8(0x028201, 42); // LDA #42 (A8)
    put8(0x028202, 0x9d); put16(0x028203, 0x000c); // STA al_worldx,x
    put8(0x028205, 0x6b); // RTL

    const starfox::assets::RomImage map_rom{rom_bytes};
    starfox::simulation::ObjectPool map_objects;
    const auto player = map_objects.allocate_after();
    starfox::simulation::MapVm map_vm{
        map_rom, starfox::simulation::MapDatabase{map_rom, 0x018000, 0x018100}, map_objects};
    map_vm.start(0x018200, player);
    map_vm.advance_distance(1);
    require(map_objects.active_count() == 2, "zero-distance map object did not spawn");
    require(map_objects.at(map_vm.last_spawned()).shape == 0x9000,
            "map shape lookup is wrong");
    require(map_objects.at(map_vm.last_spawned()).rotation_y == 64,
            "map rotation control did not modify the last object");
    require(map_vm.countdown() == 10, "map wait did not stop bytecode execution");
    map_vm.advance_distance(11);
    require(map_objects.active_count() == 3, "timed map object did not spawn");
    require(map_vm.countdown() == 5, "spawn distance was not loaded exactly");

    starfox::simulation::ObjectPool inline_objects;
    const auto inline_player = inline_objects.allocate_after();
    starfox::simulation::MapVm inline_vm{
        map_rom, starfox::simulation::MapDatabase{map_rom, 0x018000, 0x018100}, inline_objects};
    inline_vm.start(0x018300, inline_player);
    inline_vm.advance_distance(1);
    require(inline_vm.ended(), "inline 65C816 map stream did not return to bytecode");
    require(inline_objects.at(inline_vm.last_spawned()).world_y == 0x007f,
            "inline 65C816 code did not receive the original object pointer in X");
    require(inline_objects.at(inline_vm.last_spawned()).world_x == 42,
            "mapcode JSL did not synchronize object memory through WRAM");
    require(inline_objects.active_count() == 3 && inline_objects.at(3).world_x == 0x7777,
            "native 65C816 allocation did not synchronize the active/free lists");
    require(inline_vm.unsupported_controls().empty(),
            "native map code was still treated as a skipped boundary");

    auto path_address = std::uint32_t{0x018400};
    put8(path_address++, 17); put16(path_address, 100); path_address += 2; put8(path_address++, 12);
    put8(path_address++, 138); put8(path_address++, 3);
    put8(path_address++, 162); put8(path_address++, 2);
    put8(path_address++, 87);
    put8(path_address++, 166);
    put8(0x018400 + 20U, 115); // P_PARTICLES
    put8(0x018400 + 21U, 166); // P_WAIT1
    const starfox::assets::RomImage path_rom{rom_bytes};
    starfox::simulation::ObjectPool path_objects;
    const auto path_player = path_objects.allocate_after();
    const auto path_actor = path_objects.allocate_after(path_player);
    starfox::simulation::OriginalPrng path_random;
    starfox::simulation::PathVm path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random};
    path_vm.set_player(path_player);
    path_vm.attach(path_actor, 0);
    path_vm.tick(path_actor);
    require(path_objects.at(path_actor).world_x == 102,
            "PATH DO/NEXT first iteration is wrong");
    path_vm.tick(path_actor);
    path_vm.tick(path_actor);
    require(path_objects.at(path_actor).world_x == 106,
            "PATH DO/NEXT loop count is wrong");
    require(path_vm.path_offset(path_actor) == 10,
            "PATH wait1 did not yield at the correct byte boundary");

    const auto particle_source = path_objects.allocate_after(path_actor);
    path_objects.at(particle_source).world_x = 111;
    path_objects.at(particle_source).world_y = -222;
    path_objects.at(particle_source).world_z = 333;
    starfox::simulation::PathVm particle_path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random, 0x06badfU, 0x9500U};
    particle_path_vm.set_player(path_player);
    particle_path_vm.attach(particle_source, 20U);
    particle_path_vm.tick(particle_source);
    const auto particle_object = path_objects.next_active(particle_source);
    require(particle_object != 0U
                && path_objects.at(particle_object).shape == 0x9500U
                && path_objects.at(particle_object).strategy_address == 0x06badfU,
            "P_PARTICLES did not create its source strategy object");
    require(path_objects.at(particle_object).world_x == 111
                && path_objects.at(particle_object).world_y == -222
                && path_objects.at(particle_object).world_z == 333,
            "P_PARTICLES did not copy the source position");

    starfox::simulation::ObjectPool particle_objects;
    const auto particle_owner = particle_objects.allocate_after();
    auto& particle_emitter = particle_objects.at(particle_owner);
    particle_emitter.strategy_flags[0] = 0x10U;
    particle_emitter.scratch_bytes[0] = 1;
    particle_emitter.scratch_bytes[1] = 10;
    particle_emitter.scratch_bytes[2] = 4;
    starfox::simulation::ParticleSystem particle_system{
        path_rom, 0x018600U, 0x018700U};
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 1U,
            "source particle pool did not allocate an emitter particle");
    const auto generated_particle = std::find_if(
        particle_system.particles().begin(), particle_system.particles().end(),
        [](const auto& particle) { return particle.life != 0U; });
    require(generated_particle != particle_system.particles().end()
                && generated_particle->owner == particle_owner
                && generated_particle->life == 3U,
            "source particle lifetime/update order is wrong");
    const auto first_particle_velocity = std::array{
        generated_particle->velocity_x,
        generated_particle->velocity_y,
        generated_particle->velocity_z};
    particle_system.reset();
    require(particle_system.active_count() == 0U,
            "particle reset did not clear the source Super FX pool");
    particle_system.tick(particle_objects, true);
    const auto reset_particle = std::find_if(
        particle_system.particles().begin(), particle_system.particles().end(),
        [](const auto& particle) { return particle.life != 0U; });
    require(reset_particle != particle_system.particles().end()
                && std::array{reset_particle->velocity_x,
                       reset_particle->velocity_y,
                       reset_particle->velocity_z} == first_particle_velocity,
            "particle reset did not restore the INITGAME3D random seed");
    particle_emitter.scratch_bytes[2] = 0;
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 1U,
            "particle owner did not keep its existing pool entries alive");
    require(particle_objects.remove(particle_owner),
            "particle test owner could not be removed");
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 0U,
            "orphaned source particles were not retired");

    std::vector<std::uint8_t> cpu_rom_bytes(0x8000U);
    cpu_rom_bytes[0] = 0xa9; cpu_rom_bytes[1] = 0x34; cpu_rom_bytes[2] = 0x12; // LDA #$1234
    cpu_rom_bytes[3] = 0x69; cpu_rom_bytes[4] = 0x01; cpu_rom_bytes[5] = 0x00; // ADC #1
    cpu_rom_bytes[6] = 0x6b; // RTL
    cpu_rom_bytes[0x10] = 0xa9; cpu_rom_bytes[0x11] = 0x78;
    cpu_rom_bytes[0x12] = 0x56; // LDA #$5678
    cpu_rom_bytes[0x13] = 0x60; // RTS
    const starfox::assets::RomImage cpu_rom{std::move(cpu_rom_bytes)};
    starfox::simulation::Wdc65816 cpu{cpu_rom};
    cpu.write8(0x004218, 0x5a);
    require(cpu.read8(0x004218) == 0x5a,
            "native 65C816 I/O mirror did not preserve controller state");
    cpu.write8(0x002142, 0xa5);
    require((cpu.take_apu_port_writes()
                == std::vector<starfox::simulation::ApuPortWrite>{{2, 0xa5}}),
            "native 65C816 APU command writes were not captured");
    cpu.write8(0x004202, 0x7f);
    cpu.write8(0x004203, 0x81);
    require(cpu.read16(0x004216) == 0x3fff,
            "SNES 8-bit hardware multiplication is wrong");
    cpu.write16(0x004204, 50'000U);
    cpu.write8(0x004206, 37U);
    require(cpu.read16(0x004214) == 1'351U
                && cpu.read16(0x004216) == 13U,
            "SNES 16-by-8 hardware division is wrong");
    cpu.write8(0x004206, 0U);
    require(cpu.read16(0x004214) == 0xffffU
                && cpu.read16(0x004216) == 50'000U,
            "SNES divide-by-zero register results are wrong");
    cpu.write8(0x002181, 0xfeU);
    cpu.write8(0x002182, 0xffU);
    cpu.write8(0x002183, 1U);
    cpu.write8(0x002180, 0x31U);
    cpu.write8(0x002180, 0x42U);
    require(cpu.read8(0x7ffffeU) == 0x31U && cpu.read8(0x7fffffU) == 0x42U,
            "SNES WRAM data port did not write and increment across its boundary");
    starfox::simulation::Wdc65816Registers cpu_registers;
    const auto cpu_instructions = cpu.call_long(0x008000, cpu_registers);
    require(cpu_registers.a == 0x1235 && cpu_instructions == 3,
            "native 65C816 bridge did not execute a 16-bit RTL routine exactly");
    starfox::simulation::Wdc65816Registers near_registers;
    near_registers.status = 0x04U;
    const auto near_instructions = cpu.call_near(0x008010U, near_registers);
    require(near_registers.a == 0x5678U && near_instructions == 2U,
            "native 65C816 bridge did not execute a same-bank RTS routine exactly");

    if (argc == 3) {
        const auto upstream_rom = starfox::assets::RomImage::load(argv[1]);
        const auto upstream_symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto trig = starfox::simulation::TrigTables::load(
            upstream_rom, upstream_symbols);
        require(trig.sin8(0) == 0 && trig.sin8(64) == 127
                    && trig.sin8(128) == 0 && trig.sin8(192) == -127,
                "ROM 8-bit sine table quadrants are wrong");
        require(trig.cos8(0) == 127 && trig.cos8(64) == 0
                    && trig.cos8(128) == -127 && trig.cos8(192) == 0,
                "ROM 8-bit cosine table quadrants are wrong");
        require(trig.sin_q15(0x4000) == 32'767 && trig.cos_q15(0) == 32'767,
                "ROM Q15 trigonometry is wrong");

        const auto stp_char_end = upstream_symbols.find("BGSTPCCR");
        const auto stp_screen_end = upstream_symbols.find("BGSTPPCR");
        require(!stp_char_end.empty() && !stp_screen_end.empty(),
                "Corneria background archive symbols are missing");
        const auto stp_chars = starfox::assets::decrunch_reverse(
            upstream_rom, stp_char_end.front());
        const auto stp_screen = starfox::assets::decrunch_reverse(
            upstream_rom, stp_screen_end.front());
        require(stp_chars.bytes.size() == 5632U
                    && std::equal(stp_chars.bytes.begin(),
                        stp_chars.bytes.begin() + 16,
                        std::array<std::uint8_t, 16>{
                            0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00,
                            0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}.begin()),
                "MDECRU-compatible Corneria character decode diverged");
        require(stp_screen.bytes.size() == 8192U
                    && std::all_of(stp_screen.bytes.begin(),
                        stp_screen.bytes.begin() + 16,
                        [index = std::size_t{0}](std::uint8_t value) mutable {
                            return value == ((index++ & 1U) == 0U ? 0x6aU : 0x14U);
                        }),
                "MDECRU-compatible Corneria tilemap decode diverged");

        const auto map_addresses = upstream_symbols.find("MAP1_1A");
        require(!map_addresses.empty(), "MAP1_1A symbol is missing");
        starfox::simulation::ObjectPool upstream_objects;
        const auto upstream_player = upstream_objects.allocate_after();
        starfox::simulation::MapVm upstream_map{
            upstream_rom,
            starfox::simulation::MapDatabase{upstream_rom, upstream_symbols},
            upstream_objects};
        upstream_map.start(map_addresses.front(), upstream_player);
        upstream_map.advance_distance(1);
        require(upstream_objects.active_count() > 1,
                "real Corneria map did not create its initial objects");
        require(upstream_map.countdown() > 0,
                "real Corneria map did not reach its first distance wait");
        require(upstream_map.unsupported_controls().empty(),
                "real Corneria map initialization used a boundary-only map control");
        starfox::simulation::NativeStrategyScheduler upstream_strategies{
            upstream_symbols, upstream_objects, upstream_map};
        const auto strategy_stats = upstream_strategies.tick_all();
        require(strategy_stats.objects_run > 1 && strategy_stats.instructions > 0,
                "real Corneria native strategies did not execute");

        const auto map1_1b = upstream_symbols.find("MAP1_1B");
        const auto boss_ptr = upstream_symbols.find("BOSS_PTR");
        require(!map1_1b.empty() && !boss_ptr.empty(),
                "boss-map CPU integration symbols are missing");
        starfox::simulation::ObjectPool boss_objects;
        const auto boss_player = boss_objects.allocate_after();
        starfox::simulation::MapVm boss_map{
            upstream_rom,
            starfox::simulation::MapDatabase{upstream_rom, upstream_symbols},
            boss_objects};
        boss_map.set_unknown_condition_result(true);
        boss_map.start(map1_1b.front(), boss_player);
        for (std::size_t waits = 0; !boss_map.ended() && waits < 100; ++waits) {
            boss_map.advance_distance(static_cast<std::int16_t>(
                std::max<int>(1, boss_map.countdown() + 1)));
        }
        require(boss_map.ended() && boss_map.read_native_byte(boss_ptr.front()) == 2,
                "real markboss inline code did not update original WRAM state");
        require(boss_map.unsupported_controls().empty(),
                "real boss map still skipped native map controls");

        const auto walk_paths = upstream_symbols.find("PATH_E_WALK_1");
        require(!walk_paths.empty(), "PATH_E_WALK_1 symbol is missing");
        starfox::simulation::OriginalPrng upstream_random;
        starfox::simulation::PathVm upstream_paths{
            upstream_rom, upstream_symbols, upstream_objects, upstream_random};
        upstream_paths.set_player(upstream_player);
        const auto walker = upstream_objects.allocate_after(upstream_player);
        upstream_paths.attach(walker, static_cast<std::uint16_t>(walk_paths.front()));
        upstream_paths.tick(walker);
        require(upstream_objects.at(walker).rotation_y > 64,
                "real walking-enemy PATH did not enter its turn chase");
        require(!upstream_paths.events().empty(),
                "real walking-enemy PATH did not emit its positional sound");

        starfox::simulation::GameSimulation game{upstream_rom, upstream_symbols, "LEVEL1_1"};
        const auto palette_address = upstream_symbols.find("PALADDR");
        const auto controller_high_address = upstream_symbols.find("CONT0");
        const auto controller_low_address = upstream_symbols.find("CONTL0");
        require(!palette_address.empty() && !controller_high_address.empty()
                    && !controller_low_address.empty(),
                "runtime palette/input symbols are missing");
        const auto game_palette = game.palette_words();
        require(game_palette[1] == upstream_rom.read16(palette_address.front() + 2U),
                "game initialization did not load the original 3D palette");
        const auto dust_addresses = upstream_symbols.find("M_DUSTPNTS");
        require(!dust_addresses.empty(), "Super FX dust point symbol is missing");
        for (std::size_t point = 0; point < 8U; ++point) {
            const auto& native_point = game.dust().points()[point];
            require(game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U))
                        == std::bit_cast<std::uint16_t>(native_point.x)
                    && game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U + 2U))
                        == std::bit_cast<std::uint16_t>(native_point.y)
                    && game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U + 4U))
                        == std::bit_cast<std::uint16_t>(native_point.z),
                    "native dust initialization diverged from MINITDUST");
        }
        const auto x1_addresses = upstream_symbols.find("X1");
        const auto y1_addresses = upstream_symbols.find("Y1");
        const auto arctangent_routines = upstream_symbols.find("ARCTAN16_L");
        require(!x1_addresses.empty() && !y1_addresses.empty()
                    && !arctangent_routines.empty(),
                "native arctangent bridge symbols are missing");
        const auto x1_address = *std::find_if(x1_addresses.begin(), x1_addresses.end(),
            [](std::uint32_t address) { return (address >> 16U) == 0U; });
        const auto y1_address = *std::find_if(y1_addresses.begin(), y1_addresses.end(),
            [](std::uint32_t address) { return (address >> 16U) == 0U; });
        const auto arctangent = *std::find_if(arctangent_routines.begin(),
            arctangent_routines.end(), [](std::uint32_t address) {
                return (address & 0xffffU) >= 0x8000U;
            });
        const auto check_angle = [&](std::int16_t x, std::int16_t y,
                                     std::uint16_t expected_angle) {
            game.map().write_native_word(x1_address,
                std::bit_cast<std::uint16_t>(x));
            game.map().write_native_word(y1_address,
                std::bit_cast<std::uint16_t>(y));
            starfox::simulation::Wdc65816Registers registers;
            registers.status = 0x24U;
            game.map().call_native_routine(arctangent, registers);
            return registers.a == expected_angle;
        };
        require(check_angle(0, 1, 0x0000U)
                    && check_angle(1, 0, 0x4000U)
                    && check_angle(1, 1, 0x2000U)
                    && check_angle(-1, 1, 0xe000U)
                    && check_angle(0, -1, 0x8000U),
                "Super FX arctangent bridge produced wrong quadrant angles");
        const auto matxw = upstream_symbols.find("MATXW");
        const auto matyw = upstream_symbols.find("MATYW");
        const auto matzw = upstream_symbols.find("MATZW");
        const auto mat11w = upstream_symbols.find("MAT11W");
        const auto crotmat16 = upstream_symbols.find("CROTMAT16_L");
        require(!matxw.empty() && !matyw.empty() && !matzw.empty()
                    && !mat11w.empty() && !crotmat16.empty(),
                "native world-matrix bridge symbols are missing");
        constexpr std::array<std::uint16_t, 3> matrix_angles{
            0x1234U, 0x4567U, 0x89abU};
        game.map().write_native_word(matxw.front(), matrix_angles[0]);
        game.map().write_native_word(matyw.front(), matrix_angles[1]);
        game.map().write_native_word(matzw.front(), matrix_angles[2]);
        starfox::simulation::Wdc65816Registers matrix_registers;
        matrix_registers.status = 0x24U;
        game.map().call_native_routine(crotmat16.front(), matrix_registers);
        const auto expected_matrix = starfox::simulation::rotation_matrix_q15(
            trig, std::bit_cast<std::int16_t>(matrix_angles[0]),
            std::bit_cast<std::int16_t>(matrix_angles[1]),
            std::bit_cast<std::int16_t>(matrix_angles[2]));
        for (std::size_t index = 0; index < expected_matrix.size(); ++index) {
            require(game.map().read_native_word(mat11w.front()
                        + static_cast<std::uint32_t>(index * 2U))
                        == std::bit_cast<std::uint16_t>(expected_matrix[index]),
                    "Super FX world-matrix bridge diverged from source Q15 math");
        }
        const auto m_vanishx = upstream_symbols.find("M_VANISHX");
        const auto m_vanishy = upstream_symbols.find("M_VANISHY");
        const auto m_xright = upstream_symbols.find("M_XRIGHT");
        const auto m_ybot = upstream_symbols.find("M_YBOT");
        require(!m_vanishx.empty() && !m_vanishy.empty()
                    && !m_xright.empty() && !m_ybot.empty()
                    && game.map().read_native_word(m_vanishx.front()) == 112U
                    && game.map().read_native_word(m_vanishy.front()) == 96U
                    && game.map().read_native_word(m_xright.front()) == 223U
                    && game.map().read_native_word(m_ybot.front()) == 191U,
                "original game viewport was not mirrored into Super FX state");
        constexpr auto test_input = static_cast<starfox::input::ButtonMask>(
            starfox::input::left | starfox::input::a);
        starfox::audio::Spc700Audio audio;
        auto first_tick = game.tick({test_input, test_input, 0});
        const auto strategy_frame_rate = upstream_symbols.find("FRAMERATE");
        require(!strategy_frame_rate.empty()
                    && game.map().read_native_byte(strategy_frame_rate.front()) == 3U,
                "20 Hz strategy timing did not retain three NTSC video phases");
        auto pcm = audio.render_logic_tick(first_tick.audio_port_writes);
        auto heard_audio = std::any_of(pcm.begin(), pcm.end(),
            [](std::int16_t sample) { return sample != 0; });
        require(game.map().read_native_byte(controller_high_address.front())
                        == static_cast<std::uint8_t>(test_input >> 8U)
                    && game.map().read_native_byte(controller_low_address.front())
                        == static_cast<std::uint8_t>(test_input),
                "latched native input did not reach original controller WRAM");
        std::size_t boot_audio_writes = 0;
        for (std::size_t tick = 1; tick < 6; ++tick) {
            const auto tick_result = game.tick({});
            boot_audio_writes += tick_result.audio_port_writes.size();
            pcm = audio.render_logic_tick(tick_result.audio_port_writes);
            heard_audio = heard_audio || std::any_of(pcm.begin(), pcm.end(),
                [](std::int16_t sample) { return sample != 0; });
        }
        const auto current_background = upstream_symbols.find("CURRENTBG");
        require(!current_background.empty()
                    && game.map().read_native_word(current_background.front()) == 3,
                "transfer bridge did not run Corneria's original background request");
        require(game.map().display_brightness() == 15,
                "player-opening strategy did not drive the original quick fade-up");
        require((game.objects().at(game.player()).strategy_address >> 16U) == 0x0bU,
                "background info request did not install playeropening_Istrat");
        require(boot_audio_writes != 0,
                "Corneria background initialization did not execute the SPC upload protocol");
        require(audio.driver_loaded() && audio.uploaded_bytes() > 4'096U,
                "Corneria SPC700 driver/sample bank was not reconstructed from the upload");
        const auto& ppu = game.map().ppu_state();
        require(std::count_if(ppu.vram.begin(), ppu.vram.end(),
                    [](std::uint8_t value) { return value != 0U; }) > 2'000,
                "original background/OBJ DMA did not populate emulated VRAM");
        require(ppu.cgram[7U * 16U + 1U] == game.palette_words()[1],
                "3D game palette was not synchronized into CGRAM line 7");
        // Corneria deliberately suppresses the HUD during the opening fly-in.
        // Advance to the first stable gameplay presentation before checking the
        // original DO_SPRITES_L output rather than treating that suppression as
        // a failed OAM transfer.
        for (std::size_t tick = 6; tick < 300; ++tick) {
            const auto tick_result = game.tick({});
            pcm = audio.render_logic_tick(tick_result.audio_port_writes);
            heard_audio = heard_audio || std::any_of(pcm.begin(), pcm.end(),
                [](std::int16_t sample) { return sample != 0; });
        }
        if (!heard_audio) {
            const auto state = audio.state();
            std::cerr << "SPC state: pc=$" << std::hex << state.program_counter
                      << ", a=$" << static_cast<unsigned>(state.accumulator)
                      << ", x=$" << static_cast<unsigned>(state.x)
                      << ", y=$" << static_cast<unsigned>(state.y)
                      << ", psw=$" << static_cast<unsigned>(state.status)
                      << ", sp=$" << static_cast<unsigned>(state.stack)
                      << ", flg=$" << static_cast<unsigned>(state.dsp_flags)
                      << ", kon=$" << static_cast<unsigned>(state.dsp_key_on)
                      << std::dec << ", mvol=("
                      << static_cast<int>(state.main_volume_left) << ','
                      << static_cast<int>(state.main_volume_right) << ")\n";
        }
        require(heard_audio,
                "original SPC700 driver produced only silence during Corneria");
        require(std::any_of(ppu.oam.begin(), ppu.oam.begin() + 328U,
                    [](std::uint8_t value) { return value != 0U; }),
                "original HUD builder did not reach emulated OAM");

        const auto game_frame_address = upstream_symbols.find("GAMEFRAME");
        require(!game_frame_address.empty(), "GAMEFRAME symbol is missing");
        static_cast<void>(game.tick({starfox::input::start,
            starfox::input::start, 0}));
        require(game.paused(), "eligible gameplay START did not pause the port");
        const auto paused_frame = game.map().read_native_word(
            game_frame_address.front());
        const auto paused_tick = game.tick({});
        require(game.map().read_native_word(game_frame_address.front()) == paused_frame
                    && paused_tick.strategies.objects_run == 0U,
                "paused gameplay advanced original strategy state");
        static_cast<void>(game.tick({starfox::input::start,
            starfox::input::start, 0}));
        require(!game.paused(), "second START edge did not resume gameplay");

        const auto level_finished = upstream_symbols.find("LEVELFINISHED");
        const auto stage_address = upstream_symbols.find("STAGE");
        const auto new_map_address = upstream_symbols.find("NEWMAP");
        const auto level1_2 = upstream_symbols.find("LEVEL1_2");
        require(!level_finished.empty() && !stage_address.empty()
                    && !new_map_address.empty() && !level1_2.empty(),
                "route-transition symbols are missing");
        game.map().write_native_word(level_finished.front(), 1U);
        static_cast<void>(game.tick({}));
        const auto selected_map = static_cast<std::uint32_t>(
            game.map().read_native_byte(new_map_address.front()))
            | (static_cast<std::uint32_t>(
                   game.map().read_native_byte(new_map_address.front() + 1U)) << 8U)
            | (static_cast<std::uint32_t>(
                   game.map().read_native_byte(new_map_address.front() + 2U)) << 16U);
        require(game.flow_state() == starfox::simulation::GameFlowState::planet_travel
                    && game.map().read_native_word(stage_address.front()) == 1U
                    && selected_map == level1_2.front(),
                "completed Corneria did not enter the original route travel screen");
        require(game.map().ppu_state().background_mode == 3U
                    && (game.map().ppu_state().main_screen & 0x03U) == 0x03U
                    && game.map().unknown_superfx_launches().empty(),
                "route travel screen did not execute its original Mode 3 assets");
        for (std::size_t frame = 0;
             frame < 360U
                 && game.flow_state()
                     == starfox::simulation::GameFlowState::planet_travel;
             ++frame) {
            game.present_frame();
            if ((frame % 3U) == 2U) static_cast<void>(game.tick({}));
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::gameplay,
                "route ship travel did not launch the selected next map");
        require(!game.map().ended() && game.objects().is_active(game.player()),
                "next-stage INITGAME_L did not rebuild a playable object/map state");

        const auto game_shape = upstream_symbols.find("GAMESH");
        const auto over_shape = upstream_symbols.find("OVERSH");
        require(!game_shape.empty() && !over_shape.empty(),
                "game-over model symbols are missing");
        game.map().write_native_word(level_finished.front(), 10U);
        static_cast<void>(game.tick({}));
        const auto has_shape = [&game](std::uint16_t shape) {
            for (const auto handle : game.objects().active_handles()) {
                if (game.objects().at(handle).shape == shape) return true;
            }
            return false;
        };
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over
                    && has_shape(static_cast<std::uint16_t>(game_shape.front()))
                    && has_shape(static_cast<std::uint16_t>(over_shape.front())),
                "game-over exit did not initialize the original GAME/OVER scene");
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over,
                "game-over START lock accepted input before 50 source frames");
        for (std::size_t tick = 2; tick < 50; ++tick) {
            static_cast<void>(game.tick({}));
        }
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        const auto my_demo = upstream_symbols.find("MY_DEMO").front();
        const auto foxy_option = upstream_symbols.find("FOXY_OPTION").front();
        require(game.flow_state()
                        == starfox::simulation::GameFlowState::continue_choice
                    && game.map().ppu_state().background_mode == 1U
                    && has_shape(static_cast<std::uint16_t>(my_demo))
                    && std::any_of(game.map().ppu_state().vram.begin(),
                        game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; }),
                "game-over lock did not open the original Fox continue screen");
        static_cast<void>(game.tick({0, starfox::input::down, 0}));
        require(game.map().read_native_byte(foxy_option) == 0xffU,
                "continue screen DOWN did not select NO");
        static_cast<void>(game.tick({0, starfox::input::up, 0}));
        require(game.map().read_native_byte(foxy_option) == 0U,
                "continue screen UP did not restore YES");
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state() == starfox::simulation::GameFlowState::planet_travel
                    && game.map().read_native_word(stage_address.front()) == 1U
                    && game.map().ppu_state().background_mode == 3U,
                "continue YES did not return through the route screen");
        for (std::size_t frame = 0;
             frame < 360U
                 && game.flow_state()
                     == starfox::simulation::GameFlowState::planet_travel;
             ++frame) {
            game.present_frame();
            if ((frame % 3U) == 2U) static_cast<void>(game.tick({}));
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::gameplay
                    && game.objects().is_active(game.player()),
                "game-over route screen did not restart the same stage");

        game.map().write_native_word(level_finished.front(), 6U);
        static_cast<void>(game.tick({}));
        require(game.flow_state() == starfox::simulation::GameFlowState::credits
                    && !game.map().ended() && !game.meter_state().enabled,
                "end-of-game exit did not enter the original credits map");
        game.map().write_native_word(level_finished.front(), 8U);
        static_cast<void>(game.tick({}));
        require(game.flow_state() == starfox::simulation::GameFlowState::finished,
                "end-of-credits exit did not settle on the final presentation");

        starfox::simulation::GameSimulation title_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state() == starfox::simulation::GameFlowState::title,
                "title accepted START before the source GAMEFRAME lock");
        for (std::size_t tick = 1; tick < 39; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        require(title_game.map().ppu_state().background_mode == 1U
                    && (title_game.map().ppu_state().main_screen & 0x04U) != 0U
                    && std::any_of(title_game.map().ppu_state().vram.begin(),
                        title_game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; }),
                "title map did not install its original BG2/BG3 assets");
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        const auto controls_map = upstream_symbols.find("CONTMAP").front();
        const auto map_bank = upstream_symbols.find("MAPBANK").front();
        const auto control_type = upstream_symbols.find("C_TYPE").front();
        const auto vanish_x = upstream_symbols.find("M_VANISHX").front();
        const auto vanish_y = upstream_symbols.find("M_VANISHY").front();
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::controls_type
                    && title_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(controls_map >> 16U)
                    && title_game.map().read_native_word(vanish_x) == 64U
                    && title_game.map().read_native_word(vanish_y) == 48U
                    && std::any_of(
                        title_game.map().ppu_state().vram.begin() + 0xd000U,
                        title_game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; }),
                "title START did not install the original controller screen");
        const auto old_control_type = title_game.map().read_native_byte(control_type);
        static_cast<void>(title_game.tick(
            {0, starfox::input::select, 0}));
        require(title_game.map().read_native_byte(control_type)
                    == static_cast<std::uint8_t>((old_control_type + 1U) & 3U),
                "controller screen SELECT did not cycle the source control type");
        for (std::size_t tick = 1; tick < 16; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice,
                "controller screen START did not enter training/game selection");
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::training
                    && title_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("TRAININGMAP").front() >> 16U),
                "default TRAINING choice did not enter the original training map");
        for (std::size_t tick = 1; tick < 20; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice,
                "training START exit did not return to the source GAME/TRAINING choice");
        static_cast<void>(title_game.tick({0,
            static_cast<starfox::input::ButtonMask>(
                starfox::input::down | starfox::input::start), 0}));
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::planet_select
                    && title_game.map().read_native_word(stage_address.front()) == 0U
                    && title_game.map().ppu_state().background_mode == 3U
                    && title_game.map().unknown_superfx_launches().empty(),
                "GAME choice did not enter the original planet route selector");
        title_game.present_frame();
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        const auto title_selected_map = static_cast<std::uint32_t>(
            title_game.map().read_native_byte(new_map_address.front()))
            | (static_cast<std::uint32_t>(title_game.map().read_native_byte(
                   new_map_address.front() + 1U)) << 8U)
            | (static_cast<std::uint32_t>(title_game.map().read_native_byte(
                   new_map_address.front() + 2U)) << 16U);
        require(title_game.flow_state() == starfox::simulation::GameFlowState::gameplay
                    && title_selected_map == upstream_symbols.find("LEVEL1_1").front(),
                "route selector did not launch the original first map");

        starfox::simulation::GameSimulation attract_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        for (std::size_t tick = 0; tick < 880U; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        require(attract_game.flow_state() == starfox::simulation::GameFlowState::intro
                    && attract_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("INTROMAP").front() >> 16U),
                "title timeout did not enter the original attract-mode map");
        for (std::size_t tick = 0; tick < 29U; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        static_cast<void>(attract_game.tick({0, starfox::input::start, 0}));
        require(attract_game.flow_state() == starfox::simulation::GameFlowState::title
                    && attract_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("TITLEMAP").front() >> 16U),
                "attract-mode input did not return to the title map");

        const starfox::assets::ShapeDecoder textured_decoder{
            upstream_rom, upstream_symbols};
        const auto andross = textured_decoder.decode_by_name(upstream_symbols, "ANDROSS");
        const auto lfdie = textured_decoder.decode_by_name(upstream_symbols, "LFDIE");
        const auto ship4 = textured_decoder.decode_by_name(upstream_symbols, "SHIP_4");
        require(!andross.textures.empty(),
                "original texture address/coordinate tables were not decoded");
        require(std::any_of(lfdie.faces.begin(), lfdie.faces.end(),
                    [](const auto& face) { return face.sprite; }),
                "original sprite-face commands were not preserved");
        const auto sprite_face = std::find_if(lfdie.faces.begin(), lfdie.faces.end(),
            [](const auto& face) { return face.sprite; });
        require(sprite_face->vertex_indices == std::vector<std::uint8_t>{4U}
                    && sprite_face->colour_id == 0U && sprite_face->sprite_size == 1U,
                "s_sprite point/colour/size operands were not decoded in source order");
        require(starfox::assets::ShapeDecoder::select_lod_pointer(ship4.header, 999.0)
                        == static_cast<std::uint16_t>(ship4.header.address)
                    && starfox::assets::ShapeDecoder::select_lod_pointer(ship4.header, 2000.0)
                        == ship4.header.lod2_pointer,
                "Super FX z=1000/2000/3000 LOD thresholds were not preserved");
        const auto ship4_lod = textured_decoder.decode_lod(
            ship4.header, ship4.header.lod2_pointer);
        require(ship4_lod.header.shift == ship4.header.shift
                    && ship4_lod.header.colour_pointer == ship4.header.colour_pointer,
                "selected LOD did not inherit its base shift/colour state");
        starfox::render::Framebuffer textured_frame{224, 192};
        starfox::render::SoftwareRenderer textured_renderer;
        textured_renderer.draw(andross, {}, textured_frame);
        std::array<bool, 16> used_texels{};
        for (const auto pixel : textured_frame.pixels()) {
            used_texels[pixel & 15U] = true;
        }
        require(std::count(used_texels.begin(), used_texels.end(), true) > 4,
                "original packed 4-bit texture did not reach the software rasterizer");
        const auto sprite_colour = std::find_if(andross.colour_words.begin(),
            andross.colour_words.end(), [](std::uint16_t word) {
                return (word & 0xc000U) == 0x4000U;
            });
        require(sprite_colour != andross.colour_words.end(),
                "textured test shape has no software-sprite material");
        starfox::render::RenderPose simple_sprite_pose;
        simple_sprite_pose.simple_scaled_sprite = true;
        simple_sprite_pose.simple_sprite_colour = static_cast<std::uint8_t>(
            std::distance(andross.colour_words.begin(), sprite_colour));
        simple_sprite_pose.simple_sprite_world_size = 64;
        starfox::render::Framebuffer simple_sprite_frame{224, 192};
        textured_renderer.draw(andross, simple_sprite_pose, simple_sprite_frame);
        require(std::any_of(simple_sprite_frame.pixels().begin(),
                    simple_sprite_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel != 0U; }),
                "original simple scaled-sprite material did not render");
        const auto starfox_message = upstream_symbols.find("MSG_STARFOX");
        require(!starfox_message.empty(), "MSG_STARFOX symbol is missing");
        starfox::render::ScaledTextRenderer text_renderer{
            upstream_rom, upstream_symbols};
        starfox::render::RenderPose text_pose;
        text_pose.z = 1'000.0;
        starfox::render::Framebuffer text_frame{224, 192};
        text_renderer.draw(static_cast<std::uint16_t>(starfox_message.front()),
            14U, 0, text_pose, text_frame);
        require(std::any_of(text_frame.pixels().begin(), text_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel == 7U * 16U + 14U; }),
                "original projected 16x16 message font did not render");
        const auto pause_text = upstream_symbols.find("PAUSETXT");
        require(!pause_text.empty(), "PAUSETXT symbol is missing");
        starfox::render::Framebuffer pause_frame{224, 192};
        text_renderer.draw_game_text(
            pause_text.front(), 90, 90, pause_frame);
        require(std::count(pause_frame.pixels().begin(), pause_frame.pixels().end(),
                    static_cast<std::uint8_t>(7U * 16U + 14U)) > 40,
                "original variable-width PAUSED font did not render");

        starfox::render::DustRenderer dot_renderer{
            upstream_rom, upstream_symbols};
        starfox::render::Framebuffer grid_frame{224, 192};
        const auto identity = starfox::simulation::rotation_matrix_q15(
            trig, 0, 0, 0);
        dot_renderer.draw_grid({0.0, -256.0, 0.0, 0.0, 0.0, 0.0},
            identity, grid_frame);
        require(std::count(grid_frame.pixels().begin(), grid_frame.pixels().end(),
                    static_cast<std::uint8_t>(7U * 16U + 14U)) >= 15,
                "original 15x15 ground-dot lattice did not render");
    }

    std::cout << "All simulation substrate tests passed.\n";
    return 0;
}
