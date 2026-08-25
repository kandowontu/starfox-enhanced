#include "starfox/assets/decrunch.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>

namespace starfox::assets {
namespace {

class ReverseBits {
public:
    explicit ReverseBits(std::span<const std::uint8_t> source)
        : source_(source), cursor_(source.size()) {
        if (cursor_ < 8U) {
            throw std::runtime_error{"crunched stream is too short"};
        }

        // MDECRU stores the output length as a big-endian 32-bit suffix but
        // consumes only its low 16 bits while walking backwards.
        output_size_ = source_[--cursor_];
        output_size_ |= static_cast<std::uint32_t>(source_[--cursor_]) << 8U;
        cursor_ -= 2U;

        refill();
        if (buffer_ == 0U) {
            throw std::runtime_error{"crunched stream has no suffix marker"};
        }
        const auto suffix_bits = std::bit_width(buffer_);
        buffer_ &= (std::uint32_t{1} << (suffix_bits - 1U)) - 1U;
        remaining_ = suffix_bits - 1U;
    }

    [[nodiscard]] std::uint32_t output_size() const noexcept {
        return output_size_;
    }

    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }

    [[nodiscard]] std::uint32_t take(std::uint32_t count) {
        if (count > 24U) {
            throw std::invalid_argument{"decrunch bit request is too large"};
        }
        std::uint32_t result = 0U;
        while (count-- != 0U) {
            if (remaining_ == 0U) refill();
            result = (result << 1U) | (buffer_ & 1U);
            buffer_ >>= 1U;
            --remaining_;
        }
        return result;
    }

private:
    void refill() {
        if (cursor_ < 4U) {
            throw std::runtime_error{"crunched stream underflow"};
        }
        buffer_ = source_[--cursor_];
        buffer_ |= static_cast<std::uint32_t>(source_[--cursor_]) << 8U;
        buffer_ |= static_cast<std::uint32_t>(source_[--cursor_]) << 16U;
        buffer_ |= static_cast<std::uint32_t>(source_[--cursor_]) << 24U;
        remaining_ = 32U;
    }

    std::span<const std::uint8_t> source_;
    std::size_t cursor_{};
    std::uint32_t buffer_{};
    std::uint32_t remaining_{};
    std::uint32_t output_size_{};
};

std::uint32_t offset_to_lorom(std::size_t offset) {
    const auto bank = static_cast<std::uint32_t>(offset / 0x8000U);
    const auto address = static_cast<std::uint32_t>(offset % 0x8000U) + 0x8000U;
    return (bank << 16U) | address;
}

} // namespace

DecrunchResult decrunch_reverse(const RomImage& rom, std::uint32_t end_address) {
    const auto end_offset = rom.lorom_offset(end_address);
    const auto begin_limit = end_offset > 0x10000U ? end_offset - 0x10000U : 0U;
    const auto source = std::span<const std::uint8_t>{rom.bytes()}.subspan(
        begin_limit, end_offset - begin_limit);
    ReverseBits bits{source};
    if (bits.output_size() == 0U
        || bits.output_size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error{"invalid decrunched length"};
    }

    DecrunchResult result;
    result.bytes.resize(bits.output_size());
    auto destination = result.bytes.size();

    const auto write_raw = [&](std::uint32_t count) {
        if (count > destination) {
            throw std::runtime_error{"raw decrunch run exceeds output"};
        }
        while (count-- != 0U) {
            result.bytes[--destination] = static_cast<std::uint8_t>(bits.take(8U));
        }
    };
    const auto write_match = [&](std::uint32_t count, std::uint32_t offset) {
        if (count > destination || offset == 0U
            || destination + offset > result.bytes.size()) {
            throw std::runtime_error{"invalid decrunch back-reference"};
        }
        while (count-- != 0U) {
            --destination;
            result.bytes[destination] = result.bytes[destination + offset];
        }
    };

    while (destination != 0U) {
        auto raw_count = bits.take(3U);
        if (raw_count != 0U) {
            if (raw_count < 7U) {
                write_raw(raw_count);
            } else if (bits.take(1U) == 0U) {
                write_raw(bits.take(4U) + 7U);
            } else if ((raw_count = bits.take(10U)) != 0U) {
                write_raw(raw_count);
            } else {
                write_raw(bits.take(18U));
            }
        }
        if (destination == 0U) break;

        auto control = bits.take(2U);
        std::uint32_t match_count{};
        std::uint32_t offset{};
        if (control == 0U) {
            match_count = 2U;
            offset = bits.take(8U);
        } else if (control == 1U) {
            match_count = 3U;
            offset = bits.take(bits.take(1U) != 0U ? 8U : 14U);
        } else if (control == 2U) {
            match_count = 4U;
        } else {
            control = bits.take(2U);
            if (control < 2U) {
                match_count = control + 5U;
            } else if (control == 2U) {
                match_count = bits.take(2U) + 7U;
            } else {
                match_count = bits.take(8U);
            }
        }

        if (match_count > 3U) {
            if (bits.take(1U) == 0U) {
                offset = bits.take(16U);
            } else if (bits.take(1U) != 0U) {
                offset = bits.take(8U);
            } else {
                offset = bits.take(12U);
            }
        }
        write_match(match_count, offset);
    }

    result.compressed_begin = offset_to_lorom(begin_limit + bits.cursor());
    return result;
}

} // namespace starfox::assets
