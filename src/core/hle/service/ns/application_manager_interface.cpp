// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/control_metadata.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/ns/application_manager_interface.h"

#include "core/file_sys/content_archive.h"
#include "core/hle/service/ns/content_management_interface.h"
#include "core/hle/service/ns/read_only_application_control_data_interface.h"
#include "core/file_sys/patch_manager.h"
#include "frontend_common/firmware_manager.h"
#include "core/launch_timestamp_cache.h"

#include <algorithm>
#include <vector>

namespace Service::NS {

IApplicationManagerInterface::IApplicationManagerInterface(Core::System& system_)
    : ServiceFramework{system_, "IApplicationManagerInterface"},
      service_context{system, "IApplicationManagerInterface"},
      record_update_system_event{service_context}, sd_card_mount_status_event{service_context},
      gamecard_update_detection_event{service_context},
      gamecard_mount_status_event{service_context}, gamecard_mount_failure_event{service_context},
      gamecard_waken_ready_event{service_context}, unknown_event{service_context} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IApplicationManagerInterface::ListApplicationRecord>, "ListApplicationRecord"},
        {1, nullptr, "GenerateApplicationRecordCount"},
        {2, D<&IApplicationManagerInterface::GetApplicationRecordUpdateSystemEvent>, "GetApplicationRecordUpdateSystemEvent"},
        {3, nullptr, "GetApplicationViewDeprecated"},
        {4, nullptr, "DeleteApplicationEntity"},
        {5, nullptr, "DeleteApplicationCompletely"},
        {6, nullptr, "IsAnyApplicationEntityRedundant"},
        {7, nullptr, "DeleteRedundantApplicationEntity"},
        {8, nullptr, "IsApplicationEntityMovable"},
        {9, nullptr, "MoveApplicationEntity"},
        {11, nullptr, "CalculateApplicationOccupiedSize"},
        {16, nullptr, "PushApplicationRecord"},
        {17, nullptr, "ListApplicationRecordContentMeta"},
        {19, nullptr, "LaunchApplicationOld"},
        {21, nullptr, "GetApplicationContentPath"},
        {22, nullptr, "TerminateApplication"},
        {23, nullptr, "ResolveApplicationContentPath"},
        {26, nullptr, "BeginInstallApplication"},
        {27, nullptr, "DeleteApplicationRecord"},
        {30, nullptr, "RequestApplicationUpdateInfo"},
        {31, nullptr, "RequestUpdateApplication"},
        {32, nullptr, "CancelApplicationDownload"},
        {33, nullptr, "ResumeApplicationDownload"},
        {35, nullptr, "UpdateVersionList"},
        {36, nullptr, "PushLaunchVersion"},
        {37, nullptr, "ListRequiredVersion"},
        {38, D<&IApplicationManagerInterface::CheckApplicationLaunchVersion>, "CheckApplicationLaunchVersion"},
        {39, nullptr, "CheckApplicationLaunchRights"},
        {40, D<&IApplicationManagerInterface::GetApplicationLogoData>, "GetApplicationLogoData"},
        {41, nullptr, "CalculateApplicationDownloadRequiredSize"},
        {42, nullptr, "CleanupSdCard"},
        {43, D<&IApplicationManagerInterface::CheckSdCardMountStatus>, "CheckSdCardMountStatus"},
        {44, D<&IApplicationManagerInterface::GetSdCardMountStatusChangedEvent>, "GetSdCardMountStatusChangedEvent"},
        {45, nullptr, "GetGameCardAttachmentEvent"},
        {46, nullptr, "GetGameCardAttachmentInfo"},
        {47, D<&IApplicationManagerInterface::GetTotalSpaceSize>, "GetTotalSpaceSize"},
        {48, D<&IApplicationManagerInterface::GetFreeSpaceSize>, "GetFreeSpaceSize"},
        {49, nullptr, "GetSdCardRemovedEvent"},
        {52, D<&IApplicationManagerInterface::GetGameCardUpdateDetectionEvent>, "GetGameCardUpdateDetectionEvent"},
        {53, nullptr, "DisableApplicationAutoDelete"},
        {54, nullptr, "EnableApplicationAutoDelete"},
        {55, D<&IApplicationManagerInterface::GetApplicationDesiredLanguage>, "GetApplicationDesiredLanguage"},
        {56, nullptr, "SetApplicationTerminateResult"},
        {57, nullptr, "ClearApplicationTerminateResult"},
        {58, nullptr, "GetLastSdCardMountUnexpectedResult"},
        {59, D<&IApplicationManagerInterface::ConvertApplicationLanguageToLanguageCode>, "ConvertApplicationLanguageToLanguageCode"},
        {60, nullptr, "ConvertLanguageCodeToApplicationLanguage"},
        {61, nullptr, "GetBackgroundDownloadStressTaskInfo"},
        {62, nullptr, "GetGameCardStopper"},
        {63, nullptr, "IsSystemProgramInstalled"},
        {64, nullptr, "StartApplyDeltaTask"},
        {65, nullptr, "GetRequestServerStopper"},
        {66, nullptr, "GetBackgroundApplyDeltaStressTaskInfo"},
        {67, nullptr, "CancelApplicationApplyDelta"},
        {68, nullptr, "ResumeApplicationApplyDelta"},
        {69, nullptr, "CalculateApplicationApplyDeltaRequiredSize"},
        {70, D<&IApplicationManagerInterface::ResumeAll>, "ResumeAll"},
        {71, D<&IApplicationManagerInterface::GetStorageSize>, "GetStorageSize"},
        {80, nullptr, "RequestDownloadApplication"},
        {81, nullptr, "RequestDownloadAddOnContent"},
        {82, nullptr, "DownloadApplication"},
        {83, nullptr, "CheckApplicationResumeRights"},
        {84, nullptr, "GetDynamicCommitEvent"},
        {85, nullptr, "RequestUpdateApplication2"},
        {86, nullptr, "EnableApplicationCrashReport"},
        {87, nullptr, "IsApplicationCrashReportEnabled"},
        {90, nullptr, "BoostSystemMemoryResourceLimit"},
        {91, nullptr, "DeprecatedLaunchApplication"},
        {92, nullptr, "GetRunningApplicationProgramId"},
        {93, nullptr, "GetMainApplicationProgramIndex"},
        {94, nullptr, "LaunchApplication"},
        {95, nullptr, "GetApplicationLaunchInfo"},
        {96, nullptr, "AcquireApplicationLaunchInfo"},
        {97, nullptr, "GetMainApplicationProgramIndexByApplicationLaunchInfo"},
        {98, nullptr, "EnableApplicationAllThreadDumpOnCrash"},
        {99, nullptr, "LaunchDevMenu"},
        {100, nullptr, "ResetToFactorySettings"},
        {101, nullptr, "ResetToFactorySettingsWithoutUserSaveData"},
        {102, nullptr, "ResetToFactorySettingsForRefurbishment"},
        {103, nullptr, "ResetToFactorySettingsWithPlatformRegion"},
        {104, nullptr, "ResetToFactorySettingsWithPlatformRegionAuthentication"},
        {105, nullptr, "RequestResetToFactorySettingsSecurely"},
        {106, nullptr, "RequestResetToFactorySettingsWithPlatformRegionAuthenticationSecurely"},
        {200, nullptr, "CalculateUserSaveDataStatistics"},
        {201, nullptr, "DeleteUserSaveDataAll"},
        {210, nullptr, "DeleteUserSystemSaveData"},
        {211, nullptr, "DeleteSaveData"},
        {220, nullptr, "UnregisterNetworkServiceAccount"},
        {221, D<&IApplicationManagerInterface::UnregisterNetworkServiceAccountWithUserSaveDataDeletion>, "UnregisterNetworkServiceAccountWithUserSaveDataDeletion"},
        {300, nullptr, "GetApplicationShellEvent"},
        {301, nullptr, "PopApplicationShellEventInfo"},
        {302, nullptr, "LaunchLibraryApplet"},
        {303, nullptr, "TerminateLibraryApplet"},
        {304, nullptr, "LaunchSystemApplet"},
        {305, nullptr, "TerminateSystemApplet"},
        {306, nullptr, "LaunchOverlayApplet"},
        {307, nullptr, "TerminateOverlayApplet"},
        {400, D<&IApplicationManagerInterface::GetApplicationControlData>, "GetApplicationControlData"},
        {401, nullptr, "InvalidateAllApplicationControlCache"},
        {402, nullptr, "RequestDownloadApplicationControlData"},
        {403, nullptr, "GetMaxApplicationControlCacheCount"},
        {404, nullptr, "InvalidateApplicationControlCache"},
        {405, nullptr, "ListApplicationControlCacheEntryInfo"},
        {406, nullptr, "GetApplicationControlProperty"},
        {407, &IApplicationManagerInterface::ListApplicationTitle, "ListApplicationTitle"},
        {408, nullptr, "ListApplicationIcon"},
        {411, nullptr, "Unknown411"}, //19.0.0+
        {412, nullptr, "Unknown412"}, //19.0.0+
        {413, nullptr, "Unknown413"}, //19.0.0+
        {414, nullptr, "Unknown414"}, //19.0.0+
        {415, nullptr, "Unknown415"}, //19.0.0+
        {416, nullptr, "Unknown416"}, //19.0.0+
        {417, nullptr, "InvalidateAllApplicationControlCacheOfTheStage"}, //19.0.0+
        {418, nullptr, "InvalidateApplicationControlCacheOfTheStage"}, //19.0.0+
        {419, D<&IApplicationManagerInterface::RequestDownloadApplicationControlDataInBackground>, "RequestDownloadApplicationControlDataInBackground"},
        {420, nullptr, "CloneApplicationControlDataCacheForDebug"},
        {421, nullptr, "Unknown421"}, //20.0.0+
        {422, nullptr, "Unknown422"}, //20.0.0+
        {423, nullptr, "Unknown423"}, //20.0.0+
        {424, nullptr, "Unknown424"}, //20.0.0+
        {425, nullptr, "Unknown425"}, //20.0.0+
        {426, nullptr, "Unknown426"}, //20.0.0+
        {427, nullptr, "Unknown427"}, //20.0.0+
        {428, nullptr, "Unknown428"}, //21.0.0+
        {429, nullptr, "Unknown429"}, //21.0.0+
        {430, nullptr, "Unknown430"}, //21.0.0+
        {502, nullptr, "RequestCheckGameCardRegistration"},
        {503, nullptr, "RequestGameCardRegistrationGoldPoint"},
        {504, nullptr, "RequestRegisterGameCard"},
        {505, D<&IApplicationManagerInterface::GetGameCardMountFailureEvent>, "GetGameCardMountFailureEvent"},
        {506, nullptr, "IsGameCardInserted"},
        {507, nullptr, "EnsureGameCardAccess"},
        {508, nullptr, "GetLastGameCardMountFailureResult"},
        {509, nullptr, "ListApplicationIdOnGameCard"},
        {510, nullptr, "GetGameCardPlatformRegion"},
        {511, D<&IApplicationManagerInterface::GetGameCardWakenReadyEvent>, "GetGameCardWakenReadyEvent"},
        {512, D<&IApplicationManagerInterface::IsGameCardApplicationRunning>, "IsGameCardApplicationRunning"},
        {513, nullptr, "Unknown513"}, //20.0.0+
        {514, nullptr, "Unknown514"}, //20.0.0+
        {515, nullptr, "Unknown515"}, //20.0.0+
        {516, nullptr, "Unknown516"}, //21.0.0+
        {517, nullptr, "Unknown517"}, //21.0.0+
        {518, nullptr, "Unknown518"}, //21.0.0+
        {519, nullptr, "Unknown519"}, //21.0.0+
        {600, nullptr, "CountApplicationContentMeta"},
        {601, nullptr, "ListApplicationContentMetaStatus"},
        {602, nullptr, "ListAvailableAddOnContent"},
        {603, nullptr, "GetOwnedApplicationContentMetaStatus"},
        {604, nullptr, "RegisterContentsExternalKey"},
        {605, nullptr, "ListApplicationContentMetaStatusWithRightsCheck"},
        {606, nullptr, "GetContentMetaStorage"},
        {607, nullptr, "ListAvailableAddOnContent"},
        {609, nullptr, "ListAvailabilityAssuredAddOnContent"},
        {610, nullptr, "GetInstalledContentMetaStorage"},
        {611, nullptr, "PrepareAddOnContent"},
        {700, nullptr, "PushDownloadTaskList"},
        {701, nullptr, "ClearTaskStatusList"},
        {702, nullptr, "RequestDownloadTaskList"},
        {703, nullptr, "RequestEnsureDownloadTask"},
        {704, nullptr, "ListDownloadTaskStatus"},
        {705, nullptr, "RequestDownloadTaskListData"},
        {800, nullptr, "RequestVersionList"},
        {801, nullptr, "ListVersionList"},
        {802, nullptr, "RequestVersionListData"},
        {900, nullptr, "GetApplicationRecord"},
        {901, nullptr, "GetApplicationRecordProperty"},
        {902, nullptr, "EnableApplicationAutoUpdate"},
        {903, nullptr, "DisableApplicationAutoUpdate"},
        {904, D<&IApplicationManagerInterface::TouchApplication>, "TouchApplication"},
        {905, nullptr, "RequestApplicationUpdate"},
        {906, D<&IApplicationManagerInterface::IsApplicationUpdateRequested>, "IsApplicationUpdateRequested"},
        {907, nullptr, "WithdrawApplicationUpdateRequest"},
        {908, nullptr, "ListApplicationRecordInstalledContentMeta"},
        {909, nullptr, "WithdrawCleanupAddOnContentsWithNoRightsRecommendation"},
        {910, nullptr, "HasApplicationRecord"},
        {911, nullptr, "SetPreInstalledApplication"},
        {912, nullptr, "ClearPreInstalledApplicationFlag"},
        {913, nullptr, "ListAllApplicationRecord"},
        {914, nullptr, "HideApplicationRecord"},
        {915, nullptr, "ShowApplicationRecord"},
        {916, nullptr, "IsApplicationAutoDeleteDisabled"},
        {916, nullptr, "Unknown916"}, //20.0.0+
        {917, nullptr, "Unknown917"}, //20.0.0+
        {918, nullptr, "Unknown918"}, //20.0.0+
        {919, nullptr, "Unknown919"}, //20.0.0+
        {920, nullptr, "Unknown920"}, //20.0.0+
        {921, nullptr, "Unknown921"}, //20.0.0+
        {922, nullptr, "Unknown922"}, //20.0.0+
        {923, nullptr, "Unknown923"}, //20.0.0+
        {928, nullptr, "Unknown928"}, //20.0.0+
        {929, nullptr, "Unknown929"}, //20.0.0+
        {930, nullptr, "Unknown930"}, //20.0.0+
        {931, nullptr, "Unknown931"}, //20.0.0+
        {933, nullptr, "Unknown933"}, //20.0.0+
        {934, nullptr, "Unknown934"}, //20.0.0+
        {935, nullptr, "Unknown935"}, //20.0.0+
        {936, nullptr, "Unknown936"}, //20.0.0+
        {1000, nullptr, "RequestVerifyApplicationDeprecated"},
        {1001, nullptr, "CorruptApplicationForDebug"},
        {1002, nullptr, "RequestVerifyAddOnContentsRights"},
        {1003, nullptr, "RequestVerifyApplication"},
        {1004, nullptr, "CorruptContentForDebug"},
        {1200, nullptr, "NeedsUpdateVulnerability"},
        {1300, D<&IApplicationManagerInterface::IsAnyApplicationEntityInstalled>, "IsAnyApplicationEntityInstalled"},
        {1301, nullptr, "DeleteApplicationContentEntities"},
        {1302, nullptr, "CleanupUnrecordedApplicationEntity"},
        {1303, nullptr, "CleanupAddOnContentsWithNoRights"},
        {1304, nullptr, "DeleteApplicationContentEntity"},
        {1305, nullptr, "TryDeleteRunningApplicationEntity"},
        {1306, nullptr, "TryDeleteRunningApplicationCompletely"},
        {1307, nullptr, "TryDeleteRunningApplicationContentEntities"},
        {1308, nullptr, "DeleteApplicationCompletelyForDebug"},
        {1309, nullptr, "CleanupUnavailableAddOnContents"},
        {1310, nullptr, "RequestMoveApplicationEntity"},
        {1311, nullptr, "EstimateSizeToMove"},
        {1312, nullptr, "HasMovableEntity"},
        {1313, nullptr, "CleanupOrphanContents"},
        {1314, nullptr, "CheckPreconditionSatisfiedToMove"},
        {1400, nullptr, "PrepareShutdown"},
        {1500, nullptr, "FormatSdCard"},
        {1501, nullptr, "NeedsSystemUpdateToFormatSdCard"},
        {1502, nullptr, "GetLastSdCardFormatUnexpectedResult"},
        {1504, nullptr, "InsertSdCard"},
        {1505, nullptr, "RemoveSdCard"},
        {1506, nullptr, "GetSdCardStartupStatus"},
        {1508, nullptr, "Unknown1508"}, //20.0.0+
        {1509, nullptr, "Unknown1509"}, //20.0.0+
        {1510, nullptr, "Unknown1510"}, //20.0.0+
        {1511, nullptr, "Unknown1511"}, //20.0.0+
        {1512, nullptr, "Unknown1512"}, //20.0.0+
        {1600, nullptr, "GetSystemSeedForPseudoDeviceId"},
        {1601, nullptr, "ResetSystemSeedForPseudoDeviceId"},
        {1700, nullptr, "ListApplicationDownloadingContentMeta"},
        {1701, D<&IApplicationManagerInterface::GetApplicationViewDeprecated>, "GetApplicationViewDeprecated"},
        {1702, nullptr, "GetApplicationDownloadTaskStatus"},
        {1703, nullptr, "GetApplicationViewDownloadErrorContext"},
        {1704, D<&IApplicationManagerInterface::GetApplicationViewWithPromotionInfo>, "GetApplicationViewWithPromotionInfo"},
        {1705, nullptr, "IsPatchAutoDeletableApplication"},
        {1706, D<&IApplicationManagerInterface::GetApplicationView>, "GetApplicationView"},
        {1800, nullptr, "IsNotificationSetupCompleted"},
        {1801, nullptr, "GetLastNotificationInfoCount"},
        {1802, nullptr, "ListLastNotificationInfo"},
        {1803, nullptr, "ListNotificationTask"},
        {1900, nullptr, "IsActiveAccount"},
        {1901, nullptr, "RequestDownloadApplicationPrepurchasedRights"},
        {1902, nullptr, "GetApplicationTicketInfo"},
        {1903, nullptr, "RequestDownloadApplicationPrepurchasedRightsForAccount"},
        {2000, nullptr, "GetSystemDeliveryInfo"},
        {2001, nullptr, "SelectLatestSystemDeliveryInfo"},
        {2002, nullptr, "VerifyDeliveryProtocolVersion"},
        {2003, nullptr, "GetApplicationDeliveryInfo"},
        {2004, nullptr, "HasAllContentsToDeliver"},
        {2005, nullptr, "CompareApplicationDeliveryInfo"},
        {2006, nullptr, "CanDeliverApplication"},
        {2007, nullptr, "ListContentMetaKeyToDeliverApplication"},
        {2008, nullptr, "NeedsSystemUpdateToDeliverApplication"},
        {2009, nullptr, "EstimateRequiredSize"},
        {2010, nullptr, "RequestReceiveApplication"},
        {2011, nullptr, "CommitReceiveApplication"},
        {2012, nullptr, "GetReceiveApplicationProgress"},
        {2013, nullptr, "RequestSendApplication"},
        {2014, nullptr, "GetSendApplicationProgress"},
        {2015, nullptr, "CompareSystemDeliveryInfo"},
        {2016, nullptr, "ListNotCommittedContentMeta"},
        {2017, nullptr, "CreateDownloadTask"},
        {2018, nullptr, "GetApplicationDeliveryInfoHash"},
        {2019, nullptr, "Unknown2019"}, //20.0.0+
        {2050, D<&IApplicationManagerInterface::GetApplicationRightsOnClient>, "GetApplicationRightsOnClient"},
        {2051, nullptr, "InvalidateRightsIdCache"},
        {2052, nullptr, "Unknown2052"}, //20.0.0+
        {2053, nullptr, "Unknown2053"}, //20.0.0+
        {2100, D<&IApplicationManagerInterface::GetApplicationTerminateResult>, "GetApplicationTerminateResult"},
        {2101, nullptr, "GetRawApplicationTerminateResult"},
        {2150, nullptr, "CreateRightsEnvironment"},
        {2151, nullptr, "DestroyRightsEnvironment"},
        {2152, nullptr, "ActivateRightsEnvironment"},
        {2153, nullptr, "DeactivateRightsEnvironment"},
        {2154, nullptr, "ForceActivateRightsContextForExit"},
        {2155, nullptr, "UpdateRightsEnvironmentStatus"},
        {2156, nullptr, "CreateRightsEnvironmentForMicroApplication"},
        {2160, nullptr, "AddTargetApplicationToRightsEnvironment"},
        {2161, nullptr, "SetUsersToRightsEnvironment"},
        {2170, nullptr, "GetRightsEnvironmentStatus"},
        {2171, nullptr, "GetRightsEnvironmentStatusChangedEvent"},
        {2180, nullptr, "RequestExtendRightsInRightsEnvironment"},
        {2181, nullptr, "GetResultOfExtendRightsInRightsEnvironment"},
        {2182, nullptr, "SetActiveRightsContextUsingStateToRightsEnvironment"},
        {2183, nullptr, "Unknown2183"}, //20.1.0+
        {2190, nullptr, "GetRightsEnvironmentHandleForApplication"},
        {2199, nullptr, "GetRightsEnvironmentCountForDebug"},
        {2200, nullptr, "GetGameCardApplicationCopyIdentifier"},
        {2201, nullptr, "GetInstalledApplicationCopyIdentifier"},
        {2250, nullptr, "RequestReportActiveELicence"},
        {2300, nullptr, "ListEventLog"},
        {2350, nullptr, "PerformAutoUpdateByApplicationId"},
        {2351, nullptr, "RequestNoDownloadRightsErrorResolution"},
        {2352, nullptr, "RequestResolveNoDownloadRightsError"},
        {2353, nullptr, "GetApplicationDownloadTaskInfo"},
        {2354, nullptr, "PrioritizeApplicationBackgroundTask"},
        {2355, nullptr, "PreferStorageEfficientUpdate"},
        {2356, nullptr, "RequestStorageEfficientUpdatePreferable"},
        {2357, nullptr, "EnableMultiCoreDownload"},
        {2358, nullptr, "DisableMultiCoreDownload"},
        {2359, nullptr, "IsMultiCoreDownloadEnabled"},
        {2360, nullptr, "GetApplicationDownloadTaskCount"}, //19.0.0+
        {2361, nullptr, "GetMaxApplicationDownloadTaskCount"}, //19.0.0+
        {2362, nullptr, "Unknown2362"}, //20.0.0+
        {2363, nullptr, "Unknown2363"}, //20.0.0+
        {2364, nullptr, "Unknown2364"}, //20.0.0+
        {2365, nullptr, "Unknown2365"}, //20.0.0+
        {2366, nullptr, "Unknown2366"}, //20.0.0+
        {2367, nullptr, "Unknown2367"}, //20.0.0+
        {2368, nullptr, "Unknown2368"}, //20.0.0+
        {2369, nullptr, "Unknown2369"}, //21.0.0+
        {2400, nullptr, "GetPromotionInfo"},
        {2401, nullptr, "CountPromotionInfo"},
        {2402, nullptr, "ListPromotionInfo"},
        {2403, nullptr, "ImportPromotionJsonForDebug"},
        {2404, nullptr, "ClearPromotionInfoForDebug"},
        {2500, nullptr, "ConfirmAvailableTime"},
        {2510, nullptr, "CreateApplicationResource"},
        {2511, nullptr, "GetApplicationResource"},
        {2513, nullptr, "LaunchMicroApplication"},
        {2514, nullptr, "ClearTaskOfAsyncTaskManager"},
        {2515, nullptr, "CleanupAllPlaceHolderAndFragmentsIfNoTask"},
        {2516, nullptr, "EnsureApplicationCertificate"},
        {2517, nullptr, "CreateApplicationInstance"},
        {2518, nullptr, "UpdateQualificationForDebug"},
        {2519, nullptr, "IsQualificationTransitionSupported"},
        {2520, D<&IApplicationManagerInterface::IsQualificationTransitionSupportedByProcessId>, "IsQualificationTransitionSupportedByProcessId"},
        {2521, nullptr, "GetRightsUserChangedEvent"},
        {2522, nullptr, "IsRomRedirectionAvailable"},
        {2523, nullptr, "GetProgramId"}, //17.0.0+
        {2524, nullptr, "Unknown2524"}, //19.0.0+
        {2525, nullptr, "Unknown2525"}, //20.0.0+
        {2800, nullptr, "GetApplicationIdOfPreomia"},
        {3000, nullptr, "RegisterDeviceLockKey"},
        {3001, nullptr, "UnregisterDeviceLockKey"},
        {3002, nullptr, "VerifyDeviceLockKey"},
        {3003, nullptr, "HideApplicationIcon"},
        {3004, nullptr, "ShowApplicationIcon"},
        {3005, nullptr, "HideApplicationTitle"},
        {3006, nullptr, "ShowApplicationTitle"},
        {3007, nullptr, "EnableGameCard"},
        {3008, nullptr, "DisableGameCard"},
        {3009, nullptr, "EnableLocalContentShare"},
        {3010, nullptr, "DisableLocalContentShare"},
        {3011, nullptr, "IsApplicationIconHidden"},
        {3012, nullptr, "IsApplicationTitleHidden"},
        {3013, nullptr, "IsGameCardEnabled"},
        {3014, nullptr, "IsLocalContentShareEnabled"},
        {3050, nullptr, "ListAssignELicenseTaskResult"},
        {3104, nullptr, "GetApplicationNintendoLogo"}, //18.0.0+
        {3105, nullptr, "GetApplicationStartupMovie"}, //18.0.0+
        {4000, nullptr, "Unknown4000"}, //20.0.0+
        {4004, nullptr, "Unknown4004"}, //20.0.0+
        {4006, nullptr, "Unknown4006"}, //20.0.0+
        {4007, nullptr, "Unknown4007"}, //20.0.0+
        {4008, nullptr, "Unknown4008"}, //20.0.0+
        {4009, nullptr, "Unknown4009"}, //20.0.0+
        {4010, nullptr, "Unknown4010"}, //20.0.0+
        {4011, nullptr, "Unknown4011"}, //20.0.0+
        {4012, nullptr, "Unknown4012"}, //20.0.0+
        {4013, nullptr, "Unknown4013"}, //20.0.0+
        {4015, nullptr, "Unknown4015"}, //20.0.0+
        {4017, nullptr, "Unknown4017"}, //20.0.0+
        {4019, nullptr, "Unknown4019"}, //20.0.0+
        {4020, nullptr, "Unknown4020"}, //20.0.0+
        {4021, nullptr, "Unknown4021"}, //20.0.0+
        {4022, D<&IApplicationManagerInterface::Unknown4022>, "Unknown4022"}, //20.0.0+
        {4023, D<&IApplicationManagerInterface::Unknown4023>, "Unknown4023"}, //20.0.0+
        {4024, nullptr, "Unknown4024"}, //20.0.0+
        {4025, nullptr, "Unknown4025"}, //20.0.0+
        {4026, nullptr, "Unknown4026"}, //20.0.0+
        {4027, nullptr, "Unknown4027"}, //20.0.0+
        {4028, nullptr, "Unknown4028"}, //20.0.0+
        {4029, nullptr, "Unknown4029"}, //20.0.0+
        {4030, nullptr, "Unknown4030"}, //20.0.0+
        {4031, nullptr, "Unknown4031"}, //20.0.0+
        {4032, nullptr, "Unknown4032"}, //20.0.0+
        {4033, nullptr, "Unknown4033"}, //20.0.0+
        {4034, nullptr, "Unknown4034"}, //20.0.0+
        {4035, nullptr, "Unknown4035"}, //20.0.0+
        {4037, nullptr, "Unknown4037"}, //20.0.0+
        {4038, nullptr, "Unknown4038"}, //20.0.0+
        {4039, nullptr, "Unknown4039"}, //20.0.0+
        {4040, nullptr, "Unknown4040"}, //20.0.0+
        {4041, nullptr, "Unknown4041"}, //20.0.0+
        {4042, nullptr, "Unknown4042"}, //20.0.0+
        {4043, nullptr, "Unknown4043"}, //20.0.0+
        {4044, nullptr, "Unknown4044"}, //20.0.0+
        {4045, nullptr, "Unknown4045"}, //20.0.0+
        {4046, nullptr, "Unknown4046"}, //20.0.0+
        {4049, nullptr, "Unknown4049"}, //20.0.0+
        {4050, nullptr, "Unknown4050"}, //20.0.0+
        {4051, nullptr, "Unknown4051"}, //20.0.0+
        {4052, nullptr, "Unknown4052"}, //20.0.0+
        {4053, D<&IApplicationManagerInterface::Unknown4053>, "Unknown4053"}, //20.0.0+
        {4054, nullptr, "Unknown4054"}, //20.0.0+
        {4055, nullptr, "Unknown4055"}, //20.0.0+
        {4056, nullptr, "Unknown4056"}, //20.0.0+
        {4057, nullptr, "Unknown4057"}, //20.0.0+
        {4058, nullptr, "Unknown4058"}, //20.0.0+
        {4059, nullptr, "Unknown4059"}, //20.0.0+
        {4060, nullptr, "Unknown4060"}, //20.0.0+
        {4061, nullptr, "Unknown4061"}, //20.0.0+
        {4062, nullptr, "Unknown4062"}, //20.0.0+
        {4063, nullptr, "Unknown4063"}, //20.0.0+
        {4064, nullptr, "Unknown4064"}, //20.0.0+
        {4065, nullptr, "Unknown4065"}, //20.0.0+
        {4066, nullptr, "Unknown4066"}, //20.0.0+
        {4067, nullptr, "Unknown4067"}, //20.0.0+
        {4068, nullptr, "Unknown4068"}, //20.0.0+
        {4069, nullptr, "Unknown4069"}, //20.0.0+
        {4070, nullptr, "Unknown4070"}, //20.0.0+
        {4071, nullptr, "Unknown4071"}, //20.0.0+
        {4072, nullptr, "Unknown4072"}, //20.0.0+
        {4073, nullptr, "Unknown4073"}, //20.0.0+
        {4074, nullptr, "Unknown4074"}, //20.0.0+
        {4075, nullptr, "Unknown4075"}, //20.0.0+
        {4076, nullptr, "Unknown4076"}, //20.0.0+
        {4077, nullptr, "Unknown4077"}, //20.0.0+
        {4078, nullptr, "Unknown4078"}, //20.0.0+
        {4079, nullptr, "Unknown4079"}, //20.0.0+
        {4080, nullptr, "Unknown4080"}, //20.0.0+
        {4081, nullptr, "Unknown4081"}, //20.0.0+
        {4083, nullptr, "Unknown4083"}, //20.0.0+
        {4084, nullptr, "Unknown4084"}, //20.0.0+
        {4085, nullptr, "Unknown4085"}, //20.0.0+
        {4086, nullptr, "Unknown4086"}, //20.0.0+
        {4087, nullptr, "Unknown4087"}, //20.0.0+
        {4088, D<&IApplicationManagerInterface::Unknown4022>, "Unknown4088"}, //20.0.0+
        {4089, nullptr, "Unknown4089"}, //20.0.0+
        {4090, nullptr, "Unknown4090"}, //20.0.0+
        {4091, nullptr, "Unknown4091"}, //20.0.0+
        {4092, nullptr, "Unknown4092"}, //20.0.0+
        {4093, nullptr, "Unknown4093"}, //20.0.0+
        {4094, nullptr, "Unknown4094"}, //20.0.0+
        {4095, nullptr, "Unknown4095"}, //20.0.0+
        {4096, nullptr, "Unknown4096"}, //20.0.0+
        {4097, nullptr, "Unknown4097"}, //20.0.0+
        {4099, nullptr, "Unknown4099"}, //21.0.0+
        {5000, nullptr, "Unknown5000"}, //18.0.0+
        {5001, nullptr, "Unknown5001"}, //18.0.0+
        {9999, nullptr, "GetApplicationCertificate"}, //10.0.0-10.2.0
    };
    // clang-format on

    RegisterHandlers(functions);
}

IApplicationManagerInterface::~IApplicationManagerInterface() = default;

Result IApplicationManagerInterface::UnregisterNetworkServiceAccountWithUserSaveDataDeletion(Common::UUID user_id) {
    LOG_DEBUG(Service_NS, "called, user_id={}", user_id.FormattedString());
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationLogoData(
    Out<s64> out_size, OutBuffer<BufferAttr_HipcMapAlias> out_buffer, u64 application_id,
    InBuffer<BufferAttr_HipcMapAlias> logo_path_buffer) {
    const std::string path_view{reinterpret_cast<const char*>(logo_path_buffer.data()),
                                logo_path_buffer.size()};

    // Find null terminator and trim the path
    auto null_pos = path_view.find('\0');
    std::string path = (null_pos != std::string::npos) ? path_view.substr(0, null_pos) : path_view;

    LOG_DEBUG(Service_NS, "called, application_id={:016X}, logo_path={}", application_id, path);

    auto& content_provider = system.GetContentProviderUnion();

    auto program = content_provider.GetEntry(application_id, FileSys::ContentRecordType::Program);
    if (!program) {
        LOG_WARNING(Service_NS, "Application program not found for id={:016X}", application_id);
        R_RETURN(ResultUnknown);
    }

    const auto logo_dir = program->GetLogoPartition();
    if (!logo_dir) {
        LOG_WARNING(Service_NS, "Logo partition not found for id={:016X}", application_id);
        R_RETURN(ResultUnknown);
    }

    const auto file = logo_dir->GetFile(path);
    if (!file) {
        LOG_WARNING(Service_NS, "Logo path not found: {} for id={:016X}", path,
                    application_id);
        R_RETURN(ResultUnknown);
    }

    const auto data = file->ReadAllBytes();
    if (data.size() > out_buffer.size()) {
        LOG_WARNING(Service_NS, "Logo buffer too small: have={}, need={}", out_buffer.size(),
                    data.size());
        R_RETURN(ResultUnknown);
    }

    std::memcpy(out_buffer.data(), data.data(), data.size());
    *out_size = static_cast<s64>(data.size());

    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationControlData(
    OutBuffer<BufferAttr_HipcMapAlias> out_buffer, Out<u32> out_actual_size,
    ApplicationControlSource application_control_source, u64 application_id) {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(IReadOnlyApplicationControlDataInterface(system).GetApplicationControlData(
        out_buffer, out_actual_size, application_control_source, application_id));
}

Result IApplicationManagerInterface::GetApplicationDesiredLanguage(
    Out<ApplicationLanguage> out_desired_language, u32 supported_languages) {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(IReadOnlyApplicationControlDataInterface(system).GetApplicationDesiredLanguage(
        out_desired_language, supported_languages));
}

Result IApplicationManagerInterface::ConvertApplicationLanguageToLanguageCode(
    Out<u64> out_language_code, ApplicationLanguage application_language) {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(
        IReadOnlyApplicationControlDataInterface(system).ConvertApplicationLanguageToLanguageCode(
            out_language_code, application_language));
}

Result IApplicationManagerInterface::ListApplicationRecord(
    OutArray<ApplicationRecord, BufferAttr_HipcMapAlias> out_records, Out<s32> out_count,
    s32 offset) {
    const auto limit = out_records.size();

    LOG_DEBUG(Service_NS, "called");
    const auto& cache = system.GetContentProviderUnion();
    const auto installed_games = cache.ListEntriesFilterOrigin(
        std::nullopt, FileSys::TitleType::Application, FileSys::ContentRecordType::Program);

    std::vector<ApplicationRecord> records;
    records.reserve(installed_games.size());

    for (const auto& [slot, game] : installed_games) {
         if (game.title_id == 0 || game.title_id < 0x0100000000001FFFull) {
             continue;
         }
         if ((game.title_id & 0xFFF) != 0) {
             continue; // skip sub-programs (e.g., 001)
         }

         ApplicationRecord record{};
         record.application_id = game.title_id;
         record.last_event = ApplicationEvent::Installed;
         record.attributes = 0;
         record.last_updated = Core::LaunchTimestampCache::GetLaunchTimestamp(game.title_id);

         records.push_back(record);
     }

     std::sort(records.begin(), records.end(), [](const ApplicationRecord& lhs, const ApplicationRecord& rhs) {
         if (lhs.last_updated == rhs.last_updated) {
             return lhs.application_id < rhs.application_id;
         }
         return lhs.last_updated > rhs.last_updated;
     });

    size_t i = 0;
    const size_t start = static_cast<size_t>(std::max(0, offset));
    for (size_t idx = start; idx < records.size() && i < limit; ++idx) {
        out_records[i++] = records[idx];
    }

    *out_count = static_cast<s32>(i);
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationRecordUpdateSystemEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");

    record_update_system_event.Signal();
    *out_event = record_update_system_event.GetHandle();

    R_SUCCEED();
}

Result IApplicationManagerInterface::GetGameCardMountFailureEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_event = gamecard_mount_failure_event.GetHandle();
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetGameCardWakenReadyEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_event = gamecard_waken_ready_event.GetHandle();
    R_SUCCEED();
}

Result IApplicationManagerInterface::IsGameCardApplicationRunning(Out<bool> out_is_running) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_is_running = false;
    R_SUCCEED();
}

Result IApplicationManagerInterface::IsAnyApplicationEntityInstalled(
    Out<bool> out_is_any_application_entity_installed) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_is_any_application_entity_installed = true;
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationViewDeprecated(
    OutArray<ApplicationViewV19, BufferAttr_HipcMapAlias> out_application_views,
    InArray<u64, BufferAttr_HipcMapAlias> application_ids) {
    const auto size = (std::min)(out_application_views.size(), application_ids.size());
    LOG_WARNING(Service_NS, "(STUBBED) called, size={}", application_ids.size());

    for (size_t i = 0; i < size; i++) {
        ApplicationViewV19 view{};
        view.application_id = application_ids[i];
        view.version = 0x70000;
        view.flags = 0x401f17;

        out_application_views[i] = view;
    }

    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationViewWithPromotionInfo(
    OutBuffer<BufferAttr_HipcMapAlias> out_buffer,
    Out<u32> out_count,
    InArray<u64, BufferAttr_HipcMapAlias> application_ids) {
    const auto requested = application_ids.size();
    LOG_WARNING(Service_NS, "called, size={}", requested);

    const auto fw_pair = FirmwareManager::GetFirmwareVersion(system);
    const bool is_fw20 = fw_pair.first.major >= 20;

    const size_t per_entry_size = is_fw20 ? (sizeof(ApplicationViewV20) + sizeof(PromotionInfo))
                                             : (sizeof(ApplicationViewV19) + sizeof(PromotionInfo));
    const size_t capacity_entries = out_buffer.size() / per_entry_size;
    const size_t to_write_entries = (std::min)(requested, capacity_entries);

    u8* dst = out_buffer.data();
    for (size_t i = 0; i < to_write_entries; ++i) {
        ApplicationViewWithPromotionData data{};
        data.view.application_id = application_ids[i];
        data.view.version = 0x70000;
        data.view.unk = 0;
        data.view.flags = 0x401f17;
        data.view.download_state = {};
        data.view.download_progress = {};
        data.promotion = {};

        const size_t written = WriteApplicationViewWithPromotion(dst, out_buffer.size() - (dst - out_buffer.data()), data, is_fw20);
        if (written == 0) {
            break;
        }
        dst += written;
    }

    *out_count = static_cast<u32>(dst - out_buffer.data()) / static_cast<u32>(per_entry_size);
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationView(
    OutArray<ApplicationViewV20, BufferAttr_HipcMapAlias> out_application_views,
    InArray<u64, BufferAttr_HipcMapAlias> application_ids) {
    const auto size = (std::min)(out_application_views.size(), application_ids.size());
    LOG_WARNING(Service_NS, "(STUBBED) called, size={}", application_ids.size());

    for (size_t i = 0; i < size; i++) {
        ApplicationViewV20 view{};
        view.application_id = application_ids[i];
        view.version = 0x70000;
        view.unk = 0;
        view.flags = 0x401f17;

        out_application_views[i] = view;
    }

    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationRightsOnClient(
    OutArray<ApplicationRightsOnClient, BufferAttr_HipcMapAlias> out_rights, Out<u32> out_count,
    u32 flags, u64 application_id, Uid account_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called, flags={}, application_id={:016X}, account_id={}",
                flags, application_id, account_id.uuid.FormattedString());

    if (!out_rights.empty()) {
        ApplicationRightsOnClient rights{};
        rights.application_id = application_id;
        rights.uid = account_id.uuid;
        rights.flags = 0;
        rights.flags2 = 0;

        out_rights[0] = rights;
        *out_count = 1;
    } else {
        *out_count = 0;
    }

    R_SUCCEED();
}

Result IApplicationManagerInterface::CheckSdCardMountStatus() {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(IContentManagementInterface(system).CheckSdCardMountStatus());
}

Result IApplicationManagerInterface::GetSdCardMountStatusChangedEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_event = sd_card_mount_status_event.GetHandle();
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetTotalSpaceSize(Out<s64> out_total_space_size, FileSys::StorageId storage_id) {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(IContentManagementInterface(system).GetTotalSpaceSize(out_total_space_size, storage_id));
}

Result IApplicationManagerInterface::GetFreeSpaceSize(Out<s64> out_free_space_size, FileSys::StorageId storage_id) {
    LOG_DEBUG(Service_NS, "called");
    R_RETURN(IContentManagementInterface(system).GetFreeSpaceSize(out_free_space_size, storage_id));
}

Result IApplicationManagerInterface::GetGameCardUpdateDetectionEvent(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    *out_event = gamecard_update_detection_event.GetHandle();
    R_SUCCEED();
}

Result IApplicationManagerInterface::ResumeAll() {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    R_SUCCEED();
}

Result IApplicationManagerInterface::IsQualificationTransitionSupportedByProcessId(
    Out<bool> out_is_supported, u64 process_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called, process_id={}", process_id);
    *out_is_supported = true;
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetStorageSize(Out<s64> out_total_space_size,
                                                    Out<s64> out_free_space_size,
                                                    FileSys::StorageId storage_id) {
    LOG_INFO(Service_NS, "called, storage_id={}", storage_id);
    *out_total_space_size = system.GetFileSystemController().GetTotalSpaceSize(storage_id);
    *out_free_space_size = system.GetFileSystemController().GetFreeSpaceSize(storage_id);
    R_SUCCEED();
}

Result IApplicationManagerInterface::TouchApplication(u64 application_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called. application_id={:016X}", application_id);
    R_SUCCEED();
}

Result IApplicationManagerInterface::IsApplicationUpdateRequested(Out<bool> out_update_required,
                                                                  Out<u32> out_update_version,
                                                                  u64 application_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called. application_id={:016X}", application_id);
    *out_update_required = false;
    *out_update_version = 0;
    R_SUCCEED();
}

Result IApplicationManagerInterface::CheckApplicationLaunchVersion(u64 application_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called. application_id={:016X}", application_id);
    R_SUCCEED();
}

Result IApplicationManagerInterface::GetApplicationTerminateResult(Out<Result> out_result,
                                                                   u64 application_id) {
    LOG_WARNING(Service_NS, "(STUBBED) called. application_id={:016X}", application_id);
    *out_result = ResultSuccess;
    R_SUCCEED();
}

Result IApplicationManagerInterface::RequestDownloadApplicationControlDataInBackground(
    u64 control_source, u64 application_id) {
    LOG_INFO(Service_NS, "called, control_source={} app={:016X}",
             control_source, application_id);

    unknown_event.Signal();
    R_SUCCEED();
}

Result IApplicationManagerInterface::Unknown4022(
    OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_NS, "(STUBBED) called");
    unknown_event.Signal();
    *out_event = unknown_event.GetHandle();
    R_SUCCEED();
}

Result IApplicationManagerInterface::Unknown4023(Out<u64> out_result) {
    LOG_WARNING(Service_NS, "(STUBBED) called.");
    *out_result = 0;
    R_SUCCEED();
}

Result IApplicationManagerInterface::Unknown4053() {
    LOG_WARNING(Service_NS, "(STUBBED) called.");
    R_SUCCEED();
}

void IApplicationManagerInterface::ListApplicationTitle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NS, "called");
    IReadOnlyApplicationControlDataInterface(system).ListApplicationTitle(ctx);
}

} // namespace Service::NS
