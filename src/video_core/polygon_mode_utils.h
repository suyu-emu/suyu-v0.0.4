// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "video_core/engines/maxwell_3d.h"

namespace VideoCore {

inline Tegra::Engines::Maxwell3D::Regs::PolygonMode EffectivePolygonMode(
    const Tegra::Engines::Maxwell3D::Regs& regs) {
    using Maxwell = Tegra::Engines::Maxwell3D::Regs;

    const bool cull_enabled = regs.gl_cull_test_enabled != 0;
    const auto cull_face = regs.gl_cull_face;
    const bool cull_front = cull_enabled && (cull_face == Maxwell::CullFace::Front ||
                                             cull_face == Maxwell::CullFace::FrontAndBack);
    const bool cull_back = cull_enabled && (cull_face == Maxwell::CullFace::Back ||
                                            cull_face == Maxwell::CullFace::FrontAndBack);

    const bool render_front = !cull_front;
    const bool render_back = !cull_back;

    const auto front_mode = regs.polygon_mode_front;
    const auto back_mode = regs.polygon_mode_back;

    if (render_front && render_back && front_mode != back_mode) {
        if (front_mode == Maxwell::PolygonMode::Line || back_mode == Maxwell::PolygonMode::Line) {
            return Maxwell::PolygonMode::Line;
        }
        if (front_mode == Maxwell::PolygonMode::Point || back_mode == Maxwell::PolygonMode::Point) {
            return Maxwell::PolygonMode::Point;
        }
    }

    if (render_front) {
        return front_mode;
    }
    if (render_back) {
        return back_mode;
    }
    return front_mode;
}

} // namespace VideoCore

