// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QSplitter;
class QProcess;
class QPushButton;
class QLabel;
class QTabWidget;
class QComboBox;
class QLineEdit;

/// Full IDE-style programmer environment — VS Code-like layout with liquid glass theme.
class ProgrammerEnvironment : public QWidget {
    Q_OBJECT

public:
    explicit ProgrammerEnvironment(QWidget* parent = nullptr);
    ~ProgrammerEnvironment() override;

    void SetProjectRoot(const QString& path);
    void SetEmulatorPath(const QString& path);
    void SetRomPath(const QString& path);

public slots:
    void OnFileSelected();
    void OnBuildClicked();
    void OnRunClicked();
    void OnStopClicked();
    void OnDebugClicked();
    void OnOpenProjectClicked();
    void OnSaveAllClicked();
    void OnCompilerOptionsClicked();
    void OnTerminalInputSubmitted();

private:
    void SetupUi();
    void StartBuildProcess();
    void PopulateTree(QTreeWidgetItem* parent, const QString& path, int depth);
    void AppendOutput(QTextEdit* target, const QString& text, const QColor& color);

    // Toolbar
    QPushButton* btn_open_project_{};
    QPushButton* btn_save_all_{};
    QPushButton* btn_compiler_options_{};
    QPushButton* btn_build_{};
    QPushButton* btn_debug_{};
    QPushButton* btn_run_{};
    QPushButton* btn_stop_{};
    QComboBox*   build_config_{};
    QLabel*      status_label_{};

    // Left panel
    QTreeWidget* project_tree_{};

    // Center: code editor
    QTextEdit*   code_editor_{};
    QLabel*      editor_title_{};

    // Right top: game view
    QWidget*     game_view_widget_{};

    // Right bottom: inspector tabs
    QTabWidget*  inspector_tabs_{};
    QTextEdit*   inspector_props_{};
    QTextEdit*   hierarchy_view_{};
    QTextEdit*   console_view_{};

    // Bottom panel tabs
    QTabWidget*  bottom_tabs_{};
    QTextEdit*   problems_view_{};
    QTextEdit*   build_output_{};
    QTextEdit*   terminal_view_{};
    QLineEdit*   terminal_input_{};
    QTextEdit*   debug_console_{};

    // Processes
    QProcess*    build_process_{};
    QProcess*    run_process_{};

    // State
    QString project_root_;
    QString emulator_path_;
    QString rom_path_;
    QString current_file_path_;
    bool    debug_mode_{false};

    static constexpr int kMaxTreeDepth = 6;
};
