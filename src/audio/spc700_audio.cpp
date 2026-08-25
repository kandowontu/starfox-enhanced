#include "starfox/audio/spc700_audio.hpp"

#include <spc.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace starfox::audio {
namespace {

constexpr std::size_t kSpcHeaderSize = 0x100U;
constexpr std::size_t kSpcRamSize = 0x10000U;
constexpr std::size_t kSpcRamOffset = kSpcHeaderSize;
constexpr std::size_t kSpcDspOffset = 0x10100U;
constexpr std::size_t kPcLowOffset = 0x25U;
constexpr std::size_t kPcHighOffset = 0x26U;
constexpr std::size_t kAccumulatorOffset = 0x27U;
constexpr std::size_t kXOffset = 0x28U;
constexpr std::size_t kYOffset = 0x29U;
constexpr std::size_t kPswOffset = 0x2aU;
constexpr std::size_t kStackOffset = 0x2bU;
constexpr std::size_t kSmpRegistersOffset = kSpcRamOffset + 0xf0U;
constexpr std::size_t kDspFlags = 0x6cU;
constexpr int kClocksPerLogicTick = spc_clock_rate / 20;
static_assert(kClocksPerLogicTick == 51'200);
static_assert(Spc700Audio::stereo_frames_per_logic_tick
              == static_cast<std::size_t>(spc_sample_rate / 20));

void throw_spc_error(const char* operation, spc_err_t error) {
    if (error != nullptr) {
        throw std::runtime_error{
            std::string{operation} + ": " + error};
    }
}

} // namespace

struct Spc700Audio::Impl {
    enum class UploadState {
        waiting_for_address_low,
        waiting_for_address_high,
        waiting_for_transfer_flag,
        waiting_for_token,
        waiting_for_data,
    };

    SNES_SPC* spc{spc_new()};
    SPC_Filter* filter{spc_filter_new()};
    std::array<std::uint8_t, kSpcRamSize> aram{};
    std::array<std::uint8_t, 4> cpu_ports{};
    UploadState upload_state{UploadState::waiting_for_address_low};
    std::uint16_t upload_address{};
    std::uint16_t execute_address{0x0400U};
    bool transfer_enabled{};
    bool uploading{true};
    bool loaded{};
    std::size_t upload_count{};

    Impl() {
        if (spc == nullptr || filter == nullptr) {
            if (filter != nullptr) spc_filter_delete(filter);
            if (spc != nullptr) spc_delete(spc);
            throw std::runtime_error{"could not allocate SPC700 audio emulator"};
        }
        aram.fill(0xffU);
        spc_filter_clear(filter);
    }

    ~Impl() {
        spc_filter_delete(filter);
        spc_delete(spc);
    }

    void begin_upload() noexcept {
        uploading = true;
        loaded = false;
        upload_state = UploadState::waiting_for_address_low;
        upload_address = 0;
        transfer_enabled = false;
        upload_count = 0;
        // Resetting the SPC into its IPL leaves ARAM intact. Level sound
        // banks are overlays on the base sound0 driver loaded during boot.
    }

    void load_driver() {
        std::array<std::uint8_t, spc_file_size> snapshot{};
        spc_init_header(snapshot.data());
        snapshot[kPcLowOffset] = static_cast<std::uint8_t>(execute_address);
        snapshot[kPcHighOffset] = static_cast<std::uint8_t>(execute_address >> 8U);

        // The IPL enters downloaded code with an empty accumulator/index
        // context and its reset stack. Star Fox's driver initializes all of
        // its own SMP and DSP state immediately from this entry point.
        snapshot[kAccumulatorOffset] = 0;
        snapshot[kXOffset] = 0;
        snapshot[kYOffset] = 0;
        snapshot[kPswOffset] = 0;
        snapshot[kStackOffset] = 0xefU;
        std::copy(aram.begin(), aram.end(), snapshot.begin() + kSpcRamOffset);

        // SPC files mirror the SMP I/O registers into RAM $f0-$ff. Disable
        // the absent IPL mapping and start from the hardware-reset DSP state;
        // the uploaded driver replaces these values during its prologue.
        std::fill(snapshot.begin() + kSmpRegistersOffset,
                  snapshot.begin() + kSmpRegistersOffset + 16U, 0U);
        snapshot[kSpcDspOffset + kDspFlags] = 0xe0U;
        throw_spc_error("spc_load_spc",
                        spc_load_spc(spc, snapshot.data(), snapshot.size()));
        spc_filter_clear(filter);
        loaded = true;
        uploading = false;
    }

    void consume_upload_write(const simulation::ApuPortWrite& write) {
        cpu_ports[write.port] = write.value;
        switch (upload_state) {
        case UploadState::waiting_for_address_low:
            if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        case UploadState::waiting_for_address_high:
            if (write.port == 3U) {
                upload_address = static_cast<std::uint16_t>(
                    upload_address | (static_cast<std::uint16_t>(write.value) << 8U));
                upload_state = UploadState::waiting_for_transfer_flag;
            } else if (write.port == 2U) {
                upload_address = write.value;
            }
            break;
        case UploadState::waiting_for_transfer_flag:
            if (write.port == 1U) {
                transfer_enabled = write.value != 0U;
                upload_state = UploadState::waiting_for_token;
            } else if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        case UploadState::waiting_for_token:
            if (write.port == 0U) {
                if (!transfer_enabled) {
                    execute_address = upload_address;
                    load_driver();
                } else {
                    upload_state = UploadState::waiting_for_data;
                }
            }
            break;
        case UploadState::waiting_for_data:
            if (write.port == 1U) {
                aram[upload_address++] = write.value;
                ++upload_count;
            } else if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        }
    }

    std::vector<std::int16_t> render(
        std::span<const simulation::ApuPortWrite> writes) {
        std::vector<std::int16_t> output(
            Spc700Audio::stereo_frames_per_logic_tick * 2U, 0);
        if (loaded) {
            spc_set_output(spc, output.data(), static_cast<int>(output.size()));
        }
        int last_clock = 0;

        // A fresh sound bank begins with the source driver's explicit $ff
        // restart command. The subsequent writes are another IPL upload.
        for (const auto& write : writes) {
            if (!uploading && write.port == 0U && write.value == 0xffU) {
                begin_upload();
            }
            const auto was_loaded = loaded;
            if (uploading) {
                consume_upload_write(write);
            } else {
                const auto clock = std::clamp(
                    static_cast<int>(write.clock_offset), last_clock,
                    kClocksPerLogicTick);
                spc_write_port(spc, clock, write.port, write.value);
                last_clock = clock;
            }
            if (!was_loaded && loaded) {
                // Loading an SPC state resets the library's output buffer.
                // Attach this tick's buffer immediately so initialization and
                // later timestamped port traffic are rendered, not discarded.
                spc_set_output(spc, output.data(), static_cast<int>(output.size()));
                last_clock = 0;
            }
        }

        if (!loaded) return output;
        spc_end_frame(spc, kClocksPerLogicTick);
        const auto generated = std::clamp(spc_sample_count(spc), 0,
                                          static_cast<int>(output.size()));
        if (generated < static_cast<int>(output.size())) {
            std::fill(output.begin() + generated, output.end(), 0);
        }
        spc_filter_run(filter, output.data(), static_cast<int>(output.size()));
        return output;
    }
};

Spc700Audio::Spc700Audio() : impl_(std::make_unique<Impl>()) {}
Spc700Audio::~Spc700Audio() = default;
Spc700Audio::Spc700Audio(Spc700Audio&&) noexcept = default;
Spc700Audio& Spc700Audio::operator=(Spc700Audio&&) noexcept = default;

std::vector<std::int16_t> Spc700Audio::render_logic_tick(
    std::span<const simulation::ApuPortWrite> writes) {
    return impl_->render(writes);
}

bool Spc700Audio::driver_loaded() const noexcept { return impl_->loaded; }
std::size_t Spc700Audio::uploaded_bytes() const noexcept {
    return impl_->upload_count;
}

Spc700Audio::State Spc700Audio::state() const {
    if (!impl_->loaded) return {};
    std::array<std::uint8_t, spc_file_size> snapshot{};
    spc_init_header(snapshot.data());
    spc_save_spc(impl_->spc, snapshot.data());
    return {
        static_cast<std::uint16_t>(snapshot[kPcLowOffset]
            | (static_cast<std::uint16_t>(snapshot[kPcHighOffset]) << 8U)),
        snapshot[kAccumulatorOffset],
        snapshot[kXOffset],
        snapshot[kYOffset],
        snapshot[kPswOffset],
        snapshot[kStackOffset],
        snapshot[kSpcDspOffset + kDspFlags],
        snapshot[kSpcDspOffset + 0x4cU],
        static_cast<std::int8_t>(snapshot[kSpcDspOffset + 0x0cU]),
        static_cast<std::int8_t>(snapshot[kSpcDspOffset + 0x1cU]),
    };
}

} // namespace starfox::audio
