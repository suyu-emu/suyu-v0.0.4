// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/nvdrv/interface.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvdrv/nvdrv.h"

namespace Service::Nvidia {

void NVDRV::SignalGPUInterruptSyncpt(const u32 syncpoint_id, const u32 value) {
    nvdrv->SignalSyncpt(syncpoint_id, value);
}

void NVDRV::Open(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NVDRV, "called");

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    const auto& buffer = ctx.ReadBuffer();
    const std::string device_name(buffer.begin(), buffer.end());
    DeviceFD fd = nvdrv->Open(device_name);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);
    rb.Push<DeviceFD>(fd);
    rb.PushEnum(fd != INVALID_NVDRV_FD ? NvResult::Success : NvResult::FileOperationFailed);
}

void NVDRV::ServiceError(Kernel::HLERequestContext& ctx, NvResult result) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(result);
}

void NVDRV::Ioctl1(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl=0x{:08X}", fd, command.raw);

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    // Check device
    std::vector<u8> output_buffer(ctx.GetWriteBufferSize(0));
    const auto input_buffer = ctx.ReadBuffer(0);

    IoctlCtrl ctrl{};

    const auto nv_result = nvdrv->Ioctl1(fd, command, input_buffer, output_buffer, ctrl);
    if (ctrl.must_delay) {
        ctrl.fresh_call = false;
        ctx.SleepClientThread(
            "NVServices::DelayedResponse", ctrl.timeout,
            [=, this](std::shared_ptr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx_,
                      Kernel::ThreadWakeupReason reason) {
                IoctlCtrl ctrl2{ctrl};
                std::vector<u8> tmp_output = output_buffer;
                const auto nv_result2 = nvdrv->Ioctl1(fd, command, input_buffer, tmp_output, ctrl2);

                if (command.is_out != 0) {
                    ctx.WriteBuffer(tmp_output);
                }

                IPC::ResponseBuilder rb{ctx_, 3};
                rb.Push(RESULT_SUCCESS);
                rb.PushEnum(nv_result2);
            },
            nvdrv->GetEventWriteable(ctrl.event_id));
    } else {
        if (command.is_out != 0) {
            ctx.WriteBuffer(output_buffer);
        }
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nv_result);
}

void NVDRV::Ioctl2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl=0x{:08X}", fd, command.raw);

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    const auto input_buffer = ctx.ReadBuffer(0);
    const auto input_inlined_buffer = ctx.ReadBuffer(1);
    std::vector<u8> output_buffer(ctx.GetWriteBufferSize(0));

    IoctlCtrl ctrl{};

    const auto nv_result =
        nvdrv->Ioctl2(fd, command, input_buffer, input_inlined_buffer, output_buffer, ctrl);
    if (ctrl.must_delay) {
        ctrl.fresh_call = false;
        ctx.SleepClientThread(
            "NVServices::DelayedResponse", ctrl.timeout,
            [=, this](std::shared_ptr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx_,
                      Kernel::ThreadWakeupReason reason) {
                IoctlCtrl ctrl2{ctrl};
                std::vector<u8> tmp_output = output_buffer;
                const auto nv_result2 = nvdrv->Ioctl2(fd, command, input_buffer,
                                                      input_inlined_buffer, tmp_output, ctrl2);

                if (command.is_out != 0) {
                    ctx.WriteBuffer(tmp_output);
                }

                IPC::ResponseBuilder rb{ctx_, 3};
                rb.Push(RESULT_SUCCESS);
                rb.PushEnum(nv_result2);
            },
            nvdrv->GetEventWriteable(ctrl.event_id));
    } else {
        if (command.is_out != 0) {
            ctx.WriteBuffer(output_buffer);
        }
    }

    if (command.is_out != 0) {
        ctx.WriteBuffer(output_buffer);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nv_result);
}

void NVDRV::Ioctl3(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl=0x{:08X}", fd, command.raw);

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    const auto input_buffer = ctx.ReadBuffer(0);
    std::vector<u8> output_buffer(ctx.GetWriteBufferSize(0));
    std::vector<u8> output_buffer_inline(ctx.GetWriteBufferSize(1));

    IoctlCtrl ctrl{};
    const auto nv_result =
        nvdrv->Ioctl3(fd, command, input_buffer, output_buffer, output_buffer_inline, ctrl);
    if (ctrl.must_delay) {
        ctrl.fresh_call = false;
        ctx.SleepClientThread(
            "NVServices::DelayedResponse", ctrl.timeout,
            [=, this](std::shared_ptr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx_,
                      Kernel::ThreadWakeupReason reason) {
                IoctlCtrl ctrl2{ctrl};
                std::vector<u8> tmp_output = output_buffer;
                std::vector<u8> tmp_output2 = output_buffer;
                const auto nv_result2 =
                    nvdrv->Ioctl3(fd, command, input_buffer, tmp_output, tmp_output2, ctrl2);

                if (command.is_out != 0) {
                    ctx.WriteBuffer(tmp_output, 0);
                    ctx.WriteBuffer(tmp_output2, 1);
                }

                IPC::ResponseBuilder rb{ctx_, 3};
                rb.Push(RESULT_SUCCESS);
                rb.PushEnum(nv_result2);
            },
            nvdrv->GetEventWriteable(ctrl.event_id));
    } else {
        if (command.is_out != 0) {
            ctx.WriteBuffer(output_buffer, 0);
            ctx.WriteBuffer(output_buffer_inline, 1);
        }
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(nv_result);
}

void NVDRV::Close(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NVDRV, "called");

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto result = nvdrv->Close(fd);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(result);
}

void NVDRV::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");

    is_initialized = true;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(NvResult::Success);
}

void NVDRV::QueryEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto event_id = rp.Pop<u32>() & 0x00FF;
    LOG_WARNING(Service_NVDRV, "(STUBBED) called, fd={:X}, event_id={:X}", fd, event_id);

    if (!is_initialized) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initalized!");
        return;
    }

    const auto nv_result = nvdrv->VerifyFD(fd);
    if (nv_result != NvResult::Success) {
        LOG_ERROR(Service_NVDRV, "Invalid FD specified DeviceFD={}!", fd);
        ServiceError(ctx, nv_result);
        return;
    }

    if (event_id < MaxNvEvents) {
        IPC::ResponseBuilder rb{ctx, 3, 1};
        rb.Push(RESULT_SUCCESS);
        auto event = nvdrv->GetEvent(event_id);
        event->Clear();
        rb.PushCopyObjects(event);
        rb.PushEnum(NvResult::Success);
    } else {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.PushEnum(NvResult::BadParameter);
    }
}

void NVDRV::SetAruid(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    pid = rp.Pop<u64>();
    LOG_WARNING(Service_NVDRV, "(STUBBED) called, pid=0x{:X}", pid);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(NvResult::Success);
}

void NVDRV::SetGraphicsFirmwareMemoryMarginEnabled(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void NVDRV::GetStatus(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(NvResult::Success);
}

void NVDRV::DumpGraphicsMemoryInfo(Kernel::HLERequestContext& ctx) {
    // According to SwitchBrew, this has no inputs and no outputs, so effectively does nothing on
    // retail hardware.
    LOG_DEBUG(Service_NVDRV, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

NVDRV::NVDRV(std::shared_ptr<Module> nvdrv, const char* name)
    : ServiceFramework(name), nvdrv(std::move(nvdrv)) {
    static const FunctionInfo functions[] = {
        {0, &NVDRV::Open, "Open"},
        {1, &NVDRV::Ioctl1, "Ioctl"},
        {2, &NVDRV::Close, "Close"},
        {3, &NVDRV::Initialize, "Initialize"},
        {4, &NVDRV::QueryEvent, "QueryEvent"},
        {5, nullptr, "MapSharedMem"},
        {6, &NVDRV::GetStatus, "GetStatus"},
        {7, nullptr, "SetAruidForTest"},
        {8, &NVDRV::SetAruid, "SetAruid"},
        {9, &NVDRV::DumpGraphicsMemoryInfo, "DumpGraphicsMemoryInfo"},
        {10, nullptr, "InitializeDevtools"},
        {11, &NVDRV::Ioctl2, "Ioctl2"},
        {12, &NVDRV::Ioctl3, "Ioctl3"},
        {13, &NVDRV::SetGraphicsFirmwareMemoryMarginEnabled,
         "SetGraphicsFirmwareMemoryMarginEnabled"},
    };
    RegisterHandlers(functions);
}

NVDRV::~NVDRV() = default;

} // namespace Service::Nvidia
