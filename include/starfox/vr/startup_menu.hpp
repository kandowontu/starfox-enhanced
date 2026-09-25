#pragma once
#include "starfox/vr/openxr_input.hpp"
#include "starfox/vr/menu_stick.hpp"
#include "starfox/localization/menu_catalog.hpp"
#include "starfox/render/effect_types.hpp"
#include <array>
#include <string>
#include <vector>
#include <span>
namespace starfox::vr {
class StartupMenu {
public:
    enum class Page { main,options,cheats,three_d,two_d };
    Page page{Page::main};
    bool open{true},god_mode{},extended{},alternate_available{},runtime{};
    bool infinite_bombs{},infinite_boost{},infinite_lives{},swap_face_buttons{};
    bool unlocked_pace{true};
    bool msu_available{},msu_music{};
    bool ray_tracing{},ray_tracing_available{};
    bool enhanced_sky{};
    bool preview{}; // Session-only: never resume a game into preview automatically.
    unsigned model_effect{},world_effect{},model_intensity{100},world_intensity{100};
    static constexpr std::array<unsigned,11> supported_effects{0,1,4,8,9,10,11,13,14,15,16};
    static std::string_view style_name(unsigned style) noexcept {
        // Preserve existing VR display names; IDs are shared with desktop.
        if(style==14) return "POSTERIZE";
        if(style==15) return "CYANOTYPE";
        if(style==16) return "WARM FILM";
        return style<render::effect_names.size()?render::effect_names[style]:"OFF";
    }
    static unsigned next_style(unsigned current,bool world=false) noexcept {
        for(size_t i=0;i<supported_effects.size();++i)
            if(supported_effects[i]==current) {
                auto next=supported_effects[(i+1)%supported_effects.size()];
                return world && next==1?4:next;
            }
        return 0;
    }
    // Preserve the user's preference across devices without treating it as
    // hardware capability. Unsupported headsets never activate the renderer.
    bool ray_tracing_enabled() const noexcept {return ray_tracing && ray_tracing_available;}
    unsigned language{},selection{},revision{},default_laser{},selected_level{};
    unsigned music_volume{100},sfx_volume{100},crosshair_colour{};
    unsigned steer_sensitivity_index{};
    static constexpr std::array<unsigned,5> steer_sensitivities{100,40,55,70,85};
    std::array<std::vector<unsigned>,2> level_choices{{{0},{0}}};
    // Versioned preferences deliberately exclude navigation, level jumps and
    // cartridge availability. Those belong to the current session only.
    std::array<uint8_t,20> preferences() const noexcept {
        return {'S','F','V','R',5,uint8_t(language),uint8_t(god_mode),
            uint8_t(default_laser),uint8_t(msu_music),uint8_t(music_volume),
            uint8_t(sfx_volume),uint8_t(unsigned(unlocked_pace)|(unsigned(ray_tracing)<<1)|(unsigned(enhanced_sky)<<2)
                |(steer_sensitivity_index<<3)),uint8_t(crosshair_colour),
            uint8_t(swap_face_buttons),uint8_t(infinite_bombs),uint8_t(unsigned(infinite_boost)|(unsigned(infinite_lives)<<1)),
            uint8_t(model_effect),uint8_t(world_effect),uint8_t(model_intensity),uint8_t(world_intensity)};
    }
    bool restore_preferences(std::span<const uint8_t> bytes) noexcept {
        if((bytes.size()!=16 && bytes.size()!=20) || bytes[0]!='S' || bytes[1]!='F' || bytes[2]!='V'
            || bytes[3]!='R' || (bytes[4]<1 || bytes[4]>5) || bytes[5]>=6 || bytes[7]>=3
            || bytes[9]>100 || bytes[10]>100 || bytes[12]>=8) return false;
        if(bytes.size()!=(bytes[4]>=4?20U:16U)) return false;
        if(bytes[4]>=4) {
            for(unsigned i:{16U,17U}) {
                bool valid=false;for(auto effect:supported_effects) valid|=bytes[i]==effect;
                if(!valid) return false;
            }
            if(bytes[17]==1 || bytes[18]>100 || bytes[19]>100) return false;
        }
        for(const auto index:{6,8,13,14}) if(bytes[index]>1) return false;
        if(bytes[15]>(bytes[4]>=3?3:1)) return false;
        if(bytes[4]>=5) {
            if((bytes[11]>>3)>=steer_sensitivities.size()) return false;
        } else if(bytes[11]>(bytes[4]==1?1:3)) return false;
        language=bytes[5];god_mode=bytes[6];default_laser=bytes[7];msu_music=bytes[8];
        music_volume=bytes[9];sfx_volume=bytes[10];unlocked_pace=(bytes[11]&1)!=0;
        ray_tracing=bytes[4]>=2 && (bytes[11]&2)!=0;
        enhanced_sky=bytes[4]>=5 && (bytes[11]&4)!=0;
        steer_sensitivity_index=bytes[4]>=5?bytes[11]>>3:0;
        crosshair_colour=bytes[12];swap_face_buttons=bytes[13];
        infinite_bombs=bytes[14];infinite_boost=(bytes[15]&1)!=0;
        infinite_lives=bytes[4]>=3 && (bytes[15]&2)!=0;++revision;
        model_effect=bytes[4]>=4?bytes[16]:0;world_effect=bytes[4]>=4?bytes[17]:0;
        model_intensity=bytes[4]>=4?bytes[18]:100;world_intensity=bytes[4]>=4?bytes[19]:100;
        return true;
    }
    unsigned row_count() const noexcept {return page==Page::main?6:page==Page::options?10:page==Page::three_d?(ray_tracing_available?5:4):page==Page::two_d?5:7;}
    unsigned first_visible_row() const noexcept {return selection<6?0:selection-5;}
    VrControls gameplay_controls(VrControls controls) const noexcept {
        // Native mapping is Y=fire, X=boost, A=bomb, B=brake.
        // Keep menu confirmation unchanged when gameplay buttons are swapped.
        if(swap_face_buttons) {
            std::swap(controls.fire,controls.boost);
            std::swap(controls.bomb,controls.brake);
        }
        const float gain=float(steer_sensitivities[steer_sensitivity_index%steer_sensitivities.size()])/100.F;
        controls.steer.x*=gain;controls.steer.y*=gain;
        return controls;
    }
    std::string level_name() const {
        return selected_level?"LEVEL"+std::to_string(selected_level/10)+"_"+std::to_string(selected_level%10):"OFF";
    }
    void open_runtime() noexcept {
        runtime=true;open=true;page=Page::main;selection=4;selected_level=0;
        armed_=false;direction_held_=true;++revision;
    }
    void sample(const VrControls& input,bool focused) {
        const bool confirm=input.fire || input.menu;
        if(!focused) {armed_=false;direction_held_=true;menu_stick_.reset();return;}
        const auto cardinal=menu_stick_.sample(input.steer.x,input.steer.y);
        const int direction=cardinal==starfox::input::up?-1:cardinal==starfox::input::down?1:0;
        if(!confirm) armed_=true;
        if(!direction) direction_held_=false;
        if(!open) return;
        if(direction && !direction_held_) {
            const unsigned rows=row_count();
            selection=(selection+rows+direction)%rows;direction_held_=true;++revision;
        }
        if(confirm && armed_) {
            armed_=false;
            if(page==Page::main) {
                if(selection==0 && alternate_available && !runtime) {extended=!extended;selected_level=0;}
                else if(selection==1) unlocked_pace=!unlocked_pace;
                else if(selection==2 && msu_available) msu_music=!msu_music;
                else if(selection==3) {page=Page::options;selection=0;}
                else if(selection==4) open=false;
                else if(selection==5) preview=!preview;
            } else if(page==Page::options) {
                if(selection==0) {page=Page::cheats;selection=0;}
                else if(selection==1) crosshair_colour=(crosshair_colour+1)%8;
                else if(selection==2) swap_face_buttons=!swap_face_buttons;
                else if(selection==3) music_volume=(music_volume+10)%110;
                else if(selection==4) sfx_volume=(sfx_volume+10)%110;
                else if(selection==5) language=(language+1)%6;
                else if(selection==6) {page=Page::three_d;selection=0;}
                else if(selection==7) {page=Page::two_d;selection=0;}
                else if(selection==8) steer_sensitivity_index=(steer_sensitivity_index+1)%steer_sensitivities.size();
                else {page=Page::main;selection=3;}
            } else if(page==Page::three_d) {
                if(selection==0) model_effect=next_style(model_effect);
                else if(selection==1) model_intensity=(model_intensity+25)%125;
                else if(selection==2) preview=!preview;
                else if(selection==3 && ray_tracing_available) {ray_tracing=!ray_tracing;}
                else {page=Page::options;selection=6;}
            } else if(page==Page::two_d) {
                if(selection==0) world_effect=next_style(world_effect,true);
                else if(selection==1) world_intensity=(world_intensity+25)%125;
                else if(selection==2) preview=!preview;
                else if(selection==3) enhanced_sky=!enhanced_sky;
                else {page=Page::options;selection=7;}
            } else {
                if(selection==0) god_mode=!god_mode;
                else if(selection==1) {
                    const auto& choices=level_choices[extended?1:0];
                    if(!choices.empty()) {
                        size_t index=0;
                        for(size_t i=0;i<choices.size();++i) if(choices[i]==selected_level) {index=i;break;}
                        selected_level=choices[(index+1)%choices.size()];
                    }
                } else if(selection==2) default_laser=(default_laser+1)%3;
                else if(selection==3) infinite_bombs=!infinite_bombs;
                else if(selection==4) infinite_boost=!infinite_boost;
                else if(selection==5) infinite_lives=!infinite_lives;
                else {page=Page::options;selection=0;}
            }
            ++revision;
        }
    }
    std::string title() const {return page==Page::cheats?"CHEATS":page==Page::options?"OPTIONS":page==Page::three_d?"3D OPTIONS":page==Page::two_d?"2D OPTIONS":"STAR FOX ENHANCED";}
    std::vector<std::string> labels() const {
        if(page==Page::three_d || page==Page::two_d) {
            const bool models=page==Page::three_d;
            std::vector<std::string> rows{
                std::string(models?"MODEL EFFECTS: ":"WORLD EFFECTS: ")+std::string(style_name(models?model_effect:world_effect)),
                std::string(models?"MODEL EFFECT INTENSITY: ":"WORLD EFFECT INTENSITY: ")+std::to_string(models?model_intensity:world_intensity)+"%"};
            rows.push_back(std::string("PREVIEW: ")+(preview?"ON":"OFF"));
            if(!models) rows.push_back(std::string("ENHANCED SKY: ")+(enhanced_sky?"ON":"OFF"));
            if(models && ray_tracing_available) rows.push_back(std::string("RAY TRACING: ")+(ray_tracing?"ON":"OFF"));
            rows.push_back("BACK");return rows;
        }
        if(page==Page::cheats) return {std::string("GOD MODE: ")+(god_mode?"ON":"OFF"),
            "LEVEL SELECT: "+level_name(),
            std::string("DEFAULT LASER: ")+(default_laser==2?"BEAM":default_laser==1?"DUAL":"SINGLE"),
            std::string("INFINITE BOMBS: ")+(infinite_bombs?"ON":"OFF"),
            std::string("INFINITE BOOST: ")+(infinite_boost?"ON":"OFF"),
            std::string("INFINITE LIVES: ")+(infinite_lives?"ON":"OFF"),"BACK"};
        constexpr const char* languages[]{"ENGLISH","JAPANESE","GERMAN","FRENCH","SPANISH","ENGLISH (EUROPE)"};
        constexpr const char* colours[]{"GREEN","WHITE","BLUE","RED","YELLOW","CYAN","MAGENTA","ORANGE"};
        if(page==Page::options) return {"CHEATS",std::string("CROSSHAIR COLOR: ")+colours[crosshair_colour%8],
            std::string("SWAP A/B + Y/X: ")+(swap_face_buttons?"ON":"OFF"),
            "MUSIC VOLUME: "+std::to_string(music_volume)+"%","SFX VOLUME: "+std::to_string(sfx_volume)+"%",
            std::string("LANGUAGE: ")+languages[language<6?language:0],"3D OPTIONS","2D OPTIONS",
            "STICK SENSITIVITY: "+std::to_string(steer_sensitivities[steer_sensitivity_index%steer_sensitivities.size()])+"%","BACK"};
        return {std::string("EXPERIENCE: ")+(extended?"STARFOX EX":"ORIGINAL")+(runtime?" (LOCKED)":alternate_available?"":" (ONLY)"),
            std::string("PACE/SPEED: ")+(unlocked_pace?"UNLOCKED 20 HZ":"ORIGINAL"),
            std::string("MSU-1 MUSIC: ")+(msu_available?(msu_music?"ON":"OFF"):"NOT FOUND"),
            "OPTIONS",runtime?"RESUME":"START GAME",std::string("PREVIEW: ")+(preview?"ON":"OFF")};
    }
    std::array<std::u32string_view,2> localized_help() const {
        constexpr std::array<std::array<std::u32string_view,2>,6> help{{
            {U"STICK: MOVE",U"FIRE: SELECT"},{U"スティック: 移動",U"ショット: 決定"},
            {U"STICK: BEWEGEN",U"FEUER: AUSWÄHLEN"},{U"STICK : DÉPLACER",U"TIR : VALIDER"},
            {U"PALANCA: MOVER",U"DISPARO: ELEGIR"},{U"STICK: MOVE",U"FIRE: SELECT"}}};
        return help[language<6?language:0];
    }
    std::u32string translate(std::string_view key) const {
        const auto translated=localization::menu_translation(key,language);
        return translated.empty()?std::u32string(key.begin(),key.end()):std::u32string(translated);
    }
    std::vector<std::u32string> localized_labels() const {
        std::vector<std::u32string> result;
        for(const auto& label:labels()) {
            const auto colon=label.find(": ");
            if(colon==std::string::npos) {result.push_back(translate(label));continue;}
            const auto key=label.substr(0,colon),value=label.substr(colon+2);
            if(key=="LANGUAGE") {
                constexpr const char32_t* names[]{U"ENGLISH",U"日本語",U"DEUTSCH",U"FRANÇAIS",U"ESPAÑOL",U"ENGLISH (EUROPE)"};
                result.push_back(translate(key)+U": "+names[language<6?language:0]);
            } else {
                const auto suffix=value.find(" (");
                auto translated=translate(suffix==std::string::npos?value:value.substr(0,suffix));
                if(suffix!=std::string::npos) translated+=U" ("+translate(runtime?"LOCKED":"ONLY")+U")";
                result.push_back(translate(key)+U": "+translated);
            }
        }
        return result;
    }
private:
    MenuStick menu_stick_;
    bool armed_{},direction_held_{true};
};
}
