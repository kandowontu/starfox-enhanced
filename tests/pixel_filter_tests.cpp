#include "starfox/render/framebuffer.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/bloom.hpp"
#include "starfox/render/colour_math.hpp"
#include "starfox/render/model_smoothing.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/pixel_filter.hpp"
#include "starfox/render/row_workers.hpp"
#include "starfox/assets/shape.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/math.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using starfox::render::Framebuffer;
using starfox::render::PixelFilterScratch;
using starfox::render::PixelLayer;
using starfox::render::Rgba8;
using starfox::render::TwoDFilter;

constexpr std::uint32_t source_width = 64U;
constexpr std::uint32_t source_height = 48U;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<Rgba8> make_palette() {
    std::vector<Rgba8> palette(256U);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        // Spread the indices over a wide range so a filtered pixel is easy to
        // tell from an unfiltered neighbour.
        palette[index] = Rgba8{
            static_cast<std::uint8_t>((index * 37U) & 0xffU),
            static_cast<std::uint8_t>((index * 91U) & 0xffU),
            static_cast<std::uint8_t>((index * 173U) & 0xffU),
            255U};
    }
    palette[0] = Rgba8{0U, 0U, 0U, 255U};
    return palette;
}

// A 2D field with hard diagonal edges (the case a filter must smooth) plus a
// rectangular band of Super FX pixels written at stored resolution (the case it
// must leave alone).
void paint(Framebuffer& framebuffer, std::uint32_t scale) {
    framebuffer.clear(0U);
    for (std::uint32_t y = 0; y < source_height; ++y) {
        for (std::uint32_t x = 0; x < source_width; ++x) {
            const auto diagonal = (x + y) / 6U;
            const auto circle = ((x - 32) * (x - 32) + (y - 24) * (y - 24)) < 90
                ? 200U : 0U;
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y),
                static_cast<std::uint8_t>(
                    circle != 0U ? circle : 30U + (diagonal % 5U) * 40U));
        }
    }

    // Scan conversion drops the draw scale to 1 and writes stored pixels.
    const auto previous = framebuffer.draw_scale();
    framebuffer.set_draw_scale(1U);
    const starfox::render::ScopedLayer geometry{framebuffer, PixelLayer::three_d};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto in_band = x >= 10U * scale && x < 22U * scale
                && y >= 8U * scale && y < 40U * scale;
            if (in_band) {
                framebuffer.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 250U);
            }
        }
    }
    framebuffer.set_draw_scale(previous);
}

void write_bmp(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& rgba,
    std::uint32_t width, std::uint32_t height) {
    std::ofstream output{path, std::ios::binary};
    const auto row_bytes = ((width * 3U) + 3U) & ~3U;
    const auto pixel_bytes = row_bytes * height;
    const auto put32 = [&output](std::uint32_t value) {
        const std::array<char, 4> bytes{
            static_cast<char>(value & 0xffU),
            static_cast<char>((value >> 8U) & 0xffU),
            static_cast<char>((value >> 16U) & 0xffU),
            static_cast<char>((value >> 24U) & 0xffU)};
        output.write(bytes.data(), bytes.size());
    };
    const auto put16 = [&output](std::uint16_t value) {
        const std::array<char, 2> bytes{
            static_cast<char>(value & 0xffU),
            static_cast<char>((value >> 8U) & 0xffU)};
        output.write(bytes.data(), bytes.size());
    };
    output.write("BM", 2);
    put32(14U + 40U + pixel_bytes);
    put32(0U);
    put32(14U + 40U);
    put32(40U);
    put32(width);
    put32(height);
    put16(1U);
    put16(24U);
    put32(0U);
    put32(pixel_bytes);
    put32(2835U);
    put32(2835U);
    put32(0U);
    put32(0U);
    std::vector<char> row(row_bytes, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto source_y = height - 1U - y;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto* pixel = rgba.data()
                + (static_cast<std::size_t>(source_y) * width + x) * 4U;
            row[x * 3U + 0U] = static_cast<char>(pixel[2]);
            row[x * 3U + 1U] = static_cast<char>(pixel[1]);
            row[x * 3U + 2U] = static_cast<char>(pixel[0]);
        }
        output.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
}

struct Frame {
    Framebuffer framebuffer;
    std::vector<std::uint8_t> rgba;
};

Frame render(std::uint32_t scale, const std::vector<Rgba8>& palette) {
    Framebuffer framebuffer{source_width, source_height, scale};
    framebuffer.enable_layer_tags(true);
    paint(framebuffer, scale);
    std::vector<std::uint8_t> rgba;
    starfox::render::expand_rgba(framebuffer, rgba, palette);
    return Frame{std::move(framebuffer), std::move(rgba)};
}

void check_tags(std::uint32_t scale) {
    const auto palette = make_palette();
    const auto frame = render(scale, palette);
    const auto& framebuffer = frame.framebuffer;
    auto three_d_seen = std::size_t{0};
    auto two_d_seen = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto in_band = x >= 10U * scale && x < 22U * scale
                && y >= 8U * scale && y < 40U * scale;
            const auto layer = framebuffer.layer_stored(x, y);
            if (in_band) {
                require(layer == PixelLayer::three_d,
                    "stored-resolution writes must tag as 3D");
                ++three_d_seen;
            } else {
                require(layer == PixelLayer::two_d,
                    "source-raster writes must tag as 2D");
                ++two_d_seen;
            }
        }
    }
    require(three_d_seen > 0U && two_d_seen > 0U, "both layers must be present");
}

void check_filter(TwoDFilter filter, std::uint32_t scale,
    const std::filesystem::path& dump_directory) {
    const auto palette = make_palette();
    auto frame = render(scale, palette);
    const auto unfiltered = frame.rgba;

    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;
    workers.set_worker_count(1U);
    starfox::render::apply_two_d_filter(
        filter, frame.framebuffer, palette, frame.rgba, scratch, workers);
    auto scenery = render(scale, palette);
    for (auto& tag : scenery.framebuffer.layer_tags()) {
        if (tag == std::uint8_t(PixelLayer::two_d)) tag = std::uint8_t(PixelLayer::background);
    }
    starfox::render::apply_two_d_filter(
        filter, scenery.framebuffer, palette, scenery.rgba, scratch, workers);
    require(scenery.rgba == frame.rgba, "world background tags changed 2D filtering");

    const auto& framebuffer = frame.framebuffer;
    const auto stored_width = framebuffer.stored_width();
    auto changed = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < stored_width; ++x) {
            const auto offset =
                (static_cast<std::size_t>(y) * stored_width + x) * 4U;
            const auto same = frame.rgba[offset] == unfiltered[offset]
                && frame.rgba[offset + 1U] == unfiltered[offset + 1U]
                && frame.rgba[offset + 2U] == unfiltered[offset + 2U];
            if (framebuffer.layer_stored(x, y) == PixelLayer::three_d) {
                require(same, "the filter must never touch 3D-owned pixels");
            } else if (!same) {
                ++changed;
            }
        }
    }
    require(changed > 0U, "the filter must resolve some 2D edges");

    // Threading must not change a single pixel.
    auto threaded = render(scale, palette);
    PixelFilterScratch parallel_scratch;
    starfox::render::RowWorkers parallel_workers;
    parallel_workers.set_worker_count(4U);
    starfox::render::apply_two_d_filter(
        filter, threaded.framebuffer, palette, threaded.rgba, parallel_scratch,
        parallel_workers);
    require(threaded.rgba == frame.rgba,
        "threaded and single-threaded output must match exactly");

    if (!dump_directory.empty()) {
        std::filesystem::create_directories(dump_directory);
        const auto name = std::string{starfox::render::two_d_filter_name(filter)}
            + "-" + std::to_string(scale) + "x.bmp";
        write_bmp(dump_directory / name, frame.rgba,
            framebuffer.stored_width(), framebuffer.stored_height());
        write_bmp(dump_directory / ("OFF-" + std::to_string(scale) + "x.bmp"),
            unfiltered, framebuffer.stored_width(),
            framebuffer.stored_height());
    }
}

void check_disabled_paths() {
    const auto palette = make_palette();
    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;

    // Tags off means no filtering, whatever the scale.
    Framebuffer untagged{source_width, source_height, 4U};
    paint(untagged, 4U);
    std::vector<std::uint8_t> rgba;
    starfox::render::expand_rgba(untagged, rgba, palette);
    const auto untagged_copy = rgba;
    starfox::render::apply_two_d_filter(
        TwoDFilter::edge, untagged, palette, rgba, scratch, workers);
    require(rgba == untagged_copy, "untagged framebuffers must be left alone");

    // OFF is a no-op even with everything else in place.
    auto ready = render(4U, palette);
    const auto ready_copy = ready.rgba;
    starfox::render::apply_two_d_filter(
        TwoDFilter::off, ready.framebuffer, palette, ready.rgba, scratch, workers);
    require(ready.rgba == ready_copy, "OFF must be a no-op");
}

// The draw-scale derivation is only a proxy for layer ownership, and the
// software renderer breaks it on purpose: simple_scaled_sprite shapes and
// sprite faces stay on the source raster, and the cockpit HUD plots there too.
// Those are Super FX output that would otherwise be read as cartridge art and
// handed to the 2D filter, which is what SoftwareRenderer's entry-point
// ScopedLayer exists to prevent. Drive the real renderer rather than a
// hand-tagged scene, so this fails if any of its paths regress.
// Rasterized geometry is the one thing that must never reach the 2D layer.
// Drive a real polygon through the renderer rather than trusting the draw
// scale to speak for it.
void check_polygons_stay_geometry(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);
    // Start every cell owned by cartridge art, so a polygon that fails to
    // claim its pixels is visible as a leftover 2D tag.
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 50U);
        }
    }

    starfox::assets::Shape shape;
    shape.colour_words.push_back(0x0202U);
    shape.vertices = {
        starfox::assets::Vec3i{-60, -60, 0},
        starfox::assets::Vec3i{60, -50, 0},
        starfox::assets::Vec3i{0, 60, 0},
    };
    starfox::assets::Face face;
    face.visibility_index = -1;
    face.colour_id = 0U;
    face.vertex_indices = {0U, 1U, 2U};
    shape.faces.push_back(face);

    starfox::render::RenderPose pose;
    starfox::render::RenderSettings settings;
    settings.render_scale = scale;
    const starfox::render::SoftwareRenderer renderer{settings};
    const auto before = framebuffer.pixels();
    renderer.draw(shape, pose, framebuffer, false, nullptr);

    auto polygon_pixels = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == before[index]) continue;
            ++polygon_pixels;
            require(framebuffer.layer_stored(x, y) == PixelLayer::three_d,
                "rasterized polygons must stay in the geometry layer");
        }
    }
    require(polygon_pixels > 0U, "the polygon drew nothing to check");
}

void check_cockpit_hud_is_filtered(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);
    // Fill the frame with cartridge 2D art so every pixel starts tagged 2D.
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            framebuffer.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 50U);
        }
    }
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "cartridge fill should have tagged the whole frame 2D");
        }
    }

    const auto before = framebuffer.pixels();
    const starfox::render::SoftwareRenderer renderer;
    const starfox::simulation::TrigTables trigonometry;
    renderer.draw_cockpit_hud(trigonometry, 0U, 12U, 0U, 0, framebuffer, 0U);

    auto changed = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == before[index]) continue;
            ++changed;
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "cockpit HUD lines are authored-resolution art and must be "
                "filtered with the rest of the 2D layer");
        }
    }
    require(changed > 0U, "the cockpit HUD drew nothing to check");
}

// Overlays that composite in RGBA after the frame is expanded never reach the
// tagged framebuffer, so they need their own filtering pass or they stay
// blocky while everything behind them resolves.
void check_overlay_layer_is_filtered(std::uint32_t scale) {
    constexpr std::uint32_t width = 96U;
    constexpr std::uint32_t height = 72U;
    const auto palette = make_palette();

    // A source-raster overlay with hard diagonal edges over index 0, which the
    // overlay pass treats as transparent.
    Framebuffer overlay{width, height};
    overlay.clear(0U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto dx = static_cast<int>(x) - 48;
            const auto dy = static_cast<int>(y) - 36;
            if (dx * dx + dy * dy < 400) {
                overlay.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 150U);
            }
        }
    }

    PixelFilterScratch scratch;
    starfox::render::RowWorkers workers;
    std::vector<std::uint32_t> argb;
    require(starfox::render::filter_overlay_layer(TwoDFilter::edge, overlay,
                palette, scale, argb, scratch, workers),
        "the overlay filter should engage above scale 1");
    require(argb.size()
            == static_cast<std::size_t>(width) * scale * height * scale,
        "the overlay filter must fill the stored-resolution buffer");

    // Transparency must survive, art must survive, and the result must differ
    // from plain block expansion somewhere along the circle's edge.
    auto opaque_pixels = std::size_t{0};
    auto differs_from_nearest = std::size_t{0};
    for (std::uint32_t y = 0; y < height * scale; ++y) {
        for (std::uint32_t x = 0; x < width * scale; ++x) {
            const auto colour =
                argb[static_cast<std::size_t>(y) * width * scale + x];
            if (((colour >> 24U) & 0xffU) != 0U) ++opaque_pixels;
            const auto nearest = overlay.get(x / scale, y / scale);
            const auto nearest_opaque = nearest != 0U;
            if (nearest_opaque != (((colour >> 24U) & 0xffU) == 0xffU)) {
                ++differs_from_nearest;
            }
        }
    }
    require(opaque_pixels > 0U, "the overlay filter dropped all of the art");
    require(scale == 1U || differs_from_nearest > 0U,
        "the overlay filter changed nothing against block expansion");

    // Off must decline so the caller keeps its own path.
    require(!starfox::render::filter_overlay_layer(
                TwoDFilter::off, overlay, palette, scale, argb, scratch, workers),
        "OFF must decline");

}

// composite_transparent_layer has two implementations: a bulk transfer for
// equal-scale layers with no mosaic -- which is the one gameplay actually
// takes when the Super FX world reaches the presented framebuffer -- and a
// generic per-cell path for everything else. Both must move layer tags with
// the pixels, so exercise them both rather than whichever one a default
// LayerCompositeSettings happens to select.
void check_composite_carries_layers(std::uint32_t scale, bool force_generic) {
    constexpr std::uint32_t width = 64U;
    constexpr std::uint32_t height = 48U;

    // Destination: a full screen of cartridge 2D art.
    Framebuffer destination{width, height, scale};
    destination.enable_layer_tags(true);
    destination.clear(0U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            destination.set(static_cast<std::int32_t>(x),
                static_cast<std::int32_t>(y), 40U);
        }
    }

    // Source: a Super FX layer holding geometry written at stored resolution,
    // exactly as scan conversion produces it.
    Framebuffer source{width, height, scale};
    source.enable_layer_tags(true);
    source.clear(0U);
    {
        const auto previous = source.draw_scale();
        source.set_draw_scale(1U);
        const starfox::render::ScopedLayer geometry{source, PixelLayer::three_d};
        for (std::uint32_t y = source.stored_height() / 4U;
             y < source.stored_height() * 3U / 4U; ++y) {
            for (std::uint32_t x = source.stored_width() / 4U;
                 x < source.stored_width() * 3U / 4U; ++x) {
                source.set(static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y), 90U);
            }
        }
        source.set_draw_scale(previous);
    }

    starfox::render::LayerCompositeSettings settings;
    if (force_generic) {
        // A mosaic touching this layer disables the bulk transfer.
        settings.mosaic = 0x01U;
        settings.mosaic_layer_mask = 0x01U;
    }
    starfox::render::composite_transparent_layer(source, destination, settings);

    auto geometry = std::size_t{0};
    for (std::uint32_t y = 0; y < destination.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < destination.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * destination.stored_width() + x;
            if (destination.pixels()[index] != 90U) continue;
            ++geometry;
            require(destination.layer_stored(x, y) == PixelLayer::three_d,
                "composited geometry must arrive tagged as geometry");
        }
    }
    require(geometry > 0U, "the composite transferred no geometry to check");
}

// Star Fox draws explosions, asteroids and similar objects as sprites rather
// than polygons: authored texels point-sampled onto the source raster. They
// arrive through the software renderer, but they are cartridge art and belong
// to the 2D layer, or the filter leaves them pixelated while everything around
// them resolves. Drive the real sprite path to hold that classification.
void check_sprites_are_cartridge_art(std::uint32_t scale) {
    constexpr std::uint32_t width = 256U;
    constexpr std::uint32_t height = 224U;
    Framebuffer framebuffer{width, height, scale};
    framebuffer.enable_layer_tags(true);
    framebuffer.clear(0U);

    // A minimal shape whose only content is one solid sprite texture.
    starfox::assets::Shape shape;
    constexpr std::uint16_t descriptor = 0x1234U;
    shape.colour_words.push_back(descriptor);
    starfox::assets::TextureImage texture;
    texture.descriptor = descriptor;
    texture.u_mask = 7U;
    texture.v_mask = 7U;
    texture.texels.assign(64U, 6U);
    shape.textures.push_back(texture);

    starfox::render::RenderPose pose;
    pose.simple_scaled_sprite = true;
    pose.simple_sprite_colour = 0U;
    pose.simple_sprite_world_size = 300;

    starfox::render::RenderSettings settings;
    settings.render_scale = scale;
    const starfox::render::SoftwareRenderer renderer{settings};
    renderer.draw(shape, pose, framebuffer, false, nullptr);

    auto sprite_pixels = std::size_t{0};
    for (std::uint32_t y = 0; y < framebuffer.stored_height(); ++y) {
        for (std::uint32_t x = 0; x < framebuffer.stored_width(); ++x) {
            const auto index =
                static_cast<std::size_t>(y) * framebuffer.stored_width() + x;
            if (framebuffer.pixels()[index] == 0U) continue;
            ++sprite_pixels;
            require(framebuffer.layer_stored(x, y) == PixelLayer::two_d,
                "scaled sprites must reach the 2D layer so they get filtered");
        }
    }
    require(sprite_pixels > 0U, "the sprite drew nothing to check");
}

} // namespace

int main(int argc, char** argv) {
    starfox::render::RowWorkers restart_workers;
    for (const auto count : {1U, 4U, 2U, 1U, 4U}) {
        restart_workers.set_worker_count(count);
        std::array<unsigned, 127> rows{};
        restart_workers.parallel_rows(static_cast<std::uint32_t>(rows.size()),
            [&](std::uint32_t first, std::uint32_t last) {
                for (auto row = first; row < last; ++row) ++rows[row];
            });
        for (const auto visits : rows) {
            require(visits == 1U, "resized worker pool skipped or repeated rows");
        }
    }
    const auto dump_directory = argc > 1
        ? std::filesystem::path{argv[1]} : std::filesystem::path{};

    for (const auto scale : {1U, 2U, 3U, 4U, 6U, 10U}) {
        check_tags(scale);
        check_polygons_stay_geometry(scale);
        check_cockpit_hud_is_filtered(scale);
        check_overlay_layer_is_filtered(scale);
        check_composite_carries_layers(scale, false);
        check_composite_carries_layers(scale, true);
        check_sprites_are_cartridge_art(scale);
    }
    check_disabled_paths();
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer scene{400,224,scale};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,255U), scratch;
        for(unsigned y=80*scale;y<140*scale;++y) for(unsigned x=100*scale;x<200*scale;++x) {
            const auto i=std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i]=std::uint8_t(x<150*scale ? PixelLayer::three_d : PixelLayer::textured_geometry);
            for(unsigned c=0;c<3;++c) original[i*4+c]=x<150*scale?0U:240U;
        }
        unsigned previous=240U;
        starfox::render::RowWorkers workers;
        workers.set_worker_count(4);
        for(std::uint8_t level=0;level<4;++level) {
            auto pixels=original, threaded=original;
            starfox::render::smooth_models(level,scene,pixels,scratch);
            starfox::render::smooth_models(level,scene,threaded,scratch,&workers);
            require(pixels==threaded,"model smoothing differs across worker counts");
            if(level==0) require(pixels==original,"model smoothing Off changed pixels");
            const auto boundary=(std::size_t(110*scale)*scene.stored_width()+150*scale)*4U;
            if(level) require(pixels[boundary]<previous,"model smoothing strength did not increase");
            previous=pixels[boundary];
            for(std::size_t i=0;i<scene.pixels().size();++i) {
                require(pixels[i*4+3]==255U,"model smoothing changed alpha");
                if(scene.layer_tags()[i]==std::uint8_t(PixelLayer::background))
                    require(pixels[i*4]==255U,"model smoothing bled beyond geometry coverage");
            }
        }
    }
    {
        starfox::simulation::SnesPpuState ppu;
        ppu.background_mode=2U;
        ppu.bg2_screen_size=0U;
        ppu.bg2_scanline_scroll_enabled=true;
        for(unsigned i=0;i<1024;++i) ppu.vram[ppu.bg2_screen_base*2U+i*2U]=1U;
        for(unsigned y=0;y<8;++y) ppu.vram[ppu.bg2_character_base*2U+32U+y*2U]=255U;
        starfox::render::BackgroundRenderer renderer;
        for(unsigned width:{400U,512U,768U}) for(unsigned scale:{1U,2U,4U}) {
            Framebuffer wide{width,8,scale}, native{256,8,scale};
            const auto origin=int((width-256U)/2U);
            renderer.draw_bg2(ppu,0,0,wide,starfox::render::TilePriorityPass::all,origin);
            renderer.draw_bg2(ppu,0,0,native);
            for(unsigned y=0;y<8;++y) for(unsigned x=0;x<width;++x) {
                const auto expected=x<unsigned(origin) || x>=unsigned(origin)+256U
                    ? 0U : native.get(x-origin,y);
                require(wide.get(x,y)==expected,"tunnel duplicated artwork into wide borders or changed native centre");
            }
            require(native.get(0,0)!=0U,"tunnel regression fixture is empty");
        }
    }
    for (const auto layer : {PixelLayer::three_d, PixelLayer::background}) {
        Framebuffer scene{32,32};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(),scene.layer_tags().end(),std::uint8_t(layer));
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,0U);
        for (std::size_t i=0;i<scene.pixels().size();++i) original[i*4+3]=255U;
        for (unsigned y=12;y<20;++y) for(unsigned x=12;x<20;++x)
            for(unsigned c=0;c<3;++c) original[(y*32+x)*4+c]=255U;
        starfox::render::BloomPass bloom;
        auto wrong=original, right=original;
        bloom.apply(layer==PixelLayer::three_d ? 0U : 3U,
            layer==PixelLayer::three_d ? 3U : 0U,scene,wrong);
        bloom.apply(layer==PixelLayer::three_d ? 3U : 0U,
            layer==PixelLayer::three_d ? 0U : 3U,scene,right);
        require(wrong==original,"disabled bloom layer still emitted light");
        require(right!=original,"enabled bloom layer emitted no light");
        auto base = original, glow = right, final = right;
        // A late host overlay replaces this pixel after scene bloom.
        final[0] = 17U; final[1] = 33U; final[2] = 91U;
        starfox::render::split_bloom_layer(base, glow, final);
        for (std::size_t i = 0; i < final.size(); i += 4U) {
            for (unsigned c = 0; c < 3; ++c)
                require(unsigned(base[i+c])+glow[i+c] == final[i+c],
                    "separate bloom display layer changed native-resolution colors");
        }
        require(glow[0] == 0U && glow[1] == 0U && glow[2] == 0U,
            "scene bloom contaminated a host overlay");
    }
    for (const unsigned scale : {1U,2U,4U}) {
        Framebuffer scene{8,8,scale};
        scene.clear(1U);
        scene.set(4,4,129U);
        std::vector<std::uint8_t> original(scene.pixels().size()*4U,255U);
        for (const auto amount : {31U,30U,16U,2U,0U}) {
            auto pixels=original;
            starfox::simulation::ColourMathEffectState effect{
                true,true,false,0x27U,std::uint8_t(amount),std::uint8_t(amount),std::uint8_t(amount)};
            starfox::render::apply_colour_math(effect,scene,pixels);
            for (std::size_t i=0; i<scene.pixels().size(); ++i) {
                const auto expected=scene.pixels()[i]>=128U ? 255U : 255U-(amount*8U+(amount>>2U));
                for (unsigned c=0;c<3;++c) require(pixels[i*4+c]==expected,
                    "revival colour window changed OBJ or lost native background fade");
                require(pixels[i*4+3]==255U,"colour window changed alpha");
            }
        }
    }
    // Golden output from the pre-optimization bloom, plus threaded/cache reuse
    // coverage at every supported render scale.
    starfox::render::BloomPass cached_bloom;
    starfox::render::RowWorkers bloom_workers;
    bloom_workers.set_worker_count(4);
    for (const unsigned scale : {1U, 2U, 4U, 2U, 1U}) {
        Framebuffer scene{768, 224, scale};
        scene.enable_layer_tags(true);
        std::vector<std::uint8_t> original(scene.pixels().size()*4);
        for (unsigned y=0; y<scene.stored_height(); ++y) for (unsigned x=0; x<scene.stored_width(); ++x) {
            const auto i=std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i]=std::uint8_t(y<16*scale ? PixelLayer::two_d : PixelLayer::background);
            original[i*4]=(x/scale*13+y/scale*3)%256;
            original[i*4+1]=(x/scale*7+y/scale*5)%256;
            original[i*4+2]=(x/scale*3+y/scale*11)%256;
            original[i*4+3]=255;
        }
        auto serial=original, threaded=original;
        cached_bloom.apply(3,scene,serial);
        cached_bloom.apply(3,scene,threaded,&bloom_workers);
        require(serial==threaded,"threaded bloom differs from serial output");
        std::uint64_t hash=1469598103934665603ULL;
        for (auto v:serial) { hash^=v; hash*=1099511628211ULL; }
        const auto expected=scale==1 ? 13590015223919869339ULL
            : scale==2 ? 2401763838851610420ULL : 7426840680921405174ULL;
        require(hash==expected,"optimized bloom changed reference pixels");
    }
    for (const unsigned scale : {1U, 2U, 4U}) {
        Framebuffer scene{32, 32, scale};
        scene.enable_layer_tags(true);
        std::fill(scene.layer_tags().begin(), scene.layer_tags().end(), std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> original(scene.pixels().size() * 4, 0);
        for (std::size_t i = 0; i < scene.pixels().size(); ++i) original[i*4+3] = 255;
        for (unsigned y = 12*scale; y < 20*scale; ++y) for (unsigned x = 12*scale; x < 20*scale; ++x) {
            const auto i = std::size_t(y)*scene.stored_width()+x;
            scene.layer_tags()[i] = std::uint8_t(PixelLayer::three_d);
            for (unsigned c=0;c<3;++c) original[i*4+c]=255;
        }
        const auto hud = std::size_t(16*scale)*scene.stored_width()+10*scale;
        scene.layer_tags()[hud] = std::uint8_t(PixelLayer::two_d);
        original[hud*4] = 80;
        starfox::render::BloomPass bloom;
        unsigned previous = 0;
        for (std::uint8_t level=0; level<4; ++level) {
            auto pixels=original;
            bloom.apply(level, scene, pixels);
            if (!level) require(pixels == original, "Bloom Off changed output");
            const auto halo=(std::size_t(16*scale)*scene.stored_width()+7*scale)*4;
            if (level) require(pixels[halo]>previous, "Bloom strength did not increase its broad halo");
            previous=pixels[halo];
            require(std::equal(pixels.begin()+hud*4,pixels.begin()+hud*4+4,original.begin()+hud*4), "Bloom changed HUD");
            for (std::size_t i=3;i<pixels.size();i+=4) require(pixels[i]==255,"Bloom changed alpha");
        }
        std::fill(scene.layer_tags().begin(), scene.layer_tags().end(), std::uint8_t(PixelLayer::two_d));
        auto pixels=original;
        bloom.apply(3,scene,pixels);
        require(pixels==original,"HUD-only frame generated bloom");
    }
    for (const bool world : {false,true}) {
        std::uint8_t style=0;
        for (unsigned i=0;i<starfox::render::effect_count*2;++i) {
            style=starfox::render::next_effect(style,world,false);
            require(starfox::render::selectable_effect(style,world),"selector exposed a removed style");
        }
    }
    {
        Framebuffer hud{256U, 224U};
        hud.enable_layer_tags(true);
        starfox::simulation::SnesPpuState ppu{};
        ppu.main_screen = 0x10U;
        ppu.object_select = 0U;
        ppu.oam[0] = 12U;
        ppu.oam[1] = 16U;
        ppu.vram[0] = 0x80U;
        starfox::render::SpriteRenderer renderer;
        renderer.draw_objects(ppu, hud);
        require(hud.layer_stored(12, 16) == PixelLayer::two_d,
            "1x HUD sprites were tagged as effect-eligible models");
        starfox::simulation::MeterState meters{};
        meters.enabled = true;
        meters.boss_max_health = 100U;
        meters.boss_health = 50U;
        renderer.draw_meters(meters, hud);
        require(hud.layer_stored(118, 2) == PixelLayer::two_d,
            "1x boss meter was tagged as an effect-eligible model");
    }
    {
        Framebuffer frame{2U, 1U};
        frame.enable_layer_tags(true);
        frame.set_layer_override(PixelLayer::three_d);
        frame.set(0, 0, 1);
        frame.set_layer_override(PixelLayer::two_d);
        frame.set(1, 0, 2);
        const std::vector<std::uint8_t> original{180, 100, 40, 255, 12, 34, 56, 255};
        std::vector<std::uint8_t> scratch;
        for (unsigned effect = 0; effect < starfox::render::effect_names.size(); ++effect) {
            auto pixels = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, pixels, scratch);
            require(std::equal(pixels.begin() + 4, pixels.end(), original.begin() + 4),
                "effects changed HUD pixels");
            require((pixels == original) == (effect == 0), "effect/off output mismatch");
            require(pixels[3] == 255, "effects changed alpha");
            auto disabled = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, disabled, scratch, 0U);
            require(disabled == original, "zero intensity changed pixels");
            auto halfway = original;
            starfox::render::apply_effect(static_cast<starfox::render::Effect>(effect),
                frame, halfway, scratch, 50U);
            for (unsigned c = 0; c < 3; ++c) require(
                halfway[c] == (unsigned(original[c]) + pixels[c] + 1U) / 2U,
                "intensity did not blend linearly");
        }
    }
    for (const auto scale : {1U, 2U, 3U, 4U, 6U, 10U}) {
        check_filter(TwoDFilter::edge, scale, dump_directory);
        check_filter(TwoDFilter::sharp_bilinear, scale, dump_directory);
        check_filter(TwoDFilter::crt, scale, dump_directory);
        if (starfox::render::two_d_filter_compiled_in(TwoDFilter::xbrz)) {
            check_filter(TwoDFilter::xbrz, scale, dump_directory);
        }
    }

    {
        Framebuffer frame{5U, 5U};
        frame.enable_layer_tags(true);
        frame.set_layer_override(PixelLayer::two_d);
        frame.set(2, 2, 1);
        std::vector<std::uint8_t> pixels(5U * 5U * 4U, 0U), scratch;
        for (std::size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
        for (unsigned c = 0; c < 3; ++c) pixels[(2 * 5 + 2) * 4 + c] = 255;
        const auto original = pixels;
        starfox::render::apply_effect(starfox::render::Effect::bloom, frame, pixels, scratch);
        require(pixels == original, "bright HUD leaked into bloom");
        frame.set_layer_override(PixelLayer::three_d);
        frame.set(2, 2, 1);
        starfox::render::apply_effect(starfox::render::Effect::bloom, frame, pixels, scratch);
        require(pixels[(1 * 5 + 1) * 4] > original[(1 * 5 + 1) * 4],
            "bright world pixel produced no bloom halo");
    }
    {
        Framebuffer frame{4U, 1U};
        frame.enable_layer_tags(true);
        frame.layer_tags() = {std::uint8_t(PixelLayer::three_d), std::uint8_t(PixelLayer::background),
            std::uint8_t(PixelLayer::two_d), std::uint8_t(PixelLayer::world_geometry)};
        const std::vector<std::uint8_t> original{180,100,40,255,180,100,40,255,180,100,40,255,180,100,40,255};
        std::vector<std::uint8_t> scratch;
        using starfox::render::Effect;
        for (unsigned style = 8; style < starfox::render::effect_count; ++style) {
            const auto effect = static_cast<Effect>(style);
            for (const bool world : {false, true}) {
                auto pixels = original;
                starfox::render::apply_effect(world ? Effect::off : effect,
                    frame, pixels, scratch, 100, world ? effect : Effect::off, 100);
                for (unsigned pixel = 0; pixel < 4; ++pixel) {
                    const bool selected = world ? pixel == 1 || pixel == 3 : pixel == 0;
                    const bool same = std::equal(pixels.begin() + pixel * 4,
                        pixels.begin() + pixel * 4 + 3, original.begin() + pixel * 4);
                    require(same != selected, "new style missed its layer or leaked into another");
                    require(pixels[pixel * 4 + 3] == 255, "new style changed alpha");
                }
            }
        }
        auto model_only = original;
        starfox::render::apply_effect(Effect::monochrome, frame, model_only, scratch);
        require(model_only[0] != original[0] && model_only[4] == original[4]
                && model_only[8] == original[8] && model_only[12] == original[12],
            "model effects leaked into world or HUD");
        auto world_only = original;
        starfox::render::apply_effect(Effect::off, frame, world_only, scratch, 100, Effect::monochrome, 100);
        require(world_only[0] == original[0] && world_only[4] != original[4]
                && world_only[8] == original[8] && world_only[12] != original[12],
            "world effects missed scenery or changed model/HUD");
    }
    std::cout << "pixel filter tests passed\n";
    return 0;
}
