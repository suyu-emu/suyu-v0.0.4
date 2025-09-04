// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_nca_header.h"

namespace FileSys {

u8 NcaHeader::GetProperKeyGeneration() const {
    return std::max(this->key_generation, this->key_generation_2);
}

bool NcaPatchInfo::HasIndirectTable() const {
    static constexpr unsigned char BKTR[4] = {'B', 'K', 'T', 'R'};
    return std::memcmp(indirect_header.data(), BKTR, sizeof(BKTR)) == 0;
}

bool NcaPatchInfo::HasAesCtrExTable() const {
    static constexpr unsigned char BKTR[4] = {'B', 'K', 'T', 'R'};
    return std::memcmp(aes_ctr_ex_header.data(), BKTR, sizeof(BKTR)) == 0;
}

} // namespace FileSys
