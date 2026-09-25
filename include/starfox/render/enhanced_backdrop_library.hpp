#pragma once
#include "starfox/render/backdrop_image.hpp"
#include <optional>
#include <string_view>
#include <utility>

namespace starfox::render {
// Shared desktop/OpenXR artwork identity. Resource IDs stay stable for existing
// Windows resources and portable embedded bundles. Loading remains lazy.
struct EnhancedBackdropAsset {
    std::string_view path;
    bool prepare_zenith{};
};
inline constexpr std::array enhanced_backdrop_assets{
    EnhancedBackdropAsset{"assets/enhanced-backdrops/alpine-day-v2.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/macbeth-dusk-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/storm-night-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/desert-horizon-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/orbital-clouds-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/orbital-volcanic-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/city-night-v1.bmp",true},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/nebula-red-blue-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/cloud-plains-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/gold-storm-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/dune-horizon-v2.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/distant-snow-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/titania-clouds-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/ocean-clouds-v2.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/night-coast-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/day-coast-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/red-cloud-band-v1.bmp",true},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/rocky-green-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/dense-city-v1.bmp",true},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/asteroid-belt-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/fine-debris-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/ember-nebula-v2.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/comet-corona-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/cratered-planet-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/storm-planet-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/banded-planet-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/blue-cloud-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/ember-sky-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/jade-planet-horizon-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/dimension-vortex-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/amber-blue-cloud-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/inferno-band-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/spectral-clouds-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/face-moon-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/deep-space-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/orbital-ocean-surface-v1.bmp"},
    EnhancedBackdropAsset{"assets/enhanced-backdrops/orbital-lava-surface-v1.bmp"}};

// One owner/thread per rendering session; returned image addresses stay stable.
class EnhancedBackdropLibrary {
    std::array<std::optional<BackdropImage>,enhanced_backdrop_assets.size()> images_;
public:
    static constexpr unsigned resource_id(unsigned index) {
        if(index>=enhanced_backdrop_assets.size()) throw std::out_of_range("Invalid enhanced backdrop index");
        return 200+index;
    }
    // The loader returns bytes with a lifetime covering this call. Decode and
    // preparation are transactional: a failed load leaves the entry retryable.
    template<class Loader> const BackdropImage& get(unsigned index,Loader&& loader) {
        auto& image=images_.at(index);
        if(!image) {
            const auto& asset=enhanced_backdrop_assets[index];
            const auto bytes=loader(resource_id(index),asset.path);
            auto prepared=BackdropImage::decode(bytes);
            if(asset.prepare_zenith) prepared.prepare_zenith();
            prepared.seal_for_upload();
            image=std::move(prepared);
        }
        return *image;
    }
    std::optional<unsigned> index_of(const BackdropImage* image) const noexcept {
        for(unsigned i=0;i<images_.size();++i)
            if(images_[i] && &*images_[i]==image) return i;
        return std::nullopt;
    }
};
}
