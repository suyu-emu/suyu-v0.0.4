// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/store_data.h"
#include "core/hle/service/mii/types/ver3_store_data.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Mii {

class IDatabaseService final : public ServiceFramework<IDatabaseService> {
public:
    explicit IDatabaseService(Core::System& system_, std::shared_ptr<MiiManager> mii_manager,
                              bool is_system_)
        : ServiceFramework{system_, "IDatabaseService"}, manager{mii_manager}, is_system{
                                                                                   is_system_} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDatabaseService::IsUpdated, "IsUpdated"},
            {1, &IDatabaseService::IsFullDatabase, "IsFullDatabase"},
            {2, &IDatabaseService::GetCount, "GetCount"},
            {3, &IDatabaseService::Get, "Get"},
            {4, &IDatabaseService::Get1, "Get1"},
            {5, &IDatabaseService::UpdateLatest, "UpdateLatest"},
            {6, &IDatabaseService::BuildRandom, "BuildRandom"},
            {7, &IDatabaseService::BuildDefault, "BuildDefault"},
            {8, &IDatabaseService::Get2, "Get2"},
            {9, &IDatabaseService::Get3, "Get3"},
            {10, &IDatabaseService::UpdateLatest1, "UpdateLatest1"},
            {11, &IDatabaseService::FindIndex, "FindIndex"},
            {12, &IDatabaseService::Move, "Move"},
            {13, &IDatabaseService::AddOrReplace, "AddOrReplace"},
            {14, &IDatabaseService::Delete, "Delete"},
            {15, &IDatabaseService::DestroyFile, "DestroyFile"},
            {16, &IDatabaseService::DeleteFile, "DeleteFile"},
            {17, &IDatabaseService::Format, "Format"},
            {18, nullptr, "Import"},
            {19, nullptr, "Export"},
            {20, &IDatabaseService::IsBrokenDatabaseWithClearFlag, "IsBrokenDatabaseWithClearFlag"},
            {21, &IDatabaseService::GetIndex, "GetIndex"},
            {22, &IDatabaseService::SetInterfaceVersion, "SetInterfaceVersion"},
            {23, &IDatabaseService::Convert, "Convert"},
            {24, &IDatabaseService::ConvertCoreDataToCharInfo, "ConvertCoreDataToCharInfo"},
            {25, &IDatabaseService::ConvertCharInfoToCoreData, "ConvertCharInfoToCoreData"},
            {26,  &IDatabaseService::Append, "Append"},
        };
        // clang-format on

        RegisterHandlers(functions);

        manager->Initialize(metadata);
    }

private:
    void IsUpdated(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}", source_flag);

        const bool is_updated = manager->IsUpdated(metadata, source_flag);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u8>(is_updated);
    }

    void IsFullDatabase(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        const bool is_full_database = manager->IsFullDatabase();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u8>(is_full_database);
    }

    void GetCount(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        const u32 mii_count = manager->GetCount(metadata, source_flag);

        LOG_DEBUG(Service_Mii, "called with source_flag={}, mii_count={}", source_flag, mii_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(mii_count);
    }

    void Get(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<CharInfoElement>()};

        u32 mii_count{};
        std::vector<CharInfoElement> char_info_elements(output_size);
        const auto result = manager->Get(metadata, char_info_elements, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(char_info_elements);
        }

        LOG_INFO(Service_Mii, "called with source_flag={}, out_size={}, mii_count={}", source_flag,
                 output_size, mii_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void Get1(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<CharInfo>()};

        u32 mii_count{};
        std::vector<CharInfo> char_info(output_size);
        const auto result = manager->Get(metadata, char_info, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(char_info);
        }

        LOG_INFO(Service_Mii, "called with source_flag={}, out_size={}, mii_count={}", source_flag,
                 output_size, mii_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void UpdateLatest(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto char_info{rp.PopRaw<CharInfo>()};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_INFO(Service_Mii, "called with source_flag={}", source_flag);

        CharInfo new_char_info{};
        const auto result = manager->UpdateLatest(metadata, new_char_info, char_info, source_flag);
        if (result.IsFailure()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw(new_char_info);
    }

    void BuildRandom(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto age{rp.PopRaw<Age>()};
        const auto gender{rp.PopRaw<Gender>()};
        const auto race{rp.PopRaw<Race>()};

        LOG_DEBUG(Service_Mii, "called with age={}, gender={}, race={}", age, gender, race);

        if (age > Age::All) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultInvalidArgument);
            return;
        }

        if (gender > Gender::All) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultInvalidArgument);
            return;
        }

        if (race > Race::All) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultInvalidArgument);
            return;
        }

        CharInfo char_info{};
        manager->BuildRandom(char_info, age, gender, race);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw(char_info);
    }

    void BuildDefault(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto index{rp.Pop<u32>()};

        LOG_DEBUG(Service_Mii, "called with index={}", index);

        if (index > 5) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultInvalidArgument);
            return;
        }

        CharInfo char_info{};
        manager->BuildDefault(char_info, index);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw(char_info);
    }

    void Get2(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<StoreDataElement>()};

        u32 mii_count{};
        std::vector<StoreDataElement> store_data_elements(output_size);
        const auto result = manager->Get(metadata, store_data_elements, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(store_data_elements);
        }

        LOG_INFO(Service_Mii, "called with source_flag={}, out_size={}, mii_count={}", source_flag,
                 output_size, mii_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void Get3(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<StoreData>()};

        u32 mii_count{};
        std::vector<StoreData> store_data(output_size);
        const auto result = manager->Get(metadata, store_data, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(store_data);
        }

        LOG_INFO(Service_Mii, "called with source_flag={}, out_size={}, mii_count={}", source_flag,
                 output_size, mii_count);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void UpdateLatest1(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto store_data{rp.PopRaw<StoreData>()};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_INFO(Service_Mii, "called with source_flag={}", source_flag);

        Result result = ResultSuccess;
        if (!is_system) {
            result = ResultPermissionDenied;
        }

        StoreData new_store_data{};
        if (result.IsSuccess()) {
            result = manager->UpdateLatest(metadata, new_store_data, store_data, source_flag);
        }

        if (result.IsFailure()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(StoreData) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw<StoreData>(new_store_data);
    }

    void FindIndex(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto create_id{rp.PopRaw<Common::UUID>()};
        const auto is_special{rp.PopRaw<bool>()};

        LOG_INFO(Service_Mii, "called with create_id={}, is_special={}",
                 create_id.FormattedString(), is_special);

        const s32 index = manager->FindIndex(create_id, is_special);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(index);
    }

    void Move(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto create_id{rp.PopRaw<Common::UUID>()};
        const auto new_index{rp.PopRaw<s32>()};

        LOG_INFO(Service_Mii, "called with create_id={}, new_index={}", create_id.FormattedString(),
                 new_index);

        Result result = ResultSuccess;
        if (!is_system) {
            result = ResultPermissionDenied;
        }

        if (result.IsSuccess()) {
            const u32 count = manager->GetCount(metadata, SourceFlag::Database);
            if (new_index < 0 || new_index >= static_cast<s32>(count)) {
                result = ResultInvalidArgument;
            }
        }

        if (result.IsSuccess()) {
            result = manager->Move(metadata, new_index, create_id);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void AddOrReplace(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto store_data{rp.PopRaw<StoreData>()};

        LOG_INFO(Service_Mii, "called");

        Result result = ResultSuccess;

        if (!is_system) {
            result = ResultPermissionDenied;
        }

        if (result.IsSuccess()) {
            result = manager->AddOrReplace(metadata, store_data);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void Delete(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto create_id{rp.PopRaw<Common::UUID>()};

        LOG_INFO(Service_Mii, "called, create_id={}", create_id.FormattedString());

        Result result = ResultSuccess;

        if (!is_system) {
            result = ResultPermissionDenied;
        }

        if (result.IsSuccess()) {
            result = manager->Delete(metadata, create_id);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void DestroyFile(HLERequestContext& ctx) {
        // This calls nn::settings::fwdbg::GetSettingsItemValue("is_db_test_mode_enabled");
        const bool is_db_test_mode_enabled = false;

        LOG_INFO(Service_Mii, "called is_db_test_mode_enabled={}", is_db_test_mode_enabled);

        Result result = ResultSuccess;

        if (!is_db_test_mode_enabled) {
            result = ResultTestModeOnly;
        }

        if (result.IsSuccess()) {
            result = manager->DestroyFile(metadata);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void DeleteFile(HLERequestContext& ctx) {
        // This calls nn::settings::fwdbg::GetSettingsItemValue("is_db_test_mode_enabled");
        const bool is_db_test_mode_enabled = false;

        LOG_INFO(Service_Mii, "called is_db_test_mode_enabled={}", is_db_test_mode_enabled);

        Result result = ResultSuccess;

        if (!is_db_test_mode_enabled) {
            result = ResultTestModeOnly;
        }

        if (result.IsSuccess()) {
            result = manager->DeleteFile();
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void Format(HLERequestContext& ctx) {
        // This calls nn::settings::fwdbg::GetSettingsItemValue("is_db_test_mode_enabled");
        const bool is_db_test_mode_enabled = false;

        LOG_INFO(Service_Mii, "called is_db_test_mode_enabled={}", is_db_test_mode_enabled);

        Result result = ResultSuccess;

        if (!is_db_test_mode_enabled) {
            result = ResultTestModeOnly;
        }

        if (result.IsSuccess()) {
            result = manager->Format(metadata);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void IsBrokenDatabaseWithClearFlag(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        bool is_broken_with_clear_flag = false;
        Result result = ResultSuccess;

        if (!is_system) {
            result = ResultPermissionDenied;
        }

        if (result.IsSuccess()) {
            is_broken_with_clear_flag = manager->IsBrokenWithClearFlag(metadata);
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push<u8>(is_broken_with_clear_flag);
    }

    void GetIndex(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto info{rp.PopRaw<CharInfo>()};

        LOG_DEBUG(Service_Mii, "called");

        s32 index{};
        const auto result = manager->GetIndex(metadata, info, index);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(index);
    }

    void SetInterfaceVersion(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto interface_version{rp.PopRaw<u32>()};

        LOG_INFO(Service_Mii, "called, interface_version={:08X}", interface_version);

        manager->SetInterfaceVersion(metadata, interface_version);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Convert(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto mii_v3{rp.PopRaw<Ver3StoreData>()};

        LOG_INFO(Service_Mii, "called");

        CharInfo char_info{};
        const auto result = manager->ConvertV3ToCharInfo(char_info, mii_v3);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(result);
        rb.PushRaw<CharInfo>(char_info);
    }

    void ConvertCoreDataToCharInfo(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto core_data{rp.PopRaw<CoreData>()};

        LOG_INFO(Service_Mii, "called");

        CharInfo char_info{};
        const auto result = manager->ConvertCoreDataToCharInfo(char_info, core_data);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(result);
        rb.PushRaw<CharInfo>(char_info);
    }

    void ConvertCharInfoToCoreData(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto char_info{rp.PopRaw<CharInfo>()};

        LOG_INFO(Service_Mii, "called");

        CoreData core_data{};
        const auto result = manager->ConvertCharInfoToCoreData(core_data, char_info);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CoreData) / sizeof(u32)};
        rb.Push(result);
        rb.PushRaw<CoreData>(core_data);
    }

    void Append(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto char_info{rp.PopRaw<CharInfo>()};

        LOG_INFO(Service_Mii, "called");

        const auto result = manager->Append(metadata, char_info);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    std::shared_ptr<MiiManager> manager = nullptr;
    DatabaseSessionMetadata metadata{};
    bool is_system{};
};

MiiDBModule::MiiDBModule(Core::System& system_, const char* name_,
                         std::shared_ptr<MiiManager> mii_manager, bool is_system_)
    : ServiceFramework{system_, name_}, manager{mii_manager}, is_system{is_system_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &MiiDBModule::GetDatabaseService, "GetDatabaseService"},
    };
    // clang-format on

    RegisterHandlers(functions);

    if (manager == nullptr) {
        manager = std::make_shared<MiiManager>();
    }
}

MiiDBModule::~MiiDBModule() = default;

void MiiDBModule::GetDatabaseService(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDatabaseService>(system, manager, is_system);

    LOG_DEBUG(Service_Mii, "called");
}

std::shared_ptr<MiiManager> MiiDBModule::GetMiiManager() {
    return manager;
}

class MiiImg final : public ServiceFramework<MiiImg> {
public:
    explicit MiiImg(Core::System& system_) : ServiceFramework{system_, "miiimg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MiiImg::Initialize, "Initialize"},
            {10, nullptr, "Reload"},
            {11, &MiiImg::GetCount, "GetCount"},
            {12, nullptr, "IsEmpty"},
            {13, nullptr, "IsFull"},
            {14, nullptr, "GetAttribute"},
            {15, nullptr, "LoadImage"},
            {16, nullptr, "AddOrUpdateImage"},
            {17, nullptr, "DeleteImages"},
            {100, nullptr, "DeleteFile"},
            {101, nullptr, "DestroyFile"},
            {102, nullptr, "ImportFile"},
            {103, nullptr, "ExportFile"},
            {104, nullptr, "ForceInitialize"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Initialize(HLERequestContext& ctx) {
        LOG_INFO(Service_Mii, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetCount(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    std::shared_ptr<MiiManager> manager = nullptr;

    server_manager->RegisterNamedService(
        "mii:e", std::make_shared<MiiDBModule>(system, "mii:e", manager, true));
    server_manager->RegisterNamedService(
        "mii:u", std::make_shared<MiiDBModule>(system, "mii:u", manager, false));
    server_manager->RegisterNamedService("miiimg", std::make_shared<MiiImg>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Mii
