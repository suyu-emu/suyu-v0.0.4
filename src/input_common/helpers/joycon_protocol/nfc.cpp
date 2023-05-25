// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include "common/logging/log.h"
#include "input_common/helpers/joycon_protocol/nfc.h"

namespace InputCommon::Joycon {

NfcProtocol::NfcProtocol(std::shared_ptr<JoyconHandle> handle)
    : JoyconCommonProtocol(std::move(handle)) {}

DriverResult NfcProtocol::EnableNfc() {
    LOG_INFO(Input, "Enable NFC");
    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};

    if (result == DriverResult::Success) {
        result = SetReportMode(ReportMode::NFC_IR_MODE_60HZ);
    }
    if (result == DriverResult::Success) {
        result = EnableMCU(true);
    }
    if (result == DriverResult::Success) {
        result = WaitSetMCUMode(ReportMode::NFC_IR_MODE_60HZ, MCUMode::Standby);
    }
    if (result == DriverResult::Success) {
        const MCUConfig config{
            .command = MCUCommand::ConfigureMCU,
            .sub_command = MCUSubCommand::SetMCUMode,
            .mode = MCUMode::NFC,
            .crc = {},
        };

        result = ConfigureMCU(config);
    }
    if (result == DriverResult::Success) {
        result = WaitSetMCUMode(ReportMode::NFC_IR_MODE_60HZ, MCUMode::NFC);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::Ready);
    }

    return result;
}

DriverResult NfcProtocol::DisableNfc() {
    LOG_DEBUG(Input, "Disable NFC");
    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};

    if (result == DriverResult::Success) {
        result = EnableMCU(false);
    }

    is_enabled = false;

    return result;
}

DriverResult NfcProtocol::StartNFCPollingMode() {
    LOG_DEBUG(Input, "Start NFC pooling Mode");
    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};

    if (result == DriverResult::Success) {
        MCUCommandResponse output{};
        result = SendStopPollingRequest(output);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::Ready);
    }
    if (result == DriverResult::Success) {
        MCUCommandResponse output{};
        result = SendStartPollingRequest(output);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::Polling);
    }
    if (result == DriverResult::Success) {
        is_enabled = true;
    }

    return result;
}

DriverResult NfcProtocol::ScanAmiibo(std::vector<u8>& data) {
    if (update_counter++ < AMIIBO_UPDATE_DELAY) {
        return DriverResult::Delayed;
    }
    update_counter = 0;

    LOG_DEBUG(Input, "Scan for amiibos");
    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};
    TagFoundData tag_data{};

    if (result == DriverResult::Success) {
        result = IsTagInRange(tag_data);
    }

    if (result == DriverResult::Success) {
        std::string uuid_string;
        for (auto& content : tag_data.uuid) {
            uuid_string += fmt::format(" {:02x}", content);
        }
        LOG_INFO(Input, "Tag detected, type={}, uuid={}", tag_data.type, uuid_string);
        result = GetAmiiboData(data);
    }

    return result;
}

DriverResult NfcProtocol::WriteAmiibo(std::span<const u8> data) {
    LOG_DEBUG(Input, "Write amiibo");
    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};
    TagUUID tag_uuid = GetTagUUID(data);
    TagFoundData tag_data{};

    if (result == DriverResult::Success) {
        result = IsTagInRange(tag_data, 7);
    }
    if (result == DriverResult::Success) {
        if (tag_data.uuid != tag_uuid) {
            result = DriverResult::InvalidParameters;
        }
    }
    if (result == DriverResult::Success) {
        MCUCommandResponse output{};
        result = SendStopPollingRequest(output);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::Ready);
    }
    if (result == DriverResult::Success) {
        MCUCommandResponse output{};
        result = SendStartPollingRequest(output, true);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::WriteReady);
    }
    if (result == DriverResult::Success) {
        result = WriteAmiiboData(tag_uuid, data);
    }
    if (result == DriverResult::Success) {
        result = WaitUntilNfcIs(NFCStatus::WriteDone);
    }
    if (result == DriverResult::Success) {
        MCUCommandResponse output{};
        result = SendStopPollingRequest(output);
    }

    return result;
}

bool NfcProtocol::HasAmiibo() {
    if (update_counter++ < AMIIBO_UPDATE_DELAY) {
        return true;
    }
    update_counter = 0;

    ScopedSetBlocking sb(this);
    DriverResult result{DriverResult::Success};
    TagFoundData tag_data{};

    if (result == DriverResult::Success) {
        result = IsTagInRange(tag_data, 7);
    }

    return result == DriverResult::Success;
}

DriverResult NfcProtocol::WaitUntilNfcIs(NFCStatus status) {
    constexpr std::size_t timeout_limit = 10;
    MCUCommandResponse output{};
    std::size_t tries = 0;

    do {
        auto result = SendNextPackageRequest(output, {});

        if (result != DriverResult::Success) {
            return result;
        }
        if (tries++ > timeout_limit) {
            return DriverResult::Timeout;
        }
    } while (output.mcu_report != MCUReport::NFCState ||
             (output.mcu_data[1] << 8) + output.mcu_data[0] != 0x0500 ||
             output.mcu_data[5] != 0x31 || output.mcu_data[6] != static_cast<u8>(status));

    return DriverResult::Success;
}

DriverResult NfcProtocol::IsTagInRange(TagFoundData& data, std::size_t timeout_limit) {
    MCUCommandResponse output{};
    std::size_t tries = 0;

    do {
        const auto result = SendNextPackageRequest(output, {});
        if (result != DriverResult::Success) {
            return result;
        }
        if (tries++ > timeout_limit) {
            return DriverResult::Timeout;
        }
    } while (output.mcu_report != MCUReport::NFCState ||
             (output.mcu_data[1] << 8) + output.mcu_data[0] != 0x0500 ||
             (output.mcu_data[6] != 0x09 && output.mcu_data[6] != 0x04));

    data.type = output.mcu_data[12];
    data.uuid_size = std::min(output.mcu_data[14], static_cast<u8>(sizeof(TagUUID)));
    memcpy(data.uuid.data(), output.mcu_data.data() + 15, data.uuid.size());

    return DriverResult::Success;
}

DriverResult NfcProtocol::GetAmiiboData(std::vector<u8>& ntag_data) {
    constexpr std::size_t timeout_limit = 60;
    MCUCommandResponse output{};
    std::size_t tries = 0;

    u8 package_index = 0;
    std::size_t ntag_buffer_pos = 0;
    auto result = SendReadAmiiboRequest(output, NFCPages::Block135);

    if (result != DriverResult::Success) {
        return result;
    }

    // Read Tag data
    while (tries++ < timeout_limit) {
        result = SendNextPackageRequest(output, package_index);
        const auto nfc_status = static_cast<NFCStatus>(output.mcu_data[6]);

        if (result != DriverResult::Success) {
            return result;
        }

        if ((output.mcu_report == MCUReport::NFCReadData ||
             output.mcu_report == MCUReport::NFCState) &&
            nfc_status == NFCStatus::TagLost) {
            return DriverResult::ErrorReadingData;
        }

        if (output.mcu_report == MCUReport::NFCReadData && output.mcu_data[1] == 0x07) {
            std::size_t payload_size = (output.mcu_data[4] << 8 | output.mcu_data[5]) & 0x7FF;
            if (output.mcu_data[2] == 0x01) {
                memcpy(ntag_data.data() + ntag_buffer_pos, output.mcu_data.data() + 66,
                       payload_size - 60);
                ntag_buffer_pos += payload_size - 60;
            } else {
                memcpy(ntag_data.data() + ntag_buffer_pos, output.mcu_data.data() + 6,
                       payload_size);
            }
            package_index++;
            continue;
        }

        if (output.mcu_report == MCUReport::NFCState && nfc_status == NFCStatus::LastPackage) {
            LOG_INFO(Input, "Finished reading amiibo");
            return DriverResult::Success;
        }
    }

    return DriverResult::Timeout;
}

DriverResult NfcProtocol::WriteAmiiboData(const TagUUID& tag_uuid, std::span<const u8> data) {
    constexpr std::size_t timeout_limit = 60;
    const auto nfc_data = MakeAmiiboWritePackage(tag_uuid, data);
    const std::vector<u8> nfc_buffer_data = SerializeWritePackage(nfc_data);
    std::span<const u8> buffer(nfc_buffer_data);
    MCUCommandResponse output{};
    u8 block_id = 1;
    u8 package_index = 0;
    std::size_t tries = 0;
    std::size_t current_position = 0;

    LOG_INFO(Input, "Writing amiibo data");

    auto result = SendWriteAmiiboRequest(output, tag_uuid);

    if (result != DriverResult::Success) {
        return result;
    }

    // Read Tag data but ignore the actual sent data
    while (tries++ < timeout_limit) {
        result = SendNextPackageRequest(output, package_index);
        const auto nfc_status = static_cast<NFCStatus>(output.mcu_data[6]);

        if (result != DriverResult::Success) {
            return result;
        }

        if ((output.mcu_report == MCUReport::NFCReadData ||
             output.mcu_report == MCUReport::NFCState) &&
            nfc_status == NFCStatus::TagLost) {
            return DriverResult::ErrorReadingData;
        }

        if (output.mcu_report == MCUReport::NFCReadData && output.mcu_data[1] == 0x07) {
            package_index++;
            continue;
        }

        if (output.mcu_report == MCUReport::NFCState && nfc_status == NFCStatus::LastPackage) {
            LOG_INFO(Input, "Finished reading amiibo");
            break;
        }
    }

    // Send Data. Nfc buffer size is 31, Send the data in smaller packages
    while (current_position < buffer.size() && tries++ < timeout_limit) {
        const std::size_t next_position =
            std::min(current_position + sizeof(NFCRequestState::raw_data), buffer.size());
        const std::size_t block_size = next_position - current_position;
        const bool is_last_packet = block_size < sizeof(NFCRequestState::raw_data);

        SendWriteDataAmiiboRequest(output, block_id, is_last_packet,
                                   buffer.subspan(current_position, block_size));

        const auto nfc_status = static_cast<NFCStatus>(output.mcu_data[6]);

        if ((output.mcu_report == MCUReport::NFCReadData ||
             output.mcu_report == MCUReport::NFCState) &&
            nfc_status == NFCStatus::TagLost) {
            return DriverResult::ErrorReadingData;
        }

        // Increase position when data is confirmed by the joycon
        if (output.mcu_report == MCUReport::NFCState &&
            (output.mcu_data[1] << 8) + output.mcu_data[0] == 0x0500 &&
            output.mcu_data[3] == block_id) {
            block_id++;
            current_position = next_position;
        }
    }

    return result;
}

DriverResult NfcProtocol::SendStartPollingRequest(MCUCommandResponse& output,
                                                  bool is_second_attempt) {
    NFCRequestState request{
        .command_argument = NFCCommand::StartPolling,
        .block_id = {},
        .packet_id = {},
        .packet_flag = MCUPacketFlag::LastCommandPacket,
        .data_length = sizeof(NFCPollingCommandData),
        .nfc_polling =
            {
                .enable_mifare = 0x00,
                .unknown_1 = static_cast<u8>(is_second_attempt ? 0xe8 : 0x00),
                .unknown_2 = static_cast<u8>(is_second_attempt ? 0x03 : 0x00),
                .unknown_3 = 0x2c,
                .unknown_4 = 0x01,
            },
        .crc = {},
    };

    std::array<u8, sizeof(NFCRequestState)> request_data{};
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

DriverResult NfcProtocol::SendStopPollingRequest(MCUCommandResponse& output) {
    NFCRequestState request{
        .command_argument = NFCCommand::StopPolling,
        .block_id = {},
        .packet_id = {},
        .packet_flag = MCUPacketFlag::LastCommandPacket,
        .data_length = {},
        .raw_data = {},
        .crc = {},
    };

    std::array<u8, sizeof(NFCRequestState)> request_data{};
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

DriverResult NfcProtocol::SendNextPackageRequest(MCUCommandResponse& output, u8 packet_id) {
    NFCRequestState request{
        .command_argument = NFCCommand::StartWaitingRecieve,
        .block_id = {},
        .packet_id = packet_id,
        .packet_flag = MCUPacketFlag::LastCommandPacket,
        .data_length = {},
        .raw_data = {},
        .crc = {},
    };

    std::vector<u8> request_data(sizeof(NFCRequestState));
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

DriverResult NfcProtocol::SendReadAmiiboRequest(MCUCommandResponse& output, NFCPages ntag_pages) {
    NFCRequestState request{
        .command_argument = NFCCommand::ReadNtag,
        .block_id = {},
        .packet_id = {},
        .packet_flag = MCUPacketFlag::LastCommandPacket,
        .data_length = sizeof(NFCReadCommandData),
        .nfc_read =
            {
                .unknown = 0xd0,
                .uuid_length = sizeof(NFCReadCommandData::uid),
                .uid = {},
                .tag_type = NFCTagType::Ntag215,
                .read_block = GetReadBlockCommand(ntag_pages),
            },
        .crc = {},
    };

    std::array<u8, sizeof(NFCRequestState)> request_data{};
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

DriverResult NfcProtocol::SendWriteAmiiboRequest(MCUCommandResponse& output,
                                                 const TagUUID& tag_uuid) {
    NFCRequestState request{
        .command_argument = NFCCommand::ReadNtag,
        .block_id = {},
        .packet_id = {},
        .packet_flag = MCUPacketFlag::LastCommandPacket,
        .data_length = sizeof(NFCReadCommandData),
        .nfc_read =
            {
                .unknown = 0xd0,
                .uuid_length = sizeof(NFCReadCommandData::uid),
                .uid = tag_uuid,
                .tag_type = NFCTagType::Ntag215,
                .read_block = GetReadBlockCommand(NFCPages::Block3),
            },
        .crc = {},
    };

    std::array<u8, sizeof(NFCRequestState)> request_data{};
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

DriverResult NfcProtocol::SendWriteDataAmiiboRequest(MCUCommandResponse& output, u8 block_id,
                                                     bool is_last_packet,
                                                     std::span<const u8> data) {
    const auto data_size = std::min(data.size(), sizeof(NFCRequestState::raw_data));
    NFCRequestState request{
        .command_argument = NFCCommand::WriteNtag,
        .block_id = block_id,
        .packet_id = {},
        .packet_flag =
            is_last_packet ? MCUPacketFlag::LastCommandPacket : MCUPacketFlag::MorePacketsRemaining,
        .data_length = static_cast<u8>(data_size),
        .raw_data = {},
        .crc = {},
    };
    memcpy(request.raw_data.data(), data.data(), data_size);

    std::array<u8, sizeof(NFCRequestState)> request_data{};
    memcpy(request_data.data(), &request, sizeof(NFCRequestState));
    request_data[36] = CalculateMCU_CRC8(request_data.data(), 36);
    return SendMCUData(ReportMode::NFC_IR_MODE_60HZ, MCUSubCommand::ReadDeviceMode, request_data,
                       output);
}

std::vector<u8> NfcProtocol::SerializeWritePackage(const NFCWritePackage& package) const {
    const std::size_t header_size =
        sizeof(NFCWriteCommandData) + sizeof(NFCWritePackage::number_of_chunks);
    std::vector<u8> serialized_data(header_size);
    std::size_t start_index = 0;

    memcpy(serialized_data.data(), &package, header_size);
    start_index += header_size;

    for (const auto& data_chunk : package.data_chunks) {
        const std::size_t chunk_size =
            sizeof(NFCDataChunk::nfc_page) + sizeof(NFCDataChunk::data_size) + data_chunk.data_size;

        serialized_data.resize(start_index + chunk_size);
        memcpy(serialized_data.data() + start_index, &data_chunk, chunk_size);
        start_index += chunk_size;
    }

    return serialized_data;
}

NFCWritePackage NfcProtocol::MakeAmiiboWritePackage(const TagUUID& tag_uuid,
                                                    std::span<const u8> data) const {
    return {
        .command_data{
            .unknown = 0xd0,
            .uuid_length = sizeof(NFCReadCommandData::uid),
            .uid = tag_uuid,
            .tag_type = NFCTagType::Ntag215,
            .unknown2 = 0x00,
            .unknown3 = 0x01,
            .unknown4 = 0x04,
            .unknown5 = 0xff,
            .unknown6 = 0xff,
            .unknown7 = 0xff,
            .unknown8 = 0xff,
            .magic = data[16],
            .write_count = static_cast<u16>((data[17] << 8) + data[18]),
            .amiibo_version = data[19],
        },
        .number_of_chunks = 3,
        .data_chunks =
            {
                MakeAmiiboChunk(0x05, 0x20, data),
                MakeAmiiboChunk(0x20, 0xf0, data),
                MakeAmiiboChunk(0x5c, 0x98, data),
            },
    };
}

NFCDataChunk NfcProtocol::MakeAmiiboChunk(u8 page, u8 size, std::span<const u8> data) const {
    constexpr u8 PAGE_SIZE = 4;

    if (static_cast<std::size_t>(page * PAGE_SIZE) + size >= data.size()) {
        return {};
    }

    NFCDataChunk chunk{
        .nfc_page = page,
        .data_size = size,
        .data = {},
    };
    std::memcpy(chunk.data.data(), data.data() + (page * PAGE_SIZE), size);
    return chunk;
}

NFCReadBlockCommand NfcProtocol::GetReadBlockCommand(NFCPages pages) const {
    switch (pages) {
    case NFCPages::Block0:
        return {
            .block_count = 1,
        };
    case NFCPages::Block3:
        return {
            .block_count = 1,
            .blocks =
                {
                    NFCReadBlock{0x03, 0x03},
                },
        };
    case NFCPages::Block45:
        return {
            .block_count = 1,
            .blocks =
                {
                    NFCReadBlock{0x00, 0x2C},
                },
        };
    case NFCPages::Block135:
        return {
            .block_count = 3,
            .blocks =
                {
                    NFCReadBlock{0x00, 0x3b},
                    {0x3c, 0x77},
                    {0x78, 0x86},
                },
        };
    case NFCPages::Block231:
        return {
            .block_count = 4,
            .blocks =
                {
                    NFCReadBlock{0x00, 0x3b},
                    {0x3c, 0x77},
                    {0x78, 0x83},
                    {0xb4, 0xe6},
                },
        };
    default:
        return {};
    };
}

TagUUID NfcProtocol::GetTagUUID(std::span<const u8> data) const {
    if (data.size() < 10) {
        return {};
    }

    // crc byte 3 is omitted in this operation
    return {
        data[0], data[1], data[2], data[4], data[5], data[6], data[7],
    };
}

bool NfcProtocol::IsEnabled() const {
    return is_enabled;
}

} // namespace InputCommon::Joycon
