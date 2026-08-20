// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450 core

layout(binding = 0) uniform sampler2DMS tex;

layout(location = 0) in vec2 texcoord;
layout(location = 0) out vec4 color;

void main() {
    color = texelFetch(tex, ivec2(texcoord), gl_SampleID);
}
