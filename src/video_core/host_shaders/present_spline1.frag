// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Spline (smooth linear inerpolation) with 1 texel fetch (needs bilinear to work)
// Emulates bicubic without actually doing bicubic
// See https://iquilezles.org/articles/texture, unfortunely there are issues with the original
// where smoothstep "expansion" actually results in worse codegen (SPIRV-Opt does a direct conv to smoothstep)
// TODO: Numerical analysis - fract is sawtooth func and floor, reuse params? Perhaps - no need for precision

#version 460 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;
layout (binding = 0) uniform sampler2D color_texture;

vec4 textureSpline1(sampler2D sam, vec2 uv) {
    float r = float(textureSize(sam, 0).x);
    vec2 x = fract(uv * r + 0.5);
    return texture(sam, (floor(uv * r + 0.5) + smoothstep(0.0, 1.0, x) - 0.5) / r);
}

void main() {
    color = textureSpline1(color_texture, frag_tex_coord);
}
