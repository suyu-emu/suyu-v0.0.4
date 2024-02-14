// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/service.h"

namespace Service::VI {

class IHOSBinderDriver final : public ServiceFramework<IHOSBinderDriver> {
public:
    explicit IHOSBinderDriver(Core::System& system_, Nvnflinger::HosBinderDriverServer& server);
    ~IHOSBinderDriver() override;

private:
    Result TransactParcel(s32 binder_id, android::TransactionId transaction_id,
                          InBuffer<BufferAttr_HipcMapAlias> parcel_data,
                          OutBuffer<BufferAttr_HipcMapAlias> parcel_reply, u32 flags);
    Result AdjustRefcount(s32 binder_id, s32 addval, s32 type);
    Result GetNativeHandle(s32 binder_id, u32 type_id,
                           OutCopyHandle<Kernel::KReadableEvent> out_handle);
    Result TransactParcelAuto(s32 binder_id, android::TransactionId transaction_id,
                              InBuffer<BufferAttr_HipcAutoSelect> parcel_data,
                              OutBuffer<BufferAttr_HipcAutoSelect> parcel_reply, u32 flags);

private:
    Nvnflinger::HosBinderDriverServer& m_server;
};

} // namespace Service::VI
