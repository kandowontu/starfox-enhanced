#include "starfox/simulation/wdc65816.hpp"

#include "starfox/assets/decrunch.hpp"

#include "cpu/65816/cpu_65c816.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace starfox::simulation {
namespace {

constexpr std::uint32_t kAddressSpaceSize = 1U << 24U;
constexpr std::uint32_t kPageBits = 12U;
constexpr std::uint32_t kPageSize = 1U << kPageBits;
constexpr std::uint32_t kPageCount = kAddressSpaceSize / kPageSize;
constexpr std::uint32_t kBootstrap = 0x7e0100U;
constexpr std::uint32_t kReturnSentinel = 0x7e01f0U;
constexpr std::uint32_t kDmaTargetScanline = 0x1760U;
constexpr std::uint32_t kSuperFxRamBase = 0x700000U;
constexpr std::uint32_t kSuperFxRamSize = 0x10000U;
constexpr std::uint32_t kMEndData = 0x700062U;
constexpr std::uint32_t kMEndDataBank = 0x700064U;
constexpr std::uint32_t kMDecrunchAddress = 0x70002cU;
constexpr std::uint32_t kMDecrunchEnd = 0x70005aU;
constexpr std::uint32_t kMDecrunchOffset = 0x700090U;
constexpr std::uint16_t kDecrunchBuffer = 0x7800U;
constexpr std::uint16_t kScreenDecrunchBuffer = kDecrunchBuffer + 6144U;
constexpr std::uint16_t kVram1Address = 0x190fU;
constexpr std::uint16_t kVram1Length = 0x1911U;
constexpr std::uint16_t kVram2Address = 0x1913U;
constexpr std::uint16_t kVram2Length = 0x1915U;
constexpr std::uint16_t kVram3Address = 0x1917U;
constexpr std::uint16_t kVram3Length = 0x191aU;
constexpr std::uint32_t kGamePalette = 0x7e81efU;

std::uint32_t find_rom_symbol(
    const assets::SymbolMap* symbols, const char* name) noexcept {
    if (symbols == nullptr) return 0U;
    for (const auto address : symbols->find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x70U) {
            return address;
        }
    }
    return 0U;
}

std::uint32_t find_symbol(
    const assets::SymbolMap* symbols, const char* name) noexcept {
    if (symbols == nullptr) return 0U;
    const auto addresses = symbols->find(name);
    return addresses.empty() ? 0U : addresses.front();
}

} // namespace

struct Wdc65816::Impl {
    const assets::RomImage* rom{};
    SystemBus bus{};
    std::vector<Page> pages{static_cast<std::size_t>(kPageCount)};
    std::vector<std::uint8_t> wram = std::vector<std::uint8_t>(0x20000U);
    std::array<std::uint8_t, 2> controller{};
    std::array<std::uint8_t, 4> apu_ports{0xaaU, 0xbbU, 0U, 0U};
    bool apu_output_connected{};
    bool apu_upload_active{};
    std::uint8_t apu_upload_clear_sequence{};
    std::array<std::uint8_t, 0x300> superfx_registers{};
    std::vector<std::uint8_t> superfx_ram =
        std::vector<std::uint8_t>(kSuperFxRamSize);
    std::array<std::uint8_t, 0x40> ppu_registers{};
    std::array<std::uint8_t, 0x80> dma_registers{};
    SnesPpuState ppu{};
    std::uint16_t vram_address{};
    std::uint8_t cgram_address{};
    bool cgram_high_byte{};
    std::uint8_t background_scroll_low{};
    bool background_scroll_high_byte{};
    std::uint16_t oam_address{};
    bool oam_high_byte{};
    std::uint32_t mdecrunch{};
    std::uint32_t mcallarctan16{};
    std::uint32_t arctantab{};
    std::uint32_t m_cnt{};
    std::uint32_t minitdust{};
    std::uint32_t m_dustpnts{};
    std::uint32_t m_rand{};
    std::uint32_t mcrotwmatzxy16{};
    std::uint32_t mwmatrotp16{};
    std::uint32_t sintab16{};
    std::uint32_t m_rotx{};
    std::uint32_t m_roty{};
    std::uint32_t m_rotz{};
    std::uint32_t m_wmat11{};
    std::uint32_t mclrmapscreen{};
    std::uint32_t mdrawsprite32{};
    std::uint32_t musprite{};
    std::uint32_t mdrawsphere{};
    std::uint32_t mcalc_circle{};
    std::uint32_t mcopyface{};
    std::uint32_t mfprintstr{};
    std::uint32_t msprintstr{};
    std::uint32_t mshowteammate2{};
    std::uint32_t textureaddrtab{};
    std::uint32_t bitmap1{};
    std::uint32_t msprite{};
    std::uint32_t mspr_pal{};
    std::uint32_t m_xc{};
    std::uint32_t m_yc{};
    std::uint32_t m_radius{};
    std::uint32_t m_sprsize{};
    std::uint32_t m_sprxscale{};
    std::uint32_t m_bigx{};
    std::uint32_t m_bigy{};
    std::uint32_t m_bigz{};
    std::uint32_t m_lxpos{};
    std::uint32_t m_lypos{};
    std::uint32_t m_lzpos{};
    std::uint32_t m_scale{};
    std::uint32_t planetdma{};
    std::uint32_t vmap2{};
    bool vertical_counter_high_byte{};
    bool horizontal_counter_high_byte{};
    std::uint32_t wram_port_address{};
    std::uint8_t multiply_a{};
    std::uint16_t divide_dividend{};
    std::uint16_t divide_quotient{};
    std::uint16_t multiply_result{};
    std::uint32_t mrotplanet{};
    std::uint32_t mnograd{};
    std::uint32_t mtunnelgrad{};
    std::uint32_t mwibbletunnel{};
    std::uint32_t mbhole{};
    std::uint32_t bg_scrollbuffer{};
    std::uint32_t m_x1{};
    std::uint32_t m_viewposx{};
    std::uint32_t m_y1{};
    std::uint32_t m_z1{};
    std::uint32_t m_scrollxoff{};
    std::uint32_t m_sineoffset{};
    std::uint32_t testk{};
    std::uint32_t testk2{};
    std::uint32_t testk3{};
    std::uint32_t testk4{};
    std::uint32_t watersinetab{};
    std::uint32_t watersinetabend{};
    std::uint32_t wsctab{};
    std::uint32_t bholetab{};
    std::uint32_t bholetabend{};
    std::vector<ApuPortWrite> apu_writes;
    std::uint64_t apu_upload_generation{};
    std::vector<std::uint32_t> unknown_superfx_launches;
    std::uint32_t apu_clock_offset{};
    WDC65C816 cpu{&bus};

    static bool is_io_device_address(void*, cpuaddr_t address) {
        const auto low = address & 0xffffU;
        return (low & 0xfffeU) == 0x4218U || (low & 0xfffcU) == 0x2140U
            || (low >= 0x2100U && low < 0x2140U)
            || (low >= 0x4202U && low <= 0x4206U)
            || (low >= 0x4214U && low <= 0x4217U)
            || low == 0x420bU || low == 0x420cU
            || (low >= 0x2180U && low <= 0x2183U)
            || (low >= 0x4300U && low < 0x4380U)
            || (low >= 0x3000U && low < 0x3300U);
    }

    static void read_io(void* context, cpuaddr_t address, std::uint8_t* data, std::uint32_t) {
        auto& self = *static_cast<Impl*>(context);
        const auto low = address & 0xffffU;
        if ((low & 0xfffeU) == 0x4218U) {
            *data = self.controller[address & 1U];
        } else if ((low & 0xfffcU) == 0x2140U) {
            // Model the SPC boot-ROM acknowledgement protocol: it initially
            // exposes $BBAA and then echoes CPU port writes after each byte.
            *data = self.apu_ports[address & 3U];
        } else if (low == 0x2137U) {
            // Reading SLHV latches the PPU counters and resets OPVCT's
            // low/high read phase. Bounded original routines use WAITDMA_L
            // only as a hardware synchronization barrier, so expose the
            // scanline they requested in DMATEMP.
            self.vertical_counter_high_byte = false;
            self.horizontal_counter_high_byte = false;
        } else if (low == 0x213cU) {
            *data = self.horizontal_counter_high_byte ? 0U : 95U;
            self.horizontal_counter_high_byte = !self.horizontal_counter_high_byte;
        } else if (low == 0x213dU) {
            *data = self.vertical_counter_high_byte
                ? 0U : self.wram[kDmaTargetScanline];
            self.vertical_counter_high_byte = !self.vertical_counter_high_byte;
        } else if (low == 0x2180U) {
            *data = self.wram[self.wram_port_address & 0x1ffffU];
            self.wram_port_address = (self.wram_port_address + 1U) & 0x1ffffU;
        } else if (low >= 0x2181U && low <= 0x2183U) {
            *data = static_cast<std::uint8_t>(
                self.wram_port_address >> ((low - 0x2181U) * 8U));
        } else if (low >= 0x2100U && low < 0x2140U) {
            *data = self.ppu_registers[low - 0x2100U];
        } else if (low >= 0x4300U && low < 0x4380U) {
            *data = self.dma_registers[low - 0x4300U];
        } else if (low == 0x4214U || low == 0x4215U) {
            *data = static_cast<std::uint8_t>(
                self.divide_quotient >> ((low - 0x4214U) * 8U));
        } else if (low == 0x4216U || low == 0x4217U) {
            *data = static_cast<std::uint8_t>(
                self.multiply_result >> ((low - 0x4216U) * 8U));
        } else if (low >= 0x3000U && low < 0x3300U) {
            // The geometry processor is currently represented as an
            // immediately completing coprocessor. In particular SFR's GO
            // flag ($3030 bit 5) is clear when the 65C816 polls it.
            *data = self.superfx_registers[low - 0x3000U];
        }
    }

    static void write_io(
        void* context, cpuaddr_t address, const std::uint8_t* data, std::uint32_t) {
        auto& self = *static_cast<Impl*>(context);
        const auto low = address & 0xffffU;
        if ((low & 0xfffeU) == 0x4218U) {
            self.controller[address & 1U] = *data;
        } else if ((low & 0xfffcU) == 0x2140U) {
            const auto port = static_cast<std::uint8_t>(address & 3U);
            if (port == 0U && *data == 0xffU && !self.apu_upload_active) {
                // sbootapu sends $ff while the driver is idle to restart the
                // SPC boot ROM before replacing the level's sound bank. The
                // other CPU ports still contain live engine/effect values at
                // that point; the driver, not the 65C816, clears them.
                self.apu_ports = {0xaaU, 0xbbU, 0U, 0U};
                self.apu_upload_active = true;
                self.apu_upload_clear_sequence = 0U;
                ++self.apu_upload_generation;
            } else if (self.apu_upload_active || !self.apu_output_connected) {
                // During an IPL transfer, each CPU write is synchronously
                // echoed by the boot ROM. Before an external SPC core is
                // attached, retain that mirror for standalone CPU tests.
                // Once attached, normal driver reads must come from the
                // SPC700's output latch rather than the CPU's own input.
                self.apu_ports[port] = *data;
            }
            if (self.apu_upload_active) {
                if (port == 1U && *data == 0U) {
                    self.apu_upload_clear_sequence = 1U;
                } else if (self.apu_upload_clear_sequence == 1U
                           && port == 2U && *data == 0U) {
                    self.apu_upload_clear_sequence = 2U;
                } else if (self.apu_upload_clear_sequence == 2U
                           && port == 3U && *data == 0U) {
                    // SBOOTAPU clears ports 1, 2, and 3 in exactly this order
                    // after starting the downloaded driver.
                    self.apu_upload_active = false;
                    self.apu_upload_clear_sequence = 0U;
                } else if (port != 1U) {
                    self.apu_upload_clear_sequence = 0U;
                }
            }
            self.apu_writes.push_back({
                port, *data, self.apu_clock_offset});
        } else if (low >= 0x2100U && low < 0x2140U) {
            self.write_ppu(static_cast<std::uint16_t>(low), *data);
        } else if (low == 0x2180U) {
            self.wram[self.wram_port_address & 0x1ffffU] = *data;
            self.wram_port_address = (self.wram_port_address + 1U) & 0x1ffffU;
        } else if (low == 0x2181U) {
            self.wram_port_address = (self.wram_port_address & 0x1ff00U) | *data;
        } else if (low == 0x2182U) {
            self.wram_port_address = (self.wram_port_address & 0x100ffU)
                | (static_cast<std::uint32_t>(*data) << 8U);
        } else if (low == 0x2183U) {
            self.wram_port_address = (self.wram_port_address & 0x0ffffU)
                | (static_cast<std::uint32_t>(*data & 1U) << 16U);
        } else if (low >= 0x4300U && low < 0x4380U) {
            self.dma_registers[low - 0x4300U] = *data;
        } else if (low == 0x420bU) {
            self.run_dma(*data);
        } else if (low == 0x4202U) {
            self.multiply_a = *data;
        } else if (low == 0x4203U) {
            self.multiply_result = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(self.multiply_a) * *data);
        } else if (low == 0x4204U) {
            self.divide_dividend = static_cast<std::uint16_t>(
                (self.divide_dividend & 0xff00U) | *data);
        } else if (low == 0x4205U) {
            self.divide_dividend = static_cast<std::uint16_t>(
                (self.divide_dividend & 0x00ffU)
                | (static_cast<std::uint16_t>(*data) << 8U));
        } else if (low == 0x4206U) {
            if (*data == 0U) {
                self.divide_quotient = 0xffffU;
                self.multiply_result = self.divide_dividend;
            } else {
                self.divide_quotient = static_cast<std::uint16_t>(
                    self.divide_dividend / *data);
                self.multiply_result = static_cast<std::uint16_t>(
                    self.divide_dividend % *data);
            }
        } else if (low >= 0x3000U && low < 0x3300U) {
            self.superfx_registers[low - 0x3000U] = *data;
            if (low == 0x301fU) self.launch_superfx();
        }
    }

    static void irq_taken(void*, std::uint32_t) {}

    explicit Impl(const assets::RomImage& rom_image, const assets::SymbolMap* symbols)
        : rom(&rom_image),
          mdecrunch(find_rom_symbol(symbols, "MDECRUNCH")),
          mcallarctan16(find_rom_symbol(symbols, "MCALLARCTAN16")),
          arctantab(find_rom_symbol(symbols, "ARCTANTAB")),
          m_cnt(find_symbol(symbols, "M_CNT")),
          minitdust(find_rom_symbol(symbols, "MINITDUST")),
          m_dustpnts(find_symbol(symbols, "M_DUSTPNTS")),
          m_rand(find_symbol(symbols, "M_RAND")),
          mcrotwmatzxy16(find_rom_symbol(symbols, "MCROTWMATZXY16")),
          mwmatrotp16(find_rom_symbol(symbols, "MWMATROTP16")),
          sintab16(find_rom_symbol(symbols, "SINTAB16")),
          m_rotx(find_symbol(symbols, "M_ROTX")),
          m_roty(find_symbol(symbols, "M_ROTY")),
          m_rotz(find_symbol(symbols, "M_ROTZ")),
          m_wmat11(find_symbol(symbols, "M_WMAT11")),
          mclrmapscreen(find_rom_symbol(symbols, "MCLRMAPSCREEN")),
          mdrawsprite32(find_rom_symbol(symbols, "MDRAWSPRITE32")),
          musprite(find_rom_symbol(symbols, "MUSPRITE")),
          mdrawsphere(find_rom_symbol(symbols, "MDRAWSPHERE")),
          mcalc_circle(find_rom_symbol(symbols, "MCALC_CIRCLE")),
          mcopyface(find_rom_symbol(symbols, "MCOPYFACE")),
          mfprintstr(find_rom_symbol(symbols, "MFPRINTSTR")),
          msprintstr(find_rom_symbol(symbols, "MSPRINTSTR")),
          mshowteammate2(find_rom_symbol(symbols, "MSHOWTEAMMATE2")),
          textureaddrtab(find_rom_symbol(symbols, "TEXTUREADDRTAB")),
          bitmap1(find_symbol(symbols, "BITMAP1")),
          msprite(find_symbol(symbols, "MSPRITE")),
          mspr_pal(find_symbol(symbols, "MSPR_PAL")),
          m_xc(find_symbol(symbols, "M_XC")),
          m_yc(find_symbol(symbols, "M_YC")),
          m_radius(find_symbol(symbols, "M_RADIUS")),
          m_sprsize(find_symbol(symbols, "M_SPRSIZE")),
          m_sprxscale(find_symbol(symbols, "M_SPRXSCALE")),
          m_bigx(find_symbol(symbols, "M_BIGX")),
          m_bigy(find_symbol(symbols, "M_BIGY")),
          m_bigz(find_symbol(symbols, "M_BIGZ")),
          m_lxpos(find_symbol(symbols, "M_LXPOS")),
          m_lypos(find_symbol(symbols, "M_LYPOS")),
          m_lzpos(find_symbol(symbols, "M_LZPOS")),
          m_scale(find_symbol(symbols, "M_SCALE")),
          planetdma(find_symbol(symbols, "PLANETDMA")),
          vmap2(find_symbol(symbols, "VMAP2")),
          mrotplanet(find_rom_symbol(symbols, "MROTPLANET")),
          mnograd(find_rom_symbol(symbols, "MNOGRAD")),
          mtunnelgrad(find_rom_symbol(symbols, "MTUNNELGRAD")),
          mwibbletunnel(find_rom_symbol(symbols, "MWIBBLETUNNEL")),
          mbhole(find_rom_symbol(symbols, "MBHOLE")),
          bg_scrollbuffer(find_symbol(symbols, "BG_SCROLLBUFFER")),
          m_x1(find_symbol(symbols, "M_X1")),
          m_viewposx(find_symbol(symbols, "M_VIEWPOSX")),
          m_y1(find_symbol(symbols, "M_Y1")),
          m_z1(find_symbol(symbols, "M_Z1")),
          m_scrollxoff(find_symbol(symbols, "M_SCROLLXOFF")),
          m_sineoffset(find_symbol(symbols, "M_SINEOFFSET")),
          testk(find_symbol(symbols, "TESTK")),
          testk2(find_symbol(symbols, "TESTK2")),
          testk3(find_symbol(symbols, "TESTK3")),
          testk4(find_symbol(symbols, "TESTK4")),
          watersinetab(find_rom_symbol(symbols, "WATERSINETAB")),
          watersinetabend(find_rom_symbol(symbols, "WATERSINETABEND")),
          wsctab(find_rom_symbol(symbols, "WSCTAB")),
          bholetab(find_rom_symbol(symbols, "BHOLETAB")),
          bholetabend(find_rom_symbol(symbols, "BHOLETABEND")) {
        for (auto& page : pages) {
            page.ptr = nullptr;
            page.flags = 0;
            page.io_mask = 0;
            page.io_eq = 1;
            page.cycles_per_access = 1;
        }
        bus.Init(kPageBits, 24, pages.data());
        bus.open_bus_is_data = true;
        bus.io_devices = {
            &is_io_device_address,
            &read_io,
            &write_io,
            &irq_taken,
            this,
        };

        bus.Map(0x7e0000U, wram.data(), static_cast<std::uint32_t>(wram.size()));
        for (std::uint32_t bank = 0; bank < 0x40U; ++bank) {
            bus.Map(bank << 16U, wram.data(), 0x2000U);
            bus.Map((bank | 0x80U) << 16U, wram.data(), 0x2000U);
            auto& low_io_page = pages[((bank << 16U) | 0x4000U) >> kPageBits];
            low_io_page.io_mask = 0xf000U;
            low_io_page.io_eq = 0x4000U;
            auto& high_io_page = pages[(((bank | 0x80U) << 16U) | 0x4000U) >> kPageBits];
            high_io_page.io_mask = 0xf000U;
            high_io_page.io_eq = 0x4000U;
            auto& low_apu_page = pages[((bank << 16U) | 0x2000U) >> kPageBits];
            low_apu_page.io_mask = 0xfe00U;
            low_apu_page.io_eq = 0x2000U;
            auto& high_apu_page = pages[(((bank | 0x80U) << 16U) | 0x2000U) >> kPageBits];
            high_apu_page.io_mask = 0xfe00U;
            high_apu_page.io_eq = 0x2000U;
            auto& low_superfx_page = pages[((bank << 16U) | 0x3000U) >> kPageBits];
            low_superfx_page.io_mask = 0xfc00U;
            low_superfx_page.io_eq = 0x3000U;
            auto& high_superfx_page = pages[(((bank | 0x80U) << 16U) | 0x3000U) >> kPageBits];
            high_superfx_page.io_mask = 0xfc00U;
            high_superfx_page.io_eq = 0x3000U;
        }

        auto* rom_bytes = const_cast<std::uint8_t*>(rom_image.bytes().data());
        const auto banks = static_cast<std::uint32_t>(rom_image.size() / 0x8000U);
        for (std::uint32_t bank = 0; bank < banks && bank < 0x7eU; ++bank) {
            auto* bank_data = rom_bytes + bank * 0x8000U;
            bus.Map((bank << 16U) | 0x8000U, bank_data, 0x8000U, true);
            bus.Map(((bank | 0x80U) << 16U) | 0x8000U, bank_data, 0x8000U, true);
        }

        // Star Fox's GSU work RAM is CPU-visible in bank $70. This must be
        // mapped after LoROM so $70:8000-$ffff is RAM rather than cartridge.
        bus.Map(kSuperFxRamBase, superfx_ram.data(), kSuperFxRamSize);

        // Enter native mode through the architectural XCE instruction so the
        // third-party core's private emulation flag changes normally.
        write8(kBootstrap + 0U, 0x18U); // CLC
        write8(kBootstrap + 1U, 0xfbU); // XCE
        cpu.PowerOn();
        cpu.SetRegister("pc", kBootstrap);
        cpu.SingleStep();
        cpu.SingleStep();
        write8(kReturnSentinel, 0xeaU); // NOP; execution stops before this byte.
    }

    std::uint8_t read8(std::uint32_t address) const {
        return const_cast<SystemBus&>(bus).ReadByte(address);
    }

    void write8(std::uint32_t address, std::uint8_t value) {
        bus.WriteByte(address, value);
    }

    std::uint16_t read_wram16(std::uint16_t address) const noexcept {
        return static_cast<std::uint16_t>(wram[address])
            | (static_cast<std::uint16_t>(wram[address + 1U]) << 8U);
    }

    std::uint16_t read_superfx16(std::uint32_t address) const noexcept {
        const auto offset = static_cast<std::uint16_t>(address);
        return static_cast<std::uint16_t>(superfx_ram[offset])
            | (static_cast<std::uint16_t>(superfx_ram[
                   static_cast<std::uint16_t>(offset + 1U)]) << 8U);
    }

    void write_superfx16(std::uint32_t address, std::uint16_t value) noexcept {
        const auto offset = static_cast<std::uint16_t>(address);
        superfx_ram[offset] = static_cast<std::uint8_t>(value);
        superfx_ram[static_cast<std::uint16_t>(offset + 1U)] =
            static_cast<std::uint8_t>(value >> 8U);
    }

    void write_cgram_byte(std::uint16_t byte_address, std::uint8_t value) noexcept {
        byte_address &= 0x1ffU;
        auto& word = ppu.cgram[byte_address >> 1U];
        if ((byte_address & 1U) == 0U) {
            word = static_cast<std::uint16_t>((word & 0xff00U) | value);
        } else {
            word = static_cast<std::uint16_t>((word & 0x00ffU)
                | (static_cast<std::uint16_t>(value) << 8U));
        }
    }

    void write_ppu(std::uint16_t address, std::uint8_t value) noexcept {
        ppu_registers[address - 0x2100U] = value;
        switch (address) {
        case 0x2101U:
            ppu.object_select = value;
            break;
        case 0x2102U:
            oam_address = static_cast<std::uint16_t>((oam_address & 0x0100U) | value);
            oam_high_byte = false;
            break;
        case 0x2103U:
            oam_address = static_cast<std::uint16_t>((oam_address & 0x00ffU)
                | (static_cast<std::uint16_t>(value & 1U) << 8U));
            oam_high_byte = false;
            break;
        case 0x2104U: {
            const auto byte_address = static_cast<std::size_t>(oam_address) * 2U;
            ppu.oam[(byte_address + (oam_high_byte ? 1U : 0U)) % ppu.oam.size()] = value;
            if (oam_high_byte) {
                oam_address = static_cast<std::uint16_t>((oam_address + 1U) & 0x01ffU);
            }
            oam_high_byte = !oam_high_byte;
            break;
        }
        case 0x2105U:
            ppu.background_mode = static_cast<std::uint8_t>(value & 7U);
            ppu.bg3_high_priority = (value & 0x08U) != 0U;
            break;
        case 0x2107U:
            ppu.bg1_screen_base = static_cast<std::uint16_t>(value & 0xfcU) << 8U;
            ppu.bg1_screen_size = static_cast<std::uint8_t>(value & 3U);
            break;
        case 0x2108U:
            ppu.bg2_screen_base = static_cast<std::uint16_t>(value & 0xfcU) << 8U;
            ppu.bg2_screen_size = static_cast<std::uint8_t>(value & 3U);
            break;
        case 0x2109U:
            ppu.bg3_screen_base = static_cast<std::uint16_t>(value & 0xfcU) << 8U;
            ppu.bg3_screen_size = static_cast<std::uint8_t>(value & 3U);
            break;
        case 0x210bU:
            ppu.bg1_character_base = static_cast<std::uint16_t>(value & 0x0fU) << 12U;
            ppu.bg2_character_base = static_cast<std::uint16_t>(value >> 4U) << 12U;
            break;
        case 0x210cU:
            ppu.bg3_character_base = static_cast<std::uint16_t>(value & 0x0fU) << 12U;
            break;
        case 0x210dU:
        case 0x210eU:
        case 0x2111U:
        case 0x2112U:
            if (!background_scroll_high_byte) {
                background_scroll_low = value;
            } else {
                const auto scroll = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(background_scroll_low)
                    | (static_cast<std::uint16_t>(value) << 8U));
                if (address == 0x210dU) ppu.bg1_scroll_x = scroll;
                else if (address == 0x210eU) ppu.bg1_scroll_y = scroll;
                else if (address == 0x2111U) ppu.bg3_scroll_x = scroll;
                else ppu.bg3_scroll_y = scroll;
            }
            background_scroll_high_byte = !background_scroll_high_byte;
            break;
        case 0x2116U:
            vram_address = static_cast<std::uint16_t>((vram_address & 0xff00U) | value);
            break;
        case 0x2117U:
            vram_address = static_cast<std::uint16_t>((vram_address & 0x00ffU)
                | (static_cast<std::uint16_t>(value) << 8U));
            break;
        case 0x2118U:
            ppu.vram[(static_cast<std::uint32_t>(vram_address) * 2U) & 0xffffU] = value;
            if ((ppu_registers[0x15U] & 0x80U) == 0U) ++vram_address;
            break;
        case 0x2119U:
            ppu.vram[(static_cast<std::uint32_t>(vram_address) * 2U + 1U) & 0xffffU]
                = value;
            if ((ppu_registers[0x15U] & 0x80U) != 0U) ++vram_address;
            break;
        case 0x2121U:
            cgram_address = value;
            cgram_high_byte = false;
            break;
        case 0x2122U: {
            const auto byte_address = static_cast<std::uint16_t>(cgram_address) * 2U
                + (cgram_high_byte ? 1U : 0U);
            write_cgram_byte(byte_address, value);
            if (cgram_high_byte) ++cgram_address;
            cgram_high_byte = !cgram_high_byte;
            break;
        }
        case 0x212cU:
            ppu.main_screen = value;
            break;
        default:
            break;
        }
    }

    void write_bbus(std::uint16_t address, std::uint8_t value) noexcept {
        if (address >= 0x2100U && address < 0x2140U) {
            write_ppu(address, value);
        } else if (address == 0x2180U) {
            wram[wram_port_address & 0x1ffffU] = value;
            wram_port_address = (wram_port_address + 1U) & 0x1ffffU;
        }
    }

    void run_dma(std::uint8_t enabled_channels) {
        static constexpr std::array<std::array<std::uint8_t, 4>, 8> patterns{{
            {{0, 0, 0, 0}}, {{0, 1, 0, 1}}, {{0, 0, 0, 0}}, {{0, 0, 1, 1}},
            {{0, 1, 2, 3}}, {{0, 1, 0, 1}}, {{0, 0, 0, 0}}, {{0, 0, 1, 1}},
        }};
        static constexpr std::array<std::uint8_t, 8> pattern_lengths{
            1, 2, 2, 4, 4, 4, 2, 4};
        for (std::uint32_t channel = 0; channel < 8U; ++channel) {
            if ((enabled_channels & (1U << channel)) == 0U) continue;
            const auto base = channel * 16U;
            const auto parameters = dma_registers[base];
            auto source = static_cast<std::uint32_t>(dma_registers[base + 2U])
                | (static_cast<std::uint32_t>(dma_registers[base + 3U]) << 8U)
                | (static_cast<std::uint32_t>(dma_registers[base + 4U]) << 16U);
            auto length = static_cast<std::uint32_t>(dma_registers[base + 5U])
                | (static_cast<std::uint32_t>(dma_registers[base + 6U]) << 8U);
            if (length == 0U) length = 0x10000U;
            const auto mode = static_cast<std::uint8_t>(parameters & 7U);
            const auto ppu_base = static_cast<std::uint16_t>(
                0x2100U + dma_registers[base + 1U]);
            const auto decrement = (parameters & 0x10U) != 0U;
            const auto fixed = (parameters & 0x08U) != 0U;
            for (std::uint32_t index = 0; index < length; ++index) {
                const auto ppu_address = static_cast<std::uint16_t>(ppu_base
                    + patterns[mode][index % pattern_lengths[mode]]);
                if ((parameters & 0x80U) == 0U) {
                    write_bbus(ppu_address, bus.ReadByte(source));
                }
                if (!fixed) source = decrement ? source - 1U : source + 1U;
            }
            dma_registers[base + 2U] = static_cast<std::uint8_t>(source);
            dma_registers[base + 3U] = static_cast<std::uint8_t>(source >> 8U);
            dma_registers[base + 5U] = 0U;
            dma_registers[base + 6U] = 0U;
        }
    }

    static std::int16_t signed16(std::uint16_t value) noexcept {
        return std::bit_cast<std::int16_t>(value);
    }

    static std::int16_t arithmetic_shift_right(
        std::int16_t value, unsigned shift) noexcept {
        const auto wide = static_cast<std::int32_t>(value);
        if (wide >= 0) return static_cast<std::int16_t>(wide >> shift);
        const auto magnitude = -wide;
        return static_cast<std::int16_t>(
            -((magnitude + (1L << shift) - 1L) >> shift));
    }

    void write_horizontal_offset(std::size_t line, std::uint16_t value) noexcept {
        const auto address = static_cast<std::uint16_t>(
            bg_scrollbuffer + static_cast<std::uint32_t>(line * 3U));
        superfx_ram[address] = 1U;
        superfx_ram[static_cast<std::uint16_t>(address + 1U)] =
            static_cast<std::uint8_t>(value);
        superfx_ram[static_cast<std::uint16_t>(address + 2U)] =
            static_cast<std::uint8_t>(value >> 8U);
    }

    void generate_constant_horizontal_offsets() noexcept {
        auto value = arithmetic_shift_right(
            signed16(read_superfx16(m_viewposx)), 3U);
        value = signed16(static_cast<std::uint16_t>(
            static_cast<std::int32_t>(value) + 128
            + signed16(read_superfx16(m_scrollxoff))));
        for (std::size_t line = 0; line < 224U; ++line) {
            write_horizontal_offset(line, static_cast<std::uint16_t>(value));
        }
    }

    void generate_rotating_horizontal_offsets() noexcept {
        const auto roll = read_superfx16(m_viewposx);
        const auto gradient = arithmetic_shift_right(
            signed16(static_cast<std::uint16_t>(~roll)), 7U);
        const auto integer_step = arithmetic_shift_right(gradient, 8U);
        const auto fractional_step = static_cast<std::uint16_t>(gradient) & 0xffU;
        auto fraction = std::uint16_t{};
        auto displacement = std::uint16_t{};
        const auto base = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(arithmetic_shift_right(
                signed16(read_superfx16(m_y1)), 3U))
            + signed16(read_superfx16(m_scrollxoff)));

        for (std::size_t line = 0; line < 112U; ++line) {
            const auto fraction_sum = static_cast<std::uint32_t>(fraction)
                + (static_cast<std::uint32_t>(fractional_step) << 8U);
            fraction = static_cast<std::uint16_t>(fraction_sum);
            displacement = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(displacement) + integer_step
                + (fraction_sum > 0xffffU ? 1 : 0));

            // MHOFS.MC deliberately uses NOT/ADD for the lower half. That
            // yields base-displacement-1, including its one-pixel centre
            // asymmetry, while the upper half is base+displacement.
            write_horizontal_offset(112U + line, static_cast<std::uint16_t>(
                base + static_cast<std::uint16_t>(~displacement)));
            write_horizontal_offset(111U - line, static_cast<std::uint16_t>(
                base + displacement));
        }
    }

    static std::uint16_t tunnel_gradient(std::uint16_t position) noexcept {
        const auto quarter = arithmetic_shift_right(
            arithmetic_shift_right(signed16(position), 1U), 1U);
        return static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(position) * 4U
            + static_cast<std::int32_t>(quarter));
    }

    template <typename AdjustLower>
    void generate_symmetric_gradient_offsets(
        std::uint16_t gradient, std::uint16_t base, AdjustLower&& adjust_lower) {
        const auto integer_step = arithmetic_shift_right(signed16(gradient), 8U);
        const auto fractional_step = gradient & 0xffU;
        auto fraction = std::uint16_t{};
        auto displacement = std::uint16_t{};
        for (std::size_t index = 0; index < 112U; ++index) {
            const auto fraction_sum = static_cast<std::uint32_t>(fraction)
                + (static_cast<std::uint32_t>(fractional_step) << 8U);
            fraction = static_cast<std::uint16_t>(fraction_sum);
            displacement = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(displacement) + integer_step
                + (fraction_sum > 0xffffU ? 1 : 0));
            const auto value = static_cast<std::uint16_t>(base + displacement);
            write_horizontal_offset(111U - index, value);
            write_horizontal_offset(112U + index,
                adjust_lower(index, value));
        }
    }

    void generate_tunnel_horizontal_offsets() {
        generate_symmetric_gradient_offsets(
            tunnel_gradient(read_superfx16(m_viewposx)), 128U,
            [](std::size_t, std::uint16_t value) { return value; });
    }

    void generate_water_horizontal_offsets() {
        auto sine_offset = static_cast<std::int16_t>(
            read_superfx16(m_sineoffset) - 1U);
        const auto sine_length = static_cast<std::uint16_t>(
            watersinetabend - watersinetab);
        if (sine_offset < 0) {
            sine_offset = static_cast<std::int16_t>(sine_offset + sine_length);
        }
        write_superfx16(m_sineoffset, static_cast<std::uint16_t>(sine_offset));

        auto sine_address = watersinetab + static_cast<std::uint16_t>(sine_offset);
        auto sine = static_cast<std::int8_t>(rom->read8(sine_address));
        auto until_next_sine = std::int16_t{};
        generate_symmetric_gradient_offsets(
            tunnel_gradient(read_superfx16(m_viewposx)), 128U,
            [&](std::size_t index, std::uint16_t value) {
                --until_next_sine;
                if (until_next_sine < 0) {
                    ++sine_address;
                    sine = static_cast<std::int8_t>(rom->read8(sine_address));
                    until_next_sine = static_cast<std::int16_t>(index >> 3U);
                }
                auto adjusted_sine = static_cast<std::int16_t>(sine);
                const auto lines_remaining = static_cast<std::uint32_t>(112U - index);
                const auto scale = rom->read8(wsctab + lines_remaining);
                for (std::uint8_t shift = 0; shift < scale; ++shift) {
                    adjusted_sine = arithmetic_shift_right(adjusted_sine, 1U);
                }
                return static_cast<std::uint16_t>(
                    value + static_cast<std::uint16_t>(adjusted_sine));
            });
    }

    void generate_black_hole_horizontal_offsets() {
        auto countdown = static_cast<std::uint16_t>(read_superfx16(testk3) - 1U);
        write_superfx16(testk3, countdown);
        if (countdown == 0U) {
            write_superfx16(testk4, static_cast<std::uint16_t>(
                -static_cast<std::int32_t>(signed16(read_superfx16(testk4)))));
            write_superfx16(testk3, 0x00a0U * 2U);
        }
        const auto phase_step = read_superfx16(testk4);
        const auto gradient_source = static_cast<std::uint16_t>(
            read_superfx16(testk2) + phase_step);
        write_superfx16(testk2, gradient_source);

        const auto table_length = static_cast<std::uint16_t>(bholetabend - bholetab);
        auto table_phase = static_cast<std::uint16_t>(read_superfx16(testk) + 3U);
        if (table_phase >= table_length) {
            table_phase = static_cast<std::uint16_t>(table_phase - table_length);
        }
        write_superfx16(testk, table_phase);
        auto table_address = bholetab + table_phase;
        const auto scroll = read_superfx16(m_scrollxoff);
        generate_symmetric_gradient_offsets(
            tunnel_gradient(gradient_source), 512U,
            [&](std::size_t, std::uint16_t value) {
                const auto wobble = static_cast<std::int8_t>(rom->read8(table_address++));
                return static_cast<std::uint16_t>(
                    value + scroll + static_cast<std::int16_t>(wobble));
            });

        // Both halves use the table-adjusted value in MBHOLE; mirror the
        // generated lower records back over the upper half.
        for (std::size_t index = 0; index < 112U; ++index) {
            const auto lower = static_cast<std::uint16_t>(bg_scrollbuffer
                + static_cast<std::uint32_t>((112U + index) * 3U));
            const auto value = static_cast<std::uint16_t>(superfx_ram[
                static_cast<std::uint16_t>(lower + 1U)])
                | (static_cast<std::uint16_t>(superfx_ram[
                       static_cast<std::uint16_t>(lower + 2U)]) << 8U);
            write_horizontal_offset(111U - index, value);
        }
    }

    void calculate_arctangent16() {
        const auto absolute_word = [](std::int16_t value) {
            const auto word = static_cast<std::uint16_t>(value);
            return value < 0 ? static_cast<std::uint16_t>(0U - word) : word;
        };
        const auto x = signed16(read_superfx16(m_x1));
        const auto y = signed16(read_superfx16(m_y1));
        const auto x_magnitude = absolute_word(x);
        const auto y_magnitude = absolute_word(y);
        std::uint16_t angle{};
        if (y_magnitude == 0U) {
            angle = 0x4000U;
        } else if (x_magnitude == y_magnitude) {
            angle = 0x2000U;
        } else {
            const auto y_dominant = y_magnitude > x_magnitude;
            const auto minor = std::min(x_magnitude, y_magnitude);
            const auto major = std::max(x_magnitude, y_magnitude);
            const auto ratio = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(minor) << 14U) / major);
            const auto table_offset = static_cast<std::uint16_t>(
                (ratio >> 5U) & 0xfffeU);
            angle = rom->read16(arctantab + table_offset);
            if (!y_dominant) {
                angle = static_cast<std::uint16_t>(0x4000U - angle);
            }
        }
        if ((x < 0) != (y < 0)) {
            angle = static_cast<std::uint16_t>(0U - angle);
        }
        if (y < 0) angle = static_cast<std::uint16_t>(angle + 0x8000U);
        write_superfx16(m_cnt, angle);
    }

    static std::uint16_t next_dust_random(
        std::uint16_t& random, bool& carry) noexcept {
        const auto swapped = static_cast<std::uint16_t>(
            (random << 8U) | (random >> 8U));
        const auto rotated = static_cast<std::uint16_t>(
            (carry ? 0x8000U : 0U) | (swapped >> 1U));
        carry = (swapped & 1U) != 0U;
        const auto first = static_cast<std::uint32_t>(rotated) + random;
        carry = first > 0xffffU;
        const auto second = static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(first)) + random + (carry ? 1U : 0U);
        carry = second > 0xffffU;
        random = static_cast<std::uint16_t>(second + 1U);
        return random;
    }

    void initialize_dust() noexcept {
        constexpr std::size_t maximum_dust = 120U;
        auto random = std::uint16_t{0x19f8U};
        auto carry = false;
        write_superfx16(m_rand, random);
        auto pointer = static_cast<std::uint16_t>(m_dustpnts);
        for (std::size_t point = 0; point < maximum_dust; ++point) {
            for (std::size_t axis = 0; axis < 3U; ++axis) {
                write_superfx16(pointer, next_dust_random(random, carry));
                pointer = static_cast<std::uint16_t>(pointer + 2U);
            }
        }
    }

    std::int16_t sine_q15(std::uint16_t angle) const {
        const auto index = static_cast<std::uint8_t>(angle >> 8U);
        const auto fraction = static_cast<std::uint8_t>(angle);
        const auto current = rom->read_i16(sintab16
            + static_cast<std::uint32_t>(index) * 2U);
        const auto next = rom->read_i16(sintab16
            + static_cast<std::uint32_t>(static_cast<std::uint8_t>(index + 1U)) * 2U);
        const auto difference = static_cast<std::int32_t>(next) - current;
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::int32_t>(current)
            + arithmetic_shift_right32(difference * fraction, 8U)));
    }

    static std::int32_t arithmetic_shift_right32(
        std::int32_t value, unsigned shift) noexcept {
        if (value >= 0) return value >> shift;
        return static_cast<std::int32_t>(-((
            -static_cast<std::int64_t>(value) + (std::int64_t{1} << shift) - 1)
            >> shift));
    }

    static std::int16_t multiply_q15_exact(
        std::int16_t left, std::int16_t right) noexcept {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(
            arithmetic_shift_right32(static_cast<std::int32_t>(left) * right, 15U)));
    }

    static std::int16_t add_word(std::int16_t left, std::int16_t right) noexcept {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(left))
            + static_cast<std::uint16_t>(right)));
    }

    static std::int16_t subtract_word(
        std::int16_t left, std::int16_t right) noexcept {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(left))
            - static_cast<std::uint16_t>(right)));
    }

    void calculate_world_matrix() {
        const auto x = read_superfx16(m_rotx);
        const auto y = read_superfx16(m_roty);
        const auto z = read_superfx16(m_rotz);
        const auto sx = sine_q15(x);
        const auto cx = sine_q15(static_cast<std::uint16_t>(x + 0x4000U));
        const auto sy = sine_q15(y);
        const auto cy = sine_q15(static_cast<std::uint16_t>(y + 0x4000U));
        const auto sz = sine_q15(z);
        const auto cz = sine_q15(static_cast<std::uint16_t>(z + 0x4000U));
        const auto t1 = multiply_q15_exact(cz, sy);
        const auto t2 = multiply_q15_exact(cz, cy);
        const auto t3 = multiply_q15_exact(sz, sy);
        const auto t4 = multiply_q15_exact(sz, cy);
        const std::array<std::int16_t, 9> matrix{
            add_word(multiply_q15_exact(t3, sx), t2),
            subtract_word(multiply_q15_exact(t1, sx), t4),
            multiply_q15_exact(cx, sy),
            multiply_q15_exact(cx, sz),
            multiply_q15_exact(cx, cz),
            static_cast<std::int16_t>(static_cast<std::uint16_t>(0U
                - static_cast<std::uint16_t>(sx))),
            subtract_word(multiply_q15_exact(t4, sx), t1),
            add_word(multiply_q15_exact(t2, sx), t3),
            multiply_q15_exact(cx, cy),
        };
        for (std::size_t index = 0; index < matrix.size(); ++index) {
            write_superfx16(m_wmat11 + static_cast<std::uint32_t>(index * 2U),
                static_cast<std::uint16_t>(matrix[index]));
        }
    }

    void rotate_world_point() {
        const std::array<std::int16_t, 3> point{
            signed16(read_superfx16(m_x1)),
            signed16(read_superfx16(m_y1)),
            signed16(read_superfx16(m_z1)),
        };
        std::array<std::int16_t, 9> matrix{};
        for (std::size_t index = 0; index < matrix.size(); ++index) {
            matrix[index] = signed16(read_superfx16(
                m_wmat11 + static_cast<std::uint32_t>(index * 2U)));
        }
        for (std::size_t column = 0; column < 3U; ++column) {
            auto value = multiply_q15_exact(point[0], matrix[column]);
            value = add_word(value,
                multiply_q15_exact(point[1], matrix[3U + column]));
            value = add_word(value,
                multiply_q15_exact(point[2], matrix[6U + column]));
            write_superfx16(m_bigx + static_cast<std::uint32_t>(column * 2U),
                static_cast<std::uint16_t>(value));
        }
    }

    std::uint32_t texture_pointer(std::uint16_t sprite) const {
        const auto entry = textureaddrtab + static_cast<std::uint32_t>(sprite) * 3U;
        return static_cast<std::uint32_t>(rom->read8(entry))
            | (static_cast<std::uint32_t>(rom->read8(entry + 1U)) << 8U)
            | (static_cast<std::uint32_t>(rom->read8(entry + 2U)) << 16U);
    }

    std::uint8_t texture_byte(
        std::uint16_t sprite, std::uint32_t source, std::uint32_t offset) const {
        try {
            return rom->read8(source + offset);
        } catch (const std::out_of_range& error) {
            std::ostringstream message;
            message << error.what() << " (texture sprite=$" << std::hex
                    << sprite << ", source=$" << source << ", offset=$"
                    << offset << ')';
            throw std::out_of_range{message.str()};
        }
    }

    void write_planet_pixel(std::int32_t x, std::int32_t y, std::uint8_t colour) noexcept {
        if (x < 0 || y < 0 || x >= 128 || y >= 128 || colour == 0U) return;
        // SETCHARMAPPLAN_L lays the 16x16 8-bpp bitmap out column-major:
        // moving one tile right adds 16, while moving down adds one. Using a
        // conventional row-major index only became obvious during the zoom,
        // where it transposed each tile into a block of apparent static.
        const auto tile = static_cast<std::uint32_t>(x >> 3) * 16U
            + static_cast<std::uint32_t>(y >> 3);
        const auto row = static_cast<std::uint32_t>(y & 7);
        const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7));
        const auto base = static_cast<std::uint16_t>(bitmap1
            + tile * 64U + row * 2U);
        for (std::uint32_t plane = 0; plane < 8U; ++plane) {
            const auto address = static_cast<std::uint16_t>(base
                + (plane >> 1U) * 16U + (plane & 1U));
            auto& output = superfx_ram[address];
            if ((colour & (1U << plane)) != 0U) output |= mask;
            else output &= static_cast<std::uint8_t>(~mask);
        }
    }

    void clear_planet_bitmap() noexcept {
        const auto begin = superfx_ram.begin() + static_cast<std::uint16_t>(bitmap1);
        std::fill_n(begin, 16U * 16U * 64U, std::uint8_t{});
    }

    void draw_planet_sprite32() {
        const auto sprite = read_superfx16(msprite);
        // PLANETS marks spherical entries with bit 7, then clears that bit
        // immediately before launching MDRAWTSPHERE. RetroCPU currently
        // misses the 8-bit BMI on this path, so the marker can arrive at the
        // translated flat-sprite entry instead. Recover the source branch
        // here instead of indexing TEXTUREADDRTAB with the bogus $80 bit.
        const auto planet_texture = static_cast<std::uint8_t>(sprite & 0x7fU);
        if ((sprite & 0x80U) != 0U
            || (planet_texture >= 0x34U && planet_texture <= 0x38U)) {
            write_superfx16(msprite,
                static_cast<std::uint16_t>(planet_texture));
            draw_planet_sphere();
            return;
        }
        const auto source = texture_pointer(sprite);
        const auto left = static_cast<std::int32_t>(signed16(read_superfx16(m_xc))) - 16;
        const auto top = static_cast<std::int32_t>(signed16(read_superfx16(m_yc))) - 16;
        const auto palette = static_cast<std::uint8_t>(read_superfx16(mspr_pal) & 15U);
        for (std::int32_t y = 0; y < 32; ++y) {
            for (std::int32_t x = 0; x < 32; ++x) {
                const auto texel = static_cast<std::uint8_t>(
                    texture_byte(sprite, source,
                        static_cast<std::uint32_t>(y) * 256U
                            + static_cast<std::uint32_t>(x)) >> 4U);
                if (texel != 0U) {
                    write_planet_pixel(left + x, top + y,
                        static_cast<std::uint8_t>((palette << 4U) | texel));
                }
            }
        }
    }

    void draw_planet_scaled_sprite() {
        const auto sprite = read_superfx16(msprite);
        const auto planet_texture = static_cast<std::uint8_t>(sprite & 0x7fU);
        if ((sprite & 0x80U) != 0U
            || (planet_texture >= 0x34U && planet_texture <= 0x38U)) {
            write_superfx16(msprite,
                static_cast<std::uint16_t>(planet_texture));
            draw_planet_sphere();
            return;
        }
        const auto source = texture_pointer(sprite);
        const auto centre_x = static_cast<std::int32_t>(
            signed16(read_superfx16(m_xc)));
        const auto centre_y = static_cast<std::int32_t>(
            signed16(read_superfx16(m_yc)));
        const auto source_size = std::max<std::int32_t>(1,
            signed16(read_superfx16(m_sprsize)));
        const auto output_size = std::max<std::int32_t>(1,
            signed16(read_superfx16(m_sprxscale)));
        const auto left = centre_x - output_size / 2;
        const auto top = centre_y - output_size / 2;
        const auto palette = static_cast<std::uint8_t>(
            read_superfx16(mspr_pal) & 15U);
        for (std::int32_t y = 0; y < output_size; ++y) {
            const auto source_y = std::clamp(
                y * source_size / output_size, 0, 31);
            for (std::int32_t x = 0; x < output_size; ++x) {
                const auto source_x = std::clamp(
                    x * source_size / output_size, 0, 31);
                const auto texel = static_cast<std::uint8_t>(
                    texture_byte(sprite, source,
                        static_cast<std::uint32_t>(source_y) * 256U
                            + static_cast<std::uint32_t>(source_x)) >> 4U);
                if (texel != 0U) {
                    write_planet_pixel(left + x, top + y,
                        static_cast<std::uint8_t>((palette << 4U) | texel));
                }
            }
        }
    }

    void draw_planet_sphere() {
        constexpr double tau = 6.283185307179586476925286766559;
        // PLANETS stores the sphere marker in bit 7 and may retain unrelated
        // high-byte scratch bits in M_SPRITE while entering MDRAWSPHERE. The
        // original GSU consumes only the 7-bit texture index. Indexing the
        // host pointer table with the full word (for Venom this was $0838)
        // wandered into unrelated ROM bytes and eventually tried to sample
        // $70:0c6d as LoROM.
        const auto sprite = static_cast<std::uint16_t>(
            read_superfx16(msprite) & 0x007fU);
        const auto source = texture_pointer(sprite);
        const auto centre_x = static_cast<std::int32_t>(signed16(read_superfx16(m_xc)));
        const auto centre_y = static_cast<std::int32_t>(signed16(read_superfx16(m_yc)));
        const auto radius = std::max<std::int32_t>(1, signed16(read_superfx16(m_radius)));
        const auto angle = [tau](std::uint16_t value) {
            return static_cast<double>(value) * tau / 65536.0;
        };
        const auto ax = angle(read_superfx16(m_rotx));
        const auto ay = angle(read_superfx16(m_roty));
        const auto az = angle(read_superfx16(m_rotz));
        const auto sx = std::sin(ax);
        const auto cx = std::cos(ax);
        const auto sy = std::sin(ay);
        const auto cy = std::cos(ay);
        const auto sz = std::sin(az);
        const auto cz = std::cos(az);

        auto light_x = static_cast<double>(signed16(read_superfx16(m_lxpos))
            - signed16(read_superfx16(m_bigx)));
        auto light_y = static_cast<double>(signed16(read_superfx16(m_lypos))
            - signed16(read_superfx16(m_bigy)));
        auto light_z = static_cast<double>(signed16(read_superfx16(m_lzpos))
            - signed16(read_superfx16(m_bigz)));
        const auto light_length = std::sqrt(
            light_x * light_x + light_y * light_y + light_z * light_z);
        if (light_length > 0.0) {
            light_x /= light_length;
            light_y /= light_length;
            light_z /= light_length;
        } else {
            light_z = 1.0;
        }
        const auto intensity_scale = std::clamp(
            static_cast<double>(read_superfx16(m_scale)) / 32768.0, 0.0, 1.0);

        for (std::int32_t dy = -radius; dy <= radius; ++dy) {
            for (std::int32_t dx = -radius; dx <= radius; ++dx) {
                const auto squared = dx * dx + dy * dy;
                if (squared > radius * radius) continue;
                const auto nx = static_cast<double>(dx) / radius;
                const auto ny = -static_cast<double>(dy) / radius;
                const auto nz = std::sqrt(std::max(0.0, 1.0 - nx * nx - ny * ny));

                // Recreate MCROTMATZXY16's Z-X-Y matrix and map the visible
                // sphere normal back into texture space through rows two
                // and one. This keeps the fixed axial tilt while ROty
                // advances the texture during SPINPLANETS.
                const auto m11 = sz * sy * sx + cz * cy;
                const auto m12 = cz * sy * sx - sz * cy;
                const auto m13 = cx * sy;
                const auto m21 = cx * sz;
                const auto m22 = cx * cz;
                const auto m23 = -sx;
                // MGENUVLIST advances the first coordinate through the
                // scaled matrix's second row (M21/M22/M23), then derives the
                // second from its first row. Using columns here rotates and
                // transposes every planet texture even though the silhouette
                // remains plausible.
                const auto tx = m21 * nx + m22 * ny + m23 * nz;
                const auto ty = m11 * nx + m12 * ny + m13 * nz;
                // MPLANET's UV lists use the rotated Cartesian surface
                // vector directly around the $4000 fixed-point centre.
                // MERGE packs the high bytes, shifts them twice and masks
                // with $1f3f. Consequently the projected coordinate is
                // centred at texel 16, advances by 32 texels per unit and
                // wraps in the 64x32 source cell. Clamping it edge-to-edge
                // changes the source phase and turns Titania's diagonal
                // bands into a concentric ring.
                // MERGE places R7's high byte above R8's high byte before
                // the shift. The first UV list (matrix row two) therefore
                // selects the 32 texture rows, while row one selects the 64
                // columns. Reversing those axes makes latitude bands rotate
                // into concentric circles.
                const auto u = static_cast<std::int32_t>(std::floor(
                    16.0 + ty * 32.0)) & 63;
                const auto v = static_cast<std::int32_t>(std::floor(
                    16.0 + tx * 32.0)) & 31;
                const auto texel = static_cast<std::uint8_t>(texture_byte(
                    sprite, source, static_cast<std::uint32_t>(v) * 256U
                        + static_cast<std::uint32_t>(u)) >> 4U);
                if (texel == 0U) continue;

                const auto diffuse = (nx * light_x + ny * light_y + nz * light_z)
                    * intensity_scale;
                const auto shade = static_cast<std::uint8_t>(std::clamp(
                    static_cast<std::int32_t>(std::lround((1.0 - diffuse) * 7.0)),
                    0, 13));
                write_planet_pixel(centre_x + dx, centre_y + dy,
                    static_cast<std::uint8_t>((shade << 4U) | texel));
            }
        }
    }

    void launch_superfx() {
        const auto address = (static_cast<std::uint32_t>(
                                  superfx_registers[0x34U]) << 16U)
            | static_cast<std::uint16_t>(superfx_registers[0x1eU]
                | (static_cast<std::uint16_t>(superfx_registers[0x1fU]) << 8U));
        if (mrotplanet != 0U && address == mrotplanet) {
            generate_rotating_horizontal_offsets();
            return;
        }
        if (mnograd != 0U && address == mnograd) {
            generate_constant_horizontal_offsets();
            return;
        }
        if (mtunnelgrad != 0U && address == mtunnelgrad) {
            generate_tunnel_horizontal_offsets();
            return;
        }
        if (mwibbletunnel != 0U && address == mwibbletunnel) {
            generate_water_horizontal_offsets();
            return;
        }
        if (mbhole != 0U && address == mbhole) {
            generate_black_hole_horizontal_offsets();
            return;
        }
        if (mcallarctan16 != 0U && address == mcallarctan16) {
            calculate_arctangent16();
            return;
        }
        if (minitdust != 0U && address == minitdust) {
            initialize_dust();
            return;
        }
        if (mcrotwmatzxy16 != 0U && address == mcrotwmatzxy16) {
            calculate_world_matrix();
            return;
        }
        if (mwmatrotp16 != 0U && address == mwmatrotp16) {
            rotate_world_point();
            return;
        }
        if (mclrmapscreen != 0U && address == mclrmapscreen) {
            clear_planet_bitmap();
            return;
        }
        if (mdrawsprite32 != 0U && address == mdrawsprite32) {
            draw_planet_sprite32();
            return;
        }
        if (musprite != 0U && address == musprite) {
            draw_planet_scaled_sprite();
            return;
        }
        if (mdrawsphere != 0U && address == mdrawsphere) {
            draw_planet_sphere();
            return;
        }
        // These routines update only the source bitmap/window. Their visible
        // output is composed by the PC renderer from the state that the
        // surrounding 65C816 routines maintain, so completion is immediate
        // just as it is for the other translated Super FX entry points.
        if ((mcalc_circle != 0U && address == mcalc_circle)
            || (mcopyface != 0U && address == mcopyface)
            || (mfprintstr != 0U && address == mfprintstr)
            || (msprintstr != 0U && address == msprintstr)
            || (mshowteammate2 != 0U && address == mshowteammate2)) {
            return;
        }
        if (mdecrunch == 0U || address != mdecrunch) {
            if (std::find(unknown_superfx_launches.begin(),
                    unknown_superfx_launches.end(), address)
                == unknown_superfx_launches.end()) {
                unknown_superfx_launches.push_back(address);
            }
            return;
        }

        const auto end_address =
            (static_cast<std::uint32_t>(read_superfx16(kMEndDataBank) & 0xffU) << 16U)
            | read_superfx16(kMEndData);
        auto decoded = assets::decrunch_reverse(*rom, end_address);
        const auto destination = read_superfx16(kMDecrunchAddress);
        if (destination + decoded.bytes.size() > superfx_ram.size()) {
            throw std::runtime_error{"Super FX decrunch output exceeds RAM"};
        }
        const auto word_offset = read_superfx16(kMDecrunchOffset);
        if (word_offset != 0U) {
            for (std::size_t index = 0; index + 1U < decoded.bytes.size(); index += 2U) {
                const auto word = static_cast<std::uint16_t>(decoded.bytes[index])
                    | (static_cast<std::uint16_t>(decoded.bytes[index + 1U]) << 8U);
                const auto adjusted = static_cast<std::uint16_t>(word + word_offset);
                decoded.bytes[index] = static_cast<std::uint8_t>(adjusted);
                decoded.bytes[index + 1U] = static_cast<std::uint8_t>(adjusted >> 8U);
            }
        }
        std::copy(decoded.bytes.begin(), decoded.bytes.end(),
            superfx_ram.begin() + destination);
        write_superfx16(kMDecrunchEnd,
            static_cast<std::uint16_t>(destination + decoded.bytes.size()));
        write_superfx16(kMEndData,
            static_cast<std::uint16_t>(decoded.compressed_begin));
    }

    void copy_superfx_to_vram(
        std::uint16_t source, std::uint16_t destination, std::uint16_t length) noexcept {
        auto output = static_cast<std::uint32_t>(destination) * 2U;
        for (std::uint32_t index = 0; index < length; ++index) {
            ppu.vram[(output + index) & 0xffffU] =
                superfx_ram[static_cast<std::uint16_t>(source + index)];
        }
    }

    void copy_bus_to_cgram(
        std::uint32_t source, std::uint16_t destination, std::uint16_t length) {
        for (std::uint16_t index = 0; index < length; ++index) {
            write_cgram_byte(static_cast<std::uint16_t>(destination + index),
                bus.ReadByte(source + index));
        }
    }

    void service_transfer() {
        const auto flag = wram[0];
        switch (flag) {
        case 16U: {
            const auto palette_source = static_cast<std::uint32_t>(read_wram16(kVram3Address))
                | (static_cast<std::uint32_t>(wram[kVram3Address + 2U]) << 16U);
            copy_bus_to_cgram(palette_source, 0U, read_wram16(kVram3Length));
            copy_superfx_to_vram(kDecrunchBuffer,
                read_wram16(kVram1Address), read_wram16(kVram1Length));
            wram[0] = 18U;
            break;
        }
        case 18U:
            copy_superfx_to_vram(kScreenDecrunchBuffer,
                read_wram16(kVram2Address), read_wram16(kVram2Length));
            copy_bus_to_cgram(kGamePalette, 7U * 16U * 2U, 32U);
            wram[0] = 0U;
            break;
        case 20U:
            copy_superfx_to_vram(kDecrunchBuffer,
                read_wram16(kVram1Address), read_wram16(kVram1Length));
            wram[0] = 0U;
            break;
        case 22U:
            copy_superfx_to_vram(kDecrunchBuffer,
                read_wram16(kVram1Address), read_wram16(kVram1Length));
            copy_superfx_to_vram(kScreenDecrunchBuffer,
                read_wram16(kVram2Address), read_wram16(kVram2Length));
            wram[0] = 0U;
            break;
        case 36U:
        case 38U:
            ppu.background_mode = flag == 36U ? 1U : 2U;
            ppu.bg2_character_base = 0x5000U;
            ppu.bg2_screen_base = 0x7000U;
            ppu.bg2_screen_size = 3U;
            ppu.bg3_screen_base = 0x2c00U;
            ppu.bg3_screen_size = 3U;
            wram[0] = 0U;
            break;
        default:
            // Other transfer modes are presentation paths that are not
            // entered by bounded gameplay/background calls yet.
            wram[0] = 0U;
            break;
        }
    }

    void service_planet_transfer() noexcept {
        if (planetdma == 0U || vmap2 == 0U || bitmap1 == 0U) return;
        const auto request = wram[static_cast<std::uint16_t>(planetdma)];
        if (request == 0U) return;
        copy_superfx_to_vram(static_cast<std::uint16_t>(bitmap1),
            read_wram16(static_cast<std::uint16_t>(vmap2)), 16U * 16U * 64U);
        wram[static_cast<std::uint16_t>(planetdma)] = 0U;
    }
};

Wdc65816::Wdc65816(
    const assets::RomImage& rom, const assets::SymbolMap* symbols)
    : impl_(std::make_unique<Impl>(rom, symbols)) {}

Wdc65816::~Wdc65816() = default;
Wdc65816::Wdc65816(Wdc65816&&) noexcept = default;
Wdc65816& Wdc65816::operator=(Wdc65816&&) noexcept = default;

std::uint8_t Wdc65816::read8(std::uint32_t address) const {
    return impl_->read8(address);
}

std::uint16_t Wdc65816::read16(std::uint32_t address) const {
    return static_cast<std::uint16_t>(read8(address))
        | (static_cast<std::uint16_t>(read8(address + 1U)) << 8U);
}

void Wdc65816::write8(std::uint32_t address, std::uint8_t value) {
    impl_->write8(address, value);
}

void Wdc65816::write16(std::uint32_t address, std::uint16_t value) {
    write8(address, static_cast<std::uint8_t>(value));
    write8(address + 1U, static_cast<std::uint8_t>(value >> 8U));
}

std::vector<ApuPortWrite> Wdc65816::take_apu_port_writes() {
    auto result = std::move(impl_->apu_writes);
    impl_->apu_writes.clear();
    return result;
}

void Wdc65816::set_apu_clock_offset(std::uint32_t clocks) noexcept {
    impl_->apu_clock_offset = clocks;
}

void Wdc65816::set_apu_output_ports(
    const std::array<std::uint8_t, 4>& ports) noexcept {
    impl_->apu_output_connected = true;
    if (!impl_->apu_upload_active) impl_->apu_ports = ports;
}

const SnesPpuState& Wdc65816::ppu_state() const noexcept {
    return impl_->ppu;
}

const std::vector<std::uint32_t>& Wdc65816::unknown_superfx_launches()
    const noexcept {
    return impl_->unknown_superfx_launches;
}

std::uint64_t Wdc65816::apu_upload_generation() const noexcept {
    return impl_->apu_upload_generation;
}

void Wdc65816::write_cgram(
    std::uint16_t first_colour,
    std::span<const std::uint16_t> colours) noexcept {
    const auto available = std::min<std::size_t>(
        colours.size(), impl_->ppu.cgram.size() - std::min<std::size_t>(
            first_colour, impl_->ppu.cgram.size()));
    std::copy_n(colours.begin(), available,
        impl_->ppu.cgram.begin() + std::min<std::size_t>(
            first_colour, impl_->ppu.cgram.size()));
}

void Wdc65816::write_vram(
    std::uint16_t byte_offset,
    std::span<const std::uint8_t> bytes) noexcept {
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        impl_->ppu.vram[static_cast<std::uint16_t>(
            byte_offset + static_cast<std::uint16_t>(index))] = bytes[index];
    }
}

void Wdc65816::upload_oam(std::uint32_t source, std::size_t length) {
    length = std::min(length, impl_->ppu.oam.size());
    for (std::size_t index = 0; index < length; ++index) {
        impl_->ppu.oam[index] = impl_->read8(source + static_cast<std::uint32_t>(index));
    }
}

void Wdc65816::set_bg1_scroll(std::int16_t x, std::int16_t y) noexcept {
    impl_->ppu.bg1_scroll_x = x;
    impl_->ppu.bg1_scroll_y = y;
}

void Wdc65816::draw_planet_sphere(std::uint16_t sprite) {
    impl_->write_superfx16(impl_->msprite, sprite);
    impl_->draw_planet_sphere();
}

void Wdc65816::set_bg2_vertical_offsets_enabled(bool enabled) noexcept {
    impl_->ppu.bg2_vertical_offsets_enabled = enabled;
}

void Wdc65816::capture_bg2_horizontal_offsets(
    std::uint16_t source, bool enabled) noexcept {
    impl_->ppu.bg2_horizontal_offsets_enabled = enabled;
    if (!enabled) return;
    for (std::size_t line = 0; line < impl_->ppu.bg2_horizontal_offsets.size(); ++line) {
        const auto record = static_cast<std::uint16_t>(source + line * 3U);
        const auto value = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(impl_->wram[
                static_cast<std::uint16_t>(record + 1U)])
            | (static_cast<std::uint16_t>(impl_->wram[
                   static_cast<std::uint16_t>(record + 2U)]) << 8U));
        impl_->ppu.bg2_horizontal_offsets[line] = std::bit_cast<std::int16_t>(value);
    }
}

std::size_t Wdc65816::call_long(
    std::uint32_t address,
    Wdc65816Registers& registers,
    std::size_t instruction_limit,
    bool service_transfer_flag) {
    return call(address, registers, instruction_limit,
        service_transfer_flag, true);
}

std::size_t Wdc65816::call_near(
    std::uint32_t address,
    Wdc65816Registers& registers,
    std::size_t instruction_limit,
    bool service_transfer_flag) {
    return call(address, registers, instruction_limit,
        service_transfer_flag, false);
}

std::size_t Wdc65816::call(
    std::uint32_t address,
    Wdc65816Registers& registers,
    std::size_t instruction_limit,
    bool service_transfer_flag,
    bool long_return) {
    auto& cpu = impl_->cpu;
    cpu.SetRegister("p", registers.status);
    cpu.SetRegister("a", registers.a);
    cpu.SetRegister("x", registers.x);
    cpu.SetRegister("y", registers.y);
    cpu.SetRegister("d", registers.direct);
    cpu.SetRegister("sp", registers.stack);
    cpu.SetRegister("db", registers.data_bank);
    const auto return_sentinel = long_return
        ? kReturnSentinel
        : (address & 0xff0000U) | (kReturnSentinel & 0xffffU);
    if (long_return) {
        cpu.Push(static_cast<std::uint8_t>(return_sentinel >> 16U));
    }
    cpu.Push(static_cast<std::uint16_t>((kReturnSentinel & 0xffffU) - 1U));
    cpu.SetRegister("pb", address >> 16U);
    cpu.SetRegister("pc", address);

    std::size_t instructions = 0;
    std::array<std::uint32_t, 32> recent_program_counters{};
    std::vector<std::uint32_t> crash_entry_trace;
    std::uint32_t crash_entry{};
    while (cpu.program_address() != return_sentinel) {
        // BGS.ASM's waittrans macro waits for the NMI-side transfer engine to
        // clear TRANS_FLAG at WRAM $0000. During a bounded subroutine call no
        // concurrent SNES NMI runs, so acknowledge those requests here when
        // the caller is explicitly executing a transfer-side routine.
        if (service_transfer_flag && impl_->wram[0] != 0U) {
            impl_->service_transfer();
        }
        if (service_transfer_flag) impl_->service_planet_transfer();
        if (instructions == instruction_limit) {
            std::ostringstream message;
            message << "65C816 subroutine at $" << std::hex << address
                    << " exceeded the instruction limit at $"
                    << cpu.program_address() << " (recent";
            const auto available = std::min(instructions, recent_program_counters.size());
            for (std::size_t age = available; age != 0; --age) {
                const auto index = (instructions - age) % recent_program_counters.size();
                message << " $" << recent_program_counters[index];
            }
            message << ')';
            if (crash_entry != 0U) {
                message << "; entered original crash handler at $" << crash_entry
                        << " after";
                for (const auto pc : crash_entry_trace) {
                    message << " $" << pc;
                }
            }
            throw std::runtime_error{message.str()};
        }
        const auto pc = cpu.program_address();
        if (crash_entry == 0U
            && (pc == 0x1fdc9dU || pc == 0x1fdcaaU || pc == 0x1fdcb7U
                || pc == 0x1fdcc4U || pc == 0x1fdcd1U)) {
            crash_entry = pc;
            const auto available = std::min(instructions, recent_program_counters.size());
            crash_entry_trace.reserve(available);
            for (std::size_t age = available; age != 0; --age) {
                const auto index = (instructions - age) % recent_program_counters.size();
                crash_entry_trace.push_back(recent_program_counters[index]);
            }
        }
        recent_program_counters[instructions % recent_program_counters.size()]
            = pc;
        cpu.SingleStep();
        ++instructions;
    }

    registers.a = cpu.a();
    registers.x = cpu.x();
    registers.y = cpu.y();
    registers.direct = cpu.cpu_state.regs.d.u16;
    registers.stack = cpu.cpu_state.regs.sp.u16;
    registers.data_bank = static_cast<std::uint8_t>(cpu.cpu_state.data_segment_base >> 16U);
    registers.status = cpu.GetStatusRegister();
    return instructions;
}

} // namespace starfox::simulation
