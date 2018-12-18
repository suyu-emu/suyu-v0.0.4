// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Mii {

class IDatabaseService final : public ServiceFramework<IDatabaseService> {
public:
    explicit IDatabaseService() : ServiceFramework{"IDatabaseService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDatabaseService::IsUpdated, "IsUpdated"},
            {1, &IDatabaseService::IsFullDatabase, "IsFullDatabase"},
            {2, &IDatabaseService::GetCount, "GetCount"},
            {3, &IDatabaseService::Get, "Get"},
            {4, &IDatabaseService::Get1, "Get1"},
            {5, nullptr, "UpdateLatest"},
            {6, &IDatabaseService::BuildRandom, "BuildRandom"},
            {7, &IDatabaseService::BuildDefault, "BuildDefault"},
            {8, &IDatabaseService::Get2, "Get2"},
            {9, &IDatabaseService::Get3, "Get3"},
            {10, nullptr, "UpdateLatest1"},
            {11, &IDatabaseService::FindIndex, "FindIndex"},
            {12, &IDatabaseService::Move, "Move"},
            {13, &IDatabaseService::AddOrReplace, "AddOrReplace"},
            {14, &IDatabaseService::Delete, "Delete"},
            {15, nullptr, "DestroyFile"},
            {16, nullptr, "DeleteFile"},
            {17, &IDatabaseService::Format, "Format"},
            {18, nullptr, "Import"},
            {19, nullptr, "Export"},
            {20, nullptr, "IsBrokenDatabaseWithClearFlag"},
            {21, &IDatabaseService::GetIndex, "GetIndex"},
            {22, nullptr, "SetInterfaceVersion"},
            {23, nullptr, "Convert"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    template <typename OutType>
    std::vector<u8> SerializeArray(OutType (MiiManager::*getter)(u32) const, u32 offset,
                                   u32 requested_size, u32& read_size) {
        read_size = std::min(requested_size, db.Size() - offset);

        std::vector<u8> out(read_size * sizeof(OutType));

        for (u32 i = 0; i < read_size; ++i) {
            const auto obj = (db.*getter)(offset + i);
            std::memcpy(out.data() + i * sizeof(OutType), &obj, sizeof(OutType));
        }

        return out;
    }

    void IsUpdated(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto unknown{rp.PopRaw<u32>()};

        LOG_WARNING(Service_Mii, "(STUBBED) called with unknown={:08X}", unknown);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(false);
    }

    void IsFullDatabase(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(db.Full());
    }

    void GetCount(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto unknown{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with unknown={:08X}", unknown);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(db.Size());
    }

    // Gets Miis from database at offset and index in format MiiInfoElement
    void Get(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto size{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with size={:08X}, offset={:08X}", size, offsets[0]);

        u32 read_size{};
        ctx.WriteBuffer(SerializeArray(&MiiManager::GetInfoElement, offsets[0], size, read_size));
        offsets[0] += read_size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(read_size);
    }

    // Gets Miis from database at offset and index in format MiiInfo
    void Get1(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto size{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with size={:08X}, offset={:08X}", size, offsets[1]);

        u32 read_size{};
        ctx.WriteBuffer(SerializeArray(&MiiManager::GetInfo, offsets[1], size, read_size));
        offsets[1] += read_size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(read_size);
    }

    void BuildRandom(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto random_params{rp.PopRaw<RandomParameters>()};

        LOG_DEBUG(Service_Mii, "called with param_1={:08X}, param_2={:08X}, param_3={:08X}",
                  random_params.unknown_1, random_params.unknown_2, random_params.unknown_3);

        const auto info = db.CreateRandom(random_params);
        IPC::ResponseBuilder rb{ctx, 2 + sizeof(MiiInfo) / sizeof(u32)};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<MiiInfo>(info);
    }

    void BuildDefault(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto index{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with index={:08X}", index);

        const auto info = db.CreateDefault(index);
        IPC::ResponseBuilder rb{ctx, 2 + sizeof(MiiInfo) / sizeof(u32)};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<MiiInfo>(info);
    }

    // Gets Miis from database at offset and index in format MiiStoreDataElement
    void Get2(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto size{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with size={:08X}, offset={:08X}", size, offsets[2]);

        u32 read_size{};
        ctx.WriteBuffer(
            SerializeArray(&MiiManager::GetStoreDataElement, offsets[2], size, read_size));
        offsets[2] += read_size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(read_size);
    }

    // Gets Miis from database at offset and index in format MiiStoreData
    void Get3(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto size{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with size={:08X}, offset={:08X}", size, offsets[3]);

        u32 read_size{};
        ctx.WriteBuffer(SerializeArray(&MiiManager::GetStoreData, offsets[3], size, read_size));
        offsets[3] += read_size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(read_size);
    }

    void FindIndex(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto uuid{rp.PopRaw<Common::UUID>()};
        const auto unknown{rp.PopRaw<bool>()};

        LOG_DEBUG(Service_Mii, "called with uuid={}, unknown={}", uuid.FormatSwitch(), unknown);

        IPC::ResponseBuilder rb{ctx, 3};

        const auto index = db.IndexOf(uuid);
        if (index > MAX_MIIS) {
            // TODO(DarkLordZach): Find a better error code
            rb.Push(ResultCode(-1));
            rb.Push(index);
        } else {
            rb.Push(RESULT_SUCCESS);
            rb.Push(index);
        }
    }

    void Move(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto uuid{rp.PopRaw<Common::UUID>()};
        const auto index{rp.PopRaw<u32>()};

        LOG_DEBUG(Service_Mii, "called with uuid={}, index={:08X}", uuid.FormatSwitch(), index);

        const auto success = db.Move(uuid, index);

        IPC::ResponseBuilder rb{ctx, 2};
        // TODO(DarkLordZach): Find a better error code
        rb.Push(success ? RESULT_SUCCESS : ResultCode(-1));
    }

    void AddOrReplace(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto data{rp.PopRaw<MiiStoreData>()};

        LOG_DEBUG(Service_Mii, "called with Mii data uuid={}, name={}", data.uuid.FormatSwitch(),
                  Common::UTF16ToUTF8(data.Name()));

        const auto success = db.AddOrReplace(data);

        IPC::ResponseBuilder rb{ctx, 2};
        // TODO(DarkLordZach): Find a better error code
        rb.Push(success ? RESULT_SUCCESS : ResultCode(-1));
    }

    void Delete(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto uuid{rp.PopRaw<Common::UUID>()};

        LOG_DEBUG(Service_Mii, "called with uuid={}", uuid.FormatSwitch());

        const auto success = db.Remove(uuid);

        IPC::ResponseBuilder rb{ctx, 2};
        // TODO(DarkLordZach): Find a better error code
        rb.Push(success ? RESULT_SUCCESS : ResultCode(-1));
    }

    void Format(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Mii, "called");

        db.Clear();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void GetIndex(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto info{rp.PopRaw<MiiInfo>()};

        LOG_DEBUG(Service_Mii, "called with Mii info uuid={}, name={}", info.uuid.FormatSwitch(),
                  Common::UTF16ToUTF8(info.Name()));

        const auto index = db.IndexOf(info);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        rb.Push(index);
    }

    MiiManager db;

    // Last read offsets of Get functions
    std::array<u32, 4> offsets{};
};

class MiiDBModule final : public ServiceFramework<MiiDBModule> {
public:
    explicit MiiDBModule(const char* name) : ServiceFramework{name} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MiiDBModule::GetDatabaseService, "GetDatabaseService"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetDatabaseService(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDatabaseService>();

        LOG_DEBUG(Service_Mii, "called");
    }
};

class MiiImg final : public ServiceFramework<MiiImg> {
public:
    explicit MiiImg() : ServiceFramework{"miiimg"} {
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

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<MiiDBModule>("mii:e")->InstallAsService(sm);
    std::make_shared<MiiDBModule>("mii:u")->InstallAsService(sm);

    std::make_shared<MiiImg>()->InstallAsService(sm);
}

} // namespace Service::Mii
