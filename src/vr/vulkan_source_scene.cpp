#include "starfox/vr/vulkan_source_scene.hpp"
#include "starfox/vr/vulkan_source_model.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include <stdexcept>
#include <unordered_map>
#include <algorithm>
namespace starfox::vr {
struct VulkanSourceScene::State {
    // Model resources must die before their borrowed compute pipelines.
    std::shared_ptr<std::array<VulkanSpanPipeline,5>> pipelines;
    std::shared_ptr<std::array<VulkanSpanPipeline,3>> warp_pipelines;
    std::shared_ptr<VulkanSpanPipeline> axis_pipeline;
    std::shared_ptr<SourceGraphicsPipelines> graphics;
    std::shared_ptr<VulkanDrawPackets> packets;
    struct Draw {size_t legacy_first{},legacy_count{};std::shared_ptr<VulkanSourceModel> model;Matrix4 placement{};size_t packet_index{};float units{};bool unclipped{};uint32_t corners{};bool lines{};bool mixed{};};
    std::vector<Draw> draws;
    std::unordered_map<uint32_t,size_t> ray_draws;
    VkDevice device{};PFN_vkGetDeviceProcAddr get{};VkRenderPass pass{};
    std::vector<DrawPacket> source_packets;
    std::vector<uint32_t> handles;
    std::vector<uint32_t> legacy_keys;
    std::vector<bool> warp_modes;
    std::vector<bool> axis_modes;
    bool reused{};
};
VulkanSourceScene::VulkanSourceScene()=default;
VulkanSourceScene::~VulkanSourceScene()=default;
void VulkanSourceScene::close() noexcept {state_.reset();++generation_;}
bool VulkanSourceScene::reused_resources() const noexcept {return state_ && state_->reused;}
bool VulkanSourceScene::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,const VkPhysicalDeviceLimits& limits,
    VkRenderPass pass,const SourceModelPackets& source,VulkanPipelineCache* cache) {
    ++generation_; // Even failed updates may touch retained producer resources.
    try {
        // initialize is called only after both eye submissions have completed.
        // Unchanged source inputs can reuse the already-visible compute output;
        // headset eye transforms remain independent graphics inputs.
        if(state_) for(auto& draw:state_->draws) if(draw.model) draw.model->compute_completed();
        if(!source.pending.empty() || source.handles.size()!=source.packets.size() || source.packets.size()>4096)
            throw std::runtime_error("Incomplete ordered source scene");
        std::vector<const SourceComputeModel*> requests(source.packets.size());
        for(const auto& request:source.compute_models) {
            if(request.packet_index>=source.packets.size() || requests[request.packet_index]
                || source.handles[request.packet_index]!=request.key)
                throw std::runtime_error("Invalid ordered compute placeholder");
            requests[request.packet_index]=&request;
            if(request.model.warp_expanded!=bool(request.warp))
                throw std::runtime_error("Missing or unexpected scene warp inputs");
            const auto& mesh=source.packets[request.packet_index].geometry;
            if(!mesh.vertex_view().empty() || !mesh.line_view().empty() || !mesh.texel_view().empty()
                || (mesh.shared_texels && !mesh.texels.empty()))
                throw std::runtime_error("Compute placeholder contains duplicate geometry");
        }
        bool compatible=state_ && state_->device==device && state_->get==get && state_->pass==pass
            && state_->handles==source.handles && same_draw_geometry(state_->source_packets,source.packets)
            && state_->draws.size()==source.compute_models.size()+1;
        if(compatible) for(size_t i=0;i<requests.size();++i)
            if(state_->warp_modes[i]!=bool(requests[i] && requests[i]->warp)
                || state_->axis_modes[i]!=bool(requests[i] && requests[i]->axis)) {compatible=false;break;}
        if(compatible) for(const auto& draw:state_->draws) if(draw.model) {
            const auto* request=requests[draw.packet_index];
            if(!request || request->units!=draw.units || request->model.graphics_unclipped!=draw.unclipped) {compatible=false;break;}
            if(source_span_corner_capacity(request->model)!=draw.corners) {compatible=false;break;}
            if(draw.lines!=source_span_has_lines(request->model)
                || draw.mixed!=(!request->model.graphics_unclipped && source_span_has_ordinary_faces(request->model))) {compatible=false;break;}
            std::string error;
            if(!validate_source_span_upload(request->model,draw.model->layout(),error)) {compatible=false;break;}
            if(request->axis && !validate_source_axis_upload(*request->axis,draw.model->axis_layout(),error)) {compatible=false;break;}
            if(request->warp) {
                if(!validate_source_warp_upload(*request->warp,draw.model->warp_layout(),error)) {
                    compatible=false;break;
                }
            }
        }
        if(compatible) {
            std::vector<Matrix4> transforms;
            for(const auto& packet:source.packets) {
                if(!model_eye_camera(EyeCamera{},packet.model)) throw std::runtime_error("Invalid source placement");
                if(!packet.geometry.vertex_view().empty() || !packet.geometry.line_view().empty()) transforms.push_back(packet.model);
            }
            for(auto& draw:state_->draws) if(draw.model) {
                if(!draw.model->update(requests[draw.packet_index]->model,requests[draw.packet_index]->warp.get(),requests[draw.packet_index]->axis.get())) throw std::runtime_error(draw.model->status());
                draw.placement=source.packets[draw.packet_index].model;
            }
            if(!state_->packets->update_models(transforms)) throw std::runtime_error("Retained source transforms failed");
            state_->reused=true;status_="Source scene updated with retained GPU resources";return true;
        }
        auto next=std::make_unique<State>();
        next->device=device;next->get=get;next->pass=pass;
        if(state_ && state_->device==device && state_->get==get) {
            next->pipelines=state_->pipelines;
            next->warp_pipelines=state_->warp_pipelines;
            next->axis_pipeline=state_->axis_pipeline;
        }
        next->graphics=state_ && state_->device==device && state_->get==get && state_->pass==pass
            ?state_->graphics:std::make_shared<SourceGraphicsPipelines>();
        next->source_packets=source.packets;next->handles=source.handles;
        for(const auto* request:requests) next->warp_modes.push_back(bool(request && request->warp));
        for(const auto* request:requests) next->axis_modes.push_back(bool(request && request->axis));
        next->packets=state_ && state_->device==device && state_->get==get && state_->pass==pass
            ?state_->packets:std::make_shared<VulkanDrawPackets>();
        next->packets->set_pipeline_cache(cache);
        next->graphics->cache=cache;
        std::array<const VulkanSpanPipeline*,5> pipelines{};
        std::array<const VulkanSpanPipeline*,3> warp_pipelines{};
        if(std::any_of(source.compute_models.begin(),source.compute_models.end(),[](const auto& request){return bool(request.axis);})) {
            if(!next->axis_pipeline) next->axis_pipeline=std::make_shared<VulkanSpanPipeline>();
            if(!next->axis_pipeline->descriptor_layout(0)) {
                if(!next->axis_pipeline->initialize(device,get,SourceComputeStage::axis,false,cache?cache->get():VK_NULL_HANDLE))
                    throw std::runtime_error(next->axis_pipeline->status());
                if(cache) cache->checkpoint();
            }
        }
        if(!source.compute_models.empty()) {
            if(!next->pipelines)
                next->pipelines=std::make_shared<std::array<VulkanSpanPipeline,5>>();
            // Ordinary geometry never dispatches clipping or span coverage.
            // Do not compile those expensive kernels until an effect needs them.
            const bool needs_spans=std::any_of(source.compute_models.begin(),source.compute_models.end(),
                [](const auto& model){return !model.model.graphics_unclipped;});
            bool compiled=false;
            for(size_t i=needs_spans?0:2;i<5;++i) {
                if(!(*next->pipelines)[i].descriptor_layout(0)) {
                    if(!(*next->pipelines)[i].initialize(device,get,static_cast<SourceComputeStage>(i),false,cache?cache->get():VK_NULL_HANDLE))
                        throw std::runtime_error((*next->pipelines)[i].status());
                    compiled=true;
                }
            }
            if(compiled && cache) cache->checkpoint();
            for(size_t i=0;i<5;++i) pipelines[i]=&(*next->pipelines)[i];
            const bool needs_warp=std::any_of(source.compute_models.begin(),source.compute_models.end(),
                [](const auto& model){return bool(model.warp);});
            if(needs_warp) {
                if(!next->warp_pipelines) next->warp_pipelines=std::make_shared<std::array<VulkanSpanPipeline,3>>();
                bool warp_compiled=false;
                for(size_t i=0;i<3;++i) {
                    auto& pipeline=(*next->warp_pipelines)[i];
                    if(!pipeline.descriptor_layout(0)) {
                        if(!pipeline.initialize(device,get,static_cast<SourceComputeStage>(5+i),false,cache?cache->get():VK_NULL_HANDLE))
                            throw std::runtime_error(pipeline.status());
                        warp_compiled=true;
                    }
                    warp_pipelines[i]=&pipeline;
                }
                if(warp_compiled && cache) cache->checkpoint();
            }
        }
        size_t legacy=0,first=0;
        std::unordered_map<uint32_t,const State::Draw*> previous_models;
        if(state_ && state_->device==device && state_->get==get && state_->pass==pass)
            for(const auto& draw:state_->draws) if(draw.model)
                previous_models.emplace(state_->handles[draw.packet_index],&draw);
        for(size_t index=0;index<source.packets.size();++index) {
            const auto* request=requests[index];
            if(request) {
                std::shared_ptr<VulkanSourceModel> model;
                const auto previous=previous_models.find(request->key);
                if(previous!=previous_models.end()) {
                    const auto& old=*previous->second;
                    if(old.units==request->units && old.unclipped==request->model.graphics_unclipped
                        && old.corners==source_span_corner_capacity(request->model)
                        && old.lines==source_span_has_lines(request->model)
                        && old.mixed==(!request->model.graphics_unclipped && source_span_has_ordinary_faces(request->model))
                        && old.model->update(request->model,request->warp.get(),request->axis.get())) model=old.model;
                }
                if(!model) {
                    model=std::make_shared<VulkanSourceModel>();
                    if(!model->initialize(device,get,memory,limits,request->model,pipelines,request->warp.get(),
                        request->warp?&warp_pipelines:nullptr,request->axis.get(),request->axis?next->axis_pipeline.get():nullptr)) throw std::runtime_error(model->status());
                    SceneVertex material{};material.color[3]=1;
                    if(!model->prepare_graphics(pass,request->units,material,next->graphics)) throw std::runtime_error(model->status());
                }
                next->draws.push_back({first,legacy-first,std::move(model),source.packets[index].model,index,request->units,
                    request->model.graphics_unclipped,source_span_corner_capacity(request->model),
                    source_span_has_lines(request->model),
                    !request->model.graphics_unclipped && source_span_has_ordinary_faces(request->model)});first=legacy;
            } else {
                const auto& mesh=source.packets[index].geometry;
                if(!mesh.vertex_view().empty() || !mesh.line_view().empty()) {
                    ++legacy;next->legacy_keys.push_back(source.handles[index]);
                }
            }
        }
        next->draws.push_back({first,legacy-first,{}});
        // Commit the retained legacy cache only after fallible model preparation.
        if(!next->packets->initialize(device,get,memory,pass,source.packets,source.handles))
            throw std::runtime_error(next->packets->status());
        if(legacy!=next->packets->size()) throw std::runtime_error("Ordered source item count mismatch");
        next->ray_draws.reserve(source.compute_models.size());
        for(size_t i=0;i<next->draws.size();++i) if(next->draws[i].model)
            next->ray_draws.emplace(next->handles[next->draws[i].packet_index],i);
        state_=std::move(next);status_="Ordered source scene ready";return true;
    } catch(const std::exception& e) {status_=e.what();return false;}
}
bool VulkanSourceScene::ray_source(uint32_t key,RaySource& output,bool include_warp_candidates) const {
    if(!state_) return false;
    const auto found=state_->ray_draws.find(key);
    if(found!=state_->ray_draws.end()) {
        const auto& draw=state_->draws[found->second];
        RaySource next;
        if(!draw.model->ray_source_ranges(next.ranges,include_warp_candidates)) return false;
        next.placement=draw.placement;next.units=draw.units;next.generation=generation_;
        output=next;return true;
    }
    return false;
}
bool VulkanSourceScene::legacy_ray_source(uint32_t key,LegacyRaySource& output) const {
    if(!state_) return false;
    LegacyRaySource next;
    if(!state_->packets->ray_source(key,next.geometry)) return false;
    next.generation=generation_;output=next;return true;
}
bool VulkanSourceScene::record_compute(VkCommandBuffer command) const {
    if(!state_ || !command) return false;
    if(!state_->packets->record_compute(command)) return false;
    for(const auto& draw:state_->draws) if(draw.model && !draw.model->record(command)) return false;
    return true;
}
bool VulkanSourceScene::record(VkCommandBuffer command,VkExtent2D extent,const EyeCamera& camera,bool replacement_shadows_ready) const {
    if(!state_) return false;
    for(const auto& draw:state_->draws)
        if(draw.model && !model_eye_camera(camera,draw.placement)) return false;
    for(const auto& draw:state_->draws) {
        if(replacement_shadows_ready) {
            for(size_t i=draw.legacy_first;i<draw.legacy_first+draw.legacy_count;++i)
                if(!is_source_shadow_pass(state_->legacy_keys[i]) && !state_->packets->record_range(command,extent,camera,i,1)) return false;
        } else if(!state_->packets->record_range(command,extent,camera,draw.legacy_first,draw.legacy_count)) return false;
        auto draw_camera=camera;
        if(draw.model && (state_->source_packets[draw.packet_index].preserve_native_colour
            || is_source_shadow_pass(state_->handles[draw.packet_index]))) draw_camera.effects={};
        if(draw.model && !(replacement_shadows_ready && is_source_shadow_pass(state_->handles[draw.packet_index]))
            && !draw.model->record_graphics(command,extent,*model_eye_camera(draw_camera,draw.placement))) return false;
    }
    return true;
}
}
