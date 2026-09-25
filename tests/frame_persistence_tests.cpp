#include "starfox/render/frame_persistence.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox::render;
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
int main() {
    try {
        Framebuffer frame(4,2);frame.enable_layer_tags(true);
        std::fill(frame.layer_tags().begin(),frame.layer_tags().end(),std::uint8_t(PixelLayer::background));
        FramePersistence persistence;
        const auto black=[](){std::vector<std::uint8_t> v(32,0);for(unsigned i=3;i<32;i+=4)v[i]=255;return v;};
        auto pixels=black();
        persistence.apply(frame,pixels,PersistenceMode::off,true,false,0,1);
        require(persistence.allocated_bytes()==0,"disabled persistence allocates history");
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::three_d);
        pixels[0]=240;
        persistence.apply(frame,pixels,PersistenceMode::trails,true,false,0,1);
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
        pixels=black();
        persistence.apply(frame,pixels,PersistenceMode::trails,true,false,.35,1);
        require(pixels[0]==120,"moving model left no half-life trail");
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::two_d);
        pixels=black();pixels[0]=17;
        persistence.apply(frame,pixels,PersistenceMode::trails,true,false,.4,1);
        require(pixels[0]==17,"history altered HUD");
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
        pixels=black();persistence.apply(frame,pixels,PersistenceMode::trails,true,false,.5,1);
        require(pixels[0]==0,"trail survived under HUD");
        for(unsigned fps:{60U,120U,240U}) {
            persistence.reset();pixels=black();pixels[0]=240;
            frame.layer_tags()[0]=std::uint8_t(PixelLayer::textured_geometry);
            persistence.apply(frame,pixels,PersistenceMode::trails,true,false,0,1);
            frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
            for(unsigned tick=1;tick<=fps;++tick) {
                pixels=black();persistence.apply(frame,pixels,PersistenceMode::trails,true,false,double(tick)/fps,1);
            }
            require(pixels[0]==33,"trail lifetime depends on presentation FPS");
        }
        pixels=black();persistence.apply(frame,pixels,PersistenceMode::trails,true,false,1.01,2);
        require(pixels[0]==0,"previous scene survived epoch change");
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::three_d);pixels[0]=200;
        persistence.apply(frame,pixels,PersistenceMode::long_exposure,true,false,2,2);
        frame.layer_tags()[0]=std::uint8_t(PixelLayer::background);
        for(unsigned tick=1;tick<=30;++tick) {
            pixels=black();persistence.apply(frame,pixels,PersistenceMode::long_exposure,true,false,2+tick*.1,2);
        }
        require(pixels[0]==200,"long exposure unexpectedly erased its trail");
        Framebuffer resized(2,4);resized.enable_layer_tags(true);
        std::fill(resized.layer_tags().begin(),resized.layer_tags().end(),std::uint8_t(PixelLayer::background));
        pixels=black();persistence.apply(resized,pixels,PersistenceMode::long_exposure,true,false,5.1,2);
        require(pixels[0]==0,"same-area resize retained stale history");
        pixels=black();persistence.apply(frame,pixels,PersistenceMode::long_exposure,true,false,1,2);
        require(pixels[0]==0,"rewinding time retained stale history");
        persistence.apply(frame,pixels,PersistenceMode::off,true,false,1,2);
        require(persistence.allocated_bytes()==0,"disabling persistence retained buffers");
        std::cout<<"Frame persistence: trails, long exposure, FPS, HUD, epoch and off-memory checks pass\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
