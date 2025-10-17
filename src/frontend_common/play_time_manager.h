// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include <common/polyfill_thread.h>

// TODO(crueter): Extract this into frontend_common
namespace Service::Account {
class ProfileManager;
}

namespace PlayTime {

using ProgramId = u64;
using PlayTime = u64;
using PlayTimeDatabase = std::map<ProgramId, PlayTime>;

class PlayTimeManager {
public:
    explicit PlayTimeManager();
    ~PlayTimeManager();

    YUZU_NON_COPYABLE(PlayTimeManager);
    YUZU_NON_MOVEABLE(PlayTimeManager);

    u64 GetPlayTime(u64 program_id) const;
    void ResetProgramPlayTime(u64 program_id);
    void SetProgramId(u64 program_id);
    void SetPlayTime(u64 program_id, u64 play_time);
    void Start();
    void Stop();

    static std::string GetReadablePlayTime(u64 time_seconds);
    static std::string GetPlayTimeHours(u64 time_seconds);
    static std::string GetPlayTimeMinutes(u64 time_seconds);
    static std::string GetPlayTimeSeconds(u64 time_seconds);

private:
    void AutoTimestamp(std::stop_token stop_token);
    void Save();

    PlayTimeDatabase database;
    u64 running_program_id;
    std::jthread play_time_thread;
};


} // namespace PlayTime
