// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

#include <boost/range/algorithm_ext/erase.hpp>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/time_manager.h"
#include "core/memory.h"

namespace Kernel {

SessionRequestHandler::SessionRequestHandler() = default;

SessionRequestHandler::~SessionRequestHandler() = default;

void SessionRequestHandler::ClientConnected(KServerSession* session) {
    session->SetHleHandler(shared_from_this());
}

void SessionRequestHandler::ClientDisconnected(KServerSession* session) {
    session->SetHleHandler(nullptr);
}

HLERequestContext::HLERequestContext(KernelCore& kernel_, Core::Memory::Memory& memory_,
                                     KServerSession* server_session_, KThread* thread_)
    : server_session(server_session_), thread(thread_), kernel{kernel_}, memory{memory_} {
    cmd_buf[0] = 0;
}

HLERequestContext::~HLERequestContext() = default;

void HLERequestContext::ParseCommandBuffer(const KHandleTable& handle_table, u32_le* src_cmdbuf,
                                           bool incoming) {
    IPC::RequestParser rp(src_cmdbuf);
    command_header = rp.PopRaw<IPC::CommandHeader>();

    if (command_header->IsCloseCommand()) {
        // Close does not populate the rest of the IPC header
        return;
    }

    // If handle descriptor is present, add size of it
    if (command_header->enable_handle_descriptor) {
        handle_descriptor_header = rp.PopRaw<IPC::HandleDescriptorHeader>();
        if (handle_descriptor_header->send_current_pid) {
            rp.Skip(2, false);
        }
        if (incoming) {
            // Populate the object lists with the data in the IPC request.
            for (u32 handle = 0; handle < handle_descriptor_header->num_handles_to_copy; ++handle) {
                const u32 copy_handle{rp.Pop<Handle>()};
                copy_handles.push_back(copy_handle);
                copy_objects.push_back(handle_table.GetObject(copy_handle).GetPointerUnsafe());
            }
            for (u32 handle = 0; handle < handle_descriptor_header->num_handles_to_move; ++handle) {
                const u32 move_handle{rp.Pop<Handle>()};
                move_handles.push_back(move_handle);
                move_objects.push_back(handle_table.GetObject(move_handle).GetPointerUnsafe());
            }
        } else {
            // For responses we just ignore the handles, they're empty and will be populated when
            // translating the response.
            rp.Skip(handle_descriptor_header->num_handles_to_copy, false);
            rp.Skip(handle_descriptor_header->num_handles_to_move, false);
        }
    }

    for (unsigned i = 0; i < command_header->num_buf_x_descriptors; ++i) {
        buffer_x_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorX>());
    }
    for (unsigned i = 0; i < command_header->num_buf_a_descriptors; ++i) {
        buffer_a_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }
    for (unsigned i = 0; i < command_header->num_buf_b_descriptors; ++i) {
        buffer_b_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }
    for (unsigned i = 0; i < command_header->num_buf_w_descriptors; ++i) {
        buffer_w_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }

    const auto buffer_c_offset = rp.GetCurrentOffset() + command_header->data_size;

    if (!command_header->IsTipc()) {
        // Padding to align to 16 bytes
        rp.AlignWithPadding();

        if (Session()->IsDomain() &&
            ((command_header->type == IPC::CommandType::Request ||
              command_header->type == IPC::CommandType::RequestWithContext) ||
             !incoming)) {
            // If this is an incoming message, only CommandType "Request" has a domain header
            // All outgoing domain messages have the domain header, if only incoming has it
            if (incoming || domain_message_header) {
                domain_message_header = rp.PopRaw<IPC::DomainMessageHeader>();
            } else {
                if (Session()->IsDomain()) {
                    LOG_WARNING(IPC, "Domain request has no DomainMessageHeader!");
                }
            }
        }

        data_payload_header = rp.PopRaw<IPC::DataPayloadHeader>();

        data_payload_offset = rp.GetCurrentOffset();

        if (domain_message_header &&
            domain_message_header->command ==
                IPC::DomainMessageHeader::CommandType::CloseVirtualHandle) {
            // CloseVirtualHandle command does not have SFC* or any data
            return;
        }

        if (incoming) {
            ASSERT(data_payload_header->magic == Common::MakeMagic('S', 'F', 'C', 'I'));
        } else {
            ASSERT(data_payload_header->magic == Common::MakeMagic('S', 'F', 'C', 'O'));
        }
    }

    rp.SetCurrentOffset(buffer_c_offset);

    // For Inline buffers, the response data is written directly to buffer_c_offset
    // and in this case we don't have any BufferDescriptorC on the request.
    if (command_header->buf_c_descriptor_flags >
        IPC::CommandHeader::BufferDescriptorCFlag::InlineDescriptor) {
        if (command_header->buf_c_descriptor_flags ==
            IPC::CommandHeader::BufferDescriptorCFlag::OneDescriptor) {
            buffer_c_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorC>());
        } else {
            unsigned num_buf_c_descriptors =
                static_cast<unsigned>(command_header->buf_c_descriptor_flags.Value()) - 2;

            // This is used to detect possible underflows, in case something is broken
            // with the two ifs above and the flags value is == 0 || == 1.
            ASSERT(num_buf_c_descriptors < 14);

            for (unsigned i = 0; i < num_buf_c_descriptors; ++i) {
                buffer_c_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorC>());
            }
        }
    }

    rp.SetCurrentOffset(data_payload_offset);

    command = rp.Pop<u32_le>();
    rp.Skip(1, false); // The command is actually an u64, but we don't use the high part.
}

ResultCode HLERequestContext::PopulateFromIncomingCommandBuffer(const KHandleTable& handle_table,
                                                                u32_le* src_cmdbuf) {
    ParseCommandBuffer(handle_table, src_cmdbuf, true);

    if (command_header->IsCloseCommand()) {
        // Close does not populate the rest of the IPC header
        return RESULT_SUCCESS;
    }

    std::copy_n(src_cmdbuf, IPC::COMMAND_BUFFER_LENGTH, cmd_buf.begin());

    return RESULT_SUCCESS;
}

ResultCode HLERequestContext::WriteToOutgoingCommandBuffer(KThread& requesting_thread) {
    auto current_offset = handles_offset;
    auto& owner_process = *requesting_thread.GetOwnerProcess();
    auto& handle_table = owner_process.GetHandleTable();

    // The data_size already includes the payload header, the padding and the domain header.
    std::size_t size{};

    if (IsTipc()) {
        size = cmd_buf.size();
    } else {
        size = data_payload_offset + data_size - sizeof(IPC::DataPayloadHeader) / sizeof(u32) - 4;
        if (Session()->IsDomain()) {
            size -= sizeof(IPC::DomainMessageHeader) / sizeof(u32);
        }
    }

    for (auto& object : copy_objects) {
        Handle handle{};
        if (object) {
            R_TRY(handle_table.Add(&handle, object));
        }
        cmd_buf[current_offset++] = handle;
    }
    for (auto& object : move_objects) {
        Handle handle{};
        if (object) {
            R_TRY(handle_table.Add(&handle, object));

            // Close our reference to the object, as it is being moved to the caller.
            object->Close();
        }
        cmd_buf[current_offset++] = handle;
    }

    // Write the domain objects to the command buffer, these go after the raw untranslated data.
    // TODO(Subv): This completely ignores C buffers.

    if (Session()->IsDomain()) {
        current_offset = domain_offset - static_cast<u32>(domain_objects.size());
        for (const auto& object : domain_objects) {
            server_session->AppendDomainRequestHandler(object);
            cmd_buf[current_offset++] =
                static_cast<u32_le>(server_session->NumDomainRequestHandlers());
        }
    }

    // Copy the translated command buffer back into the thread's command buffer area.
    memory.WriteBlock(owner_process, requesting_thread.GetTLSAddress(), cmd_buf.data(),
                      size * sizeof(u32));

    return RESULT_SUCCESS;
}

std::vector<u8> HLERequestContext::ReadBuffer(std::size_t buffer_index) const {
    std::vector<u8> buffer{};
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};

    if (is_buffer_a) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorA().size() > buffer_index, { return buffer; },
            "BufferDescriptorA invalid buffer_index {}", buffer_index);
        buffer.resize(BufferDescriptorA()[buffer_index].Size());
        memory.ReadBlock(BufferDescriptorA()[buffer_index].Address(), buffer.data(), buffer.size());
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorX().size() > buffer_index, { return buffer; },
            "BufferDescriptorX invalid buffer_index {}", buffer_index);
        buffer.resize(BufferDescriptorX()[buffer_index].Size());
        memory.ReadBlock(BufferDescriptorX()[buffer_index].Address(), buffer.data(), buffer.size());
    }

    return buffer;
}

std::size_t HLERequestContext::WriteBuffer(const void* buffer, std::size_t size,
                                           std::size_t buffer_index) const {
    if (size == 0) {
        LOG_WARNING(Core, "skip empty buffer write");
        return 0;
    }

    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};
    const std::size_t buffer_size{GetWriteBufferSize(buffer_index)};
    if (size > buffer_size) {
        LOG_CRITICAL(Core, "size ({:016X}) is greater than buffer_size ({:016X})", size,
                     buffer_size);
        size = buffer_size; // TODO(bunnei): This needs to be HW tested
    }

    if (is_buffer_b) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorB().size() > buffer_index &&
                BufferDescriptorB()[buffer_index].Size() >= size,
            { return 0; }, "BufferDescriptorB is invalid, index={}, size={}", buffer_index, size);
        memory.WriteBlock(BufferDescriptorB()[buffer_index].Address(), buffer, size);
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorC().size() > buffer_index &&
                BufferDescriptorC()[buffer_index].Size() >= size,
            { return 0; }, "BufferDescriptorC is invalid, index={}, size={}", buffer_index, size);
        memory.WriteBlock(BufferDescriptorC()[buffer_index].Address(), buffer, size);
    }

    return size;
}

std::size_t HLERequestContext::GetReadBufferSize(std::size_t buffer_index) const {
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};
    if (is_buffer_a) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorA().size() > buffer_index, { return 0; },
            "BufferDescriptorA invalid buffer_index {}", buffer_index);
        return BufferDescriptorA()[buffer_index].Size();
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorX().size() > buffer_index, { return 0; },
            "BufferDescriptorX invalid buffer_index {}", buffer_index);
        return BufferDescriptorX()[buffer_index].Size();
    }
}

std::size_t HLERequestContext::GetWriteBufferSize(std::size_t buffer_index) const {
    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};
    if (is_buffer_b) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorB().size() > buffer_index, { return 0; },
            "BufferDescriptorB invalid buffer_index {}", buffer_index);
        return BufferDescriptorB()[buffer_index].Size();
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorC().size() > buffer_index, { return 0; },
            "BufferDescriptorC invalid buffer_index {}", buffer_index);
        return BufferDescriptorC()[buffer_index].Size();
    }
    return 0;
}

bool HLERequestContext::CanReadBuffer(std::size_t buffer_index) const {
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};

    if (is_buffer_a) {
        return BufferDescriptorA().size() > buffer_index;
    } else {
        return BufferDescriptorX().size() > buffer_index;
    }
}

bool HLERequestContext::CanWriteBuffer(std::size_t buffer_index) const {
    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};

    if (is_buffer_b) {
        return BufferDescriptorB().size() > buffer_index;
    } else {
        return BufferDescriptorC().size() > buffer_index;
    }
}

std::string HLERequestContext::Description() const {
    if (!command_header) {
        return "No command header available";
    }
    std::ostringstream s;
    s << "IPC::CommandHeader: Type:" << static_cast<u32>(command_header->type.Value());
    s << ", X(Pointer):" << command_header->num_buf_x_descriptors;
    if (command_header->num_buf_x_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_x_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorX()[i].Size();
            if (i < command_header->num_buf_x_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", A(Send):" << command_header->num_buf_a_descriptors;
    if (command_header->num_buf_a_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_a_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorA()[i].Size();
            if (i < command_header->num_buf_a_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", B(Receive):" << command_header->num_buf_b_descriptors;
    if (command_header->num_buf_b_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_b_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorB()[i].Size();
            if (i < command_header->num_buf_b_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", C(ReceiveList):" << BufferDescriptorC().size();
    if (!BufferDescriptorC().empty()) {
        s << '[';
        for (u64 i = 0; i < BufferDescriptorC().size(); ++i) {
            s << "0x" << std::hex << BufferDescriptorC()[i].Size();
            if (i < BufferDescriptorC().size() - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", data_size:" << command_header->data_size.Value();

    return s.str();
}

} // namespace Kernel
