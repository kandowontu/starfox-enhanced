#include "starfox/simulation/gsu_device.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

using starfox::simulation::GsuDevice;
void require(bool value, const char* message) { if (!value) throw std::runtime_error{message}; }
void launch(GsuDevice& device, unsigned address = 0x8000U) {
    device.write_io(0x301eU,address); device.write_io(0x301fU,address >> 8U);
}
void finish(GsuDevice& device) {
    for (unsigned guard = 0; guard < 10000U && device.running(); ++guard)
        device.run_until(device.master_clock() + 1U);
    require(!device.running(), "GSU did not finish within the fixture budget");
}
int main() try {
    std::vector<std::uint8_t> rom(0x8000U, 0x01U), ram(0x10000U);
    const std::array<std::uint8_t,3> cache_program{0x02U,0x01U,0U};
    std::copy(cache_program.begin(),cache_program.end(),rom.begin());
    for (bool fast : {false,true}) {
        GsuDevice device{rom,ram};
        device.write_io(0x303aU,0x39U); device.write_io(0x3039U,fast);
        launch(device); finish(device);
        require(device.instructions() == 4U && device.last_stop_master_clock() == (fast ? 91U : 110U),
            "cold cache/pipeline timing differs");
        require(device.irq() && (device.read_io(0x3031U) & 0x80U) && !device.irq(),
            "STOP did not raise or acknowledge IRQ");
        const auto warm_start = device.master_clock();
        launch(device); finish(device);
        require(device.instructions() == 8U && device.last_stop_master_clock() - warm_start == (fast ? 4U : 8U),
            "a new launch discarded the warm opcode cache");
        static_cast<void>(device.read_io(0x3031U));
        device.write_io(0x3030U,0x20U); device.write_io(0x3030U,0U);
        require(device.read_io(0x303eU) == 0U && device.read_io(0x303fU) == 0U,
            "CPU stop did not reset CBR");
        device.write_io(0x3037U,0xa0U);
        const auto flushed_start = device.master_clock();
        launch(device); finish(device);
        require(device.last_stop_master_clock() - flushed_start == (fast ? 91U : 110U) && !device.irq(),
            "CPU stop failed to flush cache or IRQ masking differs");
    }
    {
        const std::array<std::uint8_t,9> program{0xf0U,0xcdU,0xabU,0xf1U,0U,0x10U,0x31U,0x01U,0U};
        std::copy(program.begin(),program.end(),rom.begin());
        GsuDevice device{rom,ram};
        device.write_io(0x303aU,0x10U); // GSU RAM bus is unavailable
        launch(device); device.run_until(300U);
        require(device.running() && ram[0x1000U] == 0U && ram[0x1001U] == 0U,
            "buffered store ignored unavailable RAM");
        device.write_cpu_ram(0x1000U,0x55U);
        require(device.read_cpu_ram(0x1000U,0xaaU) == 0x55U,
            "CPU cannot access RAM released by the GSU");
        device.write_io(0x303aU,0x18U);
        finish(device);
        require(ram[0x1000U] == 0xcdU && ram[0x1001U] == 0xabU,
            "resumed buffered store lost its captured bytes");
    }
    {
        rom[0] = 0U;
        GsuDevice device{rom,ram};
        device.write_io(0x303aU,0x18U);
        unsigned observed{}, rejected{};
        device.set_bus_observer([&](auto,auto,auto,auto) {
            ++observed;
            try { device.run_until(1000U); } catch (const std::logic_error&) { ++rejected; }
            try { device.set_bus_observer({}); } catch (const std::logic_error&) { ++rejected; }
            try { device.write_io(0x3030U,0U); } catch (const std::logic_error&) { ++rejected; }
        });
        launch(device); finish(device);
        require(observed == 2U && rejected == 6U, "GSU observer allowed reentry or callback replacement");
        device.set_bus_observer([](auto,auto,auto,auto) { throw std::runtime_error{"observer failure"}; });
        launch(device);
        for (unsigned attempt = 0; attempt < 2U; ++attempt) {
            bool failed{};
            try { device.run_until(device.master_clock() + 100U); }
            catch (const std::runtime_error&) { failed = true; }
            require(failed, "failed GSU coroutine was resumed or swallowed its error");
        }
    }
    for (unsigned repeat = 0; repeat < 32U; ++repeat) {
        GsuDevice blocked{rom,ram};
        launch(blocked); blocked.run_until(1000U); // destroy a nested ROM wait
        require(blocked.running() && blocked.instructions() == 0U, "unavailable ROM did not remain suspended");
    }
    std::cout << "GSU warm cache, CPU stop, IRQ, RAM wait, observer guards and suspended destruction passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
