// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/bcat/bcat.h"
#include "core/hle/service/bcat/module.h"

namespace Service::BCAT {

class IBcatService final : public ServiceFramework<IBcatService> {
public:
    IBcatService(Backend& backend) : ServiceFramework("IBcatService"), backend(backend) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {10100, nullptr, "RequestSyncDeliveryCache"},
            {10101, nullptr, "RequestSyncDeliveryCacheWithDirectoryName"},
            {10200, nullptr, "CancelSyncDeliveryCacheRequest"},
            {20100, nullptr, "RequestSyncDeliveryCacheWithApplicationId"},
            {20101, nullptr, "RequestSyncDeliveryCacheWithApplicationIdAndDirectoryName"},
            {30100, nullptr, "SetPassphrase"},
            {30200, nullptr, "RegisterBackgroundDeliveryTask"},
            {30201, nullptr, "UnregisterBackgroundDeliveryTask"},
            {30202, nullptr, "BlockDeliveryTask"},
            {30203, nullptr, "UnblockDeliveryTask"},
            {90100, nullptr, "EnumerateBackgroundDeliveryTask"},
            {90200, nullptr, "GetDeliveryList"},
            {90201, nullptr, "ClearDeliveryCacheStorage"},
            {90300, nullptr, "GetPushNotificationLog"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void Module::Interface::CreateBcatService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_BCAT, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IBcatService>(*backend);
class IDeliveryCacheDirectoryService final
    : public ServiceFramework<IDeliveryCacheDirectoryService> {
public:
    IDeliveryCacheDirectoryService(FileSys::VirtualDir root_)
        : ServiceFramework{"IDeliveryCacheDirectoryService"}, root(std::move(root_)) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheDirectoryService::Open, "Open"},
            {1, &IDeliveryCacheDirectoryService::Read, "Read"},
            {2, &IDeliveryCacheDirectoryService::GetCount, "GetCount"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Open(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto name_raw = rp.PopRaw<DirectoryName>();
        const auto name =
            Common::StringFromFixedZeroTerminatedBuffer(name_raw.data(), name_raw.size());

        LOG_DEBUG(Service_BCAT, "called, name={}", name);

        if (!VerifyNameValidDir(ctx, name_raw))
            return;

        if (current_dir != nullptr) {
            LOG_ERROR(Service_BCAT, "A file has already been opened on this interface!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_ENTITY_ALREADY_OPEN);
            return;
        }

        current_dir = root->GetSubdirectory(name);

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "Failed to open the directory name={}!", name);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_FAILED_OPEN_ENTITY);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void Read(Kernel::HLERequestContext& ctx) {
        auto write_size = ctx.GetWriteBufferSize() / sizeof(DeliveryCacheDirectoryEntry);

        LOG_DEBUG(Service_BCAT, "called, write_size={:016X}", write_size);

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no open directory!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
            return;
        }

        const auto files = current_dir->GetFiles();
        write_size = std::min(write_size, files.size());
        std::vector<DeliveryCacheDirectoryEntry> entries(write_size);
        std::transform(
            files.begin(), files.begin() + write_size, entries.begin(), [](const auto& file) {
                FileName name{};
                std::memcpy(name.data(), file->GetName().data(),
                            std::min(file->GetName().size(), name.size()));
                return DeliveryCacheDirectoryEntry{name, file->GetSize(), DigestFile(file)};
            });

        ctx.WriteBuffer(entries);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(write_size * sizeof(DeliveryCacheDirectoryEntry));
    }

    void GetCount(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no open directory!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
            return;
        }

        const auto files = current_dir->GetFiles();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(files.size());
    }

    FileSys::VirtualDir root;
    FileSys::VirtualDir current_dir;
};

class IDeliveryCacheStorageService final : public ServiceFramework<IDeliveryCacheStorageService> {
public:
    IDeliveryCacheStorageService(FileSys::VirtualDir root_)
        : ServiceFramework{"IDeliveryCacheStorageService"}, root(std::move(root_)) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheStorageService::CreateFileService, "CreateFileService"},
            {1, &IDeliveryCacheStorageService::CreateDirectoryService, "CreateDirectoryService"},
            {10, &IDeliveryCacheStorageService::EnumerateDeliveryCacheDirectory, "EnumerateDeliveryCacheDirectory"},
        };
        // clang-format on

        RegisterHandlers(functions);

        for (const auto& subdir : root->GetSubdirectories()) {
            DirectoryName name{};
            std::memcpy(name.data(), subdir->GetName().data(),
                        std::min(sizeof(DirectoryName) - 1, subdir->GetName().size()));
            entries.push_back(name);
        }
    }

private:
    void CreateFileService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDeliveryCacheFileService>(root);
    }

    void CreateDirectoryService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDeliveryCacheDirectoryService>(root);
    }

    void EnumerateDeliveryCacheDirectory(Kernel::HLERequestContext& ctx) {
        auto size = ctx.GetWriteBufferSize() / sizeof(DirectoryName);

        LOG_DEBUG(Service_BCAT, "called, size={:016X}", size);

        size = std::min(size, entries.size() - next_read_index);
        ctx.WriteBuffer(entries.data() + next_read_index, size * sizeof(DirectoryName));
        next_read_index += size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(size);
    }

    FileSys::VirtualDir root;
    std::vector<DirectoryName> entries;
    u64 next_read_index = 0;
};

void Module::Interface::CreateDeliveryCacheStorageService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_BCAT, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IDeliveryCacheStorageService>(
        Service::FileSystem::GetBCATDirectory(Core::CurrentProcess()->GetTitleID()));
}

void Module::Interface::CreateDeliveryCacheStorageServiceWithApplicationId(
    Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto title_id = rp.PopRaw<u64>();

    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}", title_id);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IDeliveryCacheStorageService>(
        Service::FileSystem::GetBCATDirectory(title_id));
}

namespace {
std::unique_ptr<Backend> CreateBackendFromSettings(DirectoryGetter getter) {
    const auto backend = Settings::values.bcat_backend;

#ifdef YUZU_ENABLE_BOXCAT
    if (backend == "boxcat")
        return std::make_unique<Boxcat>(std::move(getter));
#endif

    return std::make_unique<NullBackend>(std::move(getter));
}
} // Anonymous namespace

Module::Interface::Interface(std::shared_ptr<Module> module, const char* name)
    : ServiceFramework(name), module(std::move(module)),
      backend(CreateBackendFromSettings(&Service::FileSystem::GetBCATDirectory)) {}

Module::Interface::~Interface() = default;

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto module = std::make_shared<Module>();
    std::make_shared<BCAT>(module, "bcat:a")->InstallAsService(service_manager);
    std::make_shared<BCAT>(module, "bcat:m")->InstallAsService(service_manager);
    std::make_shared<BCAT>(module, "bcat:u")->InstallAsService(service_manager);
    std::make_shared<BCAT>(module, "bcat:s")->InstallAsService(service_manager);
}

} // namespace Service::BCAT
