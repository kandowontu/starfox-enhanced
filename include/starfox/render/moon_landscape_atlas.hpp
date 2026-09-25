#pragma once
#include "starfox/render/backdrop_image.hpp"
namespace starfox::render {
// One immutable upload combines a repeating landscape with two unique moons.
// Only master changes rebuild it; camera/palette motion changes uniforms.
class MoonLandscapeAtlas {
    BackdropImage image_;
    std::uint64_t landscape_key_{},moon_key_{};
    std::array<float,4> bounds_{};
public:
    const BackdropImage& image(const BackdropImage& landscape,const BackdropImage& moon,
        std::array<float,4> bounds={31,29,1216,1209}) {
        if(image_.immutable_upload_key && landscape_key_==landscape.immutable_upload_key
            && moon_key_==moon.immutable_upload_key && bounds_==bounds) return image_;
        landscape_key_=landscape.immutable_upload_key;moon_key_=moon.immutable_upload_key;bounds_=bounds;
        image_.immutable_upload_key=0;image_.width=landscape.width;image_.height=landscape.height*2;
        image_.pixels.resize(std::size_t(image_.width)*image_.height);
        std::copy(landscape.pixels.begin(),landscape.pixels.end(),image_.pixels.begin());
        const unsigned slot=landscape.width/2;
        for(unsigned y=0;y<landscape.height;++y) for(unsigned x=0;x<landscape.width;++x) {
            const unsigned body=std::min(x/slot,1U);
            const float u=float(x-body*slot)/float(slot-1),v=float(y)/float(landscape.height-1);
            // Registered celestial master bounds, excluding its black margin.
            const auto sample=moon.sample_projected((bounds[0]+u*(bounds[2]-bounds[0]))/1253.f,
                (bounds[1]+v*(bounds[3]-bounds[1]))/1253.f,3);
            const float light=std::clamp((sample[0]*.2126f+sample[1]*.7152f+sample[2]*.0722f)/170.f,0.f,1.f);
            std::uint32_t pixel=0xff000000U;
            for(unsigned c=0;c<3;++c) {
                pixel|=std::uint32_t(255.f*light+.5f)<<(c*8);
            }
            image_.pixels[std::size_t(y+landscape.height)*landscape.width+x]=pixel;
        }
        image_.seal_for_upload();return image_;
    }
};
}
