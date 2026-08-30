#include "starfox/audio/msu1_pack.hpp"

#include "starfox/assets/bps.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <span>

namespace starfox::audio {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{
    'S', 'F', 'E', 'M', 'S', 'U', '1', 0};
constexpr std::uint32_t kVersion = 1U;
constexpr std::uint64_t kHeaderSize =
    16U + static_cast<std::uint64_t>(msu1_track_count) * 24U;

std::uint32_t read_u32(std::span<const std::uint8_t> bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint64_t read_u64(std::span<const std::uint8_t> bytes) noexcept {
    auto value = std::uint64_t{};
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        value |= static_cast<std::uint64_t>(bytes[shift / 8U]) << shift;
    }
    return value;
}

} // namespace

Msu1Pack::Msu1Pack(std::filesystem::path path) noexcept
    : path_(std::move(path)) {
    try {
        std::ifstream input{path_, std::ios::binary | std::ios::ate};
        if (!input) return;
        const auto end = input.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) < kHeaderSize) return;
        const auto file_size = static_cast<std::uint64_t>(end);
        input.seekg(0, std::ios::beg);
        std::array<std::uint8_t, kHeaderSize> header{};
        input.read(reinterpret_cast<char*>(header.data()),
            static_cast<std::streamsize>(header.size()));
        if (!input || !std::equal(kMagic.begin(), kMagic.end(), header.begin())
            || read_u32(std::span{header}.subspan<8U, 4U>()) != kVersion
            || read_u32(std::span{header}.subspan<12U, 4U>())
                != msu1_track_count) {
            return;
        }

        auto previous_end = kHeaderSize;
        for (std::size_t index = 0U; index < entries_.size(); ++index) {
            const auto record = std::span{header}.subspan(
                16U + index * 24U, 24U);
            auto& entry = entries_[index];
            entry.offset = read_u64(record.first<8U>());
            entry.size = read_u64(record.subspan<8U, 8U>());
            entry.checksum = read_u32(record.subspan<16U, 4U>());
            const auto reserved = read_u32(record.subspan<20U, 4U>());
            if (entry.size == 0U || reserved != 0U
                || entry.offset < previous_end || entry.offset > file_size
                || entry.size > file_size - entry.offset) {
                entries_ = {};
                return;
            }
            previous_end = entry.offset + entry.size;
        }
        if (previous_end != file_size) {
            entries_ = {};
            return;
        }
        available_ = true;
    } catch (...) {
        entries_ = {};
        available_ = false;
    }
}

std::vector<std::uint8_t> Msu1Pack::load_track(
    std::uint16_t track) const noexcept {
    try {
        if (!available_ || track == 0U || track > entries_.size()) return {};
        const auto& entry = entries_[track - 1U];
        if (entry.size > std::numeric_limits<std::size_t>::max()
            || entry.size > static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max())) return {};
        std::ifstream input{path_, std::ios::binary};
        if (!input) return {};
        input.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.size));
        input.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!input || starfox::assets::crc32(bytes) != entry.checksum) {
            return {};
        }
        return bytes;
    } catch (...) {
        return {};
    }
}

} // namespace starfox::audio
