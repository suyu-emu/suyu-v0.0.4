// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/audio/audrec_u.h"

namespace Service::Audio {

class IFinalOutputRecorder final : public ServiceFramework<IFinalOutputRecorder> {
public:
    IFinalOutputRecorder() : ServiceFramework("IFinalOutputRecorder") {
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
    ~IFinalOutputRecorder() = default;
};

AudRecU::AudRecU() : ServiceFramework("audrec:u") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "OpenFinalOutputRecorder"},
    };
    RegisterHandlers(functions);
}

AudRecU::~AudRecU() = default;

} // namespace Service::Audio
