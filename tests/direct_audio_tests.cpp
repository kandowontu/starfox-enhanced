#include "starfox/assets/rom.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <span>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::uint64_t pcm_difference(
    std::span<const std::int16_t> left,
    std::span<const std::int16_t> right) {
    return std::inner_product(left.begin(), left.end(), right.begin(),
        std::uint64_t{}, std::plus<>{},
        [](std::int16_t a, std::int16_t b) {
            return static_cast<std::uint64_t>(
                std::abs(static_cast<int>(a) - static_cast<int>(b)));
        });
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: starfox_direct_audio_tests ROM SYMBOLS\n";
        return 2;
    }
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    auto effect = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "LEVEL1_1", std::span<const std::uint8_t>{}, true);
    auto control = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "LEVEL1_1", std::span<const std::uint8_t>{}, true);
    starfox::audio::Spc700Audio effect_audio;
    starfox::audio::Spc700Audio control_audio;

    // The desktop runtime services the cartridge's boot bank before its
    // first direct-level tick. Keep the two IPL transfers in separate SPC
    // frames: the stage bank is an overlay on sound0's initialized workspace.
    const auto effect_boot_writes = effect->map().take_apu_port_writes();
    const auto control_boot_writes = control->map().take_apu_port_writes();
    require(!effect_boot_writes.empty() && !control_boot_writes.empty(),
        "direct-level startup omitted the base SPC bank");
    const auto effect_upload_frames =
        effect_audio.prime_upload_sequence(effect_boot_writes);
    const auto control_upload_frames =
        control_audio.prime_upload_sequence(control_boot_writes);
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());
    require(effect_audio.driver_loaded() && control_audio.driver_loaded(),
        "base SPC bank did not initialize before direct-level entry");
    // Constructor startup contains the boot sound0 bank. The first source
    // update requests the stage overlay after the desktop has synchronized
    // the live SPC ports, so this must be one completed frame here—not zero,
    // and not a combined base+stage transfer.
    require(effect_upload_frames == 1U && control_upload_frames == 1U,
        "direct-level startup did not isolate the base SPC bank");
    require(effect->map().apu_upload_generation() == 1U
                && control->map().apu_upload_generation() == 1U,
        "stage SPC bank was uploaded before the base driver could initialize");

    // The packaged CLI runtime performs its ordinary 1.5-second black preroll
    // after construction. Unlike a title/menu launch, a direct map otherwise
    // has no time for sound0 to finish initializing the ARAM workspace that
    // the stage bank overlays.
    constexpr std::size_t direct_entry_settle_ticks = 30U;
    for (std::size_t tick = 0U; tick < direct_entry_settle_ticks; ++tick) {
        static_cast<void>(effect_audio.render_logic_tick({}));
        static_cast<void>(control_audio.render_logic_tick({}));
    }
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());

    const auto enqueue_laser_effect = [&]() {
        const auto sound_read = symbols.find("SDGPT3").front();
        const auto sound_write = symbols.find("SDSPT3").front();
        effect->map().write_native_byte(sound_read,
            effect->map().read_native_byte(sound_write));
        effect->map().write_native_byte(
            symbols.find("SDPCK3").front(), 0U);
        effect->map().write_native_byte(0x002143U, 0U);
        for (const auto* optional_flag : {
                 "NOSFX", "BGMSFX", "NOSETPORT3"}) {
            const auto addresses = symbols.find(optional_flag);
            if (!addresses.empty()) {
                effect->map().write_native_byte(addresses.front(), 0U);
            }
        }
        starfox::simulation::Wdc65816Registers registers;
        registers.a = 0x35U;
        registers.status = 0x24U;
        effect->map().call_native_routine(
            symbols.find("SETPORT3_L").front(), registers, 5'000'000, true);
    };

    auto saw_stage_upload = false;
    auto early_effect_queued = false;
    auto early_effect_command = false;
    auto early_effect_ack = false;
    auto early_effect_audible = false;
    auto early_music_isolated = true;
    for (std::size_t tick = 0; tick < 360U; ++tick) {
        const auto effect_tick = effect->tick({});
        const auto control_tick = control->tick({});
        saw_stage_upload = saw_stage_upload
            || effect->map().apu_upload_generation() >= 2U;
        static_cast<void>(effect_audio.render_logic_tick(
            effect_tick.audio_port_writes));
        static_cast<void>(control_audio.render_logic_tick(
            control_tick.audio_port_writes));
        effect->synchronize_apu_output_ports(effect_audio.output_ports());
        control->synchronize_apu_output_ports(control_audio.output_ports());
        if (early_effect_queued) {
            early_effect_command = early_effect_command || std::find(
                effect_tick.sound_effect_commands.begin(),
                effect_tick.sound_effect_commands.end(), 0x35U)
                    != effect_tick.sound_effect_commands.end();
            early_effect_ack = early_effect_ack
                || effect_audio.output_ports()[3] == 0x35U;
            early_effect_audible = early_effect_audible || pcm_difference(
                effect_audio.last_effect_samples(),
                control_audio.last_effect_samples()) != 0U;
            early_music_isolated = early_music_isolated && std::equal(
                effect_audio.last_music_samples().begin(),
                effect_audio.last_music_samples().end(),
                control_audio.last_music_samples().begin(),
                control_audio.last_music_samples().end());
        } else if (effect->map().apu_upload_generation() >= 2U
                   && effect_audio.driver_loaded()) {
            // Exercise port 3 immediately after the first stage bank replaces
            // sound0. This is the precise startup window that the packaged CLI
            // path used to leave half-initialized.
            enqueue_laser_effect();
            early_effect_queued = true;
        }
    }
    require(effect_audio.driver_loaded() && control_audio.driver_loaded(),
        "direct-level startup did not leave a running SPC driver");
    require(effect->map().apu_upload_generation() >= 2U
                && control->map().apu_upload_generation() >= 2U,
        "direct-level startup did not replace the base SPC bank");
    require(saw_stage_upload,
        "direct-level startup never submitted its stage SPC overlay");
    require(early_effect_queued && early_effect_command && early_effect_ack
                && early_effect_audible,
        "first direct-entry stage SFX was not audible and acknowledged");
    require(early_music_isolated,
        "first direct-entry stage SFX modified the music stem");

    enqueue_laser_effect();

    const auto fired = effect->tick({});
    const auto idle = control->tick({});
    auto fired_pcm = effect_audio.render_logic_tick(fired.audio_port_writes);
    auto idle_pcm = control_audio.render_logic_tick(idle.audio_port_writes);
    effect->synchronize_apu_output_ports(effect_audio.output_ports());
    control->synchronize_apu_output_ports(control_audio.output_ports());
    auto saw_command = std::find(fired.sound_effect_commands.begin(),
        fired.sound_effect_commands.end(), 0x35U)
        != fired.sound_effect_commands.end();
    auto saw_ack = effect_audio.output_ports()[3] == 0x35U;
    auto heard_difference = pcm_difference(fired_pcm, idle_pcm) != 0U;
    auto heard_effect = std::any_of(
        effect_audio.last_effect_samples().begin(),
        effect_audio.last_effect_samples().end(),
        [](std::int16_t sample) { return sample != 0; });
    auto effect_stem_differs = pcm_difference(
        effect_audio.last_effect_samples(),
        control_audio.last_effect_samples()) != 0U;
    auto music_is_isolated = std::equal(
        effect_audio.last_music_samples().begin(),
        effect_audio.last_music_samples().end(),
        control_audio.last_music_samples().begin(),
        control_audio.last_music_samples().end());
    for (std::size_t tick = 0; tick < 50U; ++tick) {
        const auto effect_tick = effect->tick({});
        const auto control_tick = control->tick({});
        fired_pcm = effect_audio.render_logic_tick(
            effect_tick.audio_port_writes);
        idle_pcm = control_audio.render_logic_tick(
            control_tick.audio_port_writes);
        effect->synchronize_apu_output_ports(effect_audio.output_ports());
        control->synchronize_apu_output_ports(control_audio.output_ports());
        saw_command = saw_command || std::find(
            effect_tick.sound_effect_commands.begin(),
            effect_tick.sound_effect_commands.end(), 0x35U)
                != effect_tick.sound_effect_commands.end();
        saw_ack = saw_ack || effect_audio.output_ports()[3] == 0x35U;
        heard_difference = heard_difference
            || pcm_difference(fired_pcm, idle_pcm) != 0U;
        heard_effect = heard_effect || std::any_of(
            effect_audio.last_effect_samples().begin(),
            effect_audio.last_effect_samples().end(),
            [](std::int16_t sample) { return sample != 0; });
        effect_stem_differs = effect_stem_differs || pcm_difference(
            effect_audio.last_effect_samples(),
            control_audio.last_effect_samples()) != 0U;
        music_is_isolated = music_is_isolated && std::equal(
            effect_audio.last_music_samples().begin(),
            effect_audio.last_music_samples().end(),
            control_audio.last_music_samples().begin(),
            control_audio.last_music_samples().end());
    }
    if (!(saw_command && saw_ack && heard_difference && effect_stem_differs
            && heard_effect && music_is_isolated)) {
        std::cerr << "direct SFX command=" << saw_command
                  << " acknowledgement=" << saw_ack
                  << " audible=" << heard_difference
                  << " effect-stem=" << heard_effect
                  << " effect-difference=" << effect_stem_differs
                  << " music-isolated=" << music_is_isolated << '\n';
    }
    require(saw_command && saw_ack && heard_difference
                && heard_effect && effect_stem_differs,
        "direct-level laser did not produce an isolated SPC effect stem");
    require(music_is_isolated,
        "sound effect command modified or consumed a music-channel sample");

    // SOUND.ASM's PLAYERSND writes the continuous Arwing engine to port 1;
    // NEAROBJS/DO_OBSTACLES use port 2 for other engines and positional
    // loops. MSU replaces only port-0 BGM, so both continuous buses must be
    // present in the independently mixed effect stem as well as port 3.
    auto continuous_effect_changed = false;
    auto continuous_music_unchanged = true;
    for (std::size_t tick = 0U; tick < 12U; ++tick) {
        // Repeated writes match the cartridge's per-raster engine update.
        constexpr std::array writes{
            starfox::simulation::ApuPortWrite{1U, 0xc0U, 0U},
            starfox::simulation::ApuPortWrite{2U, 0x60U, 0U},
        };
        static_cast<void>(effect_audio.render_logic_tick(writes));
        static_cast<void>(control_audio.render_logic_tick({}));
        continuous_effect_changed = continuous_effect_changed || pcm_difference(
            effect_audio.last_effect_samples(),
            control_audio.last_effect_samples()) != 0U;
        continuous_music_unchanged = continuous_music_unchanged && std::equal(
            effect_audio.last_music_samples().begin(),
            effect_audio.last_music_samples().end(),
            control_audio.last_music_samples().begin(),
            control_audio.last_music_samples().end());
    }
    require(continuous_effect_changed,
        "continuous engine ports did not reach the isolated effect stem");
    require(continuous_music_unchanged,
        "continuous effect bus modified the isolated music stem");

    // PAUSESND is carried over the nominal SFX port, but $01/$02 are global
    // controls in Nintendo's driver. The two host SPC stems must therefore
    // diverge when only one receives PAUSE ON; ordinary SFX above must not.
    constexpr std::array pause_on{
        starfox::simulation::ApuPortWrite{3U, 0x02U, 0U}};
    auto pause_changed_music = false;
    static_cast<void>(effect_audio.render_logic_tick(pause_on));
    static_cast<void>(control_audio.render_logic_tick({}));
    for (std::size_t tick = 0U; tick < 8U; ++tick) {
        static_cast<void>(effect_audio.render_logic_tick({}));
        static_cast<void>(control_audio.render_logic_tick({}));
        pause_changed_music = pause_changed_music || pcm_difference(
            effect_audio.last_music_samples(),
            control_audio.last_music_samples()) != 0U;
    }
    require(pause_changed_music,
        "global PAUSE ON command did not reach the isolated music driver");
}
