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

    /// Test-only: drive a full export run without needing to click through
    /// the dialog's file pickers/combo boxes, so live automation can trigger
    /// and observe the AOT pipeline (including its known hang past ~15%)
    /// directly.
    void TriggerExportForTesting(const QString& rom_path, const QString& output_dir);

    /// Every standalone recompiled executable that has already been built for
    /// this game, one per recompiled module, newest-looking first. Empty when
    /// the game was never exported, or was exported but never compiled.
    ///
    /// @param game_name  Library title of the game.
    /// @param rom_path   ROM path; its base name is what the exporter actually
    ///                   names the output directory, which is often not the
    ///                   library title, so both are tried.
    static QStringList FindRecompiledExecutables(const QString& game_name,
                                                 const QString& rom_path = {});

    /// Directories that have been used as export output, most recent first.
    static QStringList RecompileOutputRoots();
    /// Remember @p dir as an export output root for future lookups.
    static void RememberOutputRoot(const QString& dir);

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
                             RecompileBackend backend, const QString& game_name);

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
    /// When checked and a module fails to recompile, emit a stub that falls back
    /// to the dynarmic interpreter for that module instead of aborting the export.
    QCheckBox* fallback_to_interpreter_checkbox{};
    /// Export format: index 0 = source only, index 1 = build to a native binary.
    /// "Build" is a promise, not a hint - when it is selected the export runs
    /// cmake to completion and reports a hard error if a binary cannot be
    /// produced, rather than quietly degrading to a folder of C.
    QComboBox* output_format_combo{};
    /// True when output_format_combo selects the build-to-binary format.
    bool WantsCompiledOutput() const;
    QProgressBar* progress_bar{};
    QPushButton* export_button{};
    QLabel* status_label{};
    quint64 rom_program_id{};
    QVector<LibraryEntry> library_entries_;
};
