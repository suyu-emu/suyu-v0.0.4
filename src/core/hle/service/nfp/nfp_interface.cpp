// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hid/hid_types.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nfp/nfp_device.h"
#include "core/hle/service/nfp/nfp_interface.h"
#include "core/hle/service/nfp/nfp_result.h"

namespace Service::NFP {

Interface::Interface(Core::System& system_, const char* name)
    : ServiceFramework{system_, name}, service_context{system_, service_name} {
    availability_change_event = service_context.CreateEvent("IUser:AvailabilityChangeEvent");

    for (u32 device_index = 0; device_index < 10; device_index++) {
        devices[device_index] =
            std::make_shared<NfpDevice>(Core::HID::IndexToNpadIdType(device_index), system,
                                        service_context, availability_change_event);
    }
}

Interface::~Interface() {
    availability_change_event->Close();
}

void Interface::Initialize(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::Initialized;

    for (auto& device : devices) {
        device->Initialize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::InitializeSystem(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::Initialized;

    for (auto& device : devices) {
        device->Initialize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::InitializeDebug(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::Initialized;

    for (auto& device : devices) {
        device->Initialize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::Finalize(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::NonInitialized;

    for (auto& device : devices) {
        device->Finalize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::FinalizeSystem(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::NonInitialized;

    for (auto& device : devices) {
        device->Finalize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::FinalizeDebug(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    state = State::NonInitialized;

    for (auto& device : devices) {
        device->Finalize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::ListDevices(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    if (!ctx.CanWriteBuffer()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(InvalidArgument);
        return;
    }

    if (ctx.GetWriteBufferSize() == 0) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(InvalidArgument);
        return;
    }

    std::vector<u64> nfp_devices;
    const std::size_t max_allowed_devices = ctx.GetWriteBufferNumElements<u64>();

    for (const auto& device : devices) {
        if (nfp_devices.size() >= max_allowed_devices) {
            continue;
        }
        if (device->GetCurrentState() != DeviceState::Unavailable) {
            nfp_devices.push_back(device->GetHandle());
        }
    }

    if (nfp_devices.empty()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    ctx.WriteBuffer(nfp_devices);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<s32>(nfp_devices.size()));
}

void Interface::StartDetection(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto nfp_protocol{rp.PopEnum<TagProtocol>()};
    LOG_INFO(Service_NFP, "called, device_handle={}, nfp_protocol={}", device_handle, nfp_protocol);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->StartDetection(nfp_protocol);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::StopDetection(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->StopDetection();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::Mount(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto model_type{rp.PopEnum<ModelType>()};
    const auto mount_target{rp.PopEnum<MountTarget>()};
    LOG_INFO(Service_NFP, "called, device_handle={}, model_type={}, mount_target={}", device_handle,
             model_type, mount_target);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->Mount(mount_target);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::Unmount(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->Unmount();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::OpenApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto access_id{rp.Pop<u32>()};
    LOG_INFO(Service_NFP, "called, device_handle={}, access_id={}", device_handle, access_id);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->OpenApplicationArea(access_id);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto data_size = ctx.GetWriteBufferSize();
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    if (!ctx.CanWriteBuffer()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(InvalidArgument);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    std::vector<u8> data(data_size);
    const auto result = device.value()->GetApplicationArea(data);
    ctx.WriteBuffer(data);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(static_cast<u32>(data_size));
}

void Interface::SetApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto data{ctx.ReadBuffer()};
    LOG_INFO(Service_NFP, "called, device_handle={}, data_size={}", device_handle, data.size());

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    if (!ctx.CanReadBuffer()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(InvalidArgument);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->SetApplicationArea(data);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::Flush(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->Flush();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::Restore(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_WARNING(Service_NFP, "(STUBBED) called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->RestoreAmiibo();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::CreateApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto access_id{rp.Pop<u32>()};
    const auto data{ctx.ReadBuffer()};
    LOG_INFO(Service_NFP, "called, device_handle={}, data_size={}, access_id={}", device_handle,
             access_id, data.size());

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    if (!ctx.CanReadBuffer()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(InvalidArgument);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->CreateApplicationArea(access_id, data);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetTagInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    TagInfo tag_info{};
    const auto result = device.value()->GetTagInfo(tag_info);
    ctx.WriteBuffer(tag_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetRegisterInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    RegisterInfo register_info{};
    const auto result = device.value()->GetRegisterInfo(register_info);
    ctx.WriteBuffer(register_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetCommonInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    CommonInfo common_info{};
    const auto result = device.value()->GetCommonInfo(common_info);
    ctx.WriteBuffer(common_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetModelInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    ModelInfo model_info{};
    const auto result = device.value()->GetModelInfo(model_info);
    ctx.WriteBuffer(model_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::AttachActivateEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(device.value()->GetActivateEvent());
}

void Interface::AttachDeactivateEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(device.value()->GetDeactivateEvent());
}

void Interface::GetState(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFP, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(state);
}

void Interface::GetDeviceState(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(device.value()->GetCurrentState());
}

void Interface::GetNpadId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(device.value()->GetNpadId());
}

void Interface::GetApplicationAreaSize(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(device.value()->GetApplicationAreaSize());
}

void Interface::AttachAvailabilityChangeEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_NFP, "called");

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(availability_change_event->GetReadableEvent());
}

void Interface::RecreateApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto access_id{rp.Pop<u32>()};
    const auto data{ctx.ReadBuffer()};
    LOG_INFO(Service_NFP, "called, device_handle={}, data_size={}, access_id={}", device_handle,
             access_id, data.size());

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->RecreateApplicationArea(access_id, data);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::Format(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->Format();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetAdminInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    AdminInfo admin_info{};
    const auto result = device.value()->GetAdminInfo(admin_info);
    ctx.WriteBuffer(admin_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetRegisterInfoPrivate(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    RegisterInfoPrivate register_info{};
    const auto result = device.value()->GetRegisterInfoPrivate(register_info);
    ctx.WriteBuffer(register_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::SetRegisterInfoPrivate(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto buffer{ctx.ReadBuffer()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}, buffer_size={}", device_handle,
              buffer.size());

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->SetRegisterInfoPrivate({});
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::DeleteRegisterInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->DeleteRegisterInfo();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::DeleteApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->DeleteApplicationArea();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::ExistsApplicationArea(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    bool has_application_area = false;
    const auto result = device.value()->ExistApplicationArea(has_application_area);
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(has_application_area);
}

void Interface::GetAll(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    NfpData data{};
    const auto result = device.value()->GetAll(data);

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::SetAll(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto nfp_data{ctx.ReadBuffer()};

    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    NfpData data{};
    memcpy(&data, nfp_data.data(), sizeof(NfpData));

    const auto result = device.value()->SetAll(data);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::FlushDebug(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->FlushDebug();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::BreakTag(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto break_type{rp.PopEnum<BreakType>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}, break_type={}", device_handle, break_type);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->BreakTag(break_type);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::ReadBackupData(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->ReadBackupData();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::WriteBackupData(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->WriteBackupData();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::WriteNtf(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFP, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfpDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->WriteNtf();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

std::optional<std::shared_ptr<NfpDevice>> Interface::GetNfpDevice(u64 handle) {
    for (auto& device : devices) {
        if (device->GetHandle() == handle) {
            return device;
        }
    }
    return std::nullopt;
}

} // namespace Service::NFP
