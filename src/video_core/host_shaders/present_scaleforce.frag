// MIT License
//
// Copyright (c) 2020 BreadFish64
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Adapted from https://github.com/BreadFish64/ScaleFish/tree/master/scaleforce

#version 460

#extension GL_AMD_gpu_shader_half_float : enable
#extension GL_NV_gpu_shader5 : enable

#ifdef VULKAN

#define BINDING_COLOR_TEXTURE 1

#else // ^^^ Vulkan ^^^ // vvv OpenGL vvv

#define BINDING_COLOR_TEXTURE 0

#endif

layout (location = 0) in vec2 tex_coord;

layout (location = 0) out vec4 frag_color;

layout (binding = BINDING_COLOR_TEXTURE) uniform sampler2D input_texture;

const bool ignore_alpha = true;

float16_t ColorDist1(f16vec4 a, f16vec4 b) {
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion
    const f16vec3 K = f16vec3(0.2627, 0.6780, 0.0593);
    const float16_t scaleB = float16_t(0.5) / (float16_t(1.0) - K.b);
    const float16_t scaleR = float16_t(0.5) / (float16_t(1.0) - K.r);
    f16vec4 diff = a - b;
    float16_t Y = dot(diff.rgb, K);
    float16_t Cb = scaleB * (diff.b - Y);
    float16_t Cr = scaleR * (diff.r - Y);
    f16vec3 YCbCr = f16vec3(Y, Cb, Cr);
    float16_t d = length(YCbCr);
    if (ignore_alpha) {
        return d;
    }
    return sqrt(a.a * b.a * d * d + diff.a * diff.a);
}

f16vec4 ColorDist(f16vec4 ref, f16vec4 A, f16vec4 B, f16vec4 C, f16vec4 D) {
    return f16vec4(
            ColorDist1(ref, A),
            ColorDist1(ref, B),
            ColorDist1(ref, C),
            ColorDist1(ref, D)
        );
}

vec4 Scaleforce(sampler2D tex, vec2 tex_coord) {
    f16vec4 bl = f16vec4(textureOffset(tex, tex_coord, ivec2(-1, -1)));
    f16vec4 bc = f16vec4(textureOffset(tex, tex_coord, ivec2(0, -1)));
    f16vec4 br = f16vec4(textureOffset(tex, tex_coord, ivec2(1, -1)));
    f16vec4 cl = f16vec4(textureOffset(tex, tex_coord, ivec2(-1, 0)));
    f16vec4 cc = f16vec4(texture(tex, tex_coord));
    f16vec4 cr = f16vec4(textureOffset(tex, tex_coord, ivec2(1, 0)));
    f16vec4 tl = f16vec4(textureOffset(tex, tex_coord, ivec2(-1, 1)));
    f16vec4 tc = f16vec4(textureOffset(tex, tex_coord, ivec2(0, 1)));
    f16vec4 tr = f16vec4(textureOffset(tex, tex_coord, ivec2(1, 1)));

    f16vec4 offset_tl = ColorDist(cc, tl, tc, tr, cr);
    f16vec4 offset_br = ColorDist(cc, br, bc, bl, cl);

    // Calculate how different cc is from the texels around it
    const float16_t plus_weight = float16_t(1.5);
    const float16_t cross_weight = float16_t(1.5);
    float16_t total_dist = dot(offset_tl + offset_br, f16vec4(cross_weight, plus_weight, cross_weight, plus_weight));

    if (total_dist == float16_t(0.0)) {
        return cc;
    } else {
        // Add together all the distances with direction taken into account
        f16vec4 tmp = offset_tl - offset_br;
        f16vec2 total_offset = tmp.wy * plus_weight + (tmp.zz + f16vec2(-tmp.x, tmp.x)) * cross_weight;

        // When the image has thin points, they tend to split apart.
        // This is because the texels all around are different and total_offset reaches into clear areas.
        // This works pretty well to keep the offset in bounds for these cases.
        float16_t clamp_val = length(total_offset) / total_dist;
        f16vec2 final_offset = clamp(total_offset, -clamp_val, clamp_val) / f16vec2(textureSize(tex, 0));

        return texture(tex, tex_coord - final_offset);
    }
}

void main() {
    frag_color = Scaleforce(input_texture, tex_coord);
}