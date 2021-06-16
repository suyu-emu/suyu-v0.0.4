// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/mii/manager.h"
#include "core/hle/service/service.h"

namespace Kernel {
class KEvent;
}

namespace Service::NFP {

enum class ServiceType : u32 {
    User = 0,
    Debug = 1,
    System = 2,
};

enum class State : u32 {
    NonInitialized = 0,
    Initialized = 1,
};

enum class DeviceState : u32 {
    Initialized = 0,
    SearchingForTag = 1,
    TagFound = 2,
    TagRemoved = 3,
    TagMounted = 4,
    Unaviable = 5,
    Finalized = 6
};

enum class MountTarget : u32 {
    Rom = 1,
    Ram = 2,
    All = 3,
};

struct TagInfo {
    std::array<u8, 10> uuid;
    u8 uuid_length;
    INSERT_PADDING_BYTES(0x15);
    u32_le protocol;
    u32_le tag_type;
    INSERT_PADDING_BYTES(0x2c);
};
static_assert(sizeof(TagInfo) == 0x54, "TagInfo is an invalid size");

struct CommonInfo {
    u16_be last_write_year;
    u8 last_write_month;
    u8 last_write_day;
    u16_be write_counter;
    u16_be version;
    u32_be application_area_size;
    INSERT_PADDING_BYTES(0x34);
};
static_assert(sizeof(CommonInfo) == 0x40, "CommonInfo is an invalid size");

struct ModelInfo {
    std::array<u8, 0x8> ammibo_id;
    INSERT_PADDING_BYTES(0x38);
};
static_assert(sizeof(ModelInfo) == 0x40, "ModelInfo is an invalid size");

struct RegisterInfo {
    Service::Mii::MiiInfo mii;
    u16_be first_write_year;
    u8 first_write_month;
    u8 first_write_day;
    std::array<u8, 11> amiibo_name;
    INSERT_PADDING_BYTES(0x99);
};
// static_assert(sizeof(RegisterInfo) == 0x106, "RegisterInfo is an invalid size");

class IUser final : public ServiceFramework<IUser> {
public:
    explicit IUser(Module::Interface& nfp_interface_, Core::System& system_);

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void Finalize(Kernel::HLERequestContext& ctx);
    void ListDevices(Kernel::HLERequestContext& ctx);
    void StartDetection(Kernel::HLERequestContext& ctx);
    void StopDetection(Kernel::HLERequestContext& ctx);
    void Mount(Kernel::HLERequestContext& ctx);
    void Unmount(Kernel::HLERequestContext& ctx);
    void OpenApplicationArea(Kernel::HLERequestContext& ctx);
    void GetApplicationArea(Kernel::HLERequestContext& ctx);
    void GetTagInfo(Kernel::HLERequestContext& ctx);
    void GetRegisterInfo(Kernel::HLERequestContext& ctx);
    void GetCommonInfo(Kernel::HLERequestContext& ctx);
    void GetModelInfo(Kernel::HLERequestContext& ctx);
    void AttachActivateEvent(Kernel::HLERequestContext& ctx);
    void AttachDeactivateEvent(Kernel::HLERequestContext& ctx);
    void GetState(Kernel::HLERequestContext& ctx);
    void GetDeviceState(Kernel::HLERequestContext& ctx);
    void GetNpadId(Kernel::HLERequestContext& ctx);
    void GetApplicationAreaSize(Kernel::HLERequestContext& ctx);
    void AttachAvailabilityChangeEvent(Kernel::HLERequestContext& ctx);

    bool has_attached_handle{};
    const u64 device_handle{0}; // Npad device 1
    const u32 npad_id{0};       // Player 1 controller
    State state{State::NonInitialized};
    DeviceState device_state{DeviceState::Initialized};
    Module::Interface& nfp_interface;
    Kernel::KEvent deactivate_event;
    Kernel::KEvent availability_change_event;
};

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_, Core::System& system_,
                           const char* name);
        ~Interface() override;

        struct ModelInfo {
            std::array<u8, 0x8> amiibo_identification_block;
            INSERT_PADDING_BYTES(0x38);
        };
        static_assert(sizeof(ModelInfo) == 0x40, "ModelInfo is an invalid size");

        struct AmiiboFile {
            std::array<u8, 10> uuid;
            INSERT_PADDING_BYTES(0x4a);
            ModelInfo model_info;
        };
        static_assert(sizeof(AmiiboFile) == 0x94, "AmiiboFile is an invalid size");

        void CreateUserInterface(Kernel::HLERequestContext& ctx);
        bool LoadAmiibo(const std::vector<u8>& buffer);
        Kernel::KReadableEvent& GetNFCEvent();
        const AmiiboFile& GetAmiiboBuffer() const;

    protected:
        std::shared_ptr<Module> module;

    private:
        KernelHelpers::ServiceContext service_context;
        Kernel::KEvent* nfc_tag_load;
        AmiiboFile amiibo{};
    };
};

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::NFP
