#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/assets/decrunch.hpp"
#include <filesystem>
#include <bit>
#include <charconv>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <map>

int main(int argc,char** argv) try {
    if(argc!=5 && argc!=6) {std::cerr<<"Usage: starfox_background_transition_check ROM SYMBOLS LEVEL TICKS(1..8000) [NEW_CAPTURE_DIRECTORY]\n";return 1;}
    const std::filesystem::path capture_directory=argc==6?argv[5]:"";
    if(!capture_directory.empty() && std::filesystem::exists(capture_directory))
        throw std::runtime_error("Capture directory must be new; refusing to overwrite artifacts");
    unsigned ticks=0;const std::string_view count=argv[4];
    const auto parsed=std::from_chars(count.data(),count.data()+count.size(),ticks);
    if(parsed.ec!=std::errc{} || parsed.ptr!=count.data()+count.size() || ticks<1 || ticks>8000)
        throw std::runtime_error("Invalid bounded tick count");
    const std::string_view level=argv[3];
    const bool normal=level.size()==8 && level.starts_with("LEVEL") && level[5]>='1' && level[5]<='7'
        && level[6]=='_' && level[7]>='1' && level[7]<='9';
    if(!normal && level!="LEVEL_BLACKHOLE" && level!="LEVEL_SPECIAL" && level!="LEVEL_COMET")
        throw std::runtime_error("Expected a playable level entry symbol");
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    if(symbols.find(argv[3]).empty()) throw std::runtime_error("Unknown level symbol");
    starfox::simulation::GameSimulation game(rom,symbols,argv[3],{},true);
    game.set_experience(game.peek_meter_state().extended?starfox::simulation::Experience::starfox_ex
        :starfox::simulation::Experience::original);
    game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
    game.set_god_mode(true);game.set_msu1_available(false);game.set_msu1_music(false);
    starfox::audio::Spc700Audio audio;
    std::map<unsigned,std::string> background_names;
    const auto& bases=symbols.find("BGLISTS");
    if(!bases.empty()) for(const auto& [name,addresses]:symbols.entries()) {
        if(!name.starts_with("BG_") || addresses.empty() || addresses.front()<bases.front()
            || (addresses.front()&0xff0000U)!=(bases.front()&0xff0000U)) continue;
        const auto offset=addresses.front()-bases.front();
        const auto found=background_names.find(offset);
        if(found==background_names.end() || name<found->second) background_names[offset]=name;
    }
    using Key=std::tuple<unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,bool,bool,bool>;
    std::optional<Key> previous;unsigned changes=0;
    std::optional<std::pair<int,int>> previous_orbital_scroll;
    std::cout<<"level="<<argv[3]<<" ticks="<<ticks<<" god_mode=1 input=none timing=unlocked_20_fps msu=0\n";
    for(unsigned tick=0;tick<=ticks;++tick) {
        const auto& ppu=game.map().ppu_state();
        if(const auto name=background_names.find(game.map().background());name!=background_names.end()
            && (name->second=="BG_5_1I" || name->second=="BG_5_1E")) {
            if(ppu.bg2_scanline_scroll_enabled)
                throw std::runtime_error("Orbital planet horizon inherited checkerboard tunnel scanlines");
        }
        if(const auto name=background_names.find(game.map().background());name!=background_names.end()
            && (name->second=="BG_5_1I" || name->second=="BG_5_1E")) {
            const auto override=game.map().peek_background_scroll_override();
            const std::pair<int,int> scroll{override?(*override)[0]:ppu.bg2_scroll_x,
                override?(*override)[1]:ppu.bg2_scroll_y};
            if(!previous_orbital_scroll || scroll!=*previous_orbital_scroll) {
                std::cout<<"orbital tick="<<tick<<" scroll="<<scroll.first<<','<<scroll.second<<'\n';
                previous_orbital_scroll=scroll;
            }
        }
        const Key key{unsigned(game.flow_state()),game.map().background(),ppu.background_mode,
            ppu.bg2_screen_base,ppu.bg2_character_base,ppu.bg2_screen_size,ppu.tunnel_scene,ppu.bg2_scanline_scroll_enabled,
            game.map().background_request_pending()};
        if(!previous || key!=*previous || tick==ticks) {
            const auto [flow,bg,mode,map,chr,size,tunnel,scanlines,pending]=key;
            const auto name=background_names.find(bg);
            std::cout<<"tick="<<tick<<" flow="<<flow<<" bg="<<bg<<" mode="<<mode
                <<" bg2_map="<<map<<" bg2_chr="<<chr<<" bg2_size="<<size
                <<" tunnel="<<tunnel<<" scanlines="<<scanlines
                <<" request_pending="<<pending
                <<" name="<<(name==background_names.end()?"unknown":name->second)<<'\n';
            if(previous && key!=*previous) ++changes;
            previous=key;
        }
        if(tick==ticks) {
            const auto selected_name=background_names.find(game.map().background());
            if(selected_name!=background_names.end() && selected_name->second=="BG_2_6A") {
                const auto chars=starfox::assets::decrunch_reverse(rom,symbols.find("BGCMCCR").at(0)).bytes;
                const auto map=starfox::assets::decrunch_reverse(rom,symbols.find("BGTSSPCR").at(0)).bytes;
                if(chars.size()>ppu.vram.size() || map.size()>ppu.vram.size() || map.size()%2)
                    throw std::runtime_error("Invalid colony archive dimensions");
                const auto char_address=symbols.find("VCHR_LOGBACK").at(0)*2U;
                const auto map_address=symbols.find("VSC_BASE2").at(0)*2U;
                const auto offset=symbols.find("SCR_OFFSET").at(0);
                std::size_t char_mismatch=0,map_mismatch=0;
                for(std::size_t i=0;i<chars.size();++i)
                    char_mismatch+=chars[i]!=ppu.vram[(char_address+i)&65535U];
                for(std::size_t i=0;i<map.size();i+=2) {
                    const auto expected=static_cast<std::uint16_t>((map[i]|(map[i+1]<<8U))+offset);
                    const auto actual=static_cast<std::uint16_t>(ppu.vram[(map_address+i)&65535U]
                        |(ppu.vram[(map_address+i+1)&65535U]<<8U));
                    map_mismatch+=actual!=expected;
                }
                std::cout<<"colony_archive chars="<<chars.size()<<" mismatched_bytes="<<char_mismatch
                    <<" map_words="<<map.size()/2<<" mismatched_words="<<map_mismatch<<'\n';
                const auto table=game.map().read_native_word(symbols.find("HDMABG2HOFS2").at(0));
                std::map<unsigned,unsigned> record_counts;
                std::size_t row_mismatch=0;
                for(unsigned row=0;row<224;++row) {
                    const auto address=0x7e0000U|static_cast<std::uint16_t>(table+row*3);
                    ++record_counts[game.map().read_native_byte(address)];
                    const auto value=std::bit_cast<std::int16_t>(game.map().read_native_word(address+1));
                    row_mismatch+=value!=ppu.bg2_horizontal_offsets[row];
                }
                std::cout<<"horizontal_hdma table="<<table<<" metadata_mismatches="<<row_mismatch<<" record_counts=";
                for(const auto& [value,count]:record_counts) std::cout<<value<<':'<<count<<',';
                std::cout<<'\n';
                const auto mode=game.map().read_native_word(symbols.find("HPOSJMP").at(0));
                const auto view=std::bit_cast<std::int16_t>(game.map().read_native_word(symbols.find("VIEWPOSX").at(0)));
                const auto mario_view=std::bit_cast<std::int16_t>(game.map().read_native_word(symbols.find("M_VIEWPOSX").at(0)));
                std::cout<<"horizontal_inputs mode="<<mode<<" view_x="<<view<<" mario_view_x="<<mario_view<<'\n';
                if(mode==symbols.find("TUNNEL2_HOF").at(0)) {
                    const auto floor_div=[](std::int64_t n,std::int64_t d){return n>=0?n/d:-((-n+d-1)/d);};
                    const auto gradient=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
                        std::int64_t(mario_view)*4+floor_div(mario_view,4)));
                    std::size_t mismatches=0;
                    for(unsigned i=0;i<112;++i) {
                        const auto expected=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
                            128+floor_div(std::int64_t(i+1)*gradient,256)));
                        mismatches+=ppu.bg2_horizontal_offsets[111-i]!=expected;
                        mismatches+=ppu.bg2_horizontal_offsets[112+i]!=expected;
                    }
                    std::cout<<"tunnel_gradient_reference_mismatches="<<mismatches<<'\n';
                    auto probe=game.restored_state(game.save_state());
                    const auto view_address=symbols.find("VIEWPOSX").at(0);
                    const auto routine=symbols.find("DO_HPOSITIONS_L").at(0);
                    const auto output=symbols.find("BG_SCROLLBUFFER").at(0);
                    std::size_t checked=0;
                    for(const int x:{-32768,-16385,-8193,-1025,-257,-65,-5,-4,-3,-1,
                        0,1,3,4,5,63,64,65,255,256,257,1023,8191,16383,32767}) {
                        probe->map().write_native_word(view_address,static_cast<std::uint16_t>(x));
                        starfox::simulation::Wdc65816Registers registers;registers.status=0x24;
                        probe->map().call_native_routine(routine,registers,1'000'000);
                        const auto step=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
                            std::int64_t(x)*4+floor_div(x,4)));
                        for(unsigned i=0;i<112;++i) {
                            const auto expected=static_cast<std::uint16_t>(128+floor_div(std::int64_t(i+1)*step,256));
                            for(const auto row:{111-i,112+i}) {
                                const auto address=output+row*3;
                                if(probe->map().read_native_byte(address)!=1
                                    || probe->map().read_native_word(address+1)!=expected)
                                    throw std::runtime_error("Tunnel gradient boundary mismatch: x="+std::to_string(x)
                                        +" row="+std::to_string(row));
                                ++checked;
                            }
                        }
                    }
                    std::cout<<"tunnel_gradient_boundary_rows_checked="<<checked<<'\n';
                }
            }
            if(!capture_directory.empty()) {
                std::filesystem::create_directories(capture_directory);
                const auto palette=starfox::render::decode_bgr555_palette(ppu.cgram);
                starfox::render::BackgroundRenderer renderer;
                const auto& scroll_x_symbols=symbols.find("BG2XSCROLL");
                const auto& scroll_y_symbols=symbols.find("BG2SCROLL");
                const auto requested=game.map().background_scroll_override();
                const auto host_x=requested?(*requested)[0]:scroll_x_symbols.empty()?ppu.bg2_scroll_x:
                    std::bit_cast<std::int16_t>(game.map().read_native_word(scroll_x_symbols.front()));
                const auto host_y=requested?(*requested)[1]:scroll_y_symbols.empty()?ppu.bg2_scroll_y:
                    std::bit_cast<std::int16_t>(game.map().read_native_word(scroll_y_symbols.front()));
                std::cout<<"scroll ppu="<<ppu.bg2_scroll_x<<','<<ppu.bg2_scroll_y
                    <<" requested="<<host_x<<','<<host_y<<" main_screen="<<unsigned(ppu.main_screen)<<'\n';
                std::vector<std::uint8_t> native_pixels;
                for(unsigned width:{256U,400U,800U}) {
                    starfox::render::Framebuffer frame(width,224);
                    renderer.draw_bg2(ppu,ppu.bg2_scroll_x,ppu.bg2_scroll_y,frame,
                        starfox::render::TilePriorityPass::all,int((width-256)/2),true);
                    if(width==256) {
                        native_pixels=frame.pixels();
                        std::cout<<"decoder_flags tile16="<<ppu.bg2_tile_size_16<<" mosaic="<<unsigned(ppu.mosaic)
                            <<" horizontal="<<ppu.bg2_horizontal_offsets_enabled<<" vertical="<<ppu.bg2_vertical_offsets_enabled<<'\n';
                        if(ppu.background_mode==1 && !ppu.bg2_tile_size_16 && (ppu.mosaic&2)==0) {
                            starfox::render::Framebuffer oracle(256,224);
                            const unsigned pages_wide=(ppu.bg2_screen_size&1)?2:1;
                            const unsigned map_width=pages_wide*256,map_height=(ppu.bg2_screen_size&2)?512:256;
                            const auto byte=[&](unsigned address){return ppu.vram[address&65535U];};
                            std::size_t differences=0;
                            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
                                const auto scroll_x=ppu.bg2_horizontal_offsets_enabled?ppu.bg2_horizontal_offsets[y]:ppu.bg2_scroll_x;
                                const unsigned sx=(x+scroll_x)&(map_width-1);
                                const auto scroll_y=ppu.bg2_scanline_scroll_enabled?ppu.bg2_scanline_scroll_y[y]:ppu.bg2_scroll_y;
                                const unsigned sy=(y+scroll_y)&(map_height-1);
                                const unsigned tx=sx/8,ty=sy/8;
                                const unsigned entry=((ty/32)*pages_wide+tx/32)*1024+(ty%32)*32+tx%32;
                                const unsigned address=(ppu.bg2_screen_base+entry)*2;
                                const unsigned tile=byte(address)|(byte(address+1)<<8);
                                const unsigned px=(tile&0x4000)?7-(sx%8):sx%8;
                                const unsigned py=(tile&0x8000)?7-(sy%8):sy%8;
                                const unsigned base=ppu.bg2_character_base*2+(tile&1023)*32+py*2;
                                unsigned colour=0;
                                for(unsigned plane=0;plane<4;++plane)
                                    colour|=((byte(base+(plane/2)*16+plane%2)>>(7-px))&1)<<plane;
                                const auto index=colour?((tile>>10)&7)*16+colour:0;
                                oracle.set(x,y,static_cast<std::uint8_t>(index));
                                differences+=index!=frame.get(x,y);
                            }
                            starfox::render::write_bmp(oracle,capture_directory/"direct-mode1-bg2-256.bmp",palette);
                            std::cout<<"direct_mode1_decoder_changed_pixels="<<differences<<'\n';
                        }
                    }
                    else {
                        std::size_t changed=0;
                        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
                            changed+=frame.get(x+(width-256)/2,y)!=native_pixels[y*256+x];
                        std::cout<<"native_centre_changed_pixels width="<<width<<" count="<<changed<<'\n';
                    }
                    const auto path=capture_directory/(std::string(argv[3])+"-tick-"+std::to_string(tick)
                        +"-ppu-bg2-"+std::to_string(width)+".bmp");
                    starfox::render::write_bmp(frame,path,palette);
                    std::cout<<"isolated_ppu_bg2="<<path.string()<<'\n';
                    frame.clear();
                    renderer.draw_bg3(ppu,frame,starfox::render::TilePriorityPass::all,int((width-256)/2),true);
                    starfox::render::write_bmp(frame,capture_directory/("bg3-"+std::to_string(width)+".bmp"),palette);
                    frame.clear();
                    renderer.draw_bg2(ppu,host_x,host_y,frame,
                        starfox::render::TilePriorityPass::all,int((width-256)/2),true);
                    starfox::render::write_bmp(frame,capture_directory/("requested-scroll-bg2-"+std::to_string(width)+".bmp"),palette);
                }
                std::cout<<"Isolated BG2 (PPU/requested scroll) and BG3 captures: no layer composition, models, brightness or scroll interpolation.\n";
            }
            break;
        }
        const auto result=game.tick({});
        static_cast<void>(audio.render_logic_tick(result.audio_port_writes));
        game.synchronize_apu_output_ports(audio.output_ports());
        static_cast<void>(game.map().take_msu_register_writes());
    }
    std::cout<<"changes="<<changes<<"; state transitions only, not visual or completed-route proof\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
