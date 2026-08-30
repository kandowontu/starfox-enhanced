#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace starfox::simulation {

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
    void set_bg1_scroll(std::int16_t x, std::int16_t y) noexcept;
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
    Wdc65816TaskResult begin_near_task(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::span<const std::uint32_t> stop_addresses,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

    // Continues the active task. The instruction at the address where the
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
