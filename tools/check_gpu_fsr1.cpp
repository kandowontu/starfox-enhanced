#include "starfox/render/gpu_fsr1.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>

static void require(bool condition) {
    if(!condition) throw std::runtime_error(SDL_GetError());
}
int main() {
    SDL_GPUDevice* device{};
    SDL_GPUTexture* input{};
    try {
        require(SDL_Init(SDL_INIT_VIDEO));
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL
            |SDL_GPU_SHADERFORMAT_MSL,true,nullptr);require(device);
        const auto vendor=SDL_GetNumberProperty(SDL_GetGPUDeviceProperties(device),"starfox.gpu.vendor_id",0);
        std::cout<<"active adapter vendor=0x"<<std::hex<<vendor<<std::dec<<'\n';
        if(std::string_view(SDL_GetGPUDeviceDriver(device))!="metal" && !vendor)
            throw std::runtime_error("Active adapter vendor property missing");
        SDL_GPUTextureCreateInfo info{};
        info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width=7;info.height=5;info.layer_count_or_depth=1;info.num_levels=1;
        input=SDL_CreateGPUTexture(device,&info);require(input);
        {
            starfox::render::GpuFsr1 fsr;
            // Reuse, resize and opposite constant colours catch stale contents
            // and verify dispatch guards for non-multiples of the 8x8 group.
            for(unsigned iteration=0;iteration<4;++iteration) {
                const bool red=(iteration%2)==0;
                const starfox::render::Fsr1Extent extent=iteration<2
                    ?starfox::render::Fsr1Extent{11,9}:starfox::render::Fsr1Extent{17,13};
                auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
                SDL_GPUColorTargetInfo clear{};clear.texture=input;
                clear.clear_color={red?1.0F:0.0F,red?0.0F:1.0F,0,1};
                clear.load_op=SDL_GPU_LOADOP_CLEAR;clear.store_op=SDL_GPU_STOREOP_STORE;
                auto* render=SDL_BeginGPURenderPass(command,&clear,1,nullptr);require(render);
                SDL_EndGPURenderPass(render);
                auto* output=static_cast<SDL_GPUTexture*>(fsr.enqueue(device,command,input,{7,5},extent));
                if(!output) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(fsr.status());}
                SDL_GPUTransferBufferCreateInfo transfer_info{};
                transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
                transfer_info.size=extent.width*extent.height*8;
                auto* transfer=SDL_CreateGPUTransferBuffer(device,&transfer_info);require(transfer);
                auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
                SDL_GPUTextureRegion region{};region.texture=output;region.w=extent.width;
                region.h=extent.height;region.d=1;
                SDL_GPUTextureTransferInfo destination{};destination.transfer_buffer=transfer;
                destination.pixels_per_row=extent.width;destination.rows_per_layer=extent.height;
                SDL_DownloadFromGPUTexture(copy,&region,&destination);
                SDL_EndGPUCopyPass(copy);
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
                require(SDL_WaitForGPUFences(device,true,&fence,1));
                SDL_ReleaseGPUFence(device,fence);
                const auto* pixels=static_cast<const std::uint16_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));
                require(pixels);
                bool match=true;
                for(unsigned i=0;i<extent.width*extent.height;++i) {
                    const std::uint16_t expected[4]{std::uint16_t(red?0x3c00:0),std::uint16_t(red?0:0x3c00),0,0x3c00};
                    for(unsigned c=0;c<4;++c) {
                        // Upstream approximate reciprocals do not preserve 1
                        // exactly. Permit at most one 8-bit step of darkening
                        // (eight half-float units immediately below 1).
                        // Zero channels and opaque alpha must remain exact.
                        const int delta=int(pixels[i*4+c])-int(expected[c]);
                        const bool valid=expected[c]==0x3c00 && c!=3
                            ? delta>=-8 && delta<=0 : delta==0;
                        if(!valid) {
                            if(match) std::cerr<<"iteration="<<iteration<<" pixel="<<i<<" channel="<<c
                                <<" half="<<pixels[i*4+c]<<" expected="<<expected[c]<<'\n';
                            match=false;
                        }
                    }
                }
                SDL_UnmapGPUTransferBuffer(device,transfer);
                SDL_ReleaseGPUTransferBuffer(device,transfer);
                if(!match) throw std::runtime_error("FSR1 constant-colour/resize mismatch");
            }
            fsr.release_device();
        }
        SDL_ReleaseGPUTexture(device,input);SDL_DestroyGPUDevice(device);SDL_Quit();
        std::cout<<"FSR1 EASU/RCAS constant-colour, odd-size, reuse and resize dispatch checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        if(device) {SDL_WaitForGPUIdle(device);if(input) SDL_ReleaseGPUTexture(device,input);SDL_DestroyGPUDevice(device);}
        SDL_Quit();return 1;
    }
}
