// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Mii {

class IDatabaseService final : public ServiceFramework<IDatabaseService> {
public:
    explicit IDatabaseService(Core::System& system_, bool is_system_)
        : ServiceFramework{system_, "IDatabaseService"}, is_system{is_system_} {
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
            {8, nullptr, "Get2"},
            {9, nullptr, "Get3"},
            {10, nullptr, "UpdateLatest1"},
            {11, nullptr, "FindIndex"},
            {12, nullptr, "Move"},
            {13, nullptr, "AddOrReplace"},
            {14, nullptr, "Delete"},
            {15, nullptr, "DestroyFile"},
            {16, nullptr, "DeleteFile"},
            {17, nullptr, "Format"},
            {18, nullptr, "Import"},
            {19, nullptr, "Export"},
            {20, nullptr, "IsBrokenDatabaseWithClearFlag"},
            {21, &IDatabaseService::GetIndex, "GetIndex"},
            {22, &IDatabaseService::SetInterfaceVersion, "SetInterfaceVersion"},
            {23, &IDatabaseService::Convert, "Convert"},
            {24, nullptr, "ConvertCoreDataToCharInfo"},
            {25, nullptr, "ConvertCharInfoToCoreData"},
            {26, nullptr, "Append"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void IsUpdated(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}", source_flag);

        const bool is_updated = manager.IsUpdated(metadata, source_flag);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u8>(is_updated);
    }

    void IsFullDatabase(HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        const bool is_full_database = manager.IsFullDatabase();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u8>(is_full_database);
    }

    void GetCount(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}", source_flag);

        const u32 mii_count = manager.GetCount(metadata, source_flag);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(mii_count);
    }

    void Get(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<CharInfoElement>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}, out_size={}", source_flag, output_size);

        u32 mii_count{};
        std::vector<CharInfoElement> char_info_elements(output_size);
        Result result = manager.Get(metadata, char_info_elements, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(char_info_elements);
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void Get1(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto source_flag{rp.PopRaw<SourceFlag>()};
        const auto output_size{ctx.GetWriteBufferNumElements<CharInfo>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}, out_size={}", source_flag, output_size);

        u32 mii_count{};
        std::vector<CharInfo> char_info(output_size);
        Result result = manager.Get(metadata, char_info, mii_count, source_flag);

        if (mii_count != 0) {
            ctx.WriteBuffer(char_info);
        }

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(mii_count);
    }

    void UpdateLatest(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto char_info{rp.PopRaw<CharInfo>()};
        const auto source_flag{rp.PopRaw<SourceFlag>()};

        LOG_DEBUG(Service_Mii, "called with source_flag={}", source_flag);

        CharInfo new_char_info{};
        const auto result = manager.UpdateLatest(metadata, new_char_info, char_info, source_flag);
        if (result.IsFailure()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw<CharInfo>(new_char_info);
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
        manager.BuildRandom(char_info, age, gender, race);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw<CharInfo>(char_info);
    }

    void BuildDefault(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto index{rp.Pop<u32>()};

        LOG_INFO(Service_Mii, "called with index={}", index);

        if (index > 5) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ResultInvalidArgument);
            return;
        }

        CharInfo char_info{};
        manager.BuildDefault(char_info, index);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw<CharInfo>(char_info);
    }

    void GetIndex(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto info{rp.PopRaw<CharInfo>()};

        LOG_DEBUG(Service_Mii, "called");

        s32 index{};
        const auto result = manager.GetIndex(metadata, info, index);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(result);
        rb.Push(index);
    }

    void SetInterfaceVersion(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto interface_version{rp.PopRaw<u32>()};

        LOG_INFO(Service_Mii, "called, interface_version={:08X}", interface_version);

        manager.SetInterfaceVersion(metadata, interface_version);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Convert(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto mii_v3{rp.PopRaw<Ver3StoreData>()};

        LOG_INFO(Service_Mii, "called");

        CharInfo char_info{};
        manager.ConvertV3ToCharInfo(char_info, mii_v3);

        IPC::ResponseBuilder rb{ctx, 2 + sizeof(CharInfo) / sizeof(u32)};
        rb.Push(ResultSuccess);
        rb.PushRaw<CharInfo>(char_info);
    }

    MiiManager manager{};
    DatabaseSessionMetadata metadata{};
    bool is_system{};
};

class MiiDBModule final : public ServiceFramework<MiiDBModule> {
public:
    explicit MiiDBModule(Core::System& system_, const char* name_, bool is_system_)
        : ServiceFramework{system_, name_}, is_system{is_system_} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MiiDBModule::GetDatabaseService, "GetDatabaseService"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetDatabaseService(HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IDatabaseService>(system, is_system);

        LOG_DEBUG(Service_Mii, "called");
    }

    bool is_system{};
};

class MiiImg final : public ServiceFramework<MiiImg> {
public:
    explicit MiiImg(Core::System& system_) : ServiceFramework{system_, "miiimg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {10, nullptr, "Reload"},
            {11, nullptr, "GetCount"},
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
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mii:e",
                                         std::make_shared<MiiDBModule>(system, "mii:e", true));
    server_manager->RegisterNamedService("mii:u",
                                         std::make_shared<MiiDBModule>(system, "mii:u", false));
    server_manager->RegisterNamedService("miiimg", std::make_shared<MiiImg>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Mii
