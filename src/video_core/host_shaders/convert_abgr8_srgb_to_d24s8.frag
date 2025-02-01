// SPDX-FileCopyrightText: 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450
#extension GL_ARB_shader_stencil_export : require

layout(binding = 0) uniform sampler2D color_texture;

// Efficient sRGB to linear conversion
float srgbToLinear(float srgb) {
    return srgb <= 0.04045 ?
           srgb / 12.92 :
           pow((srgb + 0.055) / 1.055, 2.4);
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec4 srgbColor = texelFetch(color_texture, coord, 0);

    // Convert RGB components to linear space
    vec3 linearColor = vec3(
        srgbToLinear(srgbColor.r),
        srgbToLinear(srgbColor.g),
        srgbToLinear(srgbColor.b)
    );

    // Calculate luminance using standard coefficients
    float luminance = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));

    // Convert to 24-bit depth value
    uint depth_val = uint(luminance * float(0xFFFFFF));

    // Extract 8-bit stencil from alpha
    uint stencil_val = uint(srgbColor.a * 255.0);

    // Pack values efficiently
    uint depth_stencil = (stencil_val << 24) | (depth_val & 0x00FFFFFF);

    gl_FragDepth = float(depth_val) / float(0xFFFFFF);
    gl_FragStencilRefARB = int(stencil_val);
}