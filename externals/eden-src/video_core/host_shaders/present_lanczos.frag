// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// https://en.wikipedia.org/wiki/Lanczos_resampling

#version 460 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;
layout (binding = 0) uniform sampler2D color_texture;

#define PI 3.1415926535897932384626433
float sinc(float x) {
    return x == 0.0f ? 1.0f : sin(PI * x) / (PI * x);
}
float lanczos(vec2 v, float a) {
    float d = length(v);
    return sinc(d) / sinc(d / a);
}
vec4 textureLanczos(sampler2D textureSampler, vec2 p) {
    vec3 c_sum = vec3(0.0f);
    float w_sum = 0.0f;
    vec2 res = vec2(textureSize(textureSampler, 0));
    vec2 cc = floor(p * res) / res;
    // kernel size = (2r + 1)^2
    const int r = 3; //radius (1 = 3 steps)
    for (int x = -r; x <= r; x++)
        for (int y = -r; y <= r; y++) {
            vec2 kp = 0.5f * (vec2(x, y) / res); // 0.5 = half-pixel level resampling
            vec2 uv = cc + kp;
            float w = lanczos(kp, float(r));
            c_sum += w * texture(textureSampler, p + kp).rgb;
            w_sum += w;
        }
    return vec4(c_sum / w_sum, 1.0f);
}

void main() {
    color = textureLanczos(color_texture, frag_tex_coord);
}
