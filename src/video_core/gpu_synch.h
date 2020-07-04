// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/gpu.h"

namespace Core::Frontend {
class GraphicsContext;
}

namespace VideoCore {
class RendererBase;
} // namespace VideoCore

namespace VideoCommon {

/// Implementation of GPU interface that runs the GPU synchronously
class GPUSynch final : public Tegra::GPU {
public:
    explicit GPUSynch(Core::System& system, std::unique_ptr<VideoCore::RendererBase>&& renderer,
                      std::unique_ptr<Core::Frontend::GraphicsContext>&& context);
    ~GPUSynch() override;

    void Start() override;
    void ObtainContext() override;
    void ReleaseContext() override;
    void PushGPUEntries(Tegra::CommandList&& entries) override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;
    void FlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void WaitIdle() const override {}

protected:
    void TriggerCpuInterrupt([[maybe_unused]] u32 syncpoint_id,
                             [[maybe_unused]] u32 value) const override {}

private:
    std::unique_ptr<Core::Frontend::GraphicsContext> context;
};

} // namespace VideoCommon
