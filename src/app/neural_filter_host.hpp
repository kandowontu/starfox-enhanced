#pragma once
#include <string_view>
#if defined(_WIN32) && !defined(STARFOX_UWP)
#include <windows.h>
#endif

// Controls only an already-loaded optional RenoDX installation. Configuration
// is a next-launch preference, not proof of successful neural evaluation.
class NeuralFilterHost {
    bool discovered_{}, launch_requested_{}, requested_{}, save_failed_{};
#if defined(_WIN32) && !defined(STARFOX_UWP)
    using Set = void(*)(void*,void*,const char*,const char*,const char*);
    using Get = bool(*)(void*,void*,const char*,const char*,char*,size_t*);
    Set set_{};
    Get get_{};
    bool read(bool& value) const {
        char text[32]{};size_t size=sizeof(text);
        if(!get_(nullptr,nullptr,"RenoDX.DLSS5","NeuralUplift",text,&size)) return false;
        if(std::string_view(text)=="0") {value=false;return true;}
        if(std::string_view(text)=="1") {value=true;return true;}
        return false;
    }
#endif
public:
    bool discover() {
        if(discovered_) return true;
#if defined(_WIN32) && !defined(STARFOX_UWP)
        // Never load unsigned code or infer compatibility from a DLL on disk.
        if(!GetModuleHandleW(L"renodx-dlss5.addon64")) return false;
        const auto proxy=GetModuleHandleW(L"dxgi.dll");
        if(!proxy) return false;
        set_=reinterpret_cast<Set>(GetProcAddress(proxy,"ReShadeSetConfigValue"));
        get_=reinterpret_cast<Get>(GetProcAddress(proxy,"ReShadeGetConfigValue"));
        // Require an explicit startup configuration rather than guessing the
        // add-on's defaults. The isolated installer/configurator defaults OFF.
        if(!set_ || !get_ || !read(launch_requested_)) return false;
        requested_=launch_requested_;discovered_=true;
#endif
        return discovered_;
    }
    bool requested() const noexcept {return requested_;}
    bool request(bool enabled) {
        if(!discovered_) return false;
        if(requested_==enabled && !save_failed_) return true;
#if defined(_WIN32) && !defined(STARFOX_UWP)
        set_(nullptr,nullptr,"RenoDX.DLSS5","NeuralUplift",enabled?"1":"0");
        bool saved{};
        save_failed_=!read(saved) || saved!=enabled;
        if(save_failed_) return false;
        requested_=enabled;
#endif
        return true;
    }
    std::string_view label() const noexcept {
        if(save_failed_) return "SAVE FAILED";
        if(!discovered_) return "NOT INSTALLED";
        if(requested_!=launch_requested_) return requested_?"ON - RESTART":"OFF - RESTART";
        return requested_?"ON":"OFF";
    }
};
