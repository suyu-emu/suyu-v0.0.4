// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/native_handle_holder.h"
#include "core/hle/service/olsc/stopper_object.h"
#include "core/hle/service/olsc/transfer_task_list_controller.h"

namespace Service::OLSC {

ITransferTaskListController::ITransferTaskListController(Core::System& system_)
    : ServiceFramework{system_, "ITransferTaskListController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetTransferTaskCountForOcean"},
        {1, nullptr, "GetTransferTaskInfoForOcean"},
        {2, nullptr, "ListTransferTaskInfoForOcean"},
        {3, nullptr, "DeleteTransferTaskForOcean"},
        {4, nullptr, "RaiseTransferTaskPriorityForOcean"},
        {5, D<&ITransferTaskListController::GetTransferTaskEndEventNativeHandleHolder>, "GetTransferTaskEndEventNativeHandleHolder"},
        {6, nullptr, "GetTransferTaskProgressForOcean"},
        {7, nullptr, "GetTransferTaskLastResultForOcean"},
        {8, D<&ITransferTaskListController::StopNextTransferTaskExecution>, "StopNextTransferTaskExecution"},
        {9, D<&ITransferTaskListController::GetTransferTaskStartEventNativeHandleHolder>, "GetTransferTaskStartEventNativeHandleHolder"},
        {10, nullptr, "SuspendTransferTaskForOcean"},
        {11, nullptr, "GetCurrentTransferTaskInfoForOcean"},
        {12, nullptr, "FindTransferTaskInfoForOcean"},
        {13, nullptr, "CancelCurrentRepairTransferTask"},
        {14, nullptr, "GetRepairTransferTaskProgress"},
        {15, nullptr, "EnsureExecutableForRepairTransferTask"},
        {16, D<&ITransferTaskListController::GetTransferTaskCount>, "GetTransferTaskCount"},
        {17, nullptr, "GetTransferTaskInfo"},
        {18, nullptr, "ListTransferTaskInfo"},
        {19, nullptr, "DeleteTransferTask"},
        {20, nullptr, "RaiseTransferTaskPriority"},
        {21, nullptr, "GetTransferTaskProgress"},
        {22, nullptr, "GetTransferTaskLastResult"},
        {23, nullptr, "SuspendTransferTask"},
        {24, D<&ITransferTaskListController::GetCurrentTransferTaskInfo>, "GetCurrentTransferTaskInfo"},
        {25, D<&ITransferTaskListController::FindTransferTaskInfo>, "FindTransferTaskInfo"},
        {26, nullptr, "Unknown26"}, //20.1.0+
        {27, nullptr, "Unknown27"}, //20.1.0+
        {28, nullptr, "Unknown28"}, //20.1.0+
        {29, nullptr, "Unknown29"}, //20.1.0+
        {30, nullptr, "Unknown30"}, //20.1.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

ITransferTaskListController::~ITransferTaskListController() = default;

Result ITransferTaskListController::GetTransferTaskEndEventNativeHandleHolder(
    Out<SharedPointer<INativeHandleHolder>> out_holder) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    *out_holder = std::make_shared<INativeHandleHolder>(system);
    R_SUCCEED();
}

Result ITransferTaskListController::StopNextTransferTaskExecution(
    Out<SharedPointer<IStopperObject>> out_stopper) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    *out_stopper = std::make_shared<IStopperObject>(system);
    R_SUCCEED();
}

Result ITransferTaskListController::GetTransferTaskStartEventNativeHandleHolder(
    Out<SharedPointer<INativeHandleHolder>> out_holder) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    *out_holder = std::make_shared<INativeHandleHolder>(system);
    R_SUCCEED();
}

Result ITransferTaskListController::GetCurrentTransferTaskInfo(Out<std::array<u8, 0x30>> out_info,
                                                              u8 unknown) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called, unknown={:#x}", unknown);
    out_info->fill(0);
    R_SUCCEED();
}

Result ITransferTaskListController::FindTransferTaskInfo(Out<std::array<u8, 0x30>> out_info,
                                                        InBuffer<BufferAttr_HipcAutoSelect> in) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called, in_size={}", in.size());
    out_info->fill(0);
    R_SUCCEED();
}

Result ITransferTaskListController::GetTransferTaskCount(Out<u32> out_count, u8 unknown) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called, unknown={:#x}", unknown);
    *out_count = 0;
    R_SUCCEED();
}

} // namespace Service::OLSC
