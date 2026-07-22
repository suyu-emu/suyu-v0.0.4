// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <boost/container/static_vector.hpp>
#include "common/alignment.h"
#include "common/swap.h"
#include "core/file_sys/fssystem/fssystem_aes_xts_storage.h"
#include "core/file_sys/fssystem/fssystem_nca_header.h"
#include "core/file_sys/fssystem/fssystem_utility.h"

namespace FileSys {

void AesXtsStorage::MakeAesXtsIv(void* dst, size_t dst_size, s64 offset, size_t block_size) {
    ASSERT(dst != nullptr);
    ASSERT(dst_size == IvSize);
    ASSERT(offset >= 0);

    const uintptr_t out_addr = reinterpret_cast<uintptr_t>(dst);

    *reinterpret_cast<s64_be*>(out_addr + sizeof(s64)) = offset / block_size;
}

AesXtsStorage::AesXtsStorage(VirtualFile base, const void* key1, const void* key2, size_t key_size,
                             const void* iv, size_t iv_size, size_t block_size)
    : m_base_storage(std::move(base)), m_block_size(block_size), m_mutex() {
    ASSERT(m_base_storage != nullptr);
    ASSERT(key1 != nullptr);
    ASSERT(key2 != nullptr);
    ASSERT(iv != nullptr);
    ASSERT(key_size == KeySize);
    ASSERT(iv_size == IvSize);
    ASSERT(Common::IsAligned(m_block_size, AesBlockSize));

    std::memcpy(m_key.data() + 0, key1, KeySize / 2);
    std::memcpy(m_key.data() + 0x10, key2, KeySize / 2);
    std::memcpy(m_iv.data(), iv, IvSize);

    m_cipher.emplace(m_key, Core::Crypto::Mode::XTS);
}

size_t AesXtsStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Allow zero-size reads.
    if (size == 0)
        return size;

    // Ensure buffer is valid and we can only read at block aligned offsets.
    ASSERT(buffer != nullptr);
    ASSERT(Common::IsAligned(offset, AesBlockSize) && Common::IsAligned(size, AesBlockSize));
    m_base_storage->Read(buffer, size, offset);

    // Setup the counter.
    std::array<u8, IvSize> ctr;
    std::memcpy(ctr.data(), m_iv.data(), IvSize);
    AddCounter(ctr.data(), IvSize, offset / m_block_size);

    // Handle any unaligned data before the start; then read said data into a local pooled
    // buffer that resides on the stack, do not use the global memory allocator this is a
    // very tiny (512 bytes) buffer so should be fine to keep on the stack (Nca::XtsBlockSize wide buffer)
    size_t processed_size = 0;
    if ((offset % m_block_size) != 0) {
        // Decrypt into our pooled stack buffer (max bound = NCA::XtsBlockSize)
        boost::container::static_vector<u8, NcaHeader::XtsBlockSize> tmp_buf;
        ASSERT(m_block_size <= tmp_buf.max_size());
        tmp_buf.resize(m_block_size);
        // Determine the size of the pre-data read.
        auto const skip_size = size_t(offset - Common::AlignDown(offset, m_block_size));
        auto const data_size = (std::min)(size, m_block_size - skip_size);
        if (skip_size > 0)
            std::fill_n(tmp_buf.begin(), skip_size, u8{0});
        std::memcpy(tmp_buf.data() + skip_size, buffer, data_size);
        m_cipher->SetIV(ctr);
        m_cipher->Transcode(tmp_buf.data(), m_block_size, tmp_buf.data(), Core::Crypto::Op::Decrypt);
        std::memcpy(buffer, tmp_buf.data() + skip_size, data_size);

        AddCounter(ctr.data(), IvSize, 1);
        processed_size += data_size;
        ASSERT(processed_size == (std::min)(size, m_block_size - skip_size));
    }

    // Decrypt aligned chunks.
    auto* cur = buffer + processed_size;
    for (size_t remaining = size - processed_size; remaining > 0; ) {
        auto const cur_size = (std::min)(m_block_size, remaining);
        m_cipher->SetIV(ctr);
        auto* char_cur = reinterpret_cast<char*>(cur); //same repr cur - diff signedness
        m_cipher->Transcode(char_cur, cur_size, char_cur, Core::Crypto::Op::Decrypt);
        remaining -= cur_size;
        cur += cur_size;
        AddCounter(ctr.data(), IvSize, 1);
    }
    return size;
}

size_t AesXtsStorage::GetSize() const {
    return m_base_storage->GetSize();
}

} // namespace FileSys
