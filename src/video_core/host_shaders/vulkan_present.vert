// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 460 core

layout (location = 0) out vec2 frag_tex_coord;

struct ScreenRectVertex {
    vec2 position;
    vec2 tex_coord;
};

layout (push_constant) uniform PushConstants {
    mat4 modelview_matrix;
    ScreenRectVertex vertices[4];
};

// Vulkan spec 15.8.1:
//   Any member of a push constant block that is declared as an
//   array must only be accessed with dynamically uniform indices.
ScreenRectVertex GetVertex(int index) {
    switch (index) {
    case 0:
    default:
        return vertices[0];
    case 1:
        return vertices[1];
    case 2:
        return vertices[2];
    case 3:
        return vertices[3];
    }
}

void main() {
    ScreenRectVertex vertex = GetVertex(gl_VertexIndex);
    gl_Position = modelview_matrix * vec4(vertex.position, 0.0, 1.0);
    frag_tex_coord = vertex.tex_coord;
}
