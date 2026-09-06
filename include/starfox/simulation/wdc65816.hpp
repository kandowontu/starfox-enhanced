#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace starfox::simulation {

class SnesCpuTimeline;

class Wdc65816ExecutionError : public std::runtime_error {
public:
    Wdc65816ExecutionError(std::string message,
        std::uint32_t entry_address,
        std::uint32_t program_address,
        std::size_t instructions)
        : std::runtime_error{std::move(message)},
          entry_address_(entry_address),
          program_address_(program_address),
          instructions_(instructions) {}

    [[nodiscard]] std::uint32_t entry_address() const noexcept {
        return entry_address_;
    }
    [[nodiscard]] std::uint32_t program_address() const noexcept {
        return program_address_;
    }
    [[nodiscard]] std::size_t instructions() const noexcept {
        return instructions_;
    }

private:
    std::uint32_t entry_address_{};
    std::uint32_t program_address_{};
    std::size_t instructions_{};
};

struct Wdc65816Registers {
    std::uint16_t a{};
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint16_t direct{};
    std::uint16_t stack{0x1ff};
    std::uint8_t data_bank{};
    // Native mode, 16-bit accumulator and index registers, IRQ disabled.
    std::uint8_t status{0x04};
};
struct Wdc65816InterruptSample {
    std::uint32_t instruction_address{};
    std::uint64_t master_clocks{};
    bool masked{};
    bool operator==(const Wdc65816InterruptSample&) const = default;
};

struct ApuPortWrite {
    std::uint8_t port{};
    std::uint8_t value{};
    std::uint32_t clock_offset{};

    friend bool operator==(const ApuPortWrite&, const ApuPortWrite&) = default;
};

struct MsuRegisterWrite {
    std::uint16_t address{};
    std::uint8_t value{};
    std::uint32_t clock_offset{};

    friend bool operator==(const MsuRegisterWrite&, const MsuRegisterWrite&)
        = default;
};

struct Wdc65816TaskResult {
    std::size_t instructions{};
    std::uint32_t stop_address{};
    bool returned{};
    // Live WAI/STP yield here; stop_address is the next PC and may not be one
    // of the requested stop addresses. resume_task advances another idle.
    bool waiting{};
    bool stopped{};
    bool deadline_reached{};
};

// Snapshot of CONTINUE.ASM's dedicated MSHOWOBJ3 launch. Unlike ordinary
// gameplay objects this model is drawn directly into FOXY_CONTINUE's 224x192
// bitmap and therefore has no entry in the host ObjectPool.
struct NativeModelDrawState {
    bool active{};
    std::uint16_t shape{};
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t z{};
    std::uint16_t rotation_x{};
    std::uint16_t rotation_y{};
    std::uint16_t rotation_z{};
    std::int16_t vanish_x{112};
    std::int16_t vanish_y{96};
    std::uint16_t animation_frame{};
    std::uint16_t colour_frame{};
    std::uint16_t colour_table{};
};

// Presentation data only: not a CPU/GSU save state. Capturing it has no bus
// side effects, including when a paused GSU still owns cartridge RAM.
struct NativePresentationSnapshot {
    std::array<std::uint8_t,0x20000U> wram{};
    std::array<std::uint8_t,0x10000U> gsu_ram{};
    std::array<std::uint8_t,0x10000U> cartridge_ram{};
    SnesPpuState ppu;
    NativeModelDrawState model;
    bool live_gsu{}, extended_gsu_ram{};
    [[nodiscard]] std::optional<std::uint8_t> read_ram(std::uint32_t address) const noexcept;
};

// Project-owned adapter around the pinned MIT RetroCPU core. It supplies the
// SNES LoROM/WRAM address map and bounded native-mode subroutine execution.
class Wdc65816 {
public:
    static constexpr std::size_t cartridge_ram_size = 0x10000U;

    explicit Wdc65816(
        const assets::RomImage& rom,
        const assets::SymbolMap* symbols = nullptr);
    ~Wdc65816();
    Wdc65816(Wdc65816&&) noexcept;
    Wdc65816& operator=(Wdc65816&&) noexcept;
    Wdc65816(const Wdc65816&) = delete;
    Wdc65816& operator=(const Wdc65816&) = delete;

    [[nodiscard]] std::uint8_t read8(std::uint32_t address) const;
    [[nodiscard]] std::uint16_t read16(std::uint32_t address) const;
    void capture_presentation(NativePresentationSnapshot& snapshot) const;
    // Cumulative native-instruction/interrupt-entry bus and internal clocks. Excludes the host's
    // synthetic call-stack setup, DMA, refresh and translated GSU execution.
    // This is a measurement input, not the game's current pace scheduler.
    [[nodiscard]] std::uint64_t executed_master_clocks() const noexcept;
    [[nodiscard]] std::uint32_t program_address() const noexcept;
    // Current native P, including during bus callbacks. Reading it does not
    // sample interrupts or advance the CPU.
    [[nodiscard]] std::uint8_t status_register() const noexcept;
    using InstructionBoundaryCallback = std::function<void(std::uint64_t)>;
    // Observe cumulative native clocks before instructions and at call/task
    // boundaries. Resuming a paused task may repeat the same timestamp.
    // The callback may update device state, but must not reenter CPU execution
    // or replace itself. With DMA ownership enabled, bounded calls stop
    // automatically draining gameplay bitmap states 2/4/6.
    // This does not supply raster timing or clocks for DMA/translated GSU work.
    void set_instruction_boundary_callback(InstructionBoundaryCallback callback,
        bool owns_gameplay_bitmap_dma = false);
    using BusClockCallback = std::function<void(std::uint32_t)>;
    // Called at the actual timed APU bus access. A value denotes a write;
    // otherwise return the sound processor's output port. IPL reads retain
    // the decoded boot protocol, while still notifying the callback of time.
    using ApuBusCallback = std::function<std::uint8_t(
        std::uint64_t,std::uint8_t,std::optional<std::uint8_t>)>;
    void set_apu_bus_callback(ApuBusCallback callback);
    // Advance a device timeline at native bus-operation boundaries: reads
    // step wait-4 clocks before sampling data and 4 afterward; writes step
    // their full wait before storing data; each idle steps separately.
    // Excludes synthetic call setup, DMA and translated GSU work. The callback
    // may update device state but must not reenter CPU execution or replace itself.
    void set_bus_clock_callback(BusClockCallback callback);
    using InterruptSampleCallback = std::function<bool(const Wdc65816InterruptSample&)>;
    // Observe the native last-cycle polling point. A true return selects the
    // pending-interrupt dummy read on idleIRQ instructions. This does not
    // itself enter a handler; live halt scheduling requires timeline binding.
    // Like bus callbacks, it must not reenter execution or replace callbacks.
    void set_interrupt_sample_callback(InterruptSampleCallback callback);
    // Bind live timer/blanking/counter registers and advance their shared
    // timeline during native bus operations. Null restores bounded-call I/O.
    // Native IRQ/NMI requests are sampled at lastCycle and delivered at the
    // next CPU step. Accepted requests and halt states survive detach. Live
    // WAI/STP yield through the task API; STP needs CPU reconstruction to reset.
    // DMA and scanline HDMA share bus ownership after their startup delay.
    // Disable HDMA and finish pending DMA before replacing the timeline.
    void set_cpu_timeline(std::shared_ptr<SnesCpuTimeline> timeline);
    // Opt in to the resumable GSU and cartridge bus map. Requires a live CPU
    // timeline. First enable starts cold internal state from recorded CPU I/O;
    // shared RAM is retained. Disable only after GO, RAM writes and IRQ finish.
    void set_gsu_timing(bool enabled);
    [[nodiscard]] bool gsu_timing_enabled() const noexcept;
    // Cooperative task-only deadline in absolute raster master clocks.
    // Yields at an instruction boundary; the final instruction/DMA may
    // overrun. The task, registers and pending interrupts remain resumable.
    // Requires a timeline. Null removes the deadline; ordinary calls ignore it.
    void set_task_clock_deadline(std::optional<std::uint64_t> deadline);
    // Without a timeline, hardware signals use legacy instruction-boundary
    // sampling; with one they use the live last-cycle polling point. IRQ is
    // level-sensitive; the device must release it. NMI is a latched edge and
    // is acknowledged on acceptance in live mode (on entry in legacy mode).
    // Neither API replaces registers or the stack.
    void set_irq_line(bool asserted) noexcept;
    void pulse_nmi() noexcept;
    [[nodiscard]] std::uint64_t interrupts_taken() const noexcept;
    void write8(std::uint32_t address, std::uint8_t value);
    void write16(std::uint32_t address, std::uint16_t value);
    [[nodiscard]] bool load_cartridge_ram(
        std::span<const std::uint8_t> bytes) noexcept;
    [[nodiscard]] std::span<const std::uint8_t> cartridge_ram() const noexcept;
    [[nodiscard]] std::vector<ApuPortWrite> take_apu_port_writes();
    [[nodiscard]] std::vector<MsuRegisterWrite> take_msu_register_writes();
    void set_apu_clock_offset(std::uint32_t clocks) noexcept;
    void set_apu_output_ports(
        const std::array<std::uint8_t, 4>& ports) noexcept;
    [[nodiscard]] const SnesPpuState& ppu_state() const noexcept;
    [[nodiscard]] const NativeModelDrawState& native_model_draw() const noexcept;
    void set_native_model_draw(const NativeModelDrawState& state) noexcept;
    [[nodiscard]] const std::vector<std::uint32_t>& unknown_superfx_launches()
        const noexcept;
    [[nodiscard]] std::uint64_t apu_upload_generation() const noexcept;
    void write_cgram(
        std::uint16_t first_colour,
        std::span<const std::uint16_t> colours) noexcept;
    void write_vram(
        std::uint16_t byte_offset,
        std::span<const std::uint8_t> bytes) noexcept;
    void upload_oam(std::uint32_t source, std::size_t length);
    void begin_superfx_bitmap_frame();
    // Submit the source 224x192 Super FX bitmap through FOXIRQ's exact two
    // VRAM transfers and buffer swap. Native front-end text is CPU-drawn
    // into this bitmap even when model geometry is host-rendered.
    void submit_superfx_bitmap();
    // Advance one NTSC gameplay bitmap DMA phase (2 -> 4 -> 6 -> 0).
    // The final phase honors NOIRQBIT3 and uploads 328 OAM bytes. This handles
    // bitmap/OAM/page state only; palette, controller and scroll work retain
    // their existing owners. Returns false when gated or outside these phases.
    [[nodiscard]] bool advance_gameplay_bitmap_dma_phase();
    void set_bg1_scroll(std::int16_t x, std::int16_t y) noexcept;
    void set_bg2_scroll(std::int16_t x, std::int16_t y) noexcept;
    // ENDSEQ's SEQSCROLL runs once per raster, independently of CPU tasks.
    void tick_ending_video_phase();
    void tick_background_video_phase();
    void draw_planet_sphere(std::uint16_t sprite);
    void set_bg2_vertical_offsets_enabled(bool enabled) noexcept;
    void capture_bg2_horizontal_offsets(
        std::uint16_t source, bool enabled) noexcept;

    // Runs a routine entered directly and expected to return with RTL.
    // Returns the number of executed instructions and writes back registers.
    std::size_t call_long(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

    // Runs a same-bank routine entered directly and expected to return with
    // RTS. This is used for source-local screen helpers that were never given
    // a JSL/RTL wrapper.
    std::size_t call_near(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

    // Starts a long-call routine whose source control flow spans multiple
    // presentation frames. Execution pauses before any stop address and can
    // later continue with resume_task(), preserving the complete CPU state.
    Wdc65816TaskResult begin_long_task(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::span<const std::uint32_t> stop_addresses,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

    // Starts a same-bank RTS routine as a resumable task. This is the task
    // counterpart of call_near() and is used by source screen sequences such
    // as END_LEVEL_SEQ that yield once per TRANSFER_L call.
    // A saved_data_bank seeds a prior PHB stack frame when entering a source
    // block after its prologue. Its PLB/RTS still execute normally; synthetic
    // frame setup is excluded from native execution clocks.
    Wdc65816TaskResult begin_near_task(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::span<const std::uint32_t> stop_addresses,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false,
        std::optional<std::uint8_t> saved_data_bank = std::nullopt);

    // Continues the active task. Waiting/stopped tasks advance one polling
    // idle and yield again while halted; a wake resumes ordinary execution.
    // The instruction at the address where the
    // previous call paused is executed before stop addresses are considered
    // again, allowing frame loops to use one stable source label as a yield.
    Wdc65816TaskResult resume_task(
        Wdc65816Registers& registers,
        std::span<const std::uint32_t> stop_addresses,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

private:
    std::size_t call(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit,
        bool service_transfer_flag,
        bool long_return);
    Wdc65816TaskResult run_task(
        Wdc65816Registers& registers,
        std::span<const std::uint32_t> stop_addresses,
        std::size_t instruction_limit,
        bool service_transfer_flag);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::simulation
