// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450 core
#extension GL_ARB_shader_stencil_export : require

layout(binding = 0) uniform sampler2DMS depth_tex;
layout(binding = 1) uniform usampler2DMS stencil_tex;

layout(location = 0) in vec2 texcoord;

void main() {
    gl_FragDepth = texelFetch(depth_tex, ivec2(texcoord), 0).r;
    gl_FragStencilRefARB = int(texelFetch(stencil_tex, ivec2(texcoord), 0).r);
}
