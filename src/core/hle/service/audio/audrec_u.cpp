// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/audio/audrec_u.h"

namespace Service::Audio {

class IFinalOutputRecorder final : public ServiceFramework<IFinalOutputRecorder> {
public:
    explicit IFinalOutputRecorder(Core::System& system_)
        : ServiceFramework{system_, "IFinalOutputRecorder"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetFinalOutputRecorderState"},
            {1, nullptr, "StartFinalOutputRecorder"},
            {2, nullptr, "StopFinalOutputRecorder"},
            {3, nullptr, "AppendFinalOutputRecorderBuffer"},
            {4, nullptr, "RegisterBufferEvent"},
            {5, nullptr, "GetReleasedFinalOutputRecorderBuffer"},
            {6, nullptr, "ContainsFinalOutputRecorderBuffer"},
            {7, nullptr, "GetFinalOutputRecorderBufferEndTime"},
            {8, nullptr, "AppendFinalOutputRecorderBufferAuto"},
            {9, nullptr, "GetReleasedFinalOutputRecorderBufferAuto"},
            {10, nullptr, "FlushFinalOutputRecorderBuffers"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

AudRecU::AudRecU(Core::System& system_) : ServiceFramework{system_, "audrec:u"} {
    static const FunctionInfo functions[] = {
        {0, nullptr, "OpenFinalOutputRecorder"},
    };
    RegisterHandlers(functions);
}

AudRecU::~AudRecU() = default;

} // namespace Service::Audio
