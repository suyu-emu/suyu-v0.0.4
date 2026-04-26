// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include "common/concepts.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/anti_piracy_manager.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/kip.h"
#include "core/loader/nax.h"
#include "core/loader/nca.h"
#include "core/loader/nro.h"
#include "core/loader/nso.h"
#include "core/loader/nsp.h"
#include "core/loader/xci.h"

namespace Loader {

namespace {

// Anti-piracy friction delays (in milliseconds)
// These make piracy harder by adding inconvenience without blocking legitimate users
constexpr int VERIFIED_GAME_DELAY_MS = 0;           // No delay for verified games
constexpr int NINTENDO_LIBRARY_DELAY_MS = 0;        // No delay for Nintendo Library matches
constexpr int LEGITIMATE_DUMP_DELAY_MS = 500;       // Small delay for legitimate dumps
constexpr int UNKNOWN_GAME_DELAY_MS = 3000;         // 3 second delay for unknown games
constexpr int SUSPICIOUS_GAME_DELAY_MS = 8000;      // 8 second delay for suspicious games
constexpr int INVALID_GAME_DELAY_MS = 15000;        // 15 second delay for invalid/pirated games

// Get friction delay based on validation result
int GetFrictionDelay(Core::ValidationResult result) {
    switch (result) {
        case Core::ValidationResult::Valid:
            return VERIFIED_GAME_DELAY_MS;
        case Core::ValidationResult::ValidNintendoLibrary:
            return NINTENDO_LIBRARY_DELAY_MS;
        case Core::ValidationResult::ValidLegitimateRip:
            return LEGITIMATE_DUMP_DELAY_MS;
        case Core::ValidationResult::Unknown:
        case Core::ValidationResult::NetworkError:
        case Core::ValidationResult::NotAuthenticated:
            return UNKNOWN_GAME_DELAY_MS;
        case Core::ValidationResult::Suspicious:
            return SUSPICIOUS_GAME_DELAY_MS;
        case Core::ValidationResult::Invalid:
            return INVALID_GAME_DELAY_MS;
        default:
            return UNKNOWN_GAME_DELAY_MS;
    }
}

template <Common::DerivedFrom<AppLoader> T>
std::optional<FileType> IdentifyFileLoader(FileSys::VirtualFile file) {
    const auto file_type = T::IdentifyType(file);
    if (file_type != FileType::Error) {
        return file_type;
    }
    return std::nullopt;
}

} // namespace

FileType IdentifyFile(FileSys::VirtualFile file) {
    if (const auto nsp_type = IdentifyFileLoader<AppLoader_NSP>(file)) {
        return *nsp_type;
    } else if (const auto xci_type = IdentifyFileLoader<AppLoader_XCI>(file)) {
        return *xci_type;
    } else if (const auto nro_type = IdentifyFileLoader<AppLoader_NRO>(file)) {
        return *nro_type;
    } else if (const auto nca_type = IdentifyFileLoader<AppLoader_NCA>(file)) {
        return *nca_type;
    } else if (const auto nax_type = IdentifyFileLoader<AppLoader_NAX>(file)) {
        return *nax_type;
    } else if (const auto kip_type = IdentifyFileLoader<AppLoader_KIP>(file)) {
        return *kip_type;
    } else if (const auto nso_type = IdentifyFileLoader<AppLoader_NSO>(file)) {
        return *nso_type;
    } else if (const auto romdir_type =
                   IdentifyFileLoader<AppLoader_DeconstructedRomDirectory>(file)) {
        return *romdir_type;
    } else {
        return FileType::Unknown;
    }
}

FileType GuessFromFilename(const std::string& name) {
    if (name == "main")
        return FileType::DeconstructedRomDirectory;
    if (name == "00")
        return FileType::NCA;

    const std::string extension =
        Common::ToLower(std::string(Common::FS::GetExtensionFromFilename(name)));

    if (extension == "nro")
        return FileType::NRO;
    if (extension == "nso")
        return FileType::NSO;
    if (extension == "nca")
        return FileType::NCA;
    if (extension == "xci")
        return FileType::XCI;
    if (extension == "nsp")
        return FileType::NSP;
    if (extension == "kip")
        return FileType::KIP;

    return FileType::Unknown;
}

std::string GetFileTypeString(FileType type) {
    switch (type) {
    case FileType::NRO:
        return "NRO";
    case FileType::NSO:
        return "NSO";
    case FileType::NCA:
        return "NCA";
    case FileType::XCI:
        return "XCI";
    case FileType::NAX:
        return "NAX";
    case FileType::NSP:
        return "NSP";
    case FileType::KIP:
        return "KIP";
    case FileType::DeconstructedRomDirectory:
        return "Directory";
    case FileType::Error:
    case FileType::Unknown:
        break;
    }

    return "unknown";
}

constexpr std::array<const char*, 68> RESULT_MESSAGES{
    "The operation completed successfully.",
    "The loader requested to load is already loaded.",
    "The operation is not implemented.",
    "The loader is not initialized properly.",
    "The NPDM file has a bad header.",
    "The NPDM has a bad ACID header.",
    "The NPDM has a bad ACI header,",
    "The NPDM file has a bad file access control.",
    "The NPDM has a bad file access header.",
    "The NPDM has bad kernel capability descriptors.",
    "The PFS/HFS partition has a bad header.",
    "The PFS/HFS partition has incorrect size as determined by the header.",
    "The NCA file has a bad header.",
    "The general keyfile could not be found.",
    "The NCA Header key could not be found.",
    "The NCA Header key is incorrect or the header is invalid.",
    "Support for NCA2-type NCAs is not implemented.",
    "Support for NCA0-type NCAs is not implemented.",
    "The titlekey for this Rights ID could not be found.",
    "The titlekek for this crypto revision could not be found.",
    "The Rights ID in the header is invalid.",
    "The key area key for this application type and crypto revision could not be found.",
    "The key area key is incorrect or the section header is invalid.",
    "The titlekey and/or titlekek is incorrect or the section header is invalid.",
    "The XCI file is missing a Program-type NCA.",
    "The NCA file is not an application.",
    "The Program-type NCA contains no executable. An update may be required.",
    "The XCI file has a bad header.",
    "The XCI file is missing a partition.",
    "The file could not be found or does not exist.",
    "The game is missing a program metadata file (main.npdm).",
    "The game uses the currently-unimplemented 32-bit architecture.",
    "Unable to completely parse the kernel metadata when loading the emulated process",
    "The RomFS could not be found.",
    "The ELF file has incorrect size as determined by the header.",
    "There was a general error loading the NRO into emulated memory.",
    "There was a general error loading the NSO into emulated memory.",
    "There is no icon available.",
    "There is no control data available.",
    "The NAX file has a bad header.",
    "The NAX file has incorrect size as determined by the header.",
    "The HMAC to generated the NAX decryption keys failed.",
    "The HMAC to validate the NAX decryption keys failed.",
    "The NAX key derivation failed.",
    "The NAX file cannot be interpreted as an NCA file.",
    "The NAX file has an incorrect path.",
    "The SD seed could not be found or derived.",
    "The SD KEK Source could not be found.",
    "The AES KEK Generation Source could not be found.",
    "The AES Key Generation Source could not be found.",
    "The SD Save Key Source could not be found.",
    "The SD NCA Key Source could not be found.",
    "The NSP file is missing a Program-type NCA.",
    "The BKTR-type NCA has a bad BKTR header.",
    "The BKTR Subsection entry is not located immediately after the Relocation entry.",
    "The BKTR Subsection entry is not at the end of the media block.",
    "The BKTR-type NCA has a bad Relocation block.",
    "The BKTR-type NCA has a bad Subsection block.",
    "The BKTR-type NCA has a bad Relocation bucket.",
    "The BKTR-type NCA has a bad Subsection bucket.",
    "Game updates cannot be loaded directly. Load the base game instead.",
    "The NSP or XCI does not contain an update in addition to the base game.",
    "The KIP file has a bad header.",
    "The KIP BLZ decompression of the section failed unexpectedly.",
    "The INI file has a bad header.",
    "The INI file contains more than the maximum allowable number of KIP files.",
    "Integrity verification could not be performed for this file.",
    "Integrity verification failed.",
};

std::string GetResultStatusString(ResultStatus status) {
    return RESULT_MESSAGES.at(static_cast<std::size_t>(status));
}

std::ostream& operator<<(std::ostream& os, ResultStatus status) {
    os << RESULT_MESSAGES.at(static_cast<std::size_t>(status));
    return os;
}

AppLoader::AppLoader(FileSys::VirtualFile file_) : file(std::move(file_)) {}
AppLoader::~AppLoader() = default;

/**
 * Get a loader for a file with a specific type
 * @param system The system context to use.
 * @param file   The file to retrieve the loader for
 * @param type   The file type
 * @param program_index Specifies the index within the container of the program to launch.
 * @return std::unique_ptr<AppLoader> a pointer to a loader object;  nullptr for unsupported type
 */
static std::unique_ptr<AppLoader> GetFileLoader(Core::System& system, FileSys::VirtualFile file,
                                                FileType type, u64 program_id,
                                                std::size_t program_index) {
    switch (type) {
    // NX NSO file format.
    case FileType::NSO:
        return std::make_unique<AppLoader_NSO>(std::move(file));

    // NX NRO file format.
    case FileType::NRO:
        return std::make_unique<AppLoader_NRO>(std::move(file));

    // NX NCA (Nintendo Content Archive) file format.
    case FileType::NCA:
        return std::make_unique<AppLoader_NCA>(std::move(file));

    // NX XCI (nX Card Image) file format.
    case FileType::XCI:
        return std::make_unique<AppLoader_XCI>(std::move(file), system.GetFileSystemController(),
                                               system.GetContentProvider(), program_id,
                                               program_index);

    // NX NAX (NintendoAesXts) file format.
    case FileType::NAX:
        return std::make_unique<AppLoader_NAX>(std::move(file));

    // NX NSP (Nintendo Submission Package) file format
    case FileType::NSP:
        return std::make_unique<AppLoader_NSP>(std::move(file), system.GetFileSystemController(),
                                               system.GetContentProvider(), program_id,
                                               program_index);

    // NX KIP (Kernel Internal Process) file format
    case FileType::KIP:
        return std::make_unique<AppLoader_KIP>(std::move(file));

    // NX deconstructed ROM directory.
    case FileType::DeconstructedRomDirectory:
        return std::make_unique<AppLoader_DeconstructedRomDirectory>(std::move(file));

    default:
        return nullptr;
    }
}

std::unique_ptr<AppLoader> GetLoader(Core::System& system, FileSys::VirtualFile file,
                                     u64 program_id, std::size_t program_index) {
    if (!file) {
        return nullptr;
    }

    FileType type = IdentifyFile(file);
    const FileType filename_type = GuessFromFilename(file->GetName());

    // Special case: 00 is either a NCA or NAX.
    if (type != filename_type && !(file->GetName() == "00" && type == FileType::NAX)) {
        LOG_WARNING(Loader, "File {} has a different type ({}) than its extension.",
                    file->GetName(), GetFileTypeString(type));
        if (FileType::Unknown == type) {
            type = filename_type;
        }
    }

    LOG_DEBUG(Loader, "Loading file {} as {}...", file->GetName(), GetFileTypeString(type));

    // Perform anti-piracy validation if available
    if (system.GetAntiPiracyManager()) {
        auto& anti_piracy = *system.GetAntiPiracyManager();
        if (anti_piracy.IsInitialized()) {
            LOG_INFO(Loader, "Performing anti-piracy validation for: {}", file->GetName());

            auto validation_result = anti_piracy.ValidateRom(file);
            auto validation_message = anti_piracy.GetValidationMessage(validation_result);

            LOG_INFO(Loader, "Anti-piracy validation result: {}", validation_message);

            // Apply friction delay based on validation result
            // This makes piracy harder by adding inconvenience without blocking legitimate users
            int delay_ms = GetFrictionDelay(validation_result);
            if (delay_ms > 0) {
                LOG_INFO(Loader, "Applying anti-piracy friction delay of {} ms", delay_ms);
                LOG_INFO(Loader, "To reduce or eliminate this delay, verify game ownership through:");
                LOG_INFO(Loader, "  - Nintendo Account linking (Settings > Nintendo Library)");
                LOG_INFO(Loader, "  - Using legitimate dump tools like NXDumpTool");

                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }

            // Show educational message for suspicious or invalid ROMs
            if (validation_result == Core::ValidationResult::Suspicious ||
                validation_result == Core::ValidationResult::Invalid) {
                LOG_WARNING(Loader, "ROM validation concerns detected.");
                LOG_INFO(Loader, "Educational message: {}", anti_piracy.GetEducationalMessage());

                // Log legitimate source suggestions
                auto suggestions = anti_piracy.GetLegitimateSourceSuggestions();
                LOG_INFO(Loader, "Consider these legitimate sources:");
                for (const auto& suggestion : suggestions) {
                    LOG_INFO(Loader, "  - {}", suggestion);
                }
            }

            // Note: We don't block loading even for suspicious ROMs, as this is educational
            // and the emulator should remain functional for legitimate use cases
        }
    }

    return GetFileLoader(system, std::move(file), type, program_id, program_index);
}

} // namespace Loader
