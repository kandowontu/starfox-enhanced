#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Standalone presentation diagnostic: deliberately no game code or assets.
int main(int argc, char** argv) {
    const char* driver = argc > 1 ? argv[1] : "gpu";
    const int vsync = argc > 2 ? std::atoi(argv[2]) : 1;
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    auto* window = SDL_CreateWindow("Presentation pacing diagnostic", 960, 540, 0);
    auto* renderer = window ? SDL_CreateRenderer(window, driver) : nullptr;
    if (!renderer || !SDL_SetRenderVSync(renderer, vsync)) {
        std::fprintf(stderr, "%s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    const auto* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    std::printf("renderer=%s vsync=%d refresh=%.3f\n", SDL_GetRendererName(renderer), vsync,
                mode ? mode->refresh_rate : 0.0f);
    std::vector<double> samples;
    for (int frame = 0; frame < 150; ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 2; }
        }
        const auto start = SDL_GetTicksNS();
        if (!SDL_SetRenderDrawColor(renderer, 20, static_cast<Uint8>(frame), 60, 255) ||
            !SDL_RenderClear(renderer) || !SDL_RenderPresent(renderer)) {
            std::fprintf(stderr, "%s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
        }
        if (frame >= 30) samples.push_back(static_cast<double>(SDL_GetTicksNS() - start) / 1e6);
    }
    std::sort(samples.begin(), samples.end());
    std::printf("clear-and-present-ms median=%.3f p99=%.3f max=%.3f\n",
                samples[samples.size()/2], samples[(samples.size()-1)*99/100], samples.back());
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
}
