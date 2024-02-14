// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::VI {

class IManagerDisplayService final : public ServiceFramework<IManagerDisplayService> {
public:
    explicit IManagerDisplayService(Core::System& system_,
                                    std::shared_ptr<Nvnflinger::Nvnflinger> surface_flinger);
    ~IManagerDisplayService() override;

private:
    Result CreateManagedLayer(Out<u64> out_layer_id, u32 unknown, u64 display_id,
                              AppletResourceUserId aruid);
    Result AddToLayerStack(u32 stack_id, u64 layer_id);
    Result SetLayerVisibility(bool visible, u64 layer_id);

private:
    const std::shared_ptr<Nvnflinger::Nvnflinger> m_surface_flinger;
};

} // namespace Service::VI
