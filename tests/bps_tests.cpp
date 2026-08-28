#include "starfox/assets/bps.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/runtime_bundle.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

std::vector<std::uint8_t> load_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) throw std::runtime_error{"unable to read " + path.string()};
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void encode_number(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (;;) {
        auto byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value == 0U) {
            bytes.push_back(static_cast<std::uint8_t>(byte | 0x80U));
            return;
        }
        bytes.push_back(byte);
        --value;
    }
}

std::vector<std::uint8_t> literal_patch(
    const std::vector<std::uint8_t>& source,
    const std::vector<std::uint8_t>& target) {
    std::vector<std::uint8_t> patch{'B', 'P', 'S', '1'};
    encode_number(patch, source.size());
    encode_number(patch, target.size());
    encode_number(patch, 0U);
    encode_number(patch, ((target.size() - 1U) << 2U) | 1U);
    patch.insert(patch.end(), target.begin(), target.end());
    append_u32(patch, starfox::assets::crc32(source));
    append_u32(patch, starfox::assets::crc32(target));
    append_u32(patch, starfox::assets::crc32(patch));
    return patch;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string crc_fixture{"123456789"};
        require(starfox::assets::crc32({
                    reinterpret_cast<const std::uint8_t*>(crc_fixture.data()),
                    crc_fixture.size()}) == 0xcbf43926U,
            "CRC32 did not match its standard check value");

        const std::vector<std::uint8_t> source{1U, 2U, 3U, 4U};
        const std::vector<std::uint8_t> target{
            9U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
        const auto patch = literal_patch(source, target);
        require(starfox::assets::apply_bps_patch(source, patch) == target,
            "literal BPS patch did not reconstruct its target");
        auto corrupt_patch = patch;
        corrupt_patch[4U] ^= 1U;
        auto rejected_corrupt_patch = false;
        try {
            static_cast<void>(
                starfox::assets::apply_bps_patch(source, corrupt_patch));
        } catch (const std::runtime_error&) {
            rejected_corrupt_patch = true;
        }
        require(rejected_corrupt_patch,
            "corrupt BPS patch was not rejected");

        const starfox::assets::RuntimeBundlePayload bundle_payload{
            source, "ORIGINAL SYMBOLS", target, "EX SYMBOLS"};
        constexpr auto bundle_manifest = std::uint32_t{0x1234abcdU};
        const auto bundle = starfox::assets::encode_runtime_bundle(
            bundle_payload, bundle_manifest);
        const auto decoded_bundle = starfox::assets::decode_runtime_bundle(
            bundle, bundle_manifest);
        require(decoded_bundle.original_rom == bundle_payload.original_rom
                && decoded_bundle.original_symbols
                    == bundle_payload.original_symbols
                && decoded_bundle.starfox_ex_rom
                    == bundle_payload.starfox_ex_rom
                && decoded_bundle.starfox_ex_symbols
                    == bundle_payload.starfox_ex_symbols,
            "runtime asset bundle did not round-trip its payloads");
        auto corrupt_bundle = bundle;
        corrupt_bundle[corrupt_bundle.size() / 2U] ^= 1U;
        auto rejected_corrupt_bundle = false;
        try {
            static_cast<void>(starfox::assets::decode_runtime_bundle(
                corrupt_bundle, bundle_manifest));
        } catch (const std::runtime_error&) {
            rejected_corrupt_bundle = true;
        }
        require(rejected_corrupt_bundle,
            "corrupt runtime asset bundle was not rejected");

        if (argc == 6) {
            const auto retail = starfox::assets::RomImage::load(argv[1]);
            const auto original_patch = load_bytes(argv[2]);
            const auto original_expected =
                starfox::assets::RomImage::load(argv[3]);
            const auto ex_patch = load_bytes(argv[4]);
            const auto ex_expected = starfox::assets::RomImage::load(argv[5]);
            require(starfox::assets::apply_bps_patch(
                        retail.bytes(), original_patch)
                    == original_expected.bytes(),
                "v1.2 retail ROM did not reconstruct UltraStarFox exactly");
            require(starfox::assets::apply_bps_patch(retail.bytes(), ex_patch)
                    == ex_expected.bytes(),
                "v1.2 retail ROM did not reconstruct Star Fox EX exactly");
        } else if (argc != 1) {
            throw std::runtime_error{
                "usage: starfox_bps_tests [BASE ORIGINAL_BPS ORIGINAL_TARGET "
                "EX_BPS EX_TARGET]"};
        }
        std::cout << "BPS tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BPS tests failed: " << error.what() << '\n';
        return 1;
    }
}
