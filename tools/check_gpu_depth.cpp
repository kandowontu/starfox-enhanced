#include "starfox/render/gpu_model.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/gpu_temporal_inputs.hpp"
#include <limits>
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool okay,const char* message) {
    if(!okay) throw std::runtime_error(std::string(message)+": "+SDL_GetError());
}
struct Capture {
    std::vector<Uint32> pixels;
    std::vector<std::array<float,4>> surfaces;
    std::vector<float> depths;
    std::vector<std::array<float,4>> motion;
};
Capture download(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,
    const starfox::render::GpuRasterOutput& output,bool depth) {
    if(!depth) require(!output.geometry_depth,"disabled depth retained previous output");
    require(output.pixels && output.surfaces && (!depth || output.geometry_depth),"missing capture buffer");
    const Uint32 count=output.width*output.height;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,count*40,0};
    auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer,"transfer");
    auto* copy=SDL_BeginGPUCopyPass(command);require(copy,"copy");
    const std::array<void*,3> inputs{output.pixels,output.surfaces,output.geometry_depth};
    const std::array<Uint32,3> sizes{count*4,count*16,count*4};
    Uint32 offset=0;
    for(unsigned i=0;i<(depth?3U:2U);++i) {
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(inputs[i]),0,sizes[i]};
        SDL_GPUTransferBufferLocation to{transfer,offset};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=sizes[i];
    }
    if(output.motion) {
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.motion),0,count*16};
        SDL_GPUTransferBufferLocation to{transfer,count*24};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);
    }
    SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"submit");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"wait");SDL_ReleaseGPUFence(device,fence);
    const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,transfer,false));require(bytes,"map");
    Capture result;result.pixels.resize(count);result.surfaces.resize(count);
    std::memcpy(result.pixels.data(),bytes,count*4);std::memcpy(result.surfaces.data(),bytes+count*4,count*16);
    if(depth) {result.depths.resize(count);std::memcpy(result.depths.data(),bytes+count*20,count*4);}
    if(output.motion) {result.motion.resize(count);std::memcpy(result.motion.data(),bytes+count*24,count*16);}
    SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);
    return result;
}
Capture capture(SDL_GPUDevice* device,starfox::render::GpuModel& model,
    const starfox::assets::Shape& shape,const starfox::render::RenderPose& pose,
    unsigned scale,bool depth) {
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"acquire");
    starfox::render::RenderSettings settings;settings.render_scale=scale;
    const auto output=model.enqueue(device,command,shape,pose,settings,224,192,true,nullptr,nullptr,depth);
    if(!output.pixels || (depth && !output.geometry_depth)) {
        SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(model.status());
    }
    return download(device,command,output,depth);
}
}
void check_temporal_textures(SDL_GPUDevice* device) {
    constexpr Uint32 width=64,height=2,count=width*height;
    std::array<float,count> depths{};std::array<std::array<float,4>,count> motions{};
    for(unsigned i=0;i<count;++i) {
        const float cases[]{0.1f,0.5f,1.f,100.f,101.f,0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()};
        depths[i]=cases[i%9];motions[i]={float(i)*0.25f,-float(i)*0.5f,depths[i],1.f};
        if(i%13==0) motions[i][3]=0;
        if(i%17==0) motions[i][2]=9;
        if(i%19==0) motions[i][0]=std::numeric_limits<float>::infinity();
    }
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,count*4,0};
    auto* depth=SDL_CreateGPUBuffer(device,&bi);bi.size=count*16;auto* motion=SDL_CreateGPUBuffer(device,&bi);
    bi.size=count*4;auto* terrain=SDL_CreateGPUBuffer(device,&bi);require(terrain,"terrain coverage buffer");
    require(depth && motion,"temporal input buffers");
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,count*24,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    auto* readback=SDL_CreateGPUTransferBuffer(device,&ti);require(upload && readback,"temporal transfers");
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(mapped,"temporal upload map");
    std::memcpy(mapped,depths.data(),count*4);std::memcpy(mapped+count*4,motions.data(),count*16);
    for(unsigned i=0;i<count;++i) {Uint32 coverage=i%3;std::memcpy(mapped+count*20+i*4,&coverage,4);}
    SDL_UnmapGPUTransferBuffer(device,upload);
    starfox::render::GpuTemporalInputs converter;
    for(unsigned ground_case=0;ground_case<5;++ground_case) for(bool reset:{false,true}) for(bool jittered:{false,true}) for(bool packed_coverage:{false,true}) {
        auto* coverage_upload=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(coverage_upload,"coverage upload map");
        for(unsigned i=0;i<count;++i) {
            const Uint32 value=packed_coverage?(0x04000223u|(i%3==1?0x08000000u:0)):i%3;
            std::memcpy(coverage_upload+count*20+i*4,&value,4);
        }
        SDL_UnmapGPUTransferBuffer(device,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"temporal command");
        require(!converter.enqueue(device,command,depth,motion,width,height,1,0,reset).depth,"bad camera accepted");
        require(!converter.enqueue(device,command,depth,depth,width,height,.1f,100,false).depth,"aliased buffers accepted");
        auto* pass=SDL_BeginGPUCopyPass(command);
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{depth,0,count*4};SDL_UploadToGPUBuffer(pass,&from,&to,false);
        from.offset=count*4;to={motion,0,count*16};SDL_UploadToGPUBuffer(pass,&from,&to,false);
        from.offset=count*20;to={terrain,0,count*4};SDL_UploadToGPUBuffer(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);
        starfox::render::TemporalGroundInputs ground;ground.coverage=terrain;ground.plane={0,0,1,-10};
        ground.packed_coverage=packed_coverage;
        ground.projection=ground.previous_projection={2,2,32,1};ground.previous_valid=true;ground.current_to_previous[12]=1;
        if(jittered) ground.raster_jitter={.375f,-.25f};
        auto bad_ground=ground;bad_ground.current_to_previous[15]=0;
        require(!converter.enqueue(device,command,depth,motion,width,height,.1f,100,reset,&bad_ground).depth,"projective terrain history accepted");
        bad_ground=ground;bad_ground.plane={};
        require(!converter.enqueue(device,command,depth,motion,width,height,.1f,100,reset,&bad_ground).depth,"degenerate terrain accepted");
        if(ground_case==2) ground.plane={0,1,.125f,-2}; // sloped ground / near-parallel rays
        if(ground_case==3) ground.plane={0,0,1,10}; // entirely behind current camera
        if(ground_case==4) ground.current_to_previous[14]=-20; // previous view behind camera
        const auto output=converter.enqueue(device,command,depth,reset?nullptr:motion,width,height,.1f,100,reset,ground_case?&ground:nullptr);
        require(output.depth && output.motion && output.exposure,converter.status().c_str());
        pass=SDL_BeginGPUCopyPass(command);
        SDL_GPUTextureRegion region{};region.texture=static_cast<SDL_GPUTexture*>(output.depth);region.w=width;region.h=height;region.d=1;
        SDL_GPUTextureTransferInfo target{};target.transfer_buffer=readback;target.pixels_per_row=width;target.rows_per_layer=height;
        SDL_DownloadFromGPUTexture(pass,&region,&target);
        region.texture=static_cast<SDL_GPUTexture*>(output.motion);target.offset=count*4;SDL_DownloadFromGPUTexture(pass,&region,&target);
        region.texture=static_cast<SDL_GPUTexture*>(output.exposure);region.w=region.h=1;target.offset=count*12;target.pixels_per_row=64;target.rows_per_layer=1;
        SDL_DownloadFromGPUTexture(pass,&region,&target);SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"temporal submit");
        require(SDL_WaitForGPUFences(device,true,&fence,1),"temporal completion");SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const float*>(SDL_MapGPUTransferBuffer(device,readback,false));require(data,"temporal readback");
        for(unsigned i=0;i<count;++i) {
            float z=depths[i];auto m=motions[i];
            if(ground_case && i%3==1 && !(std::isfinite(z) && z>=.1f && z<=100.f)) {
                const float px=float(i%width)+.5f-ground.raster_jitter[0],py=float(i/width)+.5f-ground.raster_jitter[1];
                const float rx=(px-32)/2,ry=(py-1)/2;
                const float denominator=ground.plane[0]*rx+ground.plane[1]*ry+ground.plane[2];
                const float candidate=std::abs(denominator)>1e-7f?-ground.plane[3]/denominator:0;
                if(std::isfinite(candidate) && candidate>=.1f && candidate<=100.f) {
                    z=candidate;m={};
                    const float previous_z=z+ground.current_to_previous[14];
                    if(previous_z>=.1f && previous_z<=100.f)
                        m={(rx*z+1)/previous_z*2+32-px,ry*z/previous_z*2+1-py,z,1};
                }
            }
            const bool valid=std::isfinite(z) && z>=.1f && z<=100.f;
            const float expected=valid?float((1.-double(.1f)/z)/(1.-double(.1f)/100.)):1.f;
            require(std::abs(data[i]-expected)<2e-6f,"projected depth differs");
            const bool correspondence=valid && !reset && m[3]==1 && std::isfinite(m[0]) && std::isfinite(m[1]) && std::isfinite(m[2]) && std::abs(m[2]-z)<=std::max(.001f,std::abs(z)*.00001f);
            if(correspondence) require(std::abs(data[count+i*2]-m[0])<1e-5f && std::abs(data[count+i*2+1]-m[1])<1e-5f,"motion conversion differs");
            else require(data[count+i*2]==-std::numeric_limits<float>::max() && data[count+i*2+1]==-std::numeric_limits<float>::max(),"motion validity conversion differs");
        }
        require(data[count*3]==1.f,"exposure must equal one");SDL_UnmapGPUTransferBuffer(device,readback);
    }
    converter.release_device();SDL_ReleaseGPUBuffer(device,depth);SDL_ReleaseGPUBuffer(device,motion);
    SDL_ReleaseGPUBuffer(device,terrain);
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,readback);
    std::cout<<"Temporal guide textures: 5120 depth/motion samples, packed/explicit terrain masks, jitter/slopes/occlusion/reset passed\n";
}
void check_temporal_hud(SDL_GPUDevice* device,bool preserve_artwork=true) {
    constexpr Uint32 w=64,h=2,n=w*h;
    std::array<Uint32,n*3> data{};
    for(unsigned i=0;i<n;++i) {data[i]=((i%5)<<8)|(i%2?0x10000000u:0)|(i%3?0:0x08000000u);data[n+i]=i%7?0xff123456u+i:0xff000000u;data[n*2+i]=0xffcc8844u-i;}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,n*12,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=n*4;
    auto* read=SDL_CreateGPUTransferBuffer(device,&ti);require(upload && read,"HUD transfers");
    auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);require(mapped,"HUD upload map");std::memcpy(mapped,data.data(),n*12);SDL_UnmapGPUTransferBuffer(device,upload);
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,n*4,0};auto* packed=SDL_CreateGPUBuffer(device,&bi);require(packed,"HUD tags");
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;
    auto* original=SDL_CreateGPUTexture(device,&info);auto* reconstructed=SDL_CreateGPUTexture(device,&info);require(original && reconstructed,"HUD source textures");
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"HUD command");
    auto* pass=SDL_BeginGPUCopyPass(command);SDL_GPUTransferBufferLocation src{upload,0};SDL_GPUBufferRegion dst{packed,0,n*4};SDL_UploadToGPUBuffer(pass,&src,&dst,false);
    SDL_GPUTextureTransferInfo transfer{upload,n*4,w,h};SDL_GPUTextureRegion region{};region.texture=original;region.w=w;region.h=h;region.d=1;
    SDL_UploadToGPUTexture(pass,&transfer,&region,false);transfer.offset=n*8;region.texture=reconstructed;SDL_UploadToGPUTexture(pass,&transfer,&region,false);SDL_EndGPUCopyPass(pass);
    starfox::render::GpuTemporalInputs converter;
    auto* output=converter.restore_hud(device,command,original,reconstructed,packed,w,h,preserve_artwork);require(output,converter.status().c_str());
    require(!converter.restore_hud(device,command,output,reconstructed,packed,w,h),"HUD output alias accepted");
    pass=SDL_BeginGPUCopyPass(command);region.texture=static_cast<SDL_GPUTexture*>(output);transfer={read,0,w,h};SDL_DownloadFromGPUTexture(pass,&region,&transfer);SDL_EndGPUCopyPass(pass);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"HUD submit");require(SDL_WaitForGPUFences(device,true,&fence,1),"HUD wait");SDL_ReleaseGPUFence(device,fence);
    const auto* pixels=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,read,false));require(pixels,"HUD map");
    for(unsigned i=0;i<n;++i) {
        const bool preserve=(i%5==1 && i%2==0) || (preserve_artwork && i%5==2 && i%3!=0);
        require(pixels[i]==data[(preserve?n:n*2)+i],"Artwork/HUD protection changed original/world pixels");
    }
    SDL_UnmapGPUTransferBuffer(device,read);converter.release_device();SDL_ReleaseGPUTexture(device,original);SDL_ReleaseGPUTexture(device,reconstructed);
    SDL_ReleaseGPUBuffer(device,packed);SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,read);
    std::cout<<"Temporal artwork/HUD restoration: 128 exact tagged pixels; terrain and world sprites remain reconstructed; output alias rejected\n";
}
void check_temporal_resample(SDL_GPUDevice* device) {
    constexpr Uint32 w=64,h=4,n=w*h;
    SDL_GPUTexture* inputs[3]{};
    const SDL_GPUTextureFormat formats[]{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R32_FLOAT,SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;
    for(unsigned i=0;i<3;++i) {info.format=formats[i];inputs[i]=SDL_CreateGPUTexture(device,&info);require(inputs[i],"resample inputs");}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,n*16,0};auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;auto* read=SDL_CreateGPUTransferBuffer(device,&ti);require(upload && read,"resample transfer");
    auto* bytes=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(bytes,"resample map");
    for(unsigned i=0;i<n;++i) {
        const Uint32 color=0xff804020;const float z=i%2?0.2f:0.8f;
        const float m[]{i%4==3?-std::numeric_limits<float>::max():float(i%2?4:12),i%4==3?-std::numeric_limits<float>::max():-2.f};
        std::memcpy(bytes+i*4,&color,4);std::memcpy(bytes+n*4+i*4,&z,4);std::memcpy(bytes+n*8+i*8,m,8);
    }
    SDL_UnmapGPUTransferBuffer(device,upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);auto* pass=SDL_BeginGPUCopyPass(command);
    for(unsigned i=0;i<3;++i) {SDL_GPUTextureTransferInfo t{upload,i==2?n*8:i*n*4,w,h};SDL_GPUTextureRegion r{};r.texture=inputs[i];r.w=w;r.h=h;r.d=1;SDL_UploadToGPUTexture(pass,&t,&r,false);}
    SDL_EndGPUCopyPass(pass);require(SDL_SubmitGPUCommandBuffer(command),"resample upload");
    starfox::render::GpuTemporalInputs converter;
    const starfox::render::GpuTemporalTextures guides{device,inputs[1],inputs[2],nullptr,w,h};
    for(Uint32 target_width:{32u,43u,64u,32u}) {
        const Uint32 target_height=2;
        command=SDL_AcquireGPUCommandBuffer(device);
        require(!converter.resample(device,command,inputs[0],guides,w+1,h).color,"resample accepted enlargement");
        auto result=converter.resample(device,command,inputs[0],guides,target_width,target_height);require(result.color,converter.status().c_str());
        require(!converter.resample(device,command,result.color,guides,target_width,target_height).color,"resample accepted alias");
        pass=SDL_BeginGPUCopyPass(command);
        void* textures[]{result.color,result.guides.depth,result.guides.motion};
        for(unsigned i=0;i<3;++i) {SDL_GPUTextureTransferInfo t{read,i==2?n*8:i*n*4,w,target_height};SDL_GPUTextureRegion r{};r.texture=static_cast<SDL_GPUTexture*>(textures[i]);r.w=target_width;r.h=target_height;r.d=1;SDL_DownloadFromGPUTexture(pass,&r,&t);}
        SDL_EndGPUCopyPass(pass);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"resample submit");require(SDL_WaitForGPUFences(device,true,&fence,1),"resample wait");SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,read,false));require(data,"resample download");
        for(unsigned y=0;y<target_height;++y) for(unsigned x=0;x<target_width;++x) {
            const unsigned i=y*w+x;Uint32 color;float z,m[2];std::memcpy(&color,data+i*4,4);std::memcpy(&z,data+n*4+i*4,4);std::memcpy(m,data+n*8+i*8,8);
            require(color==0xff804020,"resample constant color");
            const double ratio=double(w)/target_width;const unsigned first=unsigned(std::floor(x*ratio)),last=unsigned(std::ceil((x+1)*ratio));
            unsigned chosen=first;for(unsigned s=first;s<last;++s) if((s%2?0.2f:0.8f)<(chosen%2?0.2f:0.8f)) chosen=s;
            require(std::abs(z-(chosen%2?0.2f:0.8f))<1e-6f,"resample nearest depth");
            if(chosen%4==3) require(m[0]==-std::numeric_limits<float>::max() && m[1]==m[0],"resample invalid sentinel");
            else require(std::abs(m[0]-float(chosen%2?4:12)/ratio)<1e-5 && std::abs(m[1]+1.f)<1e-5,"resample scaled paired motion");
        }
        SDL_UnmapGPUTransferBuffer(device,read);
    }
    converter.release_device();for(auto* t:inputs) SDL_ReleaseGPUTexture(device,t);SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,read);
    std::cout<<"Temporal resampling: fractional ratios, paired closest depth/motion, invalid sentinel, resize and alias rejection passed\n";
}
int main() try {
    require(SDL_Init(SDL_INIT_VIDEO),"SDL");
    auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,true,nullptr);
    require(device,"device");
    check_temporal_textures(device);
    check_temporal_hud(device);
    check_temporal_hud(device,false);
    check_temporal_resample(device);
    starfox::render::GpuModel model;
    {
        using namespace starfox::render;
        starfox::assets::Shape sprite;sprite.colour_words={0x8000};
        starfox::assets::TextureImage texture;texture.descriptor=0x8000;
        texture.u_mask=texture.v_mask=7;texture.texels.resize(64);
        for(unsigned i=0;i<64;++i) texture.texels[i]=i%5?std::uint8_t(i+1):0;
        sprite.textures.push_back(texture);
        RenderPose p;p.simple_scaled_sprite=true;p.simple_sprite_world_size=100;p.z=300;
        p.x=-23;p.y=11;p.vanish_x=112;p.vanish_y=96;
        p.continuous_geometry=p.subpixel_projection=true;
        RenderSettings s;s.render_scale=2;
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"billboard reference command");
        const auto full=download(device,command,model.enqueue(device,command,sprite,p,s,224,192,true),false);
        for(bool scenePath:{false,true}) {
            constexpr unsigned w=149,h=127;
            command=SDL_AcquireGPUCommandBuffer(device);require(command,"billboard scaled command");
            GpuScene scene;GpuRasterOutput output;
            if(scenePath) {
                const std::array<GpuSceneDraw,1> original{GpuModelDraw{&sprite,p,s,true,GpuModelIdentity{1,1,1,1,1},true}};
                const auto scaled=resize_scene_raster(original,448,384);require(bool(scaled),"billboard scene conversion");
                output=scene.enqueue_batch(device,command,w,h,*scaled);
            } else output=model.enqueue(device,command,sprite,p,s,224,192,true,nullptr,nullptr,true,nullptr,nullptr,{}, {w,h});
            require(output.pixels && output.width==w && output.height==h,"billboard independent output");
            const auto small=download(device,command,output,true);
            unsigned covered=0;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                auto expected=full.pixels[(y*384/h)*448+x*448/w];
                if(scenePath && (expected&0x04000000U)) expected|=0x10000000U;
                require(small.pixels[y*w+x]==expected,"billboard fractional coverage/texture/palette");
                {
                    const auto expected_depth=(expected&0x04000000U)?float(p.z):0.f;
                    require(small.depths[y*w+x]==expected_depth,"billboard depth/transparent ownership");
                    require((small.pixels[y*w+x]&0x01000000U)==0,"billboard became a lighting receiver");
                }
                covered+=(expected&0x04000000U)!=0;
            }
            require(covered>100,"billboard fixture empty");
            Framebuffer cpu(w,h),world(w,h);world.enable_layer_tags(true);
            GpuComposite compositor;std::array<Rgba8,256> palette{};LayerCompositeSettings layer;
            require(compositor.compose(output,1,cpu,{},layer,palette,nullptr,nullptr,{},true),"world sprite compositor");
            std::vector<std::uint8_t> rgba;
            require(compositor.readback(world,rgba),"world sprite readback");
            for(unsigned i=0;i<w*h;++i) {
                const auto expected=scenePath?small.pixels[i]&255U:0U;
                require(world.pixels()[i]==expected,"world sprite incorrectly excluded as HUD");
            }
        }
        auto old=p;old.x+=30;old.z=400;
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"billboard motion command");
        const auto moving=model.enqueue(device,command,sprite,p,s,224,192,true,nullptr,nullptr,true,nullptr,&old,{}, {149,127});
        require(moving.motion,"billboard motion missing");
        const auto movement=download(device,command,moving,true);
        const auto rect=[&](const RenderPose& q) {
            const int size=std::clamp(int(std::trunc(q.simple_sprite_world_size*s.focal_length/q.z)),0,240);
            return std::array<double,3>{double(size),std::round(q.vanish_x)+std::trunc(q.x*s.focal_length/q.z)-size/2,
                std::round(q.vanish_y)+std::trunc(q.y*s.focal_length/q.z)-size/2};
        };
        const auto now_rect=rect(p),old_rect=rect(old);
        for(unsigned i=0;i<149*127;++i) {
            const auto& m=movement.motion[i];
            if(movement.depths[i]==0) {require(m[3]==0,"transparent sprite has motion");continue;}
            const double x=(i%149+.5)*224/149,y=(i/149+.5)*192/127;
            const double dx=((x-now_rect[1])*old_rect[0]/now_rect[0]+old_rect[1]-x)*149/224;
            const double dy=((y-now_rect[2])*old_rect[0]/now_rect[0]+old_rect[2]-y)*127/192;
            require(m[3]==1 && m[2]==300 && std::abs(m[0]-dx)<.001 && std::abs(m[1]-dy)<.001,"sprite translation/resize motion differs");
        }
        auto alternate=texture;alternate.descriptor=0x8001;
        sprite.textures.push_back(alternate);sprite.colour_words.push_back(0x8001);
        for(unsigned scenario=0;scenario<4;++scenario) {
            auto history=old;
            if(scenario==0) history.colour_frame=100; // Same static texture.
            if(scenario==1) history.simple_sprite_colour=1; // Different texture.
            if(scenario==2) history.z=127; // Previously clipped near the camera.
            if(scenario==3) history.simple_scaled_sprite=false;
            command=SDL_AcquireGPUCommandBuffer(device);require(command,"sprite history command");
            const auto output=model.enqueue(device,command,sprite,p,s,224,192,true,nullptr,nullptr,true,nullptr,&history,{}, {149,127});
            require(bool(output.motion)==(scenario==0),"sprite history accepted invalid correspondence or rejected static texture");
            const auto checked=download(device,command,output,true);
            require(checked.pixels==movement.pixels && checked.depths==movement.depths,"history changed current sprite rendering");
        }
        std::cout<<"Fractional billboard coverage, depth, translation/resize motion and history invalidation passed\n";
    }
    // Exact Q15 -identity gives the camera-space plane z = 400 + x/2.
    starfox::assets::Shape shape;
    shape.vertices={{80,60,-360},{-80,60,-440},{0,-70,-400}};
    shape.word_coordinates={true,true,true};
    starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};face.vertex_indices={0,2,1};
    shape.faces={face};shape.colour_words={0x3f11};shape.colour_materials={{0x3f11,{}}};
    starfox::render::RenderPose pose;pose.z=0;pose.use_rotation_matrix=true;
    pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};pose.force_colour=true;pose.forced_colour=0x11;
    std::size_t samples=0;
    for(bool fractional:{false,true}) for(unsigned scale:{1U,2U,4U}) for(double offset:{0.,.375}) {
        pose.subpixel_projection=fractional;
        pose.continuous_geometry=fractional;
        pose.vanish_x=112+offset;pose.vanish_y=96-offset;
        const auto enabled=capture(device,model,shape,pose,scale,true);
        const auto disabled=capture(device,model,shape,pose,scale,false);
        require(enabled.pixels==disabled.pixels,"depth changed native colour/coverage");
        require(enabled.surfaces==disabled.surfaces,"depth changed historical effects metadata");
        std::size_t valid=0;float minimum=1e9f,maximum=0;
        for(std::size_t i=0;i<enabled.pixels.size();++i) {
            const bool covered=(enabled.pixels[i]&0x04000000U)!=0;
            const float value=enabled.depths[i];
            if(!covered) {require(value==0,"uncovered depth is not unknown");continue;}
            require(value>0 && std::isfinite(value),"covered planar face has invalid depth");
            const auto x=i%(224*scale);
            const auto centre=(!fractional && scale==1)?std::round(pose.vanish_x):pose.vanish_x;
            const double expected=400/(1-.5*((double(x)+.5-centre*scale)/(256*scale)));
            require(std::abs(value-expected)<.002,"depth is not the per-pixel plane intersection");
            minimum=std::min(minimum,value);maximum=std::max(maximum,value);++valid;
        }
        if(valid<=1000*scale*scale || maximum-minimum<=50)
            throw std::runtime_error("sloped fixture: fractional="+std::to_string(fractional)+" scale="+std::to_string(scale)
                +" samples="+std::to_string(valid)+" range="+std::to_string(minimum)+".."+std::to_string(maximum));
        samples+=valid;
    }
    // Mixed scenes must retain depth through both fused and separate merges.
    // An opaque black overlay invalidates only its own pixels' geometry.
    starfox::render::GpuScene scene;
    starfox::render::RasterCommands overlay;overlay.reset(224,192);
    starfox::render::RasterCommand black{};black.left=100;black.right=120;black.top=88;black.bottom=100;
    overlay.commands.push_back(black);
    auto front_pose=pose;front_pose.z=80;front_pose.x=20;front_pose.forced_colour=0x22;
    starfox::render::GpuModelDraw back_draw{&shape,pose,{},true};back_draw.geometry_depth=true;
    starfox::render::GpuModelDraw front_draw{&shape,front_pose,{},true};front_draw.geometry_depth=true;
    const std::array<starfox::render::GpuSceneDraw,3> draws{back_draw,front_draw,starfox::render::GpuRasterDraw{&overlay,false,false}};
    const auto back_capture=capture(device,model,shape,pose,1,true);
    const auto front_capture=capture(device,model,shape,front_pose,1,true);
    Capture fused;
    for(bool separate:{false,true}) {
        if(separate) SDL_setenv_unsafe("STARFOX_TEST_SEPARATE_SCENE_MERGE","1",1);
        else SDL_unsetenv_unsafe("STARFOX_TEST_SEPARATE_SCENE_MERGE");
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"scene command");
        const auto output=scene.enqueue_batch(device,command,224,192,draws);
        if(!output.geometry_depth) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(scene.status());}
        const auto result=download(device,command,output,true);
        if(!separate) fused=result;
        else require(result.pixels==fused.pixels && result.surfaces==fused.surfaces && result.depths==fused.depths,"fused/separate scene depth mismatch");
        for(std::size_t i=0;i<result.pixels.size();++i) {
            const auto x=i%224,y=i/224;
            const bool black_pixel=x>=100 && x<120 && y>=88 && y<100;
            const auto expected=black_pixel?0.f:(front_capture.pixels[i]&0x04000000U)!=0?front_capture.depths[i]:back_capture.depths[i];
            require(result.depths[i]==expected,"scene depth did not follow visible painter ownership");
        }
    }
    SDL_unsetenv_unsafe("STARFOX_TEST_SEPARATE_SCENE_MERGE");
    // Beams must not inherit receiver metadata from the model behind them.
    front_draw.surface_metadata=false;front_draw.emissive=true;
    const std::array<starfox::render::GpuSceneDraw,2> beam_draws{back_draw,front_draw};
    {
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"beam command");
        const auto output=scene.enqueue_batch(device,command,224,192,beam_draws);
        require(output.geometry_depth,scene.status().c_str());
        const auto result=download(device,command,output,true);
        for(std::size_t i=0;i<result.pixels.size();++i)
            if(front_capture.pixels[i]&0x04000000U) {
                require((result.pixels[i]&0x01000000U)==0,"beam inherited receiver metadata");
                require((result.pixels[i]&255U)==(front_capture.pixels[i]&255U),"beam colour changed");
                require(result.depths[i]==front_capture.depths[i],"beam lost temporal depth");
            }
    }
    scene.release_device();
    for(const auto size:{std::array<unsigned,2>{149,127},std::array<unsigned,2>{299,255},std::array<unsigned,2>{533,299}})
    for(const auto jitter:{std::array<float,2>{0,0},std::array<float,2>{.375f,-.25f}}) {
        pose.continuous_geometry=true;pose.subpixel_projection=true;
        auto previous=pose;previous.x-=4;previous.y+=2;
        starfox::render::RenderSettings settings;settings.render_scale=2;
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"custom raster command");
        auto output=model.enqueue(device,command,shape,pose,settings,224,192,true,nullptr,nullptr,true,nullptr,&previous,jitter,size);
        require(output.width==size[0] && output.height==size[1] && output.motion,model.status().c_str());
        const auto capture=download(device,command,output,true);std::size_t visible=0;
        for(std::size_t i=0;i<capture.depths.size();++i) if(capture.depths[i]>0) {
            ++visible;const auto z=capture.depths[i];const auto& mv=capture.motion[i];
            require(mv[3]==1,"custom raster motion invalid");
            require(std::abs(mv[0]+4.f*256*(float(size[0])/224)/z)<.0003f,"custom raster X motion");
            require(std::abs(mv[1]-2.f*256*(float(size[1])/192)/z)<.0003f,"custom raster Y motion");
        }
        require(visible>500,"custom raster geometry empty");
        starfox::render::GpuModelDraw draw{&shape,pose,settings,true};draw.previous_pose=previous;
        draw.raster_jitter=jitter;draw.logical_viewport={224,192};
        starfox::render::GpuScene reduced_scene;const std::array<starfox::render::GpuSceneDraw,1> draws{draw};
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"reduced scene command");
        const auto merged=reduced_scene.enqueue_batch(device,command,size[0],size[1],draws);
        require(merged.motion,reduced_scene.status().c_str());const auto result=download(device,command,merged,true);
        require(result.pixels==capture.pixels && result.depths==capture.depths && result.motion==capture.motion,"reduced scene transport differs");
    }
    std::cout<<"Arbitrary model raster sizes: reduced/nonuniform dimensions and jittered depth/motion passed\n";
    // Motion is derived from the actual model's current planar surface. A
    // covering opaque HUD pixel invalidates it; transparent gaps retain it.
    for(unsigned scale:{1U,2U,4U}) for(bool moving:{false,true})
    for(const auto jitter:{std::array<float,2>{0,0},std::array<float,2>{.375f,-.25f}}) {
        SDL_unsetenv_unsafe("STARFOX_TEST_SEPARATE_SCENE_MERGE");
        pose.continuous_geometry=true;pose.subpixel_projection=true;
        auto previous=pose;if(moving) {previous.x-=4;previous.y+=2;}
        previous.animation_frame=pose.animation_frame+1; // static model, different simulation ticks
        starfox::render::RenderSettings settings;settings.render_scale=scale;
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"motion command");
        auto output=model.enqueue(device,command,shape,pose,settings,224,192,true,nullptr,nullptr,true,nullptr,&previous,jitter);
        require(output.motion,"model motion missing");
        const auto motion=download(device,command,output,true);
        std::size_t valid=0;
        for(std::size_t i=0;i<motion.pixels.size();++i) {
            const auto& mv=motion.motion[i];const float depth=motion.depths[i];
            require(mv[3]==float(depth>0),"model motion validity differs from depth");
            if(depth>0) {
                ++valid;
                require(std::abs(mv[0]-(moving?float(-4*256*int(scale))/depth:0.f))<.0002f,"model X motion mismatch");
                require(std::abs(mv[1]-(moving?float(2*256*scale)/depth:0.f))<.0002f,"model Y motion mismatch");
            }
        }
        require(valid>1000,"motion fixture empty");
        starfox::render::GpuModelDraw draw{&shape,pose,settings,true};draw.previous_pose=previous;
        draw.raster_jitter=jitter;
        starfox::render::RasterCommands commands;commands.reset(224*scale,192*scale);
        starfox::render::RasterCommand cover{};cover.left=105*scale;cover.right=117*scale;
        cover.top=90*scale;cover.bottom=102*scale;commands.commands.push_back(cover);
        const std::array<starfox::render::GpuSceneDraw,2> draws{draw,starfox::render::GpuRasterDraw{&commands,false,false}};
        starfox::render::GpuScene scene;
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"motion scene command");
        output=scene.enqueue_batch(device,command,224*scale,192*scale,draws);
        require(output.motion,"scene motion missing");
        const auto merged=download(device,command,output,true);
        for(unsigned y=0;y<192*scale;++y) for(unsigned x=0;x<224*scale;++x) {
            const auto i=size_t(y)*224*scale+x;
            const bool overlay=x>=105*scale && x<117*scale && y>=90*scale && y<102*scale;
            require(merged.motion[i]==(overlay?std::array<float,4>{}:motion.motion[i]),"motion painter ownership mismatch");
        }
        const unsigned destination_scale=scale==4?1:4;
        starfox::render::Framebuffer final_frame(236,198,destination_scale);
        std::vector<std::uint8_t> foreground(final_frame.pixels().size()),after_late(foreground.size());
        for(unsigned y=0;y<final_frame.stored_height();++y) for(unsigned x=0;x<final_frame.stored_width();++x) {
            const auto i=size_t(y)*final_frame.stored_width()+x;
            foreground[i]=(x+y)%29==0;after_late[i]=(x+3*y)%31==0;
        }
        std::array<starfox::render::Rgba8,256> palette{};
        starfox::render::LayerCompositeSettings layer{};
        layer.offset_x=6;layer.offset_y=3;layer.clip_left=6;layer.clip_right=230;
        layer.clip_top=3;layer.clip_bottom=195;
        starfox::render::GpuComposite compositor;
        require(compositor.compose(output,scale,final_frame,foreground,layer,palette,nullptr,nullptr,after_late),"temporal compositor");
        auto composite=compositor.output();
        require(composite.geometry_depth && composite.motion,"composed temporal buffers missing");
        starfox::render::GpuRasterOutput temporal{composite.device,composite.packed,composite.surfaces,
            composite.width,composite.height,0,composite.geometry_depth,composite.motion};
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"composed temporal readback");
        const auto composed=download(device,command,temporal,true);
        for(unsigned y=0;y<composite.height;++y) for(unsigned x=0;x<composite.width;++x) {
            const size_t i=size_t(y)*composite.width+x;
            const int sx=int(x/destination_scale)-6,sy=int(y/destination_scale)-3;
            std::array<float,4> expected{};float expected_depth=0;
            if(sx>=0 && sy>=0 && sx<224 && sy<192 && !foreground[i] && !after_late[i]) {
                const unsigned subx=std::min(scale-1,((x%destination_scale)*2+1)*scale/(destination_scale*2));
                const unsigned suby=std::min(scale-1,((y%destination_scale)*2+1)*scale/(destination_scale*2));
                const size_t n=size_t(sy*scale+suby)*(224*scale)+sx*scale+subx;
                if(merged.pixels[n]&255) {
                    expected_depth=merged.depths[n];expected=merged.motion[n];
                    expected[0]*=float(destination_scale)/scale;expected[1]*=float(destination_scale)/scale;
                }
            }
            require(composed.depths[i]==expected_depth && composed.motion[i]==expected,"composed temporal ownership/scale mismatch");
        }
        constexpr unsigned rw=299,rh=255;
        require(compositor.compose(output,scale,final_frame,foreground,layer,palette,nullptr,nullptr,after_late,false,
            {output.width,output.height,rw,rh}),"fractional temporal compositor");
        const auto resized=compositor.output();
        starfox::render::GpuRasterOutput scaled{resized.device,resized.packed,resized.surfaces,rw,rh,0,resized.geometry_depth,resized.motion};
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"fractional composed readback");
        const auto reduced=download(device,command,scaled,true);
        for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x) {
            const auto from=(y*composite.height/rh)*composite.width+x*composite.width/rw;
            const auto to=y*rw+x;
            require(reduced.depths[to]==composed.depths[from],"fractional compositor depth ownership");
            for(unsigned axis=0;axis<4;++axis) {
                const auto factor=axis==0?float(rw)/composite.width:axis==1?float(rh)/composite.height:1.f;
                const auto expected=composed.motion[from][axis]*factor;
                require(std::abs(reduced.motion[to][axis]-expected)<=1e-5f*std::max(1.f,std::abs(expected)),"fractional compositor motion units");
            }
        }
        compositor.release_device();scene.release_device();
    }
    std::cout<<"Model per-pixel motion and opaque HUD ownership pass at 1x/2x/4x\n";
    // A folded quad has no single plane: never report its face-average as depth.
    shape.vertices={{80,60,-360},{-80,60,-440},{-80,-60,-460},{80,-60,-360}};
    shape.word_coordinates={true,true,true,true};shape.faces[0].vertex_indices={0,3,2,1};
    const auto folded=capture(device,model,shape,pose,1,true);
    std::size_t covered=0;
    for(std::size_t i=0;i<folded.pixels.size();++i) {
        covered+=(folded.pixels[i]&0x04000000U)!=0;
        require(folded.depths[i]==0,"folded face manufactured planar depth");
    }
    require(covered>1000,"folded fixture has no coverage");
    model.release_device();SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<"GPU geometry depth: "<<samples<<" analytical plane samples, native/fractional 1x/2x/4x and fractional centres; colour/effects unchanged; folded face invalidation and mixed-scene ownership passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
