#include "starfox/assets/runtime_bundle.hpp"

#include "starfox/assets/bps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace starfox::assets {
namespace {

constexpr std::array<std::uint8_t, 8> magic{
    'S', 'F', 'O', 'X', 'A', 'S', '0', '1'};
constexpr auto header_size = std::size_t{44U};
constexpr auto trailer_size = std::size_t{4U};
constexpr auto maximum_bundle_size = std::size_t{64U * 1024U * 1024U};

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint32_t read_u32(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw std::runtime_error{
            "Starfox-Assets.BIN has a truncated header"};
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

std::uint32_t checked_size(std::size_t size) {
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error{"runtime asset is too large for its bundle"};
    }
    return static_cast<std::uint32_t>(size);
}

void append_bytes(
    std::vector<std::uint8_t>& output,
    std::span<const std::uint8_t> bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

std::span<const std::uint8_t> string_bytes(const std::string& text) {
    return {
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

} // namespace

std::vector<std::uint8_t> encode_runtime_bundle(
    const RuntimeBundlePayload& payload,
    std::uint32_t manifest) {
    const std::array sizes{
        checked_size(payload.original_rom.size()),
        checked_size(payload.original_symbols.size()),
        checked_size(payload.starfox_ex_rom.size()),
        checked_size(payload.starfox_ex_symbols.size()),
    };
    const std::array checksums{
        crc32(payload.original_rom),
        crc32(string_bytes(payload.original_symbols)),
        crc32(payload.starfox_ex_rom),
        crc32(string_bytes(payload.starfox_ex_symbols)),
    };
    auto total_size = header_size + trailer_size;
    for (const auto size : sizes) {
        if (size > maximum_bundle_size - total_size) {
            throw std::runtime_error{"runtime asset bundle is too large"};
        }
        total_size += size;
    }
    std::vector<std::uint8_t> output;
    output.reserve(total_size);
    append_bytes(output, magic);
    append_u32(output, manifest);
    for (const auto size : sizes) append_u32(output, size);
    for (const auto checksum : checksums) append_u32(output, checksum);
    append_bytes(output, payload.original_rom);
    append_bytes(output, string_bytes(payload.original_symbols));
    append_bytes(output, payload.starfox_ex_rom);
    append_bytes(output, string_bytes(payload.starfox_ex_symbols));
    append_u32(output, crc32(output));
    return output;
}

RuntimeBundlePayload decode_runtime_bundle(
    std::span<const std::uint8_t> bytes,
    std::uint32_t expected_manifest) {
    if (bytes.size() < header_size + trailer_size
        || bytes.size() > maximum_bundle_size
        || !std::equal(magic.begin(), magic.end(), bytes.begin())) {
        throw std::runtime_error{
            "Starfox-Assets.BIN is not a supported asset companion"};
    }
    if (read_u32(bytes, 8U) != expected_manifest) {
        throw std::runtime_error{
            "Starfox-Assets.BIN was built for a different game version"};
    }
    const std::array sizes{
        read_u32(bytes, 12U), read_u32(bytes, 16U),
        read_u32(bytes, 20U), read_u32(bytes, 24U),
    };
    const std::array checksums{
        read_u32(bytes, 28U), read_u32(bytes, 32U),
        read_u32(bytes, 36U), read_u32(bytes, 40U),
    };
    auto expected_size = header_size + trailer_size;
    for (const auto size : sizes) {
        if (size > maximum_bundle_size - expected_size) {
            throw std::runtime_error{
                "Starfox-Assets.BIN declares invalid asset sizes"};
        }
        expected_size += size;
    }
    if (bytes.size() != expected_size) {
        throw std::runtime_error{
            "Starfox-Assets.BIN has an invalid file size"};
    }
    if (crc32(bytes.first(bytes.size() - trailer_size))
        != read_u32(bytes, bytes.size() - trailer_size)) {
        throw std::runtime_error{
            "Starfox-Assets.BIN failed its file checksum"};
    }

    auto cursor = header_size;
    const auto take = [&bytes, &cursor](std::size_t size) {
        const auto result = bytes.subspan(cursor, size);
        cursor += size;
        return result;
    };
    const auto original_rom = take(sizes[0]);
    const auto original_symbols = take(sizes[1]);
    const auto ex_rom = take(sizes[2]);
    const auto ex_symbols = take(sizes[3]);
    const std::array payloads{
        original_rom, original_symbols, ex_rom, ex_symbols};
    for (std::size_t index = 0; index < payloads.size(); ++index) {
        if (crc32(payloads[index]) != checksums[index]) {
            throw std::runtime_error{
                "Starfox-Assets.BIN contains a corrupt asset payload"};
        }
    }
    return {
        std::vector<std::uint8_t>{original_rom.begin(), original_rom.end()},
        std::string{reinterpret_cast<const char*>(original_symbols.data()),
            original_symbols.size()},
        std::vector<std::uint8_t>{ex_rom.begin(), ex_rom.end()},
        std::string{reinterpret_cast<const char*>(ex_symbols.data()),
            ex_symbols.size()},
    };
}

} // namespace starfox::assets
