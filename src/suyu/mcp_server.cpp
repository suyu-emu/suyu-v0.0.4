// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>

#include "common/fs/path_util.h"
#include "suyu/mcp_server.h"

namespace {

QJsonObject MakeSchema(const QJsonObject& properties,
                       const QJsonArray& required = QJsonArray()) {
    QJsonObject schema;
    schema[QStringLiteral("type")] = QStringLiteral("object");
    schema[QStringLiteral("properties")] = properties;
    if (!required.isEmpty()) {
        schema[QStringLiteral("required")] = required;
    }
    return schema;
}

QJsonObject MakeProp(const QString& type, const QString& description) {
    return QJsonObject{
        {QStringLiteral("type"), type},
        {QStringLiteral("description"), description},
    };
}

} // namespace

McpServer::McpServer(QObject* parent) : QObject(parent), server_(std::make_unique<QTcpServer>()) {
    connect(server_.get(), &QTcpServer::newConnection, this, &McpServer::OnNewConnection);
    RegisterBuiltinTools();
}

McpServer::~McpServer() {
    Stop();
}

bool McpServer::Start(quint16 port) {
    if (server_->isListening()) {
        return true;
    }

    // Bind explicitly to IPv4 loopback so local clients that use 127.0.0.1
    // can connect reliably on systems where LocalHost prefers IPv6.
    if (!server_->listen(QHostAddress(QStringLiteral("127.0.0.1")), port)) {
        return false;
    }
    port_ = server_->serverPort();
    return true;
}

void McpServer::Stop() {
    if (server_->isListening()) {
        server_->close();
    }
    port_ = 0;
}

bool McpServer::IsRunning() const {
    return server_->isListening();
}

quint16 McpServer::Port() const {
    return port_;
}

void McpServer::RegisterTool(const QString& name, const QString& description,
                             const QJsonObject& input_schema, ToolHandler handler) {
    tools_.append(ToolInfo{name, description, input_schema, std::move(handler)});
}

void McpServer::SetStateProvider(std::function<QJsonObject()> provider) {
    state_provider_ = std::move(provider);
}

void McpServer::RegisterBuiltinTools() {
    // 1) get_emulator_state — returns current emulator state snapshot
    RegisterTool(
        QStringLiteral("get_emulator_state"),
        QStringLiteral("Get the current emulator state including status, current ROM, and FPS."),
        MakeSchema({}), [this](const QJsonObject& /*params*/) -> QJsonObject {
            if (state_provider_) {
                return state_provider_();
            }
            return QJsonObject{
                {QStringLiteral("status"), QStringLiteral("idle")},
                {QStringLiteral("current_rom"), QStringLiteral("")},
                {QStringLiteral("fps"), 0},
                {QStringLiteral("uptime_seconds"), 0},
            };
        });

    RegisterTool(
        QStringLiteral("get_ui_state"),
        QStringLiteral("Get the current UI state and active view for the front-end."),
        MakeSchema({}), [this](const QJsonObject& /*params*/) -> QJsonObject {
            if (state_provider_) {
                return state_provider_();
            }
            return QJsonObject{};
        });

    // 2) get_rom_info — returns metadata about a ROM file
    RegisterTool(
        QStringLiteral("get_rom_info"),
        QStringLiteral("Get metadata about a ROM file (title, size, format)."),
        MakeSchema(
            {{QStringLiteral("path"),
              MakeProp(QStringLiteral("string"), QStringLiteral("Absolute path to the ROM file"))}},
            {QStringLiteral("path")}),
        [](const QJsonObject& params) -> QJsonObject {
            const QString path = params[QStringLiteral("path")].toString();
            QFileInfo info(path);
            if (!info.exists() || !info.isFile()) {
                return QJsonObject{
                    {QStringLiteral("error"), QStringLiteral("File not found")},
                };
            }

            const QString suffix = info.suffix().toLower();
            QString format = QStringLiteral("unknown");
            if (suffix == QStringLiteral("nsp")) {
                format = QStringLiteral("NSP (Nintendo Submission Package)");
            } else if (suffix == QStringLiteral("xci")) {
                format = QStringLiteral("XCI (NX Card Image)");
            } else if (suffix == QStringLiteral("nca")) {
                format = QStringLiteral("NCA (Nintendo Content Archive)");
            } else if (suffix == QStringLiteral("nro")) {
                format = QStringLiteral("NRO (Nintendo Relocatable Object)");
            } else if (suffix == QStringLiteral("nso")) {
                format = QStringLiteral("NSO (Nintendo Shared Object)");
            }

            return QJsonObject{
                {QStringLiteral("filename"), info.fileName()},
                {QStringLiteral("path"), info.absoluteFilePath()},
                {QStringLiteral("size_bytes"), info.size()},
                {QStringLiteral("size_human"),
                 QStringLiteral("%1 MB").arg(info.size() / (1024.0 * 1024.0), 0, 'f', 2)},
                {QStringLiteral("format"), format},
                {QStringLiteral("last_modified"),
                 info.lastModified().toString(Qt::ISODate)},
            };
        });

    // 3) list_save_states — list save state files in the emulator data dir
    RegisterTool(
        QStringLiteral("list_save_states"),
        QStringLiteral("List all save state files available for a given title ID."),
        MakeSchema(
            {{QStringLiteral("title_id"),
              MakeProp(QStringLiteral("string"),
                       QStringLiteral("Title ID to search save states for (hex string)"))}},
            {QStringLiteral("title_id")}),
        [](const QJsonObject& params) -> QJsonObject {
            const QString title_id = params[QStringLiteral("title_id")].toString();
            // Look in standard suyu save directory
            const QString data_dir =
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            const QDir save_dir(data_dir + QStringLiteral("/suyu/nand/user/save/"));

            QJsonArray states;
            if (save_dir.exists()) {
                // Recursively search for save files matching the title_id
                const auto entries = save_dir.entryInfoList(
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
                for (const auto& entry : entries) {
                    if (entry.fileName().contains(title_id, Qt::CaseInsensitive) ||
                        entry.absoluteFilePath().contains(title_id, Qt::CaseInsensitive)) {
                        QJsonObject state;
                        state[QStringLiteral("name")] = entry.fileName();
                        state[QStringLiteral("path")] = entry.absoluteFilePath();
                        state[QStringLiteral("size_bytes")] = entry.size();
                        state[QStringLiteral("modified")] =
                            entry.lastModified().toString(Qt::ISODate);
                        states.append(state);
                    }
                }
            }

            return QJsonObject{
                {QStringLiteral("title_id"), title_id},
                {QStringLiteral("save_directory"), save_dir.absolutePath()},
                {QStringLiteral("count"), states.size()},
                {QStringLiteral("states"), states},
            };
        });

    // 4) list_installed_titles — scan NAND for installed titles
    RegisterTool(
        QStringLiteral("list_installed_titles"),
        QStringLiteral("List all titles installed in the emulator NAND."),
        MakeSchema({}),
        [](const QJsonObject& /*params*/) -> QJsonObject {
            const QString data_dir =
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            const QDir content_dir(data_dir +
                                   QStringLiteral("/suyu/nand/user/Contents/registered/"));

            QJsonArray titles;
            if (content_dir.exists()) {
                const auto entries = content_dir.entryInfoList(
                    QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const auto& entry : entries) {
                    QJsonObject title;
                    title[QStringLiteral("name")] = entry.fileName();
                    title[QStringLiteral("path")] = entry.absoluteFilePath();
                    titles.append(title);
                }
            }

            return QJsonObject{
                {QStringLiteral("content_directory"), content_dir.absolutePath()},
                {QStringLiteral("count"), titles.size()},
                {QStringLiteral("titles"), titles},
            };
        });

    // 5) get_system_info — report emulator and host system information
    RegisterTool(
        QStringLiteral("get_system_info"),
        QStringLiteral("Get information about the host system and emulator build."),
        MakeSchema({}),
        [](const QJsonObject& /*params*/) -> QJsonObject {
            return QJsonObject{
                {QStringLiteral("emulator"), QStringLiteral("SuyuEclipse")},
                {QStringLiteral("qt_version"), QString::fromLatin1(qVersion())},
                {QStringLiteral("compile_qt_version"),
                 QString::fromLatin1(QT_VERSION_STR)},
                {QStringLiteral("os"), QSysInfo::prettyProductName()},
                {QStringLiteral("kernel"), QSysInfo::kernelVersion()},
                {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
                {QStringLiteral("app_dir"),
                 QCoreApplication::applicationDirPath()},
            };
        });

    // 6) list_game_directories — list configured ROM scan paths
    RegisterTool(
        QStringLiteral("list_game_directories"),
        QStringLiteral("List the configured directories where SuyuEclipse scans for games."),
        MakeSchema({}),
        [](const QJsonObject& /*params*/) -> QJsonObject {
            const QString data_dir =
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            const QDir suyu_dir(data_dir + QStringLiteral("/suyu/"));

            QJsonArray dirs;
            // Check for a gamedirs config (common location)
            const QString config_path = suyu_dir.filePath(QStringLiteral("config/qt-config.ini"));
            QFileInfo config_info(config_path);
            if (config_info.exists()) {
                QJsonObject entry;
                entry[QStringLiteral("config_file")] = config_info.absoluteFilePath();
                entry[QStringLiteral("exists")] = true;
                dirs.append(entry);
            }

            // Also report the NAND/SDMC paths
            const QStringList known_paths = {
                suyu_dir.filePath(QStringLiteral("nand/")),
                suyu_dir.filePath(QStringLiteral("sdmc/")),
                suyu_dir.filePath(QStringLiteral("load/")),
            };
            for (const auto& p : known_paths) {
                QJsonObject entry;
                entry[QStringLiteral("path")] = p;
                entry[QStringLiteral("exists")] = QDir(p).exists();
                dirs.append(entry);
            }

            return QJsonObject{
                {QStringLiteral("data_directory"), suyu_dir.absolutePath()},
                {QStringLiteral("directories"), dirs},
            };
        });

    // 7) get_keys_status — check if prod.keys/title.keys exist, and external tool status
    RegisterTool(
        QStringLiteral("get_keys_status"),
        QStringLiteral(
            "Check whether decryption keys (prod.keys, title.keys) are installed, "
            "and whether an external decryption tool is configured."),
        MakeSchema({}),
        [](const QJsonObject& /*params*/) -> QJsonObject {
            const QDir keys_dir(QString::fromStdString(
                Common::FS::GetSuyuPathString(Common::FS::SuyuPath::KeysDir)));

            const QFileInfo prod(keys_dir.filePath(QStringLiteral("prod.keys")));
            const QFileInfo title(keys_dir.filePath(QStringLiteral("title.keys")));

            // External tool status from QSettings
            QSettings settings;
            const QString ext_tool_id =
                settings.value(QStringLiteral("ExternalDecryption/ToolId")).toString();
            const QString ext_tool_path =
                settings.value(QStringLiteral("ExternalDecryption/ToolPath")).toString();
            const bool ext_tool_exists =
                !ext_tool_path.isEmpty() && QFileInfo::exists(ext_tool_path);

            {
                QJsonObject result;
                result[QStringLiteral("keys_directory")] = keys_dir.absolutePath();
                result[QStringLiteral("prod_keys_present")] = prod.exists();
                result[QStringLiteral("prod_keys_size")] = prod.exists() ? prod.size() : 0;
                result[QStringLiteral("title_keys_present")] = title.exists();
                result[QStringLiteral("title_keys_size")] = title.exists() ? title.size() : 0;
                result[QStringLiteral("external_tool_id")] = ext_tool_id;
                result[QStringLiteral("external_tool_path")] = ext_tool_path;
                result[QStringLiteral("external_tool_configured")] = ext_tool_exists;
                result[QStringLiteral("note")] = QStringLiteral(
                    "Built-in key loading is supported. You can install prod.keys/title.keys locally, "
                    "or configure an external decryption tool if you prefer.");
                return result;
            }
        });

    // 8) get_log_tail — read last N lines from the emulator log
    RegisterTool(
        QStringLiteral("get_log_tail"),
        QStringLiteral("Read the last N lines of the emulator log file."),
        MakeSchema(
            {{QStringLiteral("lines"),
              MakeProp(QStringLiteral("integer"),
                       QStringLiteral("Number of lines to read from the end (default 50)"))}},
            QJsonArray()),
        [](const QJsonObject& params) -> QJsonObject {
            const int max_lines = params[QStringLiteral("lines")].toInt(50);
            const int clamped = qBound(1, max_lines, 500);

            const QString data_dir =
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            const QString log_path = data_dir + QStringLiteral("/suyu/log/suyu_log.txt");

            QFile file(log_path);
            if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return QJsonObject{
                    {QStringLiteral("error"),
                     QStringLiteral("Log file not found or unreadable")},
                    {QStringLiteral("path"), log_path},
                };
            }

            // Read all lines and take the tail
            QStringList all_lines;
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                all_lines.append(stream.readLine());
            }
            file.close();

            const int start = qMax(0, all_lines.size() - clamped);
            QJsonArray lines_arr;
            for (int i = start; i < all_lines.size(); ++i) {
                lines_arr.append(all_lines[i]);
            }

            return QJsonObject{
                {QStringLiteral("path"), log_path},
                {QStringLiteral("total_lines"), all_lines.size()},
                {QStringLiteral("returned_lines"), lines_arr.size()},
                {QStringLiteral("lines"), lines_arr},
            };
        });
}

void McpServer::OnNewConnection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket* socket = server_->nextPendingConnection();
        const QString address = socket->peerAddress().toString() + QStringLiteral(":") +
                                QString::number(socket->peerPort());

        emit ClientConnected(address);

        connect(socket, &QTcpSocket::readyRead, this, &McpServer::OnReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, [this, address, socket]() {
            emit ClientDisconnected(address);
            socket->deleteLater();
        });
    }
}

void McpServer::OnReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    const QByteArray data = socket->readAll();
    HandleRequest(data, socket);
}

void McpServer::HandleRequest(const QByteArray& data, QTcpSocket* socket) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        response[QStringLiteral("error")] =
            QJsonObject{{QStringLiteral("code"), -32700},
                        {QStringLiteral("message"), QStringLiteral("Parse error")}};
    } else {
        const QJsonObject request = doc.object();
        const QString method = request[QStringLiteral("method")].toString();
        const auto id = request[QStringLiteral("id")];
        response[QStringLiteral("id")] = id;

        emit RequestReceived(method);

        if (method == QStringLiteral("initialize")) {
            QJsonObject result;
            result[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");

            QJsonObject capabilities;
            capabilities[QStringLiteral("tools")] = QJsonObject{};
            result[QStringLiteral("capabilities")] = capabilities;

            QJsonObject server_info;
            server_info[QStringLiteral("name")] = QStringLiteral("SuyuEclipse MCP");
            server_info[QStringLiteral("version")] = QStringLiteral("0.1.0");
            result[QStringLiteral("serverInfo")] = server_info;

            response[QStringLiteral("result")] = result;

        } else if (method == QStringLiteral("notifications/initialized")) {
            // Client acknowledgement — no response needed, but we send empty result
            response[QStringLiteral("result")] = QJsonObject{};

        } else if (method == QStringLiteral("tools/list")) {
            QJsonArray tool_array;
            for (const auto& tool : tools_) {
                QJsonObject t;
                t[QStringLiteral("name")] = tool.name;
                t[QStringLiteral("description")] = tool.description;
                t[QStringLiteral("inputSchema")] = tool.input_schema;
                tool_array.append(t);
            }
            QJsonObject result;
            result[QStringLiteral("tools")] = tool_array;
            response[QStringLiteral("result")] = result;

        } else if (method == QStringLiteral("tools/call")) {
            const QJsonObject params = request[QStringLiteral("params")].toObject();
            const QString tool_name = params[QStringLiteral("name")].toString();
            const QJsonObject arguments = params[QStringLiteral("arguments")].toObject();

            // Find the registered tool
            const ToolInfo* found = nullptr;
            for (const auto& tool : tools_) {
                if (tool.name == tool_name) {
                    found = &tool;
                    break;
                }
            }

            if (!found) {
                response[QStringLiteral("error")] = QJsonObject{
                    {QStringLiteral("code"), -32602},
                    {QStringLiteral("message"),
                     QStringLiteral("Unknown tool: %1").arg(tool_name)},
                };
            } else {
                const QJsonObject tool_result = found->handler(arguments);

                // MCP tools/call returns content array with text type
                QJsonArray content;
                QJsonObject text_content;
                text_content[QStringLiteral("type")] = QStringLiteral("text");
                text_content[QStringLiteral("text")] =
                    QString::fromUtf8(
                        QJsonDocument(tool_result).toJson(QJsonDocument::Indented));
                content.append(text_content);

                QJsonObject result;
                result[QStringLiteral("content")] = content;
                response[QStringLiteral("result")] = result;
            }

        } else if (method == QStringLiteral("ping")) {
            response[QStringLiteral("result")] = QJsonObject{};

        } else {
            response[QStringLiteral("error")] =
                QJsonObject{{QStringLiteral("code"), -32601},
                            {QStringLiteral("message"), QStringLiteral("Method not found")}};
        }
    }

    const QByteArray response_data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    socket->write(response_data);
    socket->write("\n");
    socket->flush();
}
