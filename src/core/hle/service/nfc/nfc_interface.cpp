// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hid/hid_types.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nfc/nfc_device.h"
#include "core/hle/service/nfc/nfc_interface.h"
#include "core/hle/service/nfc/nfc_result.h"
#include "core/hle/service/time/clock_types.h"

namespace Service::NFC {

Interface::Interface(Core::System& system_, const char* name)
    : ServiceFramework{system_, name}, service_context{system_, service_name} {
    availability_change_event = service_context.CreateEvent("Interface:AvailabilityChangeEvent");

    for (u32 device_index = 0; device_index < 10; device_index++) {
        devices[device_index] =
            std::make_shared<NfcDevice>(Core::HID::IndexToNpadIdType(device_index), system,
                                        service_context, availability_change_event);
    }
}

Interface ::~Interface() {
    availability_change_event->Close();
}

void Interface::Initialize(HLERequestContext& ctx) {
    LOG_INFO(Service_NFC, "called");

    state = State::Initialized;

    for (auto& device : devices) {
        device->Initialize();
    }

    IPC::ResponseBuilder rb{ctx, 2, 0};
    rb.Push(ResultSuccess);
}

void Interface::Finalize(HLERequestContext& ctx) {
    LOG_INFO(Service_NFC, "called");

    state = State::NonInitialized;

    for (auto& device : devices) {
        device->Finalize();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Interface::GetState(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFC, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(state);
}

void Interface::IsNfcEnabled(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFC, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(state != State::NonInitialized);
}

void Interface::ListDevices(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NFC, "called");

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

    for (auto& device : devices) {
        if (nfp_devices.size() >= max_allowed_devices) {
            continue;
        }
        if (device->GetCurrentState() != NFP::DeviceState::Unavailable) {
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

void Interface::GetDeviceState(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFC, "called, device_handle={}", device_handle);

    auto device = GetNfcDevice(device_handle);

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
    LOG_DEBUG(Service_NFC, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(device.value()->GetNpadId());
}

void Interface::AttachAvailabilityChangeEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_NFC, "called");

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(availability_change_event->GetReadableEvent());
}

void Interface::StartDetection(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto nfp_protocol{rp.PopEnum<NFP::TagProtocol>()};
    LOG_INFO(Service_NFC, "called, device_handle={}, nfp_protocol={}", device_handle, nfp_protocol);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

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
    LOG_INFO(Service_NFC, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    const auto result = device.value()->StopDetection();
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::GetTagInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_INFO(Service_NFC, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    NFP::TagInfo tag_info{};
    const auto result = device.value()->GetTagInfo(tag_info, false);
    ctx.WriteBuffer(tag_info);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void Interface::AttachActivateEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    LOG_DEBUG(Service_NFC, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

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
    LOG_DEBUG(Service_NFC, "called, device_handle={}", device_handle);

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(device.value()->GetDeactivateEvent());
}

void Interface::SendCommandByPassThrough(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto device_handle{rp.Pop<u64>()};
    const auto timeout{rp.PopRaw<Time::Clock::TimeSpanType>()};
    const auto command_data{ctx.ReadBuffer()};

    LOG_INFO(Service_NFC, "(STUBBED) called, device_handle={}, timeout={}, data_size={}",
             device_handle, timeout.ToSeconds(), command_data.size());

    if (state == State::NonInitialized) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(NfcDisabled);
        return;
    }

    auto device = GetNfcDevice(device_handle);

    if (!device.has_value()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(DeviceNotFound);
        return;
    }

    std::vector<u8> out_data(1);
    // TODO: Request data from nfc device
    ctx.WriteBuffer(out_data);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(out_data.size()));
}

std::optional<std::shared_ptr<NfcDevice>> Interface::GetNfcDevice(u64 handle) {
    for (auto& device : devices) {
        if (device->GetHandle() == handle) {
            return device;
        }
    }
    return std::nullopt;
}

} // namespace Service::NFC
