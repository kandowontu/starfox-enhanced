#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include <cstring>
#include <bit>
#include <cmath>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/raster_portable.hpp"
#include "shaders/generated/raster_bins.hpp"
#endif
namespace starfox::render {
void replay_raster_commands(const RasterCommands& batch,Framebuffer& frame,SurfaceBuffer* surfaces,bool clear_target) {
    frame.record_to(nullptr);
    if(clear_target) {frame.clear();if(surfaces) surfaces->clear();}
    for(const auto& c:batch.commands) {
        for(int y=std::max(0,c.top);y<std::min(int(batch.height()),c.bottom);++y)
            for(int x=std::max(0,c.left);x<std::min(int(batch.width()),c.right);++x) {
                if(c.textured==0 && (c.reserved1&1U)!=0 && x!=c.u && x!=c.v) continue;
                if(c.textured==0 && (c.reserved1&4U)!=0) {
                    const auto stride=c.u_mask;
                    if(!stride || (stride&3U) || (c.texture_offset&3U)
                        || std::uint32_t(y)>=c.v_mask || std::uint32_t(x)/32>=stride/4
                        || c.texture_offset>batch.texels.size()
                        || c.v_mask>(batch.texels.size()-c.texture_offset)/stride) continue;
                    const auto at=std::size_t(c.texture_offset)+std::size_t(y)*stride+std::size_t(x/32)*4;
                    const auto bits=std::uint32_t(batch.texels[at])|(std::uint32_t(batch.texels[at+1])<<8)
                        |(std::uint32_t(batch.texels[at+2])<<16)|(std::uint32_t(batch.texels[at+3])<<24);
                    if((bits&(1U<<(x&31)))==0) continue;
                }
                const auto dither_scale=int(std::max(1U,c.reserved0));
                auto colour=c.dither && (((x/dither_scale)^(y/dither_scale))&1)?c.odd:c.even;
                auto pixel_tag=c.tag;
                if(c.textured==8) {
                    if(c.du<=0 || c.texture_offset>batch.texels.size() || batch.texels.size()-c.texture_offset<640) continue;
                    const int px=c.dv?((x-c.u)*6+2)/(7*c.du):(x-c.u)/c.du,py=(y-c.v)/c.du;
                    if(px<0 || px>=32 || py<0 || py>=40) continue;
                    const auto at=c.texture_offset+((px/8)*5+py/8)*32+(py%8)*2;
                    unsigned ink=0;
                    for(unsigned plane=0;plane<4;++plane)
                        ink|=((batch.texels[at+(plane/2)*16+plane%2]>>(7-px%8))&1U)<<plane;
                    colour=std::uint8_t(c.colour_base+ink);
                } else if(c.textured==7) {
                    if(c.du<=0 || (c.dv!=8 && c.dv!=12) || c.texture_offset>batch.texels.size()
                        || batch.texels.size()-c.texture_offset<8) continue;
                    const auto column=(x-c.u)/c.du,row=(y-c.v)/c.du;
                    if(column<0 || column>=c.dv || row<0 || row>=c.dv) continue;
                    if(!(batch.texels[c.texture_offset+row*8/c.dv]&(0x80U>>(column*8/c.dv)))) continue;
                } else if(c.textured==6) {
                    if(c.du<=0 || c.dv<2 || c.texture_offset>batch.texels.size()
                        || batch.texels.size()-c.texture_offset<24) continue;
                    const auto column=(x-c.u)/c.du,row=(y-c.v)/c.du;
                    if(column<0 || column>=16 || row<0 || row>=c.dv) continue;
                    const auto at=c.texture_offset+std::uint32_t(row*11/(c.dv-1))*2;
                    const auto bits=std::uint32_t(batch.texels[at])|(std::uint32_t(batch.texels[at+1])<<8);
                    if(!(bits&(0x8000U>>column))) continue;
                } else if(c.textured==5) {
                    if(c.du<=0 || c.dv<=0 || !c.reserved0) continue;
                    const auto size=std::uint64_t(c.u_mask)*c.v_mask;
                    if(c.texture_offset>batch.texels.size() || size>batch.texels.size()-c.texture_offset
                        || (c.dither && (c.reserved1>batch.texels.size() || size>batch.texels.size()-c.reserved1))) continue;
                    const auto snap=[&](int value,int origin) {
                        const int step=c.reserved0,delta=value-origin;
                        return (delta<0?-((-delta+step-1)/step):delta/step)*step+origin;
                    };
                    const int sx=snap(x/c.dv,int(c.even))-c.u,sy=snap(y/c.dv,int(c.odd))-c.v;
                    if(sx<0 || sy<0 || sx>=int(c.u_mask)/c.du || sy>=int(c.v_mask)/c.du) continue;
                    const auto ix=sx*c.du+((x%c.dv*2+1)*c.du)/(c.dv*2);
                    const auto iy=sy*c.du+((y%c.dv*2+1)*c.du)/(c.dv*2);
                    const auto at=std::size_t(iy)*c.u_mask+ix;
                    colour=batch.texels[c.texture_offset+at];if(!colour) continue;
                    if(c.dither) pixel_tag=batch.texels[c.reserved1+at];
                } else if(c.textured==4) {
                    if(c.du<=0 || c.dv<=0 || c.texture_offset>batch.texels.size()
                        || batch.texels.size()-c.texture_offset<65536) continue;
                    auto u=(x-c.u)/c.du,v=(y-c.v)/c.du;
                    if(u<0 || v<0 || u>=c.dv || v>=c.dv) continue;
                    if(c.reserved1&1U) u=c.dv-1-u;
                    if(c.reserved1&2U) v=c.dv-1-v;
                    const auto address=(c.reserved0+(v/8)*512+(u/8)*32+(v&7)*2)&65535U;
                    const auto bit=7-(u&7);
                    const auto byte=[&](unsigned delta){return batch.texels[c.texture_offset+((address+delta)&65535U)];};
                    const auto texel=((byte(0)>>bit)&1U)|(((byte(1)>>bit)&1U)<<1U)
                        |(((byte(16)>>bit)&1U)<<2U)|(((byte(17)>>bit)&1U)<<3U);
                    if(!texel) continue;
                    colour=c.colour_base+texel;
                } else if(c.textured) {
                    const auto dx=std::uint32_t(x-c.left);
                    std::uint32_t u,v;
                    if(c.textured==1) {
                        u=(((((std::uint32_t(c.u)+std::uint32_t(c.du)*dx)&65535U)>>8U)+c.reserved0)&c.u_mask);
                        v=(((((std::uint32_t(c.v)+std::uint32_t(c.dv)*dx)&65535U)>>8U)+c.reserved1)&c.v_mask);
                    } else if(c.textured==2) {
                        u=std::uint32_t((x-c.u)/c.dv)*(c.u_mask+1)/c.du;
                        v=std::uint32_t((y-c.v)/c.dv)*(c.v_mask+1)/c.du;
                    } else {
                        u=((std::uint32_t(c.u)+std::uint32_t(c.du)*(dx/c.dv))&65535U)>>8U;
                        v=((std::uint32_t(c.v)+std::uint32_t(c.du)*(std::uint32_t(y-c.top)/c.dv))&65535U)>>8U;
                        if(u>c.u_mask || v>c.v_mask) continue;
                    }
                    const auto texel=batch.texels[c.texture_offset+v*(c.u_mask+1)+u];
                    if(!texel) continue;
                    colour=std::uint8_t(c.colour_base+texel);
                    if(c.textured==2 && (c.reserved0&256)) colour=std::uint8_t(c.reserved0);
                }
                frame.set_stored(x,y,std::uint8_t(colour),PixelLayer(pixel_tag));
                if(surfaces && c.has_surface) surfaces->set(x,y,
                    {c.surface[0],c.surface[1],c.surface[2],c.surface[3]},std::uint8_t(colour));
            }
    }
}
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace { void require_raster(bool value) {if(!value) throw std::runtime_error(SDL_GetError());} }
struct GpuRaster::Impl {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{},*bins_pipeline{};
    SDL_GPUBuffer* buffers[7]{};Uint32 sizes[7]{};
    SDL_GPUTransferBuffer *upload{},*download{};Uint32 upload_size{},download_size{};
    SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    std::vector<SDL_GPUFence*> retired_fences;
    bool owns_device{},resident_valid{},resident_surfaces{};
    Uint32 width{},height{};std::uint64_t generation{};
    bool failed{};std::string status{"GPU raster not initialized"};
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        if(!retired_fences.empty()) SDL_WaitForGPUFences(device,true,retired_fences.data(),Uint32(retired_fences.size()));
        for(auto* retired:retired_fences) SDL_ReleaseGPUFence(device,retired);
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(bins_pipeline) SDL_ReleaseGPUComputePipeline(device,bins_pipeline);
        if(owns_device) SDL_DestroyGPUDevice(device);
    }
    void finish() {
        if(fence) {require_raster(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
        if(!retired_fences.empty()) require_raster(SDL_WaitForGPUFences(device,true,retired_fences.data(),Uint32(retired_fences.size())));
        for(auto* retired:retired_fences) SDL_ReleaseGPUFence(device,retired);
        retired_fences.clear();
    }
    void retire_submission() {
        if(fence) {retired_fences.push_back(fence);fence=nullptr;}
        // At most two previous submissions plus the frame being recorded.
        // Retire completed work immediately; bound memory/latency if GPU-bound.
        while(!retired_fences.empty() && (retired_fences.size()>2 || SDL_QueryGPUFence(device,retired_fences.front()))) {
            auto* oldest=retired_fences.front();
            require_raster(SDL_WaitForGPUFences(device,true,&oldest,1));
            SDL_ReleaseGPUFence(device,oldest);retired_fences.erase(retired_fences.begin());
        }
    }
    void initialize(SDL_GPUDevice* borrowed=nullptr,bool gpu_binning=false) {
        if(borrowed) device=borrowed;
        else {
#if defined(_WIN32)
        // Keep the established default until every model producer supports DXIL.
        SDL_SetHintWithPriority(SDL_HINT_GPU_DRIVER,"vulkan",SDL_HINT_DEFAULT);
#endif
        const auto props=SDL_CreateProperties();require_raster(props!=0);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN,
            SDL_getenv("STARFOX_TEST_SOFTWARE_GPU")==nullptr);
        device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require_raster(device);
        owns_device=true;
        }
        SDL_GPUComputePipelineCreateInfo info{};
        if(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=raster_shader::spirv;
            info.code_size=sizeof(raster_shader::spirv);info.entrypoint="main";
        } else if(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(raster_shader::metal);
            info.code_size=sizeof(raster_shader::metal)-1;info.entrypoint="main0";
        } else if(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=raster_shader::dxil;
            info.code_size=sizeof(raster_shader::dxil);info.entrypoint="main";
        } else throw std::runtime_error("Native raster requires Vulkan, Metal or D3D12");
        info.num_readonly_storage_buffers=8;info.num_readwrite_storage_buffers=3;
        info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require_raster(pipeline);
        if(gpu_binning || SDL_getenv("STARFOX_TEST_GPU_BINS")) initialize_bins();
        status=std::string("GPU native scanlines: ")+SDL_GetGPUDeviceDriver(device);
    }
    void initialize_bins() {
        if(bins_pipeline) return;
        SDL_GPUComputePipelineCreateInfo info{};
        if(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.entrypoint="main";
            info.code=raster_bins_shader::spirv;info.code_size=sizeof(raster_bins_shader::spirv);
        } else if(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.entrypoint="main0";
            info.code=reinterpret_cast<const Uint8*>(raster_bins_shader::metal);info.code_size=sizeof(raster_bins_shader::metal)-1;
        } else {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.entrypoint="main";
            info.code=raster_bins_shader::dxil;info.code_size=sizeof(raster_bins_shader::dxil);
        }
        info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=2;
        info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        bins_pipeline=SDL_CreateGPUComputePipeline(device,&info);require_raster(bins_pipeline);
    }
    void buffer(unsigned i,Uint32 bytes) {
        bytes=std::max(16U,(bytes+3)&~3U);
        if(buffers[i] && sizes[i]>=bytes) return;
        if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
        buffers[i]=nullptr;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |((i==0 || i==3)?0U:SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE),bytes,0};
        buffers[i]=SDL_CreateGPUBuffer(device,&info);require_raster(buffers[i]);sizes[i]=bytes;
    }
    void transfer(SDL_GPUTransferBuffer*& target,Uint32& capacity,Uint32 bytes,SDL_GPUTransferBufferUsage usage) {
        if(target && capacity>=bytes) return;
        if(target) SDL_ReleaseGPUTransferBuffer(device,target);
        target=nullptr;
        SDL_GPUTransferBufferCreateInfo info{usage,bytes,0};
        target=SDL_CreateGPUTransferBuffer(device,&info);require_raster(target);capacity=bytes;
    }
    void render(RasterCommands& batch,Framebuffer* frame,SurfaceBuffer* surfaces,bool keep_surfaces,bool gpu_binning=false,SDL_GPUCommandBuffer* borrowed=nullptr,bool coverage=false,std::array<std::uint32_t,2> raster_size={},std::array<float,2> jitter={}) {
        if(!valid_raster_jitter(jitter)) throw std::runtime_error("Invalid raster jitter");
        if(borrowed && (fence || command || !retired_fences.empty())) throw std::runtime_error("Finish submitted raster work before borrowing a command buffer");
        if(!borrowed) {if(frame || SDL_getenv("STARFOX_TEST_SERIAL_RASTER")) finish();else retire_submission();}
        resident_valid=false;
        const bool custom=raster_size[0] || raster_size[1];
        if(custom && (!borrowed || frame || surfaces || !raster_size[0] || !raster_size[1]
            || raster_size[0]>8192 || raster_size[1]>8192)) throw std::runtime_error("Invalid raster output size");
        const auto output_width=custom?raster_size[0]:batch.width(),output_height=custom?raster_size[1]:batch.height();
        const std::size_t count=std::size_t(output_width)*output_height;
        if(!batch.width() || !batch.height() || batch.width()>32767 || batch.height()>32767
            || !count || count>UINT32_MAX/20 || (frame && (frame->stored_width()!=batch.width() || frame->stored_height()!=batch.height()))
            || (surfaces && (surfaces->width()!=batch.width() || surfaces->height()!=batch.height())))
            throw std::runtime_error("Invalid native raster dimensions");
        const bool gpu_bins=gpu_binning || SDL_getenv("STARFOX_TEST_GPU_BINS")!=nullptr;
        if(gpu_bins) initialize_bins();
        std::uint64_t references=0;
        const auto tiles=std::uint64_t((batch.width()+63)/64)*batch.height();
        if(gpu_bins) {
            for(const auto& c:batch.commands) {
                const auto left=std::max(0,c.left),right=std::min(int(batch.width()),c.right);
                const auto top=std::max(0,c.top),bottom=std::min(int(batch.height()),c.bottom);
                if(left<right && top<bottom) references+=std::uint64_t(bottom-top)*((right+63)/64-left/64);
            }
            if((tiles+references)>UINT32_MAX/4 || batch.commands.size()>(UINT32_MAX-63)/8)
                throw std::runtime_error("GPU raster bin capacity exceeded");
        } else batch.bin_rows();
        const void* data[]{batch.commands.data(),batch.rows.data(),batch.indices.data(),batch.texels.data()};
        const std::size_t bytes[]{batch.commands.size()*sizeof(RasterCommand),gpu_bins?0:batch.rows.size()*4,
            gpu_bins?0:batch.indices.size()*4,batch.texels.size()};
        Uint32 offsets[4]{},lengths[4]{};std::uint64_t total=0;
        for(unsigned i=0;i<4;++i) {
            // GPU bin passes initialize these buffers completely. They need
            // capacity, not placeholder CPU data or copy-pass submissions.
            if(gpu_bins && (i==1 || i==2)) continue;
            if(bytes[i]>UINT32_MAX-16) throw std::runtime_error("Native raster commands too large");
            lengths[i]=std::max(16U,(Uint32(bytes[i])+3)&~3U);offsets[i]=Uint32(total);total+=lengths[i];
            if(total>UINT32_MAX) throw std::runtime_error("Native raster upload too large");
            buffer(i,lengths[i]);
        }
        const Uint32 pixel_bytes=Uint32(count*4),surface_bytes=keep_surfaces?Uint32(count*16):0;
        if(gpu_bins) {buffer(1,Uint32((tiles+1+(tiles+63)/64)*4));buffer(2,Uint32((tiles+references)*4));}
        buffer(4,pixel_bytes);buffer(5,surface_bytes);
        transfer(upload,upload_size,Uint32(total),SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        if(frame) transfer(download,download_size,pixel_bytes+surface_bytes,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require_raster(mapped);
        for(unsigned i=0;i<4;++i) {
            if(lengths[i]==0) continue;
            if(bytes[i]) std::memcpy(mapped+offsets[i],data[i],bytes[i]);
            std::memset(mapped+offsets[i]+bytes[i],0,lengths[i]-bytes[i]);
        }
        SDL_UnmapGPUTransferBuffer(device,upload);
        command=borrowed?borrowed:SDL_AcquireGPUCommandBuffer(device);require_raster(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require_raster(copy);
        for(unsigned i=0;i<4;++i) {
            if(lengths[i]==0) continue;
            SDL_GPUTransferBufferLocation source{upload,offsets[i]};SDL_GPUBufferRegion destination{buffers[i],0,lengths[i]};
            SDL_UploadToGPUBuffer(copy,&source,&destination,true);
        }
        SDL_EndGPUCopyPass(copy);
        if(gpu_bins) {
            SDL_GPUStorageBufferReadWriteBinding bins[2]{};bins[0].buffer=buffers[1];bins[1].buffer=buffers[2];
            for(Uint32 stage=0;stage<6;++stage) {
                bins[0].cycle=bins[1].cycle=stage==0;
                const Uint32 config[]{batch.width(),batch.height(),Uint32(batch.commands.size()),stage};
                SDL_PushGPUComputeUniformData(command,0,config,sizeof(config));
                auto* bin_pass=SDL_BeginGPUComputePass(command,nullptr,0,bins,2);require_raster(bin_pass);
                SDL_BindGPUComputePipeline(bin_pass,bins_pipeline);SDL_BindGPUComputeStorageBuffers(bin_pass,0,buffers,1);
                const auto work=stage==0?Uint32(tiles+1):stage==3?1U:
                    (stage==2 || stage==4)?Uint32(tiles):Uint32(batch.commands.size()*8);
                SDL_DispatchGPUCompute(bin_pass,std::max(1U,(work+63)/64),1,1);SDL_EndGPUComputePass(bin_pass);
            }
        }
        const Uint32 settings[]{batch.width(),batch.height(),keep_surfaces?1U:0U,(gpu_bins?1U:0U)|(coverage?0x40000000U:0U),0,0,keep_surfaces?1U:0U,0,Uint32(batch.texels.size()),custom?output_width:0,custom?output_height:0,0,0,0,0,0,0,0,0,0,std::bit_cast<Uint32>(jitter[0]),std::bit_cast<Uint32>(jitter[1]),0,0};
        SDL_PushGPUComputeUniformData(command,0,settings,sizeof(settings));
        buffer(6,16);
        SDL_GPUStorageBufferReadWriteBinding outputs[3]{};outputs[0].buffer=buffers[4];outputs[1].buffer=buffers[5];outputs[2].buffer=buffers[6];
        outputs[0].cycle=outputs[1].cycle=outputs[2].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,outputs,3);require_raster(pass);
        SDL_GPUBuffer* inputs[]{buffers[0],buffers[1],buffers[2],buffers[3],buffers[0],buffers[0],buffers[0],buffers[0]};
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,inputs,8);
        SDL_DispatchGPUCompute(pass,(output_width+63)/64,output_height,1);SDL_EndGPUComputePass(pass);
        width=output_width;height=output_height;resident_surfaces=keep_surfaces;
        if(borrowed) {command=nullptr;++generation;return;}
        if(frame) {
        if(!buffers[4] || (surfaces && !buffers[5]))
            throw std::runtime_error("Native raster readback source is missing");
        copy=SDL_BeginGPUCopyPass(command);require_raster(copy);
        for(unsigned i=0;i<(surfaces?2U:1U);++i) {
            SDL_GPUBufferRegion source{buffers[4+i],0,i?surface_bytes:pixel_bytes};
            SDL_GPUTransferBufferLocation destination{download,i?pixel_bytes:0};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);
        }
        SDL_EndGPUCopyPass(copy);
        }
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require_raster(fence);
        resident_valid=true;++generation;
        if(frame) {finish();unpack(*frame,surfaces);}
    }
    void unpack(Framebuffer& frame,SurfaceBuffer* surfaces) {
        const auto pixel_bytes=width*height*4;
        const auto* packed=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require_raster(packed);
        const auto* normals=reinterpret_cast<const float*>(reinterpret_cast<const Uint8*>(packed)+pixel_bytes);
        if(surfaces) surfaces->clear();
        auto& pixels=frame.pixels();auto& tags=frame.layer_tags();
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;
            pixels[i]=std::uint8_t(packed[i]);
            if(frame.layer_tags_enabled()) tags[i]=std::uint8_t(packed[i]>>8);
            if(surfaces && (packed[i]&(1U<<24))) surfaces->set(x,y,
                {normals[i*4],normals[i*4+1],normals[i*4+2],normals[i*4+3]},std::uint8_t(packed[i]>>16));
        }
        SDL_UnmapGPUTransferBuffer(device,download);
    }
    void readback(Framebuffer& frame,SurfaceBuffer* surfaces) {
        if(!resident_valid || !device || !buffers[4] || (surfaces && !buffers[5])
            || frame.stored_width()!=width || frame.stored_height()!=height
            || (surfaces && (!resident_surfaces || surfaces->width()!=width || surfaces->height()!=height)))
            throw std::runtime_error("Incompatible native raster readback");
        finish();
        const Uint32 pixel_bytes=width*height*4,surface_bytes=surfaces?width*height*16:0;
        transfer(download,download_size,pixel_bytes+surface_bytes,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        command=SDL_AcquireGPUCommandBuffer(device);require_raster(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require_raster(copy);
        for(unsigned i=0;i<(surfaces?2U:1U);++i) {
            SDL_GPUBufferRegion source{buffers[4+i],0,i?surface_bytes:pixel_bytes};
            SDL_GPUTransferBufferLocation destination{download,i?pixel_bytes:0};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);
        }
        SDL_EndGPUCopyPass(copy);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require_raster(fence);
        finish();unpack(frame,surfaces);
    }
};
#else
struct GpuRaster::Impl {std::string status{"GPU native raster unavailable on this build"};};
#endif
GpuRaster::GpuRaster():impl_(std::make_unique<Impl>()) {}
GpuRaster::~GpuRaster()=default;
void GpuRaster::release_device() noexcept {impl_.reset();}
const std::string& GpuRaster::status() const {
    static const std::string released{"GPU raster device released"};return impl_?impl_->status:released;
}
bool GpuRaster::render(RasterCommands& batch,Framebuffer& frame,SurfaceBuffer* surfaces) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {
        if(!impl_->device) impl_->initialize();
        impl_->render(batch,&frame,surfaces,surfaces!=nullptr);return true;
    } catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=error.what();impl_->failed=true;return false;
    }
#else
    (void)batch;(void)frame;(void)surfaces;return false;
#endif
}
bool GpuRaster::render_resident(void* device,RasterCommands& batch,bool surfaces,bool gpu_binning) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device) return false;
    if(!impl_ || impl_->device!=device) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device),gpu_binning);
        impl_->render(batch,nullptr,nullptr,surfaces,gpu_binning);return true;
    } catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=error.what();impl_->failed=true;impl_->resident_valid=false;return false;
    }
#else
    (void)device;(void)batch;(void)surfaces;(void)gpu_binning;return false;
#endif
}
GpuRasterOutput GpuRaster::enqueue_commands(void* device,void* command,RasterCommands& batch,bool surfaces,bool gpu_binning,std::array<std::uint32_t,2> raster_size,std::array<float,2> jitter) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device || !command) return {};
    if(!impl_ || impl_->device!=device) impl_=std::make_unique<Impl>();
    try {
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device),gpu_binning);
        impl_->render(batch,nullptr,nullptr,surfaces,gpu_binning,static_cast<SDL_GPUCommandBuffer*>(command),true,raster_size,jitter);
        impl_->status="Raster commands enqueued on caller command buffer";
        return {device,impl_->buffers[4],surfaces?impl_->buffers[5]:nullptr,impl_->width,impl_->height,impl_->generation};
    }catch(const std::exception& error){
        // This class must never submit or cancel the caller's command buffer.
        if(impl_->command==command) impl_->command=nullptr;
        impl_->status=error.what();return {};
    }
#endif
    return {};
}
GpuRasterOutput GpuRaster::resident_output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_ && impl_->resident_valid) return {impl_->device,impl_->buffers[4],
        impl_->resident_surfaces?impl_->buffers[5]:nullptr,impl_->width,impl_->height,impl_->generation};
#endif
    return {};
}
GpuRasterOutput GpuRaster::enqueue_row_spans(void* device,void* command,void* spans,
    std::uint32_t polygon_count,std::uint32_t width,std::uint32_t height,bool surfaces,void* texels,bool pixel_coverage,
    const GpuRasterOutput* background,bool wave_rows,std::int16_t wave_offset,std::uint32_t wave_frame,std::uint32_t texel_bytes,
    const GpuGeometryDepthInput* geometry_depth,std::array<std::uint32_t,2> raster_size,std::array<float,2> jitter) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device || !command || (!spans && polygon_count!=0)) return {};
    if(!impl_ || impl_->device!=device) impl_=std::make_unique<Impl>();
    try {
        const bool custom=raster_size[0] || raster_size[1];
        if(!valid_raster_jitter(jitter) || (jitter!=std::array<float,2>{} && (background || (geometry_depth && !geometry_depth->screen_aligned))))
            throw std::runtime_error("Invalid row-span jitter");
        if(custom && (!raster_size[0] || !raster_size[1] || raster_size[0]>8192 || raster_size[1]>8192
            || background || (geometry_depth && !geometry_depth->screen_aligned) || width>32767 || height>32767))
            throw std::runtime_error("Invalid independent row-span output");
        const auto outputWidth=custom?raster_size[0]:width,outputHeight=custom?raster_size[1]:height;
        const auto pixels=std::uint64_t(outputWidth)*outputHeight;
        if(!width || !height || polygon_count>=0x40000000U
            || pixels>UINT32_MAX/16 || std::uint64_t(polygon_count)*height>UINT32_MAX/96)
            throw std::runtime_error("Invalid GPU row-span dimensions");
        if(impl_->fence || impl_->command || !impl_->retired_fences.empty())
            throw std::runtime_error("Finish submitted raster work before enqueueing row spans");
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        const auto aliases_output=[&](void* input) {
            return input && (input==impl_->buffers[4] || input==impl_->buffers[5] || input==impl_->buffers[6]);
        };
        if(background && (!background->pixels || background->device!=device
            || background->width!=width || background->height!=height
            || aliases_output(background->pixels) || aliases_output(background->surfaces)
            || aliases_output(background->geometry_depth)))
            throw std::runtime_error("Invalid or aliased GPU raster background");
        if(background && background->motion)
            throw std::runtime_error("Motion-bearing backgrounds require GpuScene composition");
        if(aliases_output(spans) || aliases_output(texels))
            throw std::runtime_error("GPU row spans alias raster output");
        if(geometry_depth && (!geometry_depth->planes || !geometry_depth->count
            || aliases_output(geometry_depth->planes)
            || !std::isfinite(geometry_depth->focal_x) || geometry_depth->focal_x<=0
            || !std::isfinite(geometry_depth->focal_y) || geometry_depth->focal_y<=0
            || !std::isfinite(geometry_depth->center_x) || !std::isfinite(geometry_depth->center_y)))
            throw std::runtime_error("Invalid GPU geometry depth input");
        impl_->resident_valid=false;
        const bool output_surfaces=surfaces || (background && background->surfaces);
        impl_->buffer(4,Uint32(pixels*4));impl_->buffer(5,output_surfaces?Uint32(pixels*16):0);
        const bool output_depth=geometry_depth || (background && background->geometry_depth);
        impl_->buffer(6,output_depth?Uint32(pixels*4):16);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto tiles=std::uint64_t((width+63)/64)*height;
        const auto list_bytes=tiles*(std::uint64_t(polygon_count)+1)*4;
        const bool tiled=spans && polygon_count && !wave_rows && list_bytes<=64U*1024*1024
            && !SDL_getenv("STARFOX_TEST_DISABLE_TILED_SPANS");
        if(tiled) {
            impl_->initialize_bins();impl_->buffer(1,4);impl_->buffer(2,Uint32(list_bytes));
            const Uint32 bin_settings[]{width,height,polygon_count,6};
            SDL_PushGPUComputeUniformData(cmd,0,bin_settings,sizeof(bin_settings));
            SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
            bindings[0].buffer=impl_->buffers[1];bindings[1].buffer=impl_->buffers[2];
            // Scratch bins are consumed by the immediately following pass.
            // Queue ordering permits reuse; cycling here retains one large
            // bin allocation for every terrain patch in a submitted frame.
            bindings[0].cycle=bindings[1].cycle=false;
            auto* bin_pass=SDL_BeginGPUComputePass(cmd,nullptr,0,bindings,2);require_raster(bin_pass);
            SDL_BindGPUComputePipeline(bin_pass,impl_->bins_pipeline);
            auto* input=static_cast<SDL_GPUBuffer*>(spans);SDL_BindGPUComputeStorageBuffers(bin_pass,0,&input,1);
            SDL_DispatchGPUCompute(bin_pass,(Uint32(tiles)+63)/64,1,1);SDL_EndGPUComputePass(bin_pass);
        }
        const Uint32 settings[]{width,height,output_surfaces?1U:0U,0x80000000U|(pixel_coverage?0x40000000U:0U)|polygon_count,
            background?1U:0U,background && background->surfaces?1U:0U,surfaces?1U:0U,
            wave_rows?(1U|(std::uint32_t(std::uint16_t(wave_offset))<<1U)|((wave_frame&15U)<<17U)):(tiled?0x80000000U:0U),
            texels?texel_bytes:0U,custom?outputWidth:0,custom?outputHeight:0,0,
            output_depth?1U:0U,geometry_depth?geometry_depth->count:0U,background && background->geometry_depth?1U:0U,0,
            std::bit_cast<Uint32>(geometry_depth?geometry_depth->focal_x:1.f),
            std::bit_cast<Uint32>(geometry_depth?geometry_depth->focal_y:1.f),
            std::bit_cast<Uint32>(geometry_depth?geometry_depth->center_x:0.f),
            std::bit_cast<Uint32>(geometry_depth?geometry_depth->center_y:0.f),
            std::bit_cast<Uint32>(jitter[0]),std::bit_cast<Uint32>(jitter[1]),0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding outputs[3]{};
        outputs[0].buffer=impl_->buffers[4];outputs[1].buffer=impl_->buffers[5];
        outputs[2].buffer=impl_->buffers[6];
        // Fused draws alternate two non-aliasing renderers. The previous
        // contents have already been consumed before this ordered write.
        outputs[0].cycle=outputs[1].cycle=outputs[2].cycle=background==nullptr;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,outputs,3);require_raster(pass);
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        // Bins are unused for row spans. For solid-only input, texels also
        // bind an existing read-only buffer rather than an empty placeholder.
        if(!spans) impl_->buffer(0,4);
        auto* source=spans?static_cast<SDL_GPUBuffer*>(spans):impl_->buffers[0];
        SDL_GPUBuffer* inputs[]{source,source,tiled?impl_->buffers[2]:source,texels?static_cast<SDL_GPUBuffer*>(texels):source,
            background?static_cast<SDL_GPUBuffer*>(background->pixels):source,
            background && background->surfaces?static_cast<SDL_GPUBuffer*>(background->surfaces):source,
            geometry_depth?static_cast<SDL_GPUBuffer*>(geometry_depth->planes):source,
            background && background->geometry_depth?static_cast<SDL_GPUBuffer*>(background->geometry_depth):source};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,8);
        SDL_DispatchGPUCompute(pass,(outputWidth+63)/64,outputHeight,1);SDL_EndGPUComputePass(pass);
        impl_->status="GPU-generated row spans rasterized resident";
        return {device,impl_->buffers[4],output_surfaces?impl_->buffers[5]:nullptr,outputWidth,outputHeight,++impl_->generation,
            output_depth?impl_->buffers[6]:nullptr};
    }catch(const std::exception& error){impl_->status=error.what();return {};}
#endif
    return {};
}
bool GpuRaster::readback(Framebuffer& frame,SurfaceBuffer* surfaces) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || impl_->failed) return false;
    try {impl_->readback(frame,surfaces);return true;}
    catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=error.what();return false;
    }
#else
    (void)frame;(void)surfaces;return false;
#endif
}
bool GpuRaster::wait_for_completion() {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || impl_->failed || !impl_->resident_valid) return false;
    try {impl_->finish();return true;}
    catch(const std::exception& error) {
        impl_->status=error.what();impl_->resident_valid=false;return false;
    }
#else
    return false;
#endif
}
}
