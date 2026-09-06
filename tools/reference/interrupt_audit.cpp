#include "ares_cpu.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <tuple>

using namespace starfox;
namespace {
auto state(const simulation::Wdc65816Registers& r) {
    return std::tuple{r.a,r.x,r.y,r.direct,r.stack,r.data_bank,r.status};
}
}

int main(int argc, char** argv) try {
    std::ofstream file;
    if (argc == 2) file.open(argv[1]);
    if (argc == 2 && !file) throw std::runtime_error("Cannot create interrupt audit CSV");
    auto& out = file.is_open() ? file : std::cout;
    out << "nmi,status,pc,stack,fast,registers_equal,memory_equal,port_clocks,ares_clocks,bus_equal,status_bus_equal\n";
    std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
    bytes[0x7fea] = 0x60; bytes[0x7feb] = 0x80;
    bytes[0x7fee] = 0x40; bytes[0x7fef] = 0x80;
    const assets::RomImage rom{std::move(bytes)};
    unsigned cases=0, failures=0;
    for (bool nmi : {false,true}) for (unsigned flags=0; flags<256; ++flags) {
        if (!nmi && (flags & 4U)) continue; // IRQ is masked; covered by the line-state test.
        for (auto entry : {0x008000U,0x808000U,0x7e9000U})
            for (auto stack : {0x01ffU,0x0002U,0xfffeU}) for (bool fast : {false,true}) {
            simulation::Wdc65816 port{rom}, memory{rom};
            for (auto* cpu : {&port,&memory}) {
                cpu->write8(0x7e9000,0xea);
                cpu->write8(0x420d,fast);
                for (unsigned p=0; p<0x2000; ++p) cpu->write8(p,static_cast<std::uint8_t>(p*37U+13U));
            }
            simulation::Wdc65816Registers regs;
            regs.a=0xa75b; regs.x=(flags&0x10U)?0x67U:0x8967U;
            regs.y=(flags&0x10U)?0x45U:0xab45U;
            regs.direct=0x4567; regs.data_bank=0x7f;
            regs.stack=static_cast<std::uint16_t>(stack); regs.status=static_cast<std::uint8_t>(flags);
            auto expected_regs=regs;
            std::vector<std::uint32_t> port_bus, reference_bus;
            std::vector<std::uint8_t> port_status, reference_status;
            port.set_bus_clock_callback([&](std::uint32_t clocks) {
                port_bus.push_back(clocks);
                port_status.push_back(port.status_register());
            });
            if (nmi) port.pulse_nmi(); else port.set_irq_line(true);
            const auto handler=nmi?0x008060U:0x008040U;
            const std::array stops{handler};
            const auto task=port.begin_long_task(entry,regs,stops,1);
            reference::AresCpu reference{memory};
            reference.set_bus_clock_callback([&](std::uint32_t clocks) {
                reference_bus.push_back(clocks);
                reference_status.push_back(reference.status_register());
            });
            const auto expected=reference.enter_interrupt(entry,expected_regs,nmi?0xffeaU:0xffeeU,fast);
            bool registers_equal=state(regs)==state(expected_regs)
                && task.stop_address==expected.program_address
                && !task.returned && task.instructions==0 && port.interrupts_taken()==1;
            bool memory_equal=true;
            for (unsigned p=0; p<0x2000; ++p)
                if (port.read8(p)!=memory.read8(p)) {memory_equal=false;break;}
            const bool bus_equal=port_bus==reference_bus;
            const bool status_bus_equal=port_status==reference_status;
            const bool clock_equal=port.executed_master_clocks()==expected.master_clocks && bus_equal;
            failures += !registers_equal || !memory_equal || !clock_equal || !status_bus_equal;
            ++cases;
            out << nmi << ',' << flags << ',' << entry << ',' << stack << ',' << fast << ','
                << registers_equal << ',' << memory_equal << ',' << port.executed_master_clocks()
                << ',' << expected.master_clocks << ',' << bus_equal << ',' << status_bus_equal << '\n';
        }
    }
    out.flush();
    if (!out) throw std::runtime_error("Cannot write interrupt audit CSV");
    std::cerr << cases << " native interrupt entries; " << failures << " differences\n";
    return failures?1:0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
