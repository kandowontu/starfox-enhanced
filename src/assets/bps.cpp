#include "starfox/assets/bps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace starfox::assets {
namespace {

class PatchReader {
public:
    explicit PatchReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - std::min(position_, bytes_.size());
    }

    std::uint8_t byte() {
        if (position_ >= bytes_.size()) fail("unexpected end of patch");
        return bytes_[position_++];
    }

    std::uint64_t number() {
        std::uint64_t result{};
        std::uint64_t shift{1U};
        for (;;) {
            const auto value = byte();
            const auto digit = static_cast<std::uint64_t>(value & 0x7fU);
            if (digit != 0U
                && shift > (std::numeric_limits<std::uint64_t>::max()
                    - result) / digit) {
                fail("integer overflow");
            }
            result += digit * shift;
            if ((value & 0x80U) != 0U) return result;
            if (shift > (std::numeric_limits<std::uint64_t>::max() >> 7U)) {
                fail("integer overflow");
            }
            shift <<= 7U;
            if (result > std::numeric_limits<std::uint64_t>::max() - shift) {
                fail("integer overflow");
            }
            result += shift;
        }
    }

    void skip(std::uint64_t count) {
        if (count > remaining()) fail("metadata exceeds patch size");
        position_ += static_cast<std::size_t>(count);
    }

private:
    [[noreturn]] static void fail(const char* reason) {
        throw std::runtime_error{std::string{"invalid BPS patch: "} + reason};
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

std::uint32_t read_little_u32(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw std::runtime_error{"invalid BPS patch: truncated checksum"};
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

std::int64_t signed_offset(std::uint64_t encoded) {
    const auto magnitude = static_cast<std::int64_t>(encoded >> 1U);
    return (encoded & 1U) != 0U ? -magnitude : magnitude;
}

[[noreturn]] void fail(const char* reason) {
    throw std::runtime_error{std::string{"invalid BPS patch: "} + reason};
}

} // namespace

std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    auto result = std::uint32_t{0xffffffffU};
    for (const auto byte : bytes) {
        result ^= byte;
        for (std::uint32_t bit = 0; bit < 8U; ++bit) {
            const auto mask = static_cast<std::uint32_t>(
                -static_cast<std::int32_t>(result & 1U));
            result = (result >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return result ^ 0xffffffffU;
}

std::vector<std::uint8_t> apply_bps_patch(
    std::span<const std::uint8_t> source,
    std::span<const std::uint8_t> patch) {
    constexpr std::array signature{'B', 'P', 'S', '1'};
    if (patch.size() < signature.size() + 12U
        || !std::equal(signature.begin(), signature.end(), patch.begin())) {
        fail("missing BPS1 signature");
    }

    const auto source_crc_offset = patch.size() - 12U;
    const auto target_crc_offset = patch.size() - 8U;
    const auto patch_crc_offset = patch.size() - 4U;
    if (crc32(patch.first(patch_crc_offset))
        != read_little_u32(patch, patch_crc_offset)) {
        fail("patch checksum mismatch");
    }
    if (crc32(source) != read_little_u32(patch, source_crc_offset)) {
        throw std::runtime_error{
            "BPS source checksum mismatch; this patch requires a different ROM"};
    }

    PatchReader reader{patch.first(source_crc_offset)};
    reader.skip(signature.size());
    const auto source_size = reader.number();
    const auto target_size = reader.number();
    const auto metadata_size = reader.number();
    if (source_size != source.size()) {
        throw std::runtime_error{
            "BPS source size mismatch; this patch requires a different ROM"};
    }
    if (target_size > std::numeric_limits<std::size_t>::max()) {
        fail("target is too large");
    }
    reader.skip(metadata_size);

    std::vector<std::uint8_t> target;
    target.reserve(static_cast<std::size_t>(target_size));
    std::int64_t source_relative{};
    std::int64_t target_relative{};
    while (target.size() < target_size) {
        if (reader.remaining() == 0U) fail("missing action data");
        const auto instruction = reader.number();
        const auto action = static_cast<std::uint8_t>(instruction & 3U);
        const auto length64 = (instruction >> 2U) + 1U;
        if (length64 > target_size - target.size()) {
            fail("action exceeds target size");
        }
        const auto length = static_cast<std::size_t>(length64);
        switch (action) {
        case 0U: { // SourceRead
            const auto offset = target.size();
            if (offset > source.size() || source.size() - offset < length) {
                fail("source-read action exceeds source size");
            }
            target.insert(target.end(), source.begin() + offset,
                source.begin() + offset + length);
            break;
        }
        case 1U: // TargetRead
            if (reader.remaining() < length) {
                fail("target-read action exceeds patch data");
            }
            for (std::size_t index = 0; index < length; ++index) {
                target.push_back(reader.byte());
            }
            break;
        case 2U: { // SourceCopy
            source_relative += signed_offset(reader.number());
            if (source_relative < 0
                || static_cast<std::uint64_t>(source_relative) > source.size()
                || source.size() - static_cast<std::size_t>(source_relative)
                    < length) {
                fail("source-copy action exceeds source size");
            }
            const auto begin = static_cast<std::size_t>(source_relative);
            target.insert(target.end(), source.begin() + begin,
                source.begin() + begin + length);
            source_relative += static_cast<std::int64_t>(length);
            break;
        }
        case 3U: { // TargetCopy; byte-at-a-time preserves overlap semantics.
            target_relative += signed_offset(reader.number());
            if (target_relative < 0
                || static_cast<std::uint64_t>(target_relative) >= target.size()) {
                fail("target-copy action starts outside target data");
            }
            for (std::size_t index = 0; index < length; ++index) {
                const auto source_index = static_cast<std::uint64_t>(
                    target_relative) + index;
                if (source_index >= target.size()) {
                    fail("target-copy action exceeds available target data");
                }
                target.push_back(target[static_cast<std::size_t>(source_index)]);
            }
            target_relative += static_cast<std::int64_t>(length);
            break;
        }
        default:
            fail("unknown action");
        }
    }
    if (reader.position() != source_crc_offset) {
        fail("unused data before checksums");
    }
    if (crc32(target) != read_little_u32(patch, target_crc_offset)) {
        fail("target checksum mismatch");
    }
    return target;
}

} // namespace starfox::assets
