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
class TextureCacheOpenGL;

using Surface = std::shared_ptr<CachedSurface>;
using TextureCacheBase = VideoCommon::TextureCacheContextless<CachedSurface, CachedSurfaceView>;

class CachedSurface final
    : public VideoCommon::SurfaceBaseContextless<TextureCacheOpenGL, CachedSurfaceView> {
    friend CachedSurfaceView;

public:
    explicit CachedSurface(TextureCacheOpenGL& texture_cache, const SurfaceParams& params);
    ~CachedSurface();

    GLenum GetTarget() const {
        return target;
    }

    GLuint GetTexture() const {
        return texture.handle;
    }

protected:
    void DecorateSurfaceName();

    std::unique_ptr<CachedSurfaceView> CreateView(const ViewKey& view_key);

    void UploadTextureImpl();
    void DownloadTextureImpl();

private:
    void UploadTextureMipmap(u32 level);

    GLenum internal_format{};
    GLenum format{};
    GLenum type{};
    bool is_compressed{};
    GLenum target{};

    OGLTexture texture;
};

class CachedSurfaceView final {
public:
    explicit CachedSurfaceView(CachedSurface& surface, ViewKey key);
    ~CachedSurfaceView();

    /// Attaches this texture view to the current bound GL_DRAW_FRAMEBUFFER
    void Attach(GLenum attachment) const;

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

    u32 GetWidth() const {
        return params.GetMipWidth(GetBaseLevel());
    }

    u32 GetHeight() const {
        return params.GetMipHeight(GetBaseLevel());
    }

    u32 GetDepth() const {
        return params.GetMipDepth(GetBaseLevel());
    }

    u32 GetBaseLayer() const {
        return key.base_layer;
    }

    u32 GetNumLayers() const {
        return key.num_layers;
    }

    u32 GetBaseLevel() const {
        return key.base_level;
    }

    u32 GetNumLevels() const {
        return key.num_levels;
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
    CachedSurfaceView* TryFastGetSurfaceView(GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr,
                                             const SurfaceParams& new_params,
                                             bool preserve_contents,
                                             const std::vector<Surface>& overlaps);

    Surface CreateSurface(const SurfaceParams& params);

private:
    CachedSurfaceView* SurfaceCopy(GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr,
                                   const SurfaceParams& new_params, const Surface& old_surface,
                                   const SurfaceParams& old_params);

    CachedSurfaceView* TryCopyAsViews(GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr,
                                      const SurfaceParams& new_params,
                                      const std::vector<Surface>& overlaps);
};

} // namespace OpenGL
