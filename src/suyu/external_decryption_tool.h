// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QGroupBox;
class QProcess;

/// Represents a known external decryption tool that SuyuEclipse can hook into.
struct ExternalToolInfo {
    QString id;           // e.g. "hactool", "ryujinx", "eden", "yuzu", "suyu"
    QString display_name; // e.g. "hactool (SciresM)"
    QString description;  // Brief description of what it does
    /// Expected executable names on disk (e.g. {"hactool.exe", "hactool"}).
    QStringList exe_names;
};

/// Manages configuration and invocation of external decryption tools.
/// SuyuEclipse does NOT perform any built-in decryption. When a game requires
/// decryption, the user must configure an external tool (hactool, Ryujinx,
/// Eden, yuzu, or any previous suyu build) and SuyuEclipse will invoke it.
class ExternalDecryptionTool : public QObject {
    Q_OBJECT

public:
    explicit ExternalDecryptionTool(QObject* parent = nullptr);
    ~ExternalDecryptionTool() override;

    /// Returns the list of known tools that can be configured.
    [[nodiscard]] static std::vector<ExternalToolInfo> KnownTools();

    /// Returns the currently configured tool ID, or empty if none.
    [[nodiscard]] QString ConfiguredToolId() const;

    /// Returns the path to the configured tool executable.
    [[nodiscard]] QString ConfiguredToolPath() const;

    /// Returns true if a valid tool is configured and the executable exists.
    [[nodiscard]] bool IsToolConfigured() const;

    /// Persist the selected tool and path to QSettings.
    void SetTool(const QString& tool_id, const QString& exe_path);

    /// Clear the configured tool.
    void ClearTool();

    /// Decrypt an NCA file using the configured external tool.
    /// @param nca_path  Path to the .nca file to decrypt.
    /// @param output_dir  Directory to write decrypted output.
    /// @param keys_path  Optional path to prod.keys (empty = tool default).
    /// @return true on success.
    bool DecryptNca(const QString& nca_path, const QString& output_dir,
                    const QString& keys_path = {});

    /// Extract ExeFS from an NCA using the configured external tool.
    /// @param nca_path  Path to the .nca file.
    /// @param output_dir  Directory to write extracted ExeFS.
    /// @param keys_path  Optional path to prod.keys.
    /// @return true on success.
    bool ExtractExeFs(const QString& nca_path, const QString& output_dir,
                      const QString& keys_path = {});

    /// Decrypt an NSP/XCI using the configured external tool.
    /// @param rom_path  Path to the .nsp or .xci file.
    /// @param output_dir  Directory to write decrypted output.
    /// @param keys_path  Optional path to prod.keys.
    /// @return true on success.
    bool DecryptRom(const QString& rom_path, const QString& output_dir,
                    const QString& keys_path = {});

    /// Returns the last error message from a tool invocation.
    [[nodiscard]] QString LastError() const;

signals:
    void ToolChanged(const QString& tool_id);
    void DecryptionStarted(const QString& file);
    void DecryptionFinished(const QString& file, bool success);
    void DecryptionProgress(const QString& status_message);

private:
    /// Run the external tool with the given arguments and wait for completion.
    bool RunTool(const QStringList& args, int timeout_ms = 120000);

    /// Build hactool-compatible arguments for the given operation.
    QStringList BuildHactoolArgs(const QString& input, const QString& output_dir,
                                 const QString& keys_path, const QString& operation);

    QString tool_id_;
    QString tool_path_;
    QString last_error_;
};

// ---------------------------------------------------------------------------
// Dialog for the user to select / configure an external decryption tool.
// ---------------------------------------------------------------------------

class ExternalDecryptionToolDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExternalDecryptionToolDialog(ExternalDecryptionTool* tool,
                                          QWidget* parent = nullptr);
    ~ExternalDecryptionToolDialog() override;

private slots:
    void OnBrowse();
    void OnToolSelected(int index);
    void OnAccept();
    void OnAutoDetect();

private:
    void SetupUi();
    void RefreshStatus();

    ExternalDecryptionTool* tool_;
    QComboBox* combo_tool_{};
    QLineEdit* edit_path_{};
    QPushButton* btn_browse_{};
    QPushButton* btn_auto_detect_{};
    QLabel* lbl_status_{};
    QLabel* lbl_description_{};
    QPushButton* btn_ok_{};
    QPushButton* btn_cancel_{};
};
