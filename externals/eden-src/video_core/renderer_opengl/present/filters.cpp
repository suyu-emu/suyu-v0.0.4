// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 Torzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/host_shaders/opengl_present_frag.h"
#include "video_core/host_shaders/opengl_present_scaleforce_frag.h"
#include "video_core/host_shaders/present_area_frag.h"
#include "video_core/host_shaders/present_bicubic_frag.h"
#include "video_core/host_shaders/present_gaussian_frag.h"
#include "video_core/host_shaders/present_lanczos_frag.h"
#include "video_core/host_shaders/present_spline1_frag.h"
#include "video_core/host_shaders/present_mitchell_frag.h"
#include "video_core/host_shaders/present_bspline_frag.h"
#include "video_core/host_shaders/present_zero_tangent_frag.h"
#include "video_core/host_shaders/present_mmpx_frag.h"
#include "video_core/renderer_opengl/present/filters.h"
#include "video_core/renderer_opengl/present/util.h"

namespace OpenGL {

std::unique_ptr<WindowAdaptPass> MakeNearestNeighbor(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateNearestNeighborSampler(),
                                             HostShaders::OPENGL_PRESENT_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeBilinear(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::OPENGL_PRESENT_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeSpline1(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_SPLINE1_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeBicubic(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_BICUBIC_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeMitchell(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_MITCHELL_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeZeroTangent(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_ZERO_TANGENT_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeBSpline(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_BSPLINE_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeGaussian(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_GAUSSIAN_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeLanczos(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_LANCZOS_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeScaleForce(const Device& device) {
    return std::make_unique<WindowAdaptPass>(
        device, CreateBilinearSampler(),
        fmt::format("#version 460\n{}", HostShaders::OPENGL_PRESENT_SCALEFORCE_FRAG));
}

std::unique_ptr<WindowAdaptPass> MakeArea(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateBilinearSampler(),
                                             HostShaders::PRESENT_AREA_FRAG);
}

std::unique_ptr<WindowAdaptPass> MakeMmpx(const Device& device) {
    return std::make_unique<WindowAdaptPass>(device, CreateNearestNeighborSampler(),
                                             HostShaders::PRESENT_MMPX_FRAG);
}

} // namespace OpenGL
