// Keep the WinRT boundary in C++/CX; the portable runtime exports SDL_main.
// Owning the entry point also lets us log failures before SDL_Init runs.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <windows.h>
#include <wrl.h>
#include <cstdio>

#pragma warning(disable : 4447)
#pragma comment(lib, "runtimeobject.lib")

namespace {
void write_startup_log(const char* message, bool truncate = false) noexcept {
    try {
        const auto path = Windows::Storage::ApplicationData::Current->LocalFolder->Path
            + L"\\StarFoxEnhanced-startup.log";
        const auto file = CreateFile2(path->Data(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, truncate ? CREATE_ALWAYS : OPEN_ALWAYS, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        char line[2048];
        const auto length = std::snprintf(line, sizeof(line), "[%llu ms] %s\r\n",
            static_cast<unsigned long long>(GetTickCount64()), message);
        DWORD written{};
        if (length > 0) WriteFile(file, line,
            static_cast<DWORD>(length < sizeof(line) ? length : sizeof(line) - 1), &written, nullptr);
        CloseHandle(file);
    } catch (...) {
        // Even logging an activation failure must be best-effort.
    }
}

void log_winrt_error(Platform::Exception^ error) noexcept {
    char message[96];
    std::snprintf(message, sizeof(message), "FAILED: WinRT HRESULT 0x%08lX",
        static_cast<unsigned long>(error->HResult));
    write_startup_log(message);
}

int run_game(int argc, char** argv) {
    write_startup_log("CoreApplication activated; entering SDL_main");
    try {
        // Catch a broken SDL platform thread ID before WASAPI's management
        // thread can deadlock on a reentrant task during device activation.
        SDL_ThreadID observed_id{};
        auto* thread = SDL_CreateThread([](void* data) {
            *static_cast<SDL_ThreadID*>(data) = SDL_GetCurrentThreadID();
            return 0;
        }, "UWP thread check", &observed_id);
        if (!thread) {
            write_startup_log("FAILED: could not create startup test thread");
            return 1;
        }
        const auto reported_id = SDL_GetThreadID(thread);
        SDL_WaitThread(thread, nullptr);
        if (reported_id == 0 || reported_id != observed_id) {
            write_startup_log("FAILED: SDL worker thread ID mismatch");
            return 1;
        }
        write_startup_log("SDL worker thread identity verified");
        const auto result = SDL_main(argc, argv);
        write_startup_log(result == 0 ? "SDL_main returned normally" : "SDL_main returned an error");
        return result;
    } catch (Platform::Exception^ error) {
        log_winrt_error(error);
        throw;
    }
}
}

extern "C" void starfox_uwp_log(const char* message) noexcept {
    write_startup_log(message);
}

extern "C" int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const auto initialized = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialized)) return 1;
    write_startup_log("WinMain entered", true);
    try {
        const auto version = Windows::ApplicationModel::Package::Current->Id->Version;
        char message[100];
        std::snprintf(message, sizeof(message), "Package %u.%u.%u.%u (x64 UWP)",
            version.Major, version.Minor, version.Build, version.Revision);
        write_startup_log(message);
        SDL_SetLogOutputFunction([](void*, int, SDL_LogPriority priority, const char* text) {
            if (priority >= SDL_LOG_PRIORITY_WARN) write_startup_log(text);
        }, nullptr);
        const auto result = SDL_RunApp(0, nullptr, run_game, nullptr);
        Windows::Foundation::Uninitialize();
        return result;
    } catch (Platform::Exception^ error) {
        log_winrt_error(error);
        Windows::Foundation::Uninitialize();
        return 1;
    }
}
