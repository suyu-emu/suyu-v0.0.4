// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/display_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

namespace {
struct OutputParameters {
    bool was_written;
    s32 fbshare_layer_index;
};

static_assert(sizeof(OutputParameters) == 8, "OutputParameters has wrong size");
} // namespace

IDisplayController::IDisplayController(Core::System& system_, std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "IDisplayController"}, applet(std::move(applet_)) {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetLastForegroundCaptureImage"},
        {1, nullptr, "UpdateLastForegroundCaptureImage"},
        {2, nullptr, "GetLastApplicationCaptureImage"},
        {3, nullptr, "GetCallerAppletCaptureImage"},
        {4, nullptr, "UpdateCallerAppletCaptureImage"},
        {5, nullptr, "GetLastForegroundCaptureImageEx"},
        {6, nullptr, "GetLastApplicationCaptureImageEx"},
        {7, &IDisplayController::GetCallerAppletCaptureImageEx, "GetCallerAppletCaptureImageEx"},
        {8, &IDisplayController::TakeScreenShotOfOwnLayer, "TakeScreenShotOfOwnLayer"},
        {9, nullptr, "CopyBetweenCaptureBuffers"},
        {10, nullptr, "AcquireLastApplicationCaptureBuffer"},
        {11, nullptr, "ReleaseLastApplicationCaptureBuffer"},
        {12, nullptr, "AcquireLastForegroundCaptureBuffer"},
        {13, nullptr, "ReleaseLastForegroundCaptureBuffer"},
        {14, nullptr, "AcquireCallerAppletCaptureBuffer"},
        {15, nullptr, "ReleaseCallerAppletCaptureBuffer"},
        {16, nullptr, "AcquireLastApplicationCaptureBufferEx"},
        {17, nullptr, "AcquireLastForegroundCaptureBufferEx"},
        {18, nullptr, "AcquireCallerAppletCaptureBufferEx"},
        {20, nullptr, "ClearCaptureBuffer"},
        {21, nullptr, "ClearAppletTransitionBuffer"},
        {22, &IDisplayController::AcquireLastApplicationCaptureSharedBuffer, "AcquireLastApplicationCaptureSharedBuffer"},
        {23, &IDisplayController::ReleaseLastApplicationCaptureSharedBuffer, "ReleaseLastApplicationCaptureSharedBuffer"},
        {24, &IDisplayController::AcquireLastForegroundCaptureSharedBuffer, "AcquireLastForegroundCaptureSharedBuffer"},
        {25, &IDisplayController::ReleaseLastForegroundCaptureSharedBuffer, "ReleaseLastForegroundCaptureSharedBuffer"},
        {26, &IDisplayController::AcquireCallerAppletCaptureSharedBuffer, "AcquireCallerAppletCaptureSharedBuffer"},
        {27, &IDisplayController::ReleaseCallerAppletCaptureSharedBuffer, "ReleaseCallerAppletCaptureSharedBuffer"},
        {28, nullptr, "TakeScreenShotOfOwnLayerEx"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IDisplayController::~IDisplayController() = default;

void IDisplayController::GetCallerAppletCaptureImageEx(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    OutputParameters params{};
    const auto res = applet->system_buffer_manager.WriteAppletCaptureBuffer(
        &params.was_written, &params.fbshare_layer_index);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw(params);
}

void IDisplayController::TakeScreenShotOfOwnLayer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IDisplayController::AcquireLastApplicationCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    OutputParameters params{};
    const auto res = applet->system_buffer_manager.WriteAppletCaptureBuffer(
        &params.was_written, &params.fbshare_layer_index);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw(params);
}

void IDisplayController::ReleaseLastApplicationCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IDisplayController::AcquireLastForegroundCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    OutputParameters params{};
    const auto res = applet->system_buffer_manager.WriteAppletCaptureBuffer(
        &params.was_written, &params.fbshare_layer_index);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw(params);
}

void IDisplayController::ReleaseLastForegroundCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IDisplayController::AcquireCallerAppletCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    OutputParameters params{};
    const auto res = applet->system_buffer_manager.WriteAppletCaptureBuffer(
        &params.was_written, &params.fbshare_layer_index);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.PushRaw(params);
}

void IDisplayController::ReleaseCallerAppletCaptureSharedBuffer(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
