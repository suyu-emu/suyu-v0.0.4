// SPDX-FileCopyrightText: Copyright 2023 Your Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D color_texture;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec4 color = texelFetch(color_texture, coord, 0).abgr;

    uvec4 bytes = uvec4(color * (exp2(8) - 1.0f)) << uvec4(24, 16, 8, 0);
    uint depth_unorm = bytes.x | bytes.y | bytes.z | bytes.w;

    float depth_float = uintBitsToFloat(depth_unorm);

    gl_FragDepth = depth_float;
}
