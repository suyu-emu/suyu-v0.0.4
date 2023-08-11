// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/lz4_compression.h"
#include "core/file_sys/fssystem/fssystem_compression_configuration.h"

namespace FileSys {

namespace {

Result DecompressLz4(void* dst, size_t dst_size, const void* src, size_t src_size) {
    auto result = Common::Compression::DecompressLZ4(dst, dst_size, src, src_size);
    R_UNLESS(static_cast<size_t>(result) == dst_size, ResultUnexpectedInCompressedStorageC);
    R_SUCCEED();
}

constexpr DecompressorFunction GetNcaDecompressorFunction(CompressionType type) {
    switch (type) {
    case CompressionType::Lz4:
        return DecompressLz4;
    default:
        return nullptr;
    }
}

constexpr NcaCompressionConfiguration g_nca_compression_configuration{
    .get_decompressor = GetNcaDecompressorFunction,
};

} // namespace

const NcaCompressionConfiguration* GetNcaCompressionConfiguration() {
    return std::addressof(g_nca_compression_configuration);
}

} // namespace FileSys
