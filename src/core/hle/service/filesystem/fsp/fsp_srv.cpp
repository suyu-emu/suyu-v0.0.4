// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cinttypes>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/fs_directory.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/result.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/fsp/fs_i_filesystem.h"
#include "core/hle/service/filesystem/fsp/fs_i_save_data_info_reader.h"
#include "core/hle/service/filesystem/fsp/fs_i_storage.h"
#include "core/hle/service/filesystem/fsp/fsp_srv.h"
#include "core/hle/service/filesystem/romfs_controller.h"
#include "core/hle/service/filesystem/save_data_controller.h"
#include "core/hle/service/hle_ipc.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/loader/loader.h"
#include "core/reporter.h"

namespace Service::FileSystem {

enum class FileSystemProxyType : u8 {
    Code = 0,
    Rom = 1,
    Logo = 2,
    Control = 3,
    Manual = 4,
    Meta = 5,
    Data = 6,
    Package = 7,
    RegisteredUpdate = 8,
};

FSP_SRV::FSP_SRV(Core::System& system_)
    : ServiceFramework{system_, "fsp-srv"}, fsc{system.GetFileSystemController()},
      content_provider{system.GetContentProvider()}, reporter{system.GetReporter()} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "OpenFileSystem"},
        {1, &FSP_SRV::SetCurrentProcess, "SetCurrentProcess"},
        {2, nullptr, "OpenDataFileSystemByCurrentProcess"},
        {7, &FSP_SRV::OpenFileSystemWithPatch, "OpenFileSystemWithPatch"},
        {8, nullptr, "OpenFileSystemWithId"},
        {9, nullptr, "OpenDataFileSystemByApplicationId"},
        {11, nullptr, "OpenBisFileSystem"},
        {12, nullptr, "OpenBisStorage"},
        {13, nullptr, "InvalidateBisCache"},
        {17, nullptr, "OpenHostFileSystem"},
        {18, &FSP_SRV::OpenSdCardFileSystem, "OpenSdCardFileSystem"},
        {19, nullptr, "FormatSdCardFileSystem"},
        {21, nullptr, "DeleteSaveDataFileSystem"},
        {22, &FSP_SRV::CreateSaveDataFileSystem, "CreateSaveDataFileSystem"},
        {23, &FSP_SRV::CreateSaveDataFileSystemBySystemSaveDataId, "CreateSaveDataFileSystemBySystemSaveDataId"},
        {24, nullptr, "RegisterSaveDataFileSystemAtomicDeletion"},
        {25, nullptr, "DeleteSaveDataFileSystemBySaveDataSpaceId"},
        {26, nullptr, "FormatSdCardDryRun"},
        {27, nullptr, "IsExFatSupported"},
        {28, nullptr, "DeleteSaveDataFileSystemBySaveDataAttribute"},
        {30, nullptr, "OpenGameCardStorage"},
        {31, nullptr, "OpenGameCardFileSystem"},
        {32, nullptr, "ExtendSaveDataFileSystem"},
        {33, nullptr, "DeleteCacheStorage"},
        {34, &FSP_SRV::GetCacheStorageSize, "GetCacheStorageSize"},
        {35, nullptr, "CreateSaveDataFileSystemByHashSalt"},
        {36, nullptr, "OpenHostFileSystemWithOption"},
        {51, &FSP_SRV::OpenSaveDataFileSystem, "OpenSaveDataFileSystem"},
        {52, &FSP_SRV::OpenSaveDataFileSystemBySystemSaveDataId, "OpenSaveDataFileSystemBySystemSaveDataId"},
        {53, &FSP_SRV::OpenReadOnlySaveDataFileSystem, "OpenReadOnlySaveDataFileSystem"},
        {57, nullptr, "ReadSaveDataFileSystemExtraDataBySaveDataSpaceId"},
        {58, nullptr, "ReadSaveDataFileSystemExtraData"},
        {59, nullptr, "WriteSaveDataFileSystemExtraData"},
        {60, nullptr, "OpenSaveDataInfoReader"},
        {61, &FSP_SRV::OpenSaveDataInfoReaderBySaveDataSpaceId, "OpenSaveDataInfoReaderBySaveDataSpaceId"},
        {62, &FSP_SRV::OpenSaveDataInfoReaderOnlyCacheStorage, "OpenSaveDataInfoReaderOnlyCacheStorage"},
        {64, nullptr, "OpenSaveDataInternalStorageFileSystem"},
        {65, nullptr, "UpdateSaveDataMacForDebug"},
        {66, nullptr, "WriteSaveDataFileSystemExtraData2"},
        {67, nullptr, "FindSaveDataWithFilter"},
        {68, nullptr, "OpenSaveDataInfoReaderBySaveDataFilter"},
        {69, nullptr, "ReadSaveDataFileSystemExtraDataBySaveDataAttribute"},
        {70, &FSP_SRV::WriteSaveDataFileSystemExtraDataBySaveDataAttribute, "WriteSaveDataFileSystemExtraDataBySaveDataAttribute"},
        {71, &FSP_SRV::ReadSaveDataFileSystemExtraDataWithMaskBySaveDataAttribute, "ReadSaveDataFileSystemExtraDataWithMaskBySaveDataAttribute"},
        {80, nullptr, "OpenSaveDataMetaFile"},
        {81, nullptr, "OpenSaveDataTransferManager"},
        {82, nullptr, "OpenSaveDataTransferManagerVersion2"},
        {83, nullptr, "OpenSaveDataTransferProhibiterForCloudBackUp"},
        {84, nullptr, "ListApplicationAccessibleSaveDataOwnerId"},
        {85, nullptr, "OpenSaveDataTransferManagerForSaveDataRepair"},
        {86, nullptr, "OpenSaveDataMover"},
        {87, nullptr, "OpenSaveDataTransferManagerForRepair"},
        {100, nullptr, "OpenImageDirectoryFileSystem"},
        {101, nullptr, "OpenBaseFileSystem"},
        {102, nullptr, "FormatBaseFileSystem"},
        {110, nullptr, "OpenContentStorageFileSystem"},
        {120, nullptr, "OpenCloudBackupWorkStorageFileSystem"},
        {130, nullptr, "OpenCustomStorageFileSystem"},
        {200, &FSP_SRV::OpenDataStorageByCurrentProcess, "OpenDataStorageByCurrentProcess"},
        {201, nullptr, "OpenDataStorageByProgramId"},
        {202, &FSP_SRV::OpenDataStorageByDataId, "OpenDataStorageByDataId"},
        {203, &FSP_SRV::OpenPatchDataStorageByCurrentProcess, "OpenPatchDataStorageByCurrentProcess"},
        {204, nullptr, "OpenDataFileSystemByProgramIndex"},
        {205, &FSP_SRV::OpenDataStorageWithProgramIndex, "OpenDataStorageWithProgramIndex"},
        {206, nullptr, "OpenDataStorageByPath"},
        {400, nullptr, "OpenDeviceOperator"},
        {500, nullptr, "OpenSdCardDetectionEventNotifier"},
        {501, nullptr, "OpenGameCardDetectionEventNotifier"},
        {510, nullptr, "OpenSystemDataUpdateEventNotifier"},
        {511, nullptr, "NotifySystemDataUpdateEvent"},
        {520, nullptr, "SimulateGameCardDetectionEvent"},
        {600, nullptr, "SetCurrentPosixTime"},
        {601, nullptr, "QuerySaveDataTotalSize"},
        {602, nullptr, "VerifySaveDataFileSystem"},
        {603, nullptr, "CorruptSaveDataFileSystem"},
        {604, nullptr, "CreatePaddingFile"},
        {605, nullptr, "DeleteAllPaddingFiles"},
        {606, nullptr, "GetRightsId"},
        {607, nullptr, "RegisterExternalKey"},
        {608, nullptr, "UnregisterAllExternalKey"},
        {609, nullptr, "GetRightsIdByPath"},
        {610, nullptr, "GetRightsIdAndKeyGenerationByPath"},
        {611, nullptr, "SetCurrentPosixTimeWithTimeDifference"},
        {612, nullptr, "GetFreeSpaceSizeForSaveData"},
        {613, nullptr, "VerifySaveDataFileSystemBySaveDataSpaceId"},
        {614, nullptr, "CorruptSaveDataFileSystemBySaveDataSpaceId"},
        {615, nullptr, "QuerySaveDataInternalStorageTotalSize"},
        {616, nullptr, "GetSaveDataCommitId"},
        {617, nullptr, "UnregisterExternalKey"},
        {620, nullptr, "SetSdCardEncryptionSeed"},
        {630, nullptr, "SetSdCardAccessibility"},
        {631, nullptr, "IsSdCardAccessible"},
        {640, nullptr, "IsSignedSystemPartitionOnSdCardValid"},
        {700, nullptr, "OpenAccessFailureResolver"},
        {701, nullptr, "GetAccessFailureDetectionEvent"},
        {702, nullptr, "IsAccessFailureDetected"},
        {710, nullptr, "ResolveAccessFailure"},
        {720, nullptr, "AbandonAccessFailure"},
        {800, nullptr, "GetAndClearFileSystemProxyErrorInfo"},
        {810, nullptr, "RegisterProgramIndexMapInfo"},
        {1000, nullptr, "SetBisRootForHost"},
        {1001, nullptr, "SetSaveDataSize"},
        {1002, nullptr, "SetSaveDataRootPath"},
        {1003, &FSP_SRV::DisableAutoSaveDataCreation, "DisableAutoSaveDataCreation"},
        {1004, &FSP_SRV::SetGlobalAccessLogMode, "SetGlobalAccessLogMode"},
        {1005, &FSP_SRV::GetGlobalAccessLogMode, "GetGlobalAccessLogMode"},
        {1006, &FSP_SRV::OutputAccessLogToSdCard, "OutputAccessLogToSdCard"},
        {1007, nullptr, "RegisterUpdatePartition"},
        {1008, nullptr, "OpenRegisteredUpdatePartition"},
        {1009, nullptr, "GetAndClearMemoryReportInfo"},
        {1010, nullptr, "SetDataStorageRedirectTarget"},
        {1011, &FSP_SRV::GetProgramIndexForAccessLog, "GetProgramIndexForAccessLog"},
        {1012, nullptr, "GetFsStackUsage"},
        {1013, nullptr, "UnsetSaveDataRootPath"},
        {1014, nullptr, "OutputMultiProgramTagAccessLog"},
        {1016, &FSP_SRV::FlushAccessLogOnSdCard, "FlushAccessLogOnSdCard"},
        {1017, nullptr, "OutputApplicationInfoAccessLog"},
        {1018, nullptr, "SetDebugOption"},
        {1019, nullptr, "UnsetDebugOption"},
        {1100, nullptr, "OverrideSaveDataTransferTokenSignVerificationKey"},
        {1110, nullptr, "CorruptSaveDataFileSystemBySaveDataSpaceId2"},
        {1200, &FSP_SRV::OpenMultiCommitManager, "OpenMultiCommitManager"},
        {1300, nullptr, "OpenBisWiper"},
    };
    // clang-format on
    RegisterHandlers(functions);

    if (Settings::values.enable_fs_access_log) {
        access_log_mode = AccessLogMode::SdCard;
    }
}

FSP_SRV::~FSP_SRV() = default;

void FSP_SRV::SetCurrentProcess(HLERequestContext& ctx) {
    current_process_id = ctx.GetPID();

    LOG_DEBUG(Service_FS, "called. current_process_id=0x{:016X}", current_process_id);

    const auto res =
        fsc.OpenProcess(&program_id, &save_data_controller, &romfs_controller, current_process_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void FSP_SRV::OpenFileSystemWithPatch(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    struct InputParameters {
        FileSystemProxyType type;
        u64 program_id;
    };
    static_assert(sizeof(InputParameters) == 0x10, "InputParameters has wrong size");

    const auto params = rp.PopRaw<InputParameters>();
    LOG_ERROR(Service_FS, "(STUBBED) called with type={}, program_id={:016X}", params.type,
              params.program_id);

    // FIXME: many issues with this
    ASSERT(params.type == FileSystemProxyType::Manual);
    const auto manual_romfs = romfs_controller->OpenPatchedRomFS(
        params.program_id, FileSys::ContentRecordType::HtmlDocument);

    ASSERT(manual_romfs != nullptr);

    const auto extracted_romfs = FileSys::ExtractRomFS(manual_romfs);
    ASSERT(extracted_romfs != nullptr);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IFileSystem>(system, extracted_romfs,
                                     SizeGetter::FromStorageId(fsc, FileSys::StorageId::NandUser));
}

void FSP_SRV::OpenSdCardFileSystem(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    FileSys::VirtualDir sdmc_dir{};
    fsc.OpenSDMC(&sdmc_dir);

    auto filesystem = std::make_shared<IFileSystem>(
        system, sdmc_dir, SizeGetter::FromStorageId(fsc, FileSys::StorageId::SdCard));

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IFileSystem>(std::move(filesystem));
}

void FSP_SRV::CreateSaveDataFileSystem(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto save_struct = rp.PopRaw<FileSys::SaveDataAttribute>();
    [[maybe_unused]] auto save_create_struct = rp.PopRaw<std::array<u8, 0x40>>();
    u128 uid = rp.PopRaw<u128>();

    LOG_DEBUG(Service_FS, "called save_struct = {}, uid = {:016X}{:016X}", save_struct.DebugInfo(),
              uid[1], uid[0]);

    FileSys::VirtualDir save_data_dir{};
    save_data_controller->CreateSaveData(&save_data_dir, FileSys::SaveDataSpaceId::NandUser,
                                         save_struct);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::CreateSaveDataFileSystemBySystemSaveDataId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto save_struct = rp.PopRaw<FileSys::SaveDataAttribute>();
    [[maybe_unused]] auto save_create_struct = rp.PopRaw<std::array<u8, 0x40>>();

    LOG_DEBUG(Service_FS, "called save_struct = {}", save_struct.DebugInfo());

    FileSys::VirtualDir save_data_dir{};
    save_data_controller->CreateSaveData(&save_data_dir, FileSys::SaveDataSpaceId::NandSystem,
                                         save_struct);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::OpenSaveDataFileSystem(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    struct Parameters {
        FileSys::SaveDataSpaceId space_id;
        FileSys::SaveDataAttribute attribute;
    };

    const auto parameters = rp.PopRaw<Parameters>();

    LOG_INFO(Service_FS, "called.");

    FileSys::VirtualDir dir{};
    auto result =
        save_data_controller->OpenSaveData(&dir, parameters.space_id, parameters.attribute);
    if (result != ResultSuccess) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 0};
        rb.Push(FileSys::ResultTargetNotFound);
        return;
    }

    FileSys::StorageId id{};
    switch (parameters.space_id) {
    case FileSys::SaveDataSpaceId::NandUser:
        id = FileSys::StorageId::NandUser;
        break;
    case FileSys::SaveDataSpaceId::SdCardSystem:
    case FileSys::SaveDataSpaceId::SdCardUser:
        id = FileSys::StorageId::SdCard;
        break;
    case FileSys::SaveDataSpaceId::NandSystem:
        id = FileSys::StorageId::NandSystem;
        break;
    case FileSys::SaveDataSpaceId::TemporaryStorage:
    case FileSys::SaveDataSpaceId::ProperSystem:
    case FileSys::SaveDataSpaceId::SafeMode:
        ASSERT(false);
    }

    auto filesystem =
        std::make_shared<IFileSystem>(system, std::move(dir), SizeGetter::FromStorageId(fsc, id));

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IFileSystem>(std::move(filesystem));
}

void FSP_SRV::OpenSaveDataFileSystemBySystemSaveDataId(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called, delegating to 51 OpenSaveDataFilesystem");
    OpenSaveDataFileSystem(ctx);
}

void FSP_SRV::OpenReadOnlySaveDataFileSystem(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called, delegating to 51 OpenSaveDataFilesystem");
    OpenSaveDataFileSystem(ctx);
}

void FSP_SRV::OpenSaveDataInfoReaderBySaveDataSpaceId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto space = rp.PopRaw<FileSys::SaveDataSpaceId>();
    LOG_INFO(Service_FS, "called, space={}", space);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISaveDataInfoReader>(
        std::make_shared<ISaveDataInfoReader>(system, save_data_controller, space));
}

void FSP_SRV::OpenSaveDataInfoReaderOnlyCacheStorage(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISaveDataInfoReader>(system, save_data_controller,
                                             FileSys::SaveDataSpaceId::TemporaryStorage);
}

void FSP_SRV::WriteSaveDataFileSystemExtraDataBySaveDataAttribute(HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::ReadSaveDataFileSystemExtraDataWithMaskBySaveDataAttribute(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    struct Parameters {
        FileSys::SaveDataSpaceId space_id;
        FileSys::SaveDataAttribute attribute;
    };

    const auto parameters = rp.PopRaw<Parameters>();
    // Stub this to None for now, backend needs an impl to read/write the SaveDataExtraData
    constexpr auto flags = static_cast<u32>(FileSys::SaveDataFlags::None);

    LOG_WARNING(Service_FS,
                "(STUBBED) called, flags={}, space_id={}, attribute.title_id={:016X}\n"
                "attribute.user_id={:016X}{:016X}, attribute.save_id={:016X}\n"
                "attribute.type={}, attribute.rank={}, attribute.index={}",
                flags, parameters.space_id, parameters.attribute.title_id,
                parameters.attribute.user_id[1], parameters.attribute.user_id[0],
                parameters.attribute.save_id, parameters.attribute.type, parameters.attribute.rank,
                parameters.attribute.index);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(flags);
}

void FSP_SRV::OpenDataStorageByCurrentProcess(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    if (!romfs) {
        auto current_romfs = romfs_controller->OpenRomFSCurrentProcess();
        if (!current_romfs) {
            // TODO (bunnei): Find the right error code to use here
            LOG_CRITICAL(Service_FS, "no file system interface available!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultUnknown);
            return;
        }

        romfs = current_romfs;
    }

    auto storage = std::make_shared<IStorage>(system, romfs);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(std::move(storage));
}

void FSP_SRV::OpenDataStorageByDataId(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto storage_id = rp.PopRaw<FileSys::StorageId>();
    const auto unknown = rp.PopRaw<u32>();
    const auto title_id = rp.PopRaw<u64>();

    LOG_DEBUG(Service_FS, "called with storage_id={:02X}, unknown={:08X}, title_id={:016X}",
              storage_id, unknown, title_id);

    auto data = romfs_controller->OpenRomFS(title_id, storage_id, FileSys::ContentRecordType::Data);

    if (!data) {
        const auto archive = FileSys::SystemArchive::SynthesizeSystemArchive(title_id);

        if (archive != nullptr) {
            IPC::ResponseBuilder rb{ctx, 2, 0, 1};
            rb.Push(ResultSuccess);
            rb.PushIpcInterface(std::make_shared<IStorage>(system, archive));
            return;
        }

        // TODO(DarkLordZach): Find the right error code to use here
        LOG_ERROR(Service_FS,
                  "could not open data storage with title_id={:016X}, storage_id={:02X}", title_id,
                  storage_id);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    const FileSys::PatchManager pm{title_id, fsc, content_provider};

    auto base =
        romfs_controller->OpenBaseNca(title_id, storage_id, FileSys::ContentRecordType::Data);
    auto storage = std::make_shared<IStorage>(
        system, pm.PatchRomFS(base.get(), std::move(data), FileSys::ContentRecordType::Data));

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(std::move(storage));
}

void FSP_SRV::OpenPatchDataStorageByCurrentProcess(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto storage_id = rp.PopRaw<FileSys::StorageId>();
    const auto title_id = rp.PopRaw<u64>();

    LOG_DEBUG(Service_FS, "called with storage_id={:02X}, title_id={:016X}", storage_id, title_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(FileSys::ResultTargetNotFound);
}

void FSP_SRV::OpenDataStorageWithProgramIndex(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto program_index = rp.PopRaw<u8>();

    LOG_DEBUG(Service_FS, "called, program_index={}", program_index);

    auto patched_romfs = romfs_controller->OpenPatchedRomFSWithProgramIndex(
        program_id, program_index, FileSys::ContentRecordType::Program);

    if (!patched_romfs) {
        // TODO: Find the right error code to use here
        LOG_ERROR(Service_FS, "could not open storage with program_index={}", program_index);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto storage = std::make_shared<IStorage>(system, std::move(patched_romfs));

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(std::move(storage));
}

void FSP_SRV::DisableAutoSaveDataCreation(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    save_data_controller->SetAutoCreate(false);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::SetGlobalAccessLogMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    access_log_mode = rp.PopEnum<AccessLogMode>();

    LOG_DEBUG(Service_FS, "called, access_log_mode={}", access_log_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::GetGlobalAccessLogMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(access_log_mode);
}

void FSP_SRV::OutputAccessLogToSdCard(HLERequestContext& ctx) {
    const auto raw = ctx.ReadBufferCopy();
    auto log = Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(raw.data()), raw.size());

    LOG_DEBUG(Service_FS, "called");

    reporter.SaveFSAccessLog(log);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::GetProgramIndexForAccessLog(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(AccessLogVersion::Latest);
    rb.Push(access_log_program_index);
}

void FSP_SRV::FlushAccessLogOnSdCard(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void FSP_SRV::GetCacheStorageSize(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto index{rp.Pop<s32>()};

    LOG_WARNING(Service_FS, "(STUBBED) called with index={}", index);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push(s64{0});
    rb.Push(s64{0});
}

class IMultiCommitManager final : public ServiceFramework<IMultiCommitManager> {
public:
    explicit IMultiCommitManager(Core::System& system_)
        : ServiceFramework{system_, "IMultiCommitManager"} {
        static const FunctionInfo functions[] = {
            {1, &IMultiCommitManager::Add, "Add"},
            {2, &IMultiCommitManager::Commit, "Commit"},
        };
        RegisterHandlers(functions);
    }

private:
    FileSys::VirtualFile backend;

    void Add(HLERequestContext& ctx) {
        LOG_WARNING(Service_FS, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Commit(HLERequestContext& ctx) {
        LOG_WARNING(Service_FS, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
};

void FSP_SRV::OpenMultiCommitManager(HLERequestContext& ctx) {
    LOG_DEBUG(Service_FS, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IMultiCommitManager>(std::make_shared<IMultiCommitManager>(system));
}

} // namespace Service::FileSystem
