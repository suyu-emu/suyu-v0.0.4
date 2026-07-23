// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QTabWidget;
class QTextEdit;
class QTreeWidget;
class QTableWidget;
class QSplitter;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QComboBox;

/// Task-manager-like environment for Hacker mode.
/// Provides tabs for memory inspection, log console, MCP tools,
/// process monitoring, and system diagnostics — ideal for
/// decompilation projects and mods.
class HackerEnvironment : public QWidget {
    Q_OBJECT

public:
    explicit HackerEnvironment(QWidget* parent = nullptr);
    ~HackerEnvironment() override;

    /// Append a line to the log console tab.
    void AppendLog(const QString& message);

    /// Refresh dynamic data (processes, memory, system info).
    void Refresh();

signals:
    void ToolInvoked(const QString& tool_name, const QString& args);
    void MemoryAddressSelected(quint64 address);
    void ExportRecompiledSource(const QString& output_dir, bool source_only);

private:
    void SetupUI();
    QWidget* CreateProcessMonitorTab();
    QWidget* CreateMemoryViewerTab();
    QWidget* CreateLogConsoleTab();
    QWidget* CreateMcpToolsTab();
    QWidget* CreateSystemInfoTab();
    QWidget* CreateRecompileTab();

    void RefreshProcesses();
    void RefreshMemory();
    void RefreshSystemInfo();

    // Tabs
    QTabWidget* tab_widget_{};

    // Process monitor
    QTableWidget* process_table_{};
    QTimer* process_timer_{};

    // Memory viewer
    QTableWidget* memory_table_{};
    QLineEdit* address_input_{};
    QComboBox* region_combo_{};

    // Log console
    QTextEdit* log_output_{};
    QLineEdit* command_input_{};

    // MCP tools
    QTreeWidget* tool_tree_{};
    QTextEdit* tool_output_{};
    QLineEdit* tool_args_input_{};

    // System info
    QTreeWidget* system_tree_{};

    // Recompile export
    QTextEdit* recomp_output_{};
    QLineEdit* recomp_path_input_{};
    QComboBox* recomp_platform_{};
    QComboBox* recomp_mode_{};
};
