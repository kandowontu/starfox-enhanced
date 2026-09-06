#include "starfox/simulation/game_simulation.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <algorithm>
#include <bit>
#include <limits>

namespace starfox::simulation {

bool GameSimulation::native_transfer_active() const noexcept {
    return native_transfer_phase_ != NativeTransferPhase::idle;
}

std::uint64_t GameSimulation::native_transfer_clock() const noexcept {
    return native_transfer_timeline_ ? native_transfer_timeline_->raster().elapsed() : 0U;
}

void GameSimulation::finish_native_gameplay_exit() {
    if (!native_main_exit_pending_ || native_transfer_active() || !native_transfer_timeline_)
        throw std::logic_error{"No completed native gameplay exit is pending"};
    map_.detach_native_task();
    native_transfer_timeline_.reset();
    native_transfer_task_started_ = false;
    native_main_loop_ = false;
    native_main_exit_pending_ = false;
    native_input_pulses_.fill(0U);
    service_level_exit();
}

void GameSimulation::begin_native_transfer(const input::TickInput& input) {
    begin_native_update(input,false);
}

void GameSimulation::begin_native_gameplay_update(const input::TickInput& input) {
    if (!native_gameplay_ready())
        throw std::logic_error{"Native MAIN updates require gameplay without a host frontend transition"};
    begin_native_update(input,true);
}

std::array<std::uint32_t,2> GameSimulation::find_native_main_boundaries() const {
    // Stop on the fall-through of MAIN's LEVELFINISHED test, before it
    // increments STAGE or enters a non-gameplay sequence. Validate the
    // cartridge instructions and their backward target rather than assuming
    // a fixed offset shared by the two different MAIN implementations.
    const auto entry = rom_symbol("GAMELOOP2");
    std::uint32_t found{};
    std::uint32_t pause_return{};
    const auto pause=rom_symbol("DOPAUSE");
    const auto byte = [&](std::uint32_t address) { return map_.read_native_byte(address); };
    for (auto pc=entry;pc<entry+512U;++pc) {
        if (byte(pc)==0x20U && byte(pc+1U)==(pause & 0xffU)
                && byte(pc+2U)==((pause >> 8U) & 0xffU)) {
            if (pause_return) throw std::runtime_error{"Ambiguous native MAIN pause call"};
            pause_return=pc+3U;
        }
        if (byte(pc)!=0xc2U || byte(pc+1U)!=0x20U || byte(pc+2U)!=0xadU
                || byte(pc+3U)!=(level_finished_ & 0xffU)
                || byte(pc+4U)!=((level_finished_ >> 8U) & 0xffU)) continue;
        std::uint32_t exit{};
        if (byte(pc+5U)==0xf0U
                && std::int64_t(pc+7U)+std::bit_cast<std::int8_t>(byte(pc+6U))==entry)
            exit=pc+7U;
        if (byte(pc+5U)==0xd0U && byte(pc+6U)==3U && byte(pc+7U)==0x4cU
                && byte(pc+8U)==(entry & 0xffU) && byte(pc+9U)==((entry >> 8U) & 0xffU))
            exit=pc+10U;
        if (!exit) continue;
        if (found) throw std::runtime_error{"Ambiguous native MAIN exit boundary"};
        found=exit;
    }
    if (!found || !pause_return) throw std::runtime_error{"Unsupported native MAIN boundaries"};
    return {found,pause_return};
}

void GameSimulation::sample_native_controller_held(const std::array<input::ButtonMask,5>& physical) {
    if (!native_transfer_timeline_ || native_transfer_phase_==NativeTransferPhase::failed)
        throw std::logic_error{"Native controller sampling requires a live native binding"};
    native_controller_held_=physical;
    auto held=physical;
    if (native_transfer_active())
        for (std::size_t i=0;i<held.size();++i) held[i]|=native_input_pulses_[i];
    map_.write_native_word(hardware_controller_,held[0]);
    if (!starfox_ex_cartridge_) return;
    // Scope owns JOY2's physical packet when selected.
    if (!ex_scope_control_enabled()) map_.write_native_word(ex_hardware_controller_2_,held[1]);
    if (map_.read_native_byte(ex_multitap_mode_)!=0U
            && map_.read_native_byte(ex_number_players_)==1U) held.fill(held[0]);
    for (std::size_t i=0;i<held.size();++i) map_.write_native_word(ex_multitap_controllers_[i],held[i]);
}

void GameSimulation::begin_native_update(const input::TickInput& input, bool main_loop) {
    if (native_transfer_active()) throw std::logic_error{"A native transfer is already active or failed"};
    if (native_main_exit_pending_) throw std::logic_error{"Native MAIN requires a scene-exit handoff"};
    if (native_transfer_timeline_ && native_main_loop_ != main_loop)
        throw std::logic_error{"Cannot mix native MAIN and transfer-only execution"};
    if (!native_transfer_initialized_ || (flow_state_ != GameFlowState::gameplay
            && flow_state_ != GameFlowState::training))
        throw std::logic_error{"Native transfers require source-initialized gameplay or training"};
    if (!native_transfer_timeline_) {
        if (main_loop) {
            native_main_entry_=rom_symbol("GAMELOOP2");
            const auto boundaries=find_native_main_boundaries();
            native_main_exit_=boundaries[0];
            native_main_pause_return_=boundaries[1];
            native_main_pause_entry_=rom_symbol("DOPAUSE");
            native_pause_present_stops_.clear();
            const auto pause_end=rom_symbol("PRINTPAUSE");
            const auto wait=rom_symbol("WAITDMA_L");
            if (pause_end<=native_main_pause_entry_ || pause_end-native_main_pause_entry_>1024U)
                throw std::runtime_error{"Unsupported native pause routine range"};
            for (auto pc=native_main_pause_entry_;pc+3U<pause_end;++pc) {
                if (map_.read_native_byte(pc)==0x22U
                    && map_.read_native_byte(pc+1U)==(wait&0xffU)
                    && map_.read_native_byte(pc+2U)==((wait>>8U)&0xffU)
                    && map_.read_native_byte(pc+3U)==(wait>>16U))
                    native_pause_present_stops_.push_back(pc+4U);
            }
            if (native_pause_present_stops_.size()!=(starfox_ex_cartridge_ ? 4U : 3U))
                throw std::runtime_error{"Unsupported native pause presentation boundaries"};
            native_main_stop_addresses_={rom_symbol("SHOWVIEW_L"),rom_symbol("BUILD_DRAWLIST_L"),
                native_main_entry_,native_main_exit_,native_main_pause_entry_,native_main_pause_return_};
            native_main_stop_addresses_.insert(native_main_stop_addresses_.end(),
                native_pause_present_stops_.begin(),native_pause_present_stops_.end());
        }
        auto timeline = std::make_shared<SnesCpuTimeline>();
        map_.set_cpu_timeline(timeline);
        map_.set_gsu_timing(true);
        native_transfer_timeline_ = std::move(timeline);
        native_main_loop_=main_loop;
        // INITSCREEN_L established these before the late timeline attachment.
        map_.write_native_word(0x4209U,static_cast<std::uint16_t>(ram_symbol("GAMEVW_POS")));
        map_.write_native_word(0x4207U,0U);
        map_.write_native_byte(0x4200U,0x31U);
    }
    map_.hold_native_presentation();
    write_input(input);
    native_input_pulses_[0]=input.pressed;
    native_controller_held_[0]=input.held;
    for (std::size_t i=0;i<secondary_inputs_.size();++i) {
        native_input_pulses_[i+1U]=secondary_inputs_[i].pressed;
        native_controller_held_[i+1U]=secondary_inputs_[i].held;
    }
    native_draw_candidates_.clear();
    native_draw_order_.clear(); native_draw_order_captured_ = false;
    if (!main_loop || !native_transfer_task_started_) {
        native_transfer_registers_ = {};
        native_transfer_registers_.status = 0x20U;
    }
    native_transfer_result_ = {};
    if (!main_loop) native_transfer_task_started_ = false;
    native_transfer_phase_ = main_loop ? NativeTransferPhase::main : NativeTransferPhase::black;
    sample_native_controller_held(native_controller_held_);
}

std::optional<GameTickResult> GameSimulation::advance_native_transfer(std::uint64_t master_clocks) {
    if (!native_transfer_active() || native_transfer_phase_ == NativeTransferPhase::failed)
        throw std::logic_error{"No resumable native transfer is active"};
    if (master_clocks > std::numeric_limits<std::uint64_t>::max()-native_transfer_clock())
        throw std::invalid_argument{"Native transfer clock budget overflows"};
    const auto deadline = native_transfer_clock()+master_clocks;
    map_.set_task_clock_deadline(deadline);
    try {
        while (native_transfer_clock() < deadline) {
            const bool transfer = native_transfer_phase_ == NativeTransferPhase::transfer;
            const bool main_loop = native_transfer_phase_ == NativeTransferPhase::main;
            const std::array show_view_stop{rom_symbol("SHOWVIEW_L"),rom_symbol("BUILD_DRAWLIST_L")};
            const auto stops = main_loop ? std::span<const std::uint32_t>{native_main_stop_addresses_}
                : transfer ? std::span<const std::uint32_t>{show_view_stop}
                : std::span<const std::uint32_t>{};
            Wdc65816TaskResult task;
            if (!native_transfer_task_started_) {
                native_transfer_task_started_ = true;
                task = map_.begin_native_task(main_loop ? native_main_entry_
                        : transfer ? rom_symbol("TRANSFER_L") : set_black_,
                    native_transfer_registers_,stops,5'000'000U,false,false);
            } else task = map_.resume_native_task(native_transfer_registers_,stops,5'000'000U,false,false);
            native_transfer_result_.prelude_instructions += task.instructions;
            if (task.stopped) throw std::runtime_error{"Native transfer entered STP"};
            if (main_loop && task.returned) throw std::runtime_error{"Native MAIN unexpectedly returned"};
            if (main_loop && !task.waiting) {
                if (task.stop_address==native_main_pause_entry_) {
                    paused_=true;
                    // MAIN has accepted Start. DOPAUSE needs physical held
                    // states; gameplay taps must not latch Start or become
                    // unintended input to EX's interactive pause menu.
                    native_input_pulses_.fill(0U);
                    sample_native_controller_held(native_controller_held_);
                }
                if (task.stop_address==native_main_pause_return_) paused_=false;
                if (paused_ && std::find(native_pause_present_stops_.begin(),native_pause_present_stops_.end(),
                        task.stop_address)!=native_pause_present_stops_.end()) {
                    if (native_draw_order_captured_) publish_native_transfer(false);
                    else {
                        // EX waits once before its first paused TRANSFER.
                        // Its menu/PPU state is current, but no new geometry
                        // submission exists to replace the last object view.
                        map_.release_native_presentation();
                        ++native_presentation_revision_;
                    }
                    map_.hold_native_presentation();
                }
            }
            if (main_loop && !task.waiting && (task.stop_address==native_main_entry_
                    || task.stop_address==native_main_exit_)) {
                map_.set_task_clock_deadline({});
                publish_native_transfer();
                native_main_exit_pending_=task.stop_address==native_main_exit_;
                native_transfer_phase_=NativeTransferPhase::idle;
                native_transfer_result_.audio_port_writes=map_.take_apu_port_writes();
                return std::move(native_transfer_result_);
            }
            if (task.returned) {
                native_transfer_task_started_ = false;
                if (!transfer) { native_transfer_phase_ = NativeTransferPhase::transfer; continue; }
                map_.set_task_clock_deadline({});
                publish_native_transfer();
                native_transfer_phase_ = NativeTransferPhase::idle;
                native_transfer_result_.audio_port_writes = map_.take_apu_port_writes();
                return std::move(native_transfer_result_);
            }
            // A deadline can land exactly on a phase entry. Capture it now:
            // resume_task executes the entry before considering stop PCs again.
            if (!task.waiting && (transfer || main_loop) && task.stop_address == show_view_stop[0])
                capture_native_draw_candidates();
            if (!task.waiting && (transfer || main_loop) && task.stop_address == show_view_stop[1])
                capture_native_draw_order();
            if (task.deadline_reached || task.waiting) break;
        }
        return std::nullopt;
    } catch (...) {
        map_.set_task_clock_deadline({});
        native_transfer_phase_ = NativeTransferPhase::failed;
        throw;
    }
}

void GameSimulation::capture_native_draw_candidates() {
    if (!native_transfer_capture_) native_transfer_capture_ = std::make_unique<NativePresentationSnapshot>();
    map_.capture_live_presentation(*native_transfer_capture_);
    const auto byte = [&](std::uint32_t address) { return native_transfer_capture_->read_ram(address).value(); };
    const auto word = [&](std::uint32_t address) {
        return static_cast<std::uint16_t>(byte(address) | (std::uint16_t(byte(address+1U)) << 8U));
    };
    native_draw_candidates_.clear();
    std::array<bool,kMaximumObjects+1U> seen{};
    auto pointer = word(ram_symbol("ALLST"));
    while (pointer) {
        const auto handle = map_.native_object_handle(pointer);
        if (!handle || seen[handle]) throw std::runtime_error{"Invalid native pre-draw object list"};
        seen[handle] = true;
        if (!(byte(0x7e0000U+pointer+ram_symbol("AL_SFLAGS4")) & 8U))
            native_draw_candidates_.push_back(handle);
        pointer = word(0x7e0000U+pointer);
    }
}

void GameSimulation::capture_native_draw_order() {
    // Draw-list memory is reused by subsequent graphics work. Save it at
    // the SHOWVIEW -> BUILD_DRAWLIST boundary, before bitmap rendering.
    if (!native_transfer_capture_) throw std::runtime_error{"Native transfer did not submit a view"};
    map_.capture_live_presentation(*native_transfer_capture_);
    const auto byte = [&](std::uint32_t address) { return native_transfer_capture_->read_ram(address).value(); };
    const auto word = [&](std::uint32_t address) {
        return static_cast<std::uint16_t>(byte(address) | (std::uint16_t(byte(address+1U)) << 8U));
    };
    const auto base = ram_symbol("M_DRAWLIST") & 0xffffU;
    const auto stride = ram_symbol("DL_SIZEOF");
    if (!stride || word(ram_symbol("M_NUMSHAPES")) != native_draw_candidates_.size())
        throw std::runtime_error{"Native draw-list candidate count differs from its submitted view"};
    std::vector<std::pair<ObjectHandle,std::uint8_t>> ordered;
    std::array<bool,kMaximumObjects+1U> seen{};
    auto pointer = word(ram_symbol("M_DLPTR"));
    while (pointer) {
        if (pointer < base || (pointer-base)%stride || (pointer-base)/stride >= native_draw_candidates_.size())
            throw std::runtime_error{"Invalid native draw-list pointer " + std::to_string(pointer)
                + " base " + std::to_string(base) + " stride " + std::to_string(stride)
                + " candidates " + std::to_string(native_draw_candidates_.size())};
        const auto handle = native_draw_candidates_[(pointer-base)/stride];
        if (seen[handle]) throw std::runtime_error{"Cyclic native draw list"};
        seen[handle] = true;
        ordered.emplace_back(handle,byte(0x700000U+pointer+(ram_symbol("DL_SFLAGS") & 0xffffU)));
        pointer = word(0x700000U+pointer);
    }
    if (ordered.size() != native_draw_candidates_.size())
        throw std::runtime_error{"Native sorted draw list omitted a submitted object"};
    native_draw_order_ = std::move(ordered);
    native_draw_order_captured_ = true;
}

void GameSimulation::publish_native_transfer(bool advance_effects) {
    if (!native_draw_order_captured_) throw std::runtime_error{"Native transfer did not finish its draw-list submission"};
    map_.release_native_presentation();
    map_.restore_objects_from_native();
    map_.restore_map_state_from_native();
    refresh_player_reference();
    draw_order_.clear(); submitted_object_flags_.fill({});
    for (const auto& [handle,flags] : native_draw_order_) if (objects_.is_active(handle)) {
        draw_order_.push_back(handle);
        submitted_object_flags_[handle] = {objects_.generation(handle),flags};
    }
    MatrixQ15 world{};
    for (std::size_t i=0;i<world.size();++i)
        world[i] = std::bit_cast<std::int16_t>(map_.read_native_word(world_matrix_+std::uint32_t(i*2U)));
    const std::array camera{
        std::bit_cast<std::int16_t>(map_.read_native_word(view_position_)),
        std::bit_cast<std::int16_t>(map_.read_native_word(view_position_+2U)),
        std::bit_cast<std::int16_t>(map_.read_native_word(view_position_+4U))};
    if (advance_effects) {
        dust_.tick(camera,world,map_.dots_mode()<0,dust_point_count());
        particles_.tick(objects_,map_.read_native_word(particles_enabled_) != 0U);
    }
    ++source_update_sequence_;
    ++native_presentation_revision_;
}

} // namespace starfox::simulation
