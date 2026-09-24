#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

// A small append-only startup journal survives a driver hang/forced close.
// Diagnostics must never prevent launching from a read-only installation.
class StartupTrace {
    std::ofstream file_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
public:
    explicit StartupTrace(const std::filesystem::path& directory) noexcept {
        try {
            file_.open(directory / "startup.log",std::ios::app);
            file_<<"\nlaunch unix-seconds="<<std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()<<'\n';
            mark("process started");
        } catch (...) {}
    }
    void mark(std::string_view stage) noexcept {
        try {
            file_<<std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-start_).count()<<" ms: "<<stage<<'\n';
            file_.flush();
        } catch (...) {}
    }
};
