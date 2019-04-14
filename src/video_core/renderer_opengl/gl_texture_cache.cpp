// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "common/scope_exit.h"
#include "video_core/morton.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/texture_cache.h"
#include "video_core/textures/convert.h"
#include "video_core/textures/texture.h"

namespace OpenGL {

using Tegra::Texture::ConvertFromGuestToHost;
using Tegra::Texture::SwizzleSource;
using VideoCore::MortonSwizzleMode;

namespace {

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;
    ComponentType component_type;
    bool compressed;
};

constexpr std::array<FormatTuple, VideoCore::Surface::MaxPixelFormat> tex_format_tuples = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ComponentType::UNorm, false}, // ABGR8U
    {GL_RGBA8, GL_RGBA, GL_BYTE, ComponentType::SNorm, false},                     // ABGR8S
    {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, ComponentType::UInt, false},   // ABGR8UI
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, ComponentType::UNorm, false}, // B5G6R5U
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, ComponentType::UNorm,
     false}, // A2B10G10R10U
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, ComponentType::UNorm, false}, // A1B5G5R5U
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},                    // R8U
    {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ComponentType::UInt, false},           // R8UI
    {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, ComponentType::Float, false},                 // RGBA16F
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, ComponentType::UNorm, false},              // RGBA16U
    {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, ComponentType::UInt, false},     // RGBA16UI
    {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, ComponentType::Float,
     false},                                                                     // R11FG11FB10F
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, ComponentType::UInt, false}, // RGBA32UI
    {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // DXT1
    {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // DXT23
    {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true},                                                                                 // DXT45
    {GL_COMPRESSED_RED_RGTC1, GL_RED, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm, true}, // DXN1
    {GL_COMPRESSED_RG_RGTC2, GL_RG, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true},                                                                     // DXN2UNORM
    {GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, GL_INT, ComponentType::SNorm, true}, // DXN2SNORM
    {GL_COMPRESSED_RGBA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // BC7U
    {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, GL_RGB, GL_UNSIGNED_INT_8_8_8_8, ComponentType::Float,
     true}, // BC6H_UF16
    {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, GL_RGB, GL_UNSIGNED_INT_8_8_8_8, ComponentType::Float,
     true},                                                                    // BC6H_SF16
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_4X4
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // BGRA8
    {GL_RGBA32F, GL_RGBA, GL_FLOAT, ComponentType::Float, false},              // RGBA32F
    {GL_RG32F, GL_RG, GL_FLOAT, ComponentType::Float, false},                  // RG32F
    {GL_R32F, GL_RED, GL_FLOAT, ComponentType::Float, false},                  // R32F
    {GL_R16F, GL_RED, GL_HALF_FLOAT, ComponentType::Float, false},             // R16F
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT, ComponentType::UNorm, false},          // R16U
    {GL_R16_SNORM, GL_RED, GL_SHORT, ComponentType::SNorm, false},             // R16S
    {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, ComponentType::UInt, false}, // R16UI
    {GL_R16I, GL_RED_INTEGER, GL_SHORT, ComponentType::SInt, false},           // R16I
    {GL_RG16, GL_RG, GL_UNSIGNED_SHORT, ComponentType::UNorm, false},          // RG16
    {GL_RG16F, GL_RG, GL_HALF_FLOAT, ComponentType::Float, false},             // RG16F
    {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, ComponentType::UInt, false}, // RG16UI
    {GL_RG16I, GL_RG_INTEGER, GL_SHORT, ComponentType::SInt, false},           // RG16I
    {GL_RG16_SNORM, GL_RG, GL_SHORT, ComponentType::SNorm, false},             // RG16S
    {GL_RGB32F, GL_RGB, GL_FLOAT, ComponentType::Float, false},                // RGB32F
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ComponentType::UNorm,
     false},                                                                   // RGBA8_SRGB
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},            // RG8U
    {GL_RG8, GL_RG, GL_BYTE, ComponentType::SNorm, false},                     // RG8S
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, ComponentType::UInt, false},   // RG32UI
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, ComponentType::UInt, false},   // R32UI
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_8X8
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_8X5
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_5X4
    {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // BGRA8
    // Compressed sRGB formats
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // DXT1_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // DXT23_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true}, // DXT45_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, ComponentType::UNorm,
     true},                                                                    // BC7U_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_4X4_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_8X8_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_8X5_SRGB
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_5X4_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_5X5
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_5X5_SRGB
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false},        // ASTC_2D_10X8
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, ComponentType::UNorm, false}, // ASTC_2D_10X8_SRGB

    // Depth formats
    {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, ComponentType::Float, false}, // Z32F
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, ComponentType::UNorm,
     false}, // Z16

    // DepthStencil formats
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, ComponentType::UNorm,
     false}, // Z24S8
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, ComponentType::UNorm,
     false}, // S8Z24
    {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
     ComponentType::Float, false}, // Z32FS8
}};

const FormatTuple& GetFormatTuple(PixelFormat pixel_format, ComponentType component_type) {
    ASSERT(static_cast<std::size_t>(pixel_format) < tex_format_tuples.size());
    const auto& format{tex_format_tuples[static_cast<std::size_t>(pixel_format)]};
    ASSERT(component_type == format.component_type);
    return format;
}

GLenum GetTextureTarget(const SurfaceParams& params) {
    switch (params.GetTarget()) {
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

void ApplyTextureDefaults(const SurfaceParams& params, GLuint texture) {
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, params.GetNumLevels() - 1);
    if (params.GetNumLevels() == 1) {
        glTextureParameterf(texture, GL_TEXTURE_LOD_BIAS, 1000.0f);
    }
}

OGLTexture CreateTexture(const SurfaceParams& params, GLenum target, GLenum internal_format) {
    OGLTexture texture;
    texture.Create(target);

    switch (params.GetTarget()) {
    case SurfaceTarget::Texture1D:
        glTextureStorage1D(texture.handle, params.GetNumLevels(), internal_format,
                           params.GetWidth());
        break;
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::TextureCubemap:
        glTextureStorage2D(texture.handle, params.GetNumLevels(), internal_format,
                           params.GetWidth(), params.GetHeight());
        break;
    case SurfaceTarget::Texture3D:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubeArray:
        glTextureStorage3D(texture.handle, params.GetNumLevels(), internal_format,
                           params.GetWidth(), params.GetHeight(), params.GetDepth());
        break;
    default:
        UNREACHABLE();
    }

    ApplyTextureDefaults(params, texture.handle);

    return texture;
}

void SwizzleFunc(MortonSwizzleMode mode, u8* memory, const SurfaceParams& params, u8* buffer,
                 u32 level) {
    const u32 width{params.GetMipWidth(level)};
    const u32 height{params.GetMipHeight(level)};
    const u32 block_height{params.GetMipBlockHeight(level)};
    const u32 block_depth{params.GetMipBlockDepth(level)};

    std::size_t guest_offset{params.GetGuestMipmapLevelOffset(level)};
    if (params.IsLayered()) {
        std::size_t host_offset{0};
        const std::size_t guest_stride = params.GetGuestLayerSize();
        const std::size_t host_stride = params.GetHostLayerSize(level);
        for (u32 layer = 0; layer < params.GetNumLayers(); layer++) {
            MortonSwizzle(mode, params.GetPixelFormat(), width, block_height, height, block_depth,
                          1, params.GetTileWidthSpacing(), buffer + host_offset,
                          memory + guest_offset);
            guest_offset += guest_stride;
            host_offset += host_stride;
        }
    } else {
        MortonSwizzle(mode, params.GetPixelFormat(), width, block_height, height, block_depth,
                      params.GetMipDepth(level), params.GetTileWidthSpacing(), buffer,
                      memory + guest_offset);
    }
}

} // Anonymous namespace

CachedSurface::CachedSurface(const SurfaceParams& params)
    : VideoCommon::SurfaceBaseContextless<CachedSurfaceView>{params} {
    const auto& tuple{GetFormatTuple(params.GetPixelFormat(), params.GetComponentType())};
    internal_format = tuple.internal_format;
    format = tuple.format;
    type = tuple.type;
    is_compressed = tuple.compressed;
    target = GetTextureTarget(params);
    texture = CreateTexture(params, target, internal_format);
    staging_buffer.resize(params.GetHostSizeInBytes());
}

CachedSurface::~CachedSurface() = default;

void CachedSurface::LoadBuffer() {
    if (params.IsTiled()) {
        ASSERT_MSG(params.GetBlockWidth() == 1, "Block width is defined as {} on texture target {}",
                   params.GetBlockWidth(), static_cast<u32>(params.GetTarget()));
        for (u32 level = 0; level < params.GetNumLevels(); ++level) {
            u8* const buffer{staging_buffer.data() + params.GetHostMipmapLevelOffset(level)};
            SwizzleFunc(MortonSwizzleMode::MortonToLinear, GetHostPtr(), params, buffer, level);
        }
    } else {
        ASSERT_MSG(params.GetNumLevels() == 1, "Linear mipmap loading is not implemented");
        const u32 bpp{GetFormatBpp(params.GetPixelFormat()) / CHAR_BIT};
        const u32 block_width{VideoCore::Surface::GetDefaultBlockWidth(params.GetPixelFormat())};
        const u32 block_height{VideoCore::Surface::GetDefaultBlockHeight(params.GetPixelFormat())};
        const u32 width{(params.GetWidth() + block_width - 1) / block_width};
        const u32 height{(params.GetHeight() + block_height - 1) / block_height};
        const u32 copy_size{width * bpp};
        if (params.GetPitch() == copy_size) {
            std::memcpy(staging_buffer.data(), GetHostPtr(), params.GetHostSizeInBytes());
        } else {
            const u8* start{GetHostPtr()};
            u8* write_to{staging_buffer.data()};
            for (u32 h = height; h > 0; --h) {
                std::memcpy(write_to, start, copy_size);
                start += params.GetPitch();
                write_to += copy_size;
            }
        }
    }

    for (u32 level = 0; level < params.GetNumLevels(); ++level) {
        ConvertFromGuestToHost(staging_buffer.data() + params.GetHostMipmapLevelOffset(level),
                               params.GetPixelFormat(), params.GetMipWidth(level),
                               params.GetMipHeight(level), params.GetMipDepth(level), true, true);
    }
}

void CachedSurface::FlushBufferImpl() {
    if (!IsModified()) {
        return;
    }

    // TODO(Rodrigo): Optimize alignment
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    SCOPE_EXIT({ glPixelStorei(GL_PACK_ROW_LENGTH, 0); });

    for (u32 level = 0; level < params.GetNumLevels(); ++level) {
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(params.GetMipWidth(level)));
        if (is_compressed) {
            glGetCompressedTextureImage(
                texture.handle, level, static_cast<GLsizei>(params.GetHostMipmapSize(level)),
                staging_buffer.data() + params.GetHostMipmapLevelOffset(level));
        } else {
            glGetTextureImage(texture.handle, level, format, type,
                              static_cast<GLsizei>(params.GetHostMipmapSize(level)),
                              staging_buffer.data() + params.GetHostMipmapLevelOffset(level));
        }
    }

    if (params.IsTiled()) {
        ASSERT_MSG(params.GetBlockWidth() == 1, "Block width is defined as {}",
                   params.GetBlockWidth());
        for (u32 level = 0; level < params.GetNumLevels(); ++level) {
            u8* const buffer = staging_buffer.data() + params.GetHostMipmapLevelOffset(level);
            SwizzleFunc(MortonSwizzleMode::LinearToMorton, GetHostPtr(), params, buffer, level);
        }
    } else {
        UNIMPLEMENTED();
        /*
        ASSERT(params.GetTarget() == SurfaceTarget::Texture2D);
        ASSERT(params.GetNumLevels() == 1);

        const u32 bpp{params.GetFormatBpp() / 8};
        const u32 copy_size{params.GetWidth() * bpp};
        if (params.GetPitch() == copy_size) {
            std::memcpy(host_ptr, staging_buffer.data(), GetSizeInBytes());
        } else {
            u8* start{host_ptr};
            const u8* read_to{staging_buffer.data()};
            for (u32 h = params.GetHeight(); h > 0; --h) {
                std::memcpy(start, read_to, copy_size);
                start += params.GetPitch();
                read_to += copy_size;
            }
        }
        */
    }
}

void CachedSurface::UploadTextureImpl() {
    for (u32 level = 0; level < params.GetNumLevels(); ++level) {
        UploadTextureMipmap(level);
    }
}

void CachedSurface::UploadTextureMipmap(u32 level) {
    u8* buffer{staging_buffer.data() + params.GetHostMipmapLevelOffset(level)};

    // TODO(Rodrigo): Optimize alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(params.GetMipWidth(level)));
    SCOPE_EXIT({ glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); });

    if (is_compressed) {
        const auto image_size{static_cast<GLsizei>(params.GetHostMipmapSize(level))};
        GLint expected_size;
        glGetTextureLevelParameteriv(texture.handle, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE,
                                     &expected_size);
        switch (params.GetTarget()) {
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
            for (std::size_t face = 0; face < params.GetDepth(); ++face) {
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
        switch (params.GetTarget()) {
        case SurfaceTarget::Texture1D:
            glTextureSubImage1D(texture.handle, level, 0, params.GetMipWidth(level), format, type,
                                buffer);
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
            for (std::size_t face = 0; face < params.GetDepth(); ++face) {
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

std::unique_ptr<CachedSurfaceView> CachedSurface::CreateView(const ViewKey& view_key) {
    return std::make_unique<CachedSurfaceView>(*this, view_key);
}

CachedSurfaceView::CachedSurfaceView(CachedSurface& surface, ViewKey key)
    : surface{surface}, key{key}, params{surface.GetSurfaceParams()} {}

CachedSurfaceView::~CachedSurfaceView() = default;

void CachedSurfaceView::Attach(GLenum attachment) const {
    ASSERT(key.num_layers == 1 && key.num_levels == 1);

    switch (params.GetTarget()) {
    case SurfaceTarget::Texture1D:
        glFramebufferTexture1D(GL_DRAW_FRAMEBUFFER, attachment, surface.GetTarget(),
                               surface.GetTexture(), key.base_level);
        break;
    case SurfaceTarget::Texture2D:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, surface.GetTarget(),
                               surface.GetTexture(), key.base_level);
        break;
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, surface.GetTexture(),
                                  key.base_level, key.base_layer);
        break;
    default:
        UNIMPLEMENTED();
    }
}

GLuint CachedSurfaceView::GetTexture(Tegra::Shader::TextureType texture_type, bool is_array,
                                     SwizzleSource x_source, SwizzleSource y_source,
                                     SwizzleSource z_source, SwizzleSource w_source) {
    const auto [texture_view, target] = GetTextureView(texture_type, is_array);
    if (texture_view.get().texture.handle == 0) {
        texture_view.get() = std::move(CreateTextureView(target));
    }
    ApplySwizzle(texture_view, x_source, y_source, z_source, w_source);
    return texture_view.get().texture.handle;
}

void CachedSurfaceView::ApplySwizzle(TextureView& texture_view, SwizzleSource x_source,
                                     SwizzleSource y_source, SwizzleSource z_source,
                                     SwizzleSource w_source) {
    const std::array<SwizzleSource, 4> swizzle = {x_source, y_source, z_source, w_source};
    if (swizzle == texture_view.swizzle) {
        return;
    }
    const std::array<GLint, 4> gl_swizzle = {GetSwizzleSource(x_source), GetSwizzleSource(y_source),
                                             GetSwizzleSource(z_source),
                                             GetSwizzleSource(w_source)};
    glTextureParameteriv(texture_view.texture.handle, GL_TEXTURE_SWIZZLE_RGBA, gl_swizzle.data());
    texture_view.swizzle = swizzle;
}

CachedSurfaceView::TextureView CachedSurfaceView::CreateTextureView(GLenum target) const {
    TextureView texture_view;
    glGenTextures(1, &texture_view.texture.handle);

    const GLuint handle{texture_view.texture.handle};
    const FormatTuple& tuple{GetFormatTuple(params.GetPixelFormat(), params.GetComponentType())};

    glTextureView(handle, target, surface.texture.handle, tuple.internal_format, key.base_level,
                  key.num_levels, key.base_layer, key.num_layers);
    ApplyTextureDefaults(params, handle);

    return texture_view;
}

std::pair<std::reference_wrapper<CachedSurfaceView::TextureView>, GLenum>
CachedSurfaceView::GetTextureView(Tegra::Shader::TextureType texture_type, bool is_array) {
    using Pair = std::pair<std::reference_wrapper<TextureView>, GLenum>;
    switch (texture_type) {
    case Tegra::Shader::TextureType::Texture1D:
        return is_array ? Pair{texture_view_1d_array, GL_TEXTURE_1D_ARRAY}
                        : Pair{texture_view_1d, GL_TEXTURE_1D};
    case Tegra::Shader::TextureType::Texture2D:
        return is_array ? Pair{texture_view_2d_array, GL_TEXTURE_2D_ARRAY}
                        : Pair{texture_view_2d, GL_TEXTURE_2D};
    case Tegra::Shader::TextureType::Texture3D:
        ASSERT(!is_array);
        return {texture_view_3d, GL_TEXTURE_3D};
    case Tegra::Shader::TextureType::TextureCube:
        return is_array ? Pair{texture_view_cube_array, GL_TEXTURE_CUBE_MAP_ARRAY}
                        : Pair{texture_view_cube, GL_TEXTURE_CUBE_MAP};
    }
    UNREACHABLE();
}

TextureCacheOpenGL::TextureCacheOpenGL(Core::System& system,
                                       VideoCore::RasterizerInterface& rasterizer)
    : TextureCacheBase{system, rasterizer} {}

TextureCacheOpenGL::~TextureCacheOpenGL() = default;

CachedSurfaceView* TextureCacheOpenGL::TryFastGetSurfaceView(
    VAddr cpu_addr, u8* host_ptr, const SurfaceParams& params, bool preserve_contents,
    const std::vector<CachedSurface*>& overlaps) {
    if (overlaps.size() > 1) {
        return nullptr;
    }

    const auto& old_surface{overlaps[0]};
    const auto& old_params{old_surface->GetSurfaceParams()};
    const auto& new_params{params};

    if (old_params.GetTarget() == new_params.GetTarget() &&
        old_params.GetDepth() == new_params.GetDepth() && old_params.GetDepth() == 1 &&
        old_params.GetNumLevels() == new_params.GetNumLevels() &&
        old_params.GetPixelFormat() == new_params.GetPixelFormat()) {
        return SurfaceCopy(cpu_addr, host_ptr, new_params, old_surface, old_params);
    }

    return nullptr;
}

CachedSurfaceView* TextureCacheOpenGL::SurfaceCopy(VAddr cpu_addr, u8* host_ptr,
                                                   const SurfaceParams& new_params,
                                                   CachedSurface* old_surface,
                                                   const SurfaceParams& old_params) {
    CachedSurface* const new_surface{GetUncachedSurface(new_params)};
    Register(new_surface, cpu_addr, host_ptr);

    const u32 min_width{
        std::max(old_params.GetDefaultBlockWidth(), new_params.GetDefaultBlockWidth())};
    const u32 min_height{
        std::max(old_params.GetDefaultBlockHeight(), new_params.GetDefaultBlockHeight())};
    for (u32 level = 0; level < old_params.GetNumLevels(); ++level) {
        const u32 width{std::min(old_params.GetMipWidth(level), new_params.GetMipWidth(level))};
        const u32 height{std::min(old_params.GetMipHeight(level), new_params.GetMipHeight(level))};
        if (width < min_width || height < min_height) {
            // Avoid copies that are too small to be handled in OpenGL
            break;
        }
        glCopyImageSubData(old_surface->GetTexture(), old_surface->GetTarget(), level, 0, 0, 0,
                           new_surface->GetTexture(), new_surface->GetTarget(), level, 0, 0, 0,
                           width, height, 1);
    }

    new_surface->MarkAsModified(true);

    // TODO(Rodrigo): Add an entry to directly get the superview
    return new_surface->GetView(cpu_addr, new_params);
}

std::unique_ptr<CachedSurface> TextureCacheOpenGL::CreateSurface(const SurfaceParams& params) {
    return std::make_unique<CachedSurface>(params);
}

} // namespace OpenGL
