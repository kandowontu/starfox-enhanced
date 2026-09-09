#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/row_workers.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace starfox::render {
// Split the already tone-mapped bloom contribution for linear-filtered display.
// Later host overlays must remain untouched and must not receive scene glow.
inline void split_bloom_layer(std::vector<std::uint8_t>& base,
    std::vector<std::uint8_t>& glow, const std::vector<std::uint8_t>& final) {
    if (base.size() != final.size() || glow.size() != final.size()
        || final.size() % 4U != 0U) return;
    for (std::size_t i = 0; i < final.size(); i += 4U) {
        const auto unchanged = glow[i] == final[i] && glow[i+1] == final[i+1]
            && glow[i+2] == final[i+2] && glow[i+3] == final[i+3];
        for (unsigned c = 0; c < 3; ++c) {
            glow[i+c] = unchanged ? static_cast<std::uint8_t>(
                std::max(0, int(glow[i+c]) - int(base[i+c]))) : 0U;
            if (!unchanged) base[i+c] = final[i+c];
        }
        base[i+3] = final[i+3];
        glow[i+3] = 255U;
    }
}
// Scene-wide, linear-light bright pass with separate tight and broad halos.
// Work at half native resolution so cost stays bounded at high render scales.
class BloomPass {
    using Colour = std::array<float, 3>;
    std::vector<Colour> bright_, temp_, core_, halo_;
    struct Sample { unsigned first, second; float fraction; };
    std::vector<Sample> x_samples_;
    unsigned sample_step_{};
    static void blur(const std::vector<Colour>& source, std::vector<Colour>& target,
        unsigned width, unsigned height, int radius, bool horizontal) {
        target.resize(source.size());
        const unsigned lines = horizontal ? height : width, length = horizontal ? width : height;
        const auto index = [&](unsigned line, int p) {
            const auto clamped = unsigned(std::clamp(p, 0, int(length) - 1));
            return horizontal ? std::size_t(line) * width + clamped : std::size_t(clamped) * width + line;
        };
        for (unsigned line = 0; line < lines; ++line) {
            Colour sum{};
            for (int p = -radius; p <= radius; ++p)
                for (unsigned c = 0; c < 3; ++c) sum[c] += source[index(line, p)][c];
            for (unsigned p = 0; p < length; ++p) {
                for (unsigned c = 0; c < 3; ++c) {
                    target[index(line, p)][c] = sum[c] / float(radius * 2 + 1);
                    sum[c] += source[index(line, int(p) + radius + 1)][c]
                        - source[index(line, int(p) - radius)][c];
                }
            }
        }
    }
public:
    void apply(std::uint8_t level, const Framebuffer& frame, std::vector<std::uint8_t>& rgba,
        RowWorkers* workers = nullptr) {
        apply(level, level, frame, rgba, workers);
    }
    void apply(std::uint8_t model_level, std::uint8_t world_level,
        const Framebuffer& frame, std::vector<std::uint8_t>& rgba,
        RowWorkers* workers = nullptr) {
        if (model_level > 3U || world_level > 3U) return;
        const auto level = std::max(model_level, world_level);
        constexpr std::array<float, 4> strength{0, 0.4F, 0.85F, 1.5F};
        if (level == 0 || level > 3 || !frame.layer_tags_enabled()
            || rgba.size() != frame.pixels().size() * 4 || rgba.empty()) return;
        const auto step = 2U * frame.draw_scale();
        const auto width = (frame.stored_width() + step - 1) / step;
        const auto height = (frame.stored_height() + step - 1) / step;
        // These depend only on an 8-bit input or the viewport, not the frame.
        static const auto linear_table = [] {
            std::array<float, 256> table{};
            for (unsigned i = 0; i < table.size(); ++i) {
                const auto value = i / 255.0F;
                table[i] = value * value;
            }
            return table;
        }();
        static const auto contribution_table = [&] {
            std::array<float, 256> table{};
            for (unsigned i = 0; i < table.size(); ++i) {
                const auto peak = linear_table[i];
                const auto knee = std::clamp(peak - 0.25F, 0.0F, 0.3F);
                table[i] = std::max(peak - 0.4F, knee*knee / 0.6F) / std::max(peak, 0.001F);
            }
            return table;
        }();
        if (x_samples_.size() != frame.stored_width() || sample_step_ != step) {
            sample_step_ = step;
            x_samples_.resize(frame.stored_width());
            for (unsigned x = 0; x < frame.stored_width(); ++x) {
                const float fx = std::max(0.0F, (float(x) + 0.5F) / step - 0.5F);
                const auto first = unsigned(fx);
                x_samples_[x] = {std::min(first, width-1), std::min(first+1, width-1), fx-first};
            }
        }
        bright_.resize(std::size_t(width) * height);
        // Each reduced cell owns a disjoint rectangle. Accumulate locally in
        // the same y-then-x order as the original raster traversal, so floating
        // point sums stay identical without atomics or cross-worker reduction.
        const auto extract = [&](unsigned first_row, unsigned last_row) {
            for (unsigned cell_y=first_row; cell_y<last_row; ++cell_y) {
                const auto end_y=std::min((cell_y+1)*step,frame.stored_height());
                for (unsigned cell_x=0; cell_x<width; ++cell_x) {
                    const auto end_x=std::min((cell_x+1)*step,frame.stored_width());
                    Colour sample{};
                    for (unsigned y=cell_y*step; y<end_y; ++y) {
                        for (unsigned x=cell_x*step; x<end_x; ++x) {
                            const auto i=std::size_t(y)*frame.stored_width()+x;
                            const auto tag=frame.layer_tags()[i];
                            if (tag==std::uint8_t(PixelLayer::two_d)) continue;
                            const auto source_level=(tag==std::uint8_t(PixelLayer::three_d)
                                || tag==std::uint8_t(PixelLayer::textured_geometry))
                                ? model_level : world_level;
                            if (source_level==0U) continue;
                            const auto peak=std::max({rgba[i*4],rgba[i*4+1],rgba[i*4+2]});
                            auto contribution=contribution_table[peak];
                            if (source_level!=level) contribution*=strength[source_level]/strength[level];
                            if (contribution==0.0F) continue;
                            for (unsigned c=0;c<3;++c)
                                sample[c]+=linear_table[rgba[i*4+c]]*contribution/float(step*step);
                        }
                    }
                    bright_[std::size_t(cell_y)*width+cell_x]=sample;
                }
            }
        };
        if (workers && frame.pixels().size()>=1024U*1024U) {
            workers->parallel_rows(height,extract);
        } else {
            // On small buffers another worker barrier costs more than it
            // saves. Keep the contiguous raster traversal for that case and
            // for callers without a worker pool.
            std::fill(bright_.begin(),bright_.end(),Colour{});
            for (unsigned y=0;y<frame.stored_height();++y) for (unsigned x=0;x<frame.stored_width();++x) {
                const auto i=std::size_t(y)*frame.stored_width()+x;
                const auto tag=frame.layer_tags()[i];
                if (tag==std::uint8_t(PixelLayer::two_d)) continue;
                const auto source_level=(tag==std::uint8_t(PixelLayer::three_d)
                    || tag==std::uint8_t(PixelLayer::textured_geometry))?model_level:world_level;
                if (source_level==0U) continue;
                const auto peak=std::max({rgba[i*4],rgba[i*4+1],rgba[i*4+2]});
                auto contribution=contribution_table[peak];
                if (source_level!=level) contribution*=strength[source_level]/strength[level];
                if (contribution==0.0F) continue;
                auto& sample=bright_[std::size_t(y/step)*width+x/step];
                for (unsigned c=0;c<3;++c)
                    sample[c]+=linear_table[rgba[i*4+c]]*contribution/float(step*step);
            }
        }
        blur(bright_, temp_, width, height, 1, true);
        blur(temp_, core_, width, height, 1, false);
        blur(core_, temp_, width, height, 4, true);
        blur(temp_, halo_, width, height, 4, false);
        for (std::size_t i=0; i<core_.size(); ++i) for (unsigned c=0; c<3; ++c)
            core_[i][c] = core_[i][c] * 0.6F + halo_[i][c] * 0.8F;
        const auto compose = [&](unsigned first_row, unsigned last_row) {
        for (unsigned y = first_row; y < last_row; ++y) {
            const float fy = std::max(0.0F, (float(y) + 0.5F) / step - 0.5F);
            const auto y0 = unsigned(fy);
            const auto row0 = std::size_t(std::min(y0, height-1)) * width;
            const auto row1 = std::size_t(std::min(y0+1, height-1)) * width;
            for (unsigned x = 0; x < frame.stored_width(); ++x) {
            const auto i = std::size_t(y) * frame.stored_width() + x;
            if (frame.layer_tags()[i] == std::uint8_t(PixelLayer::two_d)) continue;
            // Bilinear reconstruction keeps the half-resolution pass smooth.
            const auto sx = x_samples_[x];
            for (unsigned c = 0; c < 3; ++c) {
                const auto glow = std::lerp(std::lerp(core_[row0+sx.first][c],core_[row0+sx.second][c],sx.fraction),
                    std::lerp(core_[row1+sx.first][c],core_[row1+sx.second][c],sx.fraction),fy-y0);
                rgba[i*4+c] = std::uint8_t(std::sqrt(std::clamp(linear_table[rgba[i*4+c]] + glow*strength[level], 0.0F, 1.0F))*255.0F + 0.5F);
            }
        }
        }
        };
        if (workers && frame.pixels().size() >= 128U * 1024U) workers->parallel_rows(frame.stored_height(), compose);
        else compose(0, frame.stored_height());
    }
};
}
