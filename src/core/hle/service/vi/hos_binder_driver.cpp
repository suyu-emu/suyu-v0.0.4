// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/nvnflinger/binder.h"
#include "core/hle/service/nvnflinger/hos_binder_driver_server.h"
#include "core/hle/service/vi/hos_binder_driver.h"

namespace Service::VI {

IHOSBinderDriver::IHOSBinderDriver(Core::System& system_, Nvnflinger::HosBinderDriverServer& server)
    : ServiceFramework{system_, "IHOSBinderDriver"}, m_server(server) {
    static const FunctionInfo functions[] = {
        {0, C<&IHOSBinderDriver::TransactParcel>, "TransactParcel"},
        {1, C<&IHOSBinderDriver::AdjustRefcount>, "AdjustRefcount"},
        {2, C<&IHOSBinderDriver::GetNativeHandle>, "GetNativeHandle"},
        {3, C<&IHOSBinderDriver::TransactParcelAuto>, "TransactParcelAuto"},
    };
    RegisterHandlers(functions);
}

IHOSBinderDriver::~IHOSBinderDriver() = default;

Result IHOSBinderDriver::TransactParcel(s32 binder_id, android::TransactionId transaction_id,
                                        InBuffer<BufferAttr_HipcMapAlias> parcel_data,
                                        OutBuffer<BufferAttr_HipcMapAlias> parcel_reply,
                                        u32 flags) {
    LOG_DEBUG(Service_VI, "called. id={} transaction={}, flags={}", binder_id, transaction_id,
              flags);
    m_server.TryGetProducer(binder_id)->Transact(transaction_id, flags, parcel_data, parcel_reply);
    R_SUCCEED();
}

Result IHOSBinderDriver::AdjustRefcount(s32 binder_id, s32 addval, s32 type) {
    LOG_WARNING(Service_VI, "(STUBBED) called id={}, addval={}, type={}", binder_id, addval, type);
    R_SUCCEED();
}

Result IHOSBinderDriver::GetNativeHandle(s32 binder_id, u32 type_id,
                                         OutCopyHandle<Kernel::KReadableEvent> out_handle) {
    LOG_WARNING(Service_VI, "(STUBBED) called id={}, type_id={}", binder_id, type_id);
    *out_handle = &m_server.TryGetProducer(binder_id)->GetNativeHandle();
    R_SUCCEED();
}

Result IHOSBinderDriver::TransactParcelAuto(s32 binder_id, android::TransactionId transaction_id,
                                            InBuffer<BufferAttr_HipcAutoSelect> parcel_data,
                                            OutBuffer<BufferAttr_HipcAutoSelect> parcel_reply,
                                            u32 flags) {
    R_RETURN(this->TransactParcel(binder_id, transaction_id, parcel_data, parcel_reply, flags));
}

} // namespace Service::VI
