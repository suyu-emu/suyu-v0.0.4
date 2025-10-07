// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#version 460 core
layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;
layout (binding = 0) uniform sampler2D color_texture;
vec4 cubic(float x) {
    float x2 = x * x;
    float x3 = x2 * x;
    return vec4(1.0, x, x2, x3) * transpose(mat4x4(
         1.0,   4.0,  1.0,  0.0,
        -3.0,   0.0,  3.0,  0.0,
         3.0,  -6.0,  3.0,  0.0,
        -1.0,   3.0, -3.0,  1.0
    ) * (1.0 / 6.0));
}
vec4 textureBicubic(sampler2D samp, vec2 uv) {
    vec2 tex_size = vec2(textureSize(samp, 0));
    vec2 cc_tex = uv * tex_size - 0.5f;
    vec2 fex = cc_tex - floor(cc_tex);
    vec4 xcubic = cubic(fex.x);
    vec4 ycubic = cubic(fex.y);
    vec4 c = floor(cc_tex).xxyy + vec2(-0.5f, 1.5f).xyxy;
    vec4 z = vec4(xcubic.yw, ycubic.yw);
    vec4 s = vec4(xcubic.xz, ycubic.xz) + z;
    vec4 offset = (c + z / s) * (1.0f / tex_size).xxyy;
    vec2 n = vec2(s.x / (s.x + s.y), s.z / (s.z + s.w));
    return mix(
        mix(texture(samp, offset.yw), texture(samp, offset.xw), n.x),
        mix(texture(samp, offset.yz), texture(samp, offset.xz), n.x),
    n.y);
}
void main() {
    color = textureBicubic(color_texture, frag_tex_coord);
}
