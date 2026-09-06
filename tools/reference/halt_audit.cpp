#include "ares_cpu.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <array>
#include <iostream>
#include <tuple>

using namespace starfox;
auto state(const simulation::Wdc65816Registers& r) {
    return std::tuple{r.a,r.x,r.y,r.direct,r.stack,r.data_bank,r.status};
}
int main() try {
    unsigned cases{};
    for (const bool stop : {false,true}) for (const bool masked : {false,true})
    for (const bool fast : {false,true}) for (unsigned schedule = 0; schedule < 10; ++schedule) {
        const auto entry = fast ? 0x808000U : 0x8000U;
        const auto fetch_clocks = fast ? 6U : 8U;
        const std::array idle_counts{1U,2U,5U,20U};
        const std::array wake_clocks{8U,9U,14U,15U,20U,32U};
        const auto wake = schedule < 4 ? std::optional<std::uint64_t>{}
            : std::optional<std::uint64_t>{wake_clocks[schedule - 4]};
        const auto budget = schedule < 4 ? fetch_clocks + 6U * idle_counts[schedule] : 200U;
        std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
        bytes[0] = stop ? 0xdbU : 0xcbU;
        const assets::RomImage rom{std::move(bytes)};
        simulation::Wdc65816 port{rom}, memory{rom};
        port.write8(0x420dU, fast);
        port.set_cpu_timeline(std::make_shared<simulation::SnesCpuTimeline>());
        std::vector<std::uint32_t> actual_bus, expected_bus;
        std::vector<simulation::Wdc65816InterruptSample> actual_samples, expected_samples;
        std::uint64_t clocks{};
        port.set_bus_clock_callback([&](std::uint32_t count) {
            clocks += count;
            actual_bus.push_back(count);
            if (wake && clocks >= *wake) port.set_irq_line(true);
        });
        port.set_interrupt_sample_callback([&](const simulation::Wdc65816InterruptSample& sample) {
            actual_samples.push_back(sample); return false;
        });
        reference::AresCpu source{memory};
        source.set_bus_clock_callback([&](std::uint32_t count) { expected_bus.push_back(count); });
        source.set_interrupt_sample_callback([&](const simulation::Wdc65816InterruptSample& sample) {
            expected_samples.push_back(sample); return false;
        });
        simulation::Wdc65816Registers actual;
        actual.status = masked ? 4U : 0U;
        auto expected = actual;
        const auto reference_run = source.run_halt(entry, expected, budget, wake, fast);
        const std::array stops{entry + 1U};
        auto task = port.begin_long_task(entry, actual, stops, 1);
        if (task.instructions != 1U) throw std::runtime_error{"halt opcode was not counted once"};
        while ((task.waiting || task.stopped) && port.executed_master_clocks() < budget) {
            task = port.resume_task(actual, stops, 1);
            if (task.instructions != 0U) throw std::runtime_error{"halt idle was counted as an instruction"};
        }
        if (task.waiting != reference_run.waiting || task.stopped != reference_run.stopped
                || port.executed_master_clocks() != reference_run.master_clocks
                || actual_bus != expected_bus || actual_samples != expected_samples
                || state(actual) != state(expected))
            throw std::runtime_error{"halt differential mismatch: stop=" + std::to_string(stop)
                + " masked=" + std::to_string(masked) + " fast=" + std::to_string(fast)
                + " schedule=" + std::to_string(schedule)};
        ++cases;
    }
    std::cout << cases << " native WAI/STP cases; zero state, bus or sampling differences\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
