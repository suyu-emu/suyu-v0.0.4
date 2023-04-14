// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <optional>

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::NFP {
class NfpDevice;

class Interface : public ServiceFramework<Interface> {
public:
    explicit Interface(Core::System& system_, const char* name);
    ~Interface() override;

    void Initialize(HLERequestContext& ctx);
    void InitializeSystem(HLERequestContext& ctx);
    void InitializeDebug(HLERequestContext& ctx);
    void Finalize(HLERequestContext& ctx);
    void FinalizeSystem(HLERequestContext& ctx);
    void FinalizeDebug(HLERequestContext& ctx);
    void ListDevices(HLERequestContext& ctx);
    void StartDetection(HLERequestContext& ctx);
    void StopDetection(HLERequestContext& ctx);
    void Mount(HLERequestContext& ctx);
    void Unmount(HLERequestContext& ctx);
    void OpenApplicationArea(HLERequestContext& ctx);
    void GetApplicationArea(HLERequestContext& ctx);
    void SetApplicationArea(HLERequestContext& ctx);
    void Flush(HLERequestContext& ctx);
    void Restore(HLERequestContext& ctx);
    void CreateApplicationArea(HLERequestContext& ctx);
    void GetTagInfo(HLERequestContext& ctx);
    void GetRegisterInfo(HLERequestContext& ctx);
    void GetCommonInfo(HLERequestContext& ctx);
    void GetModelInfo(HLERequestContext& ctx);
    void AttachActivateEvent(HLERequestContext& ctx);
    void AttachDeactivateEvent(HLERequestContext& ctx);
    void GetState(HLERequestContext& ctx);
    void GetDeviceState(HLERequestContext& ctx);
    void GetNpadId(HLERequestContext& ctx);
    void GetApplicationAreaSize(HLERequestContext& ctx);
    void AttachAvailabilityChangeEvent(HLERequestContext& ctx);
    void RecreateApplicationArea(HLERequestContext& ctx);
    void Format(HLERequestContext& ctx);
    void GetAdminInfo(HLERequestContext& ctx);
    void GetRegisterInfoPrivate(HLERequestContext& ctx);
    void SetRegisterInfoPrivate(HLERequestContext& ctx);
    void DeleteRegisterInfo(HLERequestContext& ctx);
    void DeleteApplicationArea(HLERequestContext& ctx);
    void ExistsApplicationArea(HLERequestContext& ctx);
    void GetAll(HLERequestContext& ctx);
    void SetAll(HLERequestContext& ctx);
    void FlushDebug(HLERequestContext& ctx);
    void BreakTag(HLERequestContext& ctx);
    void ReadBackupData(HLERequestContext& ctx);
    void WriteBackupData(HLERequestContext& ctx);
    void WriteNtf(HLERequestContext& ctx);

private:
    enum class State : u32 {
        NonInitialized,
        Initialized,
    };

    std::optional<std::shared_ptr<NfpDevice>> GetNfpDevice(u64 handle);

    KernelHelpers::ServiceContext service_context;

    std::array<std::shared_ptr<NfpDevice>, 10> devices{};

    State state{State::NonInitialized};
    Kernel::KEvent* availability_change_event;
};

} // namespace Service::NFP
