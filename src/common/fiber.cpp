// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

#include "common/assert.h"
#include "common/fiber.h"
#define MCO_USE_VMEM_ALLOCATOR
#define MINICORO_IMPL
#include "common/minicoro.h"

namespace Common {

struct Fiber::FiberImpl {
    FiberImpl() {}

    std::mutex guard;
    bool released{};
    bool is_thread_fiber{};
    Fiber* next_fiber{};
    Fiber** next_fiber_ptr;
    std::function<void()> entry_point;

    mco_coro* context;
};

Fiber::Fiber() : impl{std::make_unique<FiberImpl>()} {
    impl->is_thread_fiber = true;
}

Fiber::Fiber(std::function<void()>&& entry_point_func) : impl{std::make_unique<FiberImpl>()} {
    impl->entry_point = std::move(entry_point_func);
    auto desc = mco_desc_init(
        [](mco_coro* coro) { reinterpret_cast<Fiber*>(coro->user_data)->impl->entry_point(); }, 0);
    desc.user_data = this;
    mco_result res = mco_create(&impl->context, &desc);
    ASSERT(res == MCO_SUCCESS);
}

Fiber::~Fiber() {
    if (impl->released) {
        return;
    }
    DestroyPre();
    if (impl->is_thread_fiber) {
        DestroyThreadFiber();
    } else {
        DestroyWorkFiber();
    }
}

void Fiber::Exit() {
    ASSERT_MSG(impl->is_thread_fiber, "Exiting non main thread fiber");
    if (!impl->is_thread_fiber) {
        return;
    }
    DestroyPre();
    DestroyThreadFiber();
}

void Fiber::DestroyPre() {
    // Make sure the Fiber is not being used
    const bool locked = impl->guard.try_lock();
    ASSERT_MSG(locked, "Destroying a fiber that's still running");
    if (locked) {
        impl->guard.unlock();
    }
    impl->released = true;
}

void Fiber::DestroyWorkFiber() {
    mco_result res = mco_destroy(impl->context);
    ASSERT(res == MCO_SUCCESS);
}

void Fiber::DestroyThreadFiber() {
    if (*impl->next_fiber_ptr) {
        *impl->next_fiber_ptr = nullptr;
    }
}

void Fiber::YieldTo(std::weak_ptr<Fiber> weak_from, Fiber& to) {
    if (auto from = weak_from.lock()) {
        if (!from->impl->is_thread_fiber) {
            // Set next fiber
            from->impl->next_fiber = &to;
            // Yield from thread
            if (!from->impl->released) {
                from->impl->guard.unlock();
                mco_yield(from->impl->context);
            }
        } else {
            from->impl->guard.lock();
            // Keep running next fiber until they've ran out
            auto& next_fiber_ptr = from->impl->next_fiber_ptr;
            next_fiber_ptr = &from->impl->next_fiber;
            *next_fiber_ptr = &to;
            for ([[maybe_unused]] unsigned round = 0; *next_fiber_ptr; round++) {
                auto next = *next_fiber_ptr;
                *next_fiber_ptr = nullptr;
                next_fiber_ptr = &next->impl->next_fiber;
                // Stop if new thread is thread fiber
                if (next->impl->is_thread_fiber)
                    break;
                // Resume new thread
                next->impl->guard.lock();
                mco_result res = mco_resume(next->impl->context);
                ASSERT(res == MCO_SUCCESS);
            }
            from->impl->guard.unlock();
        }
    }
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    return std::shared_ptr<Fiber>{new Fiber()};
}

} // namespace Common
