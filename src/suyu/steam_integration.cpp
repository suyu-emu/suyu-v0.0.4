// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>

#include "suyu/steam_integration.h"

// Steam binary VDF type tags
namespace VdfType {
constexpr quint8 SubSection = 0x00;
constexpr quint8 String = 0x01;
constexpr quint8 Uint32 = 0x02;
constexpr quint8 EndSection = 0x08;
} // namespace VdfType

SteamIntegration::SteamIntegration(QObject* parent) : QObject(parent) {
    steam_path_ = FindSteamPath();
    network_manager_ = new QNetworkAccessManager(this);
}

QString SteamIntegration::FindSteamPath() const {
    const QString env_path = QString::fromUtf8(qgetenv("STEAM_PATH"));
    if (!env_path.isEmpty()) {
        return QDir::toNativeSeparators(env_path);
    }

#ifdef _WIN32
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"),
                       QSettings::NativeFormat);
    const QString registry_path = settings.value(QStringLiteral("SteamPath")).toString();
    if (!registry_path.isEmpty()) {
        return QDir::toNativeSeparators(registry_path);
    }
    return QStringLiteral("C:/Program Files (x86)/Steam");
#elif defined(__APPLE__)
    return QDir::homePath() + QStringLiteral("/Library/Application Support/Steam");
#else
    const QString home = QDir::homePath();
    const QString steam_root = home + QStringLiteral("/.steam/steam");
    const QString legacy_root = home + QStringLiteral("/.local/share/Steam");
    const QString flatpak_root = home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam");
    if (QDir(steam_root).exists()) {
        return steam_root;
    }
    if (QDir(legacy_root).exists()) {
        return legacy_root;
    }
    if (QDir(flatpak_root).exists()) {
        return flatpak_root;
    }
    return steam_root;
#endif
}

SteamIntegration::~SteamIntegration() = default;

bool SteamIntegration::IsSteamInstalled() const {
    return QDir(steam_path_).exists();
}

QString SteamIntegration::GetSteamUserdataPath() const {
    return QDir(steam_path_).filePath(QStringLiteral("userdata"));
}

QString SteamIntegration::FindShortcutsVdf() const {
    QDir userdata(GetSteamUserdataPath());
    if (!userdata.exists()) {
        return {};
    }

    // Iterate user directories, pick the first one that has a config/shortcuts.vdf
    const auto entries = userdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& user_id : entries) {
        const QString vdf_path =
            userdata.filePath(user_id + QStringLiteral("/config/shortcuts.vdf"));
        if (QFile::exists(vdf_path)) {
            return vdf_path;
        }
        // Also check if config directory exists but no shortcuts.vdf yet (first time)
        const QString config_dir = userdata.filePath(user_id + QStringLiteral("/config"));
        if (QDir(config_dir).exists()) {
            return vdf_path; // Return the expected path even if file doesn't exist
        }
    }
    return {};
}

quint32 SteamIntegration::GenerateAppId(const QString& exe, const QString& app_name) {
    // Steam generates non-Steam shortcut AppIDs via CRC32 of (exe + app_name)
    // then applies: (crc | 0x80000000) >> 0
    const QByteArray input = (exe + app_name).toUtf8();
    quint32 crc = 0xFFFFFFFF;
    for (char byte : input) {
        crc ^= static_cast<quint8>(byte);
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    crc ^= 0xFFFFFFFF;
    return (crc | 0x80000000);
}

// --- VDF Binary Format Parser ---
// shortcuts.vdf is a binary VDF file with the structure:
//   \x00 "shortcuts" \x00
//     \x00 "0" \x00          (shortcut index as string)
//       \x01 "appid" \x00 <4-byte-uint32>    (or sometimes \x02)
//       \x01 "AppName" \x00 <null-term-string>
//       \x01 "Exe" \x00 <null-term-string>
//       ...
//       \x08                 (end of this shortcut)
//     \x00 "1" \x00
//       ...
//     \x08                   (end of shortcuts section)
//   \x08                     (end of root)

std::vector<SteamIntegration::SteamShortcut>
SteamIntegration::ParseShortcutsVdf(const QByteArray& data) const {
    std::vector<SteamShortcut> shortcuts;

    int pos = 0;
    const int size = data.size();

    auto readByte = [&]() -> quint8 {
        if (pos >= size) return VdfType::EndSection;
        return static_cast<quint8>(data[pos++]);
    };

    auto readString = [&]() -> QByteArray {
        QByteArray result;
        while (pos < size && data[pos] != '\0') {
            result.append(data[pos++]);
        }
        if (pos < size) pos++; // skip null terminator
        return result;
    };

    auto readUint32 = [&]() -> quint32 {
        if (pos + 4 > size) return 0;
        quint32 val = 0;
        val |= static_cast<quint32>(static_cast<quint8>(data[pos]));
        val |= static_cast<quint32>(static_cast<quint8>(data[pos + 1])) << 8;
        val |= static_cast<quint32>(static_cast<quint8>(data[pos + 2])) << 16;
        val |= static_cast<quint32>(static_cast<quint8>(data[pos + 3])) << 24;
        pos += 4;
        return val;
    };

    // Top-level: expect \x00 "shortcuts" \x00
    if (readByte() != VdfType::SubSection) return shortcuts;
    const QByteArray root_key = readString();
    if (root_key != "shortcuts") return shortcuts;

    // Parse each shortcut entry
    while (pos < size) {
        const quint8 entry_type = readByte();
        if (entry_type == VdfType::EndSection) break;
        if (entry_type != VdfType::SubSection) break;

        readString(); // index string like "0", "1", etc.

        SteamShortcut sc;

        // Parse fields within this shortcut
        while (pos < size) {
            const quint8 field_type = readByte();
            if (field_type == VdfType::EndSection) break;

            const QByteArray key = readString();

            if (field_type == VdfType::String) {
                const QByteArray value = readString();
                if (key == "AppName" || key == "appname") {
                    sc.app_name = QString::fromUtf8(value);
                } else if (key == "Exe" || key == "exe") {
                    sc.exe = QString::fromUtf8(value);
                } else if (key == "StartDir" || key == "startdir") {
                    sc.start_dir = QString::fromUtf8(value);
                } else if (key == "icon") {
                    sc.icon = QString::fromUtf8(value);
                } else if (key == "ShortcutPath" || key == "shortcutpath") {
                    sc.shortcut_path = QString::fromUtf8(value);
                } else if (key == "LaunchOptions" || key == "launchoptions") {
                    sc.launch_options = QString::fromUtf8(value);
                }
            } else if (field_type == VdfType::Uint32) {
                const quint32 value = readUint32();
                if (key == "appid") {
                    sc.id = value;
                } else if (key == "IsHidden" || key == "ishidden") {
                    sc.is_hidden = (value != 0);
                } else if (key == "AllowDesktopConfig" || key == "allowdesktopconfig") {
                    sc.allow_desktop_config = (value != 0);
                } else if (key == "AllowOverlay" || key == "allowoverlay") {
                    sc.allow_overlay = (value != 0);
                } else if (key == "LastPlayTime" || key == "lastplaytime") {
                    sc.last_play_time = static_cast<qint32>(value);
                }
            } else if (field_type == VdfType::SubSection) {
                // Nested sub-section like "tags"
                const bool is_tags = (key == "tags");
                while (pos < size) {
                    const quint8 sub_type = readByte();
                    if (sub_type == VdfType::EndSection) break;
                    readString(); // sub-key (index)
                    if (sub_type == VdfType::String) {
                        const QByteArray tag_val = readString();
                        if (is_tags) {
                            sc.tags.append(QString::fromUtf8(tag_val));
                        }
                    } else if (sub_type == VdfType::Uint32) {
                        readUint32();
                    }
                }
            }
        }

        if (!sc.app_name.isEmpty()) {
            if (sc.id == 0) {
                sc.id = GenerateAppId(sc.exe, sc.app_name);
            }
            shortcuts.push_back(std::move(sc));
        }
    }

    return shortcuts;
}

void SteamIntegration::VdfWriteString(QByteArray& buf, quint8 type, const QByteArray& key,
                                       const QByteArray& value) const {
    buf.append(static_cast<char>(type));
    buf.append(key);
    buf.append('\0');
    buf.append(value);
    buf.append('\0');
}

void SteamIntegration::VdfWriteUint32(QByteArray& buf, const QByteArray& key,
                                       quint32 value) const {
    buf.append(static_cast<char>(VdfType::Uint32));
    buf.append(key);
    buf.append('\0');
    buf.append(static_cast<char>(value & 0xFF));
    buf.append(static_cast<char>((value >> 8) & 0xFF));
    buf.append(static_cast<char>((value >> 16) & 0xFF));
    buf.append(static_cast<char>((value >> 24) & 0xFF));
}

QByteArray SteamIntegration::SerializeShortcutsVdf(
    const std::vector<SteamShortcut>& shortcuts) const {
    QByteArray buf;

    // Root section: \x00 "shortcuts" \x00
    buf.append(static_cast<char>(VdfType::SubSection));
    buf.append("shortcuts");
    buf.append('\0');

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        const auto& sc = shortcuts[i];

        // Entry header: \x00 "<index>" \x00
        buf.append(static_cast<char>(VdfType::SubSection));
        buf.append(QByteArray::number(static_cast<int>(i)));
        buf.append('\0');

        VdfWriteUint32(buf, "appid", sc.id);
        VdfWriteString(buf, VdfType::String, "AppName", sc.app_name.toUtf8());
        VdfWriteString(buf, VdfType::String, "Exe", sc.exe.toUtf8());
        VdfWriteString(buf, VdfType::String, "StartDir", sc.start_dir.toUtf8());
        VdfWriteString(buf, VdfType::String, "icon", sc.icon.toUtf8());
        VdfWriteString(buf, VdfType::String, "ShortcutPath", sc.shortcut_path.toUtf8());
        VdfWriteString(buf, VdfType::String, "LaunchOptions", sc.launch_options.toUtf8());
        VdfWriteUint32(buf, "IsHidden", sc.is_hidden ? 1 : 0);
        VdfWriteUint32(buf, "AllowDesktopConfig", sc.allow_desktop_config ? 1 : 0);
        VdfWriteUint32(buf, "AllowOverlay", sc.allow_overlay ? 1 : 0);
        VdfWriteUint32(buf, "OpenVR", 0);
        VdfWriteUint32(buf, "Devkit", 0);
        VdfWriteString(buf, VdfType::String, "DevkitGameID", "");
        VdfWriteUint32(buf, "DevkitOverrideAppID", 0);
        VdfWriteUint32(buf, "LastPlayTime", static_cast<quint32>(sc.last_play_time));
        VdfWriteString(buf, VdfType::String, "FlatpakAppID", "");
        VdfWriteString(buf, VdfType::String, "LastPlayTime", "");

        // Tags sub-section
        buf.append(static_cast<char>(VdfType::SubSection));
        buf.append("tags");
        buf.append('\0');
        for (int t = 0; t < sc.tags.size(); ++t) {
            VdfWriteString(buf, VdfType::String, QByteArray::number(t),
                           sc.tags[t].toUtf8());
        }
        buf.append(static_cast<char>(VdfType::EndSection));

        buf.append(static_cast<char>(VdfType::EndSection)); // end shortcut entry
    }

    buf.append(static_cast<char>(VdfType::EndSection)); // end shortcuts section
    buf.append(static_cast<char>(VdfType::EndSection)); // end root

    return buf;
}

std::vector<SteamIntegration::SteamShortcut> SteamIntegration::ListShortcuts() const {
    const QString vdf_path = FindShortcutsVdf();
    if (vdf_path.isEmpty()) {
        return {};
    }

    QFile file(vdf_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }

    return ParseShortcutsVdf(file.readAll());
}

bool SteamIntegration::AddGameShortcut(const QString& game_title, const QString& rom_path,
                                       const QString& icon_path) {
    if (!IsSteamInstalled()) {
        return false;
    }

    const QString vdf_path = FindShortcutsVdf();
    if (vdf_path.isEmpty()) {
        return false;
    }

    // Read existing shortcuts
    std::vector<SteamShortcut> shortcuts;
    {
        QFile file(vdf_path);
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            shortcuts = ParseShortcutsVdf(file.readAll());
        }
    }

    // Check if shortcut with this title already exists
    for (const auto& sc : shortcuts) {
        if (sc.app_name == game_title) {
            return true; // Already present
        }
    }

    // Path to the currently running suyu executable
    const QString exe_path = QCoreApplication::applicationFilePath();

    SteamShortcut new_sc;
    new_sc.app_name = game_title;
    new_sc.exe = QStringLiteral("\"%1\"").arg(exe_path);
    new_sc.start_dir = QStringLiteral("\"%1\"").arg(QFileInfo(exe_path).absolutePath());
    new_sc.icon = icon_path;
    new_sc.launch_options = QStringLiteral("-g \"%1\"").arg(rom_path);
    new_sc.allow_desktop_config = true;
    new_sc.allow_overlay = true;
    new_sc.tags.append(QStringLiteral("SuyuEclipse"));
    new_sc.id = GenerateAppId(new_sc.exe, new_sc.app_name);

    shortcuts.push_back(std::move(new_sc));

    // Ensure config directory exists
    QDir().mkpath(QFileInfo(vdf_path).absolutePath());

    // Write the updated shortcuts.vdf
    QFile out_file(vdf_path);
    if (!out_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    out_file.write(SerializeShortcutsVdf(shortcuts));
    out_file.close();

    emit ShortcutAdded(game_title);
    return true;
}

bool SteamIntegration::RemoveGameShortcut(const QString& game_title) {
    if (!IsSteamInstalled()) {
        return false;
    }

    const QString vdf_path = FindShortcutsVdf();
    if (vdf_path.isEmpty()) {
        return false;
    }

    QFile file(vdf_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    auto shortcuts = ParseShortcutsVdf(file.readAll());
    file.close();

    // Remove matching shortcut
    auto it = std::remove_if(shortcuts.begin(), shortcuts.end(),
                             [&](const SteamShortcut& sc) { return sc.app_name == game_title; });
    if (it == shortcuts.end()) {
        return false; // Not found
    }
    shortcuts.erase(it, shortcuts.end());

    // Write back
    QFile out_file(vdf_path);
    if (!out_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    out_file.write(SerializeShortcutsVdf(shortcuts));
    out_file.close();

    emit ShortcutRemoved(game_title);
    return true;
}

namespace {

QString SteamStoreSearchUrl(const QString& game_title) {
    return QStringLiteral("https://store.steampowered.com/api/storesearch?term=%1&cc=us&l=en")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(game_title)));
}

QString SteamStoreArtworkUrl(quint64 app_id, SteamIntegration::ArtworkType artwork_type) {
    switch (artwork_type) {
        case SteamIntegration::ArtworkType::Hero:
            return QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/header.jpg").arg(app_id);
        case SteamIntegration::ArtworkType::Icon:
            return QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/capsule_184x69.jpg").arg(app_id);
        case SteamIntegration::ArtworkType::Artwork:
            return QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/capsule_616x353.jpg").arg(app_id);
        case SteamIntegration::ArtworkType::Grid:
        default:
            return QStringLiteral("https://cdn.cloudflare.steamstatic.com/steam/apps/%1/capsule_231x87.jpg").arg(app_id);
    }
}

QString NetworkReplyErrorString(QNetworkReply* reply) {
    const int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status_code >= 400) {
        return QStringLiteral("HTTP %1").arg(status_code);
    }
    return reply->errorString();
}

} // namespace

void SteamIntegration::FetchArtwork(const QString& game_title, const QString& output_path,
                                       ArtworkType artwork_type) {
    const QUrl search_url(SteamStoreSearchUrl(game_title));

    QNetworkRequest request(search_url);
    QNetworkReply* search_reply = network_manager_->get(request);
    connect(search_reply, &QNetworkReply::finished, this,
            [this, search_reply, game_title, output_path, artwork_type]() {
                search_reply->deleteLater();

                if (search_reply->error() != QNetworkReply::NoError) {
                    emit ArtworkFetchFailed(game_title, NetworkReplyErrorString(search_reply));
                    return;
                }

                const QJsonDocument doc = QJsonDocument::fromJson(search_reply->readAll());
                const QJsonArray items = doc.object()[QStringLiteral("items")].toArray();
                if (items.isEmpty()) {
                    emit ArtworkFetchFailed(game_title,
                                            QStringLiteral("No store match found on Steam Store"));
                    return;
                }

                const qint64 app_id = items[0].toObject()[QStringLiteral("id")].toVariant().toLongLong();
                if (app_id == 0) {
                    emit ArtworkFetchFailed(game_title,
                                            QStringLiteral("Steam Store returned an invalid app id"));
                    return;
                }

                const QUrl image_url = QUrl(SteamStoreArtworkUrl(static_cast<quint64>(app_id), artwork_type));
                QNetworkReply* img_reply = network_manager_->get(QNetworkRequest(image_url));
                connect(img_reply, &QNetworkReply::finished, this,
                        [this, img_reply, game_title, output_path]() {
                            img_reply->deleteLater();

                            if (img_reply->error() != QNetworkReply::NoError) {
                                emit ArtworkFetchFailed(game_title, NetworkReplyErrorString(img_reply));
                                return;
                            }

                            const int status_code = img_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                            if (status_code >= 400) {
                                emit ArtworkFetchFailed(game_title,
                                                        QStringLiteral("HTTP %1").arg(status_code));
                                return;
                            }

                            QFile file(output_path);
                            if (!file.open(QIODevice::WriteOnly)) {
                                emit ArtworkFetchFailed(game_title,
                                                        QStringLiteral("Cannot write to %1").arg(output_path));
                                return;
                            }
                            file.write(img_reply->readAll());
                            file.close();

                            emit ArtworkFetched(game_title, output_path);
                        });
            });
}
