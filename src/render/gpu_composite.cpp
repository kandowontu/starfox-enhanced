#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/composite_portable.hpp"
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {void checked(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}}
struct GpuComposite::Impl {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* buffers[8]{};Uint32 capacities[8]{};
    SDL_GPUTransferBuffer *upload{},*download{};Uint32 uploadSize{},downloadSize{};
    SDL_GPUTexture* rgba{};SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    Uint32 width{},height{};bool valid{},has_depth{},has_motion{};
    std::vector<Uint32> packed;
    std::vector<std::pair<Uint32,Uint32>> dirtySpans;
    std::array<Uint32,256> cachedPalette{};
    Uint32 cachedUniformValue{},cachedUniformBytes{},lastCpuUploadBytes{},lastPaletteUploadBytes{};
    bool cachedUniformValid{},cachedPackedValid{},cachedPaletteValid{};
    std::string status{"GPU composition not initialized"};
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        if(rgba) SDL_ReleaseGPUTexture(device,rgba);
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
    void finish() {if(fence) {checked(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}}
    void initialize(SDL_GPUDevice* d) {
        device=d;SDL_GPUComputePipelineCreateInfo info{};
        if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=composite_shader::spirv;
            info.code_size=sizeof(composite_shader::spirv);info.entrypoint="main";
        } else if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(composite_shader::metal);
            info.code_size=sizeof(composite_shader::metal)-1;info.entrypoint="main0";
        } else if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=composite_shader::dxil;
            info.code_size=sizeof(composite_shader::dxil);info.entrypoint="main";
        } else throw std::runtime_error("GPU composition requires Vulkan, Metal or D3D12");
        info.num_readonly_storage_buffers=8;info.num_readwrite_storage_buffers=5;
        info.num_readwrite_storage_textures=1;info.num_uniform_buffers=1;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(d,&info);checked(pipeline);
        status=std::string("GPU composition: ")+SDL_GetGPUDeviceDriver(d);
    }
    void buffer(unsigned i,Uint32 bytes) {
        if(buffers[i] && capacities[i]>=bytes) return;
        if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
        buffers[i]=nullptr;
        if(i==1) cachedPaletteValid=false;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |((i>=2 && i<=3) || i>=5?SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE:0U),bytes,0};
        buffers[i]=SDL_CreateGPUBuffer(device,&info);checked(buffers[i]);capacities[i]=bytes;
    }
    void transfer(SDL_GPUTransferBuffer*& b,Uint32& capacity,Uint32 bytes,SDL_GPUTransferBufferUsage usage) {
        if(b && capacity>=bytes) return;
        if(b) SDL_ReleaseGPUTransferBuffer(device,b);
        b=nullptr;
        SDL_GPUTransferBufferCreateInfo info{usage,bytes,0};
        b=SDL_CreateGPUTransferBuffer(device,&info);checked(b);capacity=bytes;
    }
    void compose(const GpuRasterOutput& source,Uint32 sourceScale,const Framebuffer& cpu,
        std::span<const Uint8> foreground,const LayerCompositeSettings& settings,std::span<const Rgba8> palette,
        const GpuRasterOutput* late,const GpuCompositeBackground* background,std::span<const Uint8> afterLate,bool worldOnly,
        std::array<std::uint32_t,4> mapping,std::array<float,2> jitter) {
        finish();valid=false;
        if(!valid_raster_jitter(jitter)) throw std::runtime_error("Invalid composition jitter");
        const auto count=cpu.pixels().size();
        const bool custom=std::any_of(mapping.begin(),mapping.end(),[](auto value){return value!=0;});
        if(custom && std::any_of(mapping.begin(),mapping.end(),[](auto value){return !value || value>8192;}))
            throw std::runtime_error("Invalid independent compositor dimensions");
        const auto sourceReferenceWidth=custom?mapping[0]:source.width;
        const auto sourceReferenceHeight=custom?mapping[1]:source.height;
        const auto outputWidth=custom?mapping[2]:cpu.stored_width();
        const auto outputHeight=custom?mapping[3]:cpu.stored_height();
        if(!count || count>UINT32_MAX/24 || palette.empty() || palette.size()>256
            || (!foreground.empty() && foreground.size()!=count)
            || (!afterLate.empty() && afterLate.size()!=count) || !sourceScale
            || !source.width || !source.height || sourceReferenceWidth%sourceScale || sourceReferenceHeight%sourceScale)
            throw std::runtime_error("Invalid GPU composition inputs");
        if(late && (late->device!=device || !late->pixels || !late->width || !late->height
            || (!custom && (late->width!=cpu.stored_width() || late->height!=cpu.stored_height())) || late->pixels==buffers[2]
            || late->pixels==buffers[3])) throw std::runtime_error("Invalid late GPU overlay");
        if(background) {
            const auto& b=background->raster;
            if(b.device!=device || !b.pixels || !b.width || !b.height
                || (!custom && (b.width!=cpu.stored_width() || b.height!=cpu.stored_height()))
                || b.pixels==buffers[2] || b.pixels==buffers[3]
                || (!background->cpu_coverage.empty() && background->cpu_coverage.size()!=count))
                throw std::runtime_error("Invalid resident GPU background");
            if(background->margin_origin && (!background->margin_width
                || background->margin_origin>=cpu.width()
                || background->margin_width>cpu.width()-background->margin_origin))
                throw std::runtime_error("Invalid GPU background margins");
        }
        const Uint32 bytes=Uint32(count*4);
        const Uint32 outputBytes=outputWidth*outputHeight*4;
        if(width!=outputWidth || height!=outputHeight) {
            if(rgba) SDL_ReleaseGPUTexture(device,rgba);
            rgba=nullptr;width=height=0;
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width=outputWidth;info.height=outputHeight;info.layer_count_or_depth=1;info.num_levels=1;
            rgba=SDL_CreateGPUTexture(device,&info);checked(rgba);width=info.width;height=info.height;
        }
        const auto same_mask=[](std::span<const Uint8> mask,bool expected) {
            return mask.empty()? !expected : std::all_of(mask.begin(),mask.end(),
                [expected](Uint8 value){return bool(value)==expected;});
        };
        const auto backgroundCoverage=background?background->cpu_coverage:std::span<const Uint8>{};
        const auto packedAt=[&](std::size_t i) {
            return Uint32(cpu.pixels()[i])
                |(cpu.layer_tags_enabled()?Uint32(cpu.layer_tags()[i])<<8:0U)
                |(!foreground.empty() && foreground[i]?0x80000000U:0U)
                |(!backgroundCoverage.empty() && backgroundCoverage[i]?0x40000000U:0U)
                |(!afterLate.empty() && afterLate[i]?0x20000000U:0U);
        };
        const auto cached=cachedUniformValue;
        const bool allowCpuCache=!std::getenv("STARFOX_DISABLE_UNIFORM_CPU_CACHE")
            && !std::getenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE");
        const bool reuseCpu=allowCpuCache
            && cachedUniformValid && cachedUniformBytes==bytes
            && std::all_of(cpu.pixels().begin(),cpu.pixels().end(),
                [cached](Uint8 value){return value==Uint8(cached);})
            && (cpu.layer_tags_enabled()
                ?std::all_of(cpu.layer_tags().begin(),cpu.layer_tags().end(),
                    [cached](Uint8 value){return value==Uint8(cached>>8);})
                : Uint8(cached>>8)==0)
            && same_mask(foreground,(cached&0x80000000U)!=0)
            && same_mask(backgroundCoverage,(cached&0x40000000U)!=0)
            && same_mask(afterLate,(cached&0x20000000U)!=0);
        bool uniform=reuseCpu;
        Uint32 uniformValue=reuseCpu?cached:0U;
        if(!uniform && allowCpuCache && (cachedUniformValid || !cachedPackedValid)) {
            const Uint8 colour=cpu.pixels()[0];
            const Uint8 tag=cpu.layer_tags_enabled()?cpu.layer_tags()[0]:0U;
            const bool front=!foreground.empty() && foreground[0];
            const bool back=!backgroundCoverage.empty() && backgroundCoverage[0];
            const bool last=!afterLate.empty() && afterLate[0];
            uniform=std::all_of(cpu.pixels().begin(),cpu.pixels().end(),
                    [colour](Uint8 value){return value==colour;})
                && (!cpu.layer_tags_enabled() || std::all_of(cpu.layer_tags().begin(),cpu.layer_tags().end(),
                    [tag](Uint8 value){return value==tag;}))
                && same_mask(foreground,front) && same_mask(backgroundCoverage,back)
                && same_mask(afterLate,last);
            if(uniform) uniformValue=Uint32(colour)|(Uint32(tag)<<8)
                |(front?0x80000000U:0U)|(back?0x40000000U:0U)|(last?0x20000000U:0U);
        }
        // A full-height cartridge border is often two constant vertical
        // strips over an otherwise uniform CPU backing. Describe that exact
        // pattern in constants instead of packing/uploading the whole image.
        std::array<std::pair<Uint32,Uint32>,2> stripes{};
        Uint32 stripeValue=0;
        bool striped=false;
        if(!uniform && allowCpuCache && cpu.stored_width()>1) {
            const Uint32 rowWidth=cpu.stored_width();
            const auto base=packedAt(0);
            bool candidate=true;
            unsigned stripeCount=0;
            for(Uint32 x=0;x<rowWidth && candidate;) {
                const auto value=packedAt(x);
                if(value==base) {++x;continue;}
                if(stripeCount>=stripes.size() || (stripeCount && value!=stripeValue)) {
                    candidate=false;break;
                }
                stripeValue=value;
                const auto left=x;
                while(x<rowWidth && packedAt(x)==stripeValue) ++x;
                stripes[stripeCount++]={left,x};
            }
            if(candidate && stripeCount) {
                for(Uint32 y=1;y<cpu.stored_height() && candidate;++y)
                    for(Uint32 x=0;x<rowWidth;++x) {
                        const bool inside=(x>=stripes[0].first && x<stripes[0].second)
                            || (x>=stripes[1].first && x<stripes[1].second);
                        if(packedAt(std::size_t(y)*rowWidth+x)!=(inside?stripeValue:base)) {
                            candidate=false;break;
                        }
                    }
                if(candidate) {striped=true;uniformValue=base;}
            }
        }
        dirtySpans.clear();
        if(!uniform && !striped) {
            const bool comparePrevious=allowCpuCache && cachedPackedValid && capacities[0]>=bytes
                && cachedUniformBytes==bytes && packed.size()==count;
            packed.resize(count);
            uniform=true;
            for(Uint32 y=0;y<cpu.stored_height();++y) {
                const auto rowStart=std::size_t(y)*cpu.stored_width();
                Uint32 firstChanged=cpu.stored_width(),lastChanged=0;
                for(Uint32 x=0;x<cpu.stored_width();++x) {
                    const auto i=rowStart+x;
                    const Uint32 value=packedAt(i);
                    if(comparePrevious && packed[i]!=value) {
                        firstChanged=std::min(firstChanged,x);lastChanged=x;
                    }
                    packed[i]=value;
                    if(i==0) uniformValue=value;
                    else if(value!=uniformValue) uniform=false;
                }
                if(comparePrevious && firstChanged<cpu.stored_width()) {
                    const auto start=Uint32(rowStart+firstChanged),end=Uint32(rowStart+lastChanged+1);
                    if(!dirtySpans.empty() && (dirtySpans.back().second-1)/cpu.stored_width()==y-1
                        && start-dirtySpans.back().second<=64U)
                        dirtySpans.back().second=end;
                    else dirtySpans.emplace_back(start,end);
                }
            }
            if(!comparePrevious) dirtySpans.emplace_back(0,Uint32(count));
        }
        const bool gpuUniform=allowCpuCache && uniform;
        const bool gpuStriped=allowCpuCache && striped;
        if(gpuUniform || gpuStriped) dirtySpans.clear();
        // Many tiny copy commands cost more than a contiguous upload. Keep
        // sparse updates for local overlays, but cap command fan-out.
        if(dirtySpans.size()>64) {
            dirtySpans.clear();dirtySpans.emplace_back(0,Uint32(count));
        }
        Uint32 cpuUploadBytes=0;
        for(const auto& [start,end]:dirtySpans) cpuUploadBytes+=(end-start)*4;
        buffer(0,(gpuUniform || gpuStriped)?4U:bytes);buffer(1,1024);buffer(2,outputBytes);
        buffer(3,outputBytes*4);buffer(4,16);buffer(5,8);
        for(unsigned i=2;i<8;++i)
            if((source.geometry_depth==buffers[i] && source.geometry_depth)
                || (source.motion==buffers[i] && source.motion))
                throw std::runtime_error("Temporal inputs alias compositor output");
        has_depth=source.geometry_depth!=nullptr;has_motion=source.motion!=nullptr;
        buffer(6,has_depth?outputBytes:4);buffer(7,has_motion?outputBytes*4:16);
        std::array<Uint32,256> paletteWords{};
        for(unsigned i=0;i<paletteWords.size();++i) {
            const auto c=palette[std::min<std::size_t>(i,palette.size()-1)];
            paletteWords[i]=c.r|(Uint32(c.g)<<8)|(Uint32(c.b)<<16)|(Uint32(c.a)<<24);
        }
        const bool paletteChanged=!cachedPaletteValid || paletteWords!=cachedPalette;
        const Uint32 paletteOffset=dirtySpans.empty()?0U:bytes;
        if(cpuUploadBytes || paletteChanged)
            transfer(upload,uploadSize,paletteOffset+(paletteChanged?1024U:0U),SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        // The resident storage buffer survives submissions. Reuse uniform
        // backings without repacking, and upload only changed row runs for
        // nonuniform CPU overlays. The palette remains independent.
        if(std::getenv("STARFOX_TRACE_GPU_CPU_UPLOAD")) {
            const auto stripePixels=std::size_t(stripes[0].second-stripes[0].first
                +stripes[1].second-stripes[1].first)*cpu.stored_height();
            const auto tally=[&](auto predicate) {
                if(gpuUniform) return predicate(uniformValue)?count:std::size_t{0};
                if(gpuStriped) return (predicate(uniformValue)?count-stripePixels:std::size_t{0})
                    +(predicate(stripeValue)?stripePixels:std::size_t{0});
                return std::size_t(std::count_if(packed.begin(),packed.end(),predicate));
            };
            const auto meaningful=tally([](Uint32 value){return value!=0;});
            const auto ink=tally([](Uint32 value){return (value&255U)!=0;});
            const auto covered=tally([](Uint32 value){return (value&0xe0000000U)!=0;});
            const auto [least,greatest]=gpuUniform?std::pair{uniformValue,uniformValue}:gpuStriped?
                std::pair{std::min(uniformValue,stripeValue),std::max(uniformValue,stripeValue)}:
                [&]{const auto [lo,hi]=std::minmax_element(packed.begin(),packed.end());return std::pair{*lo,*hi};}();
            std::cerr<<"gpu-cpu-upload: pixels="<<count<<" meaningful="<<meaningful
                <<" ink="<<ink<<" coverage="<<covered<<" min="<<least
                <<" max="<<greatest<<" bytes="<<cpuUploadBytes<<'\n';
            std::cerr<<"gpu-palette-upload: bytes="<<(paletteChanged?1024U:0U)<<'\n';
            if(gpuStriped) std::cerr<<"gpu-cpu-stripes: "<<stripes[0].first<<'-'<<stripes[0].second
                <<' '<<stripes[1].first<<'-'<<stripes[1].second<<" value="<<stripeValue<<'\n';
            if(!gpuUniform && cpuUploadBytes && ink) {
                Uint32 minX=cpu.stored_width(),minY=cpu.stored_height(),maxX=0,maxY=0;
                for(std::size_t i=0;i<packed.size();++i) if(packed[i]&255U) {
                    const auto x=Uint32(i%cpu.stored_width()),y=Uint32(i/cpu.stored_width());
                    minX=std::min(minX,x);minY=std::min(minY,y);
                    maxX=std::max(maxX,x);maxY=std::max(maxY,y);
                }
                std::cerr<<"gpu-cpu-content-bounds: "<<minX<<','<<minY<<".."<<maxX<<','<<maxY<<'\n';
            }
        }
        if(cpuUploadBytes || paletteChanged) {
            auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));checked(mapped);
            for(const auto& [start,end]:dirtySpans)
                std::memcpy(mapped+std::size_t(start)*4,packed.data()+start,std::size_t(end-start)*4);
            if(paletteChanged)
                std::memcpy(mapped+paletteOffset,paletteWords.data(),1024);
            SDL_UnmapGPUTransferBuffer(device,upload);
        }
        command=SDL_AcquireGPUCommandBuffer(device);checked(command);
        if(cpuUploadBytes || paletteChanged) {
            auto* copy=SDL_BeginGPUCopyPass(command);checked(copy);
            for(const auto& [start,end]:dirtySpans) {
                SDL_GPUTransferBufferLocation from{upload,start*4};
                SDL_GPUBufferRegion to{buffers[0],start*4,(end-start)*4};
                SDL_UploadToGPUBuffer(copy,&from,&to,false);
            }
            if(paletteChanged) {
                SDL_GPUTransferBufferLocation paletteFrom{upload,paletteOffset};
                SDL_GPUBufferRegion paletteTo{buffers[1],0,1024};
                SDL_UploadToGPUBuffer(copy,&paletteFrom,&paletteTo,false);
            }
            SDL_EndGPUCopyPass(copy);
        }
        Sint32 constants[]{Sint32(cpu.stored_width()),Sint32(cpu.stored_height()),Sint32(source.width),Sint32(source.height),
            Sint32(cpu.draw_scale()),Sint32(sourceScale),
            (settings.mosaic & settings.mosaic_layer_mask)?Sint32((settings.mosaic>>4)+1):1,source.surfaces?1:0,
            settings.offset_x,settings.offset_y,settings.clip_left,settings.clip_top,
            settings.clip_right,settings.clip_bottom,settings.mosaic_origin_x,settings.mosaic_origin_y,
            late?1:0,background?1:0,1,background?Sint32(background->margin_origin):0,
            background?Sint32(background->margin_width):0,background && background->repair_transparent_margins?1:0,has_depth?1:0,has_motion?1:0,
            worldOnly?1:0,std::bit_cast<Sint32>(jitter[0]),std::bit_cast<Sint32>(jitter[1]),background && background->match_right_margin?1:0,Sint32(sourceReferenceWidth),Sint32(sourceReferenceHeight),Sint32(width),Sint32(height),
            late?Sint32(late->width):0,late?Sint32(late->height):0,
            background?Sint32(background->raster.width):0,background?Sint32(background->raster.height):0,
            gpuUniform?1:0,std::bit_cast<Sint32>(uniformValue),gpuStriped?1:0,
            std::bit_cast<Sint32>(stripeValue),Sint32(stripes[0].first),Sint32(stripes[0].second),
            Sint32(stripes[1].first),Sint32(stripes[1].second)};
        SDL_GPUStorageTextureReadWriteBinding texture{};texture.texture=rgba;
        SDL_GPUStorageBufferReadWriteBinding outputs[5]{};outputs[0].buffer=buffers[2];outputs[1].buffer=buffers[3];outputs[2].buffer=buffers[5];outputs[3].buffer=buffers[6];outputs[4].buffer=buffers[7];
        SDL_GPUBuffer* inputs[]{buffers[0],static_cast<SDL_GPUBuffer*>(source.pixels),
            source.surfaces?static_cast<SDL_GPUBuffer*>(source.surfaces):buffers[4],buffers[1],
            late?static_cast<SDL_GPUBuffer*>(late->pixels):buffers[4],
            background?static_cast<SDL_GPUBuffer*>(background->raster.pixels):buffers[4],
            has_depth?static_cast<SDL_GPUBuffer*>(source.geometry_depth):buffers[4],
            has_motion?static_cast<SDL_GPUBuffer*>(source.motion):buffers[4]};
        for(unsigned phase=constants[19]?0U:1U;phase<2;++phase) {
            constants[18]=Sint32(phase);SDL_PushGPUComputeUniformData(command,0,constants,sizeof(constants));
            auto* pass=SDL_BeginGPUComputePass(command,&texture,1,outputs,5);checked(pass);
            SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,inputs,8);
            SDL_DispatchGPUCompute(pass,phase?(width+7)/8:1,phase?(height+7)/8:1,1);SDL_EndGPUComputePass(pass);
        }
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;checked(fence);valid=true;
        if(paletteChanged) {cachedPalette=paletteWords;cachedPaletteValid=true;}
        cachedPackedValid=!(gpuUniform || gpuStriped);cachedUniformValid=uniform;
        cachedUniformValue=uniformValue;cachedUniformBytes=bytes;
        lastCpuUploadBytes=cpuUploadBytes;
        lastPaletteUploadBytes=paletteChanged?1024U:0U;
    }
    void read(Framebuffer& target,std::vector<Uint8>& colours,SurfaceBuffer* surfaces) {
        if(!device || !rgba || !buffers[2] || (surfaces && !buffers[3]))
            throw std::runtime_error("GPU composition readback source is missing");
        if(!valid || target.stored_width()!=width || target.stored_height()!=height
            || (surfaces && (surfaces->width()!=width || surfaces->height()!=height)))
            throw std::runtime_error("Invalid composition readback dimensions");
        finish();const Uint32 bytes=width*height*4;
        transfer(download,downloadSize,bytes*6,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        command=SDL_AcquireGPUCommandBuffer(device);checked(command);
        auto* pass=SDL_BeginGPUCopyPass(command);checked(pass);
        SDL_GPUTextureRegion tex{rgba,0,0,0,0,0,width,height,1};SDL_GPUTextureTransferInfo out{download,0,0,0};
        SDL_DownloadFromGPUTexture(pass,&tex,&out);
        for(unsigned i=0;i<(surfaces?2U:1U);++i) {
            SDL_GPUBufferRegion from{buffers[2+i],0,i?bytes*4:bytes};SDL_GPUTransferBufferLocation to{download,i?bytes*2:bytes};
            SDL_DownloadFromGPUBuffer(pass,&from,&to);
        }
        SDL_EndGPUCopyPass(pass);fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;checked(fence);finish();
        const auto* mapped=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));checked(mapped);
        colours.assign(mapped,mapped+bytes);
        const auto* values=reinterpret_cast<const Uint32*>(mapped+bytes);
        const auto* normals=reinterpret_cast<const float*>(mapped+bytes*2);
        if(surfaces) surfaces->clear();
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;target.set_stored(x,y,Uint8(values[i]),PixelLayer((values[i]>>8)&255));
            if(surfaces && (values[i]&(1U<<24))) surfaces->set(x,y,
                {normals[i*4],normals[i*4+1],normals[i*4+2],normals[i*4+3]},Uint8(values[i]>>16));
        }
        SDL_UnmapGPUTransferBuffer(device,download);
    }
};
#else
struct GpuComposite::Impl {std::string status{"GPU composition unavailable on this build"};};
#endif
GpuComposite::GpuComposite():impl_(std::make_unique<Impl>()) {}
GpuComposite::~GpuComposite()=default;
void GpuComposite::release_device() noexcept {impl_.reset();}
const std::string& GpuComposite::status() const {static const std::string empty{"GPU composition released"};return impl_?impl_->status:empty;}
GpuCompositeOutput GpuComposite::output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_ && impl_->valid) return {impl_->device,impl_->rgba,impl_->buffers[2],impl_->buffers[3],impl_->width,impl_->height,
        impl_->has_depth?impl_->buffers[6]:nullptr,impl_->has_motion?impl_->buffers[7]:nullptr};
#endif
    return {};
}
std::size_t GpuComposite::last_cpu_upload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_ && impl_->valid ? impl_->lastCpuUploadBytes : 0U;
#else
    return 0U;
#endif
}
std::size_t GpuComposite::last_palette_upload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_ && impl_->valid ? impl_->lastPaletteUploadBytes : 0U;
#else
    return 0U;
#endif
}
bool GpuComposite::compose(const GpuRasterOutput& source,std::uint32_t scale,const Framebuffer& cpu,
    std::span<const std::uint8_t> foreground,const LayerCompositeSettings& settings,std::span<const Rgba8> palette,
    const GpuRasterOutput* late,const GpuCompositeBackground* background,std::span<const std::uint8_t> afterLate,bool worldOnly,
    std::array<std::uint32_t,4> mapping,std::array<float,2> jitter) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!source.device || !source.pixels) return false;
    if(!impl_ || impl_->device!=source.device) impl_=std::make_unique<Impl>();
    try {if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source.device));impl_->compose(source,scale,cpu,foreground,settings,palette,late,background,afterLate,worldOnly,mapping,jitter);return true;}
    catch(const std::exception& e) {if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->cachedUniformValid=impl_->cachedPackedValid=impl_->cachedPaletteValid=false;
        impl_->status=e.what();return false;}
#else
    (void)source;(void)scale;(void)cpu;(void)foreground;(void)settings;(void)palette;(void)late;(void)background;(void)afterLate;(void)worldOnly;(void)mapping;return false;
#endif
}
bool GpuComposite::readback(Framebuffer& target,std::vector<std::uint8_t>& rgba,SurfaceBuffer* surfaces) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || !impl_->valid) return false;
    try {impl_->read(target,rgba,surfaces);return true;}
    catch(const std::exception& e) {if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=e.what();return false;}
#else
    (void)target;(void)rgba;(void)surfaces;return false;
#endif
}
}
