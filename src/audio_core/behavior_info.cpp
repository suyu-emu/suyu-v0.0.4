// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "audio_core/behavior_info.h"
#include "audio_core/common.h"
#include "common/logging/log.h"

namespace AudioCore {

BehaviorInfo::BehaviorInfo() : process_revision(CURRENT_PROCESS_REVISION) {}
BehaviorInfo::~BehaviorInfo() = default;

bool BehaviorInfo::UpdateInput(const std::vector<u8>& buffer, std::size_t offset) {
    if (!CanConsumeBuffer(buffer.size(), offset, sizeof(InParams))) {
        LOG_ERROR(Audio, "Buffer is an invalid size!");
        return false;
    }
    InParams params{};
    std::memcpy(&params, buffer.data() + offset, sizeof(InParams));

    if (!IsValidRevision(params.revision)) {
        LOG_ERROR(Audio, "Invalid input revision, revision=0x{:08X}", params.revision);
        return false;
    }

    if (user_revision != params.revision) {
        LOG_ERROR(Audio,
                  "User revision differs from input revision, expecting 0x{:08X} but got 0x{:08X}",
                  user_revision, params.revision);
        return false;
    }

    ClearError();
    UpdateFlags(params.flags);

    // TODO(ogniK): Check input params size when InfoUpdater is used

    return true;
}

bool BehaviorInfo::UpdateOutput(std::vector<u8>& buffer, std::size_t offset) {
    if (!CanConsumeBuffer(buffer.size(), offset, sizeof(OutParams))) {
        LOG_ERROR(Audio, "Buffer is an invalid size!");
        return false;
    }

    OutParams params{};
    std::memcpy(params.errors.data(), errors.data(), sizeof(ErrorInfo) * errors.size());
    params.error_count = static_cast<u32_le>(error_count);
    std::memcpy(buffer.data() + offset, &params, sizeof(OutParams));
    return true;
}

void BehaviorInfo::ClearError() {
    error_count = 0;
}

void BehaviorInfo::UpdateFlags(u64_le dest_flags) {
    flags = dest_flags;
}

void BehaviorInfo::SetUserRevision(u32_le revision) {
    user_revision = revision;
}

bool BehaviorInfo::IsAdpcmLoopContextBugFixed() const {
    return IsRevisionSupported(2, user_revision);
}

bool BehaviorInfo::IsSplitterSupported() const {
    return IsRevisionSupported(2, user_revision);
}

bool BehaviorInfo::IsLongSizePreDelaySupported() const {
    return IsRevisionSupported(3, user_revision);
}

bool BehaviorInfo::IsAudioRenererProcessingTimeLimit80PercentSupported() const {
    return IsRevisionSupported(5, user_revision);
}

bool BehaviorInfo::IsAudioRenererProcessingTimeLimit75PercentSupported() const {
    return IsRevisionSupported(4, user_revision);
}

bool BehaviorInfo::IsAudioRenererProcessingTimeLimit70PercentSupported() const {
    return IsRevisionSupported(1, user_revision);
}

bool BehaviorInfo::IsElapsedFrameCountSupported() const {
    return IsRevisionSupported(5, user_revision);
}

bool BehaviorInfo::IsMemoryPoolForceMappingEnabled() const {
    return (flags & 1) != 0;
}

} // namespace AudioCore
