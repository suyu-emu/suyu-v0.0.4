// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <vector>

#include <SDL3/SDL.h>

#include "audio_core/common/common.h"
#include "audio_core/sink/sdl3_sink.h"
#include "audio_core/sink/sink_stream.h"
#include "common/logging.h"
#include "common/scope_exit.h"
#include "core/core.h"

namespace AudioCore::Sink {

namespace {
SDL_AudioDeviceID FindAudioDeviceByName(const std::string& device_name, bool capture) {
    int device_count = 0;
    SDL_AudioDeviceID* devices = capture ? SDL_GetAudioRecordingDevices(&device_count)
                                         : SDL_GetAudioPlaybackDevices(&device_count);
    if (devices == nullptr) {
        return capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    }

    SDL_AudioDeviceID selected = capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                         : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    for (int i = 0; i < device_count; ++i) {
        const char* current_name = SDL_GetAudioDeviceName(devices[i]);
        if (current_name != nullptr && device_name == current_name) {
            selected = devices[i];
            break;
        }
    }
    SDL_free(devices);
    return selected;
}
} // Anonymous namespace

/**
 * SDL sink stream, responsible for sinking samples to hardware.
 */
class SDLSinkStream final : public SinkStream {
public:
    /**
     * Create a new sink stream.
     *
     * @param device_channels_ - Number of channels supported by the hardware.
     * @param system_channels_ - Number of channels the audio systems expect.
     * @param output_device    - Name of the output device to use for this stream.
     * @param input_device     - Name of the input device to use for this stream.
     * @param type_            - Type of this stream.
     * @param system_          - Core system.
     * @param event            - Event used only for audio renderer, signalled on buffer consume.
     */
    SDLSinkStream(u32 device_channels_, u32 system_channels_, const std::string& output_device,
                  const std::string& input_device, StreamType type_, Core::System& system_)
        : SinkStream{system_, type_} {
        system_channels = system_channels_;
        device_channels = device_channels_;

        SDL_AudioSpec spec{};
        spec.freq = TargetSampleRate;
        spec.channels = static_cast<u8>(device_channels);
        spec.format = SDL_AUDIO_S16;

        std::string device_name{output_device};
        bool capture{false};
        if (type == StreamType::In) {
            device_name = input_device;
            capture = true;
        }

        const SDL_AudioDeviceID audio_device =
            device_name.empty() ? (capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                           : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
                                : FindAudioDeviceByName(device_name, capture);

        stream = SDL_OpenAudioDeviceStream(audio_device, &spec, &SDLSinkStream::DataCallback,
                                           this);

        if (stream == nullptr) {
            LOG_CRITICAL(Audio_Sink, "Error opening SDL audio device: {}", SDL_GetError());
            return;
        }

        SDL_AudioSpec stream_in{};
        SDL_AudioSpec stream_out{};
        static_cast<void>(SDL_GetAudioStreamFormat(stream, &stream_in, &stream_out));

        LOG_INFO(Service_Audio,
                 "Opening SDL stream {} with: rate {} channels {} (system channels {}) "
                 " format {}",
                 static_cast<const void*>(stream), stream_out.freq, stream_out.channels,
                 system_channels, static_cast<int>(stream_out.format));
    }

    /**
     * Destroy the sink stream.
     */
    ~SDLSinkStream() override {
        LOG_DEBUG(Service_Audio, "Destructing SDL stream {}", name);
        Finalize();
    }

    /**
     * Finalize the sink stream.
     */
    void Finalize() override {
        if (stream == nullptr) {
            return;
        }

        Stop();
        SDL_ClearAudioStream(stream);
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }

    /**
     * Start the sink stream.
     *
     * @param resume - Set to true if this is resuming the stream a previously-active stream.
     *                 Default false.
     */
    void Start(bool resume = false) override {
        if (stream == nullptr || !paused) {
            return;
        }

        paused = false;
        static_cast<void>(SDL_ResumeAudioStreamDevice(stream));
    }

    /**
     * Stop the sink stream.
     */
    void Stop() override {
        if (stream == nullptr || paused) {
            return;
        }
        SignalPause();
        static_cast<void>(SDL_PauseAudioStreamDevice(stream));
    }

private:
    /**
     * Main callback from SDL. Either expects samples from us (audio render/audio out), or will
     * provide samples to be copied (audio in).
     *
     * @param userdata - Custom data pointer passed along, points to a SDLSinkStream.
     * @param stream   - Buffer of samples to be filled or read.
     * @param len      - Length of the stream in bytes.
     */
    static void DataCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                             int total_amount) {
        auto* impl = static_cast<SDLSinkStream*>(userdata);

        if (!impl) {
            return;
        }

        const std::size_t num_channels = impl->GetDeviceChannels();
        const std::size_t frame_size = num_channels;

        if (impl->type == StreamType::In) {
            const int bytes_available = SDL_GetAudioStreamAvailable(stream);
            if (bytes_available <= 0) {
                return;
            }

            std::vector<s16> input(bytes_available / static_cast<int>(sizeof(s16)));
            const int bytes_read = SDL_GetAudioStreamData(stream, input.data(), bytes_available);
            if (bytes_read <= 0) {
                return;
            }

            const std::size_t num_frames =
                static_cast<std::size_t>(bytes_read) / sizeof(s16) / frame_size;
            std::span<const s16> input_buffer{input.data(),
                                              static_cast<std::size_t>(bytes_read) / sizeof(s16)};
            impl->ProcessAudioIn(input_buffer, num_frames);
        } else {
            if (additional_amount <= 0 && total_amount <= 0) {
                return;
            }

            const int bytes_requested = additional_amount > 0 ? additional_amount : total_amount;
            std::vector<s16> output(bytes_requested / static_cast<int>(sizeof(s16)));
            const std::size_t num_frames =
                static_cast<std::size_t>(bytes_requested) / sizeof(s16) / frame_size;
            std::span<s16> output_buffer{output.data(), output.size()};
            impl->ProcessAudioOutAndRender(output_buffer, num_frames);
            static_cast<void>(SDL_PutAudioStreamData(stream, output.data(), bytes_requested));
        }
    }

    /// SDL stream attached to an opened input/output device
    SDL_AudioStream* stream{};
};

SDLSink::SDLSink(std::string_view target_device_name) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return;
        }
    }

    if (target_device_name != auto_device_name && !target_device_name.empty()) {
        output_device = target_device_name;
    } else {
        output_device.clear();
    }

    device_channels = 2;
}

SDLSink::~SDLSink() = default;

SinkStream* SDLSink::AcquireSinkStream(Core::System& system, u32 system_channels_,
                                       const std::string&, StreamType type) {
    system_channels = system_channels_;
    SinkStreamPtr& stream = sink_streams.emplace_back(std::make_unique<SDLSinkStream>(
        device_channels, system_channels, output_device, input_device, type, system));
    return stream.get();
}

void SDLSink::CloseStream(SinkStream* stream) {
    for (size_t i = 0; i < sink_streams.size(); i++) {
        if (sink_streams[i].get() == stream) {
            sink_streams[i].reset();
            sink_streams.erase(sink_streams.begin() + i);
            break;
        }
    }
}

void SDLSink::CloseStreams() {
    sink_streams.clear();
}

f32 SDLSink::GetDeviceVolume() const {
    if (sink_streams.empty()) {
        return 1.0f;
    }

    return sink_streams[0]->GetDeviceVolume();
}

void SDLSink::SetDeviceVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetDeviceVolume(volume);
    }
}

void SDLSink::SetSystemVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetSystemVolume(volume);
    }
}

std::vector<std::string> ListSDLSinkDevices(bool capture) {
    std::vector<std::string> device_list;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return {};
        }
    }

    int device_count = 0;
    SDL_AudioDeviceID* devices =
        capture ? SDL_GetAudioRecordingDevices(&device_count)
                : SDL_GetAudioPlaybackDevices(&device_count);
    if (devices == nullptr) {
        return device_list;
    }

    for (int i = 0; i < device_count; ++i) {
        if (const char* name = SDL_GetAudioDeviceName(devices[i])) {
            device_list.emplace_back(name);
        }
    }
    SDL_free(devices);

    return device_list;
}

/* REVERSION to 3833 - function GetSDLLatency() REINTRODUCED FROM 3833 - DIABLO 3 FIX */
u32 GetSDLLatency() {
    return TargetSampleCount * 2;
}

// REVERTED back to 3833 - Below function IsSDLSuitable() removed, reverting to GetSDLLatency() above. - DIABLO 3 FIX
/*
bool IsSDLSuitable() {
#if !defined(HAVE_SDL3)
    return false;
#else
    // Check SDL can init
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            LOG_ERROR(Audio_Sink, "SDL failed to init, it is not suitable. Error: {}",
                      SDL_GetError());
            return false;
        }
    }

    // We can set any latency frequency we want with SDL, so no need to check that.

    // Check we can open a device with standard parameters
    SDL_AudioSpec spec;
    spec.freq = TargetSampleRate;
    spec.channels = 2u;
    spec.format = AUDIO_S16SYS;
    spec.samples = TargetSampleCount * 2;
    spec.callback = nullptr;
    spec.userdata = nullptr;

    SDL_AudioSpec obtained;
    auto device = SDL_OpenAudioDevice(nullptr, false, &spec, &obtained, false);

    if (device == 0) {
        LOG_ERROR(Audio_Sink, "SDL failed to open a device, it is not suitable. Error: {}",
                  SDL_GetError());
        return false;
    }

    SDL_CloseAudioDevice(device);
    return true;
#endif
}
*/

} // namespace AudioCore::Sink
