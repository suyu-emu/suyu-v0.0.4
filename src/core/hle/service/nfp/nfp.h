// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/service.h"

namespace Kernel {
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Core::HID {
enum class NpadIdType : u32;
} // namespace Core::HID

namespace Service::NFP {

enum class ServiceType : u32 {
    User,
    Debug,
    System,
};

enum class State : u32 {
    NonInitialized,
    Initialized,
};

enum class DeviceState : u32 {
    Initialized,
    SearchingForTag,
    TagFound,
    TagRemoved,
    TagMounted,
    Unaviable,
    Finalized,
};

enum class ModelType : u32 {
    Amiibo,
};

enum class MountTarget : u32 {
    Rom,
    Ram,
    All,
};

enum class AmiiboType : u8 {
    Figure,
    Card,
    Yarn,
};

enum class AmiiboSeries : u8 {
    SuperSmashBros,
    SuperMario,
    ChibiRobo,
    YoshiWoollyWorld,
    Splatoon,
    AnimalCrossing,
    EightBitMario,
    Skylanders,
    Unknown8,
    TheLegendOfZelda,
    ShovelKnight,
    Unknown11,
    Kiby,
    Pokemon,
    MarioSportsSuperstars,
    MonsterHunter,
    BoxBoy,
    Pikmin,
    FireEmblem,
    Metroid,
    Others,
    MegaMan,
    Diablo
};

struct TagInfo {
    std::array<u8, 10> uuid;
    u8 uuid_length;
    INSERT_PADDING_BYTES(0x15);
    s32 protocol;
    u32 tag_type;
    INSERT_PADDING_BYTES(0x30);
};
static_assert(sizeof(TagInfo) == 0x58, "TagInfo is an invalid size");

struct CommonInfo {
    u16 last_write_year;
    u8 last_write_month;
    u8 last_write_day;
    u16 write_counter;
    u16 version;
    u32 application_area_size;
    INSERT_PADDING_BYTES(0x34);
};
static_assert(sizeof(CommonInfo) == 0x40, "CommonInfo is an invalid size");

struct ModelInfo {
    u16 character_id;
    u8 character_variant;
    AmiiboType amiibo_type;
    u16 model_number;
    AmiiboSeries series;
    u8 fixed;                   // Must be 02
    INSERT_PADDING_BYTES(0x4);  // Unknown
    INSERT_PADDING_BYTES(0x20); // Probably a SHA256-(HMAC?) hash
    INSERT_PADDING_BYTES(0x14); // SHA256-HMAC
};
static_assert(sizeof(ModelInfo) == 0x40, "ModelInfo is an invalid size");

struct RegisterInfo {
    Service::Mii::MiiInfo mii_char_info;
    u16 first_write_year;
    u8 first_write_month;
    u8 first_write_day;
    std::array<u8, 11> amiibo_name;
    u8 unknown;
    INSERT_PADDING_BYTES(0x98);
};
static_assert(sizeof(RegisterInfo) == 0x100, "RegisterInfo is an invalid size");

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_, Core::System& system_,
                           const char* name);
        ~Interface() override;

        struct AmiiboFile {
            std::array<u8, 10> uuid;
            u16 uuid_lock;               // Must be 0F E0
            u32 compability_container;   // Must be F1 10 FF EE
            u16 crypto_init;             // Must be A5 XX
            u16 write_count;             // Number of times the amiibo has been written?
            INSERT_PADDING_BYTES(0x20);  // System crypts
            INSERT_PADDING_BYTES(0x20);  // SHA256-(HMAC?) hash
            ModelInfo model_info;        // This struct is bigger than documentation
            INSERT_PADDING_BYTES(0xC);   // SHA256-HMAC
            INSERT_PADDING_BYTES(0x114); // section 1 encrypted buffer
            INSERT_PADDING_BYTES(0x54);  // section 2 encrypted buffer
            u32 tag_dynamic_lock;        // Must be 01 00 0F XX
            u32 tag_CFG0;                // Must be 00 00 00 04
            u32 tag_CFG1;                // Must be 50 00 00 00
        };
        static_assert(sizeof(AmiiboFile) == 0x214, "AmiiboFile is an invalid size");

        void CreateUserInterface(Kernel::HLERequestContext& ctx);
        bool LoadAmiibo(const std::vector<u8>& buffer);
        void CloseAmiibo();

        void Initialize();
        void Finalize();

        ResultCode StartDetection(s32 protocol_);
        ResultCode StopDetection();
        ResultCode Mount();
        ResultCode Unmount();

        ResultCode GetTagInfo(TagInfo& tag_info) const;
        ResultCode GetCommonInfo(CommonInfo& common_info) const;
        ResultCode GetModelInfo(ModelInfo& model_info) const;
        ResultCode GetRegisterInfo(RegisterInfo& register_info) const;

        ResultCode OpenApplicationArea(u32 access_id);
        ResultCode GetApplicationArea(std::vector<u8>& data) const;
        ResultCode SetApplicationArea(const std::vector<u8>& data);
        ResultCode CreateApplicationArea(u32 access_id, const std::vector<u8>& data);

        u64 GetHandle() const;
        DeviceState GetCurrentState() const;
        Core::HID::NpadIdType GetNpadId() const;

        Kernel::KReadableEvent& GetActivateEvent() const;
        Kernel::KReadableEvent& GetDeactivateEvent() const;

    protected:
        std::shared_ptr<Module> module;

    private:
        /// Validates that the amiibo file is not corrupted
        bool IsAmiiboValid() const;
        bool AmiiboApplicationDataExist(u32 access_id) const;
        const std::vector<u8> LoadAmiiboApplicationData(u32 access_id) const;
        void SaveAmiiboApplicationData(u32 access_id, const std::vector<u8>& data) const;

        const Core::HID::NpadIdType npad_id;

        DeviceState device_state{DeviceState::Unaviable};
        KernelHelpers::ServiceContext service_context;
        Kernel::KEvent* activate_event;
        Kernel::KEvent* deactivate_event;
        AmiiboFile amiibo{};
        s32 protocol;
        bool is_application_area_initialized{};
        u32 application_area_id;
        std::vector<u8> application_area_data;
    };
};

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
    void SetApplicationArea(Kernel::HLERequestContext& ctx);
    void CreateApplicationArea(Kernel::HLERequestContext& ctx);
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

    KernelHelpers::ServiceContext service_context;

    // TODO(german77): We should have a vector of interfaces
    Module::Interface& nfp_interface;

    State state{State::NonInitialized};
    Kernel::KEvent* availability_change_event;
};

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::NFP
