#version 450

layout(location = 0) in vec2 texcoord;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D input_texture;

layout(push_constant) uniform PushConstants {
    float frame_count;
    float dither_strength;
} constants;

// Pseudo-random number generator
float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    vec4 input_color = texture(input_texture, texcoord);

    // Generate temporal noise based on frame count
    vec2 noise_coord = gl_FragCoord.xy + vec2(constants.frame_count);
    float noise = rand(noise_coord) * 2.0 - 1.0;

    // Apply dithering
    vec3 dithered = input_color.rgb + noise * constants.dither_strength;

    color = vec4(dithered, input_color.a);
}