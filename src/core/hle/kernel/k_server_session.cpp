// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <tuple>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/message_buffer.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/memory.h"

namespace Kernel {

namespace {

template <bool MoveHandleAllowed>
Result ProcessMessageSpecialData(KProcess& dst_process, KProcess& src_process, KThread& src_thread,
                                 MessageBuffer& dst_msg, const MessageBuffer& src_msg,
                                 MessageBuffer::SpecialHeader& src_special_header) {
    // Copy the special header to the destination.
    s32 offset = dst_msg.Set(src_special_header);

    // Copy the process ID.
    if (src_special_header.GetHasProcessId()) {
        offset = dst_msg.SetProcessId(offset, src_process.GetProcessId());
    }

    // Prepare to process handles.
    auto& dst_handle_table = dst_process.GetHandleTable();
    auto& src_handle_table = src_process.GetHandleTable();
    Result result = ResultSuccess;

    // Process copy handles.
    for (auto i = 0; i < src_special_header.GetCopyHandleCount(); ++i) {
        // Get the handles.
        const Handle src_handle = src_msg.GetHandle(offset);
        Handle dst_handle = Svc::InvalidHandle;

        // If we're in a success state, try to move the handle to the new table.
        if (R_SUCCEEDED(result) && src_handle != Svc::InvalidHandle) {
            KScopedAutoObject obj =
                src_handle_table.GetObjectForIpc(src_handle, std::addressof(src_thread));
            if (obj.IsNotNull()) {
                Result add_result =
                    dst_handle_table.Add(std::addressof(dst_handle), obj.GetPointerUnsafe());
                if (R_FAILED(add_result)) {
                    result = add_result;
                    dst_handle = Svc::InvalidHandle;
                }
            } else {
                result = ResultInvalidHandle;
            }
        }

        // Set the handle.
        offset = dst_msg.SetHandle(offset, dst_handle);
    }

    // Process move handles.
    if constexpr (MoveHandleAllowed) {
        for (auto i = 0; i < src_special_header.GetMoveHandleCount(); ++i) {
            // Get the handles.
            const Handle src_handle = src_msg.GetHandle(offset);
            Handle dst_handle = Svc::InvalidHandle;

            // Whether or not we've succeeded, we need to remove the handles from the source table.
            if (src_handle != Svc::InvalidHandle) {
                if (R_SUCCEEDED(result)) {
                    KScopedAutoObject obj =
                        src_handle_table.GetObjectForIpcWithoutPseudoHandle(src_handle);
                    if (obj.IsNotNull()) {
                        Result add_result = dst_handle_table.Add(std::addressof(dst_handle),
                                                                 obj.GetPointerUnsafe());

                        src_handle_table.Remove(src_handle);

                        if (R_FAILED(add_result)) {
                            result = add_result;
                            dst_handle = Svc::InvalidHandle;
                        }
                    } else {
                        result = ResultInvalidHandle;
                    }
                } else {
                    src_handle_table.Remove(src_handle);
                }
            }

            // Set the handle.
            offset = dst_msg.SetHandle(offset, dst_handle);
        }
    }

    R_RETURN(result);
}

void CleanupSpecialData(KProcess& dst_process, u32* dst_msg_ptr, size_t dst_buffer_size) {
    // Parse the message.
    const MessageBuffer dst_msg(dst_msg_ptr, dst_buffer_size);
    const MessageBuffer::MessageHeader dst_header(dst_msg);
    const MessageBuffer::SpecialHeader dst_special_header(dst_msg, dst_header);

    // Check that the size is big enough.
    if (MessageBuffer::GetMessageBufferSize(dst_header, dst_special_header) > dst_buffer_size) {
        return;
    }

    // Set the special header.
    int offset = dst_msg.Set(dst_special_header);

    // Clear the process id, if needed.
    if (dst_special_header.GetHasProcessId()) {
        offset = dst_msg.SetProcessId(offset, 0);
    }

    // Clear handles, as relevant.
    auto& dst_handle_table = dst_process.GetHandleTable();
    for (auto i = 0;
         i < (dst_special_header.GetCopyHandleCount() + dst_special_header.GetMoveHandleCount());
         ++i) {
        const Handle handle = dst_msg.GetHandle(offset);

        if (handle != Svc::InvalidHandle) {
            dst_handle_table.Remove(handle);
        }

        offset = dst_msg.SetHandle(offset, Svc::InvalidHandle);
    }
}

} // namespace

using ThreadQueueImplForKServerSessionRequest = KThreadQueue;

KServerSession::KServerSession(KernelCore& kernel)
    : KSynchronizationObject{kernel}, m_lock{m_kernel} {}

KServerSession::~KServerSession() = default;

void KServerSession::Destroy() {
    m_parent->OnServerClosed();

    this->CleanupRequests();

    m_parent->Close();
}

void KServerSession::OnClientClosed() {
    KScopedLightLock lk{m_lock};

    // Handle any pending requests.
    KSessionRequest* prev_request = nullptr;
    while (true) {
        // Declare variables for processing the request.
        KSessionRequest* request = nullptr;
        KEvent* event = nullptr;
        KThread* thread = nullptr;
        bool cur_request = false;
        bool terminate = false;

        // Get the next request.
        {
            KScopedSchedulerLock sl{m_kernel};

            if (m_current_request != nullptr && m_current_request != prev_request) {
                // Set the request, open a reference as we process it.
                request = m_current_request;
                request->Open();
                cur_request = true;

                // Get thread and event for the request.
                thread = request->GetThread();
                event = request->GetEvent();

                // If the thread is terminating, handle that.
                if (thread->IsTerminationRequested()) {
                    request->ClearThread();
                    request->ClearEvent();
                    terminate = true;
                }

                prev_request = request;
            } else if (!m_request_list.empty()) {
                // Pop the request from the front of the list.
                request = std::addressof(m_request_list.front());
                m_request_list.pop_front();

                // Get thread and event for the request.
                thread = request->GetThread();
                event = request->GetEvent();
            }
        }

        // If there are no requests, we're done.
        if (request == nullptr) {
            break;
        }

        // All requests must have threads.
        ASSERT(thread != nullptr);

        // Ensure that we close the request when done.
        SCOPE_EXIT({ request->Close(); });

        // If we're terminating, close a reference to the thread and event.
        if (terminate) {
            thread->Close();
            if (event != nullptr) {
                event->Close();
            }
        }

        // If we need to, reply.
        if (event != nullptr && !cur_request) {
            // There must be no mappings.
            ASSERT(request->GetSendCount() == 0);
            ASSERT(request->GetReceiveCount() == 0);
            ASSERT(request->GetExchangeCount() == 0);

            // // Get the process and page table.
            // KProcess *client_process = thread->GetOwnerProcess();
            // auto& client_pt = client_process->GetPageTable();

            // // Reply to the request.
            // ReplyAsyncError(client_process, request->GetAddress(), request->GetSize(),
            //                 ResultSessionClosed);

            // // Unlock the buffer.
            // // NOTE: Nintendo does not check the result of this.
            // client_pt.UnlockForIpcUserBuffer(request->GetAddress(), request->GetSize());

            // Signal the event.
            event->Signal();
        }
    }

    // Notify.
    this->NotifyAvailable(ResultSessionClosed);
}

bool KServerSession::IsSignaled() const {
    ASSERT(KScheduler::IsSchedulerLockedByCurrentThread(m_kernel));

    // If the client is closed, we're always signaled.
    if (m_parent->IsClientClosed()) {
        return true;
    }

    // Otherwise, we're signaled if we have a request and aren't handling one.
    return !m_request_list.empty() && m_current_request == nullptr;
}

Result KServerSession::OnRequest(KSessionRequest* request) {
    // Create the wait queue.
    ThreadQueueImplForKServerSessionRequest wait_queue{m_kernel};

    {
        // Lock the scheduler.
        KScopedSchedulerLock sl{m_kernel};

        // Ensure that we can handle new requests.
        R_UNLESS(!m_parent->IsServerClosed(), ResultSessionClosed);

        // Check that we're not terminating.
        R_UNLESS(!GetCurrentThread(m_kernel).IsTerminationRequested(), ResultTerminationRequested);

        // Get whether we're empty.
        const bool was_empty = m_request_list.empty();

        // Add the request to the list.
        request->Open();
        m_request_list.push_back(*request);

        // If we were empty, signal.
        if (was_empty) {
            this->NotifyAvailable();
        }

        // If we have a request event, this is asynchronous, and we don't need to wait.
        R_SUCCEED_IF(request->GetEvent() != nullptr);

        // This is a synchronous request, so we should wait for our request to complete.
        GetCurrentThread(m_kernel).SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::IPC);
        GetCurrentThread(m_kernel).BeginWait(std::addressof(wait_queue));
    }

    return GetCurrentThread(m_kernel).GetWaitResult();
}

Result KServerSession::SendReply(bool is_hle) {
    // Lock the session.
    KScopedLightLock lk{m_lock};

    // Get the request.
    KSessionRequest* request;
    {
        KScopedSchedulerLock sl{m_kernel};

        // Get the current request.
        request = m_current_request;
        R_UNLESS(request != nullptr, ResultInvalidState);

        // Clear the current request, since we're processing it.
        m_current_request = nullptr;
        if (!m_request_list.empty()) {
            this->NotifyAvailable();
        }
    }

    // Close reference to the request once we're done processing it.
    SCOPE_EXIT({ request->Close(); });

    // Extract relevant information from the request.
    const uintptr_t client_message = request->GetAddress();
    const size_t client_buffer_size = request->GetSize();
    KThread* client_thread = request->GetThread();
    KEvent* event = request->GetEvent();

    // Check whether we're closed.
    const bool closed = (client_thread == nullptr || m_parent->IsClientClosed());

    Result result = ResultSuccess;
    if (!closed) {
        // If we're not closed, send the reply.
        if (is_hle) {
            // HLE servers write directly to a pointer to the thread command buffer. Therefore
            // the reply has already been written in this case.
        } else {
            Core::Memory::Memory& memory{client_thread->GetOwnerProcess()->GetMemory()};
            KThread* server_thread = GetCurrentThreadPointer(m_kernel);
            KProcess& src_process = *client_thread->GetOwnerProcess();
            KProcess& dst_process = *server_thread->GetOwnerProcess();
            UNIMPLEMENTED_IF(server_thread->GetOwnerProcess() != client_thread->GetOwnerProcess());

            auto* src_msg_buffer = memory.GetPointer<u32>(server_thread->GetTlsAddress());
            auto* dst_msg_buffer = memory.GetPointer<u32>(client_message);
            std::memcpy(dst_msg_buffer, src_msg_buffer, client_buffer_size);

            // Translate special header ad-hoc.
            MessageBuffer src_msg(src_msg_buffer, client_buffer_size);
            MessageBuffer::MessageHeader src_header(src_msg);
            MessageBuffer::SpecialHeader src_special_header(src_msg, src_header);
            if (src_header.GetHasSpecialHeader()) {
                MessageBuffer dst_msg(dst_msg_buffer, client_buffer_size);
                result = ProcessMessageSpecialData<true>(dst_process, src_process, *server_thread,
                                                         dst_msg, src_msg, src_special_header);
                if (R_FAILED(result)) {
                    CleanupSpecialData(dst_process, dst_msg_buffer, client_buffer_size);
                }
            }
        }
    } else {
        result = ResultSessionClosed;
    }

    // Select a result for the client.
    Result client_result = result;
    if (closed && R_SUCCEEDED(result)) {
        result = ResultSessionClosed;
        client_result = ResultSessionClosed;
    } else {
        result = ResultSuccess;
    }

    // If there's a client thread, update it.
    if (client_thread != nullptr) {
        if (event != nullptr) {
            // // Get the client process/page table.
            // KProcess *client_process             = client_thread->GetOwnerProcess();
            // KProcessPageTable *client_page_table = std::addressof(client_process->PageTable());

            // // If we need to, reply with an async error.
            // if (R_FAILED(client_result)) {
            //     ReplyAsyncError(client_process, client_message, client_buffer_size,
            //     client_result);
            // }

            // // Unlock the client buffer.
            // // NOTE: Nintendo does not check the result of this.
            // client_page_table->UnlockForIpcUserBuffer(client_message, client_buffer_size);

            // Signal the event.
            event->Signal();
        } else {
            // End the client thread's wait.
            KScopedSchedulerLock sl{m_kernel};

            if (!client_thread->IsTerminationRequested()) {
                client_thread->EndWait(client_result);
            }
        }
    }

    R_RETURN(result);
}

Result KServerSession::ReceiveRequest(std::shared_ptr<Service::HLERequestContext>* out_context,
                                      std::weak_ptr<Service::SessionRequestManager> manager) {
    // Lock the session.
    KScopedLightLock lk{m_lock};

    // Get the request and client thread.
    KSessionRequest* request;
    KThread* client_thread;

    {
        KScopedSchedulerLock sl{m_kernel};

        // Ensure that we can service the request.
        R_UNLESS(!m_parent->IsClientClosed(), ResultSessionClosed);

        // Ensure we aren't already servicing a request.
        R_UNLESS(m_current_request == nullptr, ResultNotFound);

        // Ensure we have a request to service.
        R_UNLESS(!m_request_list.empty(), ResultNotFound);

        // Pop the first request from the list.
        request = std::addressof(m_request_list.front());
        m_request_list.pop_front();

        // Get the thread for the request.
        client_thread = request->GetThread();
        R_UNLESS(client_thread != nullptr, ResultSessionClosed);

        // Open the client thread.
        client_thread->Open();
    }

    SCOPE_EXIT({ client_thread->Close(); });

    // Set the request as our current.
    m_current_request = request;

    // Get the client address.
    uintptr_t client_message = request->GetAddress();
    size_t client_buffer_size = request->GetSize();
    // bool recv_list_broken = false;

    if (!client_message) {
        client_message = GetInteger(client_thread->GetTlsAddress());
        client_buffer_size = MessageBufferSize;
    }

    // Receive the message.
    Core::Memory::Memory& memory{client_thread->GetOwnerProcess()->GetMemory()};
    if (out_context != nullptr) {
        // HLE request.
        u32* cmd_buf{reinterpret_cast<u32*>(memory.GetPointer(client_message))};
        *out_context =
            std::make_shared<Service::HLERequestContext>(m_kernel, memory, this, client_thread);
        (*out_context)->SetSessionRequestManager(manager);
        (*out_context)
            ->PopulateFromIncomingCommandBuffer(*client_thread->GetOwnerProcess(), cmd_buf);
    } else {
        KThread* server_thread = GetCurrentThreadPointer(m_kernel);
        KProcess& src_process = *client_thread->GetOwnerProcess();
        KProcess& dst_process = *server_thread->GetOwnerProcess();
        UNIMPLEMENTED_IF(client_thread->GetOwnerProcess() != server_thread->GetOwnerProcess());

        auto* src_msg_buffer = memory.GetPointer<u32>(client_message);
        auto* dst_msg_buffer = memory.GetPointer<u32>(server_thread->GetTlsAddress());
        std::memcpy(dst_msg_buffer, src_msg_buffer, client_buffer_size);

        // Translate special header ad-hoc.
        // TODO: fix this mess
        MessageBuffer src_msg(src_msg_buffer, client_buffer_size);
        MessageBuffer::MessageHeader src_header(src_msg);
        MessageBuffer::SpecialHeader src_special_header(src_msg, src_header);
        if (src_header.GetHasSpecialHeader()) {
            MessageBuffer dst_msg(dst_msg_buffer, client_buffer_size);
            Result res = ProcessMessageSpecialData<false>(dst_process, src_process, *client_thread,
                                                          dst_msg, src_msg, src_special_header);
            if (R_FAILED(res)) {
                CleanupSpecialData(dst_process, dst_msg_buffer, client_buffer_size);
            }
        }
    }

    // We succeeded.
    R_SUCCEED();
}

void KServerSession::CleanupRequests() {
    KScopedLightLock lk(m_lock);

    // Clean up any pending requests.
    while (true) {
        // Get the next request.
        KSessionRequest* request = nullptr;
        {
            KScopedSchedulerLock sl{m_kernel};

            if (m_current_request) {
                // Choose the current request if we have one.
                request = m_current_request;
                m_current_request = nullptr;
            } else if (!m_request_list.empty()) {
                // Pop the request from the front of the list.
                request = std::addressof(m_request_list.front());
                m_request_list.pop_front();
            }
        }

        // If there's no request, we're done.
        if (request == nullptr) {
            break;
        }

        // Close a reference to the request once it's cleaned up.
        SCOPE_EXIT({ request->Close(); });

        // Extract relevant information from the request.
        // const uintptr_t client_message  = request->GetAddress();
        // const size_t client_buffer_size = request->GetSize();
        KThread* client_thread = request->GetThread();
        KEvent* event = request->GetEvent();

        // KProcess *server_process             = request->GetServerProcess();
        // KProcess *client_process             = (client_thread != nullptr) ?
        //                                         client_thread->GetOwnerProcess() : nullptr;
        // KProcessPageTable *client_page_table = (client_process != nullptr) ?
        //                                         std::addressof(client_process->GetPageTable())
        //                                         : nullptr;

        // Cleanup the mappings.
        // Result result = CleanupMap(request, server_process, client_page_table);

        // If there's a client thread, update it.
        if (client_thread != nullptr) {
            if (event != nullptr) {
                // // We need to reply async.
                // ReplyAsyncError(client_process, client_message, client_buffer_size,
                //                 (R_SUCCEEDED(result) ? ResultSessionClosed : result));

                // // Unlock the client buffer.
                // NOTE: Nintendo does not check the result of this.
                // client_page_table->UnlockForIpcUserBuffer(client_message, client_buffer_size);

                // Signal the event.
                event->Signal();
            } else {
                // End the client thread's wait.
                KScopedSchedulerLock sl{m_kernel};

                if (!client_thread->IsTerminationRequested()) {
                    client_thread->EndWait(ResultSessionClosed);
                }
            }
        }
    }
}

} // namespace Kernel
