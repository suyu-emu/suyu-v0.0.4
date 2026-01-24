// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include "common/uuid.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::OLSC {

class IDaemonController;
class IRemoteStorageController;
class ITransferTaskListController;

struct DataTransferPolicy {
    u8 upload_policy;
    u8 download_policy;
};

struct TransferTaskErrorInfo {
    Common::UUID uid;
    u64 application_id;
    u8 unknown_0x18;
    std::array<u8, 7> reserved_0x19;
    u64 unknown_0x20;
    u32 error_code;
    u32 reserved_0x2C;
};
static_assert(sizeof(TransferTaskErrorInfo) == 0x30, "TransferTaskErrorInfo has incorrect size.");

class IOlscServiceForSystemService final : public ServiceFramework<IOlscServiceForSystemService> {
public:
    explicit IOlscServiceForSystemService(Core::System& system_);
    ~IOlscServiceForSystemService() override;

private:
    Result GetTransferTaskListController(
        Out<SharedPointer<ITransferTaskListController>> out_interface);
    Result GetRemoteStorageController(Out<SharedPointer<IRemoteStorageController>> out_interface);
    Result GetDaemonController(Out<SharedPointer<IDaemonController>> out_interface);
    Result GetDataTransferPolicy(Out<DataTransferPolicy> out_policy, u64 application_id);
    Result GetTransferTaskErrorInfo(Out<TransferTaskErrorInfo> out_info, Common::UUID uid, u64 application_id);
    Result GetOlscServiceForSystemService(
        Out<SharedPointer<IOlscServiceForSystemService>> out_interface);
};

} // namespace Service::OLSC
