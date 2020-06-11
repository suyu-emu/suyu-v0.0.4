// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hardware_interrupt_manager.h"
#include "video_core/gpu_asynch.h"
#include "video_core/gpu_thread.h"
#include "video_core/renderer_base.h"

namespace VideoCommon {

GPUAsynch::GPUAsynch(Core::System& system) : GPU{system, true}, gpu_thread{system} {}

GPUAsynch::~GPUAsynch() = default;

void GPUAsynch::Start() {
    gpu_thread.StartThread(*renderer, renderer->Context(), *dma_pusher);
    cpu_context = renderer->GetRenderWindow().CreateSharedContext();
    cpu_context->MakeCurrent();
}

void GPUAsynch::ObtainContext() {
    cpu_context->MakeCurrent();
}

void GPUAsynch::ReleaseContext() {
    cpu_context->DoneCurrent();
}

void GPUAsynch::PushGPUEntries(Tegra::CommandList&& entries) {
    gpu_thread.SubmitList(std::move(entries));
}

void GPUAsynch::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    gpu_thread.SwapBuffers(framebuffer);
}

void GPUAsynch::FlushRegion(VAddr addr, u64 size) {
    gpu_thread.FlushRegion(addr, size);
}

void GPUAsynch::InvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.InvalidateRegion(addr, size);
}

void GPUAsynch::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.FlushAndInvalidateRegion(addr, size);
}

void GPUAsynch::TriggerCpuInterrupt(const u32 syncpoint_id, const u32 value) const {
    auto& interrupt_manager = system.InterruptManager();
    interrupt_manager.GPUInterruptSyncpt(syncpoint_id, value);
}

void GPUAsynch::WaitIdle() const {
    gpu_thread.WaitIdle();
}

void GPUAsynch::OnCommandListEnd() {
    gpu_thread.OnCommandListEnd();
}

} // namespace VideoCommon
