// Interactive Windows regression: checks actual desktop pixels, not renderer readback.
// Run with no arguments for the fix; --without-replacement reproduces the old freeze.
#include "../src/app/renderer_window.hpp"
#include <windows.h>
#include <cstdio>

int main(int argc, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 2;
    auto* window = SDL_CreateWindow("Renderer switch check", 320, 240, SDL_WINDOW_RESIZABLE);
    auto* renderer = SDL_CreateRenderer(window, "direct3d11");
    if (!renderer) return 3;
    auto present = [&](unsigned char r, unsigned char g, unsigned char b) {
        SDL_RaiseWindow(window);
        const auto hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        if (!SDL_RenderClear(renderer) || !SDL_RenderPresent(renderer)) return false;
        for (int i = 0; i < 15; ++i) { SDL_PumpEvents(); SDL_Delay(10); }
        POINT point{160, 120};
        ClientToScreen(hwnd, &point);
        const auto dc = GetDC(nullptr);
        const auto color = GetPixel(dc, point.x, point.y);
        ReleaseDC(nullptr, dc);
        std::printf("wanted %u,%u,%u; desktop %u,%u,%u\n", r,g,b,
            GetRValue(color),GetGValue(color),GetBValue(color));
        return color == RGB(r,g,b);
    };
    bool success = present(255,0,0);
    for (int pass = 0; pass < 2; ++pass) {
        SDL_DestroyRenderer(renderer);
        if (argc == 1) window = recreate_software_window(window);
        renderer = SDL_CreateRenderer(window, "software");
        if (!renderer) return 4;
        success = present(0,255,0) && success;
        success = present(0,0,255) && success;
        SDL_SetWindowSize(window, 400, 300);
        success = present(255,255,0) && success;
        SDL_DestroyRenderer(renderer);
        renderer = SDL_CreateRenderer(window, "direct3d11");
        if (!renderer) return 5;
        success = present(255,0,0) && success;
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return success ? 0 : 1;
}
