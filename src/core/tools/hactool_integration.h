// SPDX-FileCopyrightText: Copyright 2025 suyu Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/file_sys/vfs/vfs_types.h"

namespace Tools {

/// Describes the type of extraction that was performed
enum class ExtractionType {
    ExeFS,     ///< Contains main, main.npdm, rtld, etc.
    RomFS,     ///< Contains the game's virtual filesystem
    Full,      ///< Contains both exefs/ and romfs/ subdirectories
    NCA,       ///< A decrypted NCA file
    Directory, ///< Generic directory with game content
};

/// Result of an extraction/detection operation
struct ExtractionResult {
    bool success = false;
    ExtractionType type = ExtractionType::Directory;
    std::filesystem::path output_path;
    std::string error_message;
};

/// Information about an external decryption tool
struct ToolInfo {
    std::string name;
    std::filesystem::path executable_path;
    std::string version;
    bool available = false;
};

/**
 * Integrates with external decryption tools (hactool, hac2l, nstool)
 * to allow the emulator to use pre-decrypted game content.
 *
 * This class does NOT perform any decryption itself — it orchestrates
 * external tools that the user has installed separately.
 */
class HactoolIntegration {
public:
    using ProgressCallback = std::function<void(const std::string& status, int percent)>;

    HactoolIntegration();
    ~HactoolIntegration();

    /// Scan for available external tools in PATH and common locations
    void ScanForTools();

    /// Get list of all detected tools
    [[nodiscard]] std::vector<ToolInfo> GetAvailableTools() const;

    /// Check if any external decryption tool is available
    [[nodiscard]] bool HasAnyTool() const;

    /// Check if a specific tool is available
    [[nodiscard]] bool HasTool(const std::string& name) const;

    /// Get info about a specific tool
    [[nodiscard]] std::optional<ToolInfo> GetToolInfo(const std::string& name) const;

    /// Set a custom path for a specific tool
    void SetToolPath(const std::string& name, const std::filesystem::path& path);

    /**
     * Extract an NCA file using an available external tool.
     * Output goes to a temporary directory managed by the integration.
     *
     * @param nca_path Path to the encrypted NCA file
     * @param keys_path Optional path to keys file (prod.keys)
     * @param progress Optional progress callback
     * @return ExtractionResult with output directory on success
     */
    ExtractionResult ExtractNCA(const std::filesystem::path& nca_path,
                                const std::filesystem::path& keys_path = {},
                                ProgressCallback progress = nullptr);

    /**
     * Extract an NSP file using an available external tool.
     *
     * @param nsp_path Path to the NSP file
     * @param keys_path Optional path to keys file
     * @param progress Optional progress callback
     * @return ExtractionResult with output directory on success
     */
    ExtractionResult ExtractNSP(const std::filesystem::path& nsp_path,
                                const std::filesystem::path& keys_path = {},
                                ProgressCallback progress = nullptr);

    /**
     * Extract an XCI file using an available external tool.
     *
     * @param xci_path Path to the XCI file
     * @param keys_path Optional path to keys file
     * @param progress Optional progress callback
     * @return ExtractionResult with output directory on success
     */
    ExtractionResult ExtractXCI(const std::filesystem::path& xci_path,
                                const std::filesystem::path& keys_path = {},
                                ProgressCallback progress = nullptr);

    /**
     * Detect whether a directory contains hactool-extracted content.
     * Checks for typical hactool output structure:
     *   - exefs/ subdirectory with main + main.npdm
     *   - romfs/ subdirectory
     *   - Direct exefs content (main + main.npdm at root)
     *
     * @param dir_path Path to check
     * @return ExtractionType if detected, nullopt otherwise
     */
    [[nodiscard]] static std::optional<ExtractionType>
    DetectExtractedContent(const std::filesystem::path& dir_path);

    /**
     * Create a VirtualDir from a hactool-extracted directory.
     * Handles the various output structures that hactool produces.
     *
     * @param dir_path Path to the extracted content
     * @param vfs The virtual filesystem to use for creating entries
     * @return VirtualDir pointing to the loadable content, or nullptr
     */
    [[nodiscard]] static FileSys::VirtualDir
    CreateVfsFromExtracted(const std::filesystem::path& dir_path,
                           FileSys::VirtualFilesystem& vfs);

    /// Get the temporary extraction directory
    [[nodiscard]] const std::filesystem::path& GetExtractionDir() const;

    /// Clean up temporary extraction files
    void CleanupExtractions();

private:
    /// Try to find a tool executable in PATH and common locations
    std::optional<std::filesystem::path> FindToolExecutable(const std::string& name) const;

    /// Run an external tool and capture output
    ExtractionResult RunTool(const std::filesystem::path& tool_path,
                             const std::vector<std::string>& args,
                             ProgressCallback progress = nullptr);

    /// Get the version string from a tool
    std::string GetToolVersion(const std::filesystem::path& tool_path) const;

    std::vector<ToolInfo> tools_;
    std::filesystem::path extraction_dir_;
    std::vector<std::filesystem::path> search_paths_;
};

} // namespace Tools
