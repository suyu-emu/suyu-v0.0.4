// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "core/file_sys/fssystem/fs_i_storage.h"
#include "core/file_sys/vfs/vfs.h"

namespace FileSys {

//TODO: No integrity verification.
class PassthroughStorage final : public IReadOnlyStorage {
    YUZU_NON_COPYABLE(PassthroughStorage);
    YUZU_NON_MOVEABLE(PassthroughStorage);

public:
    explicit PassthroughStorage(VirtualFile base) : base_(std::move(base)) {}
    ~PassthroughStorage() override = default;

    size_t Read(u8* buffer, size_t size, size_t offset) const override {
        if (!base_ || size == 0)
            return 0;
        return base_->Read(buffer, size, offset);
    }
    size_t GetSize() const override {
        return base_ ? base_->GetSize() : 0;
    }

private:
    VirtualFile base_{};
};

} // namespace FileSys
