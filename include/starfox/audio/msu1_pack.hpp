#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace starfox::audio {

inline constexpr std::string_view msu1_pack_filename{"Starfox-MSU1.PAK"};
inline constexpr std::uint16_t msu1_track_count = 52U;

// Read-only companion archive for the optional lossless MSU-1 soundtrack.
// Construction validates only the small index; an individual FLAC is read
// and checksum-verified when the cartridge selects that track.
class Msu1Pack {
public:
    explicit Msu1Pack(std::filesystem::path path) noexcept;

    [[nodiscard]] bool available() const noexcept { return available_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }
    [[nodiscard]] std::vector<std::uint8_t> load_track(
        std::uint16_t track) const noexcept;

private:
    struct Entry {
        std::uint64_t offset{};
        std::uint64_t size{};
        std::uint32_t checksum{};
    };

    std::filesystem::path path_;
    std::array<Entry, msu1_track_count> entries_{};
    bool available_{};
};

} // namespace starfox::audio
