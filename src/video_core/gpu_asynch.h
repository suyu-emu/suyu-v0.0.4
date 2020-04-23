// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"

namespace Core::Frontend {
class GraphicsContext;
}

namespace VideoCore {
class RendererBase;
} // namespace VideoCore

namespace VideoCommon {

/// Implementation of GPU interface that runs the GPU asynchronously
class GPUAsynch final : public Tegra::GPU {
public:
    explicit GPUAsynch(Core::System& system, std::unique_ptr<VideoCore::RendererBase>&& renderer,
                       std::unique_ptr<Core::Frontend::GraphicsContext>&& context);
    ~GPUAsynch() override;

    void Start() override;
    void PushGPUEntries(Tegra::CommandList&& entries) override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;
    void FlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void WaitIdle() const override;

    void OnCommandListEnd() override;

protected:
    void TriggerCpuInterrupt(u32 syncpoint_id, u32 value) const override;

private:
    GPUThread::ThreadManager gpu_thread;
    std::unique_ptr<Core::Frontend::GraphicsContext> cpu_context;
    std::unique_ptr<Core::Frontend::GraphicsContext> gpu_context;
};

} // namespace VideoCommon
