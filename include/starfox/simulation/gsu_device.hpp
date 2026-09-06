#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace starfox::simulation {

// Resumable GSU device for the accurate machine timeline. The caller owns
// ROM/RAM and advances this device before CPU accesses to shared devices.
class GsuDevice {
public:
    GsuDevice(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram,
        std::span<std::uint8_t> additional_ram = {});
    ~GsuDevice();
    GsuDevice(const GsuDevice&) = delete;
    GsuDevice& operator=(const GsuDevice&) = delete;

    using BusObserver = std::function<void(std::uint64_t, std::uint32_t, std::uint8_t, bool)>;
    // Observation only: callbacks must not reenter or replace this device.
    void set_bus_observer(BusObserver observer);
    void run_until(std::uint64_t master_clock);
    [[nodiscard]] std::uint64_t master_clock() const noexcept;
    [[nodiscard]] std::uint64_t last_stop_master_clock() const noexcept;
    [[nodiscard]] std::uint16_t last_stop_status() const noexcept;
    [[nodiscard]] std::uint64_t instructions() const noexcept;
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool owns_rom() const noexcept;
    [[nodiscard]] bool owns_ram() const noexcept;
    [[nodiscard]] std::uint32_t ram_size() const noexcept;
    [[nodiscard]] bool irq() const noexcept;
    [[nodiscard]] std::uint32_t pending_ram_clocks() const noexcept;
    std::uint8_t read_io(std::uint32_t address);
    void write_io(std::uint32_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t read_cpu_rom(std::uint32_t offset) const;
    [[nodiscard]] std::uint8_t read_cpu_ram(std::uint32_t offset, std::uint8_t open_bus) const;
    void write_cpu_ram(std::uint32_t offset, std::uint8_t value);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::simulation
