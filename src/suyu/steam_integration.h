// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

/// Steam library integration — adds games to Steam as non-Steam shortcuts with artwork.
/// Reads and writes Steam's binary VDF shortcuts.vdf format directly.
class SteamIntegration : public QObject {
    Q_OBJECT

public:
    explicit SteamIntegration(QObject* parent = nullptr);
    ~SteamIntegration() override;

    /// Detect whether Steam is installed.
    [[nodiscard]] bool IsSteamInstalled() const;

    /// Get the Steam userdata directory path.
    [[nodiscard]] QString GetSteamUserdataPath() const;

    /// Add a game as a non-Steam shortcut.
    bool AddGameShortcut(const QString& game_title, const QString& rom_path,
                         const QString& icon_path = {});

    /// Remove a previously added shortcut.
    bool RemoveGameShortcut(const QString& game_title);

    enum class ArtworkType {
        Grid,
        Hero,
        Icon,
        Artwork,
    };

    /// Fetch artwork for a game title using public Steam Store endpoints.
    /// Downloads asynchronously; emits ArtworkFetched on completion.
    void FetchArtwork(const QString& game_title, const QString& output_path,
                      ArtworkType artwork_type = ArtworkType::Grid);

    struct SteamShortcut {
        quint32 id{};
        QString app_name;
        QString exe;
        QString start_dir;
        QString icon;
        QString shortcut_path;
        QString launch_options;
        bool is_hidden{false};
        bool allow_desktop_config{true};
        bool allow_overlay{true};
        qint32 last_play_time{0};
        QStringList tags;
    };

    /// List all currently registered shortcuts.
    [[nodiscard]] std::vector<SteamShortcut> ListShortcuts() const;

    /// Compute the Steam AppID for a non-Steam shortcut.
    [[nodiscard]] static quint32 GenerateAppId(const QString& exe, const QString& app_name);

signals:
    void ShortcutAdded(const QString& title);
    void ShortcutRemoved(const QString& title);
    void ArtworkFetched(const QString& title, const QString& path);
    void ArtworkFetchFailed(const QString& title, const QString& error);

private:
    /// Find the first user's shortcuts.vdf path.
    [[nodiscard]] QString FindShortcutsVdf() const;

    /// Parse Steam binary VDF shortcuts.vdf into a list of shortcuts.
    [[nodiscard]] std::vector<SteamShortcut> ParseShortcutsVdf(const QByteArray& data) const;

    /// Serialize a list of shortcuts back into binary VDF format.
    [[nodiscard]] QByteArray SerializeShortcutsVdf(const std::vector<SteamShortcut>& shortcuts) const;

    /// Write VDF byte helpers.
    void VdfWriteString(QByteArray& buf, quint8 type, const QByteArray& key,
                        const QByteArray& value) const;
    void VdfWriteUint32(QByteArray& buf, const QByteArray& key, quint32 value) const;

    [[nodiscard]] QString FindSteamPath() const;

    QString steam_path_;
    QNetworkAccessManager* network_manager_{};
};
