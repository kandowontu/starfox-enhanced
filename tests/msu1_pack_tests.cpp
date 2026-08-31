#include "starfox/audio/msu1_pack.hpp"
#include "starfox/assets/bps.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::vector<std::uint8_t> make_test_pack() {
    constexpr std::array<std::uint8_t, 8> magic{
        'S', 'F', 'E', 'M', 'S', 'U', '1', 0};
    constexpr std::uint64_t header_size =
        16U + starfox::audio::msu1_track_count * 24U;
    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), magic.begin(), magic.end());
    append_u32(bytes, 1U);
    append_u32(bytes, starfox::audio::msu1_track_count);
    auto offset = header_size;
    for (std::uint16_t track = 1U;
         track <= starfox::audio::msu1_track_count; ++track) {
        const std::array payload{
            static_cast<std::uint8_t>(track),
            static_cast<std::uint8_t>(track ^ 0x5aU)};
        append_u64(bytes, offset);
        append_u64(bytes, payload.size());
        append_u32(bytes, starfox::assets::crc32(payload));
        append_u32(bytes, 0U);
        offset += payload.size();
    }
    for (std::uint16_t track = 1U;
         track <= starfox::audio::msu1_track_count; ++track) {
        bytes.push_back(static_cast<std::uint8_t>(track));
        bytes.push_back(static_cast<std::uint8_t>(track ^ 0x5aU));
    }
    return bytes;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-msu1-pack-test.pak";
    const auto bytes = make_test_pack();
    {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(output), "could not write MSU-1 test pack");
    }

    const starfox::audio::Msu1Pack pack{path};
    require(pack.available(), "valid MSU-1 companion was not detected");
    require(pack.load_track(1U) == std::vector<std::uint8_t>({1U, 0x5bU})
            && pack.load_track(52U)
                == std::vector<std::uint8_t>({52U, 110U}),
        "MSU-1 companion returned the wrong track payload");
    require(pack.load_track(0U).empty()
            && pack.load_track(53U).empty(),
        "MSU-1 companion accepted an invalid track number");

    const auto case_variant_path = path.parent_path()
        / "STARFOX-ENHANCED-MSU1-PACK-TEST.PAK";
    const starfox::audio::Msu1Pack case_variant_pack{case_variant_path};
    require(case_variant_pack.available(),
        "case-sensitive host did not discover the MSU-1 companion by name");

    auto corrupt = bytes;
    corrupt.back() ^= 0xffU;
    {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(corrupt.data()),
            static_cast<std::streamsize>(corrupt.size()));
    }
    const starfox::audio::Msu1Pack corrupt_pack{path};
    require(corrupt_pack.available()
            && corrupt_pack.load_track(52U).empty(),
        "MSU-1 companion did not reject a corrupt track checksum");

    corrupt[0] ^= 0xffU;
    {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(corrupt.data()),
            static_cast<std::streamsize>(corrupt.size()));
    }
    const starfox::audio::Msu1Pack invalid_pack{path};
    require(!invalid_pack.available(),
        "invalid MSU-1 companion header was reported as available");

    std::error_code error;
    std::filesystem::remove(path, error);
    require(!error, "could not remove MSU-1 test pack");
    std::cout << "All MSU-1 pack tests passed.\n";
    return 0;
}
