#include "ares_cpu.hpp"
#include "starfox/simulation/bcd_arithmetic.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

using namespace starfox;
namespace {
struct Operation { unsigned opcode, size; }; // size 0/5 denotes M/X immediate
auto state(const simulation::Wdc65816Registers& r) {
    return std::tuple{r.a, r.x, r.y, r.direct, r.stack, r.data_bank, r.status};
}
template<class Word> void check_decimal(reference::AresCpu& reference, Word a, Word b, bool carry) {
    for (const bool subtract : {false,true}) {
        simulation::Wdc65816Registers regs;
        regs.a = a;
        regs.status = 0x0cU | (sizeof(Word) == 1 ? 0x20U : 0U) | carry;
        reference.decimal(regs, b, subtract);
        const auto result = subtract
            ? simulation::decimal_arithmetic<Word, true>(a,b,carry)
            : simulation::decimal_arithmetic<Word, false>(a,b,carry);
        const auto flags = (result.carry ? 1U : 0U) | (result.overflow ? 0x40U : 0U)
            | (result.value == 0 ? 2U : 0U)
            | ((result.value >> (sizeof(Word)*8U-1U)) ? 0x80U : 0U);
        if (regs.a != result.value || (regs.status & 0xc3U) != flags)
            throw std::runtime_error("Decimal oracle mismatch: a=" + std::to_string(a)
                + " b=" + std::to_string(b) + " carry=" + std::to_string(carry)
                + " subtract=" + std::to_string(subtract));
    }
}
}

int main(int argc, char** argv) try {
    std::ofstream file;
    if (argc == 2) file.open(argv[1]);
    auto& out = file.is_open() ? file : std::cout;
    out << "opcode,status,direct,fast,registers_equal,memory_equal,port_clocks,ares_clocks,index,bus_equal,port_bus_steps,ares_bus_steps\n";
    std::vector<Operation> ops;
    for (unsigned group = 0; group < 8; ++group) {
        for (const auto mode : std::array<Operation, 15>{{
                {1,2},{3,2},{5,2},{7,2},{9,0},{13,3},{15,4},
                {17,2},{18,2},{19,2},{21,2},{23,2},{25,3},{29,3},{31,4}}}) {
            if (group == 4 && mode.opcode == 9) continue;
            ops.push_back({group * 32 + mode.opcode, mode.size});
        }
    }
    for (unsigned group : {0U, 0x20U, 0x40U, 0x60U, 0xc0U, 0xe0U})
        for (const auto mode : std::array<Operation, 4>{{{6,2},{14,3},{22,2},{30,3}}})
            ops.push_back({group + mode.opcode, mode.size});
    for (const auto mode : std::array<Operation, 44>{{
        {0x04,2},{0x0c,3},{0x14,2},{0x1c,3},
        {0x24,2},{0x2c,3},{0x34,2},{0x3c,3},{0x89,0},
        {0x64,2},{0x74,2},{0x9c,3},{0x9e,3},
        {0xa2,5},{0xa6,2},{0xae,3},{0xb6,2},{0xbe,3},
        {0xa0,5},{0xa4,2},{0xac,3},{0xb4,2},{0xbc,3},
        {0x86,2},{0x96,2},{0x8e,3},{0x84,2},{0x94,2},{0x8c,3},
        {0xc0,5},{0xc4,2},{0xcc,3},{0xe0,5},{0xe4,2},{0xec,3},
        {0xc2,2},{0xe2,2},{0x82,3},
        {0x10,2},{0x30,2},{0x50,2},{0x70,2},{0x90,2},{0xb0,2}}}) ops.push_back(mode);
    for (const auto opcode : {0x0a,0x18,0x1a,0x2a,0x38,0x3a,0x4a,0x58,0x6a,
            0x78,0x88,0x8a,0x98,0x9a,0x9b,0xa8,0xaa,0xb8,0xba,0xbb,0xc8,
            0xca,0xd8,0xe8,0xea,0xeb,0xf8,0x1b,0x3b,0x5b,0x7b,
            0x08,0x0b,0x28,0x2b,0x48,0x4b,0x5a,0x68,0x7a,0x8b,0xab,0xda,0xfa})
        ops.push_back({static_cast<unsigned>(opcode),1});
    for (const auto mode : std::array<Operation, 23>{{
        {0x00,2},{0x02,2},{0x20,3},{0x22,4},{0x40,1},{0x42,2},
        {0x44,3},{0x4c,3},{0x54,3},{0x5c,4},{0x60,1},{0x62,3},
        {0x6b,1},{0x6c,3},{0x7c,3},{0x80,2},{0xd0,2},{0xd4,2},
        {0xdc,3},{0xf0,2},{0xf4,3},{0xfb,1},{0xfc,3}}})
        if (std::none_of(ops.begin(),ops.end(),[&](auto item){return item.opcode == mode.opcode;}))
            ops.push_back(mode);
    unsigned failures{}, cases{}, functional{}, timing{};
    const std::array direct_indexed_ops{0x01U,0x21U,0x41U,0x61U,0x81U,0xa1U,0xc1U,0xe1U,
        0x15U,0x16U,0x34U,0x35U,0x36U,
        0x55U,0x56U,0x74U,0x75U,0x76U,0x94U,0x95U,0x96U,0xb4U,0xb5U,
        0xb6U,0xd5U,0xd6U,0xf5U,0xf6U};
    for (const auto op : ops) for (unsigned flags : {0x04U,0x05U,0x0cU,0x0dU,
            0x14U,0x24U,0x34U,0xc5U,0x2cU,0x2dU,0x3cU,0x3dU}) for (unsigned direct : {0x1000U,0x1001U})
        for (const auto index : (std::find(direct_indexed_ops.begin(), direct_indexed_ops.end(), op.opcode)
                != direct_indexed_ops.end() ? std::vector<unsigned>{7,0xdf,0xe0,0xff,0x100,0x7ff,0xffff}
                                           : std::vector<unsigned>{7}))
        for (const bool fast : {false,true}) {
        const auto size = op.size == 0 ? (flags & 0x20U ? 2U : 3U)
            : op.size == 5 ? (flags & 0x10U ? 2U : 3U) : op.size;
        std::vector<std::uint8_t> bytes(0x8000U);
        bytes[0] = static_cast<std::uint8_t>(op.opcode);
        bytes[1] = 0x20; bytes[2] = 0x10; bytes[3] = 0x7e;
        if ((op.opcode & 0x1fU) == 0x10U || op.opcode == 0x82 || op.opcode == 0x80) {
            bytes[1] = 0; bytes[2] = 0; // branch to following instruction
        }
        const auto entry = fast ? 0x808000U : 0x008000U;
        auto stop = entry + size;
        if (op.opcode == 0x20 || op.opcode == 0x4c) {
            bytes[1] = 0x38; bytes[2] = 0x80; stop = entry + 0x38;
        }
        if (op.opcode == 0x22 || op.opcode == 0x5c) {
            bytes[1] = 0x38; bytes[2] = 0x80; bytes[3] = 1; stop = 0x018038;
        }
        if (op.opcode == 0x7c || op.opcode == 0xfc) {
            bytes[1] = 0x10; bytes[2] = 0x80;
            bytes[0x17] = 0x38; bytes[0x18] = 0x80; stop = entry + 0x38;
        }
        if (op.opcode == 0x6c) stop = entry + 0x38;
        if (op.opcode == 0xdc) stop = 0x018038;
        if (op.opcode == 0x60) stop = (entry & 0xff0000U) | 0x01f0;
        if (op.opcode == 0x6b) stop = 0x7e01f0;
        if (op.opcode == 0x40) stop = 0x017e01;
        if (op.opcode == 0x00 || op.opcode == 0x02) {
            bytes[0x7fe4] = bytes[0x7fe6] = 0x38;
            bytes[0x7fe5] = bytes[0x7fe7] = 0x80;
            stop = 0x008038;
        }
        if (op.opcode == 0x44 || op.opcode == 0x54) bytes[1] = bytes[2] = 0x7e;
        const assets::RomImage rom{std::move(bytes)};
        simulation::Wdc65816 port{rom}, memory{rom};
        for (unsigned i = 0; i < 0x2000U; ++i) {
            const auto data = static_cast<std::uint8_t>((i * 37U + i / 7U) & 255U);
            port.write8(i, data); memory.write8(i, data);
        }
        // Every indirect mode used here points to mapped WRAM.
        for (auto* cpu : {&port,&memory}) {
            for (unsigned base : {direct + 0x20U, direct + 0x27U, 0x21cU}) {
                cpu->write16(base, 0x1020);
                cpu->write8(base + 2, 0x7e);
            }
            if ((op.opcode & 0x1fU) == 1U) {
                const auto base = static_cast<std::uint16_t>(direct + 0x20U
                    + (index & (flags & 0x10U ? 0xffU : 0xffffU)));
                cpu->write16(base, 0x1020U);
            }
            cpu->write8(0x00420d, fast);
            cpu->write8(0x0200, 1); // bank pulled by the isolated RTI case
            if (op.opcode == 0x6c || op.opcode == 0xdc) {
                cpu->write16(0x1020, 0x8038);
                cpu->write8(0x1022, 1);
            }
        }
        simulation::Wdc65816Registers regs;
        regs.a = 0x8799; regs.x = 7; regs.y = 9;
        if (index != 7) regs.x = regs.y = static_cast<std::uint16_t>(
            index & (flags & 0x10U ? 0xffU : 0xffffU));
        if (op.opcode == 0x44 || op.opcode == 0x54) regs.a = 2;
        regs.status = static_cast<std::uint8_t>(flags);
        regs.direct = static_cast<std::uint16_t>(direct);
        regs.data_bank = 0x7e;
        auto reference_regs = regs;
        std::vector<std::uint32_t> port_bus, reference_bus;
        port.set_bus_clock_callback([&](std::uint32_t clocks) { port_bus.push_back(clocks); });
        const std::array stops{stop};
        const auto actual = port.begin_long_task(entry, regs, stops, 100);
        reference::AresCpu reference{memory};
        reference.set_bus_clock_callback([&](std::uint32_t clocks) { reference_bus.push_back(clocks); });
        const auto expected = reference.run(entry, reference_regs, stops[0], 100, fast);
        bool registers_equal = state(regs) == state(reference_regs);
        bool memory_equal = true;
        for (unsigned address = 0; address < 0x20000U; ++address)
            if (port.read8(0x7e0000U + address) != memory.read8(0x7e0000U + address)) {
                memory_equal = false;
                break;
            }
        const bool bus_equal = port_bus == reference_bus;
        const bool clocks_equal = port.executed_master_clocks() == expected.master_clocks && bus_equal;
        ++cases;
        if (!registers_equal || !memory_equal) ++functional;
        if (!clocks_equal) ++timing;
        if (!registers_equal || !memory_equal || !clocks_equal) ++failures;
        out << op.opcode << ',' << flags << ',' << direct << ',' << fast << ','
            << registers_equal << ',' << memory_equal << ','
            << port.executed_master_clocks() << ',' << expected.master_clocks << ',' << index << ',' << bus_equal;
        for (const auto* steps : {&port_bus, &reference_bus}) {
            out << ',';
            for (const auto clocks : *steps) out << clocks << '|';
        }
        out << '\n';
        if (actual.instructions != expected.instructions)
            throw std::runtime_error("CPU audit did not compare the same instruction count");
    }
    std::cerr << cases << " CPU cases; " << failures << " differences ("
        << functional << " state, " << timing << " timing)\n";
    const assets::RomImage decimal_rom{std::vector<std::uint8_t>(0x8000U)};
    simulation::Wdc65816 decimal_memory{decimal_rom};
    reference::AresCpu decimal_reference{decimal_memory};
    for (unsigned a = 0; a < 256; ++a) for (unsigned b = 0; b < 256; ++b)
        for (const bool carry : {false,true})
            check_decimal(decimal_reference, static_cast<std::uint8_t>(a),
                static_cast<std::uint8_t>(b), carry);
    std::uint32_t random = 0x65816;
    for (unsigned a = 0; a < 65536; ++a) {
        random = random * 1664525U + 1013904223U;
        for (const bool carry : {false,true})
            check_decimal(decimal_reference, static_cast<std::uint16_t>(a),
                static_cast<std::uint16_t>(random >> 16U), carry);
    }
    std::cerr << "524288 decimal ALU cases agree, including all 8-bit inputs\n";
    simulation::Wdc65816 arithmetic_port{decimal_rom}, arithmetic_memory{decimal_rom};
    reference::AresCpu arithmetic_reference{arithmetic_memory};
    unsigned arithmetic_cases = 0;
    for (const bool word : {false,true}) for (unsigned a = 0; a < (word ? 65536U : 256U); ++a) {
        random = random * 1664525U + 1013904223U;
        for (unsigned iteration = 0; iteration < (word ? 1U : 256U); ++iteration) {
            const auto b = word ? (random >> 16U) : iteration;
            for (const bool decimal : {false,true}) for (const bool carry : {false,true})
                for (const bool subtract : {false,true}) {
                for (auto* cpu : {&arithmetic_port,&arithmetic_memory}) {
                    cpu->write8(0x001000, subtract ? 0xe9 : 0x69);
                    cpu->write8(0x001001, static_cast<std::uint8_t>(b));
                    cpu->write8(0x001002, word ? static_cast<std::uint8_t>(b >> 8U) : 0x6b);
                    cpu->write8(0x001003, 0x6b);
                }
                simulation::Wdc65816Registers actual_regs;
                actual_regs.a = static_cast<std::uint16_t>(word ? a : 0xab00U | a);
                actual_regs.status = 4U | (word ? 0U : 0x20U)
                    | (decimal ? 8U : 0U) | carry;
                auto expected_regs = actual_regs;
                const auto before = arithmetic_port.executed_master_clocks();
                arithmetic_port.call_long(0x001000U, actual_regs);
                const auto expected = arithmetic_reference.run(0x001000U, expected_regs, 0x7e01f0U);
                if (state(actual_regs) != state(expected_regs)
                        || arithmetic_port.executed_master_clocks() - before != expected.master_clocks)
                    throw std::runtime_error("Native arithmetic mismatch: a=" + std::to_string(a)
                        + " b=" + std::to_string(b) + " word=" + std::to_string(word)
                        + " decimal=" + std::to_string(decimal) + " carry=" + std::to_string(carry)
                        + " subtract=" + std::to_string(subtract));
                ++arithmetic_cases;
            }
        }
    }
    std::cerr << arithmetic_cases << " native ADC/SBC routines agree, including all 8-bit inputs\n";
    return failures ? 1 : 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
