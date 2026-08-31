#include "starfox/assets/bps.hpp"
#include "starfox/assets/embedded.hpp"
#include "starfox/assets/runtime_bundle.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct RetailVariant {
    std::string_view name;
    std::uint32_t crc32;
    int canonicalization_resource;
};

constexpr auto retail_size = std::size_t{1U << 20U};
constexpr auto retail_v12_crc32 = std::uint32_t{0x8fc4e6d0U};
constexpr std::array retail_variants{
    RetailVariant{"Star Fox (USA) (Rev 2)", retail_v12_crc32, 0},
    RetailVariant{"Star Fox (Japan)", 0x41a60b3fU, 120},
    RetailVariant{"Star Fox (Japan) (Rev 1)", 0xad668a41U, 121},
    RetailVariant{"Star Fox (USA)", 0x0bae0941U, 122},
    RetailVariant{"Star Fox (USA) (Rev 1)", 0xb18676b2U, 123},
    RetailVariant{"Starwing (Europe)", 0x865f1a71U, 124},
    RetailVariant{"Starwing (Europe) (Rev 1)", 0xba64da2bU, 125},
    RetailVariant{"Starwing (Germany)", 0xb48ca238U, 126},
};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"unable to open file: " + path.string()};
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
}

std::span<const std::uint8_t> resource(int identifier) {
    return starfox::assets::embedded_asset(identifier);
}

std::string text_resource(int identifier) {
    const auto bytes = resource(identifier);
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::uint32_t asset_manifest() {
    std::vector<std::uint8_t> bytes;
    for (const auto identifier : {
             101, 102, 108, 109, 120, 121, 122, 123, 124, 125, 126}) {
        const auto payload = resource(identifier);
        const auto size = static_cast<std::uint32_t>(payload.size());
        for (std::uint32_t shift = 0; shift < 32U; shift += 8U) {
            bytes.push_back(static_cast<std::uint8_t>(size >> shift));
        }
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    return starfox::assets::crc32(bytes);
}

std::pair<std::string_view, std::vector<std::uint8_t>> canonicalize_retail(
    const std::filesystem::path& path) {
    auto bytes = read_file(path);
    if (bytes.size() == retail_size + 512U) {
        bytes.erase(bytes.begin(), bytes.begin() + 512U);
    }
    const auto checksum = bytes.size() == retail_size
        ? starfox::assets::crc32(bytes)
        : std::uint32_t{};
    const auto variant = std::find_if(retail_variants.begin(),
        retail_variants.end(), [checksum](const RetailVariant& candidate) {
            return candidate.crc32 == checksum;
        });
    if (variant == retail_variants.end()) {
        throw std::runtime_error{
            "input is not a supported unmodified Star Fox/Starwing ROM"};
    }
    if (variant->canonicalization_resource != 0) {
        bytes = starfox::assets::apply_bps_patch(
            bytes, resource(variant->canonicalization_resource));
    }
    if (bytes.size() != retail_size
        || starfox::assets::crc32(bytes) != retail_v12_crc32) {
        throw std::runtime_error{"regional ROM canonicalization failed"};
    }
    return {variant->name, std::move(bytes)};
}

void write_file(const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes) {
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            throw std::runtime_error{"unable to create output directory: "
                + error.message()};
        }
    }
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        throw std::runtime_error{"unable to create output: " + path.string()};
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"unable to write output: " + path.string()};
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3) {
            std::cerr << "usage: starfox_asset_builder RETAIL_ROM "
                         "[Starfox-Assets.BIN]\n";
            return 2;
        }
        const auto input = std::filesystem::path{argv[1]};
        const auto output = argc == 3
            ? std::filesystem::path{argv[2]}
            : std::filesystem::path{"Starfox-Assets.BIN"};
        auto [variant, retail] = canonicalize_retail(input);

        starfox::assets::RuntimeBundlePayload payload;
        payload.original_rom =
            starfox::assets::apply_bps_patch(retail, resource(101));
        payload.original_symbols = text_resource(102);
        payload.starfox_ex_rom =
            starfox::assets::apply_bps_patch(retail, resource(108));
        payload.starfox_ex_symbols = text_resource(109);
        const auto bundle = starfox::assets::encode_runtime_bundle(
            payload, asset_manifest());
        write_file(output, bundle);

        // Decode once before reporting success so a corrupt or incompatible
        // generator can never emit a file that the runtime will later reject.
        static_cast<void>(starfox::assets::decode_runtime_bundle(
            bundle, asset_manifest()));
        std::cout << "Validated " << variant << "\nCreated "
                  << std::filesystem::absolute(output).string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Starfox-Assets.BIN generation failed: "
                  << error.what() << '\n';
        return 1;
    }
}
