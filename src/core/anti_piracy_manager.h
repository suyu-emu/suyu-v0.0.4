// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs.h"

namespace Nintendo {
class Library;
struct GameInfo;
} // namespace Nintendo

namespace Core {

class System;

/**
 * Anti-piracy validation results
 */
enum class ValidationResult {
    Valid,                    // ROM is verified as legitimate
    ValidNintendoLibrary,    // ROM matches Nintendo purchase history
    ValidLegitimateRip,      // ROM appears to be legitimately dumped
    Unknown,                 // Cannot determine legitimacy (not necessarily invalid)
    Suspicious,              // ROM has characteristics suggesting piracy
    Invalid,                 // ROM is clearly pirated or corrupted
    NetworkError,            // Could not verify due to network issues
    NotAuthenticated         // User not authenticated with Nintendo account
};

/**
 * ROM metadata extracted for validation purposes
 */
struct RomMetadata {
    u64 title_id = 0;
    std::string title_name;
    std::string version;
    std::string region;
    std::string file_path;
    std::string file_hash;
    u64 file_size = 0;
    bool has_nxdump_signature = false;
    bool has_proper_headers = false;
    std::string dump_tool_signature;
    std::vector<std::string> validation_notes;
};

/**
 * Configuration for anti-piracy validation
 */
struct ValidationConfig {
    bool enable_nintendo_library_check = true;
    bool enable_dump_tool_validation = true;
    bool enable_header_validation = true;
    bool require_authentication = false;
    bool show_educational_messages = true;
    bool cache_validation_results = true;
    int validation_timeout_seconds = 30;
};

/**
 * Anti-Piracy Manager
 * 
 * Coordinates all anti-piracy validation efforts including Nintendo Library
 * cross-referencing, legitimate dump detection, and educational messaging.
 * 
 * This system is designed to encourage legitimate game ownership without
 * implementing restrictive DRM. All validation is optional and configurable.
 */
class AntiPiracyManager {
public:
    explicit AntiPiracyManager(Core::System& system);
    ~AntiPiracyManager();

    // Core lifecycle
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    // Configuration management
    void SetValidationConfig(const ValidationConfig& config);
    ValidationConfig GetValidationConfig() const;
    void LoadConfigFromFile(const std::string& config_path);
    void SaveConfigToFile(const std::string& config_path) const;

    // Nintendo Library integration
    bool AuthenticateWithNintendo(const std::string& username, const std::string& password);
    bool IsNintendoAuthenticated() const;
    void ClearNintendoAuthentication();
    std::vector<Nintendo::GameInfo> GetNintendoGameLibrary();

    // ROM validation
    ValidationResult ValidateRom(const std::string& file_path);
    ValidationResult ValidateRom(FileSys::VirtualFile file);
    RomMetadata ExtractRomMetadata(FileSys::VirtualFile file);
    
    // Asynchronous validation for better performance
    using ValidationCallback = std::function<void(ValidationResult, const RomMetadata&)>;
    void ValidateRomAsync(const std::string& file_path, ValidationCallback callback);
    void ValidateRomAsync(FileSys::VirtualFile file, ValidationCallback callback);

    // Cache management
    void ClearValidationCache();
    bool IsRomCached(const std::string& file_hash) const;
    std::optional<ValidationResult> GetCachedResult(const std::string& file_hash) const;

    // Educational and user guidance
    std::string GetValidationMessage(ValidationResult result) const;
    std::string GetEducationalMessage() const;
    std::vector<std::string> GetLegitimateSourceSuggestions() const;

    // Statistics and reporting
    struct ValidationStats {
        u32 total_validations = 0;
        u32 valid_roms = 0;
        u32 nintendo_library_matches = 0;
        u32 legitimate_dumps = 0;
        u32 suspicious_roms = 0;
        u32 invalid_roms = 0;
    };
    ValidationStats GetValidationStats() const;
    void ResetValidationStats();

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    // Internal validation methods
    ValidationResult ValidateWithNintendoLibrary(const RomMetadata& metadata);
    ValidationResult ValidateDumpTool(const RomMetadata& metadata);
    ValidationResult ValidateHeaders(FileSys::VirtualFile file);
    bool DetectNXDumpSignature(FileSys::VirtualFile file);
    std::string CalculateFileHash(FileSys::VirtualFile file);
    
    // Nintendo Library cross-referencing
    bool MatchesNintendoLibrary(const RomMetadata& metadata);
    std::optional<Nintendo::GameInfo> FindMatchingGame(const RomMetadata& metadata);
    
    // Legitimate dump detection
    bool HasLegitimateRipCharacteristics(FileSys::VirtualFile file);
    std::string DetectDumpToolSignature(FileSys::VirtualFile file);
    
    // Caching and persistence
    void CacheValidationResult(const std::string& file_hash, ValidationResult result);
    void LoadValidationCache();
    void SaveValidationCache();
};

} // namespace Core