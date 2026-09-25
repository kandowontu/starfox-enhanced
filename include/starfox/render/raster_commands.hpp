#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>
#include <unordered_map>
namespace starfox::render {
// Ordered native scanline commands. Projection, clipping and BSP selection
// retain source arithmetic; pixel coverage/material sampling can execute on
// either backend without substituting hardware triangle edge conventions.
struct RasterCommand {
    std::int32_t left{},top{},right{},bottom{};
    std::uint32_t even{},odd{},dither{},tag{};
    std::uint32_t texture_offset{},u_mask{},v_mask{},colour_base{};
    std::int32_t u{},v{},du{},dv{};
    std::array<float,4> surface{};
    // Solid GPU row spans: reserved0 is dither scale; reserved1 bit 0 selects
    // edge-only coverage at original X endpoints u/v (not clipped bounds).
    // Bit 1 marks a wave span: du is the signed source phase offset and dv
    // the animation phase. GPU row lookup must explicitly enable wave rows.
    // Bit 2 masks solid coverage using little-endian uint32 rows in texels:
    // texture_offset is aligned byte offset, u_mask is aligned byte stride,
    // v_mask is row count. Bits use absolute stored-frame X/Y coordinates.
    // Textured commands use these fields for texture scroll instead.
    // textured=4 is SNES OBJ 4bpp: texture_offset points to a 64 KiB VRAM
    // snapshot, u/v are unclipped stored origins, du is draw scale, dv size,
    // reserved0 is the base byte address, reserved1 bits 0/1 are X/Y flips.
    // textured=5 composites indexed layers: u/v logical offsets, du/dv source/
    // destination scale, u_mask/v_mask stored source extents, even/odd mosaic
    // origins, reserved0 mosaic step; dither enables per-pixel tags at reserved1.
    // textured=6 decodes 24 font bytes (12 little-endian 16-bit rows): u/v
    // unclipped stored origin, du draw scale, dv output glyph height.
    // textured=7 decodes 8 bitmap-font row bytes: same origin/scale, dv square
    // output edge, nearest-neighbour source sampling (not endpoint sampling).
    // textured=8 decodes a 32x40 column-major SNES 4bpp portrait (640 bytes).
    // u/v stored origin, du scale, dv enables the source 7:6 aspect correction.
    // Index zero is opaque; colour_base is added to all 16 possible indices.
    std::uint32_t has_surface{},textured{},reserved0{},reserved1{};
};
static_assert(sizeof(RasterCommand)==96);
class RasterCommands {
public:
    void reset(std::uint32_t width,std::uint32_t height) {
        width_=width;height_=height;commands.clear();texels.clear();textures_.clear();
    }
    void add(RasterCommand command) {
        if(command.right<=0 || command.bottom<=0 || command.left>=int(width_) || command.top>=int(height_)
            || command.left>=command.right || command.top>=command.bottom) return;
        commands.push_back(command);
    }
    std::uint32_t texture(std::span<const std::uint8_t> pixels) {
        const auto found=textures_.find(pixels.data());
        if(found!=textures_.end()) return found->second;
        const auto offset=std::uint32_t(texels.size());
        texels.insert(texels.end(),pixels.begin(),pixels.end());
        textures_.emplace(pixels.data(),offset);return offset;
    }
    // Mutable cartridge memory must not use pointer-only texture deduplication.
    std::uint32_t snapshot(std::span<const std::uint8_t> bytes) {
        const auto offset=std::uint32_t(texels.size());
        texels.insert(texels.end(),bytes.begin(),bytes.end());return offset;
    }
    void bin_rows() {
        const auto tiles=(width_+63)/64;
        rows.assign(std::size_t(height_)*tiles+1,0);
        const auto visit=[&](const RasterCommand& c,const auto& action) {
            const auto first=unsigned(std::max(0,c.left))/64;
            const auto last=(unsigned(std::min(int(width_),c.right))+63)/64;
            for(int y=std::max(0,c.top);y<std::min(int(height_),c.bottom);++y)
                for(unsigned tile=first;tile<last;++tile) action(unsigned(y)*tiles+tile);
        };
        for(const auto& c:commands) visit(c,[&](unsigned row){++rows[row+1];});
        for(std::size_t row=1;row<rows.size();++row) rows[row]+=rows[row-1];
        indices.resize(rows.back());cursors_=rows;
        for(std::uint32_t i=0;i<commands.size();++i) {
            visit(commands[i],[&](unsigned row){indices[cursors_[row]++]=i;});
        }
    }
    std::uint32_t width() const {return width_;}
    std::uint32_t height() const {return height_;}
    std::vector<RasterCommand> commands;
    std::vector<std::uint8_t> texels;
    std::vector<std::uint32_t> rows,indices;
private:
    std::uint32_t width_{},height_{};
    std::vector<std::uint32_t> cursors_;
    std::unordered_map<const std::uint8_t*,std::uint32_t> textures_;
};
}
