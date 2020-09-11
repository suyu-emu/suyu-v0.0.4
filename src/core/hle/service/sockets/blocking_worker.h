// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/microprofile.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/writable_event.h"

namespace Service::Sockets {

/**
 * Worker abstraction to execute blocking calls on host without blocking the guest thread
 *
 * @tparam Service  Service where the work is executed
 * @tparam Types Types of work to execute
 */
template <class Service, class... Types>
class BlockingWorker {
    using This = BlockingWorker<Service, Types...>;
    using WorkVariant = std::variant<std::monostate, Types...>;

public:
    /// Create a new worker
    static std::unique_ptr<This> Create(Core::System& system, Service* service,
                                        std::string_view name) {
        return std::unique_ptr<This>(new This(system, service, name));
    }

    ~BlockingWorker() {
        while (!is_available.load(std::memory_order_relaxed)) {
            // Busy wait until work is finished
            std::this_thread::yield();
        }
        // Monostate means to exit the thread
        work = std::monostate{};
        work_event.Set();
        thread.join();
    }

    /**
     * Try to capture the worker to send work after a success
     * @returns True when the worker has been successfully captured
     */
    bool TryCapture() {
        bool expected = true;
        return is_available.compare_exchange_weak(expected, false, std::memory_order_relaxed,
                                                  std::memory_order_relaxed);
    }

    /**
     * Send work to this worker abstraction
     * @see TryCapture must be called before attempting to call this function
     */
    template <class Work>
    void SendWork(Work new_work) {
        ASSERT_MSG(!is_available, "Trying to send work on a worker that's not captured");
        work = std::move(new_work);
        work_event.Set();
    }

    /// Generate a callback for @see SleepClientThread
    template <class Work>
    auto Callback() {
        return [this](std::shared_ptr<Kernel::Thread>, Kernel::HLERequestContext& ctx,
                      Kernel::ThreadWakeupReason reason) {
            ASSERT(reason == Kernel::ThreadWakeupReason::Signal);
            std::get<Work>(work).Response(ctx);
            is_available.store(true);
        };
    }

    /// Get kernel event that will be signalled by the worker when the host operation finishes
    std::shared_ptr<Kernel::WritableEvent> KernelEvent() const {
        return kernel_event;
    }

private:
    explicit BlockingWorker(Core::System& system, Service* service, std::string_view name) {
        auto pair = Kernel::WritableEvent::CreateEventPair(system.Kernel(), std::string(name));
        kernel_event = std::move(pair.writable);
        thread = std::thread([this, &system, service, name] { Run(system, service, name); });
    }

    void Run(Core::System& system, Service* service, std::string_view name) {
        system.RegisterHostThread();

        const std::string thread_name = fmt::format("yuzu:{}", name);
        MicroProfileOnThreadCreate(thread_name.c_str());
        Common::SetCurrentThreadName(thread_name.c_str());

        bool keep_running = true;
        while (keep_running) {
            work_event.Wait();

            const auto visit_fn = [service, &keep_running]<typename T>(T&& w) {
                if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {
                    keep_running = false;
                } else {
                    w.Execute(service);
                }
            };
            std::visit(visit_fn, work);

            kernel_event->Signal();
        }
    }

    std::thread thread;
    WorkVariant work;
    Common::Event work_event;
    std::shared_ptr<Kernel::WritableEvent> kernel_event;
    std::atomic_bool is_available{true};
};

template <class Service, class... Types>
class BlockingWorkerPool {
    using Worker = BlockingWorker<Service, Types...>;

public:
    explicit BlockingWorkerPool(Core::System& system_, Service* service_)
        : system{system_}, service{service_} {}

    /// Returns a captured worker thread, creating new ones if necessary
    Worker* CaptureWorker() {
        for (auto& worker : workers) {
            if (worker->TryCapture()) {
                return worker.get();
            }
        }
        auto new_worker = Worker::Create(system, service, fmt::format("BSD:{}", workers.size()));
        [[maybe_unused]] const bool success = new_worker->TryCapture();
        ASSERT(success);

        return workers.emplace_back(std::move(new_worker)).get();
    }

private:
    Core::System& system;
    Service* const service;

    std::vector<std::unique_ptr<Worker>> workers;
};

} // namespace Service::Sockets
