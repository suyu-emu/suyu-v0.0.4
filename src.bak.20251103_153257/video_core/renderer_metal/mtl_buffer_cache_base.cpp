// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/renderer_metal/mtl_buffer_cache.h"

namespace VideoCommon {
template class VideoCommon::BufferCache<Metal::BufferCacheParams>;
}
