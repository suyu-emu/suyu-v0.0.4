// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>
#include <glad/glad.h>
#include "video_core/engines/maxwell_3d.h"

namespace OpenGL {

class OpenGLState {
public:
    struct {
        bool enabled = false; // GL_FRAMEBUFFER_SRGB
    } framebuffer_srgb;

    struct {
        bool alpha_to_coverage = false; // GL_ALPHA_TO_COVERAGE
        bool alpha_to_one = false;      // GL_ALPHA_TO_ONE
    } multisample_control;

    struct {
        bool enabled = false; // GL_CLAMP_FRAGMENT_COLOR_ARB
    } fragment_color_clamp;

    struct {
        bool far_plane = false;
        bool near_plane = false;
    } depth_clamp; // GL_DEPTH_CLAMP

    struct {
        bool enabled = false;       // GL_CULL_FACE
        GLenum mode = GL_BACK;      // GL_CULL_FACE_MODE
        GLenum front_face = GL_CCW; // GL_FRONT_FACE
    } cull;

    struct {
        bool test_enabled = false;      // GL_DEPTH_TEST
        GLboolean write_mask = GL_TRUE; // GL_DEPTH_WRITEMASK
        GLenum test_func = GL_LESS;     // GL_DEPTH_FUNC
    } depth;

    struct {
        bool enabled = false;
        GLuint index = 0;
    } primitive_restart; // GL_PRIMITIVE_RESTART

    struct ColorMask {
        GLboolean red_enabled = GL_TRUE;
        GLboolean green_enabled = GL_TRUE;
        GLboolean blue_enabled = GL_TRUE;
        GLboolean alpha_enabled = GL_TRUE;
    };
    std::array<ColorMask, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets>
        color_mask; // GL_COLOR_WRITEMASK
    struct {
        bool test_enabled = false; // GL_STENCIL_TEST
        struct {
            GLenum test_func = GL_ALWAYS;         // GL_STENCIL_FUNC
            GLint test_ref = 0;                   // GL_STENCIL_REF
            GLuint test_mask = 0xFFFFFFFF;        // GL_STENCIL_VALUE_MASK
            GLuint write_mask = 0xFFFFFFFF;       // GL_STENCIL_WRITEMASK
            GLenum action_stencil_fail = GL_KEEP; // GL_STENCIL_FAIL
            GLenum action_depth_fail = GL_KEEP;   // GL_STENCIL_PASS_DEPTH_FAIL
            GLenum action_depth_pass = GL_KEEP;   // GL_STENCIL_PASS_DEPTH_PASS
        } front, back;
    } stencil;

    struct Blend {
        bool enabled = false;              // GL_BLEND
        GLenum rgb_equation = GL_FUNC_ADD; // GL_BLEND_EQUATION_RGB
        GLenum a_equation = GL_FUNC_ADD;   // GL_BLEND_EQUATION_ALPHA
        GLenum src_rgb_func = GL_ONE;      // GL_BLEND_SRC_RGB
        GLenum dst_rgb_func = GL_ZERO;     // GL_BLEND_DST_RGB
        GLenum src_a_func = GL_ONE;        // GL_BLEND_SRC_ALPHA
        GLenum dst_a_func = GL_ZERO;       // GL_BLEND_DST_ALPHA
    };
    std::array<Blend, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets> blend;

    struct {
        bool enabled = false;
    } independant_blend;

    struct {
        GLclampf red = 0.0f;
        GLclampf green = 0.0f;
        GLclampf blue = 0.0f;
        GLclampf alpha = 0.0f;
    } blend_color; // GL_BLEND_COLOR

    struct {
        bool enabled = false; // GL_LOGIC_OP_MODE
        GLenum operation = GL_COPY;
    } logic_op;

    static constexpr std::size_t NumSamplers = 32 * 5;
    static constexpr std::size_t NumImages = 8 * 5;
    std::array<GLuint, NumSamplers> textures = {};
    std::array<GLuint, NumSamplers> samplers = {};
    std::array<GLuint, NumImages> images = {};

    struct {
        GLuint read_framebuffer = 0; // GL_READ_FRAMEBUFFER_BINDING
        GLuint draw_framebuffer = 0; // GL_DRAW_FRAMEBUFFER_BINDING
        GLuint vertex_array = 0;     // GL_VERTEX_ARRAY_BINDING
        GLuint shader_program = 0;   // GL_CURRENT_PROGRAM
        GLuint program_pipeline = 0; // GL_PROGRAM_PIPELINE_BINDING
    } draw;

    struct Viewport {
        GLint x = 0;
        GLint y = 0;
        GLint width = 0;
        GLint height = 0;
        GLfloat depth_range_near = 0.0f; // GL_DEPTH_RANGE
        GLfloat depth_range_far = 1.0f;  // GL_DEPTH_RANGE
        struct {
            bool enabled = false; // GL_SCISSOR_TEST
            GLint x = 0;
            GLint y = 0;
            GLsizei width = 0;
            GLsizei height = 0;
        } scissor;
    };
    std::array<Viewport, Tegra::Engines::Maxwell3D::Regs::NumViewports> viewports;

    struct {
        float size = 1.0f; // GL_POINT_SIZE
    } point;

    struct {
        bool point_enable = false;
        bool line_enable = false;
        bool fill_enable = false;
        GLfloat units = 0.0f;
        GLfloat factor = 0.0f;
        GLfloat clamp = 0.0f;
    } polygon_offset;

    struct {
        bool enabled = false;    // GL_ALPHA_TEST
        GLenum func = GL_ALWAYS; // GL_ALPHA_TEST_FUNC
        GLfloat ref = 0.0f;      // GL_ALPHA_TEST_REF
    } alpha_test;

    std::array<bool, 8> clip_distance = {}; // GL_CLIP_DISTANCE

    struct {
        GLenum origin = GL_LOWER_LEFT;
    } clip_control;

    OpenGLState();

    /// Get the currently active OpenGL state
    static OpenGLState GetCurState() {
        return cur_state;
    }

    void SetDefaultViewports();
    /// Apply this state as the current OpenGL state
    void Apply();

    void ApplyFramebufferState();
    void ApplyVertexArrayState();
    void ApplyShaderProgram();
    void ApplyProgramPipeline();
    void ApplyClipDistances();
    void ApplyPointSize();
    void ApplyFragmentColorClamp();
    void ApplyMultisample();
    void ApplySRgb();
    void ApplyCulling();
    void ApplyColorMask();
    void ApplyDepth();
    void ApplyPrimitiveRestart();
    void ApplyStencilTest();
    void ApplyViewport();
    void ApplyTargetBlending(std::size_t target, bool force);
    void ApplyGlobalBlending();
    void ApplyBlending();
    void ApplyLogicOp();
    void ApplyTextures();
    void ApplySamplers();
    void ApplyImages();
    void ApplyDepthClamp();
    void ApplyPolygonOffset();
    void ApplyAlphaTest();
    void ApplyClipControl();

    /// Resets any references to the given resource
    OpenGLState& UnbindTexture(GLuint handle);
    OpenGLState& ResetSampler(GLuint handle);
    OpenGLState& ResetProgram(GLuint handle);
    OpenGLState& ResetPipeline(GLuint handle);
    OpenGLState& ResetVertexArray(GLuint handle);
    OpenGLState& ResetFramebuffer(GLuint handle);

    /// Viewport does not affects glClearBuffer so emulate viewport using scissor test
    void EmulateViewportWithScissor();

    void MarkDirtyBlendState() {
        dirty.blend_state = true;
    }

    void MarkDirtyStencilState() {
        dirty.stencil_state = true;
    }

    void MarkDirtyPolygonOffset() {
        dirty.polygon_offset = true;
    }

    void MarkDirtyColorMask() {
        dirty.color_mask = true;
    }

    void AllDirty() {
        dirty.blend_state = true;
        dirty.stencil_state = true;
        dirty.polygon_offset = true;
        dirty.color_mask = true;
    }

private:
    static OpenGLState cur_state;

    struct {
        bool blend_state;
        bool stencil_state;
        bool viewport_state;
        bool polygon_offset;
        bool color_mask;
    } dirty{};
};
static_assert(std::is_trivially_copyable_v<OpenGLState>);

} // namespace OpenGL
