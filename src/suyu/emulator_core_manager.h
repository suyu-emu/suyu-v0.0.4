// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class QProcess;
class QTcpSocket;

/// Manages multiple emulator backends (native, libretro cores, troppical).
class EmulatorCoreManager : public QObject {
    Q_OBJECT

public:
    enum class CoreType {
        Native,   ///< Built-in SuyuEclipse core
        Libretro, ///< libretro-compatible core loaded as shared library
        Troppical ///< Troppical app bridge
    };

    struct CoreInfo {
        CoreType type;
        QString name;
        QString version;
        QString path;
        bool available;
    };

    explicit EmulatorCoreManager(QObject* parent = nullptr);
    ~EmulatorCoreManager() override;

    /// Scan for available cores in known directories.
    void ScanCores();

    /// List all discovered cores.
    [[nodiscard]] std::vector<CoreInfo> AvailableCores() const;

    /// Select a core by name for subsequent game launches.
    bool SelectCore(const QString& name);

    /// Get the currently selected core.
    [[nodiscard]] CoreInfo SelectedCore() const;

    /// Load a libretro core from a shared library path.
    bool LoadLibretroCore(const QString& path);

    /// Connect to a Troppical app instance via TCP.
    bool ConnectTroppical(const QString& endpoint);

    /// Disconnect from the current Troppical instance.
    void DisconnectTroppical();

    /// Returns true if connected to Troppical.
    [[nodiscard]] bool IsTroppicalConnected() const;

    /// Launch a game ROM with the currently selected core.
    bool LaunchGame(const QString& rom_path);

    /// Stop the currently running game.
    void StopGame();

    /// Returns true if a game is currently running.
    [[nodiscard]] bool IsGameRunning() const;

signals:
    void CoreSelected(const QString& name);
    void CoresScanned(int count);
    void CoreLoadError(const QString& error);
    void TroppicalConnected();
    void TroppicalDisconnected();
    void GameLaunched(const QString& rom_path);
    void GameStopped();

private:
    std::vector<CoreInfo> cores_;
    int selected_index_{0};
    std::unique_ptr<QTcpSocket> troppical_socket_;
    std::unique_ptr<QProcess> game_process_;
    QString troppical_endpoint_;
};
