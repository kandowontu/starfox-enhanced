#include "starfox/render/pixel_filter.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#ifdef STARFOX_ENABLE_XBRZ
#include "xbrz.h"
#endif

namespace starfox::render {
namespace {

constexpr std::uint8_t two_d_tag =
    static_cast<std::uint8_t>(PixelLayer::two_d);

// xBRZ's own packing: alpha in the high byte, blue in the low byte, which is
// BGRA byte order on the little-endian targets this port builds for. EDGE uses
// the same layout so both backends share the composite path.
[[nodiscard]] constexpr std::uint32_t pack_opaque(const Rgba8& colour) noexcept {
    return 0xff000000U
        | (static_cast<std::uint32_t>(colour.r) << 16U)
        | (static_cast<std::uint32_t>(colour.g) << 8U)
        | static_cast<std::uint32_t>(colour.b);
}

[[nodiscard]] constexpr bool opaque(std::uint32_t colour) noexcept {
    return (colour >> 24U) != 0U;
}

// Source rows a filter needs either side of a slice to resolve its edges.
constexpr std::uint32_t filter_context_rows = 2U;

} // namespace

std::string_view two_d_filter_name(TwoDFilter filter) noexcept {
    switch (filter) {
    case TwoDFilter::edge: return "EDGE";
    case TwoDFilter::xbrz: return "XBRZ";
    case TwoDFilter::off: break;
    }
    return "OFF";
}

bool two_d_filter_compiled_in(TwoDFilter filter) noexcept {
    switch (filter) {
    case TwoDFilter::off:
    case TwoDFilter::edge:
        return true;
    case TwoDFilter::xbrz:
#ifdef STARFOX_ENABLE_XBRZ
        return true;
#else
        return false;
#endif
    }
    return false;
}

void scale_edge(
    std::size_t factor,
    const std::uint32_t* source,
    std::uint32_t* target,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t first_row,
    std::uint32_t last_row) noexcept {
    if (factor < 2U || factor > 10U || width == 0U || height == 0U) return;
    const auto target_width = static_cast<std::size_t>(width) * factor;

    // Which corner each subpixel belongs to depends only on the factor, not on
    // the pixel being scaled, so resolve the geometry once instead of doing
    // four floating-point comparisons per output pixel. At 10x that was the
    // single largest cost in the filter.
    std::array<std::uint8_t, 100> pattern{};
    for (std::size_t row = 0; row < factor; ++row) {
        const auto v = (static_cast<double>(row) + 0.5)
            / static_cast<double>(factor);
        for (std::size_t column = 0; column < factor; ++column) {
            const auto u = (static_cast<double>(column) + 0.5)
                / static_cast<double>(factor);
            // The half-pixel diagonal through each corner. At factor 2 this
            // reproduces EPX exactly; at higher factors the cut becomes a real
            // 45-degree staircase instead of one pixel.
            auto corner = std::uint8_t{0};
            if (u + v <= 0.5) {
                corner = 1U;
            } else if ((1.0 - u) + v <= 0.5) {
                corner = 2U;
            } else if (u + (1.0 - v) <= 0.5) {
                corner = 3U;
            } else if ((1.0 - u) + (1.0 - v) <= 0.5) {
                corner = 4U;
            }
            pattern[row * factor + column] = corner;
        }
    }
    const auto sample = [source, width, height](
                            std::int64_t x, std::int64_t y) {
        x = std::clamp<std::int64_t>(x, 0, static_cast<std::int64_t>(width) - 1);
        y = std::clamp<std::int64_t>(y, 0, static_cast<std::int64_t>(height) - 1);
        return source[static_cast<std::size_t>(y) * width
            + static_cast<std::size_t>(x)];
    };

    for (auto y = first_row; y < std::min(last_row, height); ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto centre = sample(x, y);
            const auto up = sample(x, static_cast<std::int64_t>(y) - 1);
            const auto right = sample(static_cast<std::int64_t>(x) + 1, y);
            const auto down = sample(x, static_cast<std::int64_t>(y) + 1);
            const auto left = sample(static_cast<std::int64_t>(x) - 1, y);

            // Classic EPX corner rules: a corner is replaced only when the two
            // neighbours meeting there agree with each other and disagree with
            // the neighbours opposite, which is exactly the signature of a
            // diagonal edge passing through this pixel.
            auto top_left = centre;
            auto top_right = centre;
            auto bottom_left = centre;
            auto bottom_right = centre;
            if (left == up && left != down && up != right) top_left = up;
            if (up == right && up != left && right != down) top_right = right;
            if (down == left && down != right && left != up) bottom_left = left;
            if (right == down && right != left && down != up) bottom_right = down;

            // A hole is where the 3D layer owns the pixel. Art must never be
            // eroded into one, so only let transparency win if the centre is
            // itself transparent.
            if (opaque(centre)) {
                if (!opaque(top_left)) top_left = centre;
                if (!opaque(top_right)) top_right = centre;
                if (!opaque(bottom_left)) bottom_left = centre;
                if (!opaque(bottom_right)) bottom_right = centre;
            }

            const std::array<std::uint32_t, 5> choices{
                centre, top_left, top_right, bottom_left, bottom_right};
            for (std::size_t row = 0; row < factor; ++row) {
                auto* output = target
                    + (static_cast<std::size_t>(y) * factor + row) * target_width
                    + static_cast<std::size_t>(x) * factor;
                const auto* corners = pattern.data() + row * factor;
                for (std::size_t column = 0; column < factor; ++column) {
                    output[column] = choices[corners[column]];
                }
            }
        }
    }
}

bool filter_overlay_layer(
    TwoDFilter filter,
    const Framebuffer& overlay,
    std::span<const Rgba8> palette,
    std::uint32_t render_scale,
    std::vector<std::uint32_t>& out_argb,
    PixelFilterScratch& scratch,
    RowWorkers& workers) {
    if (filter == TwoDFilter::off || render_scale < 2U || render_scale > 10U) return false;
    const auto width = overlay.width();
    const auto height = overlay.height();
    if (width == 0U || height == 0U || palette.empty()) return false;

    auto backend = filter;
    if (!two_d_filter_compiled_in(backend)) backend = TwoDFilter::edge;
    const std::size_t factor = backend == TwoDFilter::xbrz
        ? std::min<std::size_t>(render_scale, 6U)
        : render_scale;

    const auto cells = static_cast<std::size_t>(width) * height;
    scratch.source.assign(cells, 0U);
    auto occupied = false;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto entry = overlay.get(x, y);
            // Index 0 is the overlay's transparency, and the filter treats it
            // as a hole so art is reconstructed against it rather than into it.
            if (entry == 0U || entry >= palette.size()) continue;
            scratch.source[static_cast<std::size_t>(y) * width + x] =
                pack_opaque(palette[entry]);
            occupied = true;
        }
    }
    if (!occupied) return false;

    // resize, not assign: every pixel the composite reads is written by the
    // filter below, and at 10x an assign would zero ~36 MB per frame for
    // nothing.
    scratch.filtered.resize(cells * factor * factor);
    const auto* source = scratch.source.data();
    auto* filtered = scratch.filtered.data();
    workers.parallel_rows(height,
        [&](std::uint32_t first, std::uint32_t last) {
#ifdef STARFOX_ENABLE_XBRZ
            if (backend == TwoDFilter::xbrz) {
                xbrz::scale(factor, source, filtered,
                    static_cast<int>(width), static_cast<int>(height),
                    xbrz::ColorFormat::ARGB, xbrz::ScalerCfg{},
                    static_cast<int>(first), static_cast<int>(last));
                return;
            }
#endif
            scale_edge(factor, source, filtered, width, height, first, last);
        });

    const auto stored_width = static_cast<std::size_t>(width) * render_scale;
    const auto stored_height = height * render_scale;
    const auto filtered_width = static_cast<std::size_t>(width) * factor;
    out_argb.assign(stored_width * stored_height, 0U);
    auto* output = out_argb.data();
    workers.parallel_rows(stored_height,
        [&](std::uint32_t first, std::uint32_t last) {
            for (auto y = first; y < last; ++y) {
                const auto filtered_y =
                    static_cast<std::size_t>(y) * factor / render_scale;
                const auto* filtered_row = filtered
                    + filtered_y * filtered_width;
                auto* row = output + static_cast<std::size_t>(y) * stored_width;
                for (std::size_t x = 0; x < stored_width; ++x) {
                    row[x] = filtered_row[x * factor / render_scale];
                }
            }
        });
    return true;
}

void apply_two_d_filter(
    TwoDFilter filter,
    const Framebuffer& framebuffer,
    std::span<const Rgba8> palette,
    std::vector<std::uint8_t>& rgba,
    PixelFilterScratch& scratch,
    RowWorkers& workers,
    bool highlight) {
    if (filter == TwoDFilter::off) return;
    if (!framebuffer.layer_tags_enabled()) return;

    const auto scale = framebuffer.draw_scale();
    if (scale < 2U || scale > 10U) return;
    const auto width = framebuffer.width();
    const auto height = framebuffer.height();
    const auto stored_width = framebuffer.stored_width();
    const auto stored_height = framebuffer.stored_height();
    if (width == 0U || height == 0U || palette.empty()) return;
    if (rgba.size()
        < static_cast<std::size_t>(stored_width) * stored_height * 4U) {
        return;
    }

    auto backend = filter;
    if (!two_d_filter_compiled_in(backend)) backend = TwoDFilter::edge;
    // EDGE is defined at every integer factor, so it tracks RENDER UPSCALE all
    // the way to 10x. xBRZ tops out at 6x and is point-sampled the rest of the
    // way, which still resolves far more detail than block expansion.
    const std::size_t factor = backend == TwoDFilter::xbrz
        ? std::min<std::size_t>(scale, 6U)
        : scale;

    const auto& tags = framebuffer.layer_tags();
    if (tags.size()
        < static_cast<std::size_t>(stored_width) * stored_height) {
        return;
    }

    const auto cells = static_cast<std::size_t>(width) * height;
    scratch.source.assign(cells, 0U);

    // A source cell only joins the 2D layer when the render scale expanded it
    // into a block the 3D pass never overwrote. Partially covered cells stay
    // holes so a polygon edge crossing 2D art is never filtered over.
    auto first_row = height;
    auto last_row = std::uint32_t{0U};
    for (std::uint32_t y = 0; y < height; ++y) {
        auto row_has_art = false;
        for (std::uint32_t x = 0; x < width; ++x) {
            auto covered = true;
            for (std::uint32_t row = 0; row < scale && covered; ++row) {
                const auto* block = tags.data()
                    + static_cast<std::size_t>(y * scale + row) * stored_width
                    + static_cast<std::size_t>(x) * scale;
                covered = std::all_of(block, block + scale,
                    [](std::uint8_t tag) { return tag == two_d_tag; });
            }
            if (!covered) continue;
            const auto index = static_cast<std::size_t>(y) * width + x;
            const auto entry = std::min<std::size_t>(
                framebuffer.get(x, y), palette.size() - 1U);
            scratch.source[index] = pack_opaque(palette[entry]);
            row_has_art = true;
        }
        if (row_has_art) {
            first_row = std::min(first_row, y);
            last_row = y + 1U;
        }
    }
    if (first_row >= last_row) return;

    const auto band_first = first_row > filter_context_rows
        ? first_row - filter_context_rows : 0U;
    const auto band_last = std::min(height, last_row + filter_context_rows);

    // resize, not assign: every pixel the composite reads is written by the
    // filter below, and at 10x an assign would zero ~36 MB per frame for
    // nothing.
    scratch.filtered.resize(cells * factor * factor);
    const auto* source = scratch.source.data();
    auto* filtered = scratch.filtered.data();
    workers.parallel_rows(band_last - band_first,
        [&](std::uint32_t slice_first, std::uint32_t slice_last) {
            const auto y_first = band_first + slice_first;
            const auto y_last = band_first + slice_last;
#ifdef STARFOX_ENABLE_XBRZ
            if (backend == TwoDFilter::xbrz) {
                xbrz::scale(factor, source, filtered,
                    static_cast<int>(width), static_cast<int>(height),
                    xbrz::ColorFormat::ARGB, xbrz::ScalerCfg{},
                    static_cast<int>(y_first), static_cast<int>(y_last));
                return;
            }
#endif
            scale_edge(factor, source, filtered, width, height,
                y_first, y_last);
        });

    const auto filtered_width = static_cast<std::size_t>(width) * factor;
    workers.parallel_rows(stored_height,
        [&](std::uint32_t slice_first, std::uint32_t slice_last) {
            for (auto y = slice_first; y < slice_last; ++y) {
                const auto source_y = y / scale;
                if (source_y < first_row || source_y >= last_row) continue;
                const auto filtered_y = static_cast<std::size_t>(y) * factor
                    / scale;
                const auto* row_tags = tags.data()
                    + static_cast<std::size_t>(y) * stored_width;
                const auto* filtered_row = filtered
                    + filtered_y * filtered_width;
                auto* output = rgba.data()
                    + static_cast<std::size_t>(y) * stored_width * 4U;
                for (std::uint32_t x = 0; x < stored_width; ++x) {
                    if (row_tags[x] != two_d_tag) continue;
                    const auto colour = filtered_row[
                        static_cast<std::size_t>(x) * factor / scale];
                    const auto alpha = (colour >> 24U) & 0xffU;
                    if (alpha == 0U) continue;
                    auto* pixel = output + static_cast<std::size_t>(x) * 4U;
                    if (highlight) {
                        pixel[0] = 255U;
                        pixel[1] = 0U;
                        pixel[2] = 255U;
                        continue;
                    }
                    const auto red = (colour >> 16U) & 0xffU;
                    const auto green = (colour >> 8U) & 0xffU;
                    const auto blue = colour & 0xffU;
                    if (alpha == 0xffU) {
                        pixel[0] = static_cast<std::uint8_t>(red);
                        pixel[1] = static_cast<std::uint8_t>(green);
                        pixel[2] = static_cast<std::uint8_t>(blue);
                        continue;
                    }
                    // Partial coverage only happens where the filter blended
                    // art against a 3D hole. Lay it over the unfiltered pixel
                    // already in place so the seam stays continuous.
                    const auto blend = [alpha](std::uint32_t value,
                                           std::uint8_t existing) {
                        return static_cast<std::uint8_t>(
                            (value * alpha + existing * (255U - alpha) + 127U)
                            / 255U);
                    };
                    pixel[0] = blend(red, pixel[0]);
                    pixel[1] = blend(green, pixel[1]);
                    pixel[2] = blend(blue, pixel[2]);
                }
            }
        });
}

} // namespace starfox::render
