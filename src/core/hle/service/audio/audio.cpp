// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/audio/audio.h"
#include "core/hle/service/audio/audio_controller.h"
#include "core/hle/service/audio/audio_in_manager.h"
#include "core/hle/service/audio/audio_out_manager.h"
#include "core/hle/service/audio/audio_renderer_manager.h"
#include "core/hle/service/audio/final_output_recorder_manager.h"
#include "core/hle/service/audio/final_output_recorder_manager_for_applet.h"
#include "core/hle/service/audio/hardware_opus_decoder_manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Audio {

class IAudioOutManagerForApplet final : public ServiceFramework<IAudioOutManagerForApplet> {
public:
    explicit IAudioOutManagerForApplet(Core::System& system_)
        : ServiceFramework{system_, "audout:a"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
            {2, nullptr, "GetProcessMasterVolume"},
            {3, nullptr, "SetProcessMasterVolume"},
            {4, nullptr, "GetProcessRecordVolume"},
            {5, nullptr, "SetProcessRecordVolume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioSnoopManager final : public ServiceFramework<IAudioSnoopManager> {
public:
    explicit IAudioSnoopManager(Core::System& system_)
        : ServiceFramework{system_, "auddev"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetDspStatistics"},
            {1, nullptr, "GetAppletStateSummaries"},
            {2, nullptr, "SetDspStatisticsParameter"},
            {3, nullptr, "GetDspStatisticsParameter"},
            {6, nullptr, "GetDspUsage"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioInManagerForApplet final : public ServiceFramework<IAudioInManagerForApplet> {
public:
    explicit IAudioInManagerForApplet(Core::System& system_)
        : ServiceFramework{system_, "audin:a"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
            {2, nullptr, "GetProcessMasterVolume"},
            {3, nullptr, "SetProcessMasterVolume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioRendererManagerForApplet final : public ServiceFramework<IAudioRendererManagerForApplet> {
public:
    explicit IAudioRendererManagerForApplet(Core::System& system_)
        : ServiceFramework{system_, "audren:a"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
            {2, nullptr, "GetProcessMasterVolume"},
            {3, nullptr, "SetProcessMasterVolume"},
            {4, nullptr, "RegisterAppletResourceUserId"},
            {5, nullptr, "UnregisterAppletResourceUserId"},
            {6, nullptr, "GetProcessRecordVolume"},
            {7, nullptr, "SetProcessRecordVolume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioOutManagerForDebugger final : public ServiceFramework<IAudioOutManagerForDebugger> {
public:
    explicit IAudioOutManagerForDebugger(Core::System& system_)
        : ServiceFramework{system_, "audout:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioInManagerForDebugger final : public ServiceFramework<IAudioInManagerForDebugger> {
public:
    explicit IAudioInManagerForDebugger(Core::System& system_)
        : ServiceFramework{system_, "audin:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IFinalOutputRecorderManagerForDebugger final : public ServiceFramework<IFinalOutputRecorderManagerForDebugger> {
public:
    explicit IFinalOutputRecorderManagerForDebugger(Core::System& system_)
        : ServiceFramework{system_, "audrec:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioRendererManagerForDebugger final : public ServiceFramework<IAudioRendererManagerForDebugger> {
public:
    explicit IAudioRendererManagerForDebugger(Core::System& system_)
        : ServiceFramework{system_, "audren:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspend"},
            {1, nullptr, "RequestResume"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioSystemManagerForApplet final : public ServiceFramework<IAudioSystemManagerForApplet> {
public:
    explicit IAudioSystemManagerForApplet(Core::System& system_)
        : ServiceFramework{system_, "aud:a"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RegisterAppletResourceUserId"},
            {1, nullptr, "UnregisterAppletResourceUserId"},
            {2, nullptr, "RequestSuspendAudio"},
            {3, nullptr, "RequestResumeAudio"},
            {4, nullptr, "GetAudioOutputProcessMasterVolume"},
            {5, nullptr, "SetAudioOutputProcessMasterVolume"},
            {6, nullptr, "GetAudioInputProcessMasterVolume"},
            {7, nullptr, "SetAudioInputProcessMasterVolume"},
            {8, nullptr, "GetAudioOutputProcessRecordVolume"},
            {9, nullptr, "SetAudioOutputProcessRecordVolume"},
            {10, nullptr, "GetAppletStateSummaries"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class IAudioSystemManagerForDebugger final : public ServiceFramework<IAudioSystemManagerForDebugger> {
public:
    explicit IAudioSystemManagerForDebugger(Core::System& system_)
        : ServiceFramework{system_, "aud:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RequestSuspendAudioForDebug"},
            {1, nullptr, "RequestResumeAudioForDebug"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("aud:a", std::make_shared<IAudioSystemManagerForApplet>(system));
    server_manager->RegisterNamedService("aud:d", std::make_shared<IAudioSystemManagerForDebugger>(system));

    server_manager->RegisterNamedService("audout:d", std::make_shared<IAudioOutManagerForDebugger>(system));
    server_manager->RegisterNamedService("audin:d", std::make_shared<IAudioInManagerForDebugger>(system));
    server_manager->RegisterNamedService("audrec:d", std::make_shared<IFinalOutputRecorderManagerForDebugger>(system));
    server_manager->RegisterNamedService("audren:d", std::make_shared<IAudioInManager>(system));

    server_manager->RegisterNamedService("audin:u", std::make_shared<IAudioInManager>(system));
    server_manager->RegisterNamedService("audin:a", std::make_shared<IAudioInManagerForApplet>(system));
    server_manager->RegisterNamedService("audout:u", std::make_shared<IAudioOutManager>(system));
    server_manager->RegisterNamedService("audout:a", std::make_shared<IAudioOutManagerForApplet>(system));
    server_manager->RegisterNamedService("auddev", std::make_shared<IAudioSnoopManager>(system));
    // Depends on audout:u and audin:u on ctor!
    server_manager->RegisterNamedService("audctl", std::make_shared<IAudioController>(system));
    server_manager->RegisterNamedService("audrec:a", std::make_shared<IFinalOutputRecorderManagerForApplet>(system));
    server_manager->RegisterNamedService("audrec:u", std::make_shared<IFinalOutputRecorderManager>(system));
    server_manager->RegisterNamedService("audren:u", std::make_shared<IAudioRendererManager>(system));
    server_manager->RegisterNamedService("audren:a", std::make_shared<IAudioRendererManagerForApplet>(system));
    server_manager->RegisterNamedService("hwopus", std::make_shared<IHardwareOpusDecoderManager>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Audio
