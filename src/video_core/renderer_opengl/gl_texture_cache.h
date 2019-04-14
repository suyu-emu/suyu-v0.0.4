// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/texture_cache.h"

namespace OpenGL {

using VideoCommon::SurfaceParams;
using VideoCommon::ViewKey;
using VideoCore::Surface::ComponentType;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

class CachedSurfaceView;
class CachedSurface;

using TextureCacheBase = VideoCommon::TextureCacheContextless<CachedSurface, CachedSurfaceView>;

class CachedSurface final : public VideoCommon::SurfaceBaseContextless<CachedSurfaceView> {
    friend CachedSurfaceView;

public:
    explicit CachedSurface(const SurfaceParams& params);
    ~CachedSurface();

    void LoadBuffer();

    GLenum GetTarget() const {
        return target;
    }

    GLuint GetTexture() const {
        return texture.handle;
    }

protected:
    std::unique_ptr<CachedSurfaceView> CreateView(const ViewKey& view_key);

    void FlushBufferImpl();

    void UploadTextureImpl();

private:
    void UploadTextureMipmap(u32 level);

    GLenum internal_format{};
    GLenum format{};
    GLenum type{};
    bool is_compressed{};
    GLenum target{};

    OGLTexture texture;

    std::vector<u8> staging_buffer;
    u8* host_ptr{};
};

class CachedSurfaceView final {
public:
    explicit CachedSurfaceView(CachedSurface& surface, ViewKey key);
    ~CachedSurfaceView();

    GLuint GetTexture();

    GLuint GetTexture(Tegra::Shader::TextureType texture_type, bool is_array,
                      Tegra::Texture::SwizzleSource x_source,
                      Tegra::Texture::SwizzleSource y_source,
                      Tegra::Texture::SwizzleSource z_source,
                      Tegra::Texture::SwizzleSource w_source);

    void MarkAsModified(bool is_modified) {
        surface.MarkAsModified(is_modified);
    }

    const SurfaceParams& GetSurfaceParams() const {
        return params;
    }

private:
    struct TextureView {
        OGLTexture texture;
        std::array<Tegra::Texture::SwizzleSource, 4> swizzle{
            Tegra::Texture::SwizzleSource::R, Tegra::Texture::SwizzleSource::G,
            Tegra::Texture::SwizzleSource::B, Tegra::Texture::SwizzleSource::A};
    };

    void ApplySwizzle(TextureView& texture_view, Tegra::Texture::SwizzleSource x_source,
                      Tegra::Texture::SwizzleSource y_source,
                      Tegra::Texture::SwizzleSource z_source,
                      Tegra::Texture::SwizzleSource w_source);

    TextureView CreateTextureView(GLenum target) const;

    std::pair<std::reference_wrapper<TextureView>, GLenum> GetTextureView(
        Tegra::Shader::TextureType texture_type, bool is_array);

    CachedSurface& surface;
    const ViewKey key;
    const SurfaceParams params;

    TextureView texture_view_1d;
    TextureView texture_view_1d_array;
    TextureView texture_view_2d;
    TextureView texture_view_2d_array;
    TextureView texture_view_3d;
    TextureView texture_view_cube;
    TextureView texture_view_cube_array;
};

class TextureCacheOpenGL final : public TextureCacheBase {
public:
    explicit TextureCacheOpenGL(Core::System& system, VideoCore::RasterizerInterface& rasterizer);
    ~TextureCacheOpenGL();

protected:
    CachedSurfaceView* TryFastGetSurfaceView(VAddr cpu_addr, u8* host_ptr,
                                             const SurfaceParams& params, bool preserve_contents,
                                             const std::vector<CachedSurface*>& overlaps);

    std::unique_ptr<CachedSurface> CreateSurface(const SurfaceParams& params);

private:
    CachedSurfaceView* SurfaceCopy(VAddr cpu_addr, u8* host_ptr, const SurfaceParams& new_params,
                                   CachedSurface* old_surface, const SurfaceParams& old_params);
};

} // namespace OpenGL
