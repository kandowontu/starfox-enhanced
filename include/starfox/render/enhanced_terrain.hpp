#pragma once
#include "starfox/assets/shape.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>

namespace starfox::render {
// Presentation geometry only. It does not enter the cartridge object pool,
// alter collisions, consume game RNG, or replace authored tunnel geometry.
class EnhancedTerrain {
public:
    static constexpr int tile_size=128;
    static constexpr int patch_span=4;
    static constexpr int patch_size=tile_size*patch_span;
    // Retained distance buckets for existing terrain callers; geometry no
    // longer includes individual grass blades at any distance.
    static unsigned foliage_detail(double distance) {
        return distance < 512. ? 2U : distance < 1024. ? 1U : 0U;
    }
    struct Patch {assets::Shape shape;int x{},z{};unsigned kind{},detail{};};
    // Preserve painter order while amortizing GPU projection/clip/raster
    // dispatches. Face indices are bytes, so never exceed 256 vertices.
    struct Batch {
        assets::Shape shape;
        int x{},z{};
        unsigned patches{};
        bool append(const Patch& patch) {
            if(shape.vertices.size()+patch.shape.vertices.size()>256) return false;
            if(patches && shape.colour_words!=patch.shape.colour_words) return false;
            if(!patches) {
                x=patch.x;z=patch.z;
                shape.name="Enhanced terrain batch";
                shape.colour_words=patch.shape.colour_words;
            }
            const auto first=unsigned(shape.vertices.size());
            for(auto v:patch.shape.vertices) {
                v.x+=(patch.x-x)*patch_size;v.z+=(patch.z-z)*patch_size;
                shape.vertices.push_back(v);
            }
            for(auto face:patch.shape.faces) {
                for(auto& index:face.vertex_indices) index=std::uint8_t(first+index);
                shape.faces.push_back(std::move(face));
            }
            shape.word_coordinates.resize(shape.vertices.size(),true);
            ++patches;return true;
        }
    };
    using Key=std::tuple<int,int,unsigned,unsigned>;
    static std::uint32_t hash(int x,int z) {
        // Source positions wrap after 65536 world units. Matching that period
        // prevents an entirely different landscape at the coordinate seam.
        std::uint32_t h=std::uint32_t(x&511)*1597334677u^std::uint32_t(z&511)*3812015801u;
        h^=h>>16;h*=2246822519u;return h^(h>>13);
    }
    static double height(int cell_x,int cell_z,double u,double v,unsigned kind) {
        // Sample one continuous, periodic world field: no mound per tile.
        const double x=double(cell_x)+u,z=double(cell_z)+v;
        const auto noise=[&](double spacing,unsigned salt) {
            const double sx=x/spacing,sz=z/spacing;
            const int ix=int(std::floor(sx)),iz=int(std::floor(sz)),mask=int(512/spacing)-1;
            double fx=sx-std::floor(sx),fz=sz-std::floor(sz);
            fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
            const auto sample=[&](int a,int b){return double(hash((a&mask)+int(salt),(b&mask)-int(salt))&65535)/65535.;};
            return std::lerp(std::lerp(sample(ix,iz),sample(ix+1,iz),fx),
                std::lerp(sample(ix,iz+1),sample(ix+1,iz+1),fx),fz);
        };
        const double broad=noise(8,19),detail=noise(1,73);
        double coverage=std::clamp((noise(16,131)-.35)/.35,0.,1.);
        coverage=coverage*coverage*(3-2*coverage);
        if(kind==1) return -.5-coverage*(broad*10+noise(4,53)*noise(4,53)*16);
        if(kind==2) return -.5-noise(4,41)*1.2-detail*.6;
        if(kind==3) {
            const double ridge=1-std::abs(2*noise(4,53)-1);
            return -.5-coverage*(broad*7+ridge*ridge*22);
        }
        return -.5-coverage*(broad*8+noise(2,97)*noise(2,97)*13);
    }
    static assets::Shape make_shape(int x,int z,unsigned kind,unsigned detail) {
        assets::Shape s;s.name="Enhanced terrain";
        (void)detail;
        // Larger terrain patches reduce draw submissions while preserving the
        // continuous world field. Sample hills every 128 world units.
        const unsigned divisions=4;
        const unsigned span=patch_span,extent=patch_size;
        for(unsigned j=0;j<=divisions;++j) for(unsigned i=0;i<=divisions;++i) {
            const double u=double(i)/divisions,v=double(j)/divisions;
            s.vertices.push_back({int(i*extent/divisions),int(std::lround(height(x*int(span),z*int(span),u*span,v*span,kind))),int(j*extent/divisions)});
        }
        const auto triangle=[&](unsigned a,unsigned b,unsigned c,unsigned shade) {
            assets::Face f;f.visibility_index=-1;f.colour_id=std::uint8_t(shade);
            f.vertex_indices={std::uint8_t(a),std::uint8_t(b),std::uint8_t(c)};
            const auto& p=s.vertices[a];const auto& q=s.vertices[b];const auto& r=s.vertices[c];
            const double nx=double(q.y-p.y)*(r.z-p.z)-double(q.z-p.z)*(r.y-p.y);
            const double ny=double(q.z-p.z)*(r.x-p.x)-double(q.x-p.x)*(r.z-p.z);
            const double nz=double(q.x-p.x)*(r.y-p.y)-double(q.y-p.y)*(r.x-p.x);
            const double length=std::max(1.,std::sqrt(nx*nx+ny*ny+nz*nz));
            f.normal={int(nx*120/length),int(ny*120/length),int(nz*120/length)};
            if(shade==4) {
                const double light=std::clamp((nx+ny+nz)*-.577350269/length,0.,1.);
                f.colour_id=std::uint8_t(light>.72?3:2);
            }
            s.faces.push_back(std::move(f));
        };
        for(int j=int(divisions)-1;j>=0;--j) for(unsigned i=0;i<divisions;++i) {
            const unsigned a=unsigned(j)*(divisions+1)+i,b=a+1,c=a+divisions+1,d=c+1;
            triangle(a,b,c,4);triangle(b,d,c,4);
        }
        s.word_coordinates.assign(s.vertices.size(),true);
        s.colour_words.resize(4);
        return s;
    }
    const Patch& patch(int x,int z,unsigned kind,unsigned detail,const std::array<std::uint8_t,4>& shades) {
        detail=0; // All terrain distances share the same bounded mesh.
        auto [it,inserted]=patches_.try_emplace(Key{x,z,kind,detail});
        auto& p=it->second;
        if(inserted) {p={make_shape(x,z,kind,detail),x,z,kind,detail};}
        for(unsigned i=0;i<4;++i) p.shape.colour_words[i]=std::uint16_t((shades[i]&15)*17);
        touched_.push_back(it->first);
        return p;
    }
    void begin_frame(){touched_.clear();}
    void end_frame(){
        std::sort(touched_.begin(),touched_.end());
        std::erase_if(patches_,[&](const auto& p){return !std::binary_search(touched_.begin(),touched_.end(),p.first);});
    }
    std::size_t cached_patches() const{return patches_.size();}
private:
    std::map<Key,Patch> patches_;
    std::vector<Key> touched_;
};
}
