// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/aoc/purchase_event_manager.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AOC {

constexpr Result ResultNoPurchasedProductInfoAvailable{ErrorModule::NIMShop, 400};

IPurchaseEventManager::IPurchaseEventManager(Core::System& system_)
    : ServiceFramework{system_, "IPurchaseEventManager"},
      service_context{system, "IPurchaseEventManager"} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IPurchaseEventManager::SetDefaultDeliveryTarget, "SetDefaultDeliveryTarget"},
            {1, &IPurchaseEventManager::SetDeliveryTarget, "SetDeliveryTarget"},
            {2, &IPurchaseEventManager::GetPurchasedEventReadableHandle, "GetPurchasedEventReadableHandle"},
            {3, &IPurchaseEventManager::PopPurchasedProductInfo, "PopPurchasedProductInfo"},
            {4, &IPurchaseEventManager::PopPurchasedProductInfoWithUid, "PopPurchasedProductInfoWithUid"},
        };
    // clang-format on

    RegisterHandlers(functions);

    purchased_event = service_context.CreateEvent("IPurchaseEventManager:PurchasedEvent");
}

IPurchaseEventManager::~IPurchaseEventManager() {
    service_context.CloseEvent(purchased_event);
}

void IPurchaseEventManager::SetDefaultDeliveryTarget(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto unknown_1 = rp.Pop<u64>();
    [[maybe_unused]] const auto unknown_2 = ctx.ReadBuffer();

    LOG_WARNING(Service_AOC, "(STUBBED) called, unknown_1={}", unknown_1);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IPurchaseEventManager::SetDeliveryTarget(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto unknown_1 = rp.Pop<u64>();
    [[maybe_unused]] const auto unknown_2 = ctx.ReadBuffer();

    LOG_WARNING(Service_AOC, "(STUBBED) called, unknown_1={}", unknown_1);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IPurchaseEventManager::GetPurchasedEventReadableHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AOC, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(purchased_event->GetReadableEvent());
}

void IPurchaseEventManager::PopPurchasedProductInfo(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AOC, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNoPurchasedProductInfoAvailable);
}

void IPurchaseEventManager::PopPurchasedProductInfoWithUid(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AOC, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNoPurchasedProductInfoAvailable);
}

} // namespace Service::AOC
