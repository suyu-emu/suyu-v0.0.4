// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/fiber.h"
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <boost/context/detail/fcontext.hpp>
#endif

namespace Common {

#if defined(_WIN32) || defined(WIN32)

struct Fiber::FiberImpl {
    LPVOID handle = nullptr;
};

void Fiber::start() {
    ASSERT(previous_fiber != nullptr);
    previous_fiber->guard.unlock();
    previous_fiber.reset();
    entry_point(start_parameter);
    UNREACHABLE();
}

void __stdcall Fiber::FiberStartFunc(void* fiber_parameter) {
    auto fiber = static_cast<Fiber*>(fiber_parameter);
    fiber->start();
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : entry_point{std::move(entry_point_func)}, start_parameter{start_parameter} {
    impl = std::make_unique<FiberImpl>();
    impl->handle = CreateFiber(0, &FiberStartFunc, this);
}

Fiber::Fiber() {
    impl = std::make_unique<FiberImpl>();
}

Fiber::~Fiber() {
    // Make sure the Fiber is not being used
    bool locked = guard.try_lock();
    ASSERT_MSG(locked, "Destroying a fiber that's still running");
    if (locked) {
        guard.unlock();
    }
    DeleteFiber(impl->handle);
}

void Fiber::Exit() {
    ASSERT_MSG(is_thread_fiber, "Exitting non main thread fiber");
    if (!is_thread_fiber) {
        return;
    }
    ConvertFiberToThread();
    guard.unlock();
}

void Fiber::YieldTo(std::shared_ptr<Fiber> from, std::shared_ptr<Fiber> to) {
    ASSERT_MSG(from != nullptr, "Yielding fiber is null!");
    ASSERT_MSG(to != nullptr, "Next fiber is null!");
    to->guard.lock();
    to->previous_fiber = from;
    SwitchToFiber(to->impl->handle);
    auto previous_fiber = from->previous_fiber;
    ASSERT(previous_fiber != nullptr);
    previous_fiber->guard.unlock();
    previous_fiber.reset();
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->guard.lock();
    fiber->impl->handle = ConvertThreadToFiber(NULL);
    fiber->is_thread_fiber = true;
    return fiber;
}

#else
constexpr std::size_t default_stack_size = 1024 * 1024; // 1MB

struct Fiber::FiberImpl {
    alignas(64) std::array<u8, default_stack_size> stack;
    boost::context::detail::fcontext_t context;
};

void Fiber::start(boost::context::detail::transfer_t& transfer) {
    ASSERT(previous_fiber != nullptr);
    previous_fiber->impl->context = transfer.fctx;
    previous_fiber->guard.unlock();
    previous_fiber.reset();
    entry_point(start_parameter);
    UNREACHABLE();
}

void Fiber::FiberStartFunc(boost::context::detail::transfer_t transfer) {
    auto fiber = static_cast<Fiber*>(transfer.data);
    fiber->start(transfer);
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : guard{}, entry_point{std::move(entry_point_func)}, start_parameter{start_parameter},
      previous_fiber{} {
    impl = std::make_unique<FiberImpl>();
    u8* stack_limit = impl->stack.data();
    u8* stack_base = stack_limit + default_stack_size;
    impl->context =
        boost::context::detail::make_fcontext(stack_base, impl->stack.size(), FiberStartFunc);
}

Fiber::Fiber() {
    impl = std::make_unique<FiberImpl>();
}

Fiber::~Fiber() {
    // Make sure the Fiber is not being used
    bool locked = guard.try_lock();
    ASSERT_MSG(locked, "Destroying a fiber that's still running");
    if (locked) {
        guard.unlock();
    }
}

void Fiber::Exit() {
    ASSERT_MSG(is_thread_fiber, "Exitting non main thread fiber");
    if (!is_thread_fiber) {
        return;
    }
    guard.unlock();
}

void Fiber::YieldTo(std::shared_ptr<Fiber> from, std::shared_ptr<Fiber> to) {
    ASSERT_MSG(from != nullptr, "Yielding fiber is null!");
    ASSERT_MSG(to != nullptr, "Next fiber is null!");
    to->guard.lock();
    to->previous_fiber = from;
    auto transfer = boost::context::detail::jump_fcontext(to->impl->context, to.get());
    auto previous_fiber = from->previous_fiber;
    ASSERT(previous_fiber != nullptr);
    previous_fiber->impl->context = transfer.fctx;
    previous_fiber->guard.unlock();
    previous_fiber.reset();
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->guard.lock();
    fiber->is_thread_fiber = true;
    return fiber;
}

#endif
} // namespace Common
