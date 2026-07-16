// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450 core

layout(binding = 0) uniform sampler2D img_in;

layout(push_constant) uniform PushConstants {
    ivec2 dst_offset;
    ivec2 src_offset;
    ivec2 scale;
};

layout(location = 0) out vec4 frag_color;

void main() {
    const ivec2 msaa_coord = ivec2(gl_FragCoord.xy) - dst_offset;
    const ivec2 sample_offset = ivec2(gl_SampleID % scale.x, gl_SampleID / scale.x);
    const ivec2 coord = msaa_coord * scale + sample_offset + src_offset;
    frag_color = texelFetch(img_in, coord, 0);
}
