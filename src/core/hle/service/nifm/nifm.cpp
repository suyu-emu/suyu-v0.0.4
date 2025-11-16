// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <fstream>
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/nifm/nifm.h"
#include "core/hle/service/server_manager.h"
#include "core/internal_network/emu_net_state.h"
#include "core/internal_network/network.h"
#include "core/internal_network/network_interface.h"
#include "core/internal_network/wifi_scanner.h"
#include "network/network.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>
#include <common/settings.h>

#ifdef _WIN32
#include <windows.h>
#undef CreateEvent
#pragma push_macro("interface")
#undef interface
#include <wlanapi.h>
#pragma pop_macro("interface")
#ifdef _MSC_VER
#pragma comment(lib, "wlanapi.lib")
#endif
#endif

namespace {

// Avoids name conflict with Windows' CreateEvent macro.
[[nodiscard]] Kernel::KEvent* CreateKEvent(Service::KernelHelpers::ServiceContext& service_context,
                                           std::string&& name) {
    return service_context.CreateEvent(std::move(name));
}

} // Anonymous namespace

namespace Service::NIFM {

static u128 MakeUuidFromName(std::string_view name) {
    constexpr u64 kOff = 0xcbf29ce484222325ULL;
    constexpr u64 kPrime = 0x100000001b3ULL;

    u64 h1 = kOff;
    u64 h2 = kOff ^ 0x9e3779b97f4a7c15ULL;

    for (unsigned char c : name) {
        h1 ^= c;
        h1 *= kPrime;

        h2 ^= static_cast<u8>(c ^ 0x5A);
        h2 *= kPrime;
    }
    if (h1 == 0 && h2 == 0)
        h1 = 1;

    return {h1, h2};
}

// This is nn::nifm::RequestState
enum class RequestState : u32 {
    NotSubmitted = 1,
    Invalid = 1, ///< The duplicate 1 is intentional; it means both not submitted and error on HW.
    OnHold = 2,
    Accepted = 3,
    Blocking = 4,
};

// This is nn::nifm::NetworkInterfaceType
enum class NetworkInterfaceType : u32 {
    Invalid = 0,
    WiFi_Ieee80211 = 1,
    Ethernet = 2,
};

enum class InternetConnectionStatus : u8 {
    ConnectingUnknown1,
    ConnectingUnknown2,
    ConnectingUnknown3,
    ConnectingUnknown4,
    Connected,
};

// This is nn::nifm::NetworkProfileType
enum class NetworkProfileType : u32 {
    User,
    SsidList,
    Temporary,
};

// This is nn::nifm::IpAddressSetting
struct IpAddressSetting {
    bool is_automatic{};
    Network::IPv4Address ip_address{};
    Network::IPv4Address subnet_mask{};
    Network::IPv4Address default_gateway{};
};
static_assert(sizeof(IpAddressSetting) == 0xD, "IpAddressSetting has incorrect size.");

// This is nn::nifm::DnsSetting
struct DnsSetting {
    bool is_automatic{};
    Network::IPv4Address primary_dns{};
    Network::IPv4Address secondary_dns{};
};
static_assert(sizeof(DnsSetting) == 0x9, "DnsSetting has incorrect size.");

// This is nn::nifm::detail::sf::AccessPointData (for >= 19.0.0)
struct AccessPointDataV3 {
    u8 ssid_len;     // Length of SSID
    char ssid[0x20]; // SSID Name (not null-terminated)
    u8 unk_pad[11];  // Unk
    u8 strength;     // 0-3 = Signal Strengh Bar
    u8 pad2[3];      // Unk 3 bytes
    u8 visible;      // 0 = Hidden, 1 = Shows up in UI
    u8 has_password; // 0 = Open, 1 = Password Protected
    u8 uk1;          // Unk
    u8 is_available; // 0 = Gray, 1 = Connectable
};
static_assert(sizeof(AccessPointDataV3) == 52, "AccessPointData18 must be 64 bytes");

// This is nn::nifm::AuthenticationSetting
struct AuthenticationSetting {
    bool is_enabled{};
    std::array<char, 0x20> user{};
    std::array<char, 0x20> password{};
};
static_assert(sizeof(AuthenticationSetting) == 0x41, "AuthenticationSetting has incorrect size.");

// This is nn::nifm::ProxySetting
struct ProxySetting {
    bool is_enabled{};
    INSERT_PADDING_BYTES(1);
    u16 port{};
    std::array<char, 0x64> proxy_server{};
    AuthenticationSetting authentication{};
    INSERT_PADDING_BYTES(1);
};
static_assert(sizeof(ProxySetting) == 0xAA, "ProxySetting has incorrect size.");

// This is nn::nifm::IpSettingData
struct IpSettingData {
    IpAddressSetting ip_address_setting{};
    DnsSetting dns_setting{};
    ProxySetting proxy_setting{};
    u16 mtu{};
};
static_assert(sizeof(IpSettingData) == 0xC2, "IpSettingData has incorrect size.");

struct SfWirelessSettingData {
    u8 ssid_length{};
    std::array<char, 0x20> ssid{};
    u8 unknown_1{};
    u8 unknown_2{};
    u8 is_secured{};
    std::array<char, 0x41> passphrase{};
};
static_assert(sizeof(SfWirelessSettingData) == 0x65, "SfWirelessSettingData has incorrect size.");

struct NifmWirelessSettingData {
    u8 ssid_length{};
    std::array<char, 0x21> ssid{};
    u8 unknown_1{};
    INSERT_PADDING_BYTES(1);
    u32 unknown_2{};
    u32 unknown_3{};
    std::array<char, 0x41> passphrase{};
    INSERT_PADDING_BYTES(3);
};
static_assert(sizeof(NifmWirelessSettingData) == 0x70,
              "NifmWirelessSettingData has incorrect size.");

#pragma pack(push, 1)
// This is nn::nifm::detail::sf::NetworkProfileData
struct SfNetworkProfileData {
    IpSettingData ip_setting_data{};
    u128 uuid{};
    std::array<char, 0x40> network_name{};
    u8 profile_type;
    u8 interface_type;
    u8 is_auto_connect;
    u8 is_large_capacity;
    SfWirelessSettingData wireless_setting_data{};
    INSERT_PADDING_BYTES(1);
};
static_assert(sizeof(SfNetworkProfileData) == 0x17C, "SfNetworkProfileData has incorrect size.");

// This is nn::nifm::NetworkProfileData
struct NifmNetworkProfileData {
    u128 uuid{};
    std::array<char, 0x40> network_name{};
    NetworkProfileType network_profile_type{};
    NetworkInterfaceType network_interface_type{};
    bool is_auto_connect{};
    bool is_large_capacity{};
    INSERT_PADDING_BYTES(2);
    NifmWirelessSettingData wireless_setting_data{};
    IpSettingData ip_setting_data{};
};
static_assert(sizeof(NifmNetworkProfileData) == 0x18E,
              "NifmNetworkProfileData has incorrect size.");

struct PendingProfile {
    std::array<char, 0x21> ssid{};
    u8 ssid_len{};
    std::array<char, 0x41> passphrase{};
};

constexpr Result ResultPendingConnection{ErrorModule::NIFM, 111};
constexpr Result ResultInvalidInput{ErrorModule::NIFM, 112};
constexpr Result ResultNetworkCommunicationDisabled{ErrorModule::NIFM, 1111};

static std::mutex g_scan_mtx;
static std::vector<Network::ScanData> g_last_scan_results;
static std::mutex g_profile_mtx;
static std::optional<PendingProfile> g_pending_profile;

class IScanRequest final : public ServiceFramework<IScanRequest> {
public:
    explicit IScanRequest(Core::System& system_)
        : ServiceFramework{system_, "IScanRequest"}, svc_ctx{system_, "IScanRequest"} {

        static const FunctionInfo functions[] = {
            {0, &IScanRequest::Submit, "Submit"},
            {1, &IScanRequest::IsProcessing, "IsProcessing"},
            {2, &IScanRequest::GetResult, "GetResult"},
            {3, &IScanRequest::GetSystemEventReadableHandle, "GetSystemEventReadableHandle"},
            {4, &IScanRequest::SetChannels, "SetChannels"},
        };
        RegisterHandlers(functions);

        evt_scan_complete = CreateKEvent(svc_ctx, "IScanRequest:Complete");
        evt_processing = CreateKEvent(svc_ctx, "IScanRequest:Processing");
    }

    ~IScanRequest() override {
        state.store(State::Idle);
        svc_ctx.CloseEvent(evt_scan_complete);
        svc_ctx.CloseEvent(evt_processing);
        if (worker.joinable())
            worker.join();
    }

private:
    std::vector<Network::ScanData> scan_results;

    void Submit(HLERequestContext& ctx) {

        if (state.load() == State::Finished) {
            if (worker.joinable())
                worker.join();

            state.store(State::Idle);
            worker_result.store(ResultPendingConnection);
        }

        if (state.load() != State::Idle) {
            IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
            return;
        }

        state.store(State::Processing);
        evt_processing->Signal();

        worker = std::thread(&IScanRequest::WorkerThread, this);
        IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
    }

    void IsProcessing(HLERequestContext& ctx) {
        const bool processing = state.load() == State::Processing;
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u8>(processing);
    }

    void GetResult(HLERequestContext& ctx) {
        const Result rc = worker_result.load();
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void GetSystemEventReadableHandle(HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 2};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(evt_scan_complete->GetReadableEvent(),
                           evt_processing->GetReadableEvent());
    }

    void SetChannels(HLERequestContext& ctx) {
        LOG_WARNING(Service_NIFM, "(STUBBED) called");
        IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
    }

    enum class State { Idle, Processing, Finished };

    void WorkerThread() {
        using namespace std::chrono_literals;

        scan_results = Network::ScanWifiNetworks(3s);

        {
            std::scoped_lock lk{g_scan_mtx};
            g_last_scan_results = scan_results;
        }

        // choose result code
        const bool ok = !scan_results.empty();
        Finish(ok ? ResultSuccess : ResultPendingConnection);
    }

    void Finish(Result rc) {
        worker_result.store(rc);
        state.store(State::Finished);
        evt_scan_complete->Signal();
    }

    KernelHelpers::ServiceContext svc_ctx;

    Kernel::KEvent* evt_scan_complete{};
    Kernel::KEvent* evt_processing{};
    std::thread worker;
    std::atomic<State> state{State::Idle};
    std::atomic<Result> worker_result{ResultPendingConnection};
};

class IRequest final : public ServiceFramework<IRequest> {
public:
    explicit IRequest(Core::System& system_)
        : ServiceFramework{system_, "IRequest"}, service_context{system_, "IRequest"} {
        static const FunctionInfo functions[] = {
            {0, &IRequest::GetRequestState, "GetRequestState"},
            {1, &IRequest::GetResult, "GetResult"},
            {2, &IRequest::GetSystemEventReadableHandles, "GetSystemEventReadableHandles"},
            {3, &IRequest::Cancel, "Cancel"},
            {4, &IRequest::Submit, "Submit"},
            {5, nullptr, "SetRequirement"},
            {6, &IRequest::SetRequirementPreset, "SetRequirementPreset"},
            {8, nullptr, "SetPriority"},
            {9, &IRequest::SetNetworkProfileId, "SetNetworkProfileId"},
            {10, nullptr, "SetRejectable"},
            {11, &IRequest::SetConnectionConfirmationOption, "SetConnectionConfirmationOption"},
            {12, nullptr, "SetPersistent"},
            {13, nullptr, "SetInstant"},
            {14, nullptr, "SetSustainable"},
            {15, nullptr, "SetRawPriority"},
            {16, nullptr, "SetGreedy"},
            {17, nullptr, "SetSharable"},
            {18, nullptr, "SetRequirementByRevision"},
            {19, nullptr, "GetRequirement"},
            {20, nullptr, "GetRevision"},
            {21, &IRequest::GetAppletInfo, "GetAppletInfo"},
            {22, nullptr, "GetAdditionalInfo"},
            {23, nullptr, "SetKeptInSleep"},
            {24, nullptr, "RegisterSocketDescriptor"},
            {25, nullptr, "UnregisterSocketDescriptor"},
            {26, nullptr, "GetNetworkAccessStatus"}, //21.0.0+
        };
        RegisterHandlers(functions);

        event1 = CreateKEvent(service_context, "IRequest:Event1");
        event2 = CreateKEvent(service_context, "IRequest:Event2");
        state = RequestState::NotSubmitted;
    }

    ~IRequest() override {
        service_context.CloseEvent(event1);
        service_context.CloseEvent(event2);
    }

private:
    void Submit(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NIFM, "(STUBBED) called");

        if (state == RequestState::NotSubmitted) {
            UpdateState(RequestState::OnHold);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetRequestState(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NIFM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.PushEnum(state);
    }

    void SetRequirementPreset(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto param_1 = rp.Pop<u32>();

        LOG_WARNING(Service_NIFM, "(STUBBED) called, param_1={}", param_1);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetNetworkProfileId(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto ssid_length = rp.Pop<u32>();
        if (ssid_length > 0x20) {
            LOG_ERROR(Service_NIFM, "Invalid SSID length: {}", ssid_length);
            IPC::ResponseBuilder{ctx, 2}.Push(ResultInvalidInput);
            return;
        }
        LOG_WARNING(Service_NIFM, "(STUBBED) called, ssid_length={}", ssid_length);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(1);
    }

    void GetResult(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NIFM, "(STUBBED) called");

        const auto result = [this] {
            const auto has_connection = Network::GetHostIPv4Address().has_value() &&
                                        !Settings::values.airplane_mode.GetValue();
            switch (state) {
            case RequestState::NotSubmitted:
                return has_connection ? ResultSuccess : ResultNetworkCommunicationDisabled;
            case RequestState::OnHold:
                if (has_connection) {
                    UpdateState(RequestState::Accepted);
                } else {
                    UpdateState(RequestState::Invalid);
                }
                return ResultPendingConnection;
            case RequestState::Accepted:
            default:
                return ResultSuccess;
            }
        }();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void GetSystemEventReadableHandles(HLERequestContext& ctx) {
        LOG_WARNING(Service_NIFM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 2};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(event1->GetReadableEvent(), event2->GetReadableEvent());
    }

    void Cancel(HLERequestContext& ctx) {
        LOG_WARNING(Service_NIFM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetConnectionConfirmationOption(HLERequestContext& ctx) {
        LOG_WARNING(Service_NIFM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetAppletInfo(HLERequestContext& ctx) {
        LOG_WARNING(Service_NIFM, "(STUBBED) called");

        std::vector<u8> out_buffer(ctx.GetWriteBufferSize());

        ctx.WriteBuffer(out_buffer);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(ResultSuccess);
        rb.Push<u32>(0);
        rb.Push<u32>(0);
        rb.Push<u32>(0);
    }

    void UpdateState(RequestState new_state) {
        LOG_DEBUG(Service_NIFM, "(STUBBED) called");
        state = new_state;
        event1->Signal();
    }

    KernelHelpers::ServiceContext service_context;

    RequestState state;

    Kernel::KEvent* event1;
    Kernel::KEvent* event2;

    std::thread connect_worker;
    std::atomic<bool> connect_done{false};
    std::atomic<Result> connect_result{ResultPendingConnection};
};

class INetworkProfile final : public ServiceFramework<INetworkProfile> {
public:
    explicit INetworkProfile(Core::System& system_) : ServiceFramework{system_, "INetworkProfile"} {
        static const FunctionInfo functions[] = {
            {0, nullptr, "Update"},
            {1, nullptr, "PersistOld"},
            {2, nullptr, "Persist"},
        };
        RegisterHandlers(functions);
    }
};

void IGeneralService::GetClientId(HLERequestContext& ctx) {
    static constexpr u32 client_id = 1;
    LOG_WARNING(Service_NIFM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(client_id); // Client ID needs to be non zero otherwise it's considered invalid
}

void IGeneralService::CreateScanRequest(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IScanRequest>(system);
}

void IGeneralService::CreateRequest(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IRequest>(system);
}

void IGeneralService::GetCurrentNetworkProfile(HLERequestContext& ctx) {

    Network::RefreshFromHost();
    const auto net_iface = Network::GetSelectedNetworkInterface();
    const auto& st = Network::EmuNetState::Get();

    SfNetworkProfileData profile{};

    if (st.connected) {

        profile.ip_setting_data.ip_address_setting.is_automatic = true;
        profile.ip_setting_data.ip_address_setting.ip_address = st.ip;
        profile.ip_setting_data.ip_address_setting.subnet_mask = st.mask;
        profile.ip_setting_data.ip_address_setting.default_gateway = st.gw;

        profile.ip_setting_data.dns_setting.is_automatic = true;
        profile.ip_setting_data.dns_setting.primary_dns = {1, 1, 1, 1};
        profile.ip_setting_data.dns_setting.secondary_dns = {1, 0, 0, 1},

        profile.uuid = MakeUuidFromName(st.ssid);
        profile.profile_type = static_cast<u8>(NetworkProfileType::User);
        profile.interface_type = static_cast<u8>(st.via_wifi ? NetworkInterfaceType::WiFi_Ieee80211
                                                             : NetworkInterfaceType::Ethernet);

        std::strncpy(profile.network_name.data(), st.ssid, sizeof(profile.network_name) - 1);

        if (auto room_member = Network::GetRoomMember().lock()) {
            if (room_member->IsConnected()) {
                profile.ip_setting_data.ip_address_setting.ip_address =
                    room_member->GetFakeIpAddress();
            }
            profile.ip_setting_data.dns_setting.secondary_dns = {8, 8, 8, 8};
        }

        if (st.via_wifi) {
            profile.wireless_setting_data.ssid_length = static_cast<u8>(std::strlen(st.ssid));
            std::memcpy(profile.wireless_setting_data.ssid.data(), st.ssid,
                        profile.wireless_setting_data.ssid_length);
        }
    }

    ctx.WriteBuffer(profile);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IGeneralService::EnumerateNetworkInterfaces(HLERequestContext& ctx) {

    using Network::HostAdapterKind;

    const auto adapters = Network::GetAvailableNetworkInterfaces();
    constexpr size_t kEntry = 0x3F0;

    std::vector<u8> blob(adapters.size() * kEntry, 0);

    for (size_t i = 0; i < adapters.size(); ++i) {
        const auto& host = adapters[i];
        u8* const base = blob.data() + i * kEntry;

        *reinterpret_cast<u32*>(base + 0x0) = host.kind == HostAdapterKind::Wifi ? 1u : 2u;
        *reinterpret_cast<u32*>(base + 0x4) = 1u;

        *reinterpret_cast<in_addr*>(base + 0x18) = host.ip_address;
        *reinterpret_cast<in_addr*>(base + 0x1C) = host.subnet_mask;
        *reinterpret_cast<in_addr*>(base + 0x20) = host.gateway;

        std::string name_utf8 = host.name;
        name_utf8.resize(0x110, '\0');
        std::memcpy(base + 0x2E0, name_utf8.data(), 0x110);
    }

    const size_t guest_bytes = ctx.GetWriteBufferSize();
    if (guest_bytes && !blob.empty())
        ctx.WriteBuffer(blob.data(), (std::min)(guest_bytes, blob.size()));

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(adapters.size()));
}

void IGeneralService::EnumerateNetworkProfiles(HLERequestContext& ctx) {
    const auto adapter = Network::GetSelectedNetworkInterface();

    if (!adapter) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push(0);
        return;
    }

    const u32 count = static_cast<u32>(1);

    std::vector<u128> uuids;
    uuids.reserve(count);

    uuids.push_back(MakeUuidFromName(adapter.value().name));

    const size_t guest_sz = ctx.GetWriteBufferSize();
    if (guest_sz && uuids.size()) {
        const size_t to_copy = (std::min)(guest_sz, uuids.size() * sizeof(u128));
        ctx.WriteBuffer(uuids.data(), to_copy);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(count);
}

void IGeneralService::GetNetworkProfile(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "GetNetworkProfile called");

    IPC::RequestParser rp{ctx};

    ASSERT_MSG(ctx.GetWriteBufferSize() == sizeof(SfNetworkProfileData),
               "Caller expects SfNetworkProfileData (0x17C bytes)");

    const auto net_iface = Network::GetSelectedNetworkInterface();

    SfNetworkProfileData profile = [&] {
        if (!net_iface) {
            return SfNetworkProfileData{};
        }

        const auto& net_state = Network::EmuNetState::Get();

        std::array<char, 0x40> net_name{};
        const size_t ssid_len = std::min<size_t>(std::strlen(net_state.ssid), net_name.size());
        std::memcpy(net_name.data(), net_state.ssid, ssid_len);

        SfWirelessSettingData wifi{};
        wifi.ssid_length =
            static_cast<u8>(std::min<size_t>(std::strlen(net_state.ssid), net_name.size()));
        wifi.is_secured = !net_state.secure; // somehow reversed
        wifi.passphrase = {"password"};
        std::memcpy(wifi.ssid.data(), net_state.ssid, wifi.ssid_length);

        LOG_INFO(Service_NIFM, "ssid={} lenght={}", wifi.ssid, wifi.ssid_length);

        return SfNetworkProfileData{
            .ip_setting_data{
                .ip_address_setting{
                    .is_automatic{true},
                    .ip_address{Network::TranslateIPv4(net_iface->ip_address)},
                    .subnet_mask{Network::TranslateIPv4(net_iface->subnet_mask)},
                    .default_gateway{Network::TranslateIPv4(net_iface->gateway)},
                },
                .dns_setting{
                    .is_automatic{true},
                    .primary_dns{1, 1, 1, 1},
                    .secondary_dns{1, 0, 0, 1},
                },
                .proxy_setting{
                    .is_enabled{false},
                },
                .mtu{1500},
            },
            .uuid{MakeUuidFromName(net_state.ssid)},
            .network_name{net_name},
            .profile_type = static_cast<u8>(NetworkProfileType::User),
            .interface_type = static_cast<u8>(net_iface->kind == Network::HostAdapterKind::Wifi
                                                  ? NetworkInterfaceType::WiFi_Ieee80211
                                                  : NetworkInterfaceType::Ethernet),
            .wireless_setting_data{wifi}};
    }();

    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            profile.ip_setting_data.ip_address_setting.ip_address = room_member->GetFakeIpAddress();
        }
    }

    ctx.WriteBuffer(profile);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<INetworkProfile>(system);
}

void IGeneralService::SetNetworkProfile(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "SetNetworkProfile called");

    if (!ctx.CanReadBuffer(0) || ctx.GetReadBufferSize() < sizeof(NifmNetworkProfileData)) {
        IPC::ResponseBuilder{ctx, 2}.Push(ResultInvalidInput);
        return;
    }

    const auto data = ctx.ReadBufferCopy(0);
    const auto* np = reinterpret_cast<const NifmNetworkProfileData*>(data.data());

    PendingProfile prof{};
    prof.ssid_len = np->wireless_setting_data.ssid_length;
    std::memcpy(prof.ssid.data(), np->wireless_setting_data.ssid.data(), prof.ssid_len);
    std::memcpy(prof.passphrase.data(), np->wireless_setting_data.passphrase.data(),
                sizeof(prof.passphrase));

    {
        std::scoped_lock lk{g_profile_mtx};
        g_pending_profile = prof;
    }

    IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
}

void IGeneralService::RemoveNetworkProfile(HLERequestContext& ctx) {
    LOG_WARNING(Service_NIFM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IGeneralService::GetScanData(HLERequestContext& ctx) {
    LOG_INFO(Service_NIFM, "GetScanData called");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(0);
}

void IGeneralService::GetScanDataV2(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{g_scan_mtx};
    const auto& scans = g_last_scan_results;

    const auto& net_state = Network::EmuNetState::Get();

    const std::size_t guest_bytes = ctx.GetWriteBufferSize();
    const std::size_t max_rows = guest_bytes / sizeof(AccessPointDataV3);
    const std::size_t rows_copy = std::min<std::size_t>(scans.size(), max_rows);

    std::vector<AccessPointDataV3> rows;
    rows.resize(rows_copy);

    auto to_bars = [](u8 q) {
        return static_cast<u8>((q + 16) / 33); // quick maths
    };

    for (std::size_t i = 0; i < rows_copy; ++i) {
        const Network::ScanData& s = scans[i];
        auto& ap = rows[i];

        ap.ssid_len = s.ssid_len;
        std::memcpy(ap.ssid, s.ssid, s.ssid_len);
        ap.strength = to_bars(s.quality);

        bool is_connected = std::strncmp(net_state.ssid, ap.ssid, ap.ssid_len) == 0 &&
                            net_state.ssid[ap.ssid_len] == '\0';

        ap.visible = (is_connected) ? 0 : 1;
        ap.has_password = (s.flags & 2) ? 2 : 1;
        ap.is_available = 1;
    }

    if (rows_copy)
        ctx.WriteBuffer(rows.data(), rows_copy * sizeof(AccessPointDataV3));

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(scans.size()));
}

void IGeneralService::GetScanDataV3(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{g_scan_mtx};
    const auto& scans = g_last_scan_results;

    const std::size_t guest_bytes = ctx.GetWriteBufferSize();
    const std::size_t max_rows = guest_bytes / sizeof(AccessPointDataV3);
    const std::size_t rows_copy = std::min<std::size_t>(scans.size(), max_rows);

    std::vector<AccessPointDataV3> rows;
    rows.resize(rows_copy);

    auto to_bars = [](u8 q) {
        return static_cast<u8>((q + 16) / 33); // quick maths
    };

    for (std::size_t i = 0; i < rows_copy; ++i) {
        const Network::ScanData& s = scans[i];
        auto& ap = rows[i];

        ap.ssid_len = s.ssid_len;
        std::memcpy(ap.ssid, s.ssid, s.ssid_len);
        ap.strength = to_bars(s.quality);
        ap.visible = 1;
        ap.has_password = (s.flags & 2) ? 2 : 1;
        ap.is_available = 1;
    }

    if (rows_copy)
        ctx.WriteBuffer(rows.data(), rows_copy * sizeof(AccessPointDataV3));

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(scans.size()));
}

void IGeneralService::GetCurrentIpAddress(HLERequestContext& ctx) {
    LOG_WARNING(Service_NIFM, "(STUBBED) called");

    auto ipv4 = Network::GetHostIPv4Address();
    if (!ipv4) {
        LOG_ERROR(Service_NIFM, "Couldn't get host IPv4 address, defaulting to 0.0.0.0");
        ipv4.emplace(Network::IPv4Address{0, 0, 0, 0});
    }

    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            ipv4 = room_member->GetFakeIpAddress();
        }
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(*ipv4);
}

void IGeneralService::CreateTemporaryNetworkProfile(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "called");

    ASSERT_MSG(ctx.GetReadBufferSize() == 0x17c, "SfNetworkProfileData is not the correct size");
    u128 uuid{};
    auto buffer = ctx.ReadBuffer();
    std::memcpy(&uuid, buffer.data() + 8, sizeof(u128));

    IPC::ResponseBuilder rb{ctx, 6, 0, 1};

    rb.Push(ResultSuccess);
    rb.PushIpcInterface<INetworkProfile>(system);
    rb.PushRaw<u128>(uuid);
}

void IGeneralService::GetCurrentIpConfigInfo(HLERequestContext& ctx) {
    Network::RefreshFromHost();
    const auto& st = Network::EmuNetState::Get();

    struct IpConfigInfo {
        IpAddressSetting ip{};
        DnsSetting dns{};
    };
    static_assert(sizeof(IpConfigInfo) == sizeof(IpAddressSetting) + sizeof(DnsSetting));

    IpConfigInfo info{};

    if (st.connected) {
        info.ip.is_automatic = true;
        info.ip.ip_address = st.ip;
        info.ip.subnet_mask = st.mask;
        info.ip.default_gateway = st.gw;

        info.dns.is_automatic = true;
        info.dns.primary_dns = {1, 1, 1, 1};
        info.dns.secondary_dns = {8, 8, 8, 8};
    }

    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->IsConnected()) {
            info.ip.ip_address = room_member->GetFakeIpAddress();
        }
        info.dns.is_automatic = true;
        info.dns.primary_dns = {1, 1, 1, 1};
        info.dns.secondary_dns = {8, 8, 8, 8};
    }

    IPC::ResponseBuilder rb{ctx, 2 + (sizeof(IpConfigInfo) + 3) / sizeof(u32)};
    rb.Push(ResultSuccess);
    rb.PushRaw(info);
}

void IGeneralService::SetWirelessCommunicationEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u8 enable = rp.Pop<u8>();

    Settings::values.airplane_mode.SetValue(enable == 0);

    IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
}

void IGeneralService::IsWirelessCommunicationEnabled(HLERequestContext& ctx) {
    const bool en = !Settings::values.airplane_mode.GetValue();
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(en);
}

void IGeneralService::GetInternetConnectionStatus(HLERequestContext& ctx) {

    Network::RefreshFromHost();

    const auto& st = Network::EmuNetState::Get();
    struct Output {
        u8 type; // 1 Wi-Fi, 2 Ethernet
        u8 bars; // 0-3
        InternetConnectionStatus state;
    };
    Output out{};

    if (!st.connected) {
        out.type = 1;
        out.bars = 0;
        out.state = InternetConnectionStatus::ConnectingUnknown1;
    } else {
        out.type = st.via_wifi ? 1 : 2;
        out.bars = st.bars;
        out.state = InternetConnectionStatus::Connected;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(out);
}

void IGeneralService::SetEthernetCommunicationEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u8 enable = rp.Pop<u8>();

    Network::EmuNetState::Get().ethernet_enabled.store(enable != 0);

    IPC::ResponseBuilder{ctx, 2}.Push(ResultSuccess);
}

void IGeneralService::IsEthernetCommunicationEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_NIFM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    if (Network::GetHostIPv4Address().has_value() && !Settings::values.airplane_mode.GetValue()) {
        rb.Push<u8>(1);
    } else {
        rb.Push<u8>(0);
    }
}

void IGeneralService::IsAnyInternetRequestAccepted(HLERequestContext& ctx) {
    LOG_ERROR(Service_NIFM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    if (Network::GetHostIPv4Address().has_value() && !Settings::values.airplane_mode.GetValue()) {
        rb.Push<u8>(1);
    } else {
        rb.Push<u8>(0);
    }
}

void IGeneralService::IsAnyForegroundRequestAccepted(HLERequestContext& ctx) {
    const bool is_accepted{};

    LOG_WARNING(Service_NIFM, "(STUBBED) called, is_accepted={}", is_accepted);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_accepted);
}

void IGeneralService::GetSsidListVersion(HLERequestContext& ctx) {
    LOG_WARNING(Service_NIFM, "(STUBBED) called");

    constexpr u32 ssid_list_version = 0;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(ssid_list_version);
}

void IGeneralService::ConfirmSystemAvailability(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NIFM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IGeneralService::SetBackgroundRequestEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_NIFM, "(STUBBED) called.");

    // TODO (jarrodnorwell)

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IGeneralService::GetCurrentAccessPoint(HLERequestContext& ctx) {
    Network::RefreshFromHost();

    const auto& st = Network::EmuNetState::Get();

    AccessPointDataV3 access_point_info{};

    if (st.connected && st.via_wifi) {
        access_point_info.ssid_len = static_cast<u8>(std::strlen(st.ssid));
        access_point_info.strength = st.bars;
        access_point_info.visible = 1;
        access_point_info.is_available = 1;
        std::memcpy(access_point_info.ssid, st.ssid, access_point_info.ssid_len);
    }

    ctx.WriteBuffer(access_point_info);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

IGeneralService::IGeneralService(Core::System& system_)
    : ServiceFramework{system_, "IGeneralService"} {
    // clang-format off

    static const FunctionInfo functions[] = {
        {1, &IGeneralService::GetClientId, "GetClientId"},
        {2, &IGeneralService::CreateScanRequest, "CreateScanRequest"},
        {4, &IGeneralService::CreateRequest, "CreateRequest"},
        {5, &IGeneralService::GetCurrentNetworkProfile, "GetCurrentNetworkProfile"},
        {6, &IGeneralService::EnumerateNetworkInterfaces, "EnumerateNetworkInterfaces"},
        {7, &IGeneralService::EnumerateNetworkProfiles, "EnumerateNetworkProfiles"},
        {8, &IGeneralService::GetNetworkProfile, "GetNetworkProfile"},
        {9, &IGeneralService::SetNetworkProfile, "SetNetworkProfile"},
        {10, &IGeneralService::RemoveNetworkProfile, "RemoveNetworkProfile"},
        {11, &IGeneralService::GetScanData, "GetScanDataOld"},
        {12, &IGeneralService::GetCurrentIpAddress, "GetCurrentIpAddress"},
        {13, nullptr, "GetCurrentAccessPointOld"},
        {14, &IGeneralService::CreateTemporaryNetworkProfile, "CreateTemporaryNetworkProfile"},
        {15, &IGeneralService::GetCurrentIpConfigInfo, "GetCurrentIpConfigInfo"},
        {16, &IGeneralService::SetWirelessCommunicationEnabled, "SetWirelessCommunicationEnabled"},
        {17, &IGeneralService::IsWirelessCommunicationEnabled, "IsWirelessCommunicationEnabled"},
        {18, &IGeneralService::GetInternetConnectionStatus, "GetInternetConnectionStatus"},
        {19, &IGeneralService::SetEthernetCommunicationEnabled, "SetEthernetCommunicationEnabled"},
        {20, &IGeneralService::IsEthernetCommunicationEnabled, "IsEthernetCommunicationEnabled"},
        {21, &IGeneralService::IsAnyInternetRequestAccepted, "IsAnyInternetRequestAccepted"},
        {22, &IGeneralService::IsAnyForegroundRequestAccepted, "IsAnyForegroundRequestAccepted"},
        {23, nullptr, "PutToSleep"},
        {24, nullptr, "WakeUp"},
        {25, &IGeneralService::GetSsidListVersion, "GetSsidListVersion"},
        {26, nullptr, "SetExclusiveClient"},
        {27, nullptr, "GetDefaultIpSetting"},
        {28, nullptr, "SetDefaultIpSetting"},
        {29, nullptr, "SetWirelessCommunicationEnabledForTest"},
        {30, nullptr, "SetEthernetCommunicationEnabledForTest"},
        {31, nullptr, "GetTelemetorySystemEventReadableHandle"},
        {32, nullptr, "GetTelemetryInfo"},
        {33, &IGeneralService::ConfirmSystemAvailability, "ConfirmSystemAvailability"}, // 2.0.0+
        {34, &IGeneralService::SetBackgroundRequestEnabled, "SetBackgroundRequestEnabled"}, // 4.0.0+
        {35, &IGeneralService::GetScanDataV2, "GetScanData"},
        {36, &IGeneralService::GetCurrentAccessPoint, "GetCurrentAccessPoint"},
        {37, nullptr, "Shutdown"},
        {38, nullptr, "GetAllowedChannels"},
        {39, nullptr, "NotifyApplicationSuspended"},
        {40, nullptr, "SetAcceptableNetworkTypeFlag"},
        {41, nullptr, "GetAcceptableNetworkTypeFlag"},
        {42, nullptr, "NotifyConnectionStateChanged"},
        {43, nullptr, "SetWowlDelayedWakeTime"},
        {44, nullptr, "IsWiredConnectionAvailable"}, // 18.0.0+
        {45, nullptr, "IsNetworkEmulationFeatureEnabled"}, // 18.0.0+
        {46, nullptr, "SelectActiveNetworkEmulationProfileIdForDebug"}, // 18.0.0+
        {47, &IGeneralService::GetScanDataV3, "GetScanData"}, // 19.0.0+
        {50, nullptr, "IsRewriteFeatureEnabled"}, // 18.0.0+
        {51, nullptr, "CreateRewriteRule"}, // 18.0.0+
        {52, nullptr, "DestroyRewriteRule"} // 18.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IGeneralService::~IGeneralService() = default;

class NetworkInterface final : public ServiceFramework<NetworkInterface> {
public:
    explicit NetworkInterface(const char* name, Core::System& system_)
        : ServiceFramework{system_, name} {
        static const FunctionInfo functions[] = {
            {4, &NetworkInterface::CreateGeneralServiceOld, "CreateGeneralServiceOld"},
            {5, &NetworkInterface::CreateGeneralService, "CreateGeneralService"},
        };
        RegisterHandlers(functions);
    }

private:
    void CreateGeneralServiceOld(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NIFM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IGeneralService>(system);
    }

    void CreateGeneralService(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NIFM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IGeneralService>(system);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("nifm:a",
                                         std::make_shared<NetworkInterface>("nifm:a", system));
    server_manager->RegisterNamedService("nifm:s",
                                         std::make_shared<NetworkInterface>("nifm:s", system));
    server_manager->RegisterNamedService("nifm:u",
                                         std::make_shared<NetworkInterface>("nifm:u", system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::NIFM
