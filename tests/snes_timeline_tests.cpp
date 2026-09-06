#include "starfox/simulation/snes_timeline.hpp"
#include <iostream>
#include <stdexcept>

using namespace starfox::simulation;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error{message};
}

int main() try {
    for (const auto region : {SnesRegion::ntsc, SnesRegion::pal}) {
        for (const bool interlace : {false, true}) {
            SnesRasterClock raster{region};
            while (raster.fields() < 2U) raster.tick(interlace);
            const auto expected = region == SnesRegion::ntsc
                ? (interlace ? 716100U : 714732U) : (interlace ? 852504U : 851136U);
            require(raster.elapsed() == expected, "two-field raster duration differs");
            require(raster.vertical(2U) == (region == SnesRegion::ntsc ? 261U : 311U),
                "beam history lost the preceding field's final line");
        }
    }
    SnesRasterClock raster;
    while (!(raster.field() && raster.vertical() == 240U)) raster.tick(false);
    require(raster.horizontal_period() == 1360U, "NTSC odd field omitted its short line");
    while (raster.vertical() == 240U) raster.tick(false);
    require(raster.horizontal(10U) == 1350U && raster.vertical(10U) == 240U,
        "delayed beam assumed a regular preceding scanline");

    SnesCpuTimeline timeline;
    auto& irq = timeline.interrupts();
    irq.write_timer(0U, 132U, timeline.beam());
    irq.write_timer(1U, 0U, timeline.beam());
    irq.write_control(0x10U);
    timeline.step(532U);
    require(timeline.raster().elapsed() == 532U && timeline.totals().refresh == 0U,
        "refresh ran before the version-2 threshold");
    timeline.step(8U);
    require(timeline.raster().elapsed() == 580U && timeline.totals().cpu == 540U
        && timeline.totals().refresh == 40U, "refresh did not wait for the bus operation");
    require(irq.sample(false) == InterruptRequest{false, true, true},
        "refresh clocks failed to advance the IRQ comparator and hold delay");
    timeline.step(784U, SnesClockWork::dma);
    require(timeline.raster().horizontal() == 0U && timeline.raster().vertical() == 1U
        && timeline.totals().dma == 784U && timeline.refresh_position() == 534U,
        "DMA clocks or next-line refresh alignment differs");
    timeline.step(532U);
    require(timeline.totals().refresh == 40U, "second refresh ran early");
    timeline.step(2U);
    require(timeline.raster().horizontal() == 574U && timeline.totals().refresh == 80U,
        "next-line refresh did not use the CPU clock divider");

    SnesCpuTimeline version1{SnesRegion::ntsc, 1U};
    version1.step(528U);
    require(version1.totals().refresh == 0U, "version-1 refresh ran early");
    version1.step(2U);
    require(version1.raster().horizontal() == 570U, "version-1 refresh threshold differs");
    bool rejected = false;
    try { timeline.step(3U); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "odd bus step was silently rounded");
    require(timeline.raster().elapsed() == timeline.totals().cpu + timeline.totals().dma
        + timeline.totals().refresh, "clock categories do not partition elapsed time");
    std::cout << "raster fields, short-line history, CPU/DMA refresh and live IRQ timing passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
