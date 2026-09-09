#include "starfox/render/shadow_geometry.hpp"
#include "starfox/render/shadow_scene.hpp"
#include "starfox/render/shadow_mask.hpp"
#include <cstdlib>
#include <iostream>

void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    using namespace starfox::render::shadows;
    const Triangle face{{-2, 0, -2}, {2, 0, -2}, {0, 0, 2}};
    require(intersect({0, -5, 0}, {0, 1, 0}, face, .01, 10) == 5.0,
        "light ray must hit receiving geometry");
    require(intersect({0, -5, 0}, {0, 1, 0}, {face.c, face.b, face.a}, .01, 10) == 5.0,
        "shadow occlusion must be winding independent");
    require(!intersect({0, 0, 0}, {0, 1, 0}, face, .01, 10), "self-shadow bias");
    require(!intersect({4, -5, 0}, {0, 1, 0}, face, .01, 10), "outside triangle");
    require(!intersect({0, -5, 0}, {0, -1, 0}, face, .01, 10), "behind light ray");
    require(!intersect({0, -5, 0}, {0, 1, 0}, face, .01, 4), "finite light distance");
    require(!intersect({0, -5, 0}, {0, 1, 0}, {{}, {}, {}}, .01, 10), "degenerate face");
    const auto shadow = project_to_plane({0, -10, 0}, {1, 1, 0}, {}, {0, 1, 0});
    require(shadow && shadow->x == 10 && shadow->y == 0, "angled light projection");
    require(!project_to_plane({0, -10, 0}, {1, 0, 0}, {}, {0, 1, 0}), "parallel light");
    require(!project_to_plane({0, 10, 0}, {1, 1, 0}, {}, {0, 1, 0}), "receiver behind caster");
    Scene scene;
    std::vector<Triangle> reference;
    for (int z=-10;z<10;++z) for (int x=-10;x<10;++x) {
        const Vec3 offset{x*8.0,0,z*8.0};
        const Triangle triangle{face.a+offset,face.b+offset,face.c+offset};
        scene.add(triangle); reference.push_back(triangle);
    }
    scene.build();
    const auto prepared_light=scene.prepare_direction({0,1,0});
    for (int x=-85;x<=85;++x) {
        const Vec3 origin{double(x),-5,0};
        bool expected=false;
        for (const auto& triangle:reference)
            expected |= intersect(origin,{0,1,0},triangle,.05,100).has_value();
        std::size_t tested=0;
        require(scene.occluded(origin,{0,1,0},.05,100,&tested)==expected,
            "accelerated shadow scene differs from all-triangle reference");
        require(scene.occluded(origin,{0,1,0},.05,100,nullptr,&prepared_light)==expected,
            "prepared light differs from uncached triangle intersections");
        require(scene.occluded(origin,{0,-1,0},.05,100,nullptr,&prepared_light)
                ==scene.occluded(origin,{0,-1,0},.05,100),
            "mismatched light direction used stale prepared terms");
        require(tested<reference.size()/4, "shadow lookup failed to prune distant geometry");
        for (double limit:{4.0,5.0,100.0}) {
            std::optional<double> closest;
            const Vec3 ray{.1,1,.25};
            for (const auto& triangle:reference) {
                const auto hit=intersect(origin,ray,triangle,.05,limit);
                if(hit && (!closest || *hit<*closest)) closest=hit;
            }
            require(scene.nearest(origin,ray,.05,limit)==closest,
                "near-first bounded traversal changed nearest receiver");
        }
    }
    scene.clear(); scene.build();
    require(!scene.occluded({0,-5,0},{0,1,0}), "scene retained previous frame casters");
    scene.add({{-10,-10,20},{10,-10,20},{0,10,40}});
    scene.add({{-10,-10,60},{10,-10,60},{0,10,60}});
    scene.build();
    require(scene.occluded({0,0,0},{0,1,0},.05,100,nullptr,&prepared_light)
        ==scene.occluded({0,0,0},{0,1,0},.05,100),
        "rebuilt scene reused stale prepared geometry");
    const auto near_surface=scene.nearest({}, {0,0,1});
    require(near_surface && std::abs(*near_surface-30)<1e-8,
        "receiver must follow slope and occlude the rear polygon");
    const auto lower_surface=scene.nearest({}, {0,-.2,1});
    require(lower_surface && *lower_surface<*near_surface,
        "sloping receiver incorrectly used one average face depth");
    scene.clear();
    scene.add({{-2,-2,20},{2,-2,20},{0,2,20}}); scene.build();
    std::vector<std::uint8_t> mask;
    const Camera camera{200,200,100,100,100};
    render_mask(scene,camera,{-1,0,-1},ReceiverPlane{{0,0,40},{0,0,1}},mask);
    require(mask[100*200+150]!=0, "angled light did not cast onto receiver");
    require(mask[100*200+100]==0, "caster incorrectly shadowed itself");
    require(mask[100*200+130]==0, "shadow leaked outside projected geometry");
    require(std::any_of(mask.begin(),mask.end(),[](auto value){return value>0 && value<160;}),
        "area light did not generate a partial-coverage penumbra");
    const auto serial_mask=mask;
    starfox::render::RowWorkers workers;
    workers.set_worker_count(4);
    render_mask(scene,camera,{-1,0,-1},ReceiverPlane{{0,0,40},{0,0,1}},mask,&workers);
    require(mask==serial_mask,"parallel shadows changed receiver pixels");
    render_mask(scene,camera,{-1,0,-1},std::nullopt,mask);
    require(mask[100*200+150]==0, "empty space received a shadow");
}
