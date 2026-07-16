// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#version 450 core

layout(binding = 0) uniform sampler2DMS msaa_in;

layout(push_constant) uniform PushConstants {
    ivec2 dst_offset;
    ivec2 src_offset;
    ivec2 scale;
};

layout(location = 0) out vec4 frag_color;

void main() {
    const ivec2 coord = ivec2(gl_FragCoord.xy) - dst_offset + src_offset;
    const ivec2 msaa_coord = coord / scale;
    const ivec2 sample_offset = coord % scale;
    const int sample_id = sample_offset.x + scale.x * sample_offset.y;
    frag_color = texelFetch(msaa_in, msaa_coord, sample_id);
}
