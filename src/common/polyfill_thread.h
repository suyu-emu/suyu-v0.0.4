// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

//
// TODO: remove this file when jthread is supported by all compilation targets
//

#pragma once

#include <chrono>
#include <condition_variable>
#include <thread>
#include <utility>

namespace Common {

template <typename Rep, typename Period>
bool StoppableTimedWait(std::stop_token token, const std::chrono::duration<Rep, Period>& rel_time) {
    std::condition_variable_any cv;
    std::mutex m;

    // Perform the timed wait.
    std::unique_lock lk{m};
    return !cv.wait_for(lk, token, rel_time, [&] { return token.stop_requested(); });
}

// suyu uses Common::CondvarWait — Eden removed it; restore as inline helper
template <typename Mutex, typename Pred>
void CondvarWait(std::condition_variable_any& cv, std::unique_lock<Mutex>& lk,
                 std::stop_token token, Pred pred) {
    cv.wait(lk, token, std::move(pred));
}

} // namespace Common
