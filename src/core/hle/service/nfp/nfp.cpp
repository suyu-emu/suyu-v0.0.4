// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <atomic>

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/nfp/nfp.h"
#include "core/hle/service/nfp/nfp_user.h"

namespace Service::NFP {
namespace ErrCodes {
constexpr ResultCode ERR_NO_APPLICATION_AREA(ErrorModule::NFP, 152);
} // namespace ErrCodes

IUser::IUser(Module::Interface& nfp_interface_, Core::System& system_)
    : ServiceFramework{system_, "NFP::IUser"}, nfp_interface{nfp_interface_},
      deactivate_event{system.Kernel()}, availability_change_event{system.Kernel()} {
    static const FunctionInfo functions[] = {
        {0, &IUser::Initialize, "Initialize"},
        {1, &IUser::Finalize, "Finalize"},
        {2, &IUser::ListDevices, "ListDevices"},
        {3, &IUser::StartDetection, "StartDetection"},
        {4, &IUser::StopDetection, "StopDetection"},
        {5, &IUser::Mount, "Mount"},
        {6, &IUser::Unmount, "Unmount"},
        {7, &IUser::OpenApplicationArea, "OpenApplicationArea"},
        {8, &IUser::GetApplicationArea, "GetApplicationArea"},
        {9, nullptr, "SetApplicationArea"},
        {10, nullptr, "Flush"},
        {11, nullptr, "Restore"},
        {12, nullptr, "CreateApplicationArea"},
        {13, &IUser::GetTagInfo, "GetTagInfo"},
        {14, &IUser::GetRegisterInfo, "GetRegisterInfo"},
        {15, &IUser::GetCommonInfo, "GetCommonInfo"},
        {16, &IUser::GetModelInfo, "GetModelInfo"},
        {17, &IUser::AttachActivateEvent, "AttachActivateEvent"},
        {18, &IUser::AttachDeactivateEvent, "AttachDeactivateEvent"},
        {19, &IUser::GetState, "GetState"},
        {20, &IUser::GetDeviceState, "GetDeviceState"},
        {21, &IUser::GetNpadId, "GetNpadId"},
        {22, &IUser::GetApplicationAreaSize, "GetApplicationAreaSize"},
        {23, &IUser::AttachAvailabilityChangeEvent, "AttachAvailabilityChangeEvent"},
        {24, nullptr, "RecreateApplicationArea"},
    };
    RegisterHandlers(functions);

    Kernel::KAutoObject::Create(std::addressof(deactivate_event));
    Kernel::KAutoObject::Create(std::addressof(availability_change_event));

    deactivate_event.Initialize("IUser:DeactivateEvent");
    availability_change_event.Initialize("IUser:AvailabilityChangeEvent");
}

void IUser::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFC, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0};
    rb.Push(ResultSuccess);

    state = State::Initialized;
}

void IUser::Finalize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    device_state = DeviceState::Finalized;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::ListDevices(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u32 array_size = rp.Pop<u32>();
    LOG_DEBUG(Service_NFP, "called, array_size={}", array_size);

    ctx.WriteBuffer(device_handle);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(1);
}

void IUser::StartDetection(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    if (device_state == DeviceState::Initialized || device_state == DeviceState::TagRemoved) {
        device_state = DeviceState::SearchingForTag;
    }
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::StopDetection(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    switch (device_state) {
    case DeviceState::TagFound:
    case DeviceState::TagMounted:
        deactivate_event.GetWritableEvent().Signal();
        device_state = DeviceState::Initialized;
        break;
    case DeviceState::SearchingForTag:
    case DeviceState::TagRemoved:
        device_state = DeviceState::Initialized;
        break;
    default:
        break;
    }
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::Mount(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    device_state = DeviceState::TagMounted;
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::Unmount(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    device_state = DeviceState::TagFound;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::OpenApplicationArea(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ErrCodes::ERR_NO_APPLICATION_AREA);
}

void IUser::GetApplicationArea(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");

    // TODO(ogniK): Pull application area from amiibo

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<u32>(0); // This is from the GetCommonInfo stub
}

void IUser::GetTagInfo(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    const auto& amiibo = nfp_interface.GetAmiiboBuffer();
    const TagInfo tag_info{
        .uuid = amiibo.uuid,
        .uuid_length = static_cast<u8>(amiibo.uuid.size()),
        .protocol = 1, // TODO(ogniK): Figure out actual values
        .tag_type = 2,
    };
    ctx.WriteBuffer(tag_info);
    rb.Push(ResultSuccess);
}

void IUser::GetRegisterInfo(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");

    // TODO(ogniK): Pull Mii and owner data from amiibo

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::GetCommonInfo(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");

    // TODO(ogniK): Pull common information from amiibo

    CommonInfo common_info{};
    common_info.application_area_size = 0;
    ctx.WriteBuffer(common_info);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IUser::GetModelInfo(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    const auto& amiibo = nfp_interface.GetAmiiboBuffer();
    ctx.WriteBuffer(amiibo.model_info);
    rb.Push(ResultSuccess);
}

void IUser::AttachActivateEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 dev_handle = rp.Pop<u64>();
    LOG_DEBUG(Service_NFP, "called, dev_handle=0x{:X}", dev_handle);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(nfp_interface.GetNFCEvent());
    has_attached_handle = true;
}

void IUser::AttachDeactivateEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 dev_handle = rp.Pop<u64>();
    LOG_DEBUG(Service_NFP, "called, dev_handle=0x{:X}", dev_handle);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(deactivate_event.GetReadableEvent());
}

void IUser::GetState(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFC, "called");

    IPC::ResponseBuilder rb{ctx, 3, 0};
    rb.Push(ResultSuccess);
    rb.PushRaw<u32>(static_cast<u32>(state));
}

void IUser::GetDeviceState(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(device_state));
}

void IUser::GetNpadId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 dev_handle = rp.Pop<u64>();
    LOG_DEBUG(Service_NFP, "called, dev_handle=0x{:X}", dev_handle);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(npad_id);
}

void IUser::GetApplicationAreaSize(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");
    // We don't need to worry about this since we can just open the file
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<u32>(0); // This is from the GetCommonInfo stub
}

void IUser::AttachAvailabilityChangeEvent(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NFP, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(availability_change_event.GetReadableEvent());
}

Module::Interface::Interface(std::shared_ptr<Module> module_, Core::System& system_,
                             const char* name)
    : ServiceFramework{system_, name}, nfc_tag_load{system.Kernel()}, module{std::move(module_)} {
    Kernel::KAutoObject::Create(std::addressof(nfc_tag_load));
    nfc_tag_load.Initialize("IUser:NFCTagDetected");
}

Module::Interface::~Interface() = default;

void Module::Interface::CreateUserInterface(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IUser>(*this, system, service_context);
}

bool Module::Interface::LoadAmiibo(const std::vector<u8>& buffer) {
    if (buffer.size() < sizeof(AmiiboFile)) {
        return false;
    }

    std::memcpy(&amiibo, buffer.data(), sizeof(amiibo));
    nfc_tag_load->GetWritableEvent().Signal();
    return true;
}

Kernel::KReadableEvent& Module::Interface::GetNFCEvent() {
    return nfc_tag_load->GetReadableEvent();
}

const Module::Interface::AmiiboFile& Module::Interface::GetAmiiboBuffer() const {
    return amiibo;
}

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system) {
    auto module = std::make_shared<Module>();
    std::make_shared<NFP_User>(module, system)->InstallAsService(service_manager);
}

} // namespace Service::NFP
