// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QVector>

/**
 * Dialog for exporting a game as a native-export artifact bundle using
 * ahead-of-time (AOT) static recompilation.
 *
 * Pipeline:
 *   1. Extract ExeFS/RomFS from the ROM container (NSP/XCI/NCA)
 *   2. Translate ARM64 code blocks into Dynarmic IR compiler artifacts
 *   3. Package IR dumps, guest code slices, block maps, and game data
 *   4. Generate a platform-specific export bundle for a future custom runtime
 *
 * Dynarmic does not expose a stable block-serialization API for ready-made
 * host machine code export, so suyu serializes a frontend boundary instead:
 * translated IR dumps plus guest code slices and metadata. These artifacts are
 * the input for a future minimal runtime/codegen stage.
 */
class GameExportDialog : public QDialog {
    Q_OBJECT

public:
    struct LibraryEntry {
        QString title;
        QString path;
        quint64 program_id{};
    };

    explicit GameExportDialog(QWidget* parent = nullptr);
    ~GameExportDialog() override = default;

    /// Set the game ROM path and optional title-id for portable data bundling.
    void SetRomPath(const QString& path, quint64 program_id = 0);
    void SetLibraryEntries(QVector<LibraryEntry> entries);

    enum class TargetPlatform {
        Windows,
        Linux,
        MacOS,
    };

    enum class RecompileBackend {
        Dynarmic,   ///< Default — mature and stable
        Ballistic,  ///< WIP — from pound-emu/ballistic
    };

signals:
    void ExportFinished(bool success, const QString& output_path);

private slots:
    void OnBrowseRom();
    void OnSelectFromLibrary();
    void OnBrowseOutput();
    void OnExport();

private:
    void SetupUi();

    /// AOT export: scan ARM code and serialize translated compiler artifacts.
    /// Returns path to the generated cache directory, or empty string on failure.
    QString RunAotPrecompile(const QString& exefs_dir, const QString& cache_dir,
                             RecompileBackend backend);

    /// Package the translated output into a platform-specific export bundle.
    bool PackageNativeExport(const QString& rom_path, const QString& cache_dir,
                             const QString& output_dir, const QString& game_name,
                             TargetPlatform platform);

    QLineEdit* rom_path_edit{};
    QLineEdit* output_path_edit{};
    QComboBox* platform_combo{};
    QComboBox* backend_combo{};
    QCheckBox* include_save_data_checkbox{};
    QCheckBox* include_shader_cache_checkbox{};
    QCheckBox* include_custom_config_checkbox{};
    QCheckBox* aot_full_scan_checkbox{};
    QProgressBar* progress_bar{};
    QPushButton* export_button{};
    QLabel* status_label{};
    quint64 rom_program_id{};
    QVector<LibraryEntry> library_entries_;
};
