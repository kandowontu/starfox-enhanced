#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/snes_ppu.hpp"

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

// Project-owned adapter around the pinned MIT RetroCPU core. It supplies the
// SNES LoROM/WRAM address map and bounded native-mode subroutine execution.
class Wdc65816 {
public:
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
    [[nodiscard]] std::vector<ApuPortWrite> take_apu_port_writes();
    void set_apu_clock_offset(std::uint32_t clocks) noexcept;
    [[nodiscard]] const SnesPpuState& ppu_state() const noexcept;
    [[nodiscard]] const std::vector<std::uint32_t>& unknown_superfx_launches()
        const noexcept;
    void write_cgram(
        std::uint16_t first_colour,
        std::span<const std::uint16_t> colours) noexcept;
    void write_vram(
        std::uint16_t byte_offset,
        std::span<const std::uint8_t> bytes) noexcept;
    void upload_oam(std::uint32_t source, std::size_t length);
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

private:
    std::size_t call(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit,
        bool service_transfer_flag,
        bool long_return);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::simulation
