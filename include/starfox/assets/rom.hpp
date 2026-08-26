#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace starfox::assets {

class RomImage {
public:
    explicit RomImage(std::vector<std::uint8_t> bytes);

    [[nodiscard]] static RomImage load(const std::filesystem::path& path);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::size_t lorom_offset(std::uint32_t snes_address) const;
    [[nodiscard]] std::uint8_t read8(std::uint32_t snes_address) const;
    [[nodiscard]] std::uint16_t read16(std::uint32_t snes_address) const;
    [[nodiscard]] std::int16_t read_i16(std::uint32_t snes_address) const;

private:
    std::vector<std::uint8_t> bytes_;
};

class SymbolMap {
public:
    using Entries = std::unordered_map<std::string, std::vector<std::uint32_t>>;

    [[nodiscard]] static SymbolMap load(const std::filesystem::path& path);
    [[nodiscard]] static SymbolMap parse(std::string_view text);
    [[nodiscard]] const std::vector<std::uint32_t>& find(const std::string& name) const;
    [[nodiscard]] const Entries& entries() const noexcept;

private:
    Entries symbols_;
};

} // namespace starfox::assets
