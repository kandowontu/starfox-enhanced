#include "starfox/assets/rom.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace starfox::assets {
namespace {

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

} // namespace

RomImage::RomImage(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    if (bytes_.empty()) {
        throw std::invalid_argument{"ROM image is empty"};
    }
    if ((bytes_.size() % 0x8000U) != 0) {
        throw std::invalid_argument{"ROM image must not include a copier header"};
    }
}

RomImage RomImage::load(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"unable to open ROM image: " + path.string()};
    }
    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    return RomImage{std::move(bytes)};
}

std::size_t RomImage::size() const noexcept {
    return bytes_.size();
}

std::size_t RomImage::lorom_offset(std::uint32_t snes_address) const {
    const auto bank = static_cast<std::uint8_t>((snes_address >> 16U) & 0xffU);
    const auto address = static_cast<std::uint16_t>(snes_address & 0xffffU);
    if (address < 0x8000U) {
        std::ostringstream message;
        message << "LoROM address $" << std::hex << snes_address
                << " is outside the cartridge window";
        throw std::out_of_range{message.str()};
    }

    const auto offset = static_cast<std::size_t>(bank & 0x7fU) * 0x8000U
        + static_cast<std::size_t>(address & 0x7fffU);
    if (offset >= bytes_.size()) {
        std::ostringstream message;
        message << "LoROM address $" << std::hex << snes_address
                << " exceeds the supplied image";
        throw std::out_of_range{message.str()};
    }
    return offset;
}

std::uint8_t RomImage::read8(std::uint32_t snes_address) const {
    return bytes_.at(lorom_offset(snes_address));
}

std::uint16_t RomImage::read16(std::uint32_t snes_address) const {
    const auto low = read8(snes_address);
    const auto high = read8(snes_address + 1U);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8U));
}

std::int16_t RomImage::read_i16(std::uint32_t snes_address) const {
    return static_cast<std::int16_t>(read16(snes_address));
}

SymbolMap SymbolMap::load(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"unable to open symbol map: " + path.string()};
    }
    const std::string text{
        std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    return parse(text);
}

SymbolMap SymbolMap::parse(std::string_view text) {
    SymbolMap result;
    std::istringstream stream{std::string{text}};
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream row{line};
        std::string name;
        std::string address_text;
        if (!(row >> name >> address_text) || address_text.size() < 2 || address_text[0] != '$') {
            continue;
        }
        try {
            const auto address = static_cast<std::uint32_t>(
                std::stoul(address_text.substr(1), nullptr, 16));
            result.symbols_[upper(std::move(name))].push_back(address);
        } catch (const std::exception&) {
            // Ignore assembler report rows that are not simple symbols.
        }
    }
    return result;
}

const std::vector<std::uint32_t>& SymbolMap::find(const std::string& name) const {
    static const std::vector<std::uint32_t> empty;
    const auto iterator = symbols_.find(upper(name));
    return iterator == symbols_.end() ? empty : iterator->second;
}

const SymbolMap::Entries& SymbolMap::entries() const noexcept {
    return symbols_;
}

} // namespace starfox::assets
