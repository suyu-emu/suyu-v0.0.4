// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/storage.h"
#include "core/hle/service/am/storage_accessor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IStorageImpl::~IStorageImpl() = default;

class StorageDataImpl final : public IStorageImpl {
public:
    explicit StorageDataImpl(std::vector<u8>&& buffer_) : buffer{std::move(buffer_)} {}

    std::vector<u8>& GetData() override {
        return buffer;
    }

    const std::vector<u8>& GetData() const override {
        return buffer;
    }

    std::size_t GetSize() const override {
        return buffer.size();
    }

private:
    std::vector<u8> buffer;
};

IStorage::IStorage(Core::System& system_, std::vector<u8>&& buffer)
    : ServiceFramework{system_, "IStorage"},
      impl{std::make_shared<StorageDataImpl>(std::move(buffer))} {
    Register();
}

void IStorage::Register() {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IStorage::Open, "Open"},
            {1, nullptr, "OpenTransferStorage"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

IStorage::~IStorage() = default;

void IStorage::Open(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorageAccessor>(system, *this);
}

} // namespace Service::AM
