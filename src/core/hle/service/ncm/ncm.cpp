// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "core/core.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/hle/api_version.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ncm/ncm.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::NCM {

class ILocationResolver final : public ServiceFramework<ILocationResolver> {
public:
    explicit ILocationResolver(Core::System& system_, FileSys::StorageId id)
        : ServiceFramework{system_, "ILocationResolver"}, storage{id} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ResolveProgramPath"},
            {1, nullptr, "RedirectProgramPath"},
            {2, nullptr, "ResolveApplicationControlPath"},
            {3, nullptr, "ResolveApplicationHtmlDocumentPath"},
            {4, nullptr, "ResolveDataPath"},
            {5, nullptr, "RedirectApplicationControlPath"},
            {6, nullptr, "RedirectApplicationHtmlDocumentPath"},
            {7, nullptr, "ResolveApplicationLegalInformationPath"},
            {8, nullptr, "RedirectApplicationLegalInformationPath"},
            {9, nullptr, "Refresh"},
            {10, nullptr, "RedirectApplicationProgramPath"},
            {11, nullptr, "ClearApplicationRedirection"},
            {12, nullptr, "EraseProgramRedirection"},
            {13, nullptr, "EraseApplicationControlRedirection"},
            {14, nullptr, "EraseApplicationHtmlDocumentRedirection"},
            {15, nullptr, "EraseApplicationLegalInformationRedirection"},
            {16, nullptr, "ResolveProgramPathForDebug"},
            {17, nullptr, "RedirectProgramPathForDebug"},
            {18, nullptr, "RedirectApplicationProgramPathForDebug"},
            {19, nullptr, "EraseProgramRedirectionForDebug"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    [[maybe_unused]] FileSys::StorageId storage;
};

class IRegisteredLocationResolver final : public ServiceFramework<IRegisteredLocationResolver> {
public:
    explicit IRegisteredLocationResolver(Core::System& system_)
        : ServiceFramework{system_, "IRegisteredLocationResolver"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ResolveProgramPath"},
            {1, nullptr, "RegisterProgramPath"},
            {2, nullptr, "UnregisterProgramPath"},
            {3, nullptr, "RedirectProgramPath"},
            {4, nullptr, "ResolveHtmlDocumentPath"},
            {5, nullptr, "RegisterHtmlDocumentPath"},
            {6, nullptr, "UnregisterHtmlDocumentPath"},
            {7, nullptr, "RedirectHtmlDocumentPath"},
            {8, nullptr, "Refresh"},
            {9, nullptr, "RefreshExcluding"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IAddOnContentLocationResolver final : public ServiceFramework<IAddOnContentLocationResolver> {
public:
    explicit IAddOnContentLocationResolver(Core::System& system_)
        : ServiceFramework{system_, "IAddOnContentLocationResolver"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ResolveAddOnContentPath"},
            {1, nullptr, "RegisterAddOnContentStorage"},
            {2, nullptr, "UnregisterAllAddOnContentPath"},
            {3, nullptr, "RefreshApplicationAddOnContent"},
            {4, nullptr, "UnregisterApplicationAddOnContent"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IContentStorage final : public ServiceFramework<IContentStorage> {
public:
    explicit IContentStorage(Core::System& system_, FileSys::StorageId id)
        : ServiceFramework{system_, "IContentStorage"}, storage{id} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IContentStorage::GeneratePlaceHolderId, "GeneratePlaceHolderId"},
            {1, &IContentStorage::CreatePlaceHolder, "CreatePlaceHolder"},
            {2, &IContentStorage::DeletePlaceHolder, "DeletePlaceHolder"},
            {4, &IContentStorage::WritePlaceHolder, "WritePlaceHolder"},
            {5, &IContentStorage::Register, "Register"},
            {6, &IContentStorage::Delete, "Delete"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GeneratePlaceHolderId(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NCM, "called");

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);
        rb.PushRaw(FileSys::PlaceholderCache::Generate());
    }

    void CreatePlaceHolder(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        [[maybe_unused]] FileSys::NcaID content_id{};
        FileSys::NcaID placeholder_id{};
        if constexpr (HLE::ApiVersion::HOS_VERSION_MAJOR >= 16) {
            placeholder_id = rp.PopRaw<FileSys::NcaID>();
            content_id = rp.PopRaw<FileSys::NcaID>();
        } else {
            content_id = rp.PopRaw<FileSys::NcaID>();
            placeholder_id = rp.PopRaw<FileSys::NcaID>();
        }
        const auto size = rp.Pop<s64>();

        auto* const placeholder_cache =
            system.GetFileSystemController().GetPlaceholderCacheForStorage(storage);
        const bool succeeded =
            placeholder_cache != nullptr && size >= 0 &&
            (placeholder_cache->Exists(placeholder_id) ||
             placeholder_cache->Create(placeholder_id, static_cast<u64>(size)));

        if (succeeded) {
            LOG_DEBUG(Service_NCM, "called, storage_id={}, size={}", static_cast<u32>(storage),
                      size);
        } else {
            LOG_WARNING(Service_NCM, "failed, storage_id={}, size={}", static_cast<u32>(storage),
                        size);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(succeeded ? ResultSuccess : ResultUnknown);
    }

    void DeletePlaceHolder(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto placeholder_id = rp.PopRaw<FileSys::NcaID>();

        auto* const placeholder_cache =
            system.GetFileSystemController().GetPlaceholderCacheForStorage(storage);
        const bool succeeded =
            placeholder_cache != nullptr &&
            (!placeholder_cache->Exists(placeholder_id) || placeholder_cache->Delete(placeholder_id));

        if (succeeded) {
            LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage));
        } else {
            LOG_WARNING(Service_NCM, "failed, storage_id={}", static_cast<u32>(storage));
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(succeeded ? ResultSuccess : ResultUnknown);
    }

    void WritePlaceHolder(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto placeholder_id = rp.PopRaw<FileSys::NcaID>();
        const auto offset = rp.Pop<u64>();
        const auto data = ctx.ReadBuffer();

        auto* const placeholder_cache =
            system.GetFileSystemController().GetPlaceholderCacheForStorage(storage);
        const std::vector<u8> write_data{data.begin(), data.end()};
        const bool succeeded =
            placeholder_cache != nullptr &&
            placeholder_cache->Write(placeholder_id, offset, write_data);

        if (succeeded) {
            LOG_DEBUG(Service_NCM, "called, storage_id={}, offset={}, size={}",
                      static_cast<u32>(storage), offset, data.size());
        } else {
            LOG_WARNING(Service_NCM, "failed, storage_id={}, offset={}, size={}",
                        static_cast<u32>(storage), offset, data.size());
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(succeeded ? ResultSuccess : ResultUnknown);
    }

    void Register(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        FileSys::NcaID content_id{};
        FileSys::NcaID placeholder_id{};
        if constexpr (HLE::ApiVersion::HOS_VERSION_MAJOR >= 16) {
            placeholder_id = rp.PopRaw<FileSys::NcaID>();
            content_id = rp.PopRaw<FileSys::NcaID>();
        } else {
            content_id = rp.PopRaw<FileSys::NcaID>();
            placeholder_id = rp.PopRaw<FileSys::NcaID>();
        }

        auto& fsc = system.GetFileSystemController();
        auto* const placeholder_cache = fsc.GetPlaceholderCacheForStorage(storage);
        auto* const registered_cache = fsc.GetRegisteredCacheForStorage(storage);
        const bool succeeded =
            placeholder_cache != nullptr && registered_cache != nullptr &&
            placeholder_cache->Register(registered_cache, placeholder_id, content_id);

        if (succeeded) {
            LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage));
        } else {
            LOG_WARNING(Service_NCM, "failed, storage_id={}", static_cast<u32>(storage));
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(succeeded ? ResultSuccess : ResultUnknown);
    }

    void Delete(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto content_id = rp.PopRaw<FileSys::NcaID>();

        auto* const registered_cache =
            system.GetFileSystemController().GetRegisteredCacheForStorage(storage);
        const bool succeeded = registered_cache != nullptr && registered_cache->Delete(content_id);
        if (succeeded) {
            registered_cache->Refresh();
        }

        if (succeeded) {
            LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage));
        } else {
            LOG_WARNING(Service_NCM, "failed, storage_id={}", static_cast<u32>(storage));
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(succeeded ? ResultSuccess : ResultUnknown);
    }

    FileSys::StorageId storage;
};

class IContentMetaDatabase final : public ServiceFramework<IContentMetaDatabase> {
public:
    explicit IContentMetaDatabase(Core::System& system_, FileSys::StorageId id)
        : ServiceFramework{system_, "IContentMetaDatabase"}, storage{id} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IContentMetaDatabase::Set, "Set"},
            {2, &IContentMetaDatabase::Remove, "Remove"},
            {8, &IContentMetaDatabase::Has, "Has"},
            {15, &IContentMetaDatabase::Commit, "Commit"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    struct ContentMetaKey {
        u64 id;
        u32 version;
        FileSys::TitleType type;
        u8 install_type;
        std::array<u8, 2> padding;
    };
    static_assert(sizeof(ContentMetaKey) == 0x10);

    void Set(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto key = rp.PopRaw<ContentMetaKey>();

        const auto entry_matches = [&key](const ContentMetaKey& entry) {
            return entry.id == key.id && entry.version == key.version && entry.type == key.type &&
                   entry.install_type == key.install_type;
        };
        if (std::find_if(entries.begin(), entries.end(), entry_matches) == entries.end()) {
            entries.push_back(key);
        }

        LOG_DEBUG(Service_NCM,
                  "called, storage_id={}, title_id={:016X}, version={}, type={}, size={}",
                  static_cast<u32>(storage), key.id, key.version, static_cast<u8>(key.type),
                  ctx.GetReadBufferSize());

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Remove(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto key = rp.PopRaw<ContentMetaKey>();

        std::erase_if(entries, [&key](const ContentMetaKey& entry) {
            return entry.id == key.id && entry.version == key.version && entry.type == key.type &&
                   entry.install_type == key.install_type;
        });

        LOG_DEBUG(Service_NCM, "called, storage_id={}, title_id={:016X}, version={}, type={}",
                  static_cast<u32>(storage), key.id, key.version, static_cast<u8>(key.type));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Has(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto key = rp.PopRaw<ContentMetaKey>();

        const bool has_pending =
            std::find_if(entries.begin(), entries.end(), [&key](const ContentMetaKey& entry) {
                return entry.id == key.id && entry.version == key.version &&
                       entry.type == key.type && entry.install_type == key.install_type;
            }) != entries.end();

        auto* const registered_cache =
            system.GetFileSystemController().GetRegisteredCacheForStorage(storage);
        const bool has_registered =
            registered_cache != nullptr &&
            registered_cache->HasEntry(key.id, FileSys::ContentRecordType::Meta);

        LOG_DEBUG(Service_NCM, "called, storage_id={}, title_id={:016X}, version={}, type={}, has={}",
                  static_cast<u32>(storage), key.id, key.version, static_cast<u8>(key.type),
                  has_pending || has_registered);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(has_pending || has_registered);
    }

    void Commit(HLERequestContext& ctx) {
        auto* const registered_cache =
            system.GetFileSystemController().GetRegisteredCacheForStorage(storage);
        if (registered_cache != nullptr) {
            registered_cache->Refresh();
        }

        LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    FileSys::StorageId storage;
    std::vector<ContentMetaKey> entries;
};

class LR final : public ServiceFramework<LR> {
public:
    explicit LR(Core::System& system_) : ServiceFramework{system_, "lr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "OpenLocationResolver"},
            {1, nullptr, "OpenRegisteredLocationResolver"},
            {2, nullptr, "RefreshLocationResolver"},
            {3, nullptr, "OpenAddOnContentLocationResolver"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NCM final : public ServiceFramework<NCM> {
public:
    explicit NCM(Core::System& system_) : ServiceFramework{system_, "ncm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateContentStorage"},
            {1, nullptr, "CreateContentMetaDatabase"},
            {2, nullptr, "VerifyContentStorage"},
            {3, nullptr, "VerifyContentMetaDatabase"},
            {4, &NCM::OpenContentStorage, "OpenContentStorage"},
            {5, &NCM::OpenContentMetaDatabase, "OpenContentMetaDatabase"},
            {6, nullptr, "CloseContentStorageForcibly"},
            {7, nullptr, "CloseContentMetaDatabaseForcibly"},
            {8, nullptr, "CleanupContentMetaDatabase"},
            {9, nullptr, "ActivateContentStorage"},
            {10, nullptr, "InactivateContentStorage"},
            {11, nullptr, "ActivateContentMetaDatabase"},
            {12, nullptr, "InactivateContentMetaDatabase"},
            {13, nullptr, "InvalidateRightsIdCache"},
            {14, nullptr, "GetMemoryReport"},
            {15, nullptr, "ActivateFsContentStorage"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void OpenContentStorage(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto storage_id = rp.PopEnum<FileSys::StorageId>();

        LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage_id));

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IContentStorage>(ctx, system, storage_id);
    }

    void OpenContentMetaDatabase(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto storage_id = rp.PopEnum<FileSys::StorageId>();

        LOG_DEBUG(Service_NCM, "called, storage_id={}", static_cast<u32>(storage_id));

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IContentMetaDatabase>(ctx, system, storage_id);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("lr", std::make_shared<LR>(system));
    server_manager->RegisterNamedService("ncm", std::make_shared<NCM>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NCM
