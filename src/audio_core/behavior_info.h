// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace AudioCore {
class BehaviorInfo {
public:
    explicit BehaviorInfo();
    ~BehaviorInfo();

    bool UpdateInput(const std::vector<u8>& buffer, std::size_t offset);
    bool UpdateOutput(std::vector<u8>& buffer, std::size_t offset);

    void ClearError();
    void UpdateFlags(u64_le dest_flags);
    void SetUserRevision(u32_le revision);

    bool IsAdpcmLoopContextBugFixed() const;
    bool IsSplitterSupported() const;
    bool IsLongSizePreDelaySupported() const;
    bool IsAudioRenererProcessingTimeLimit80PercentSupported() const;
    bool IsAudioRenererProcessingTimeLimit75PercentSupported() const;
    bool IsAudioRenererProcessingTimeLimit70PercentSupported() const;
    bool IsElapsedFrameCountSupported() const;
    bool IsMemoryPoolForceMappingEnabled() const;

private:
    u32_le process_revision{};
    u32_le user_revision{};
    u64_le flags{};

    struct ErrorInfo {
        u32_le result{};
        INSERT_PADDING_WORDS(1);
        u64_le result_info{};
    };
    static_assert(sizeof(ErrorInfo) == 0x10, "ErrorInfo is an invalid size");

    std::array<ErrorInfo, 10> errors{};
    std::size_t error_count{};

    struct InParams {
        u32_le revision{};
        u32_le padding{};
        u64_le flags{};
    };
    static_assert(sizeof(InParams) == 0x10, "InParams is an invalid size");

    struct OutParams {
        std::array<ErrorInfo, 10> errors{};
        u32_le error_count{};
        INSERT_PADDING_BYTES(12);
    };
    static_assert(sizeof(OutParams) == 0xb0, "OutParams is an invalid size");
};

} // namespace AudioCore
