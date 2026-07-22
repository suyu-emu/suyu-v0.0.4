// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450

layout(push_constant) uniform constants {
    vec2 scale;
    vec2 size;
    vec2 resize_factor;
    float edge_sharpness;
};
layout(location = 0) out highp vec2 texcoord;

void main() {
    float x = float((gl_VertexIndex & 1) << 2);
    float y = float((gl_VertexIndex & 2) << 1);
    gl_Position = vec4(x - 1.0f, y - 1.0f, 0.0, 1.0f) * vec4(sign(resize_factor), 1.f, 1.f);
    texcoord = vec2(x, y) * abs(resize_factor) * 0.5;
}
