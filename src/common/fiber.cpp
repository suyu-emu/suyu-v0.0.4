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

constexpr std::size_t default_stack_size = 256 * 1024; // 256kb

#if defined(_WIN32) || defined(WIN32)

struct Fiber::FiberImpl {
    LPVOID handle = nullptr;
    LPVOID rewind_handle = nullptr;
};

void Fiber::Start() {
    ASSERT(previous_fiber != nullptr);
    previous_fiber->guard.unlock();
    previous_fiber.reset();
    entry_point(start_parameter);
    UNREACHABLE();
}

void Fiber::OnRewind() {
    ASSERT(impl->handle != nullptr);
    DeleteFiber(impl->handle);
    impl->handle = impl->rewind_handle;
    impl->rewind_handle = nullptr;
    rewind_point(rewind_parameter);
    UNREACHABLE();
}

void Fiber::FiberStartFunc(void* fiber_parameter) {
    auto fiber = static_cast<Fiber*>(fiber_parameter);
    fiber->Start();
}

void Fiber::RewindStartFunc(void* fiber_parameter) {
    auto fiber = static_cast<Fiber*>(fiber_parameter);
    fiber->OnRewind();
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : entry_point{std::move(entry_point_func)}, start_parameter{start_parameter} {
    impl = std::make_unique<FiberImpl>();
    impl->handle = CreateFiber(default_stack_size, &FiberStartFunc, this);
}

Fiber::Fiber() : impl{std::make_unique<FiberImpl>()} {}

Fiber::~Fiber() {
    if (released) {
        return;
    }
    // Make sure the Fiber is not being used
    const bool locked = guard.try_lock();
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
    released = true;
}

void Fiber::SetRewindPoint(std::function<void(void*)>&& rewind_func, void* start_parameter) {
    rewind_point = std::move(rewind_func);
    rewind_parameter = start_parameter;
}

void Fiber::Rewind() {
    ASSERT(rewind_point);
    ASSERT(impl->rewind_handle == nullptr);
    impl->rewind_handle = CreateFiber(default_stack_size, &RewindStartFunc, this);
    SwitchToFiber(impl->rewind_handle);
}

void Fiber::YieldTo(std::shared_ptr<Fiber>& from, std::shared_ptr<Fiber>& to) {
    ASSERT_MSG(from != nullptr, "Yielding fiber is null!");
    ASSERT_MSG(to != nullptr, "Next fiber is null!");
    to->guard.lock();
    to->previous_fiber = from;
    SwitchToFiber(to->impl->handle);
    ASSERT(from->previous_fiber != nullptr);
    from->previous_fiber->guard.unlock();
    from->previous_fiber.reset();
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->guard.lock();
    fiber->impl->handle = ConvertThreadToFiber(nullptr);
    fiber->is_thread_fiber = true;
    return fiber;
}

#else

struct Fiber::FiberImpl {
    alignas(64) std::array<u8, default_stack_size> stack;
    alignas(64) std::array<u8, default_stack_size> rewind_stack;
    u8* stack_limit;
    u8* rewind_stack_limit;
    boost::context::detail::fcontext_t context;
    boost::context::detail::fcontext_t rewind_context;
};

void Fiber::Start(boost::context::detail::transfer_t& transfer) {
    ASSERT(previous_fiber != nullptr);
    previous_fiber->impl->context = transfer.fctx;
    previous_fiber->guard.unlock();
    previous_fiber.reset();
    entry_point(start_parameter);
    UNREACHABLE();
}

void Fiber::OnRewind([[maybe_unused]] boost::context::detail::transfer_t& transfer) {
    ASSERT(impl->context != nullptr);
    impl->context = impl->rewind_context;
    impl->rewind_context = nullptr;
    u8* tmp = impl->stack_limit;
    impl->stack_limit = impl->rewind_stack_limit;
    impl->rewind_stack_limit = tmp;
    rewind_point(rewind_parameter);
    UNREACHABLE();
}

void Fiber::FiberStartFunc(boost::context::detail::transfer_t transfer) {
    auto fiber = static_cast<Fiber*>(transfer.data);
    fiber->Start(transfer);
}

void Fiber::RewindStartFunc(boost::context::detail::transfer_t transfer) {
    auto fiber = static_cast<Fiber*>(transfer.data);
    fiber->OnRewind(transfer);
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : entry_point{std::move(entry_point_func)}, start_parameter{start_parameter} {
    impl = std::make_unique<FiberImpl>();
    impl->stack_limit = impl->stack.data();
    impl->rewind_stack_limit = impl->rewind_stack.data();
    u8* stack_base = impl->stack_limit + default_stack_size;
    impl->context =
        boost::context::detail::make_fcontext(stack_base, impl->stack.size(), FiberStartFunc);
}

void Fiber::SetRewindPoint(std::function<void(void*)>&& rewind_func, void* start_parameter) {
    rewind_point = std::move(rewind_func);
    rewind_parameter = start_parameter;
}

Fiber::Fiber() : impl{std::make_unique<FiberImpl>()} {}

Fiber::~Fiber() {
    if (released) {
        return;
    }
    // Make sure the Fiber is not being used
    const bool locked = guard.try_lock();
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
    released = true;
}

void Fiber::Rewind() {
    ASSERT(rewind_point);
    ASSERT(impl->rewind_context == nullptr);
    u8* stack_base = impl->rewind_stack_limit + default_stack_size;
    impl->rewind_context =
        boost::context::detail::make_fcontext(stack_base, impl->stack.size(), RewindStartFunc);
    boost::context::detail::jump_fcontext(impl->rewind_context, this);
}

void Fiber::YieldTo(std::shared_ptr<Fiber>& from, std::shared_ptr<Fiber>& to) {
    ASSERT_MSG(from != nullptr, "Yielding fiber is null!");
    ASSERT_MSG(to != nullptr, "Next fiber is null!");
    to->guard.lock();
    to->previous_fiber = from;
    auto transfer = boost::context::detail::jump_fcontext(to->impl->context, to.get());
    ASSERT(from->previous_fiber != nullptr);
    from->previous_fiber->impl->context = transfer.fctx;
    from->previous_fiber->guard.unlock();
    from->previous_fiber.reset();
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->guard.lock();
    fiber->is_thread_fiber = true;
    return fiber;
}

#endif
} // namespace Common
