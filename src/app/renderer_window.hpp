#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

// A Windows HWND used by a flip-model swap chain cannot subsequently present
// GDI software frames reliably. Replace that HWND when leaving GPU rendering.
inline SDL_Window* recreate_software_window(SDL_Window* previous) {
    int x{}, y{}, width{}, height{};
    SDL_GetWindowPosition(previous, &x, &y);
    SDL_GetWindowSize(previous, &width, &height);
    const auto flags = SDL_GetWindowFlags(previous);
    auto* replacement = SDL_CreateWindow(SDL_GetWindowTitle(previous), width, height,
        SDL_WINDOW_HIDDEN | (flags & (SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS
            | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_ALWAYS_ON_TOP)));
    if (!replacement) {
        throw std::runtime_error{std::string{"Recreate software window: "} + SDL_GetError()};
    }
    SDL_SetWindowPosition(replacement, x, y);
    if (flags & SDL_WINDOW_MAXIMIZED) SDL_MaximizeWindow(replacement);
    if (flags & SDL_WINDOW_FULLSCREEN) SDL_SetWindowFullscreen(replacement, true);
    const auto relative_mouse = SDL_GetWindowRelativeMouseMode(previous);
    SDL_DestroyWindow(previous);
    if (!(flags & SDL_WINDOW_HIDDEN)) SDL_ShowWindow(replacement);
    SDL_SetWindowRelativeMouseMode(replacement, relative_mouse);
    SDL_SyncWindow(replacement);
    return replacement;
}
