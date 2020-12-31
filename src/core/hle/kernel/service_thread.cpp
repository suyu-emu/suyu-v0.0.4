// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "common/assert.h"
#include "common/scope_exit.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/service_thread.h"
#include "core/hle/lock.h"
#include "video_core/renderer_base.h"

namespace Kernel {

class ServiceThread::Impl final {
public:
    explicit Impl(KernelCore& kernel, std::size_t num_threads, const std::string& name);
    ~Impl();

    void QueueSyncRequest(ServerSession& session, std::shared_ptr<HLERequestContext>&& context);

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> requests;
    std::mutex queue_mutex;
    std::condition_variable condition;
    const std::string service_name;
    bool stop{};
};

ServiceThread::Impl::Impl(KernelCore& kernel, std::size_t num_threads, const std::string& name)
    : service_name{name} {
    for (std::size_t i = 0; i < num_threads; ++i)
        threads.emplace_back([this, &kernel] {
            Common::SetCurrentThreadName(std::string{"yuzu:HleService:" + service_name}.c_str());

            // Wait for first request before trying to acquire a render context
            {
                std::unique_lock lock{queue_mutex};
                condition.wait(lock, [this] { return stop || !requests.empty(); });
            }

            kernel.RegisterHostThread();

            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock lock{queue_mutex};
                    condition.wait(lock, [this] { return stop || !requests.empty(); });
                    if (stop || requests.empty()) {
                        return;
                    }
                    task = std::move(requests.front());
                    requests.pop();
                }

                task();
            }
        });
}

void ServiceThread::Impl::QueueSyncRequest(ServerSession& session,
                                           std::shared_ptr<HLERequestContext>&& context) {
    {
        std::unique_lock lock{queue_mutex};

        // ServerSession owns the service thread, so we cannot caption a strong pointer here in the
        // event that the ServerSession is terminated.
        std::weak_ptr<ServerSession> weak_ptr{SharedFrom(&session)};
        requests.emplace([weak_ptr, context{std::move(context)}]() {
            if (auto strong_ptr = weak_ptr.lock()) {
                strong_ptr->CompleteSyncRequest(*context);
            }
        });
    }
    condition.notify_one();
}

ServiceThread::Impl::~Impl() {
    {
        std::unique_lock lock{queue_mutex};
        stop = true;
    }
    condition.notify_all();
    for (std::thread& thread : threads) {
        thread.join();
    }
}

ServiceThread::ServiceThread(KernelCore& kernel, std::size_t num_threads, const std::string& name)
    : impl{std::make_unique<Impl>(kernel, num_threads, name)} {}

ServiceThread::~ServiceThread() = default;

void ServiceThread::QueueSyncRequest(ServerSession& session,
                                     std::shared_ptr<HLERequestContext>&& context) {
    impl->QueueSyncRequest(session, std::move(context));
}

} // namespace Kernel
