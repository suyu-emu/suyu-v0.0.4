// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "anti_piracy_manager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

#include "common/fs/file.h"
#include "common/fs/path_util.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/loader/loader.h"
#include "nintendo_library/nintendo_library.h"

namespace Core {

// Implementation class using PIMPL pattern
class AntiPiracyManager::Impl {
public:
    explicit Impl(Core::System& system_) 
        : system(system_), nintendo_library(std::make_unique<Nintendo::Library>()) {}

    Core::System& system;
    std::unique_ptr<Nintendo::Library> nintendo_library;
    ValidationConfig config;
    ValidationStats stats;
    
    bool initialized = false;
    bool nintendo_authenticated = false;
    
    // Validation cache: file_hash -> ValidationResult
    std::unordered_map<std::string, ValidationResult> validation_cache;
    
    // Nintendo game library cache
    std::vector<Nintendo::GameInfo> nintendo_games;
    std::chrono::steady_clock::time_point last_library_refresh;
    
    // Known legitimate dump tool signatures
    std::unordered_set<std::string> legitimate_dump_signatures = {
        "NXDumpTool",
        "nxdumptool",
        "Lockpick_RCM",
        "TegraExplorer",
        "SX Dumper",
        "Goldleaf"
    };
    
    // Educational messages
    const std::string educational_message = 
        "suyu supports legitimate game ownership. Please ensure you own the games you're playing.\n"
        "You can verify your game library by linking your Nintendo account in the settings.\n"
        "For more information about legitimate game dumping, visit our documentation.";
};

AntiPiracyManager::AntiPiracyManager(Core::System& system) 
    : impl(std::make_unique<Impl>(system)) {}

AntiPiracyManager::~AntiPiracyManager() {
    if (impl->initialized) {
        Shutdown();
    }
}

bool AntiPiracyManager::Initialize() {
    if (impl->initialized) {
        return true;
    }

    LOG_INFO(Core, "Initializing Anti-Piracy Manager");

    // Initialize Nintendo Library
    if (!impl->nintendo_library->Initialize()) {
        LOG_WARNING(Core, "Failed to initialize Nintendo Library - network features may be limited");
    }

    // Load configuration
    LoadConfigFromFile("anti_piracy_config.json");
    
    // Load validation cache
    LoadValidationCache();

    impl->initialized = true;
    LOG_INFO(Core, "Anti-Piracy Manager initialized successfully");
    return true;
}

void AntiPiracyManager::Shutdown() {
    if (!impl->initialized) {
        return;
    }

    LOG_INFO(Core, "Shutting down Anti-Piracy Manager");

    // Save validation cache
    SaveValidationCache();
    
    // Save configuration
    SaveConfigToFile("anti_piracy_config.json");

    // Shutdown Nintendo Library
    impl->nintendo_library->Shutdown();

    impl->initialized = false;
    LOG_INFO(Core, "Anti-Piracy Manager shut down");
}

bool AntiPiracyManager::IsInitialized() const {
    return impl->initialized;
}

void AntiPiracyManager::SetValidationConfig(const ValidationConfig& config) {
    impl->config = config;
    LOG_INFO(Core, "Anti-piracy validation config updated");
}

ValidationConfig AntiPiracyManager::GetValidationConfig() const {
    return impl->config;
}

bool AntiPiracyManager::AuthenticateWithNintendo(const std::string& username, const std::string& password) {
    if (!impl->initialized) {
        LOG_ERROR(Core, "Anti-Piracy Manager not initialized");
        return false;
    }

    LOG_INFO(Core, "Attempting Nintendo account authentication");
    
    bool success = impl->nintendo_library->StartAuthentication(username, password);
    if (success) {
        // Wait for authentication to complete (with timeout)
        auto start_time = std::chrono::steady_clock::now();
        while (impl->nintendo_library->IsAuthenticationInProgress()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > std::chrono::seconds(impl->config.validation_timeout_seconds)) {
                LOG_WARNING(Core, "Nintendo authentication timed out");
                return false;
            }
        }
        
        impl->nintendo_authenticated = 
            (impl->nintendo_library->GetAuthenticationState() == Nintendo::AuthenticationState::Authenticated);
        
        if (impl->nintendo_authenticated) {
            LOG_INFO(Core, "Nintendo account authentication successful");
            // Refresh game library
            GetNintendoGameLibrary();
        } else {
            LOG_WARNING(Core, "Nintendo account authentication failed");
        }
    }
    
    return impl->nintendo_authenticated;
}

bool AntiPiracyManager::IsNintendoAuthenticated() const {
    return impl->nintendo_authenticated;
}

void AntiPiracyManager::ClearNintendoAuthentication() {
    impl->nintendo_authenticated = false;
    impl->nintendo_games.clear();
    LOG_INFO(Core, "Nintendo authentication cleared");
}

std::vector<Nintendo::GameInfo> AntiPiracyManager::GetNintendoGameLibrary() {
    if (!impl->nintendo_authenticated) {
        return {};
    }

    // Check if we need to refresh the library (cache for 1 hour)
    auto now = std::chrono::steady_clock::now();
    auto cache_duration = std::chrono::hours(1);
    
    if (impl->nintendo_games.empty() || 
        (now - impl->last_library_refresh) > cache_duration) {
        
        LOG_INFO(Core, "Refreshing Nintendo game library");
        impl->nintendo_games = impl->nintendo_library->GetGameList();
        impl->last_library_refresh = now;
        
        LOG_INFO(Core, "Retrieved {} games from Nintendo library", impl->nintendo_games.size());
    }
    
    return impl->nintendo_games;
}

ValidationResult AntiPiracyManager::ValidateRom(const std::string& file_path) {
    auto file = FileSys::VfsFilesystem::OpenFile(file_path, FileSys::Mode::Read);
    if (!file) {
        LOG_ERROR(Core, "Failed to open ROM file: {}", file_path);
        return ValidationResult::Invalid;
    }
    return ValidateRom(file);
}

ValidationResult AntiPiracyManager::ValidateRom(FileSys::VirtualFile file) {
    if (!impl->initialized) {
        LOG_WARNING(Core, "Anti-Piracy Manager not initialized, skipping validation");
        return ValidationResult::Unknown;
    }

    impl->stats.total_validations++;

    // Extract ROM metadata
    RomMetadata metadata = ExtractRomMetadata(file);
    
    // Check cache first
    if (impl->config.cache_validation_results && !metadata.file_hash.empty()) {
        auto cached_result = GetCachedResult(metadata.file_hash);
        if (cached_result.has_value()) {
            LOG_DEBUG(Core, "Using cached validation result for ROM: {}", metadata.title_name);
            return cached_result.value();
        }
    }

    ValidationResult result = ValidationResult::Unknown;

    // Perform validation checks in order of preference
    if (impl->config.enable_nintendo_library_check && impl->nintendo_authenticated) {
        result = ValidateWithNintendoLibrary(metadata);
        if (result == ValidationResult::ValidNintendoLibrary) {
            impl->stats.nintendo_library_matches++;
            impl->stats.valid_roms++;
            CacheValidationResult(metadata.file_hash, result);
            return result;
        }
    }

    if (impl->config.enable_dump_tool_validation) {
        ValidationResult dump_result = ValidateDumpTool(metadata);
        if (dump_result == ValidationResult::ValidLegitimateRip) {
            result = dump_result;
            impl->stats.legitimate_dumps++;
            impl->stats.valid_roms++;
        }
    }

    if (impl->config.enable_header_validation) {
        ValidationResult header_result = ValidateHeaders(file);
        if (result == ValidationResult::Unknown) {
            result = header_result;
        }
    }

    // Update statistics
    switch (result) {
        case ValidationResult::Valid:
        case ValidationResult::ValidLegitimateRip:
            impl->stats.valid_roms++;
            break;
        case ValidationResult::Suspicious:
            impl->stats.suspicious_roms++;
            break;
        case ValidationResult::Invalid:
            impl->stats.invalid_roms++;
            break;
        default:
            break;
    }

    // Cache the result
    if (impl->config.cache_validation_results && !metadata.file_hash.empty()) {
        CacheValidationResult(metadata.file_hash, result);
    }

    LOG_INFO(Core, "ROM validation complete: {} - Result: {}", 
             metadata.title_name, static_cast<int>(result));

    return result;
}

RomMetadata AntiPiracyManager::ExtractRomMetadata(FileSys::VirtualFile file) {
    RomMetadata metadata;
    metadata.file_path = file->GetName();
    metadata.file_size = file->GetSize();
    metadata.file_hash = CalculateFileHash(file);

    try {
        // Try to load as different file types to extract metadata
        auto loader = Loader::GetLoader(impl->system, file);
        if (loader) {
            u64 program_id;
            if (loader->ReadProgramId(program_id) == Loader::ResultStatus::Success) {
                metadata.title_id = program_id;
            }

            std::string title;
            if (loader->ReadTitle(title) == Loader::ResultStatus::Success) {
                metadata.title_name = title;
            }

            // Check for NXDump signature
            metadata.has_nxdump_signature = DetectNXDumpSignature(file);
            metadata.dump_tool_signature = DetectDumpToolSignature(file);
            
            // Validate headers
            metadata.has_proper_headers = (ValidateHeaders(file) != ValidationResult::Invalid);
        }
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Error extracting ROM metadata: {}", e.what());
        metadata.validation_notes.push_back("Metadata extraction failed: " + std::string(e.what()));
    }

    return metadata;
}

void AntiPiracyManager::ValidateRomAsync(const std::string& file_path, ValidationCallback callback) {
    std::thread validation_thread([this, file_path, callback]() {
        ValidationResult result = ValidateRom(file_path);
        auto file = FileSys::VfsFilesystem::OpenFile(file_path, FileSys::Mode::Read);
        RomMetadata metadata;
        if (file) {
            metadata = ExtractRomMetadata(file);
        }
        callback(result, metadata);
    });
    validation_thread.detach();
}

void AntiPiracyManager::ValidateRomAsync(FileSys::VirtualFile file, ValidationCallback callback) {
    std::thread validation_thread([this, file, callback]() {
        ValidationResult result = ValidateRom(file);
        RomMetadata metadata = ExtractRomMetadata(file);
        callback(result, metadata);
    });
    validation_thread.detach();
}

void AntiPiracyManager::ClearValidationCache() {
    impl->validation_cache.clear();
    LOG_INFO(Core, "Validation cache cleared");
}

bool AntiPiracyManager::IsRomCached(const std::string& file_hash) const {
    return impl->validation_cache.find(file_hash) != impl->validation_cache.end();
}

std::optional<ValidationResult> AntiPiracyManager::GetCachedResult(const std::string& file_hash) const {
    auto it = impl->validation_cache.find(file_hash);
    if (it != impl->validation_cache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string AntiPiracyManager::GetValidationMessage(ValidationResult result) const {
    switch (result) {
        case ValidationResult::Valid:
            return "ROM validation successful - game appears legitimate.";
        case ValidationResult::ValidNintendoLibrary:
            return "ROM verified against Nintendo purchase history - legitimate copy confirmed.";
        case ValidationResult::ValidLegitimateRip:
            return "ROM appears to be a legitimate dump created with proper tools.";
        case ValidationResult::Unknown:
            return "ROM legitimacy could not be determined. This doesn't necessarily indicate piracy.";
        case ValidationResult::Suspicious:
            return "ROM has characteristics that may indicate piracy. Please ensure you own this game.";
        case ValidationResult::Invalid:
            return "ROM appears to be pirated or corrupted. Please use legitimate game copies.";
        case ValidationResult::NetworkError:
            return "Could not verify ROM due to network issues. Validation will be retried later.";
        case ValidationResult::NotAuthenticated:
            return "Nintendo account authentication required for full validation.";
        default:
            return "Unknown validation result.";
    }
}

std::string AntiPiracyManager::GetEducationalMessage() const {
    return impl->educational_message;
}

std::vector<std::string> AntiPiracyManager::GetLegitimateSourceSuggestions() const {
    return {
        "Purchase games from the Nintendo eShop",
        "Buy physical cartridges from authorized retailers",
        "Use legitimate dumping tools like NXDumpTool for your own games",
        "Ensure you have proper console keys from your own Switch",
        "Visit our documentation for guidance on legitimate game dumping"
    };
}

AntiPiracyManager::ValidationStats AntiPiracyManager::GetValidationStats() const {
    return impl->stats;
}

void AntiPiracyManager::ResetValidationStats() {
    impl->stats = ValidationStats{};
    LOG_INFO(Core, "Validation statistics reset");
}

// Private implementation methods

ValidationResult AntiPiracyManager::ValidateWithNintendoLibrary(const RomMetadata& metadata) {
    if (!impl->nintendo_authenticated) {
        return ValidationResult::NotAuthenticated;
    }

    try {
        auto nintendo_games = GetNintendoGameLibrary();
        auto matching_game = FindMatchingGame(metadata);
        
        if (matching_game.has_value()) {
            LOG_INFO(Core, "ROM matches Nintendo purchase history: {}", metadata.title_name);
            return ValidationResult::ValidNintendoLibrary;
        }
        
        LOG_DEBUG(Core, "ROM not found in Nintendo purchase history: {}", metadata.title_name);
        return ValidationResult::Unknown;
        
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "Error validating with Nintendo Library: {}", e.what());
        return ValidationResult::NetworkError;
    }
}

ValidationResult AntiPiracyManager::ValidateDumpTool(const RomMetadata& metadata) {
    if (metadata.has_nxdump_signature) {
        LOG_INFO(Core, "ROM has NXDump signature - appears to be legitimate dump");
        return ValidationResult::ValidLegitimateRip;
    }
    
    if (!metadata.dump_tool_signature.empty()) {
        if (impl->legitimate_dump_signatures.count(metadata.dump_tool_signature) > 0) {
            LOG_INFO(Core, "ROM created with legitimate dump tool: {}", metadata.dump_tool_signature);
            return ValidationResult::ValidLegitimateRip;
        } else {
            LOG_WARNING(Core, "ROM created with unknown tool: {}", metadata.dump_tool_signature);
            return ValidationResult::Suspicious;
        }
    }
    
    return ValidationResult::Unknown;
}

ValidationResult AntiPiracyManager::ValidateHeaders(FileSys::VirtualFile file) {
    try {
        // Basic file integrity checks
        if (!file || file->GetSize() == 0) {
            return ValidationResult::Invalid;
        }
        
        // Check for common piracy indicators in file structure
        auto loader = Loader::GetLoader(impl->system, file);
        if (!loader) {
            return ValidationResult::Invalid;
        }
        
        // Verify the file can be properly parsed
        u64 program_id;
        if (loader->ReadProgramId(program_id) != Loader::ResultStatus::Success) {
            LOG_WARNING(Core, "Failed to read program ID from ROM");
            return ValidationResult::Suspicious;
        }
        
        // Additional header validation could be added here
        return ValidationResult::Valid;
        
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "Error validating ROM headers: {}", e.what());
        return ValidationResult::Invalid;
    }
}

bool AntiPiracyManager::DetectNXDumpSignature(FileSys::VirtualFile file) {
    try {
        // Look for NXDump signature in the file
        // NXDump typically adds metadata to the end of files
        const size_t signature_search_size = std::min(file->GetSize(), static_cast<size_t>(1024));
        std::vector<u8> buffer(signature_search_size);
        
        // Read from the end of the file
        file->Seek(file->GetSize() - signature_search_size, FileSys::SeekOrigin::SetOrigin);
        size_t bytes_read = file->Read(buffer.data(), signature_search_size);
        
        std::string content(buffer.begin(), buffer.begin() + bytes_read);
        
        // Look for NXDump signatures
        return content.find("nxdumptool") != std::string::npos ||
               content.find("NXDumpTool") != std::string::npos ||
               content.find("NXDT") != std::string::npos;
               
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Error detecting NXDump signature: {}", e.what());
        return false;
    }
}

std::string AntiPiracyManager::CalculateFileHash(FileSys::VirtualFile file) {
    try {
        // Calculate SHA-256 hash of the first 1MB for performance
        const size_t hash_size = std::min(file->GetSize(), static_cast<size_t>(1024 * 1024));
        std::vector<u8> buffer(hash_size);
        
        file->Seek(0, FileSys::SeekOrigin::SetOrigin);
        size_t bytes_read = file->Read(buffer.data(), hash_size);
        
        // Simple hash calculation (in a real implementation, use proper SHA-256)
        std::hash<std::string> hasher;
        std::string data(buffer.begin(), buffer.begin() + bytes_read);
        size_t hash = hasher(data);
        
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str();
        
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Error calculating file hash: {}", e.what());
        return "";
    }
}

std::optional<Nintendo::GameInfo> AntiPiracyManager::FindMatchingGame(const RomMetadata& metadata) {
    auto nintendo_games = GetNintendoGameLibrary();
    
    for (const auto& game : nintendo_games) {
        // Try exact title ID match first
        if (metadata.title_id != 0) {
            std::stringstream ss;
            ss << std::hex << metadata.title_id;
            if (game.title_id == ss.str()) {
                return game;
            }
        }
        
        // Try fuzzy title name matching
        if (!metadata.title_name.empty() && !game.title_name.empty()) {
            std::string rom_title = metadata.title_name;
            std::string lib_title = game.title_name;
            
            // Convert to lowercase for comparison
            std::transform(rom_title.begin(), rom_title.end(), rom_title.begin(), ::tolower);
            std::transform(lib_title.begin(), lib_title.end(), lib_title.begin(), ::tolower);
            
            // Remove common suffixes and prefixes
            std::regex cleanup_regex(R"(\s*\(.*\)|\s*\[.*\]|\s*-.*$)");
            rom_title = std::regex_replace(rom_title, cleanup_regex, "");
            lib_title = std::regex_replace(lib_title, cleanup_regex, "");
            
            if (rom_title == lib_title) {
                return game;
            }
            
            // Check if one title contains the other
            if (rom_title.find(lib_title) != std::string::npos ||
                lib_title.find(rom_title) != std::string::npos) {
                return game;
            }
        }
    }
    
    return std::nullopt;
}

std::string AntiPiracyManager::DetectDumpToolSignature(FileSys::VirtualFile file) {
    try {
        // Search for dump tool signatures in file metadata
        const size_t search_size = std::min(file->GetSize(), static_cast<size_t>(2048));
        std::vector<u8> buffer(search_size);
        
        file->Seek(0, FileSys::SeekOrigin::SetOrigin);
        size_t bytes_read = file->Read(buffer.data(), search_size);
        
        std::string content(buffer.begin(), buffer.begin() + bytes_read);
        
        // Check for known dump tool signatures
        for (const auto& signature : impl->legitimate_dump_signatures) {
            if (content.find(signature) != std::string::npos) {
                return signature;
            }
        }
        
        return "";
        
    } catch (const std::exception& e) {
        LOG_WARNING(Core, "Error detecting dump tool signature: {}", e.what());
        return "";
    }
}

void AntiPiracyManager::CacheValidationResult(const std::string& file_hash, ValidationResult result) {
    if (!file_hash.empty()) {
        impl->validation_cache[file_hash] = result;
    }
}

void AntiPiracyManager::LoadValidationCache() {
    // TODO: Implement cache persistence
    LOG_DEBUG(Core, "Loading validation cache (not implemented)");
}

void AntiPiracyManager::SaveValidationCache() {
    // TODO: Implement cache persistence
    LOG_DEBUG(Core, "Saving validation cache (not implemented)");
}

void AntiPiracyManager::LoadConfigFromFile(const std::string& config_path) {
    // TODO: Implement configuration file loading
    LOG_DEBUG(Core, "Loading anti-piracy config from: {}", config_path);
    
    // Set default configuration for now
    impl->config = ValidationConfig{};
}

void AntiPiracyManager::SaveConfigToFile(const std::string& config_path) const {
    // TODO: Implement configuration file saving
    LOG_DEBUG(Core, "Saving anti-piracy config to: {}", config_path);
}

} // namespace Core