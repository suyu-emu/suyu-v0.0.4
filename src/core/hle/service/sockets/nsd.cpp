// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/sockets/nsd.h"

namespace Service::Sockets {

NSD::NSD(const char* name) : ServiceFramework(name) {
    static const FunctionInfo functions[] = {
        {10, nullptr, "GetSettingName"},
        {11, nullptr, "GetEnvironmentIdentifier"},
        {12, nullptr, "GetDeviceId"},
        {13, nullptr, "DeleteSettings"},
        {14, nullptr, "ImportSettings"},
        {20, nullptr, "Resolve"},
        {21, nullptr, "ResolveEx"},
        {30, nullptr, "GetNasServiceSetting"},
        {31, nullptr, "GetNasServiceSettingEx"},
        {40, nullptr, "GetNasRequestFqdn"},
        {41, nullptr, "GetNasRequestFqdnEx"},
        {42, nullptr, "GetNasApiFqdn"},
        {43, nullptr, "GetNasApiFqdnEx"},
        {50, nullptr, "GetCurrentSetting"},
        {60, nullptr, "ReadSaveDataFromFsForTest"},
        {61, nullptr, "WriteSaveDataToFsForTest"},
        {62, nullptr, "DeleteSaveDataOfFsForTest"},
    };
    RegisterHandlers(functions);
}

NSD::~NSD() = default;

} // namespace Service::Sockets
