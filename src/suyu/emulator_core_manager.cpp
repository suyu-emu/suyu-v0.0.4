// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QProcess>
#include <QTcpSocket>

#include "suyu/emulator_core_manager.h"

EmulatorCoreManager::EmulatorCoreManager(QObject* parent) : QObject(parent) {
    // Always have the native core available
    CoreInfo native{};
    native.type = CoreType::Native;
    native.name = QStringLiteral("SuyuEclipse");
    native.version = QStringLiteral("1.0");
    native.available = true;
    cores_.push_back(native);
}

EmulatorCoreManager::~EmulatorCoreManager() {
    StopGame();
    DisconnectTroppical();
}

void EmulatorCoreManager::ScanCores() {
    // Keep native core, remove previously scanned external cores
    if (cores_.size() > 1) {
        cores_.erase(cores_.begin() + 1, cores_.end());
    }

    // Scan for libretro cores in common directories
    QStringList search_paths;
#ifdef _WIN32
    search_paths << QStringLiteral("cores");
    search_paths << QDir::homePath() + QStringLiteral("/.config/retroarch/cores");
#elif defined(__APPLE__)
    search_paths << QDir::homePath() + QStringLiteral("/.config/retroarch/cores");
    search_paths << QStringLiteral("/usr/lib/libretro");
#else
    search_paths << QDir::homePath() + QStringLiteral("/.config/retroarch/cores");
    search_paths << QStringLiteral("/usr/lib/libretro");
    search_paths << QStringLiteral("/usr/lib64/libretro");
#endif

    for (const auto& path : search_paths) {
        QDir dir(path);
        if (!dir.exists()) {
            continue;
        }
#ifdef _WIN32
        const auto entries = dir.entryInfoList({QStringLiteral("*.dll")}, QDir::Files);
#elif defined(__APPLE__)
        const auto entries = dir.entryInfoList({QStringLiteral("*.dylib")}, QDir::Files);
#else
        const auto entries = dir.entryInfoList({QStringLiteral("*.so")}, QDir::Files);
#endif
        for (const auto& entry : entries) {
            CoreInfo info{};
            info.type = CoreType::Libretro;
            info.name = entry.baseName();
            info.path = entry.absoluteFilePath();
            info.available = true;
            cores_.push_back(info);
        }
    }

    // Also scan for Troppical if not already listed
    // Check common Troppical install paths
    QStringList troppical_paths;
#ifdef _WIN32
    troppical_paths << QDir::homePath() + QStringLiteral("/AppData/Local/Troppical/troppical.exe");
    troppical_paths << QStringLiteral("C:/Program Files/Troppical/troppical.exe");
#elif defined(__APPLE__)
    troppical_paths << QStringLiteral("/Applications/Troppical.app/Contents/MacOS/Troppical");
#else
    troppical_paths << QStringLiteral("/usr/bin/troppical");
    troppical_paths << QDir::homePath() + QStringLiteral("/.local/bin/troppical");
#endif

    for (const auto& tp : troppical_paths) {
        if (QFileInfo::exists(tp)) {
            CoreInfo info{};
            info.type = CoreType::Troppical;
            info.name = QStringLiteral("Troppical");
            info.path = tp;
            info.version = QStringLiteral("bridge");
            info.available = true;
            cores_.push_back(info);
            break;
        }
    }

    emit CoresScanned(static_cast<int>(cores_.size()));
}

std::vector<EmulatorCoreManager::CoreInfo> EmulatorCoreManager::AvailableCores() const {
    return cores_;
}

bool EmulatorCoreManager::SelectCore(const QString& name) {
    for (size_t i = 0; i < cores_.size(); ++i) {
        if (cores_[i].name == name && cores_[i].available) {
            selected_index_ = static_cast<int>(i);
            emit CoreSelected(name);
            return true;
        }
    }
    return false;
}

EmulatorCoreManager::CoreInfo EmulatorCoreManager::SelectedCore() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(cores_.size())) {
        return cores_[selected_index_];
    }
    return cores_.front();
}

bool EmulatorCoreManager::LoadLibretroCore(const QString& path) {
    QLibrary lib(path);
    if (!lib.load()) {
        emit CoreLoadError(lib.errorString());
        return false;
    }

    // Verify it's a libretro core by checking for retro_api_version
    auto* api_version = reinterpret_cast<unsigned (*)()>(lib.resolve("retro_api_version"));
    if (!api_version) {
        emit CoreLoadError(QStringLiteral("Not a valid libretro core: missing retro_api_version"));
        lib.unload();
        return false;
    }

    // Also verify retro_get_system_info exists
    using SystemInfoFn = void (*)(void*);
    auto* get_info = reinterpret_cast<SystemInfoFn>(lib.resolve("retro_get_system_info"));

    CoreInfo info{};
    info.type = CoreType::Libretro;
    info.name = QFileInfo(path).baseName();
    info.path = path;
    info.version = QString::number(api_version());
    info.available = (get_info != nullptr);
    cores_.push_back(info);

    lib.unload(); // unload for now, will re-load when actually running
    emit CoresScanned(static_cast<int>(cores_.size()));
    return true;
}

bool EmulatorCoreManager::ConnectTroppical(const QString& endpoint) {
    DisconnectTroppical();

    // Parse endpoint as host:port (default port 15151)
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 15151;

    if (!endpoint.isEmpty()) {
        const int colon = endpoint.lastIndexOf(QLatin1Char(':'));
        if (colon > 0) {
            host = endpoint.left(colon);
            bool ok = false;
            const int p = endpoint.mid(colon + 1).toInt(&ok);
            if (ok && p > 0 && p <= 65535) {
                port = static_cast<quint16>(p);
            }
        } else {
            host = endpoint;
        }
    }

    troppical_socket_ = std::make_unique<QTcpSocket>();
    troppical_socket_->connectToHost(host, port);

    if (!troppical_socket_->waitForConnected(3000)) {
        emit CoreLoadError(
            QStringLiteral("Failed to connect to Troppical at %1:%2 - %3")
                .arg(host)
                .arg(port)
                .arg(troppical_socket_->errorString()));
        troppical_socket_.reset();
        return false;
    }

    troppical_endpoint_ = QStringLiteral("%1:%2").arg(host).arg(port);

    connect(troppical_socket_.get(), &QTcpSocket::disconnected, this, [this]() {
        troppical_endpoint_.clear();
        emit TroppicalDisconnected();
    });

    // Send a handshake message
    QJsonObject handshake;
    handshake[QStringLiteral("type")] = QStringLiteral("handshake");
    handshake[QStringLiteral("client")] = QStringLiteral("SuyuEclipse");
    handshake[QStringLiteral("version")] = QStringLiteral("1.0");
    const QByteArray msg = QJsonDocument(handshake).toJson(QJsonDocument::Compact) + "\n";
    troppical_socket_->write(msg);
    troppical_socket_->flush();

    // Register a Troppical core entry if not already present
    bool found = false;
    for (const auto& c : cores_) {
        if (c.type == CoreType::Troppical && c.name == QStringLiteral("Troppical")) {
            found = true;
            break;
        }
    }
    if (!found) {
        CoreInfo info{};
        info.type = CoreType::Troppical;
        info.name = QStringLiteral("Troppical");
        info.version = QStringLiteral("bridge");
        info.path = troppical_endpoint_;
        info.available = true;
        cores_.push_back(info);
    }

    emit TroppicalConnected();
    return true;
}

void EmulatorCoreManager::DisconnectTroppical() {
    if (troppical_socket_) {
        if (troppical_socket_->state() == QAbstractSocket::ConnectedState) {
            // Send disconnect message
            QJsonObject msg;
            msg[QStringLiteral("type")] = QStringLiteral("disconnect");
            troppical_socket_->write(
                QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
            troppical_socket_->flush();
            troppical_socket_->disconnectFromHost();
        }
        troppical_socket_.reset();
    }
    troppical_endpoint_.clear();
}

bool EmulatorCoreManager::IsTroppicalConnected() const {
    return troppical_socket_ &&
           troppical_socket_->state() == QAbstractSocket::ConnectedState;
}

bool EmulatorCoreManager::LaunchGame(const QString& rom_path) {
    if (rom_path.isEmpty() || !QFileInfo::exists(rom_path)) {
        emit CoreLoadError(QStringLiteral("ROM file not found: %1").arg(rom_path));
        return false;
    }

    StopGame();

    const CoreInfo core = SelectedCore();

    switch (core.type) {
    case CoreType::Native: {
        // For native core, launch the emulator binary with the ROM
        const QString exe = QCoreApplication::applicationFilePath();
        game_process_ = std::make_unique<QProcess>();
        game_process_->setProgram(exe);
        game_process_->setArguments({QStringLiteral("-g"), rom_path});

        connect(game_process_.get(),
                qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
                    game_process_.reset();
                    emit GameStopped();
                });

        game_process_->start();
        if (!game_process_->waitForStarted(5000)) {
            emit CoreLoadError(
                QStringLiteral("Failed to start native core: %1")
                    .arg(game_process_->errorString()));
            game_process_.reset();
            return false;
        }
        emit GameLaunched(rom_path);
        return true;
    }

    case CoreType::Libretro: {
        // Launch via RetroArch with the core and ROM
        QString retroarch;
#ifdef _WIN32
        retroarch = QStringLiteral("retroarch.exe");
#else
        retroarch = QStringLiteral("retroarch");
#endif
        game_process_ = std::make_unique<QProcess>();
        game_process_->setProgram(retroarch);
        game_process_->setArguments(
            {QStringLiteral("-L"), core.path, rom_path});

        connect(game_process_.get(),
                qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
                    game_process_.reset();
                    emit GameStopped();
                });

        game_process_->start();
        if (!game_process_->waitForStarted(5000)) {
            emit CoreLoadError(
                QStringLiteral("Failed to start RetroArch with core %1: %2")
                    .arg(core.name, game_process_->errorString()));
            game_process_.reset();
            return false;
        }
        emit GameLaunched(rom_path);
        return true;
    }

    case CoreType::Troppical: {
        if (!IsTroppicalConnected()) {
            emit CoreLoadError(
                QStringLiteral("Not connected to Troppical. Call ConnectTroppical() first."));
            return false;
        }

        // Send launch command via Troppical bridge
        QJsonObject launch_cmd;
        launch_cmd[QStringLiteral("type")] = QStringLiteral("launch");
        launch_cmd[QStringLiteral("rom_path")] = rom_path;
        launch_cmd[QStringLiteral("core")] = QStringLiteral("switch");

        const QByteArray msg =
            QJsonDocument(launch_cmd).toJson(QJsonDocument::Compact) + "\n";
        troppical_socket_->write(msg);
        troppical_socket_->flush();

        emit GameLaunched(rom_path);
        return true;
    }
    }

    return false;
}

void EmulatorCoreManager::StopGame() {
    if (game_process_) {
        game_process_->terminate();
        if (!game_process_->waitForFinished(3000)) {
            game_process_->kill();
            game_process_->waitForFinished(1000);
        }
        game_process_.reset();
        emit GameStopped();
    }

    // Send stop to Troppical if connected
    if (IsTroppicalConnected()) {
        QJsonObject stop_cmd;
        stop_cmd[QStringLiteral("type")] = QStringLiteral("stop");
        troppical_socket_->write(
            QJsonDocument(stop_cmd).toJson(QJsonDocument::Compact) + "\n");
        troppical_socket_->flush();
    }
}

bool EmulatorCoreManager::IsGameRunning() const {
    if (game_process_ && game_process_->state() == QProcess::Running) {
        return true;
    }
    return false;
}
