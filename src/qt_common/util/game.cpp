// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/util/game.h"

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/file_sys/savedata_factory.h"
#include "core/hle/service/am/am_types.h"
#include "frontend_common/content_manager.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"

#include <QDesktopServices>
#include <QStandardPaths>
#include <QUrl>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "common/fs/file.h"
#else
#include <fstream>
#include "fmt/ostream.h"
#endif

namespace QtCommon::Game {

bool CreateShortcutLink(const std::filesystem::path& shortcut_path, const std::string& comment,
                        const std::filesystem::path& icon_path,
                        const std::filesystem::path& command, const std::string& arguments,
                        const std::string& categories, const std::string& keywords,
                        const std::string& name) try {
#ifdef _WIN32 // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT {
        CoUninitialize();
    };
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT {
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    };
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(command.c_str());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(
        std::filesystem::path{shortcut_path / (Common::UTF8ToUTF16W(name) + L".lnk")}.c_str(),
        TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__) // Any desktop NIX
    std::filesystem::path shortcut_path_full = shortcut_path / (name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", icon_path.string());
    }
    fmt::print(shortcut_stream, "TryExec={}\n", command.string());
    fmt::print(shortcut_stream, "Exec={} {}\n", command.string(), arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#else                                                                   // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

bool MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                         std::filesystem::path& out_icon_path) {
    // Get path to Yuzu icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = Common::FS::GetEdenPath(Common::FS::EdenPath::IconsDir);
    ico_extension = "ico";
#elif !defined(__ANDROID__) // Any *nix but android
    out_icon_path = Common::FS::GetDataDirectory("XDG_DATA_HOME") / "icons/hicolor/256x256";
#endif
    // Create icons directory if it doesn't exist
    if (!Common::FS::CreateDirs(out_icon_path)) {
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /= (program_id == 0 ? fmt::format("eden-{}.{}", game_file_name, ico_extension)
                                      : fmt::format("eden-{:016X}.{}", program_id, ico_extension));
    return true;
}

void OpenEdenFolder(const Common::FS::EdenPath& path) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QString::fromStdString(Common::FS::GetEdenPathString(path))));
}

void OpenRootDataFolder() {
    OpenEdenFolder(Common::FS::EdenPath::EdenDir);
}

void OpenNANDFolder() {
    OpenEdenFolder(Common::FS::EdenPath::NANDDir);
}

void OpenSaveFolder() {
    const auto path =
        Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) / "user/save/0000000000000000";
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
}

void OpenSDMCFolder() {
    OpenEdenFolder(Common::FS::EdenPath::SDMCDir);
}

void OpenModFolder() {
    OpenEdenFolder(Common::FS::EdenPath::LoadDir);
}

void OpenLogFolder() {
    OpenEdenFolder(Common::FS::EdenPath::LogDir);
}

static QString GetGameListErrorRemoving(QtCommon::Game::InstalledEntryType type) {
    switch (type) {
    case QtCommon::Game::InstalledEntryType::Game:
        return tr("Error Removing Contents");
    case QtCommon::Game::InstalledEntryType::Update:
        return tr("Error Removing Update");
    case QtCommon::Game::InstalledEntryType::AddOnContent:
        return tr("Error Removing DLC");
    default:
        return QStringLiteral("Error Removing <Invalid Type>");
    }
}

// Game Content //
void RemoveBaseContent(u64 program_id, InstalledEntryType type) {
    const auto res =
        ContentManager::RemoveBaseContent(system->GetFileSystemController(), program_id);
    if (res) {
        QtCommon::Frontend::Information(tr("Successfully Removed"),
                                        tr("Successfully removed the installed base game."));
    } else {
        QtCommon::Frontend::Warning(

            GetGameListErrorRemoving(type),
            tr("The base game is not installed in the NAND and cannot be removed."));
    }
}

void RemoveUpdateContent(u64 program_id, InstalledEntryType type) {
    const auto res = ContentManager::RemoveUpdate(system->GetFileSystemController(), program_id);
    if (res) {
        QtCommon::Frontend::Information(tr("Successfully Removed"),
                                        tr("Successfully removed the installed update."));
    } else {
        QtCommon::Frontend::Warning(GetGameListErrorRemoving(type),
                                    tr("There is no update installed for this title."));
    }
}

void RemoveAddOnContent(u64 program_id, InstalledEntryType type) {
    const size_t count = ContentManager::RemoveAllDLC(*system, program_id);
    if (count == 0) {
        QtCommon::Frontend::Warning(GetGameListErrorRemoving(type),
                                    tr("There are no DLCs installed for this title."));
        return;
    }

    QtCommon::Frontend::Information(tr("Successfully Removed"),
                                    tr("Successfully removed %1 installed DLC.").arg(count));
}

// Global Content //

void RemoveTransferableShaderCache(u64 program_id, GameListRemoveTarget target) {
    const auto target_file_name = [target] {
        switch (target) {
        case GameListRemoveTarget::GlShaderCache:
            return "opengl.bin";
        case GameListRemoveTarget::VkShaderCache:
            return "vulkan.bin";
        default:
            return "";
        }
    }();
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        QtCommon::Frontend::Warning(tr("Error Removing Transferable Shader Cache"),
                                    tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveFile(target_file)) {
        QtCommon::Frontend::Information(tr("Successfully Removed"),
                                        tr("Successfully removed the transferable shader cache."));
    } else {
        QtCommon::Frontend::Warning(tr("Error Removing Transferable Shader Cache"),
                                    tr("Failed to remove the transferable shader cache."));
    }
}

void RemoveVulkanDriverPipelineCache(u64 program_id) {
    static constexpr std::string_view target_file_name = "vulkan_pipelines.bin";

    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        return;
    }
    if (!Common::FS::RemoveFile(target_file)) {
        QtCommon::Frontend::Warning(tr("Error Removing Vulkan Driver Pipeline Cache"),
                                    tr("Failed to remove the driver pipeline cache."));
    }
}

void RemoveAllTransferableShaderCaches(u64 program_id) {
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto program_shader_cache_dir = shader_cache_dir / fmt::format("{:016x}", program_id);

    if (!Common::FS::Exists(program_shader_cache_dir)) {
        QtCommon::Frontend::Warning(tr("Error Removing Transferable Shader Caches"),
                                    tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveDirRecursively(program_shader_cache_dir)) {
        QtCommon::Frontend::Information(tr("Successfully Removed"),
                                        tr("Successfully removed the transferable shader caches."));
    } else {
        QtCommon::Frontend::Warning(

            tr("Error Removing Transferable Shader Caches"),
            tr("Failed to remove the transferable shader cache directory."));
    }
}

void RemoveCustomConfiguration(u64 program_id, const std::string& game_path) {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(game_path));
    const auto config_file_name =
        program_id == 0 ? Common::FS::PathToUTF8String(file_path.filename()).append(".ini")
                        : fmt::format("{:016X}.ini", program_id);
    const auto custom_config_file_path =
        Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "custom" / config_file_name;

    if (!Common::FS::Exists(custom_config_file_path)) {
        QtCommon::Frontend::Warning(tr("Error Removing Custom Configuration"),
                                    tr("A custom configuration for this title does not exist."));
        return;
    }

    if (Common::FS::RemoveFile(custom_config_file_path)) {
        QtCommon::Frontend::Information(tr("Successfully Removed"),
                                        tr("Successfully removed the custom game configuration."));
    } else {
        QtCommon::Frontend::Warning(tr("Error Removing Custom Configuration"),
                                    tr("Failed to remove the custom game configuration."));
    }
}

void RemoveCacheStorage(u64 program_id) {
    const auto nand_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir);
    auto vfs_nand_dir =
        vfs->OpenDirectory(Common::FS::PathToUTF8String(nand_dir), FileSys::OpenMode::Read);

    const auto cache_storage_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Cache,
        0 /* program_id */, {}, 0);

    const auto path = Common::FS::ConcatPathSafe(nand_dir, cache_storage_path);

    // Not an error if it wasn't cleared.
    Common::FS::RemoveDirRecursively(path);
}

// Metadata //
void ResetMetadata(bool show_message) {
    const QString title = tr("Reset Metadata Cache");

    if (!Common::FS::Exists(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                            "game_list/")) {
        if (show_message)
            QtCommon::Frontend::Warning(rootObject, title,
                                        tr("The metadata cache is already empty."));
    } else if (Common::FS::RemoveDirRecursively(
                   Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list")) {
        if (show_message)
            QtCommon::Frontend::Information(rootObject, title,
                                            tr("The operation completed successfully."));
        UISettings::values.is_game_list_reload_pending.exchange(true);
    } else {
        if (show_message)
            QtCommon::Frontend::Warning(
                title,
                tr("The metadata cache couldn't be deleted. It might be in use or non-existent."));
    }
}

// Uhhh //

// TODO(crueter): Make QtCommon::Shortcut
// Messages in pre-defined message boxes for less code spaghetti
inline constexpr bool CreateShortcutMessagesGUI(ShortcutMessages imsg, const QString& game_title) {
    int result = 0;
    using namespace QtCommon::Frontend;
    int buttons;

    switch (imsg) {
    case ShortcutMessages::Fullscreen:
        buttons = Yes | No;
        result = QtCommon::Frontend::Information(
            tr("Create Shortcut"), tr("Do you want to launch the game in fullscreen?"), buttons);
        return result == Yes;
    case ShortcutMessages::Success:
        QtCommon::Frontend::Information(
            tr("Shortcut Created"), tr("Successfully created a shortcut to %1").arg(game_title));
        return false;
    case ShortcutMessages::Volatile:
        buttons = Ok | Cancel;
        result = QtCommon::Frontend::Warning(
            tr("Shortcut may be Volatile!"),
            tr("This will create a shortcut to the current AppImage. This may "
               "not work well if you update. Continue?"),
            buttons);
        return result == Ok;
    default:
        buttons = Ok;
        QtCommon::Frontend::Critical(tr("Failed to Create Shortcut"),
                                     tr("Failed to create a shortcut to %1").arg(game_title),
                                     buttons);
        return false;
    }
}

void CreateShortcut(const std::string& game_path, const u64 program_id,
                    const std::string& game_title_, const ShortcutTarget& target,
                    std::string arguments_, const bool needs_title) {
    // Get path to Eden executable
    std::filesystem::path command = GetEdenCommand();

    // Shortcut path
    std::filesystem::path shortcut_path = GetShortcutPath(target);

    if (!std::filesystem::exists(shortcut_path)) {
        CreateShortcutMessagesGUI(ShortcutMessages::Failed,
                                  QString::fromStdString(shortcut_path.generic_string()));
        LOG_ERROR(Frontend, "Invalid shortcut target {}", shortcut_path.generic_string());
        return;
    }

    const FileSys::PatchManager pm{program_id, QtCommon::system->GetFileSystemController(),
                                   QtCommon::system->GetContentProvider()};
    const auto control = pm.GetControlMetadata();
    const auto loader = Loader::GetLoader(
        *QtCommon::system, QtCommon::vfs->OpenFile(game_path, FileSys::OpenMode::Read));

    std::string game_title{game_title_};

    // Delete illegal characters from title
    if (needs_title) {
        game_title = fmt::format("{:016X}", program_id);
        if (control.first != nullptr) {
            game_title = control.first->GetApplicationName();
        } else {
            loader->ReadTitle(game_title);
        }
    }

    const std::string illegal_chars = "<>:\"/\\|?*.";
    for (auto it = game_title.rbegin(); it != game_title.rend(); ++it) {
        if (illegal_chars.find(*it) != std::string::npos) {
            game_title.erase(it.base() - 1);
        }
    }

    const QString qgame_title = QString::fromStdString(game_title);

    // Get icon from game file
    std::vector<u8> icon_image_file{};
    if (control.second != nullptr) {
        icon_image_file = control.second->ReadAllBytes();
    } else if (loader->ReadIcon(icon_image_file) != Loader::ResultStatus::Success) {
        LOG_WARNING(Frontend, "Could not read icon from {:s}", game_path);
    }

    QImage icon_data =
        QImage::fromData(icon_image_file.data(), static_cast<int>(icon_image_file.size()));
    std::filesystem::path out_icon_path;
    if (QtCommon::Game::MakeShortcutIcoPath(program_id, game_title, out_icon_path)) {
        if (!SaveIconToFile(out_icon_path, icon_data)) {
            LOG_ERROR(Frontend, "Could not write icon to file");
        }
    } else {
        QtCommon::Frontend::Critical(
            tr("Create Icon"),
            tr("Cannot create icon file. Path \"%1\" does not exist and cannot be created.")
                .arg(QString::fromStdString(out_icon_path.string())));
    }

#if defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
    // Special case for AppImages
    // Warn once if we are making a shortcut to a volatile AppImage
    if (command.string().ends_with(".AppImage") && !UISettings::values.shortcut_already_warned) {
        if (!CreateShortcutMessagesGUI(ShortcutMessages::Volatile, qgame_title)) {
            return;
        }
        UISettings::values.shortcut_already_warned = true;
    }
#endif

    // Create shortcut
    std::string arguments{arguments_};
    if (CreateShortcutMessagesGUI(ShortcutMessages::Fullscreen, qgame_title)) {
        arguments = "-f " + arguments;
    }
    const std::string comment = fmt::format("Start {:s} with the Eden Emulator", game_title);
    const std::string categories = "Game;Emulator;Qt;";
    const std::string keywords = "Switch;Nintendo;";

    if (QtCommon::Game::CreateShortcutLink(shortcut_path, comment, out_icon_path, command,
                                           arguments, categories, keywords, game_title)) {
        CreateShortcutMessagesGUI(ShortcutMessages::Success, qgame_title);
        return;
    }
    CreateShortcutMessagesGUI(ShortcutMessages::Failed, qgame_title);
}

// TODO: You want this to be constexpr? Well too bad, clang19 doesn't believe this is a string
// literal
std::string GetShortcutPath(ShortcutTarget target) {
    {
        std::string shortcut_path{};
        if (target == ShortcutTarget::Desktop) {
            shortcut_path =
                QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString();
        } else if (target == ShortcutTarget::Applications) {
            shortcut_path = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
                                .toStdString();
        }

        return shortcut_path;
    }
}

void CreateHomeMenuShortcut(ShortcutTarget target) {
    constexpr u64 QLaunchId = static_cast<u64>(Service::AM::AppletProgramId::QLaunch);
    auto bis_system = QtCommon::system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QtCommon::Frontend::Warning(tr("No firmware available"),
                                    tr("Please install firmware to use the home menu."));
        return;
    }

    auto qlaunch_nca = bis_system->GetEntry(QLaunchId, FileSys::ContentRecordType::Program);
    if (!qlaunch_nca) {
        QtCommon::Frontend::Warning(tr("Home Menu Applet"),
                                    tr("Home Menu is not available. Please reinstall firmware."));
        return;
    }

    auto qlaunch_applet_nca = bis_system->GetEntry(QLaunchId, FileSys::ContentRecordType::Program);
    const auto game_path = qlaunch_applet_nca->GetFullPath();

    // TODO(crueter): Make this use the Eden icon
    CreateShortcut(game_path, QLaunchId, "Switch Home Menu", target, "-qlaunch", false);
}

bool SaveIconToFile(const std::filesystem::path& icon_path, const QImage& image) {
#if defined(WIN32)
#pragma pack(push, 2)
    struct IconDir {
        WORD id_reserved;
        WORD id_type;
        WORD id_count;
    };

    struct IconDirEntry {
        BYTE width;
        BYTE height;
        BYTE color_count;
        BYTE reserved;
        WORD planes;
        WORD bit_count;
        DWORD bytes_in_res;
        DWORD image_offset;
    };
#pragma pack(pop)

    const QImage source_image = image.convertToFormat(QImage::Format_RGB32);
    constexpr std::array<int, 7> scale_sizes{256, 128, 64, 48, 32, 24, 16};
    constexpr int bytes_per_pixel = 4;

    const IconDir icon_dir{
        .id_reserved = 0,
        .id_type = 1,
        .id_count = static_cast<WORD>(scale_sizes.size()),
    };

    Common::FS::IOFile icon_file(icon_path.string(), Common::FS::FileAccessMode::Write,
                                 Common::FS::FileType::BinaryFile);
    if (!icon_file.IsOpen()) {
        return false;
    }

    if (!icon_file.Write(icon_dir)) {
        return false;
    }

    std::size_t image_offset = sizeof(IconDir) + (sizeof(IconDirEntry) * scale_sizes.size());
    for (std::size_t i = 0; i < scale_sizes.size(); i++) {
        const int image_size = scale_sizes[i] * scale_sizes[i] * bytes_per_pixel;
        const IconDirEntry icon_entry{
            .width = static_cast<BYTE>(scale_sizes[i]),
            .height = static_cast<BYTE>(scale_sizes[i]),
            .color_count = 0,
            .reserved = 0,
            .planes = 1,
            .bit_count = bytes_per_pixel * 8,
            .bytes_in_res = static_cast<DWORD>(sizeof(BITMAPINFOHEADER) + image_size),
            .image_offset = static_cast<DWORD>(image_offset),
        };
        image_offset += icon_entry.bytes_in_res;
        if (!icon_file.Write(icon_entry)) {
            return false;
        }
    }

    for (std::size_t i = 0; i < scale_sizes.size(); i++) {
        const QImage scaled_image = source_image.scaled(
            scale_sizes[i], scale_sizes[i], Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const BITMAPINFOHEADER info_header{
            .biSize = sizeof(BITMAPINFOHEADER),
            .biWidth = scaled_image.width(),
            .biHeight = scaled_image.height() * 2,
            .biPlanes = 1,
            .biBitCount = bytes_per_pixel * 8,
            .biCompression = BI_RGB,
            .biSizeImage{},
            .biXPelsPerMeter{},
            .biYPelsPerMeter{},
            .biClrUsed{},
            .biClrImportant{},
        };

        if (!icon_file.Write(info_header)) {
            return false;
        }

        for (int y = 0; y < scaled_image.height(); y++) {
            const auto* line = scaled_image.scanLine(scaled_image.height() - 1 - y);
            std::vector<u8> line_data(scaled_image.width() * bytes_per_pixel);
            std::memcpy(line_data.data(), line, line_data.size());
            if (!icon_file.Write(line_data)) {
                return false;
            }
        }
    }
    icon_file.Close();

    return true;
#elif defined(__unix__) && !defined(__APPLE__) && !defined(__ANDROID__)
    // Convert and write the icon as a PNG
    if (!image.save(QString::fromStdString(icon_path.string()))) {
        LOG_ERROR(Frontend, "Could not write icon as PNG to file");
    } else {
        LOG_INFO(Frontend, "Wrote an icon to {}", icon_path.string());
    }
    return true;
#else
    return false;
#endif
}

} // namespace QtCommon::Game
