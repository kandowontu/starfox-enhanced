#include "ares_gsu.hpp"
#include "starfox/simulation/gsu_device.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <tuple>

using starfox::simulation::GsuDevice;
void require(bool value, const char* message) { if (!value) throw std::runtime_error{message}; }

int main() try {
    std::vector<std::uint8_t> rom(0x8000U), source_ram(0x10000U), host_ram(0x10000U);
    starfox::reference::AresGsu reference{rom, source_ram};
    using Event = std::tuple<std::uint64_t,std::uint32_t,std::uint8_t,bool>;
    std::vector<Event> expected_events, actual_events;
    reference.set_bus_observer([&](auto t,auto a,auto v,auto w) { expected_events.emplace_back(t,a,v,w); });
    unsigned cases{};
    for (unsigned opcode = 0; opcode < 256U; ++opcode)
    for (unsigned alt = 0; alt < 4U; ++alt)
    for (bool fast : {false, true}) for (unsigned cfgr : {0U, 0xa0U}) for (unsigned selection : {0U, 1U})
    for (unsigned pattern : {0U, 0x7fffU, 0x8000U}) {
        std::fill(rom.begin(), rom.end(), 0U);
        std::fill(source_ram.begin(), source_ram.end(), 0x17U);
        host_ram = source_ram;
        std::vector<std::uint8_t> program;
        const auto iwt = [&](unsigned reg, unsigned value) {
            program.push_back(0xf0U | reg); program.push_back(value); program.push_back(value >> 8U);
        };
        iwt(0U, 0x0fU); program.push_back(0x4eU); // visible plot colour
        for (unsigned reg = 0; reg < 15U; ++reg) {
            unsigned value = reg < 8U ? (pattern ^ (reg * 17U)) : 0x8100U;
            if (reg == 12U) value = 1U; // LOOP exits
            if (reg == 0U && opcode >= 0x98U && opcode <= 0x9dU) value = 0x8100U;
            if (reg == 3U && opcode >= 0x98U && opcode <= 0x9dU) value = 0x8100U;
            iwt(reg,value);
        }
        program.push_back(selection ? 0xb3U : 0xb0U);
        program.push_back(selection ? 0x15U : 0x10U);
        if (alt) program.push_back(0x3cU + alt);
        program.push_back(opcode);
        // Zero branch displacement; immediate words lead jumps out of the
        // setup code to zero-filled ROM, where STOP terminates either path.
        program.push_back(opcode == 0xafU || opcode == 0xffU ? 0x81U : 0U); program.push_back(0x81U);
        program.push_back(0U); program.push_back(0U);
        std::copy(program.begin(), program.end(), rom.begin());
        expected_events.clear(); actual_events.clear();
        starfox::reference::GsuRun expected;
        try { expected = reference.run(0x8000U, 0U, 0U, 10000U, fast, cfgr, true); }
        catch (const std::exception& error) { throw std::runtime_error{std::string{error.what()} + " opcode " + std::to_string(opcode) + " alt " + std::to_string(alt) + " case " + std::to_string(cases)}; }
        GsuDevice device{rom, host_ram};
        device.set_bus_observer([&](auto t,auto a,auto v,auto w) { actual_events.emplace_back(t,a,v,w); });
        device.write_io(0x3037U, cfgr);
        device.write_io(0x303aU, 0x39U); device.write_io(0x3038U, 0x10U);
        device.write_io(0x3039U, fast); device.write_io(0x301eU, 0U); device.write_io(0x301fU, 0x80U);
        // Vary how the caller partitions its work. No instruction is restarted
        // when the device yields in a cache fill, buffered write or operand.
        const unsigned quantum = std::array{1U,2U,6U,8U,12U,37U}[cases % 6U];
        for (std::uint64_t time = quantum; device.running() && time < 100000U; time += quantum)
            device.run_until(time);
        bool different = device.running() || device.instructions() != expected.instructions
            || device.last_stop_master_clock() != expected.stop_master_clocks || host_ram != source_ram || actual_events != expected_events;
        for (unsigned reg = 0; reg < 16U; ++reg) {
            const auto value = device.read_io(0x3000U + reg * 2U)
                | (unsigned(device.read_io(0x3001U + reg * 2U)) << 8U);
            different |= value != expected.registers[reg];
        }
        const auto status = device.read_io(0x3030U) | (unsigned(device.read_io(0x3031U)) << 8U);
        different |= device.last_stop_status() != expected.status;
        different |= (status & ~0x40U) != (expected.status & ~0x40U);
        if (different) throw std::runtime_error{"GSU mismatch opcode " + std::to_string(opcode)
            + " alt " + std::to_string(alt) + " case " + std::to_string(cases)
            + " clocks " + std::to_string(device.last_stop_master_clock()) + "/" + std::to_string(expected.stop_master_clocks)
            + " instructions " + std::to_string(device.instructions()) + "/" + std::to_string(expected.instructions)};
        ++cases;
    }
    {
        std::fill(rom.begin(), rom.end(), 0U);
        GsuDevice device{rom, host_ram};
        device.write_io(0x303aU, 8U); // RAM enabled, ROM blocked
        device.write_io(0x301fU, 0x80U);
        device.run_until(100U);
        require(device.running() && device.instructions() == 0U, "GSU did not yield while ROM was unavailable");
        require(device.read_cpu_ram(0U, 0x5aU) == 0x5aU, "CPU read bypassed GSU RAM ownership");
        device.write_cpu_ram(0U, 0xa5U);
        require(host_ram[0] == 0xa5U, "GSU ownership incorrectly blocked CPU RAM writes");
        device.write_io(0x303aU, 0x18U);
        require(device.read_cpu_rom(4U) == 4U, "CPU ROM ownership pattern differs");
        device.run_until(200U);
        require(!device.running() && device.instructions() == 2U && device.irq(), "GSU failed to resume its interrupted ROM read");
        require((device.read_io(0x3031U) & 0x80U) && !device.irq(), "GSU IRQ acknowledgement differs");
        bool rejected{};
        try { device.run_until(199U); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "GSU accepted a backwards deadline");
    }
    std::cout << cases << " GSU opcode/ALT/state/clock/RAM/bus-event cases; zero differences; bus-wait/resume checks passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
