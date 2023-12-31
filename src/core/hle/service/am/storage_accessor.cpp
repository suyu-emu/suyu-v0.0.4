// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/storage_accessor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IStorageAccessor::IStorageAccessor(Core::System& system_, IStorage& backing_)
    : ServiceFramework{system_, "IStorageAccessor"}, backing{backing_} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IStorageAccessor::GetSize, "GetSize"},
            {10, &IStorageAccessor::Write, "Write"},
            {11, &IStorageAccessor::Read, "Read"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

IStorageAccessor::~IStorageAccessor() = default;

void IStorageAccessor::GetSize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(ResultSuccess);
    rb.Push(static_cast<u64>(backing.GetSize()));
}

void IStorageAccessor::Write(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const u64 offset{rp.Pop<u64>()};
    const auto data{ctx.ReadBuffer()};
    const std::size_t size{std::min<u64>(data.size(), backing.GetSize() - offset)};

    LOG_DEBUG(Service_AM, "called, offset={}, size={}", offset, size);

    if (offset > backing.GetSize()) {
        LOG_ERROR(Service_AM,
                  "offset is out of bounds, backing_buffer_sz={}, data_size={}, offset={}",
                  backing.GetSize(), size, offset);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultInvalidOffset);
        return;
    }

    std::memcpy(backing.GetData().data() + offset, data.data(), size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IStorageAccessor::Read(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const u64 offset{rp.Pop<u64>()};
    const std::size_t size{std::min<u64>(ctx.GetWriteBufferSize(), backing.GetSize() - offset)};

    LOG_DEBUG(Service_AM, "called, offset={}, size={}", offset, size);

    if (offset > backing.GetSize()) {
        LOG_ERROR(Service_AM, "offset is out of bounds, backing_buffer_sz={}, size={}, offset={}",
                  backing.GetSize(), size, offset);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultInvalidOffset);
        return;
    }

    ctx.WriteBuffer(backing.GetData().data() + offset, size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
