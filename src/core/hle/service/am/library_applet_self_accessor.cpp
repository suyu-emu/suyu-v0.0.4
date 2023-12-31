// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core_timing.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applets/applet_cabinet.h"
#include "core/hle/service/am/applets/applet_controller.h"
#include "core/hle/service/am/applets/applet_mii_edit_types.h"
#include "core/hle/service/am/applets/applet_software_keyboard_types.h"
#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"
#include "hid_core/hid_types.h"

namespace Service::AM {

ILibraryAppletSelfAccessor::ILibraryAppletSelfAccessor(Core::System& system_)
    : ServiceFramework{system_, "ILibraryAppletSelfAccessor"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletSelfAccessor::PopInData, "PopInData"},
        {1, &ILibraryAppletSelfAccessor::PushOutData, "PushOutData"},
        {2, nullptr, "PopInteractiveInData"},
        {3, nullptr, "PushInteractiveOutData"},
        {5, nullptr, "GetPopInDataEvent"},
        {6, nullptr, "GetPopInteractiveInDataEvent"},
        {10, &ILibraryAppletSelfAccessor::ExitProcessAndReturn, "ExitProcessAndReturn"},
        {11, &ILibraryAppletSelfAccessor::GetLibraryAppletInfo, "GetLibraryAppletInfo"},
        {12, &ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo, "GetMainAppletIdentityInfo"},
        {13, nullptr, "CanUseApplicationCore"},
        {14, &ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo, "GetCallerAppletIdentityInfo"},
        {15, nullptr, "GetMainAppletApplicationControlProperty"},
        {16, nullptr, "GetMainAppletStorageId"},
        {17, nullptr, "GetCallerAppletIdentityInfoStack"},
        {18, nullptr, "GetNextReturnDestinationAppletIdentityInfo"},
        {19, &ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout, "GetDesirableKeyboardLayout"},
        {20, nullptr, "PopExtraStorage"},
        {25, nullptr, "GetPopExtraStorageEvent"},
        {30, nullptr, "UnpopInData"},
        {31, nullptr, "UnpopExtraStorage"},
        {40, nullptr, "GetIndirectLayerProducerHandle"},
        {50, nullptr, "ReportVisibleError"},
        {51, nullptr, "ReportVisibleErrorWithErrorContext"},
        {60, nullptr, "GetMainAppletApplicationDesiredLanguage"},
        {70, nullptr, "GetCurrentApplicationId"},
        {80, nullptr, "RequestExitToSelf"},
        {90, nullptr, "CreateApplicationAndPushAndRequestToLaunch"},
        {100, nullptr, "CreateGameMovieTrimmer"},
        {101, nullptr, "ReserveResourceForMovieOperation"},
        {102, nullptr, "UnreserveResourceForMovieOperation"},
        {110, &ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers, "GetMainAppletAvailableUsers"},
        {120, nullptr, "GetLaunchStorageInfoForDebug"},
        {130, nullptr, "GetGpuErrorDetectedSystemEvent"},
        {140, nullptr, "SetApplicationMemoryReservation"},
        {150, &ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually, "ShouldSetGpuTimeSliceManually"},
    };
    // clang-format on
    RegisterHandlers(functions);

    switch (system.GetAppletManager().GetCurrentAppletId()) {
    case Applets::AppletId::Cabinet:
        PushInShowCabinetData();
        break;
    case Applets::AppletId::MiiEdit:
        PushInShowMiiEditData();
        break;
    case Applets::AppletId::PhotoViewer:
        PushInShowAlbum();
        break;
    case Applets::AppletId::SoftwareKeyboard:
        PushInShowSoftwareKeyboard();
        break;
    case Applets::AppletId::Controller:
        PushInShowController();
        break;
    default:
        break;
    }
}

ILibraryAppletSelfAccessor::~ILibraryAppletSelfAccessor() = default;

void ILibraryAppletSelfAccessor::PopInData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    if (queue_data.empty()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultNoDataInChannel);
        return;
    }

    auto data = queue_data.front();
    queue_data.pop_front();

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, std::move(data));
}

void ILibraryAppletSelfAccessor::PushOutData(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::ExitProcessAndReturn(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    system.Exit();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetLibraryAppletInfo(HLERequestContext& ctx) {
    struct LibraryAppletInfo {
        Applets::AppletId applet_id;
        Applets::LibraryAppletMode library_applet_mode;
    };

    LOG_WARNING(Service_AM, "(STUBBED) called");

    const LibraryAppletInfo applet_info{
        .applet_id = system.GetAppletManager().GetCurrentAppletId(),
        .library_applet_mode = Applets::LibraryAppletMode::AllForeground,
    };

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo(HLERequestContext& ctx) {
    struct AppletIdentityInfo {
        Applets::AppletId applet_id;
        INSERT_PADDING_BYTES(0x4);
        u64 application_id;
    };
    static_assert(sizeof(AppletIdentityInfo) == 0x10, "AppletIdentityInfo has incorrect size.");

    LOG_WARNING(Service_AM, "(STUBBED) called");

    const AppletIdentityInfo applet_info{
        .applet_id = Applets::AppletId::QLaunch,
        .application_id = 0x0100000000001000ull,
    };

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo(HLERequestContext& ctx) {
    struct AppletIdentityInfo {
        Applets::AppletId applet_id;
        INSERT_PADDING_BYTES(0x4);
        u64 application_id;
    };
    static_assert(sizeof(AppletIdentityInfo) == 0x10, "AppletIdentityInfo has incorrect size.");
    LOG_WARNING(Service_AM, "(STUBBED) called");

    const AppletIdentityInfo applet_info{
        .applet_id = Applets::AppletId::QLaunch,
        .application_id = 0x0100000000001000ull,
    };

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers(HLERequestContext& ctx) {
    const Service::Account::ProfileManager manager{};
    bool is_empty{true};
    s32 user_count{-1};

    LOG_INFO(Service_AM, "called");

    if (manager.GetUserCount() > 0) {
        is_empty = false;
        user_count = static_cast<s32>(manager.GetUserCount());
        ctx.WriteBuffer(manager.GetAllUsers());
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_empty);
    rb.Push(user_count);
}

void ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(0);
}

void ILibraryAppletSelfAccessor::PushInShowAlbum() {
    const Applets::CommonArguments arguments{
        .arguments_version = Applets::CommonArgumentVersion::Version3,
        .size = Applets::CommonArgumentSize::Version3,
        .library_version = 1,
        .theme_color = Applets::ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> settings_data{2};
    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    queue_data.emplace_back(std::move(argument_data));
    queue_data.emplace_back(std::move(settings_data));
}

void ILibraryAppletSelfAccessor::PushInShowController() {
    const Applets::CommonArguments common_args = {
        .arguments_version = Applets::CommonArgumentVersion::Version3,
        .size = Applets::CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Applets::ControllerAppletVersion::Version8),
        .theme_color = Applets::ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    Applets::ControllerSupportArgNew user_args = {
        .header = {.player_count_min = 1,
                   .player_count_max = 4,
                   .enable_take_over_connection = true,
                   .enable_left_justify = false,
                   .enable_permit_joy_dual = true,
                   .enable_single_mode = false,
                   .enable_identification_color = false},
        .identification_colors = {},
        .enable_explain_text = false,
        .explain_text = {},
    };

    Applets::ControllerSupportArgPrivate private_args = {
        .arg_private_size = sizeof(Applets::ControllerSupportArgPrivate),
        .arg_size = sizeof(Applets::ControllerSupportArgNew),
        .is_home_menu = true,
        .flag_1 = true,
        .mode = Applets::ControllerSupportMode::ShowControllerSupport,
        .caller = Applets::ControllerSupportCaller::
            Application, // switchbrew: Always zero except with
                         // ShowControllerFirmwareUpdateForSystem/ShowControllerKeyRemappingForSystem,
                         // which sets this to the input param
        .style_set = Core::HID::NpadStyleSet::None,
        .joy_hold_type = 0,
    };
    std::vector<u8> common_args_data(sizeof(common_args));
    std::vector<u8> private_args_data(sizeof(private_args));
    std::vector<u8> user_args_data(sizeof(user_args));

    std::memcpy(common_args_data.data(), &common_args, sizeof(common_args));
    std::memcpy(private_args_data.data(), &private_args, sizeof(private_args));
    std::memcpy(user_args_data.data(), &user_args, sizeof(user_args));

    queue_data.emplace_back(std::move(common_args_data));
    queue_data.emplace_back(std::move(private_args_data));
    queue_data.emplace_back(std::move(user_args_data));
}

void ILibraryAppletSelfAccessor::PushInShowCabinetData() {
    const Applets::CommonArguments arguments{
        .arguments_version = Applets::CommonArgumentVersion::Version3,
        .size = Applets::CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Applets::CabinetAppletVersion::Version1),
        .theme_color = Applets::ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    const Applets::StartParamForAmiiboSettings amiibo_settings{
        .param_1 = 0,
        .applet_mode = system.GetAppletManager().GetCabinetMode(),
        .flags = Applets::CabinetFlags::None,
        .amiibo_settings_1 = 0,
        .device_handle = 0,
        .tag_info{},
        .register_info{},
        .amiibo_settings_3{},
    };

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> settings_data(sizeof(amiibo_settings));
    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    std::memcpy(settings_data.data(), &amiibo_settings, sizeof(amiibo_settings));
    queue_data.emplace_back(std::move(argument_data));
    queue_data.emplace_back(std::move(settings_data));
}

void ILibraryAppletSelfAccessor::PushInShowMiiEditData() {
    struct MiiEditV3 {
        Applets::MiiEditAppletInputCommon common;
        Applets::MiiEditAppletInputV3 input;
    };
    static_assert(sizeof(MiiEditV3) == 0x100, "MiiEditV3 has incorrect size.");

    MiiEditV3 mii_arguments{
        .common =
            {
                .version = Applets::MiiEditAppletVersion::Version3,
                .applet_mode = Applets::MiiEditAppletMode::ShowMiiEdit,
            },
        .input{},
    };

    std::vector<u8> argument_data(sizeof(mii_arguments));
    std::memcpy(argument_data.data(), &mii_arguments, sizeof(mii_arguments));

    queue_data.emplace_back(std::move(argument_data));
}

void ILibraryAppletSelfAccessor::PushInShowSoftwareKeyboard() {
    const Applets::CommonArguments arguments{
        .arguments_version = Applets::CommonArgumentVersion::Version3,
        .size = Applets::CommonArgumentSize::Version3,
        .library_version = static_cast<u32>(Applets::SwkbdAppletVersion::Version524301),
        .theme_color = Applets::ThemeColor::BasicBlack,
        .play_startup_sound = true,
        .system_tick = system.CoreTiming().GetClockTicks(),
    };

    std::vector<char16_t> initial_string(0);

    const Applets::SwkbdConfigCommon swkbd_config{
        .type = Applets::SwkbdType::Qwerty,
        .ok_text{},
        .left_optional_symbol_key{},
        .right_optional_symbol_key{},
        .use_prediction = false,
        .key_disable_flags{},
        .initial_cursor_position = Applets::SwkbdInitialCursorPosition::Start,
        .header_text{},
        .sub_text{},
        .guide_text{},
        .max_text_length = 500,
        .min_text_length = 0,
        .password_mode = Applets::SwkbdPasswordMode::Disabled,
        .text_draw_type = Applets::SwkbdTextDrawType::Box,
        .enable_return_button = true,
        .use_utf8 = false,
        .use_blur_background = true,
        .initial_string_offset{},
        .initial_string_length = static_cast<u32>(initial_string.size()),
        .user_dictionary_offset{},
        .user_dictionary_entries{},
        .use_text_check = false,
    };

    Applets::SwkbdConfigNew swkbd_config_new{};

    std::vector<u8> argument_data(sizeof(arguments));
    std::vector<u8> swkbd_data(sizeof(swkbd_config) + sizeof(swkbd_config_new));
    std::vector<u8> work_buffer(swkbd_config.initial_string_length * sizeof(char16_t));

    std::memcpy(argument_data.data(), &arguments, sizeof(arguments));
    std::memcpy(swkbd_data.data(), &swkbd_config, sizeof(swkbd_config));
    std::memcpy(swkbd_data.data() + sizeof(swkbd_config), &swkbd_config_new,
                sizeof(Applets::SwkbdConfigNew));
    std::memcpy(work_buffer.data(), initial_string.data(),
                swkbd_config.initial_string_length * sizeof(char16_t));

    queue_data.emplace_back(std::move(argument_data));
    queue_data.emplace_back(std::move(swkbd_data));
    queue_data.emplace_back(std::move(work_buffer));
}

} // namespace Service::AM
