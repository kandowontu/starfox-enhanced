#include "starfox/assets/embedded.hpp"
#include "starfox/render/enhanced_backdrop_library.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include <iostream>

int main() try {
    using namespace starfox::render;
    for(unsigned i=0;i<enhanced_backdrop_assets.size();++i) {
        // Decode one master at a time; this package check need not retain the
        // entire art collection in memory as if every level were active.
        EnhancedBackdropLibrary library;
        const auto& image=library.get(i,[](unsigned id,std::string_view) {
            return starfox::assets::embedded_asset(int(id));
        });
        if(!image.width || !image.height || !image.immutable_upload_key
            || image.pixels.size()!=std::size_t(image.width)*image.height)
            throw std::runtime_error("Invalid decoded backdrop storage");
        const auto texture=starfox::vr::make_backdrop_texture(image);
        if(!starfox::vr::backdrop_texture_valid(*texture,image.width,image.height))
            throw std::runtime_error("Invalid photographic mip pyramid");
        if(i==0) {
            const auto panorama=starfox::vr::make_landscape_texture(image,false,false,8);
            const auto overlap=std::max(1U,image.width/8);
            if(!starfox::vr::backdrop_texture_valid(*panorama,image.width-overlap,image.height))
                throw std::runtime_error("Invalid Corneria panorama with widened seam");
        }
        std::cout<<"Decoded embedded backdrop "<<i<<": "<<enhanced_backdrop_assets[i].path
            <<" ("<<image.width<<'x'<<image.height<<")\n";
    }
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"VR backdrop resource failure: "<<error.what()<<'\n';return 1;
}
