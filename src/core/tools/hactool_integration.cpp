// SPDX-FileCopyrightText: Copyright 2025 suyu Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>

#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/tools/hactool_integration.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace Tools {

namespace {

/// Known external decryption tools and their typical executable names
constexpr std::array<const char*, 4> KNOWN_TOOLS = {
    "hactool",
    "hac2l",
    "nstool",
    "hacpack",
};

#ifdef _WIN32
constexpr const char* EXE_SUFFIX = ".exe";
#else
constexpr const char* EXE_SUFFIX = "";
#endif

std::vector<std::filesystem::path> GetDefaultSearchPaths() {
    std::vector<std::filesystem::path> paths;

    // Current directory
    paths.emplace_back(".");

    // Suyu tools directory
    const auto suyu_path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::SuyuDir);
    paths.push_back(suyu_path / "tools");

#ifdef _WIN32
    // Common Windows install locations
    if (const char* program_files = std::getenv("ProgramFiles")) {
        paths.emplace_back(std::filesystem::path(program_files) / "hactool");
        paths.emplace_back(std::filesystem::path(program_files) / "hac2l");
        paths.emplace_back(std::filesystem::path(program_files) / "nstool");
    }
    if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        paths.emplace_back(std::filesystem::path(local_app_data) / "hactool");
    }
    if (const char* userprofile = std::getenv("USERPROFILE")) {
        paths.emplace_back(std::filesystem::path(userprofile) / "hactool");
        paths.emplace_back(std::filesystem::path(userprofile) / ".local" / "bin");
    }
#else
    // Common Unix locations
    paths.emplace_back("/usr/local/bin");
    paths.emplace_back("/usr/bin");
    if (const char* home = std::getenv("HOME")) {
        paths.emplace_back(std::filesystem::path(home) / ".local" / "bin");
        paths.emplace_back(std::filesystem::path(home) / "bin");
    }
#endif

    return paths;
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

bool DirectoryExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

} // namespace

HactoolIntegration::HactoolIntegration() {
    search_paths_ = GetDefaultSearchPaths();

    // Create extraction temp directory
    const auto suyu_path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::SuyuDir);
    extraction_dir_ = suyu_path / "extracted";

    std::error_code ec;
    std::filesystem::create_directories(extraction_dir_, ec);
}

HactoolIntegration::~HactoolIntegration() = default;

void HactoolIntegration::ScanForTools() {
    tools_.clear();

    for (const auto& tool_name : KNOWN_TOOLS) {
        ToolInfo info;
        info.name = tool_name;

        auto path = FindToolExecutable(tool_name);
        if (path) {
            info.executable_path = *path;
            info.available = true;
            info.version = GetToolVersion(*path);
            LOG_INFO(Common, "Found external tool: {} at {} ({})", tool_name,
                     path->generic_string(), info.version);
        } else {
            info.available = false;
            LOG_DEBUG(Common, "External tool not found: {}", tool_name);
        }

        tools_.push_back(std::move(info));
    }
}

std::vector<ToolInfo> HactoolIntegration::GetAvailableTools() const {
    std::vector<ToolInfo> available;
    std::copy_if(tools_.begin(), tools_.end(), std::back_inserter(available),
                 [](const ToolInfo& t) { return t.available; });
    return available;
}

bool HactoolIntegration::HasAnyTool() const {
    return std::any_of(tools_.begin(), tools_.end(),
                       [](const ToolInfo& t) { return t.available; });
}

bool HactoolIntegration::HasTool(const std::string& name) const {
    return std::any_of(tools_.begin(), tools_.end(),
                       [&name](const ToolInfo& t) { return t.available && t.name == name; });
}

std::optional<ToolInfo> HactoolIntegration::GetToolInfo(const std::string& name) const {
    auto it = std::find_if(tools_.begin(), tools_.end(),
                           [&name](const ToolInfo& t) { return t.name == name; });
    if (it != tools_.end()) {
        return *it;
    }
    return std::nullopt;
}

void HactoolIntegration::SetToolPath(const std::string& name,
                                     const std::filesystem::path& path) {
    auto it = std::find_if(tools_.begin(), tools_.end(),
                           [&name](const ToolInfo& t) { return t.name == name; });
    if (it != tools_.end()) {
        it->executable_path = path;
        it->available = FileExists(path);
        if (it->available) {
            it->version = GetToolVersion(path);
        }
    } else {
        ToolInfo info;
        info.name = name;
        info.executable_path = path;
        info.available = FileExists(path);
        if (info.available) {
            info.version = GetToolVersion(path);
        }
        tools_.push_back(std::move(info));
    }
}

ExtractionResult HactoolIntegration::ExtractNCA(const std::filesystem::path& nca_path,
                                                 const std::filesystem::path& keys_path,
                                                 ProgressCallback progress) {
    ExtractionResult result;

    if (!HasTool("hactool") && !HasTool("hac2l")) {
        result.success = false;
        result.error_message = "No NCA extraction tool found (need hactool or hac2l)";
        return result;
    }

    // Create output directory based on NCA filename
    const auto stem = nca_path.stem().generic_string();
    const auto output_dir = extraction_dir_ / stem;
    std::error_code ec;
    std::filesystem::create_directories(output_dir / "exefs", ec);
    std::filesystem::create_directories(output_dir / "romfs", ec);

    // Prefer hactool, fall back to hac2l
    const auto tool = HasTool("hactool") ? GetToolInfo("hactool") : GetToolInfo("hac2l");
    if (!tool) {
        result.success = false;
        result.error_message = "Tool lookup failed unexpectedly";
        return result;
    }

    if (progress) {
        progress("Extracting NCA with " + tool->name + "...", 10);
    }

    std::vector<std::string> args;

    if (tool->name == "hactool") {
        args = {
            nca_path.generic_string(),
            "--exefsdir=" + (output_dir / "exefs").generic_string(),
            "--romfsdir=" + (output_dir / "romfs").generic_string(),
        };
        if (!keys_path.empty() && FileExists(keys_path)) {
            args.push_back("--keyset=" + keys_path.generic_string());
        }
    } else if (tool->name == "hac2l") {
        args = {
            "--intype=nca",
            nca_path.generic_string(),
            "--exefsdir=" + (output_dir / "exefs").generic_string(),
            "--romfsdir=" + (output_dir / "romfs").generic_string(),
        };
        if (!keys_path.empty() && FileExists(keys_path)) {
            args.push_back("--keyset=" + keys_path.generic_string());
        }
    }

    result = RunTool(tool->executable_path, args, progress);
    result.output_path = output_dir;
    result.type = ExtractionType::Full;

    if (progress) {
        progress(result.success ? "Extraction complete" : "Extraction failed",
                 result.success ? 100 : -1);
    }

    return result;
}

ExtractionResult HactoolIntegration::ExtractNSP(const std::filesystem::path& nsp_path,
                                                 const std::filesystem::path& keys_path,
                                                 ProgressCallback progress) {
    ExtractionResult result;

    if (!HasTool("hactool")) {
        result.success = false;
        result.error_message = "hactool not found (required for NSP extraction)";
        return result;
    }

    const auto stem = nsp_path.stem().generic_string();
    const auto output_dir = extraction_dir_ / stem;
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);

    const auto tool = GetToolInfo("hactool");

    if (progress) {
        progress("Extracting NSP with hactool...", 10);
    }

    std::vector<std::string> args = {
        "--intype=pfs0",
        nsp_path.generic_string(),
        "--outdir=" + output_dir.generic_string(),
    };
    if (!keys_path.empty() && FileExists(keys_path)) {
        args.push_back("--keyset=" + keys_path.generic_string());
    }

    result = RunTool(tool->executable_path, args, progress);
    result.output_path = output_dir;
    result.type = ExtractionType::Full;

    if (progress) {
        progress(result.success ? "Extraction complete" : "Extraction failed",
                 result.success ? 100 : -1);
    }

    return result;
}

ExtractionResult HactoolIntegration::ExtractXCI(const std::filesystem::path& xci_path,
                                                 const std::filesystem::path& keys_path,
                                                 ProgressCallback progress) {
    ExtractionResult result;

    if (!HasTool("hactool")) {
        result.success = false;
        result.error_message = "hactool not found (required for XCI extraction)";
        return result;
    }

    const auto stem = xci_path.stem().generic_string();
    const auto output_dir = extraction_dir_ / stem;
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);

    const auto tool = GetToolInfo("hactool");

    if (progress) {
        progress("Extracting XCI with hactool...", 10);
    }

    std::vector<std::string> args = {
        "--intype=xci",
        xci_path.generic_string(),
        "--outdir=" + output_dir.generic_string(),
    };
    if (!keys_path.empty() && FileExists(keys_path)) {
        args.push_back("--keyset=" + keys_path.generic_string());
    }

    result = RunTool(tool->executable_path, args, progress);
    result.output_path = output_dir;
    result.type = ExtractionType::Full;

    if (progress) {
        progress(result.success ? "Extraction complete" : "Extraction failed",
                 result.success ? 100 : -1);
    }

    return result;
}

std::optional<ExtractionType>
HactoolIntegration::DetectExtractedContent(const std::filesystem::path& dir_path) {
    if (!DirectoryExists(dir_path)) {
        return std::nullopt;
    }

    // Check for exefs/ subdirectory (hactool --exefsdir output)
    const auto exefs_dir = dir_path / "exefs";
    const auto romfs_dir = dir_path / "romfs";

    const bool has_exefs_subdir =
        DirectoryExists(exefs_dir) && FileExists(exefs_dir / "main") &&
        FileExists(exefs_dir / "main.npdm");

    const bool has_romfs_subdir = DirectoryExists(romfs_dir);

    if (has_exefs_subdir && has_romfs_subdir) {
        return ExtractionType::Full;
    }
    if (has_exefs_subdir) {
        return ExtractionType::ExeFS;
    }
    if (has_romfs_subdir) {
        return ExtractionType::RomFS;
    }

    // Check for direct exefs content at root (main + main.npdm)
    if (FileExists(dir_path / "main") && FileExists(dir_path / "main.npdm")) {
        return ExtractionType::ExeFS;
    }

    // Check for decrypted NCA files in directory
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".nca") {
            return ExtractionType::NCA;
        }
    }

    return std::nullopt;
}

FileSys::VirtualDir
HactoolIntegration::CreateVfsFromExtracted(const std::filesystem::path& dir_path,
                                           FileSys::VirtualFilesystem& vfs) {
    auto content_type = DetectExtractedContent(dir_path);
    if (!content_type) {
        LOG_WARNING(Common, "Could not detect extracted content type in: {}",
                    dir_path.generic_string());
        return nullptr;
    }

    switch (*content_type) {
    case ExtractionType::Full: {
        // Open the exefs/ subdirectory — this is what the deconstructed loader needs
        const auto exefs_path = dir_path / "exefs";
        return vfs->OpenDirectory(exefs_path.generic_string(), FileSys::OpenMode::Read);
    }
    case ExtractionType::ExeFS: {
        // Direct exefs content or exefs/ subdir
        const auto exefs_dir = dir_path / "exefs";
        if (DirectoryExists(exefs_dir)) {
            return vfs->OpenDirectory(exefs_dir.generic_string(), FileSys::OpenMode::Read);
        }
        return vfs->OpenDirectory(dir_path.generic_string(), FileSys::OpenMode::Read);
    }
    case ExtractionType::RomFS:
    case ExtractionType::NCA:
    case ExtractionType::Directory:
        // For romfs-only or NCA directories, open the directory as-is
        return vfs->OpenDirectory(dir_path.generic_string(), FileSys::OpenMode::Read);
    }

    return nullptr;
}

const std::filesystem::path& HactoolIntegration::GetExtractionDir() const {
    return extraction_dir_;
}

void HactoolIntegration::CleanupExtractions() {
    std::error_code ec;
    if (std::filesystem::exists(extraction_dir_, ec)) {
        std::filesystem::remove_all(extraction_dir_, ec);
        std::filesystem::create_directories(extraction_dir_, ec);
        LOG_INFO(Common, "Cleaned up extraction directory");
    }
}

std::optional<std::filesystem::path>
HactoolIntegration::FindToolExecutable(const std::string& name) const {
    const std::string exe_name = name + EXE_SUFFIX;

    // Check in custom search paths first
    for (const auto& search_path : search_paths_) {
        const auto full_path = search_path / exe_name;
        if (FileExists(full_path)) {
            return full_path;
        }
    }

    // Check PATH environment variable
#ifdef _WIN32
    // Use SearchPathW on Windows
    wchar_t result_path[MAX_PATH];
    const auto wide_name = std::filesystem::path(exe_name).wstring();
    if (SearchPathW(nullptr, wide_name.c_str(), nullptr, MAX_PATH, result_path, nullptr)) {
        return std::filesystem::path(result_path);
    }
#else
    // Parse PATH on Unix
    if (const char* path_env = std::getenv("PATH")) {
        std::string path_str(path_env);
        size_t pos = 0;
        while (pos < path_str.size()) {
            size_t end = path_str.find(':', pos);
            if (end == std::string::npos)
                end = path_str.size();

            std::filesystem::path dir(path_str.substr(pos, end - pos));
            auto full_path = dir / exe_name;
            if (FileExists(full_path)) {
                return full_path;
            }
            pos = end + 1;
        }
    }
#endif

    return std::nullopt;
}

ExtractionResult HactoolIntegration::RunTool(const std::filesystem::path& tool_path,
                                             const std::vector<std::string>& args,
                                             ProgressCallback progress) {
    ExtractionResult result;

    // Build command line
    std::string cmdline = "\"" + tool_path.generic_string() + "\"";
    for (const auto& arg : args) {
        cmdline += " \"" + arg + "\"";
    }

    LOG_INFO(Common, "Running external tool: {}", cmdline);

    if (progress) {
        progress("Running " + tool_path.filename().generic_string() + "...", 50);
    }

#ifdef _WIN32
    // Use CreateProcess on Windows for proper output capture
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    // Create pipes for stdout/stderr capture
    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        si.hStdOutput = stdout_write;
        si.hStdError = stdout_write;
    }

    std::string mutable_cmdline(cmdline);
    const BOOL success = CreateProcessA(nullptr, mutable_cmdline.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (stdout_write) {
        CloseHandle(stdout_write);
    }

    if (!success) {
        result.success = false;
        result.error_message = "Failed to start process: " + tool_path.generic_string();
        if (stdout_read) {
            CloseHandle(stdout_read);
        }
        LOG_ERROR(Common, "{}", result.error_message);
        return result;
    }

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytes_read;
    while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) &&
           bytes_read > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    CloseHandle(stdout_read);

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.success = (exit_code == 0);
    if (!result.success) {
        result.error_message =
            "Tool exited with code " + std::to_string(exit_code) + ": " + output;
        LOG_ERROR(Common, "{}", result.error_message);
    } else {
        LOG_INFO(Common, "Tool completed successfully");
    }
#else
    // Use fork/exec on Unix
    const int ret = std::system(cmdline.c_str());
    result.success = (ret == 0);
    if (!result.success) {
        result.error_message = "Tool exited with code " + std::to_string(ret);
        LOG_ERROR(Common, "{}", result.error_message);
    }
#endif

    return result;
}

std::string
HactoolIntegration::GetToolVersion(const std::filesystem::path& tool_path) const {
    // Try --version flag
    std::string cmdline = "\"" + tool_path.generic_string() + "\" --version";

#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        return "unknown";
    }

    si.hStdOutput = stdout_write;
    si.hStdError = stdout_write;

    std::string mutable_cmdline(cmdline);
    const BOOL success = CreateProcessA(nullptr, mutable_cmdline.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(stdout_write);

    if (!success) {
        CloseHandle(stdout_read);
        return "unknown";
    }

    std::string output;
    char buffer[1024];
    DWORD bytes_read;
    while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) &&
           bytes_read > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    CloseHandle(stdout_read);

    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Extract first line as version info
    if (!output.empty()) {
        const auto newline = output.find('\n');
        return output.substr(0, std::min(newline, static_cast<size_t>(80)));
    }
#endif

    return "unknown";
}

} // namespace Tools
