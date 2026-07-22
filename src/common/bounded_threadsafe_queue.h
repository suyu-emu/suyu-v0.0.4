// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <new>

#include "common/polyfill_thread.h"

namespace Common {

namespace detail {
constexpr size_t DefaultCapacity = 0x1000;
} // namespace detail

template <typename T, size_t Capacity = detail::DefaultCapacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two.");

public:
    template <typename... Args>
    bool TryEmplace(Args&&... args) noexcept {
        return Emplace<PushMode::Try>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void EmplaceWait(Args&&... args) noexcept {
        Emplace<PushMode::Wait>(std::forward<Args>(args)...);
    }

    bool TryPop(T& t) noexcept {
        return Pop<PopMode::Try>(t);
    }

    void PopWait(T& t) noexcept {
        Pop<PopMode::Wait>(t);
    }

    void PopWait(T& t, const std::stop_token stop_token) noexcept {
        Pop<PopMode::WaitWithStopToken>(t, stop_token);
    }

    T PopWait() noexcept {
        T t{};
        Pop<PopMode::Wait>(t);
        return t;
    }

    T PopWait(const std::stop_token stop_token) noexcept {
        T t{};
        Pop<PopMode::WaitWithStopToken>(t, stop_token);
        return t;
    }

private:
    enum class PushMode {
        Try,
        Wait,
        Count,
    };

    enum class PopMode {
        Try,
        Wait,
        WaitWithStopToken,
        Count,
    };

    template <PushMode Mode, typename... Args>
    bool Emplace(Args&&... args) noexcept {
        const std::size_t write_index = producer.index.load(std::memory_order::relaxed);
        if constexpr (Mode == PushMode::Try) {
            // Check if we have free slots to write to.
            if ((write_index - consumer.index.load(std::memory_order::acquire)) == Capacity) {
                return false;
            }
        } else if constexpr (Mode == PushMode::Wait) {
            // Wait until we have free slots to write to.
            std::unique_lock lock{producer.cv_mutex};
            producer.cv.wait(lock, [this, write_index] {
                return (write_index - consumer.index.load(std::memory_order::acquire)) < Capacity;
            });
        } else {
            static_assert(Mode < PushMode::Count, "Invalid PushMode.");
        }
        // Determine the position to write to.
        const std::size_t pos = write_index % Capacity;
        // Emplace into the queue.
        std::construct_at(std::addressof(m_data[pos]), std::forward<Args>(args)...);
        // Increment the write index.
        ++producer.index;
        // Notify the consumer that we have pushed into the queue.
        std::scoped_lock lock{consumer.cv_mutex};
        consumer.cv.notify_one();
        return true;
    }

    template <PopMode Mode>
    bool Pop(T& t, [[maybe_unused]] std::stop_token stop_token = {}) noexcept {
        const std::size_t read_index = consumer.index.load(std::memory_order::relaxed);
        if constexpr (Mode == PopMode::Try) {
            // Check if the queue is empty.
            if (read_index == producer.index.load(std::memory_order::acquire)) {
                return false;
            }
        } else if constexpr (Mode == PopMode::Wait) {
            // Wait until the queue is not empty.
            std::unique_lock lock{consumer.cv_mutex};
            consumer.cv.wait(lock, [this, read_index] {
                return read_index != producer.index.load(std::memory_order::acquire);
            });
        } else if constexpr (Mode == PopMode::WaitWithStopToken) {
            // Wait until the queue is not empty.
            std::unique_lock lock{consumer.cv_mutex};
            consumer.cv.wait(lock, stop_token, [this, read_index] {
                return read_index != producer.index.load(std::memory_order::acquire);
            });
            if (stop_token.stop_requested()) {
                return false;
            }
        } else {
            static_assert(Mode < PopMode::Count, "Invalid PopMode.");
        }
        // Determine the position to read from.
        const std::size_t pos = read_index % Capacity;
        // Pop the data off the queue, moving it.
        t = std::move(m_data[pos]);
        // Increment the read index.
        ++consumer.index;
        // Notify the producer that we have popped off the queue.
        std::scoped_lock lock{producer.cv_mutex};
        producer.cv.notify_one();
        return true;
    }

    std::array<T, Capacity> m_data;
    alignas(64) struct {
        std::atomic_size_t index{0};
        std::condition_variable_any cv;
        std::mutex cv_mutex;
    } producer;
    alignas(64) struct {
        std::atomic_size_t index{0};
        std::condition_variable_any cv;
        std::mutex cv_mutex;
    } consumer;
};

template <typename T, size_t Capacity = detail::DefaultCapacity>
class MPSCQueue {
public:
    template <typename... Args>
    bool TryEmplace(Args&&... args) {
        std::scoped_lock lock{write_mutex};
        return spsc_queue.TryEmplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void EmplaceWait(Args&&... args) {
        std::scoped_lock lock{write_mutex};
        spsc_queue.EmplaceWait(std::forward<Args>(args)...);
    }

    bool TryPop(T& t) {
        return spsc_queue.TryPop(t);
    }

    void PopWait(T& t) {
        spsc_queue.PopWait(t);
    }

    void PopWait(T& t, std::stop_token stop_token) {
        spsc_queue.PopWait(t, stop_token);
    }

    T PopWait() {
        return spsc_queue.PopWait();
    }

    T PopWait(std::stop_token stop_token) {
        return spsc_queue.PopWait(stop_token);
    }

private:
    SPSCQueue<T, Capacity> spsc_queue;
    std::mutex write_mutex;
};

template <typename T, size_t Capacity = detail::DefaultCapacity>
class MPMCQueue {
public:
    template <typename... Args>
    bool TryEmplace(Args&&... args) {
        std::scoped_lock lock{write_mutex};
        return spsc_queue.TryEmplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void EmplaceWait(Args&&... args) {
        std::scoped_lock lock{write_mutex};
        spsc_queue.EmplaceWait(std::forward<Args>(args)...);
    }

    bool TryPop(T& t) {
        std::scoped_lock lock{read_mutex};
        return spsc_queue.TryPop(t);
    }

    void PopWait(T& t) {
        std::scoped_lock lock{read_mutex};
        spsc_queue.PopWait(t);
    }

    void PopWait(T& t, std::stop_token stop_token) {
        std::scoped_lock lock{read_mutex};
        spsc_queue.PopWait(t, stop_token);
    }

    T PopWait() {
        std::scoped_lock lock{read_mutex};
        return spsc_queue.PopWait();
    }

    T PopWait(std::stop_token stop_token) {
        std::scoped_lock lock{read_mutex};
        return spsc_queue.PopWait(stop_token);
    }

private:
    SPSCQueue<T, Capacity> spsc_queue;
    alignas(64) std::mutex write_mutex;
    alignas(64) std::mutex read_mutex;
};

} // namespace Common
