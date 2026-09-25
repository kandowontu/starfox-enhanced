// Accessors for Star Fox's Metal acceleration-structure pass. This file is
// included by the private pinned SDL Metal backend, where its types and
// resource-tracking helpers are defined.
void *SDL_StarfoxMetalDevice(SDL_GPUDevice *device)
{
    if (!device || SDL_strcmp(SDL_GetGPUDeviceDriver(device), "metal") != 0) {
        return NULL;
    }
    MetalRenderer *renderer = (MetalRenderer *)device->driverData;
    return (__bridge void *)renderer->device;
}

void *SDL_StarfoxMetalCommandBuffer(SDL_GPUCommandBuffer *command)
{
    if (!command) {
        return NULL;
    }
    return (__bridge void *)((MetalCommandBuffer *)command)->handle;
}

void *SDL_StarfoxMetalBuffer(SDL_GPUBuffer *buffer)
{
    if (!buffer) {
        return NULL;
    }
    MetalBufferContainer *container = (MetalBufferContainer *)buffer;
    return container->activeBuffer
        ? (__bridge void *)container->activeBuffer->handle : NULL;
}

bool SDL_StarfoxMetalTrackBuffer(SDL_GPUCommandBuffer *command,
                                SDL_GPUBuffer *buffer)
{
    if (!command || !buffer) {
        return false;
    }
    MetalBufferContainer *container = (MetalBufferContainer *)buffer;
    if (!container->activeBuffer) {
        return false;
    }
    METAL_INTERNAL_TrackBuffer((MetalCommandBuffer *)command,
                               container->activeBuffer);
    return true;
}

bool SDL_StarfoxMetalPrepareWrite(SDL_GPUDevice *device,
                                  SDL_GPUCommandBuffer *command,
                                  SDL_GPUBuffer *buffer)
{
    if (!device || !command || !buffer ||
        SDL_strcmp(SDL_GetGPUDeviceDriver(device), "metal") != 0) {
        return false;
    }
    MetalRenderer *renderer = (MetalRenderer *)device->driverData;
    MetalBufferContainer *container = (MetalBufferContainer *)buffer;
    MetalBuffer *active = METAL_INTERNAL_PrepareBufferForWrite(renderer,
                                                               container, true);
    if (!active) {
        return false;
    }
    METAL_INTERNAL_TrackBuffer((MetalCommandBuffer *)command, active);
    return true;
}
