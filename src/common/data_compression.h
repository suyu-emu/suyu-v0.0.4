// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace Compression {

// Compresses a source memory region with LZ4 and returns the compressed data in an vector. If
// use_LZ4_high_compression is true, the LZ4 subalgortihmn LZ4HC is used with the highst possible
// compression level. This results in a smaller compressed size, but requires more CPU time for
// compression. Data compressed with LZ4HC can also be decompressed with the default LZ4
// decompression function.
std::vector<u8> CompressDataLZ4(const u8* source, std::size_t source_size,
                                bool use_LZ4_high_compression);

std::vector<u8> DecompressDataLZ4(const std::vector<u8>& compressed, std::size_t uncompressed_size);

} // namespace Compression