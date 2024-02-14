// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/service.h"

namespace Service::Nvnflinger {

class HosBinderDriverServer;
class Nvnflinger;

class IHOSBinderDriver final : public ServiceFramework<IHOSBinderDriver> {
public:
    explicit IHOSBinderDriver(Core::System& system_, std::shared_ptr<HosBinderDriverServer> server,
                              std::shared_ptr<Nvnflinger> surface_flinger);
    ~IHOSBinderDriver() override;

    std::shared_ptr<Nvnflinger> GetSurfaceFlinger() {
        return m_surface_flinger;
    }

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
    const std::shared_ptr<HosBinderDriverServer> m_server;
    const std::shared_ptr<Nvnflinger> m_surface_flinger;
};

} // namespace Service::Nvnflinger
