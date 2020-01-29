// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/bit_util.h"
#include "common/common_types.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "video_core/morton.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/utils.h"
#include "video_core/texture_cache/surface_base.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/textures/convert.h"
#include "video_core/textures/texture.h"

namespace OpenGL {

using Tegra::Texture::SwizzleSource;
using VideoCore::MortonSwizzleMode;

using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceCompression;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

MICROPROFILE_DEFINE(OpenGL_Texture_Upload, "OpenGL", "Texture Upload", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(OpenGL_Texture_Download, "OpenGL", "Texture Download", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(OpenGL_Texture_Buffer_Copy, "OpenGL", "Texture Buffer Copy",
                    MP_RGB(128, 192, 128));

namespace {

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;
    bool compressed;
};

constexpr std::array<FormatTuple, VideoCore::Surface::MaxPixelFormat> tex_format_tuples = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, false},                        // ABGR8U
    {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, false},                                      // ABGR8S
    {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, false},                         // ABGR8UI
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, false},                        // B5G6R5U
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false},                  // A2B10G10R10U
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, false},                    // A1B5G5R5U
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE, false},                                       // R8U
    {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, false},                             // R8UI
    {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false},                                    // RGBA16F
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, false},                                 // RGBA16U
    {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, false},                       // RGBA16UI
    {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false},            // R11FG11FB10F
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, false},                         // RGBA32UI
    {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true},     // DXT1
    {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true},     // DXT23
    {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true},     // DXT45
    {GL_COMPRESSED_RED_RGTC1, GL_RED, GL_UNSIGNED_INT_8_8_8_8, true},               // DXN1
    {GL_COMPRESSED_RG_RGTC2, GL_RG, GL_UNSIGNED_INT_8_8_8_8, true},                 // DXN2UNORM
    {GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, GL_INT, true},                           // DXN2SNORM
    {GL_COMPRESSED_RGBA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true},        // BC7U
    {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, GL_RGB, GL_UNSIGNED_INT_8_8_8_8, true}, // BC6H_UF16
    {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, GL_RGB, GL_UNSIGNED_INT_8_8_8_8, true},   // BC6H_SF16
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},                                   // ASTC_2D_4X4
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, false},                                   // BGRA8
    {GL_RGBA32F, GL_RGBA, GL_FLOAT, false},                                         // RGBA32F
    {GL_RG32F, GL_RG, GL_FLOAT, false},                                             // RG32F
    {GL_R32F, GL_RED, GL_FLOAT, false},                                             // R32F
    {GL_R16F, GL_RED, GL_HALF_FLOAT, false},                                        // R16F
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT, false},                                     // R16U
    {GL_R16_SNORM, GL_RED, GL_SHORT, false},                                        // R16S
    {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false},                           // R16UI
    {GL_R16I, GL_RED_INTEGER, GL_SHORT, false},                                     // R16I
    {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false},                                     // RG16
    {GL_RG16F, GL_RG, GL_HALF_FLOAT, false},                                        // RG16F
    {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, false},                           // RG16UI
    {GL_RG16I, GL_RG_INTEGER, GL_SHORT, false},                                     // RG16I
    {GL_RG16_SNORM, GL_RG, GL_SHORT, false},                                        // RG16S
    {GL_RGB32F, GL_RGB, GL_FLOAT, false},                                           // RGB32F
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, false},                 // RGBA8_SRGB
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false},                                       // RG8U
    {GL_RG8_SNORM, GL_RG, GL_BYTE, false},                                          // RG8S
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, false},                             // RG32UI
    {GL_RGB16F, GL_RGBA, GL_HALF_FLOAT, false},                                     // RGBX16F
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false},                             // R32UI
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},                                   // ASTC_2D_8X8
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},                                   // ASTC_2D_8X5
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},                                   // ASTC_2D_5X4
    {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, false},                            // BGRA8
    // Compressed sRGB formats
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true}, // DXT1_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true}, // DXT23_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true}, // DXT45_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, true},    // BC7U_SRGB
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, false},                        // R4G4B4A4U
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_4X4_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_8X8_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_8X5_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_5X4_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_5X5
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_5X5_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_10X8
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_10X8_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_6X6
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_6X6_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_10X10
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_10X10_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_12X12
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_12X12_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_8X6
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_8X6_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},             // ASTC_2D_6X5
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},      // ASTC_2D_6X5_SRGB
    {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, false}, // E5B9G9R9F

    // Depth formats
    {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false},         // Z32F
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false}, // Z16

    // DepthStencil formats
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false},               // Z24S8
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false},               // S8Z24
    {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, false}, // Z32FS8
}};

const FormatTuple& GetFormatTuple(PixelFormat pixel_format) {
    ASSERT(static_cast<std::size_t>(pixel_format) < tex_format_tuples.size());
    const auto& format{tex_format_tuples[static_cast<std::size_t>(pixel_format)]};
    return format;
}

GLenum GetTextureTarget(const SurfaceTarget& target) {
    switch (target) {
    case SurfaceTarget::TextureBuffer:
        return GL_TEXTURE_BUFFER;
    case SurfaceTarget::Texture1D:
        return GL_TEXTURE_1D;
    case SurfaceTarget::Texture2D:
        return GL_TEXTURE_2D;
    case SurfaceTarget::Texture3D:
        return GL_TEXTURE_3D;
    case SurfaceTarget::Texture1DArray:
        return GL_TEXTURE_1D_ARRAY;
    case SurfaceTarget::Texture2DArray:
        return GL_TEXTURE_2D_ARRAY;
    case SurfaceTarget::TextureCubemap:
        return GL_TEXTURE_CUBE_MAP;
    case SurfaceTarget::TextureCubeArray:
        return GL_TEXTURE_CUBE_MAP_ARRAY;
    }
    UNREACHABLE();
    return {};
}

GLint GetSwizzleSource(SwizzleSource source) {
    switch (source) {
    case SwizzleSource::Zero:
        return GL_ZERO;
    case SwizzleSource::R:
        return GL_RED;
    case SwizzleSource::G:
        return GL_GREEN;
    case SwizzleSource::B:
        return GL_BLUE;
    case SwizzleSource::A:
        return GL_ALPHA;
    case SwizzleSource::OneInt:
    case SwizzleSource::OneFloat:
        return GL_ONE;
    }
    UNREACHABLE();
    return GL_NONE;
}

GLenum GetComponent(PixelFormat format, bool is_first) {
    switch (format) {
    case PixelFormat::Z24S8:
    case PixelFormat::Z32FS8:
        return is_first ? GL_DEPTH_COMPONENT : GL_STENCIL_INDEX;
    case PixelFormat::S8Z24:
        return is_first ? GL_STENCIL_INDEX : GL_DEPTH_COMPONENT;
    default:
        UNREACHABLE();
        return GL_DEPTH_COMPONENT;
    }
}

void ApplyTextureDefaults(const SurfaceParams& params, GLuint texture) {
    if (params.IsBuffer()) {
        return;
    }
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(params.num_levels - 1));
    if (params.num_levels == 1) {
        glTextureParameterf(texture, GL_TEXTURE_LOD_BIAS, 1000.0f);
    }
}

OGLTexture CreateTexture(const SurfaceParams& params, GLenum target, GLenum internal_format,
                         OGLBuffer& texture_buffer) {
    OGLTexture texture;
    texture.Create(target);

    switch (params.target) {
    case SurfaceTarget::Texture1D:
        glTextureStorage1D(texture.handle, params.emulated_levels, internal_format, params.width);
        break;
    case SurfaceTarget::TextureBuffer:
        texture_buffer.Create();
        glNamedBufferStorage(texture_buffer.handle, params.width * params.GetBytesPerPixel(),
                             nullptr, GL_DYNAMIC_STORAGE_BIT);
        glTextureBuffer(texture.handle, internal_format, texture_buffer.handle);
        break;
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::TextureCubemap:
        glTextureStorage2D(texture.handle, params.emulated_levels, internal_format, params.width,
                           params.height);
        break;
    case SurfaceTarget::Texture3D:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubeArray:
        glTextureStorage3D(texture.handle, params.emulated_levels, internal_format, params.width,
                           params.height, params.depth);
        break;
    default:
        UNREACHABLE();
    }

    ApplyTextureDefaults(params, texture.handle);

    return texture;
}

} // Anonymous namespace

CachedSurface::CachedSurface(const GPUVAddr gpu_addr, const SurfaceParams& params)
    : VideoCommon::SurfaceBase<View>(gpu_addr, params) {
    const auto& tuple{GetFormatTuple(params.pixel_format)};
    internal_format = tuple.internal_format;
    format = tuple.format;
    type = tuple.type;
    is_compressed = tuple.compressed;
    target = GetTextureTarget(params.target);
    texture = CreateTexture(params, target, internal_format, texture_buffer);
    DecorateSurfaceName();
    main_view = CreateViewInner(
        ViewParams(params.target, 0, params.is_layered ? params.depth : 1, 0, params.num_levels),
        true);
}

CachedSurface::~CachedSurface() = default;

void CachedSurface::DownloadTexture(std::vector<u8>& staging_buffer) {
    MICROPROFILE_SCOPE(OpenGL_Texture_Download);

    SCOPE_EXIT({ glPixelStorei(GL_PACK_ROW_LENGTH, 0); });

    for (u32 level = 0; level < params.emulated_levels; ++level) {
        glPixelStorei(GL_PACK_ALIGNMENT, std::min(8U, params.GetRowAlignment(level)));
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(params.GetMipWidth(level)));
        const std::size_t mip_offset = params.GetHostMipmapLevelOffset(level);
        u8* const mip_data = staging_buffer.data() + mip_offset;
        const GLsizei size = static_cast<GLsizei>(params.GetHostMipmapSize(level));
        if (is_compressed) {
            glGetCompressedTextureImage(texture.handle, level, size, mip_data);
        } else {
            glGetTextureImage(texture.handle, level, format, type, size, mip_data);
        }
    }
}

void CachedSurface::UploadTexture(const std::vector<u8>& staging_buffer) {
    MICROPROFILE_SCOPE(OpenGL_Texture_Upload);
    SCOPE_EXIT({ glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); });
    for (u32 level = 0; level < params.emulated_levels; ++level) {
        UploadTextureMipmap(level, staging_buffer);
    }
}

void CachedSurface::UploadTextureMipmap(u32 level, const std::vector<u8>& staging_buffer) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, std::min(8U, params.GetRowAlignment(level)));
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(params.GetMipWidth(level)));

    auto compression_type = params.GetCompressionType();

    const std::size_t mip_offset = compression_type == SurfaceCompression::Converted
                                       ? params.GetConvertedMipmapOffset(level)
                                       : params.GetHostMipmapLevelOffset(level);
    const u8* buffer{staging_buffer.data() + mip_offset};
    if (is_compressed) {
        const auto image_size{static_cast<GLsizei>(params.GetHostMipmapSize(level))};
        switch (params.target) {
        case SurfaceTarget::Texture2D:
            glCompressedTextureSubImage2D(texture.handle, level, 0, 0,
                                          static_cast<GLsizei>(params.GetMipWidth(level)),
                                          static_cast<GLsizei>(params.GetMipHeight(level)),
                                          internal_format, image_size, buffer);
            break;
        case SurfaceTarget::Texture3D:
        case SurfaceTarget::Texture2DArray:
        case SurfaceTarget::TextureCubeArray:
            glCompressedTextureSubImage3D(texture.handle, level, 0, 0, 0,
                                          static_cast<GLsizei>(params.GetMipWidth(level)),
                                          static_cast<GLsizei>(params.GetMipHeight(level)),
                                          static_cast<GLsizei>(params.GetMipDepth(level)),
                                          internal_format, image_size, buffer);
            break;
        case SurfaceTarget::TextureCubemap: {
            const std::size_t layer_size{params.GetHostLayerSize(level)};
            for (std::size_t face = 0; face < params.depth; ++face) {
                glCompressedTextureSubImage3D(texture.handle, level, 0, 0, static_cast<GLint>(face),
                                              static_cast<GLsizei>(params.GetMipWidth(level)),
                                              static_cast<GLsizei>(params.GetMipHeight(level)), 1,
                                              internal_format, static_cast<GLsizei>(layer_size),
                                              buffer);
                buffer += layer_size;
            }
            break;
        }
        default:
            UNREACHABLE();
        }
    } else {
        switch (params.target) {
        case SurfaceTarget::Texture1D:
            glTextureSubImage1D(texture.handle, level, 0, params.GetMipWidth(level), format, type,
                                buffer);
            break;
        case SurfaceTarget::TextureBuffer:
            ASSERT(level == 0);
            glNamedBufferSubData(texture_buffer.handle, 0,
                                 params.GetMipWidth(level) * params.GetBytesPerPixel(), buffer);
            break;
        case SurfaceTarget::Texture1DArray:
        case SurfaceTarget::Texture2D:
            glTextureSubImage2D(texture.handle, level, 0, 0, params.GetMipWidth(level),
                                params.GetMipHeight(level), format, type, buffer);
            break;
        case SurfaceTarget::Texture3D:
        case SurfaceTarget::Texture2DArray:
        case SurfaceTarget::TextureCubeArray:
            glTextureSubImage3D(
                texture.handle, level, 0, 0, 0, static_cast<GLsizei>(params.GetMipWidth(level)),
                static_cast<GLsizei>(params.GetMipHeight(level)),
                static_cast<GLsizei>(params.GetMipDepth(level)), format, type, buffer);
            break;
        case SurfaceTarget::TextureCubemap:
            for (std::size_t face = 0; face < params.depth; ++face) {
                glTextureSubImage3D(texture.handle, level, 0, 0, static_cast<GLint>(face),
                                    params.GetMipWidth(level), params.GetMipHeight(level), 1,
                                    format, type, buffer);
                buffer += params.GetHostLayerSize(level);
            }
            break;
        default:
            UNREACHABLE();
        }
    }
}

void CachedSurface::DecorateSurfaceName() {
    LabelGLObject(GL_TEXTURE, texture.handle, GetGpuAddr(), params.TargetName());
}

void CachedSurfaceView::DecorateViewName(GPUVAddr gpu_addr, std::string prefix) {
    LabelGLObject(GL_TEXTURE, texture_view.handle, gpu_addr, prefix);
}

View CachedSurface::CreateView(const ViewParams& view_key) {
    return CreateViewInner(view_key, false);
}

View CachedSurface::CreateViewInner(const ViewParams& view_key, const bool is_proxy) {
    auto view = std::make_shared<CachedSurfaceView>(*this, view_key, is_proxy);
    views[view_key] = view;
    if (!is_proxy)
        view->DecorateViewName(gpu_addr, params.TargetName() + "V:" + std::to_string(view_count++));
    return view;
}

CachedSurfaceView::CachedSurfaceView(CachedSurface& surface, const ViewParams& params,
                                     const bool is_proxy)
    : VideoCommon::ViewBase(params), surface{surface}, is_proxy{is_proxy} {
    target = GetTextureTarget(params.target);
    if (!is_proxy) {
        texture_view = CreateTextureView();
    }
    swizzle = EncodeSwizzle(SwizzleSource::R, SwizzleSource::G, SwizzleSource::B, SwizzleSource::A);
}

CachedSurfaceView::~CachedSurfaceView() = default;

void CachedSurfaceView::Attach(GLenum attachment, GLenum target) const {
    ASSERT(params.num_layers == 1 && params.num_levels == 1);

    const auto& owner_params = surface.GetSurfaceParams();

    switch (owner_params.target) {
    case SurfaceTarget::Texture1D:
        glFramebufferTexture1D(target, attachment, surface.GetTarget(), surface.GetTexture(),
                               params.base_level);
        break;
    case SurfaceTarget::Texture2D:
        glFramebufferTexture2D(target, attachment, surface.GetTarget(), surface.GetTexture(),
                               params.base_level);
        break;
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        glFramebufferTextureLayer(target, attachment, surface.GetTexture(), params.base_level,
                                  params.base_layer);
        break;
    default:
        UNIMPLEMENTED();
    }
}

void CachedSurfaceView::ApplySwizzle(SwizzleSource x_source, SwizzleSource y_source,
                                     SwizzleSource z_source, SwizzleSource w_source) {
    u32 new_swizzle = EncodeSwizzle(x_source, y_source, z_source, w_source);
    if (new_swizzle == swizzle)
        return;
    swizzle = new_swizzle;
    const std::array gl_swizzle = {GetSwizzleSource(x_source), GetSwizzleSource(y_source),
                                   GetSwizzleSource(z_source), GetSwizzleSource(w_source)};
    const GLuint handle = GetTexture();
    const PixelFormat format = surface.GetSurfaceParams().pixel_format;
    switch (format) {
    case PixelFormat::Z24S8:
    case PixelFormat::Z32FS8:
    case PixelFormat::S8Z24:
        glTextureParameteri(handle, GL_DEPTH_STENCIL_TEXTURE_MODE,
                            GetComponent(format, x_source == SwizzleSource::R));
        break;
    default:
        glTextureParameteriv(handle, GL_TEXTURE_SWIZZLE_RGBA, gl_swizzle.data());
        break;
    }
}

OGLTextureView CachedSurfaceView::CreateTextureView() const {
    const auto& owner_params = surface.GetSurfaceParams();
    OGLTextureView texture_view;
    texture_view.Create();

    const GLuint handle{texture_view.handle};
    const FormatTuple& tuple{GetFormatTuple(owner_params.pixel_format)};

    glTextureView(handle, target, surface.texture.handle, tuple.internal_format, params.base_level,
                  params.num_levels, params.base_layer, params.num_layers);

    ApplyTextureDefaults(owner_params, handle);

    return texture_view;
}

TextureCacheOpenGL::TextureCacheOpenGL(Core::System& system,
                                       VideoCore::RasterizerInterface& rasterizer,
                                       const Device& device)
    : TextureCacheBase{system, rasterizer} {
    src_framebuffer.Create();
    dst_framebuffer.Create();
}

TextureCacheOpenGL::~TextureCacheOpenGL() = default;

Surface TextureCacheOpenGL::CreateSurface(GPUVAddr gpu_addr, const SurfaceParams& params) {
    return std::make_shared<CachedSurface>(gpu_addr, params);
}

void TextureCacheOpenGL::ImageCopy(Surface& src_surface, Surface& dst_surface,
                                   const VideoCommon::CopyParams& copy_params) {
    const auto& src_params = src_surface->GetSurfaceParams();
    const auto& dst_params = dst_surface->GetSurfaceParams();
    if (src_params.type != dst_params.type) {
        // A fallback is needed
        return;
    }
    const auto src_handle = src_surface->GetTexture();
    const auto src_target = src_surface->GetTarget();
    const auto dst_handle = dst_surface->GetTexture();
    const auto dst_target = dst_surface->GetTarget();
    glCopyImageSubData(src_handle, src_target, copy_params.source_level, copy_params.source_x,
                       copy_params.source_y, copy_params.source_z, dst_handle, dst_target,
                       copy_params.dest_level, copy_params.dest_x, copy_params.dest_y,
                       copy_params.dest_z, copy_params.width, copy_params.height,
                       copy_params.depth);
}

void TextureCacheOpenGL::ImageBlit(View& src_view, View& dst_view,
                                   const Tegra::Engines::Fermi2D::Config& copy_config) {
    const auto& src_params{src_view->GetSurfaceParams()};
    const auto& dst_params{dst_view->GetSurfaceParams()};

    OpenGLState prev_state{OpenGLState::GetCurState()};
    SCOPE_EXIT({
        prev_state.AllDirty();
        prev_state.Apply();
    });

    OpenGLState state;
    state.draw.read_framebuffer = src_framebuffer.handle;
    state.draw.draw_framebuffer = dst_framebuffer.handle;
    state.framebuffer_srgb.enabled = dst_params.srgb_conversion;
    state.AllDirty();
    state.Apply();

    u32 buffers{};

    UNIMPLEMENTED_IF(src_params.target == SurfaceTarget::Texture3D);
    UNIMPLEMENTED_IF(dst_params.target == SurfaceTarget::Texture3D);

    if (src_params.type == SurfaceType::ColorTexture) {
        src_view->Attach(GL_COLOR_ATTACHMENT0, GL_READ_FRAMEBUFFER);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        dst_view->Attach(GL_COLOR_ATTACHMENT0, GL_DRAW_FRAMEBUFFER);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        buffers = GL_COLOR_BUFFER_BIT;
    } else if (src_params.type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        src_view->Attach(GL_DEPTH_ATTACHMENT, GL_READ_FRAMEBUFFER);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        dst_view->Attach(GL_DEPTH_ATTACHMENT, GL_DRAW_FRAMEBUFFER);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        buffers = GL_DEPTH_BUFFER_BIT;
    } else if (src_params.type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        src_view->Attach(GL_DEPTH_STENCIL_ATTACHMENT, GL_READ_FRAMEBUFFER);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        dst_view->Attach(GL_DEPTH_STENCIL_ATTACHMENT, GL_DRAW_FRAMEBUFFER);

        buffers = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }

    const Common::Rectangle<u32>& src_rect = copy_config.src_rect;
    const Common::Rectangle<u32>& dst_rect = copy_config.dst_rect;
    const bool is_linear = copy_config.filter == Tegra::Engines::Fermi2D::Filter::Linear;

    glBlitFramebuffer(static_cast<GLint>(src_rect.left), static_cast<GLint>(src_rect.top),
                      static_cast<GLint>(src_rect.right), static_cast<GLint>(src_rect.bottom),
                      static_cast<GLint>(dst_rect.left), static_cast<GLint>(dst_rect.top),
                      static_cast<GLint>(dst_rect.right), static_cast<GLint>(dst_rect.bottom),
                      buffers,
                      is_linear && (buffers == GL_COLOR_BUFFER_BIT) ? GL_LINEAR : GL_NEAREST);
}

void TextureCacheOpenGL::BufferCopy(Surface& src_surface, Surface& dst_surface) {
    MICROPROFILE_SCOPE(OpenGL_Texture_Buffer_Copy);
    const auto& src_params = src_surface->GetSurfaceParams();
    const auto& dst_params = dst_surface->GetSurfaceParams();
    UNIMPLEMENTED_IF(src_params.num_levels > 1 || dst_params.num_levels > 1);

    const auto source_format = GetFormatTuple(src_params.pixel_format);
    const auto dest_format = GetFormatTuple(dst_params.pixel_format);

    const std::size_t source_size = src_surface->GetHostSizeInBytes();
    const std::size_t dest_size = dst_surface->GetHostSizeInBytes();

    const std::size_t buffer_size = std::max(source_size, dest_size);

    GLuint copy_pbo_handle = FetchPBO(buffer_size);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, copy_pbo_handle);

    if (source_format.compressed) {
        glGetCompressedTextureImage(src_surface->GetTexture(), 0, static_cast<GLsizei>(source_size),
                                    nullptr);
    } else {
        glGetTextureImage(src_surface->GetTexture(), 0, source_format.format, source_format.type,
                          static_cast<GLsizei>(source_size), nullptr);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, copy_pbo_handle);

    const GLsizei width = static_cast<GLsizei>(dst_params.width);
    const GLsizei height = static_cast<GLsizei>(dst_params.height);
    const GLsizei depth = static_cast<GLsizei>(dst_params.depth);
    if (dest_format.compressed) {
        LOG_CRITICAL(HW_GPU, "Compressed buffer copy is unimplemented!");
        UNREACHABLE();
    } else {
        switch (dst_params.target) {
        case SurfaceTarget::Texture1D:
            glTextureSubImage1D(dst_surface->GetTexture(), 0, 0, width, dest_format.format,
                                dest_format.type, nullptr);
            break;
        case SurfaceTarget::Texture2D:
            glTextureSubImage2D(dst_surface->GetTexture(), 0, 0, 0, width, height,
                                dest_format.format, dest_format.type, nullptr);
            break;
        case SurfaceTarget::Texture3D:
        case SurfaceTarget::Texture2DArray:
        case SurfaceTarget::TextureCubeArray:
            glTextureSubImage3D(dst_surface->GetTexture(), 0, 0, 0, 0, width, height, depth,
                                dest_format.format, dest_format.type, nullptr);
            break;
        case SurfaceTarget::TextureCubemap:
            glTextureSubImage3D(dst_surface->GetTexture(), 0, 0, 0, 0, width, height, depth,
                                dest_format.format, dest_format.type, nullptr);
            break;
        default:
            LOG_CRITICAL(Render_OpenGL, "Unimplemented surface target={}",
                         static_cast<u32>(dst_params.target));
            UNREACHABLE();
        }
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glTextureBarrier();
}

GLuint TextureCacheOpenGL::FetchPBO(std::size_t buffer_size) {
    ASSERT_OR_EXECUTE(buffer_size > 0, { return 0; });
    const u32 l2 = Common::Log2Ceil64(static_cast<u64>(buffer_size));
    OGLBuffer& cp = copy_pbo_cache[l2];
    if (cp.handle == 0) {
        const std::size_t ceil_size = 1ULL << l2;
        cp.Create();
        cp.MakeStreamCopy(ceil_size);
    }
    return cp.handle;
}

} // namespace OpenGL
