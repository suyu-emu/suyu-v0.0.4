// SPDX-FileCopyrightText: Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#version 460 core

precision highp float;
precision highp int;

// Operation modes: RGBA -> 1, RGBY -> 3, LERP -> 4
#define OPERATION_MODE 1
#define EDGE_THRESHOLD (8.0 / 255.0)

layout(push_constant) uniform constants {
    vec2 scale;
    vec2 size;
    vec2 resize_factor;
    float edge_sharpness;
};
layout(set = 0, binding = 0) uniform sampler2D sampler0;
layout(location=0) in vec2 texcoord;
layout(location=0) out vec4 frag_color;

vec4 weightY(vec4 dx, vec4 dy, vec4 std) {
    vec4 x = ((dx * dx) + (dy * dy)) * 0.55f + std;
    return (x - 1.f) * (x - 4.f) * 3.8125f; // approx. of (x - 1) * (x - 4)^3
}

void main() {
    vec4 color = textureLod(sampler0, texcoord.xy, 0.0f);
    // image coord
    vec2 icoord = (texcoord * size + vec2(-0.5f, 0.5f));
    vec2 icoord_pixel = floor(icoord);
    vec2 coord = icoord_pixel * scale;
    vec2 pl = icoord - icoord_pixel;
    // left: 0, right: 1, upDown: 2
    mat3x4 dg = mat3x4(
        textureGather(sampler0, coord, 1),
        textureGather(sampler0, coord + vec2(2.f * scale.x, 0.0f), 1),
        vec4(
            textureGather(sampler0, coord + vec2(scale.x, -scale.y), 1).wz,
            textureGather(sampler0, coord + vec2(scale.x, +scale.y), 1).yx
        )
    );
    float edgeVote = abs(dg[0].z - dg[0].y) + abs(color.y - dg[0].y) + abs(color.y - dg[0].z);
    if (edgeVote > EDGE_THRESHOLD) {
        float mean = (dg[0].y + dg[0].z + dg[1].x + dg[1].w) * 0.25f;
        dg = dg - mean;
        vec4 sum = abs(dg[0]) + abs(dg[1]) + abs(dg[2]);
        float std = 2.181818f / (sum.x + sum.y + sum.z + sum.w);
        mat2x4 w = mat2x4(
            weightY(
                pl.xxxx + vec4(+1.0f, +0.0f, +0.0f, +1.0f),
                pl.yyyy + vec4(-1.0f, -1.0f, +0.0f, +0.0f),
                clamp(abs(dg[0]) * std, 0.0f, 1.0f)
            ) + weightY(
                pl.xxxx + vec4(-1.0f, -2.0f, -2.0f, -1.0f),
                pl.yyyy + vec4(-1.0f, -1.0f, +0.0f, +0.0f),
                clamp(abs(dg[1]) * std, 0.0f, 1.0f)
            ) + weightY(
                pl.xxxx + vec4(+0.0f, -1.0f, -1.0f, +0.0f),
                pl.yyyy + vec4(+1.0f, +1.0f, -2.0f, -2.0f),
                clamp(abs(dg[2]) * std, 0.0f, 1.0f)
            ),
            dg[0] + dg[1] + dg[2]
        );
        // compute final y with bounds
        vec2 yb = vec2(
            min(min(dg[0].y, dg[0].z), min(dg[1].x, dg[1].w)), // min
            max(max(dg[0].y, dg[0].z), max(dg[1].x, dg[1].w))  // max
        );
        vec2 fvy = vec2(
            w[0].x + w[0].y + w[0].z + w[0].w,
            w[1].x + w[1].y + w[1].z + w[1].w
        );
        float fy = clamp((fvy.y / fvy.x) * edge_sharpness, yb[0], yb[1]);
        // Smooth high contrast input
        float dy = clamp(fy - color.y + mean, -23.0f / 255.0f, 23.0f / 255.0f);
        color = clamp(color + dy, 0.0f, 1.0f);
    }
    color.w = 1.0f;  //assume alpha channel is not used
    frag_color.xyzw = color;
}