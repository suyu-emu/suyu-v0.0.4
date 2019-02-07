// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace Compression {

std::vector<u8> CompressDataLZ4(const u8* source, std::size_t source_size);

std::vector<u8> DecompressDataLZ4(const std::vector<u8>& compressed, std::size_t uncompressed_size);

} // namespace Compression