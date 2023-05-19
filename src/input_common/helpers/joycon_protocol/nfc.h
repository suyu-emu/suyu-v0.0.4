// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on dkms-hid-nintendo implementation, CTCaer joycon toolkit and dekuNukem reverse
// engineering https://github.com/nicman23/dkms-hid-nintendo/blob/master/src/hid-nintendo.c
// https://github.com/CTCaer/jc_toolkit
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering

#pragma once

#include <vector>

#include "input_common/helpers/joycon_protocol/common_protocol.h"
#include "input_common/helpers/joycon_protocol/joycon_types.h"

namespace InputCommon::Joycon {

class NfcProtocol final : private JoyconCommonProtocol {
public:
    explicit NfcProtocol(std::shared_ptr<JoyconHandle> handle);

    DriverResult EnableNfc();

    DriverResult DisableNfc();

    DriverResult StartNFCPollingMode();

    DriverResult ScanAmiibo(std::vector<u8>& data);

    bool HasAmiibo();

    bool IsEnabled() const;

private:
    // Number of times the function will be delayed until it outputs valid data
    static constexpr std::size_t AMIIBO_UPDATE_DELAY = 15;

    struct TagFoundData {
        u8 type;
        std::vector<u8> uuid;
    };

    DriverResult WaitUntilNfcIsReady();

    DriverResult WaitUntilNfcIsPolling();

    DriverResult IsTagInRange(TagFoundData& data, std::size_t timeout_limit = 1);

    DriverResult GetAmiiboData(std::vector<u8>& data);

    DriverResult SendStartPollingRequest(MCUCommandResponse& output);

    DriverResult SendStopPollingRequest(MCUCommandResponse& output);

    DriverResult SendNextPackageRequest(MCUCommandResponse& output, u8 packet_id);

    DriverResult SendReadAmiiboRequest(MCUCommandResponse& output, NFCPages ntag_pages);

    NFCReadBlockCommand GetReadBlockCommand(NFCPages pages) const;

    bool is_enabled{};
    std::size_t update_counter{};
};

} // namespace InputCommon::Joycon
