// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(binding = 0) uniform sampler2D depth_tex;

layout(location = 0) out vec4 color;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    float depth = textureLod(depth_tex, coord, 0).r;
    color = vec4(depth, depth*depth, sqrt(depth), 1.0); // Convert depth to color
}
