// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class QTcpServer;
class QTcpSocket;

/// Model Context Protocol (MCP) server for game assistance and troubleshooting.
/// Exposes emulator state, ROM metadata, and diagnostic info over a local JSON-RPC endpoint.
class McpServer : public QObject {
    Q_OBJECT

public:
    explicit McpServer(QObject* parent = nullptr);
    ~McpServer() override;

    /// Start the MCP server on the given port.
    bool Start(quint16 port = 9742);

    /// Stop the server.
    void Stop();

    /// Returns true if the server is actively listening.
    [[nodiscard]] bool IsRunning() const;

    /// Returns the last socket error reported by the underlying server.
    [[nodiscard]] QString GetLastErrorString() const;

    /// Get the port the server is listening on.
    [[nodiscard]] quint16 Port() const;

    /// Callback type for tool implementations: receives params, returns result JSON.
    using ToolHandler = std::function<QJsonObject(const QJsonObject& params)>;

    /// Description of a registered MCP tool.
    struct ToolInfo {
        QString name;
        QString description;
        QJsonObject input_schema;
        ToolHandler handler;
    };

    /// Register a tool with the server.
    void RegisterTool(const QString& name, const QString& description,
                      const QJsonObject& input_schema, ToolHandler handler);

    /// Provide a callback to query current emulator state.
    void SetStateProvider(std::function<QJsonObject()> provider);

signals:
    void ClientConnected(const QString& address);
    void ClientDisconnected(const QString& address);
    void RequestReceived(const QString& method);

private slots:
    void OnNewConnection();
    void OnReadyRead();

private:
    void HandleRequest(const QByteArray& data, QTcpSocket* socket);
    void RegisterBuiltinTools();

    std::unique_ptr<QTcpServer> server_;
    QList<ToolInfo> tools_;
    std::function<QJsonObject()> state_provider_;
    quint16 port_{0};
};
