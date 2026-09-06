#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <bit>
#include <limits>

namespace starfox::simulation {

bool GameSimulation::native_transfer_active() const noexcept {
    return native_transfer_phase_ != NativeTransferPhase::idle;
}

std::uint64_t GameSimulation::native_transfer_clock() const noexcept {
    return native_transfer_timeline_ ? native_transfer_timeline_->raster().elapsed() : 0U;
}

void GameSimulation::begin_native_transfer(const input::TickInput& input) {
    if (native_transfer_active()) throw std::logic_error{"A native transfer is already active or failed"};
    if (!native_transfer_initialized_ || (flow_state_ != GameFlowState::gameplay
            && flow_state_ != GameFlowState::training))
        throw std::logic_error{"Native transfers require source-initialized gameplay or training"};
    if (!native_transfer_timeline_) {
        auto timeline = std::make_shared<SnesCpuTimeline>();
        map_.set_cpu_timeline(timeline);
        map_.set_gsu_timing(true);
        native_transfer_timeline_ = std::move(timeline);
        // INITSCREEN_L established these before the late timeline attachment.
        map_.write_native_word(0x4209U,static_cast<std::uint16_t>(ram_symbol("GAMEVW_POS")));
        map_.write_native_word(0x4207U,0U);
        map_.write_native_byte(0x4200U,0x31U);
    }
    map_.hold_native_presentation();
    write_input(input);
    native_draw_candidates_.clear();
    native_draw_order_.clear(); native_draw_order_captured_ = false;
    native_transfer_registers_ = {};
    native_transfer_registers_.status = 0x20U;
    native_transfer_result_ = {};
    native_transfer_task_started_ = false;
    native_transfer_phase_ = NativeTransferPhase::black;
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
            const std::array show_view_stop{rom_symbol("SHOWVIEW_L"),rom_symbol("BUILD_DRAWLIST_L")};
            const auto stops = transfer ? std::span<const std::uint32_t>{show_view_stop}
                : std::span<const std::uint32_t>{};
            Wdc65816TaskResult task;
            if (!native_transfer_task_started_) {
                native_transfer_task_started_ = true;
                task = map_.begin_native_task(transfer ? rom_symbol("TRANSFER_L") : set_black_,
                    native_transfer_registers_,stops,5'000'000U,false,false);
            } else task = map_.resume_native_task(native_transfer_registers_,stops,5'000'000U,false,false);
            native_transfer_result_.prelude_instructions += task.instructions;
            if (task.stopped) throw std::runtime_error{"Native transfer entered STP"};
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
            if (!task.waiting && transfer && task.stop_address == show_view_stop[0])
                capture_native_draw_candidates();
            if (!task.waiting && transfer && task.stop_address == show_view_stop[1])
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

void GameSimulation::publish_native_transfer() {
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
    dust_.tick(camera,world,map_.dots_mode()<0,dust_point_count());
    particles_.tick(objects_,map_.read_native_word(particles_enabled_) != 0U);
    ++source_update_sequence_;
}

} // namespace starfox::simulation
