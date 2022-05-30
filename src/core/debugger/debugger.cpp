// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <thread>

#include <boost/asio.hpp>
#include <boost/process/async_pipe.hpp>

#include "common/logging/log.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/debugger/debugger.h"
#include "core/debugger/debugger_interface.h"
#include "core/debugger/gdbstub.h"
#include "core/hle/kernel/global_scheduler_context.h"

template <typename Readable, typename Buffer, typename Callback>
static void AsyncReceiveInto(Readable& r, Buffer& buffer, Callback&& c) {
    static_assert(std::is_trivial_v<Buffer>);
    auto boost_buffer{boost::asio::buffer(&buffer, sizeof(Buffer))};
    r.async_read_some(boost_buffer, [&](const boost::system::error_code& error, size_t bytes_read) {
        if (!error.failed()) {
            const u8* buffer_start = reinterpret_cast<const u8*>(&buffer);
            std::span<const u8> received_data{buffer_start, buffer_start + bytes_read};
            c(received_data);
        }

        AsyncReceiveInto(r, buffer, c);
    });
}

template <typename Readable, typename Buffer>
static std::span<const u8> ReceiveInto(Readable& r, Buffer& buffer) {
    static_assert(std::is_trivial_v<Buffer>);
    auto boost_buffer{boost::asio::buffer(&buffer, sizeof(Buffer))};
    size_t bytes_read = r.read_some(boost_buffer);
    const u8* buffer_start = reinterpret_cast<const u8*>(&buffer);
    std::span<const u8> received_data{buffer_start, buffer_start + bytes_read};
    return received_data;
}

namespace Core {

class DebuggerImpl : public DebuggerBackend {
public:
    explicit DebuggerImpl(Core::System& system_, u16 port)
        : system{system_}, signal_pipe{io_context}, client_socket{io_context} {
        frontend = std::make_unique<GDBStub>(*this, system);
        InitializeServer(port);
    }

    ~DebuggerImpl() {
        ShutdownServer();
    }

    bool NotifyThreadStopped(Kernel::KThread* thread) {
        std::scoped_lock lk{connection_lock};

        if (stopped) {
            // Do not notify the debugger about another event.
            // It should be ignored.
            return false;
        }
        stopped = true;

        signal_pipe.write_some(boost::asio::buffer(&thread, sizeof(thread)));
        return true;
    }

    std::span<const u8> ReadFromClient() override {
        return ReceiveInto(client_socket, client_data);
    }

    void WriteToClient(std::span<const u8> data) override {
        client_socket.write_some(boost::asio::buffer(data.data(), data.size_bytes()));
    }

    void SetActiveThread(Kernel::KThread* thread) override {
        active_thread = thread;
    }

    Kernel::KThread* GetActiveThread() override {
        return active_thread;
    }

    bool IsStepping() const {
        return stepping;
    }

private:
    void InitializeServer(u16 port) {
        using boost::asio::ip::tcp;

        LOG_INFO(Debug_GDBStub, "Starting server on port {}...", port);

        // Initialize the listening socket and accept a new client.
        tcp::endpoint endpoint{boost::asio::ip::address_v4::loopback(), port};
        tcp::acceptor acceptor{io_context, endpoint};
        client_socket = acceptor.accept();

        // Run the connection thread.
        connection_thread = std::jthread([&](std::stop_token stop_token) {
            try {
                ThreadLoop(stop_token);
            } catch (const std::exception& ex) {
                LOG_CRITICAL(Debug_GDBStub, "Stopping server: {}", ex.what());
            }

            client_socket.shutdown(client_socket.shutdown_both);
            client_socket.close();
        });
    }

    void ShutdownServer() {
        connection_thread.request_stop();
        io_context.stop();
        connection_thread.join();
    }

    void ThreadLoop(std::stop_token stop_token) {
        Common::SetCurrentThreadName("yuzu:Debugger");

        // Set up the client signals for new data.
        AsyncReceiveInto(signal_pipe, active_thread, [&](auto d) { PipeData(d); });
        AsyncReceiveInto(client_socket, client_data, [&](auto d) { ClientData(d); });

        // Stop the emulated CPU.
        AllCoreStop();

        // Set the active thread.
        active_thread = ThreadList()[0];
        active_thread->Resume(Kernel::SuspendType::Debug);

        // Set up the frontend.
        frontend->Connected();

        // Main event loop.
        while (!stop_token.stop_requested() && io_context.run()) {
        }
    }

    void PipeData(std::span<const u8> data) {
        AllCoreStop();
        active_thread->Resume(Kernel::SuspendType::Debug);
        frontend->Stopped(active_thread);
    }

    void ClientData(std::span<const u8> data) {
        const auto actions{frontend->ClientData(data)};
        for (const auto action : actions) {
            switch (action) {
            case DebuggerAction::Interrupt: {
                {
                    std::scoped_lock lk{connection_lock};
                    stopped = true;
                }
                AllCoreStop();
                active_thread = ThreadList()[0];
                active_thread->Resume(Kernel::SuspendType::Debug);
                frontend->Stopped(active_thread);
                break;
            }
            case DebuggerAction::Continue:
                stepping = false;
                ResumeInactiveThreads();
                AllCoreResume();
                break;
            case DebuggerAction::StepThread:
                stepping = true;
                SuspendInactiveThreads();
                AllCoreResume();
                break;
            case DebuggerAction::ShutdownEmulation: {
                // Suspend all threads and release any locks held
                active_thread->RequestSuspend(Kernel::SuspendType::Debug);
                SuspendInactiveThreads();
                AllCoreResume();

                // Spawn another thread that will exit after shutdown,
                // to avoid a deadlock
                Core::System* system_ref{&system};
                std::thread t([system_ref] { system_ref->Exit(); });
                t.detach();
                break;
            }
            }
        }
    }

    void AllCoreStop() {
        if (!suspend) {
            suspend = system.StallCPU();
        }
    }

    void AllCoreResume() {
        stopped = false;
        system.UnstallCPU();
        suspend.reset();
    }

    void SuspendInactiveThreads() {
        for (auto* thread : ThreadList()) {
            if (thread != active_thread) {
                thread->RequestSuspend(Kernel::SuspendType::Debug);
            }
        }
    }

    void ResumeInactiveThreads() {
        for (auto* thread : ThreadList()) {
            if (thread != active_thread) {
                thread->Resume(Kernel::SuspendType::Debug);
            }
        }
    }

    const std::vector<Kernel::KThread*>& ThreadList() {
        return system.GlobalSchedulerContext().GetThreadList();
    }

private:
    System& system;
    std::unique_ptr<DebuggerFrontend> frontend;

    std::jthread connection_thread;
    std::mutex connection_lock;
    boost::asio::io_context io_context;
    boost::process::async_pipe signal_pipe;
    boost::asio::ip::tcp::socket client_socket;
    std::optional<std::unique_lock<std::mutex>> suspend;

    Kernel::KThread* active_thread;
    bool stopped;
    bool stepping;

    std::array<u8, 4096> client_data;
};

Debugger::Debugger(Core::System& system, u16 port) {
    try {
        impl = std::make_unique<DebuggerImpl>(system, port);
    } catch (const std::exception& ex) {
        LOG_CRITICAL(Debug_GDBStub, "Failed to initialize debugger: {}", ex.what());
    }
}

Debugger::~Debugger() = default;

bool Debugger::NotifyThreadStopped(Kernel::KThread* thread) {
    return impl && impl->NotifyThreadStopped(thread);
}

bool Debugger::IsStepping() const {
    return impl && impl->IsStepping();
}

} // namespace Core
