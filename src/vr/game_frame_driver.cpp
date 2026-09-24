#include "starfox/vr/game_frame_driver.hpp"
#include <stdexcept>
namespace starfox::vr {
void GameFrameDriver::reset_for_scene_change() noexcept {
    previous_.reset();clock_.reset();input_.reset();fraction_=0;
    apu_.clear();msu_.clear();audio_phase_=0;
    if(scenes_) scenes_->reset_interpolation();
}
GameFrameDriver::GameFrameDriver(simulation::GameSimulation& game,AudioTick audio,GameSceneHistory* scenes)
    :game_(game),audio_(std::move(audio)),scenes_(scenes) {
    if(scenes_ && &scenes_->game()!=&game_)
        throw std::invalid_argument("VR scene history belongs to a different game");
}
GameFrameAdvance GameFrameDriver::advance(XrTime time,const VrControls& controls,bool focused) {
    if(failed_) throw std::runtime_error("VR game frame driver requires reconstruction after a failed tick");
    if(!audio_ || time<0) throw std::invalid_argument("Invalid VR frame time/audio callback");
    GameFrameAdvance result;
    // Pause source time when the application loses focus; do not turn the
    // headset pause into a burst of movement or buffered button presses.
    if(!focused) {previous_.reset();clock_.reset();input_.reset();fraction_=0;if(scenes_) scenes_->reset_interpolation();return result;}
    if(previous_ && time==*previous_) {result.duplicate=true;result.raster_fraction=fraction_;return result;}
    input_.sample(controls,game_.in_setup_menu()
        || game_.flow_state()==simulation::GameFlowState::ex_pregame_menu);
    if(!previous_ || time<*previous_) {previous_=time;clock_.reset();fraction_=0;if(scenes_) scenes_->reset_interpolation();return result;}
    const auto elapsed=std::chrono::nanoseconds(time-*previous_);previous_=time;
    const auto batch=clock_.advance(elapsed);
    result.time_clamped=batch.time_was_clamped;result.raster_fraction=batch.interpolation_alpha;
    fraction_=result.raster_fraction;
    try {
        for(unsigned phase=0;phase<batch.simulation_steps;++phase) {
            game_.present_frame();++result.video_phases;
            if(game_.logic_tick_ready()) {
                const bool options_open=game_.runtime_options_open();
                const auto tick=game_.tick(input_.consume());++result.logic_ticks;
                if(!options_open) apu_.insert(apu_.end(),tick.audio_port_writes.begin(),tick.audio_port_writes.end());
                auto writes=game_.map().take_msu_register_writes();
                if(!options_open) msu_.insert(msu_.end(),writes.begin(),writes.end());
                if(options_open && !game_.runtime_options_open()) input_.reset();
                if(scenes_) scenes_->capture();
            }
            if(!game_.runtime_options_open() && ++audio_phase_==3) {
                game_.synchronize_apu_output_ports(audio_(apu_,msu_));
                apu_.clear();msu_.clear();audio_phase_=0;++result.audio_blocks;
            }
        }
    } catch(...) {failed_=true;throw;}
    return result;
}
}
