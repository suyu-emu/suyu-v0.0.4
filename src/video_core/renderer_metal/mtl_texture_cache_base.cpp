// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_metal/mtl_texture_cache.h"
#include "video_core/texture_cache/texture_cache.h"

namespace VideoCommon {
template class VideoCommon::TextureCache<Metal::TextureCacheParams>;
}
