#version 450

layout(location = 0) in vec2 texcoord;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D input_texture;

void main() {
    vec4 rgba = texture(input_texture, texcoord);
    color = rgba.bgra; // Swap red and blue channels
}