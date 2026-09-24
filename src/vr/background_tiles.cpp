#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/packed_vram.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/background_renderer.hpp"
#include <stdexcept>
#include <cmath>
#include <numbers>
#include <mutex>
#include <bit>

namespace starfox::vr {
Matrix4 photographic_scroll_correction(float dx,float dy,double alpha,float horizontal_scale,float vertical_scale) {
    if(!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(alpha)
        || !std::isfinite(horizontal_scale) || horizontal_scale<=0
        || !std::isfinite(vertical_scale) || vertical_scale<=0)
        throw std::invalid_argument("Invalid photographic scroll correction");
    const float weight=float(1-std::clamp(alpha,0.,1.));
    const float yaw=std::remainder(dx,512.F)*weight/(512*horizontal_scale);
    const float pitch=std::atan(-std::remainder(dy,512.F)*weight/(224*vertical_scale));
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    return {cy,0,sy,0, -sy*sp,cp,cy*sp,0, -sy*cp,-sp,cy*cp,0, 0,0,0,1};
}
Matrix4 photographic_body_motion(std::array<float,2> center) {
    for(float value:center) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body motion");
    const float yaw=std::atan((center[0]-128)/512),pitch=std::atan((112-center[1])/512);
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    return {cy,0,sy,0, -sy*sp,cp,cy*sp,0, -sy*cp,-sp,cy*cp,0, 0,0,0,1};
}
DrawPacket photographic_body_packet(std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicBody& options,bool srgb) {
    if(!texture || texture->size()<7 || !backdrop_texture_valid(*texture,(*texture)[5],(*texture)[6])
        || (*texture)[2]!=0 || options.bright>32767 || options.dark>32767)
        throw std::invalid_argument("Invalid photographic body texture/palette");
    for(float value:options.center) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body center");
    for(float value:options.diameter) if(!std::isfinite(value) || value<=0 || value>1024)
        throw std::invalid_argument("Invalid photographic body size");
    for(float value:options.response) if(!std::isfinite(value) || value<0 || value>4096)
        throw std::invalid_argument("Invalid photographic body response");
    for(float value:options.palette_shift) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic body palette shift");
    if((options.palette[0]!=0 && (options.palette[0]<4 || options.palette[0]>8)) || (options.palette[0] && options.two_tone)
        || std::any_of(options.palette.begin()+1,options.palette.end(),[](auto c){return c>32767;}))
        throw std::invalid_argument("Invalid photographic body shade ramp");
    DrawPacket packet;packet.geometry.shared_texels=std::move(texture);
    auto vertices=std::make_shared<std::vector<SceneVertex>>();vertices->reserve(6);
    const float yaw=std::atan((options.center[0]-128)/512),pitch=std::atan((112-options.center[1])/512);
    const float sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    for(const auto& corner:std::array<std::array<float,2>,6>{{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}}) {
        const float u=(corner[0]-.5F)*options.diameter[0]*.125F;
        const float v=(.5F-corner[1])*options.diameter[1]*.125F;
        SceneVertex vertex{};
        vertex.position[0]=64*sy*cp+u*cy-v*sy*sp;
        vertex.position[1]=64*sp+v*cp;
        vertex.position[2]=-64*cy*cp+u*sy+v*cy*sp;
        std::copy(corner.begin(),corner.end(),vertex.uv);
        std::copy(options.response.begin(),options.response.end(),vertex.color);
        std::copy(options.palette_shift.begin(),options.palette_shift.end(),vertex.odd_color);
        vertex.texture[1]=(*packet.geometry.shared_texels)[5]-1;
        vertex.texture[2]=(*packet.geometry.shared_texels)[6]-1;
        vertex.texture[3]=backdrop_texture_flag|(srgb?2U:0U);
        if(options.two_tone) {
            vertex.visibility_a[0]=3;vertex.visibility_a[1]=options.bright;vertex.visibility_a[2]=options.dark;
        }
        if(options.palette[0]) {
            float* ramp[]{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c,vertex.group_a,vertex.group_b,vertex.group_c};
            for(unsigned i=0;i<16;++i) ramp[i/3][i%3]=options.palette[i];
        }
        vertices->push_back(vertex);
    }
    packet.geometry.shared_vertices=std::move(vertices);return packet;
}
DrawPacket photographic_landscape_packet(std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicLandscape& options,bool srgb) {
    if(!texture || texture->size()<7 || !backdrop_texture_valid(*texture,(*texture)[5],(*texture)[6])
        || !std::isfinite(options.horizon_v) || options.horizon_v<(options.full_sphere?-4:0)
        || options.horizon_v>(options.full_sphere?4:1)
        || !std::isfinite(options.vertical_scale) || options.vertical_scale<=0 || options.vertical_scale>16
        || !std::isfinite(options.horizontal_offset) || std::abs(options.horizontal_offset)>16
        || !std::isfinite(options.horizontal_scale) || options.horizontal_scale<=0 || options.horizontal_scale>4
        || options.repeats<1 || options.repeats>16 || (options.latitude_uv && !options.full_sphere)
        || (options.orbital_surface && (!options.full_sphere || options.latitude_uv)))
        throw std::invalid_argument("Invalid photographic landscape projection");
    for(float value:options.response) if(!std::isfinite(value) || value<0 || value>4096)
        throw std::invalid_argument("Invalid photographic landscape response");
    for(float value:options.palette_shift) if(!std::isfinite(value) || std::abs(value)>4096)
        throw std::invalid_argument("Invalid photographic landscape palette shift");
    if(options.cloud_palette[0]>2 || std::any_of(options.cloud_palette.begin()+1,options.cloud_palette.end(),
        [](auto colour){return colour>32767;})) throw std::invalid_argument("Invalid photographic cloud palette");
    DrawPacket packet;packet.geometry.shared_texels=std::move(texture);
    constexpr unsigned columns=64;
    const unsigned rows=options.full_sphere?32:16;
    constexpr float pi=std::numbers::pi_v<float>;
    auto vertices=std::make_shared<std::vector<SceneVertex>>();vertices->reserve(columns*rows*6);
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float longitude=(float(x+corner[0])/columns*2-1)*pi;
            const float latitude=(1.F-float(y+corner[1])/rows*(options.full_sphere?2.F:1.F))*pi*.5F;
            const float radial=64*std::max(0.F,std::cos(latitude));
            SceneVertex vertex{};
            vertex.position[0]=radial*std::sin(longitude);vertex.position[1]=64*std::sin(latitude);
            vertex.position[2]=-radial*std::cos(longitude);
            const float rear=longitude/pi;
            // Keep the forward angular scale unchanged. Spread the small
            // periodic-closure correction toward the rear of the panorama.
            vertex.uv[0]=options.horizontal_offset+longitude*options.horizontal_scale
                +(.5F*options.repeats-pi*options.horizontal_scale)*rear*rear*rear;
            vertex.uv[1]=options.latitude_uv?.5F-latitude/pi
                :std::clamp(options.horizon_v-vertex.position[1]/std::max(radial,.0001F)*options.vertical_scale,0.F,1.F);
            vertex.texture[1]=(*packet.geometry.shared_texels)[5]-1;
            vertex.texture[2]=(*packet.geometry.shared_texels)[6]-1;
            vertex.texture[3]=backdrop_texture_flag|(srgb?2U:0U);
            std::copy(options.response.begin(),options.response.end(),vertex.color);
            std::copy(options.palette_shift.begin(),options.palette_shift.end(),vertex.odd_color);
            vertex.odd_color[3]=options.orbital_surface?4:options.full_sphere?3:1;
            float* ramp[]{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c,
                vertex.group_a,vertex.group_b,vertex.group_c};
            for(unsigned i=0;i<16;++i) ramp[i/3][i%3]=float(options.cloud_palette[i]);
            vertices->push_back(vertex);
        }
    }
    packet.geometry.shared_vertices=std::move(vertices);
    return packet;
}
DrawPacket tunnel_surround_packet(const std::array<float,4>& colour) {
    for(auto value:colour) if(!std::isfinite(value) || value<0 || value>1)
        throw std::invalid_argument("Invalid tunnel surround colour");
    DrawPacket packet;
    const auto quad=[&](std::array<float,3> a,std::array<float,3> b,
        std::array<float,3> c,std::array<float,3> d) {
        for(const auto& point:{a,b,c,a,c,d}) {
            SceneVertex vertex{};std::copy(point.begin(),point.end(),vertex.position);
            std::copy(colour.begin(),colour.end(),vertex.color);
            vertex.color[3]=1;packet.geometry.vertices.push_back(vertex);
        }
    };
    constexpr float l=-1024,r=1280,t=-1024,b=1248,z=4;
    quad({l,t,0},{0,t,0},{0,b,0},{l,b,0});
    quad({256,t,0},{r,t,0},{r,b,0},{256,b,0});
    quad({0,t,0},{256,t,0},{256,0,0},{0,0,0});
    quad({0,224,0},{256,224,0},{256,b,0},{0,b,0});
    quad({l,t,z},{r,t,z},{r,b,z},{l,b,z});
    quad({l,t,0},{l,t,z},{l,b,z},{l,b,0});
    quad({r,t,0},{r,t,z},{r,b,z},{r,b,0});
    quad({l,t,0},{r,t,0},{r,t,z},{l,t,z});
    quad({l,b,0},{r,b,0},{r,b,z},{l,b,z});
    packet.model=source_layer_matrix(128,112,2).value();
    return packet;
}
namespace {
std::shared_ptr<const std::vector<SceneVertex>> make_sky_vertices(bool srgb,bool landscape,float horizon=112.F) {
    auto vertices=std::make_shared<std::vector<SceneVertex>>();
    constexpr unsigned columns=64,rows=32;
    vertices->reserve(columns*rows*6);
    constexpr float pi=std::numbers::pi_v<float>;
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<columns;++x) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float longitude=(float(x+corner[0])/columns*2-1)*pi;
            const float latitude=(.5F-float(y+corner[1])/rows)*pi;
            SceneVertex v{};
            v.position[0]=64*std::cos(latitude)*std::sin(longitude);
            v.position[1]=64*std::sin(latitude);
            v.position[2]=-64*std::cos(latitude)*std::cos(longitude);
            v.uv[0]=128+longitude*512;v.uv[1]=112-latitude*512;
            if(landscape) {
                const float radial=std::max(.0001F,std::hypot(v.position[0],v.position[2]));
                v.uv[1]=std::clamp(horizon-v.position[1]*512/radial,0.F,223.F);
            }
            v.texture[3]=8|(srgb?2:0);vertices->push_back(v);
        }
    }
    return vertices;
}
std::shared_ptr<const std::vector<SceneVertex>> sky_vertices(bool srgb,bool landscape,float horizon=112.F) {
    if(landscape && horizon!=112.F) return make_sky_vertices(srgb,true,horizon);
    // Immutable geometry is shared across ticks, eyes and cartridge changes.
    // Only the palette/VRAM payload and model uniforms remain per-scene data.
    if(landscape) {
        if(srgb) {static const auto vertices=make_sky_vertices(true,true);return vertices;}
        static const auto vertices=make_sky_vertices(false,true);return vertices;
    }
    if(srgb) {static const auto vertices=make_sky_vertices(true,false);return vertices;}
    static const auto vertices=make_sky_vertices(false,false);return vertices;
}
}
std::array<float,4> source_menu_background_colour(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool srgb) {
    // One palette-index sample at the authored menu's outer corner; no image
    // is rasterized/uploaded for the surrounding clear. Respect source scroll.
    render::Framebuffer corner(1,1);
    render::BackgroundRenderer decoder;
    decoder.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,corner);
    return source_backdrop_colour(ppu.cgram[corner.get(0,0)],brightness,srgb);
}
DrawPacket intro_star_sphere_packet(const simulation::SnesPpuState& ppu,unsigned brightness,bool srgb,bool full_atlas,bool upper_left_only,bool retain_source_scroll) {
    BackgroundTileOptions options;options.brightness=brightness;options.transparent_black=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,false);
    if(!retain_source_scroll) {packet.geometry.texels[10]=0;packet.geometry.texels[12]=0;}
    packet.geometry.texels[15]=full_atlas?1:9; // EX's CRED atlas is entirely stars/nebulae; DEMO has a unique planet below.
    if(upper_left_only) packet.geometry.texels[15]=137; // Star-only upper-left 256x256 quadrant.
    // Full-atlas panoramas retain their authored placement even when native
    // scanline deformation is disabled. Resetting both scroll registers here
    // misplaced meteor/nebula bands in every caller sharing this path.
    if(!retain_source_scroll) packet.geometry.texels.resize(272+16384);
    return packet;
}
DrawPacket game_over_star_sphere_packet(const simulation::SnesPpuState& ppu,
    unsigned brightness,unsigned colour_subtract,bool srgb) {
    BackgroundTileOptions options;options.brightness=brightness;options.colour_subtract=colour_subtract;
    options.transparent_black=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();packet.geometry.shared_vertices=sky_vertices(srgb,false);
    auto& data=packet.geometry.texels;
    data[7]|=512U; // Stable star-only patches; keep Andross in the native front layer.
    data[10]=data[12]=0;data[15]=1;data.resize(272+16384);
    return packet;
}
Matrix4 intro_planet_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,float horizon_y) {
    if(!std::isfinite(alpha) || !std::isfinite(horizon_y)) throw std::invalid_argument("Invalid planet motion");
    const int current=uint16_t(current_scroll)&511;
    int delta=current-(uint16_t(previous_scroll)&511);
    if(delta>255) delta-=512;else if(delta< -256) delta+=512;
    const double scroll=current-delta*(1-std::clamp(alpha,0.,1.));
    // Atlas planet center is y=364. Follow the native vertical reveal, but
    // rotate a distant patch rather than moving a close screen-space panel.
    const float angle=float(std::atan((horizon_y-364+scroll)/512.));
    const float c=std::cos(angle),s=std::sin(angle);
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
DrawPacket intro_planet_packet(const simulation::SnesPpuState& ppu,unsigned brightness,bool srgb) {
    BackgroundTileOptions options;options.brightness=brightness;options.transparent_black=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    // DEMO's single planet occupies this atlas rectangle. Use atlas coordinates,
    // not the scrolling foreground window, and place it on the distant sky.
    packet.geometry.texels[3]=packet.geometry.texels[4]=0;
    packet.geometry.texels[10]=packet.geometry.texels[12]=0;
    packet.geometry.texels.resize(272+16384);
    for(auto& vertex:packet.geometry.vertices) {
        const float x=vertex.uv[0]/256.F,y=vertex.uv[1]/224.F;
        vertex.uv[0]=88+x*72;vertex.uv[1]=328+y*72;
        vertex.position[0]=(x-.5F)*9;vertex.position[1]=(.5F-y)*9;
        vertex.position[2]=-64;
    }
    return packet;
}
DrawPacket unique_planet_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,const std::array<unsigned,4>& rectangle,bool srgb) {
    const auto [x,y,w,h]=rectangle;
    if(!w || !h || x>=512 || y>=512 || w>512-x || h>512-y)
        throw std::invalid_argument("Invalid unique planet rectangle");
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    // Use the same effective signed scroll as the ordinary background path,
    // including presentation overrides, before clearing atlas sampling scroll.
    const float cx=float(x)+float(w)*.5F-float(static_cast<int32_t>(packet.geometry.texels[3]));
    const float cy=float(y)+float(h)*.5F-float(static_cast<int32_t>(packet.geometry.texels[4]));
    const float yaw=std::atan((cx-128.F)/512.F),pitch=std::atan((112.F-cy)/512.F);
    const float sy=std::sin(yaw),cyaw=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    packet.geometry.vertices.clear();
    for(const auto& corner:std::array<std::array<float,2>,6>{{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}}) {
        const float u=(corner[0]-.5F)*float(w)*.125F,v=(.5F-corner[1])*float(h)*.125F;
        SceneVertex vertex{};
        vertex.position[0]=64*sy*cp+u*cyaw-v*sy*sp;
        vertex.position[1]=64*sp+v*cp;
        vertex.position[2]=-64*cyaw*cp+u*sy+v*cyaw*sp;
        vertex.uv[0]=float(x)+corner[0]*float(w);vertex.uv[1]=float(y)+corner[1]*float(h);
        vertex.texture[3]=8|(srgb?2:0);packet.geometry.vertices.push_back(vertex);
    }
    auto& data=packet.geometry.texels;
    data[3]=data[4]=data[9]=data[10]=data[11]=data[12]=data[14]=0;data[15]=1;
    data.resize(272+16384);
    return packet;
}
DrawPacket landscape_landmark_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,const std::array<unsigned,4>& rectangle,
    const std::array<bool,256>& keep,bool srgb) {
    auto packet=unique_planet_packet(ppu,options,rectangle,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    const auto corners=packet.geometry.vertices;packet.geometry.vertices.clear();
    const auto [rx,ry,width,height]=rectangle;
    const auto word=[&](unsigned a){return unsigned(ppu.vram[a&65535])|(unsigned(ppu.vram[(a+1)&65535])<<8);};
    const auto selected=[&](unsigned x,unsigned y) {
        const unsigned edge=ppu.bg2_tile_size_16?16:8;
        x=(x+rx)&((ppu.bg2_screen_size&1)?64*edge-1:32*edge-1);
        y=(y+ry)&((ppu.bg2_screen_size&2)?64*edge-1:32*edge-1);
        const unsigned mx=x/edge,my=y/edge,pages=(ppu.bg2_screen_size&1)?2:1;
        const unsigned entry=(mx/32+my/32*pages)*1024+(my%32)*32+mx%32;
        const unsigned tile=word(ppu.bg2_screen_base*2+entry*2);
        unsigned px=x%edge,py=y%edge;
        if(tile&0x4000) px=edge-1-px;if(tile&0x8000) py=edge-1-py;
        const unsigned character=((tile&1023)+(px/8)+(py/8)*16)&1023;
        const unsigned base=ppu.bg2_character_base*2+character*32+(py%8)*2;
        unsigned ink=0;
        for(unsigned bit=0;bit<4;++bit) ink|=((ppu.vram[(base+bit%2+bit/2*16)&65535]>>(7-px%8))&1)<<bit;
        return ink!=0 && keep[((tile>>10)&7)*16+ink];
    };
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;) {
        if(!selected(x,y)) {++x;continue;}
        const unsigned left=x;while(x<width && selected(x,y)) ++x;
        const std::array<std::array<float,2>,4> run{{{float(left)/width,float(y)/height},
            {float(x)/width,float(y)/height},{float(x)/width,float(y+1)/height},
            {float(left)/width,float(y+1)/height}}};
        for(unsigned i:{0U,1U,2U,0U,2U,3U}) {
            auto vertex=corners[0];const auto [u,v]=run[i];
            for(unsigned c=0;c<3;++c) vertex.position[c]+=u*(corners[1].position[c]-corners[0].position[c])
                +v*(corners[5].position[c]-corners[0].position[c]);
            vertex.uv[0]=float(rx)+u*width;vertex.uv[1]=float(ry)+v*height;
            packet.geometry.vertices.push_back(vertex);
        }
    }
    return packet;
}
DrawPacket landscape_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,float horizon_y,bool srgb,bool unique_half,uint16_t atlas_origin,bool unique_right_half) {
    if(!std::isfinite(horizon_y)) throw std::invalid_argument("Invalid landscape horizon");
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,true,horizon_y);
    // Each atlas has a source-specific origin. Its native rotate/offset
    // tables fake camera motion on a flat screen and must not deform a sphere.
    packet.geometry.texels[3]=0;packet.geometry.texels[4]=atlas_origin&511U;
    // Landscapes surround the viewer horizontally, but their terminal ground
    // row must not wrap vertically into sky when the source origin is low.
    packet.geometry.texels[15]|=0x20000000U;
    if(atlas_origin>=288) packet.geometry.texels[15]|=0x40000000U;
    bool wide_unique_left=false;
    if(unique_half || unique_right_half) {
        const auto atlas_width=((packet.geometry.texels[2]&1U)?64U:32U)
            *(packet.geometry.texels[8]?16U:8U);
        if(atlas_width!=512U && atlas_width!=1024U)
            throw std::invalid_argument("Unsupported unique landscape atlas width");
        wide_unique_left=unique_half && atlas_width==1024U;
        packet.geometry.texels[15]|=64U;
        if(unique_right_half) packet.geometry.texels[15]|=128U;
    }
    if(unique_right_half || options.ex_ocean_island || options.ex_volcanic_horizon || wide_unique_left) {
        // These EX atlases put the moon on the first visible sky row.
        // Clamping the upper hemisphere to that row turns it into a stripe;
        // extend into the verified empty sky band immediately above it.
        const auto build_vertices=[&]() -> std::shared_ptr<const std::vector<SceneVertex>> {
          auto vertices=std::make_shared<std::vector<SceneVertex>>(*packet.geometry.shared_vertices);
          for(auto& vertex:*vertices) {
            const auto radial=std::max(.0001F,std::hypot(vertex.position[0],vertex.position[2]));
            vertex.uv[1]=std::clamp(horizon_y-vertex.position[1]*512/radial,
                options.ex_city_planets?-208.F:options.ex_volcanic_horizon?-176.F:(options.ex_ocean_island || wide_unique_left)?-64.F:-32.F,223.F);
            // Repeated stars must continue varying with latitude all the way
            // to the zenith; a clamped source row creates a large empty cap.
            if(options.ex_city_planets && vertex.position[1]>0)
                vertex.uv[1]=horizon_y-std::atan2(vertex.position[1],radial)*512.F;
          }
          return vertices;
        };
        if(horizon_y==112.F) {
            struct CachedSky {
                std::once_flag initialized;
                std::shared_ptr<const std::vector<SceneVertex>> vertices;
            };
            static std::array<CachedSky,8> cache;
            const unsigned variant=options.ex_city_planets?3U:options.ex_volcanic_horizon?2U:
                (options.ex_ocean_island || wide_unique_left)?1U:0U;
            auto& cached=cache[variant*2U+unsigned(srgb)];
            std::call_once(cached.initialized,[&]{cached.vertices=build_vertices();});
            packet.geometry.shared_vertices=cached.vertices;
        } else packet.geometry.shared_vertices=build_vertices();
    }
    packet.geometry.texels[9]=packet.geometry.texels[10]=packet.geometry.texels[11]=0;
    packet.geometry.texels[12]=packet.geometry.texels[14]=0;
    packet.geometry.texels.resize(272+16384);
    return packet;
}
void place_landscape_ground(DrawPacket& packet,float ground_y,bool gpu) {
    if(!std::isfinite(ground_y) || ground_y>=0 || ground_y< -8.F)
        throw std::invalid_argument("Invalid landscape receiver height");
    const auto source=packet.geometry.vertex_view();
    if(source.empty()) return;
    // Reconstruct the lower hemisphere's source projection per fragment;
    // screen-linear UV interpolation across flattened triangles stretches rows.
    packet.geometry.texels[15]|=0x10000000U;
    if(gpu) {
        if(packet.geometry.texels.size()!=272+16384)
            throw std::invalid_argument("GPU landscape receiver requires a plain landscape tile payload");
        static std::mutex cache_mutex;
        static std::shared_ptr<const std::vector<SceneVertex>> cached_source,cached_vertices;
        std::lock_guard lock(cache_mutex);
        if(!packet.geometry.shared_vertices || cached_source!=packet.geometry.shared_vertices) {
            auto prepared=std::make_shared<std::vector<SceneVertex>>(source.begin(),source.end());
            for(size_t triangle=0;triangle+2<prepared->size();triangle+=3) {
                if((*prepared)[triangle].position[1]>0 || (*prepared)[triangle+1].position[1]>0
                    || (*prepared)[triangle+2].position[1]>0) continue;
                for(size_t corner=0;corner<3;++corner) (*prepared)[triangle+corner].texture[3]|=268435456U;
            }
            cached_source=packet.geometry.shared_vertices;cached_vertices=std::move(prepared);
        }
        packet.geometry.vertices.clear();packet.geometry.shared_vertices=cached_vertices;
        packet.geometry.texels.push_back(std::bit_cast<uint32_t>(ground_y));
        return;
    }
    auto vertices=std::make_shared<std::vector<SceneVertex>>(source.begin(),source.end());
    for(size_t triangle=0;triangle+2<vertices->size();triangle+=3) {
        // Horizon vertices are duplicated per triangle. Keep the sky's copy
        // on its distant sphere; pulling it out to the floor's far edge
        // stretches the adjoining cloud triangles into visible wedges.
        if((*vertices)[triangle].position[1]>0 || (*vertices)[triangle+1].position[1]>0
            || (*vertices)[triangle+2].position[1]>0) continue;
        for(size_t corner=0;corner<3;++corner) {
        auto& vertex=(*vertices)[triangle+corner];
        const float radial=std::hypot(vertex.position[0],vertex.position[2]);
        if(radial>.0001F) {
            const float distance=vertex.position[1]<-.0001F
                ?std::min(4096.F,radial*ground_y/vertex.position[1]):4096.F;
            vertex.position[0]*=distance/radial;vertex.position[2]*=distance/radial;
        } else vertex.position[0]=vertex.position[2]=0;
        vertex.position[1]=ground_y;
        }
    }
    packet.geometry.vertices.clear();packet.geometry.shared_vertices=std::move(vertices);
}
DrawPacket water_surround_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg3,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,true);
    // Distant Mode-1 backdrop retains its own atlas origin. The BG2
    // perspective water/bridge layer must not repeat over the sky sphere.
    return packet;
}
DrawPacket water_surface_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,float height,bool srgb) {
    if(!std::isfinite(height) || height<.01F || height>8.F)
        throw std::invalid_argument("Invalid water receiver height");
    auto expanded=options;expanded.expanded_horizontal=true;
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,expanded,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.texels[14]=0; // Water is open, not an enclosed tunnel.
    packet.geometry.texels[11]=1; // Clamp side water sampling; never repeat bridge.
    packet.geometry.texels[15]|=32U; // Fragment-local inverse projection.
    constexpr unsigned columns=1,rows=1;
    packet.geometry.vertices.reserve(columns*rows*6*2);
    // One floor and one authored overhead surface. Geometry, unlike the
    // source screen image, remains planar under independent eye/head poses.
    for(int side:{-1,1}) for(unsigned row=0;row<rows;++row) for(unsigned col=0;col<columns;++col) {
        constexpr unsigned corners[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
        for(const auto& corner:corners) {
            const float x=(float(col+corner[0])/columns*2-1)*64;
            const float z=(float(row+corner[1])/rows*2-1)*64;
            const float depth=std::max(std::abs(z),.01F);
            SceneVertex vertex{};
            vertex.position[0]=x;vertex.position[1]=side*height;vertex.position[2]=z;
            // Behind the viewer sample water-only outer columns, not a
            // mirrored second bridge. Source rows remain within its band.
            vertex.uv[0]=z<0?128+x*256/depth:-256.F;
            vertex.uv[1]=std::clamp(112-side*height*256/depth,0.F,223.F);
            vertex.texture[3]=8|(srgb?2:0);
            packet.geometry.vertices.push_back(vertex);
        }
    }
    return packet;
}
Matrix4 water_height_motion(float previous_height,float current_height,double alpha) {
    if(!std::isfinite(previous_height) || !std::isfinite(current_height)
        || previous_height<.01F || current_height<.01F || previous_height>8.F || current_height>8.F || !std::isfinite(alpha))
        throw std::invalid_argument("Invalid water height motion");
    const auto height=previous_height+(current_height-previous_height)*std::clamp(alpha,0.,1.);
    Matrix4 motion{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    motion[5]=float(height/current_height);
    return motion;
}
DrawPacket space_horizon_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=sky_vertices(srgb,false);
    // Keep source scroll/offset tables and unique-planet policy. Only map the
    // native vertical band onto distant geometry, with space beyond its ends.
    packet.geometry.texels[15]|=options.single_occurrence_top_rows==512?2U:16U;
    return packet;
}
Matrix4 orbital_horizon_motion(float previous_scroll,float current_scroll,double alpha,bool thin,bool entry,bool gameplay) {
    if(!std::isfinite(previous_scroll) || !std::isfinite(current_scroll) || !std::isfinite(alpha))
        throw std::invalid_argument("Invalid orbital horizon motion");
    const double scroll=std::lerp(double(previous_scroll),double(current_scroll),std::clamp(alpha,0.,1.));
    const double horizon=entry?424.:thin?400.:384.;
    const float pitch=float(-std::atan((horizon-scroll-112.)/512.));
    const float c=std::cos(pitch),s=std::sin(pitch);
    // EX's entry/boss horizon needs a clockwise quarter-turn; keep this
    // correction on the planet layer, never on the eye camera or UI.
    if(gameplay && (entry || thin)) return {0,-1,0,0,c,0,s,0,-s,0,c,0,0,0,0,1};
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
DrawPacket orbital_planet_sphere_packet(const simulation::SnesPpuState& ppu,
    const BackgroundTileOptions& options,bool srgb,bool thin_horizon,bool entry_horizon,bool gameplay) {
    auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb);
    if(packet.geometry.vertices.empty()) return packet;
    const auto make=[](bool linear,bool thin,bool entry=false) {
        const float horizon=entry?424.F:thin?400.F:384.F;
        const float depth=entry?40.F:thin?15.F:80.F;
        auto result=std::make_shared<std::vector<SceneVertex>>(*sky_vertices(linear,false));
        for(auto& vertex:*result) {
            const float latitude=(112.F-vertex.uv[1])/512.F;
            // LSB uses rows 384..464; EX's late BG_5_1E uses only
            // rows 400..415. Below either surface, the atlas returns to space.
            // Match the surround's 512 texels/radian at the horizon. A
            // linear fit of this short strip to a hemisphere enlarged its
            // details by 5x (LSB) to 27x (thin EX horizon). Approach the last
            // surface row smoothly farther below; never sample space again.
            vertex.uv[1]=latitude>=0?std::max(0.F,horizon-latitude*512.F)
                :horizon-depth*std::expm1(latitude*512.F/depth);
            // Longitude is undefined at the nadir. Transition to a Cartesian
            // cap before reaching it, keeping the source horizon unchanged.
            const float cap=std::clamp((-latitude-.35F)/.55F,0.F,1.F);
            const float blend=cap*cap*(3.F-2.F*cap);
            // Cartesian cap uses equal texel density on X/Z. The previous
            // fixed X scale stretched surface details by 6.4x..34x along Z.
            vertex.uv[0]=std::lerp(vertex.uv[0],128.F+vertex.position[0]*(depth/128.F),blend);
            vertex.uv[1]=std::lerp(vertex.uv[1],horizon+depth*(.5F+vertex.position[2]/128.F),blend);
        }
        return result;
    };
    static const auto normal=make(false,false),linear=make(true,false);
    static const auto thin_normal=make(false,true),thin_linear=make(true,true);
    static const auto entry_normal=make(false,false,true),entry_linear=make(true,false,true);
    packet.geometry.vertices.clear();
    packet.geometry.shared_vertices=entry_horizon?(srgb?entry_linear:entry_normal):thin_horizon?(srgb?thin_linear:thin_normal):(srgb?linear:normal);
    // Preserve the source horizon's angular height while extending the surface.
    // The atlas mapping places its boundary at latitude zero before this pitch.
    const float scroll=options.scroll_override?float((*options.scroll_override)[1]):float(ppu.bg2_scroll_y);
    packet.model=orbital_horizon_motion(scroll,scroll,1.,thin_horizon,entry_horizon,gameplay);
    auto& data=packet.geometry.texels;
    data[3]=data[4]=data[9]=data[10]=data[11]=data[12]=data[14]=data[15]=0;
    data[15]=0x80000000U|(entry_horizon?0x8000000U:thin_horizon?0x40000000U:0U);
    data.resize(272+16384);
    return packet;
}
Matrix4 landscape_pitch_motion(uint16_t previous_pitch,uint16_t current_pitch,double alpha) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape pitch");
    int delta=int(current_pitch)-int(previous_pitch);
    if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
    const double pitch=double(previous_pitch)+delta*std::clamp(alpha,0.,1.);
    const float angle=float(-pitch*2*std::numbers::pi/65536.);
    const float c=std::cos(angle),s=std::sin(angle);
    return {1,0,0,0,0,c,s,0,0,-s,c,0,0,0,0,1};
}
Matrix4 landscape_scroll_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,
    uint16_t previous_bank,uint16_t current_bank,uint16_t atlas_origin) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape scroll");
    // Native vertical scroll simulates looking up/down on a flat screen.
    // It must not pitch the surrounding VR world when the player steers.
    (void)previous_scroll;(void)current_scroll;(void)atlas_origin;
    constexpr float c=1.F,s=0.F;
    int delta=int(current_bank)-int(previous_bank);
    if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
    const double bank=(previous_bank+delta*std::clamp(alpha,0.,1.))*2*std::numbers::pi/65536.;
    const float bc=float(std::cos(bank)),bs=float(std::sin(bank));
    return {bc,bs,0,0,-bs*c,bc*c,s,0,bs*s,-bc*s,c,0,0,0,0,1};
}
std::array<float,4> source_background_border_colour(const std::array<uint16_t,256>& palette,
    unsigned brightness,bool srgb) {
    // Match the GPU unique-background border selection, not CGRAM zero:
    // that slot can be a model colour (pink in the source intro).
    uint16_t selected=palette.front();unsigned darkest=~0U;
    for(auto colour:palette) {
        const unsigned luma=77U*(colour&31)+150U*((colour>>5)&31)+29U*((colour>>10)&31);
        if(luma<darkest) {darkest=luma;selected=colour;}
        if(luma==0) break;
    }
    return source_backdrop_colour(selected,brightness,srgb);
}
std::array<float,4> source_backdrop_colour(uint16_t bgr555,unsigned brightness,bool srgb) {
    if(brightness>15) throw std::invalid_argument("Invalid backdrop brightness");
    std::array<float,4> result{0,0,0,1};
    for(unsigned channel=0;channel<3;++channel) {
        const unsigned component=(bgr555>>(channel*5))&31;
        const unsigned expanded=(component<<3)|(component>>2);
        const float faded=float(expanded*brightness/15)/255.F;
        result[channel]=!srgb?faded:faded<=.04045F?faded/12.92F:
            std::pow((faded+.055F)/1.055F,2.4F);
    }
    return result;
}
std::vector<SceneVertex> game_over_foreground_vertices(const simulation::SnesPpuState& ppu,bool srgb) {
    constexpr unsigned width=256,height=224;
    render::Framebuffer image(width,height);
    render::BackgroundRenderer{}.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,image);
    std::vector<uint8_t> exterior(width*height);
    std::vector<unsigned> queue;queue.reserve(width*height);
    const auto visit=[&](unsigned i) {
        if(!exterior[i] && (ppu.cgram[image.pixels()[i]]&32767U)==0) {
            exterior[i]=1;queue.push_back(i);
        }
    };
    for(unsigned x=0;x<width;++x) {visit(x);visit((height-1)*width+x);}
    for(unsigned y=0;y<height;++y) {visit(y*width);visit(y*width+width-1);}
    for(size_t cursor=0;cursor<queue.size();++cursor) {
        const auto i=queue[cursor],x=i%width,y=i/width;
        if(x) visit(i-1);if(x+1<width) visit(i+1);
        if(y) visit(i-width);if(y+1<height) visit(i+width);
    }
    // Horizontal runs avoid a quad per source pixel. All colour/fading still
    // comes from the original indexed GPU layer, not a baked RGBA replacement.
    std::vector<SceneVertex> vertices;
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;) {
        if(exterior[y*width+x]) {++x;continue;}
        const unsigned left=x;
        while(x<width && !exterior[y*width+x]) ++x;
        const float corners[4][2]{{float(left),float(y)},{float(x),float(y)},
            {float(x),float(y+1)},{float(left),float(y+1)}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex vertex{};
            vertex.position[0]=vertex.uv[0]=corners[corner][0];
            vertex.position[1]=vertex.uv[1]=corners[corner][1];
            vertex.texture[3]=8|(srgb?2:0);vertices.push_back(vertex);
        }
    }
    return vertices;
}
DrawPacket background_tile_packet(const simulation::SnesPpuState& ppu,
    BackgroundLayer layer,const BackgroundTileOptions& options,bool srgb) {
    unsigned enable=0;
    switch(layer) {
    case BackgroundLayer::bg1:enable=1;break;
    case BackgroundLayer::bg2:enable=2;break;
    case BackgroundLayer::bg3:enable=4;break;
    default:throw std::invalid_argument("Invalid background layer");
    }
    if(options.priority>2 || options.brightness>15 || options.colour_subtract>31 || options.guard_inset>128
        || (options.single_occurrence_top_rows>224 && options.single_occurrence_top_rows!=512))
        throw std::invalid_argument("Invalid background packet options");
    const auto bounds=options.horizontal_bounds;
    if(bounds[0]<-4096 || bounds[1]>4096 || bounds[1]<bounds[0])
        throw std::invalid_argument("Invalid background horizontal bounds");
    DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float left=float(options.guard_inset?std::max(bounds[0],int32_t(options.guard_inset)):bounds[0]);
    const float right=float(options.guard_inset?std::min(bounds[1],256-int32_t(options.guard_inset)):bounds[1]);
    if(!(ppu.main_screen&enable) || right<=left) return packet;
    packet.geometry.texels=background_tile_payload(ppu,layer,options);
    const float corners[4][2]{{left,0},{right,0},{right,224},{left,224}};
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
        SceneVertex vertex{};
        vertex.position[0]=vertex.uv[0]=corners[corner][0];
        vertex.position[1]=vertex.uv[1]=corners[corner][1];
        vertex.texture[3]=8|(srgb?2:0);
        packet.geometry.vertices.push_back(vertex);
    }
    return packet;
}
std::vector<DrawPacket> title_foreground_packets(const simulation::SnesPpuState& ppu,
    unsigned brightness,bool include_bg1,
    std::optional<std::array<int16_t,2>> bg2_scroll,bool srgb) {
    std::vector<DrawPacket> packets;
    if(ppu.background_mode!=1) return packets;
    BackgroundTileOptions options;
    options.brightness=brightness;options.priority=2;options.scroll_override=bg2_scroll;
    packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg2,options,srgb));
    options.scroll_override.reset();
    if(include_bg1) {
        options.priority=0;
        packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg1,options,srgb));
    }
    options.priority=2;
    packets.push_back(background_tile_packet(ppu,BackgroundLayer::bg3,options,srgb));
    return packets;
}
std::vector<uint32_t> background_tile_payload(const simulation::SnesPpuState& ppu,
    BackgroundLayer layer,const BackgroundTileOptions& options) {
    if(options.priority>2) throw std::invalid_argument("Invalid background priority");
    if(options.brightness>15) throw std::invalid_argument("Invalid background brightness");
    if(options.colour_subtract>31) throw std::invalid_argument("Invalid background palette subtraction");
    if(options.guard_inset>128) throw std::invalid_argument("Invalid background guard inset");
    if(options.single_occurrence_top_rows>224 && options.single_occurrence_top_rows!=512)
        throw std::invalid_argument("Invalid unique background row count");
    if(ppu.background_mode<1 || ppu.background_mode>3
        || (layer==BackgroundLayer::bg3 && ppu.background_mode!=1))
        throw std::invalid_argument("Unsupported GPU background mode/layer");
    const bool bg2=layer==BackgroundLayer::bg2;
    unsigned scanlines=bg2?unsigned(ppu.bg2_horizontal_offsets_enabled)
        +2U*unsigned(ppu.bg2_scanline_scroll_enabled):0;
    std::vector<uint32_t> data(272+16384+(scanlines?448:0));
    unsigned mosaic_bit=0;
    switch(layer) {
    case BackgroundLayer::bg1:
        data[0]=ppu.bg1_character_base;data[1]=ppu.bg1_screen_base;data[2]=ppu.bg1_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg1_scroll_x));data[4]=uint32_t(int32_t(ppu.bg1_scroll_y));
        data[5]=ppu.background_mode==3?8:4;data[8]=ppu.bg1_tile_size_16;mosaic_bit=1;break;
    case BackgroundLayer::bg2:
        data[0]=ppu.bg2_character_base;data[1]=ppu.bg2_screen_base;data[2]=ppu.bg2_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg2_scroll_x));data[4]=uint32_t(int32_t(ppu.bg2_scroll_y));
        data[5]=4;data[8]=ppu.bg2_tile_size_16;mosaic_bit=2;break;
    case BackgroundLayer::bg3:
        data[0]=ppu.bg3_character_base;data[1]=ppu.bg3_screen_base;data[2]=ppu.bg3_screen_size;
        data[3]=uint32_t(int32_t(ppu.bg3_scroll_x));data[4]=uint32_t(int32_t(ppu.bg3_scroll_y));
        data[5]=2;data[8]=ppu.bg3_tile_size_16;mosaic_bit=4;break;
    default: throw std::invalid_argument("Invalid background layer");
    }
    if(options.scroll_override) {
        data[3]=uint32_t(int32_t((*options.scroll_override)[0]));
        data[4]=uint32_t(int32_t((*options.scroll_override)[1]));
    }
    data[7]=options.priority|(bg2 && options.expanded_horizontal && options.ex_face_planets?256U:0U);
    data[9]=(ppu.mosaic&mosaic_bit)?(ppu.mosaic>>4)+1:0;
    data[13]=15-options.brightness;
    data[15]=unsigned(options.transparent_black)|(options.wrap_horizontal?0U:2U)
        |(bg2 && options.expanded_horizontal && options.ex_twin_planets?4U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_ocean_island?0x1000000U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_volcanic_horizon?0x2000000U:0U)
        |(bg2 && options.expanded_horizontal && options.ex_city_planets?0x4000000U:0U)
        |(bg2?options.single_occurrence_top_rows<<8:0U);
    data[14]=bg2 && ppu.tunnel_scene && options.expanded_horizontal
        ?unsigned(render::tunnel_wall_index(ppu))+1U:0U;
    data[10]=scanlines;
    data[11]=bg2 && ppu.background_mode==1 && ppu.bg2_scanline_scroll_enabled
        && !ppu.tunnel_scene && options.expanded_horizontal;
    data[12]=bg2 && ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled
        ?(options.expanded_horizontal?2:1):0;
    if(scanlines) for(unsigned row=0;row<224;++row) {
        data[272+16384+row]=uint32_t(int32_t(ppu.bg2_horizontal_offsets[row]));
        data[272+16384+224+row]=uint32_t(int32_t(ppu.bg2_scanline_scroll_y[row]));
    }
    auto colours=ppu.cgram;
    if(bg2 && options.colour_subtract) for(auto& colour:colours) {
        uint16_t dark=0;
        for(unsigned c=0;c<3;++c) dark|=uint16_t(std::max(0,int((colour>>(c*5))&31)-int(options.colour_subtract))<<(c*5));
        colour=dark;
    }
    const auto palette=render::decode_bgr555_palette(colours);
    for(unsigned i=0;i<256;++i) data[16+i]=uint32_t(palette[i].r)
        |(uint32_t(palette[i].g)<<8)|(uint32_t(palette[i].b)<<16)|0xff000000U;
    pack_vram(ppu.vram,std::span<uint32_t,16384>(data.data()+272,16384));
    return data;
}
}
