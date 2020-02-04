// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/fiber.h"

namespace Common {

#ifdef _MSC_VER
#include <windows.h>

struct Fiber::FiberImpl {
    LPVOID handle = nullptr;
};

void Fiber::_start([[maybe_unused]] void* parameter) {
    guard.lock();
    if (previous_fiber) {
        previous_fiber->guard.unlock();
        previous_fiber = nullptr;
    }
    entry_point(start_parameter);
}

static void __stdcall FiberStartFunc(LPVOID lpFiberParameter)
{
   auto fiber = static_cast<Fiber *>(lpFiberParameter);
   fiber->_start(nullptr);
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : guard{}, entry_point{std::move(entry_point_func)}, start_parameter{start_parameter}, previous_fiber{} {
    impl = std::make_unique<FiberImpl>();
    impl->handle = CreateFiber(0, &FiberStartFunc, this);
}

Fiber::Fiber() : guard{}, entry_point{}, start_parameter{}, previous_fiber{} {
    impl = std::make_unique<FiberImpl>();
}

Fiber::~Fiber() {
    // Make sure the Fiber is not being used
    guard.lock();
    guard.unlock();
    DeleteFiber(impl->handle);
}

void Fiber::Exit() {
    if (!is_thread_fiber) {
        return;
    }
    ConvertFiberToThread();
    guard.unlock();
}

void Fiber::YieldTo(std::shared_ptr<Fiber> from, std::shared_ptr<Fiber> to) {
    to->guard.lock();
    to->previous_fiber = from;
    SwitchToFiber(to->impl->handle);
    auto previous_fiber = from->previous_fiber;
    if (previous_fiber) {
        previous_fiber->guard.unlock();
        previous_fiber.reset();
    }
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->guard.lock();
    fiber->impl->handle = ConvertThreadToFiber(NULL);
    fiber->is_thread_fiber = true;
    return fiber;
}

#else

#include <boost/context/detail/fcontext.hpp>

constexpr std::size_t default_stack_size = 1024 * 1024 * 4; // 4MB

struct Fiber::FiberImpl {
    boost::context::detail::fcontext_t context;
    std::array<u8, default_stack_size> stack;
};

void Fiber::_start(void* parameter) {
    guard.lock();
    boost::context::detail::transfer_t* transfer = static_cast<boost::context::detail::transfer_t*>(parameter);
    if (previous_fiber) {
        previous_fiber->impl->context = transfer->fctx;
        previous_fiber->guard.unlock();
        previous_fiber = nullptr;
    }
    entry_point(start_parameter);
}

static void FiberStartFunc(boost::context::detail::transfer_t transfer)
{
   auto fiber = static_cast<Fiber *>(transfer.data);
   fiber->_start(&transfer);
}

Fiber::Fiber(std::function<void(void*)>&& entry_point_func, void* start_parameter)
    : guard{}, entry_point{std::move(entry_point_func)}, start_parameter{start_parameter}, previous_fiber{} {
    impl = std::make_unique<FiberImpl>();
    auto start_func = std::bind(&Fiber::start, this);
    impl->context =
        boost::context::detail::make_fcontext(impl->stack.data(), impl->stack.size(), &start_func);
}

Fiber::Fiber() : guard{}, entry_point{}, start_parameter{}, previous_fiber{} {
    impl = std::make_unique<FiberImpl>();
}

Fiber::~Fiber() {
    // Make sure the Fiber is not being used
    guard.lock();
    guard.unlock();
}

void Fiber::Exit() {
    if (!is_thread_fiber) {
        return;
    }
    guard.unlock();
}

void Fiber::YieldTo(std::shared_ptr<Fiber> from, std::shared_ptr<Fiber> to) {
    to->guard.lock();
    to->previous_fiber = from;
    auto transfer = boost::context::detail::jump_fcontext(to->impl.context, nullptr);
    auto previous_fiber = from->previous_fiber;
    if (previous_fiber) {
        previous_fiber->impl->context = transfer.fctx;
        previous_fiber->guard.unlock();
        previous_fiber.reset();
    }
}

std::shared_ptr<Fiber> Fiber::ThreadToFiber() {
    std::shared_ptr<Fiber> fiber = std::shared_ptr<Fiber>{new Fiber()};
    fiber->is_thread_fiber = true;
    return fiber;
}

#endif
} // namespace Common
