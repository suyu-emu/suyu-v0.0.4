// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <condition_variable>
#include <mutex>
#include <thread>

#include "core/core.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

namespace Kernel::Svc {

constexpr auto MAX_MSG_TIME = std::chrono::milliseconds(250);
const auto MAX_MSG_SIZE = 0x1000;

static std::string msg_buffer;
static std::mutex msg_mutex;
static std::condition_variable msg_cv;
static std::chrono::steady_clock::time_point last_msg_time;
static bool worker_running = true;
static std::unique_ptr<std::thread> flush_thread;
static std::once_flag start_flag;

static void FlushDbgLoop() {
    while (true) {
        std::unique_lock lock(msg_mutex);

        msg_cv.wait(lock, [] { return !msg_buffer.empty() || !worker_running; });
        if (!worker_running && msg_buffer.empty()) break;

        auto timeout = last_msg_time + MAX_MSG_TIME;
        bool woke_early = msg_cv.wait_until(lock, timeout, [] {
            return msg_buffer.size() >= MAX_MSG_SIZE || !worker_running;
        });

        if (!woke_early || msg_buffer.size() >= MAX_MSG_SIZE || !worker_running) {
            if (!msg_buffer.empty()) {
                // Remove trailing newline as LOG_INFO adds that anyways
                if (msg_buffer.back() == '\n')
                    msg_buffer.pop_back();

                LOG_INFO(Debug_Emulated, "\n{}", msg_buffer);
                msg_buffer.clear();
            }
            if (!worker_running) break;
        }
    }
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
Result OutputDebugString(Core::System& system, u64 address, u64 len) {
    R_SUCCEED_IF(len == 0);

    // Only start the thread the very first time this function is called
    std::call_once(start_flag, [] {
        flush_thread = std::make_unique<std::thread>(FlushDbgLoop);
    });

    {
        std::lock_guard lock(msg_mutex);
        const auto old_size = msg_buffer.size();
        msg_buffer.resize(old_size + len);
        GetCurrentMemory(system.Kernel()).ReadBlock(address, msg_buffer.data() + old_size, len);

        last_msg_time = std::chrono::steady_clock::now();
    }

    msg_cv.notify_one();
    R_SUCCEED();
}

Result OutputDebugString64(Core::System& system, uint64_t debug_str, uint64_t len) {
    R_RETURN(OutputDebugString(system, debug_str, len));
}

Result OutputDebugString64From32(Core::System& system, uint32_t debug_str, uint32_t len) {
    R_RETURN(OutputDebugString(system, debug_str, len));
}

struct BufferAutoFlush {
    ~BufferAutoFlush() {
        {
            std::lock_guard lock(msg_mutex);
            worker_running = false;
        }
        msg_cv.notify_all();
        if (flush_thread && flush_thread->joinable()) flush_thread->join();
    }
};
static BufferAutoFlush auto_flusher;

} // namespace Kernel::Svc
