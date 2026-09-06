#include "starfox/simulation/snes_interrupts.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox::simulation;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

int main() try {
    SnesInterrupts timer;
    InterruptBeam beam{0, 8, 0, 4, 0, 225};
    timer.write_timer(0, 0, beam);
    timer.write_timer(1, 0, beam);
    timer.write_control(0x10);
    timer.clock_step();
    timer.poll(beam); // HTIME=0: compare at H=4, ten clocks in the past.
    require(timer.sample(false) == InterruptRequest{}, "IRQ asserted before the hold delay");
    require(timer.read_irq(0x35) == 0xb5 && timer.read_irq(0x35) == 0xb5,
        "TIMEUP acknowledged during the IRQ hold or lost open-bus bits");
    beam.horizontal_10 = 8;
    timer.poll(beam);
    require(timer.sample(false) == InterruptRequest{false, true, true},
        "IRQ did not become pending after the hold");
    require(timer.read_irq(0x35) == 0xb5 && timer.read_irq(0xff) == 0x7f,
        "TIMEUP did not acknowledge after the hold");
    timer.poll(beam);
    require(timer.sample(false) == InterruptRequest{}, "acknowledged IRQ repeated");
    require(timer.sample(true, true) == InterruptRequest{false, false, true},
        "masked external IRQ must still release WAI");
    require(timer.sample(false, true) == InterruptRequest{false, true, true},
        "timer acknowledgement cleared an external IRQ");

    // Timer writes run the comparator immediately, even between regular polls.
    beam.horizontal_10 = 12;
    timer.write_timer(0, 2, beam);
    require(timer.read_irq(0) == 0x80, "HTIMEL write omitted immediate comparison");
    timer.write_control(0);
    require(timer.read_irq(0) == 0, "disabling IRQ left TIMEUP asserted");

    SnesInterrupts nmi;
    nmi.write_control(0x80);
    nmi.clock_step();
    beam.vertical_2 = 225;
    nmi.poll(beam);
    require(nmi.read_nmi(0x70) == 0xf2 && nmi.read_nmi(0x70) == 0xf2,
        "RDNMI lost its hold, version or open-bus bits");
    require(nmi.sample(false) == InterruptRequest{}, "NMI skipped the hold delay");
    nmi.poll(beam);
    require(nmi.read_nmi(0) == 0x82 && nmi.read_nmi(0) == 2,
        "RDNMI acknowledgement failed");
    require(nmi.sample(true) == InterruptRequest{true, false, true},
        "RDNMI incorrectly cleared a pending edge or I masked NMI");
    nmi.poll(beam);
    require(nmi.sample(false) == InterruptRequest{}, "NMI repeated inside vblank");
    beam.vertical_2 = 0;
    nmi.poll(beam);
    nmi.write_control(0);
    beam.vertical_2 = 225;
    nmi.poll(beam);
    nmi.poll(beam);
    nmi.write_control(0x80);
    require(nmi.sample(false) == InterruptRequest{}, "NMITIMEN write missed the polling lock");
    nmi.clock_step();
    require(nmi.sample(false) == InterruptRequest{true, false, true},
        "enabling NMI during vblank lost its edge");
    nmi.inhibit();
    require(nmi.sample(false, true) == InterruptRequest{}, "DMA lock did not inhibit polling");
    nmi.clock_step();
    require(nmi.sample(false, true).irq, "clock step did not release polling lock");
    nmi.inhibit();
    require(nmi.sample(true, false, true) == InterruptRequest{},
        "external NMI bypassed the polling lock");
    nmi.clock_step();
    require(nmi.sample(true, false, true) == InterruptRequest{true, false, true},
        "external NMI was masked by I after the polling lock cleared");
    std::cout << "Timer/NMI holds, acknowledgements, register writes and CPU polling pass\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
