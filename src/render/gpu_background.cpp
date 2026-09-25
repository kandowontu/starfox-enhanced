#include "starfox/render/gpu_background.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/background_portable.hpp"
#include "shaders/generated/background2_portable.hpp"
#endif
namespace starfox::render {
struct GpuBackground::Impl {
    std::string status{"GPU background unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline *pipeline{},*bg2_pipeline{};
    SDL_GPUBuffer *memory{},*pixels{},*prepared{};SDL_GPUTransferBuffer* upload{};
    unsigned capacity{};std::uint64_t generation{};
    static void require(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
    ~Impl(){release();}
    void release() noexcept {
        if(memory) SDL_ReleaseGPUBuffer(device,memory);
        if(pixels) SDL_ReleaseGPUBuffer(device,pixels);
        if(prepared) SDL_ReleaseGPUBuffer(device,prepared);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(bg2_pipeline) SDL_ReleaseGPUComputePipeline(device,bg2_pipeline);
        memory=pixels=prepared=nullptr;upload=nullptr;pipeline=bg2_pipeline=nullptr;device=nullptr;capacity=0;
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && pipeline && memory && upload) return;
        release();device=next;
        const auto formats=SDL_GetGPUShaderFormats(device);
        const bool spv=formats&SDL_GPU_SHADERFORMAT_SPIRV,dxil=formats&SDL_GPU_SHADERFORMAT_DXIL;
        if(!spv && !dxil && !(formats&SDL_GPU_SHADERFORMAT_MSL)) throw std::runtime_error("No GPU background shader format");
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spv?background_shader::spirv:dxil?background_shader::dxil:reinterpret_cast<const Uint8*>(background_shader::metal);
        info.code_size=spv?sizeof(background_shader::spirv):dxil?sizeof(background_shader::dxil):std::strlen(background_shader::metal);
        info.entrypoint=(spv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
        SDL_GPUBufferCreateInfo mem{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,70400,0};
        memory=SDL_CreateGPUBuffer(device,&mem);require(memory);
        SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,70400,0};
        upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(upload);
    }
    void initialize_bg2() {
        if(bg2_pipeline && prepared) return;
        const auto formats=SDL_GetGPUShaderFormats(device);
        const bool spv=formats&SDL_GPU_SHADERFORMAT_SPIRV,dxil=formats&SDL_GPU_SHADERFORMAT_DXIL;
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spv?background2_shader::spirv:dxil?background2_shader::dxil:reinterpret_cast<const Uint8*>(background2_shader::metal);
        info.code_size=spv?sizeof(background2_shader::spirv):dxil?sizeof(background2_shader::dxil):std::strlen(background2_shader::metal);
        info.entrypoint=(spv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=info.num_uniform_buffers=1;info.num_readwrite_storage_buffers=2;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        if(!bg2_pipeline) {bg2_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(bg2_pipeline);}
        SDL_GPUBufferCreateInfo aux{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,64,0};
        if(!prepared) {prepared=SDL_CreateGPUBuffer(device,&aux);require(prepared);}
    }
#endif
};
GpuBackground::GpuBackground():impl_(std::make_unique<Impl>()){}
GpuBackground::~GpuBackground()=default;
const std::string& GpuBackground::status()const noexcept {return impl_->status;}
void GpuBackground::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuRasterOutput GpuBackground::enqueue(void* device,void* command,const simulation::SnesPpuState& ppu,
    unsigned width,unsigned height,unsigned scale,const GpuBackgroundSettings& s) {
    const bool custom=s.logical_viewport[0] || s.logical_viewport[1];
    const auto logical_width=custom?s.logical_viewport[0]:width/(scale?scale:1);
    const auto logical_height=custom?s.logical_viewport[1]:height/(scale?scale:1);
    if(!valid_raster_jitter(s.raster_jitter) || !device || !command || !width || !height || width>4096 || height>4096 || !scale || scale>4
        || (!custom && (width%scale || height%scale)) || !logical_width || !logical_height || logical_width>4096 || logical_height>4096 || s.layer<1 || s.layer>3
        || unsigned(s.priority)>2 || unsigned(s.tag)>4 || s.horizontal_origin < -65536 || s.horizontal_origin>65536
        || s.scroll_x < -1000000 || s.scroll_x>1000000 || s.scroll_y < -1000000 || s.scroll_y>1000000 || s.unique_regions.size()>64) {
        impl_->status="Invalid GPU background input";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        const unsigned bytes=width*height*4;
        if(impl_->capacity<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
            auto* buffer=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(buffer);
            if(impl_->pixels) SDL_ReleaseGPUBuffer(impl_->device,impl_->pixels);
            impl_->pixels=buffer;impl_->capacity=bytes;
        }
        auto* mapped=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(impl_->device,impl_->upload,true));Impl::require(mapped);
        std::memcpy(mapped,ppu.vram.data(),ppu.vram.size());
        for(unsigned i=0;i<256;++i) {const std::uint32_t c=ppu.cgram[i];std::memcpy(mapped+65536+i*4,&c,4);}
        unsigned upload_bytes=66560;
        if(s.layer==2) {
            for(unsigned i=0;i<224;++i) {
                const std::int32_t x=ppu.bg2_horizontal_offsets[i],y=ppu.bg2_scanline_scroll_y[i];
                std::memcpy(mapped+66560+i*4,&x,4);std::memcpy(mapped+67456+i*4,&y,4);
            }
            for(unsigned i=0;i<s.unique_regions.size();++i) {
                const auto& r=s.unique_regions[i];const std::array<std::int32_t,8> values{
                    r.left,r.top,r.right,r.bottom,r.first_colour,r.last_colour,r.replacement_colour,r.replacement_x_offset};
                std::memcpy(mapped+68352+i*32,values.data(),32);
            }
            upload_bytes=68352+unsigned(s.unique_regions.size())*32;
        }
        SDL_UnmapGPUTransferBuffer(impl_->device,impl_->upload);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
        SDL_GPUTransferBufferLocation from{impl_->upload,0};SDL_GPUBufferRegion to{impl_->memory,0,upload_bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        if(s.layer==2) {
            impl_->initialize_bg2();
            const unsigned flags=(s.extend_horizontal?1U:0U)|(s.wrap_horizontal?2U:0U)
                |(ppu.background_mode==2 && ppu.bg2_vertical_offsets_enabled?4U:0U)
                |(ppu.bg2_horizontal_offsets_enabled?8U:0U)|(ppu.bg2_scanline_scroll_enabled?16U:0U)
                |(ppu.tunnel_scene?32U:0U)|(ppu.background_mode==1?64U:0U)
                |(s.ending_star_extension?128U:0U)|(s.game_over_star_extension?256U:0U)
                |(std::any_of(s.unique_regions.begin(),s.unique_regions.end(),[](const auto& r) {
                    return r.replacement_x_offset>=0 && (r.replacement_x_offset&BackgroundUniqueRegion::suppress_every_copy)!=0;
                })?512U:0U);
            std::array<std::int32_t,32> data{int(width),int(height),int(scale),0,
                ppu.bg2_screen_base,ppu.bg2_character_base,(ppu.bg2_screen_size&1)?64:32,(ppu.bg2_screen_size&2)?64:32,
                s.scroll_x,s.scroll_y,s.horizontal_origin,s.extend_horizontal?0:std::max(s.horizontal_origin,0),
                s.extend_horizontal?int(logical_width):std::min(int(logical_width),std::max(s.horizontal_origin+256,0)),
                (ppu.mosaic&2)?int((ppu.mosaic>>4)+1):1,ppu.bg2_tile_size_16?16:8,int(s.priority),
                int(s.tag),(ppu.main_screen&2)?1:0,s.transparent_cgram_black?1:0,int(flags),
                std::bit_cast<std::int32_t>(s.single_occurrence_top_rows),int(s.unique_regions.size()),ppu.bg2_scroll_x,ppu.bg2_scroll_y,
                int(s.terrain_source_rows[0]),int(s.terrain_source_rows[1]),int(logical_width),int(logical_height),
                std::bit_cast<std::int32_t>(s.raster_jitter[0]),std::bit_cast<std::int32_t>(s.raster_jitter[1]),int(s.sky_source_min),0};
            for(unsigned phase=0;phase<2;++phase) {
                data[3]=int(phase);SDL_PushGPUComputeUniformData(cmd,0,data.data(),sizeof(data));
                SDL_GPUStorageBufferReadWriteBinding outputs[2]{};
                outputs[0].buffer=impl_->pixels;outputs[1].buffer=impl_->prepared;
                outputs[0].cycle=outputs[1].cycle=phase==0;
                auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,outputs,2);Impl::require(pass);
                SDL_BindGPUComputePipeline(pass,impl_->bg2_pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&impl_->memory,1);
                SDL_DispatchGPUCompute(pass,phase?(logical_width+63)/64:1,1,1);SDL_EndGPUComputePass(pass);
            }
            impl_->status="GPU BG2 resident";return {device,impl_->pixels,nullptr,width,height,++impl_->generation};
        }
        const bool bg1=s.layer==1;
        const unsigned size=bg1?ppu.bg1_screen_size:ppu.bg3_screen_size;
        const int inset=bg1?int(std::min(s.horizontal_inset,128U)):0;
        const int inset_step=bg1 && s.mosaic_staging_inset && (ppu.mosaic&1U)?(ppu.mosaic>>4U)+1:1;
        const int left_inset=((inset+inset_step-1)/inset_step)*inset_step;
        const int right_limit=std::min(256,((256-inset+inset_step-1)/inset_step)*inset_step);
        const unsigned mask=bg1?1U:4U;
        const std::array<std::int32_t,24> constants{
            int(width),int(height),int(scale),bg1?(ppu.background_mode==3?8:4):2,
            bg1?ppu.bg1_screen_base:ppu.bg3_screen_base,bg1?ppu.bg1_character_base:ppu.bg3_character_base,
            (size&1)?64:32,(size&2)?64:32,
            bg1?ppu.bg1_scroll_x:ppu.bg3_scroll_x,bg1?ppu.bg1_scroll_y:ppu.bg3_scroll_y,s.horizontal_origin,
            s.extend_horizontal?0:std::max(s.horizontal_origin+left_inset,0),
            s.extend_horizontal?int(logical_width):std::min(int(logical_width),std::max(s.horizontal_origin+right_limit,0)),
            (ppu.mosaic&mask)?int((ppu.mosaic>>4)+1):1,
            (bg1?ppu.bg1_tile_size_16:ppu.bg3_tile_size_16)?16:8,int(s.priority),int(s.tag),
            (ppu.main_screen&mask) && (!bg1 || (ppu.background_mode>=1 && ppu.background_mode<=3))?1:0,
            bg1 && s.transparent_cgram_black?1:0,bg1 && s.text_outline?1:0,int(logical_width),int(logical_height),
            std::bit_cast<std::int32_t>(s.raster_jitter[0]),std::bit_cast<std::int32_t>(s.raster_jitter[1])};
        SDL_PushGPUComputeUniformData(cmd,0,constants.data(),sizeof(constants));
        SDL_GPUStorageBufferReadWriteBinding output{};output.buffer=impl_->pixels;output.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_BindGPUComputeStorageBuffers(pass,0,&impl_->memory,1);
        SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);SDL_EndGPUComputePass(pass);
        impl_->status="GPU background resident";
        return {device,impl_->pixels,nullptr,width,height,++impl_->generation};
    } catch(const std::exception& error) {impl_->status=error.what();}
#else
    (void)ppu;
#endif
    return {};
}
}
