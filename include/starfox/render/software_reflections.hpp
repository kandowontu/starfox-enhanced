#pragma once
#include "starfox/render/shadow_mask.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/palette.hpp"

namespace starfox::render {

struct SoftwareReflectionSettings {
    shadows::Camera camera;
    std::int32_t offset_x{}, offset_y{};
    unsigned quality{}, intensity{};
    unsigned metallic{}; // Same conductor codes as DXR; cheap fixed tint on CPU.
    std::optional<shadows::ReceiverPlane> ground;
};

// A deliberately bounded CPU alternative, not a DXR fallback: one bounce,
// flat reflected materials, no recursive/roughness rays and no frame history.
// LOW/MEDIUM/HIGH trace at quarter/half/native logical resolution regardless
// of Render Upscale. Full-resolution ownership checks protect HUD and edges.
inline void apply_software_reflections(const shadows::Scene& scene,
    const SurfaceBuffer& surfaces, const Framebuffer& frame,
    const Framebuffer* background, std::span<const Rgba8> palette,
    std::vector<std::uint8_t>& rgba, SoftwareReflectionSettings settings,
    RowWorkers* workers=nullptr) {
    using namespace shadows;
    if (!settings.quality || !settings.intensity || surfaces.empty()
        || !frame.layer_tags_enabled() || palette.size()<256
        || rgba.size()!=std::size_t(frame.stored_width())*frame.stored_height()*4
        || !std::isfinite(settings.camera.focal_length) || settings.camera.focal_length<=0
        || !std::isfinite(settings.camera.vertical_focal_length()) || settings.camera.vertical_focal_length()<=0) return;
    const auto normalized=[](Vec3 v) {
        const auto length=std::sqrt(dot(v,v));
        return std::isfinite(length) && length>1e-12?v*(1.0/length):Vec3{};
    };
    const auto owns=[&](unsigned x,unsigned y) -> const SurfaceSample* {
        if(x>=frame.stored_width() || y>=frame.stored_height()) return nullptr;
        const auto sx=int(x)-settings.offset_x, sy=int(y)-settings.offset_y;
        if(sx<0 || sy<0 || sx>=int(surfaces.width()) || sy>=int(surfaces.height())) return nullptr;
        const auto layer=frame.layer_stored(x,y);
        if(layer!=PixelLayer::three_d && layer!=PixelLayer::textured_geometry) return nullptr;
        const auto& sample=surfaces.get(unsigned(sx),unsigned(sy));
        return sample.valid && sample.palette_index==frame.get_stored(x,y)
            && std::isfinite(sample.depth) && sample.depth>0 ? &sample : nullptr;
    };
    // Out-of-view rays use a cheap hemispherical sky/ground probe made from
    // the authored background alone. Never reflect the HUD or repeat planets.
    std::array<Rgba8,16> environment{};
    const auto background_colour=[&](unsigned x,unsigned y) {
        if(background->layer_tags_enabled()) {
            const auto layer=background->layer_stored(x,y);
            if(layer!=PixelLayer::background && layer!=PixelLayer::world_geometry && layer!=PixelLayer::terrain_geometry) return Rgba8{};
        }
        return palette[background->get_stored(x,y)];
    };
    if(background && background->stored_width() && background->stored_height()) {
        for(unsigned row=0;row<environment.size();++row) {
            unsigned r=0,g=0,b=0;
            const auto y=(2*row+1)*background->stored_height()/(2*environment.size());
            for(unsigned col=0;col<32;++col) {
                const auto x=(2*col+1)*background->stored_width()/64;
                const auto c=background_colour(x,y);r+=c.r;g+=c.g;b+=c.b;
            }
            environment[row]={std::uint8_t(r/32),std::uint8_t(g/32),std::uint8_t(b/32),255};
        }
    }
    const auto environment_colour=[&](Vec3 direction) {
        if(background && direction.z>1e-6) {
            const auto px=settings.camera.center_x+settings.camera.focal_length*direction.x/direction.z+settings.offset_x;
            const auto py=settings.camera.center_y+settings.camera.vertical_focal_length()*direction.y/direction.z+settings.offset_y;
            if(px>=0 && py>=0 && px<background->stored_width() && py<background->stored_height())
                return background_colour(unsigned(px),unsigned(py));
        }
        return environment[std::clamp(int((direction.y*.5+.5)*16),0,15)];
    };
    const auto step=std::max(1U,frame.draw_scale())*(settings.quality==1?4U:settings.quality==2?2U:1U);
    const auto trace=[&](unsigned sx,unsigned sy) -> std::optional<Rgba8> {
            const Vec3 ray{(sx+.5-settings.offset_x-settings.camera.center_x)/settings.camera.focal_length,
                (sy+.5-settings.offset_y-settings.camera.center_y)/settings.camera.vertical_focal_length(),1};
            const auto primary=scene.nearest_hit({},ray,1,65536,true);
            if(!primary) return {};
            const auto& face=scene.triangles()[primary->triangle];
            auto normal=normalized(cross(face.b-face.a,face.c-face.a));
            const auto incident=normalized(ray);
            if(dot(normal,incident)>0) normal=normal*(-1);
            const auto direction=incident-normal*(2*dot(incident,normal));
            const auto bias=std::max(.1,primary->distance*1e-5);
            const auto origin=ray*primary->distance+normal*bias;
            double maximum=65536;
            bool ground_hit=false;
            if(settings.ground) {
                const auto denominator=dot(direction,settings.ground->normal);
                if(std::abs(denominator)>1e-10) {
                    const auto t=dot(settings.ground->point-origin,settings.ground->normal)/denominator;
                    if(t>bias && t<maximum) {maximum=t;ground_hit=true;}
                }
            }
            const auto hit=scene.nearest_hit(origin,direction,bias,maximum,true);
            auto colour=environment_colour(direction);
            if(hit) {
                const auto& reflected=scene.triangles()[hit->triangle];
                const auto a=palette[reflected.reflection_even], b=palette[reflected.reflection_odd];
                colour={std::uint8_t((unsigned(a.r)+b.r)/2),std::uint8_t((unsigned(a.g)+b.g)/2),std::uint8_t((unsigned(a.b)+b.b)/2),255};
            } else if(ground_hit) {
                const auto point=origin+direction*maximum;
                colour=environment_colour(normalized(point));
            }
            return colour;
    };
    const auto first_x=unsigned(std::clamp(int(surfaces.minimum_x())+settings.offset_x,0,int(frame.stored_width())))/step*step;
    const auto last_x=unsigned(std::clamp(int(surfaces.maximum_x())+settings.offset_x,0,int(frame.stored_width())));
    const auto first_row=unsigned(std::clamp(int(surfaces.minimum_y())+settings.offset_y,0,int(frame.stored_height())))/step;
    const auto last_row=(unsigned(std::clamp(int(surfaces.maximum_y())+settings.offset_y,0,int(frame.stored_height())))+step-1)/step;
    const auto render_rows=[&](unsigned first,unsigned last) {
        for(unsigned row=first+first_row;row<last+first_row;++row) for(unsigned x=first_x;x<last_x;x+=step) {
            const auto y=row*step, end_x=std::min(x+step,frame.stored_width()), end_y=std::min(y+step,frame.stored_height());
            struct Cached { const SurfaceSample* sample; Vec3 normal; std::optional<Rgba8> colour; };
            std::array<Cached,8> cache{};unsigned count=0;
            // Trace a separate representative at material/normal/depth edges.
            // Coarse quality must not leave checkerboard holes in small faces.
            for(unsigned yy=y;yy<end_y;++yy) for(unsigned xx=x;xx<end_x;++xx) {
                const auto* target=owns(xx,yy);
                if(!target) continue;
                const auto normal=normalized({target->normal_x,target->normal_y,target->normal_z});
                unsigned match=0;
                for(;match<count;++match) if(target->palette_index==cache[match].sample->palette_index
                    && std::abs(target->depth-cache[match].sample->depth)<=std::max(1.F,target->depth*.03F)
                    && dot(normal,cache[match].normal)>.999) break;
                const auto reflected_colour=match<count?cache[match].colour:trace(xx,yy);
                if(match==count && count<cache.size()) cache[count++]={target,normal,reflected_colour};
                if(!reflected_colour) continue;
                const auto colour=*reflected_colour;
                const auto at=(std::size_t(yy)*frame.stored_width()+xx)*4;
                const auto base=palette[target->palette_index];
                const std::array<unsigned,3> reflected{colour.r,colour.g,colour.b}, tint{base.r,base.g,base.b};
                const auto amount=std::min(settings.intensity,100U);
                for(unsigned c=0;c<3;++c) {
                    const auto gain=settings.metallic==2?std::array<unsigned,3>{255,223,148}[c]
                        :settings.metallic==3?std::array<unsigned,3>{249,204,187}[c]:64+191*tint[c]/255;
                    const auto value=settings.metallic?reflected[c]*gain/255:reflected[c];
                    rgba[at+c]=std::uint8_t((unsigned(rgba[at+c])*(100-amount)+value*amount+50)/100);
                }
            }
        }
    };
    if(last_row<=first_row) return;
    if(workers) workers->parallel_rows(last_row-first_row,render_rows); else render_rows(0,last_row-first_row);
}
} // namespace starfox::render
