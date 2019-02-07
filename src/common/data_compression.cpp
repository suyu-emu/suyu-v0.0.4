// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <lz4.h>

#include "data_compression.h"

namespace Compression {


std::vector<u8> CompressDataLZ4(const u8* source, std::size_t source_size) {
    if (source_size > LZ4_MAX_INPUT_SIZE) {
        // Source size exceeds LZ4 maximum input size
        return {};
    }
    const auto source_size_int = static_cast<int>(source_size);
    const int max_compressed_size = LZ4_compressBound(source_size_int);
    std::vector<u8> compressed(max_compressed_size);
    const int compressed_size = LZ4_compress_default(reinterpret_cast<const char*>(source),
                                                     reinterpret_cast<char*>(compressed.data()),
                                                     source_size_int, max_compressed_size);
    if (compressed_size <= 0) {
        // Compression failed
        return {};
    }
    compressed.resize(compressed_size);
    return compressed;
}

std::vector<u8> DecompressDataLZ4(const std::vector<u8>& compressed, std::size_t uncompressed_size) {
    std::vector<u8> uncompressed(uncompressed_size);
    const int size_check = LZ4_decompress_safe(reinterpret_cast<const char*>(compressed.data()),
                                               reinterpret_cast<char*>(uncompressed.data()),
                                               static_cast<int>(compressed.size()),
                                               static_cast<int>(uncompressed.size()));
    if (static_cast<int>(uncompressed_size) != size_check) {
        // Decompression failed
        return {};
    }
    return uncompressed;
}

} // namespace Compression
