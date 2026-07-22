// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>

namespace Ui {
class ModeSelector;
}

/// The three operational modes of the application.
enum class AppMode {
    Gamer,      ///< Default: game library, simple controls, Steam integration
    Programmer, ///< Unity-like IDE: code editor, live editing, compilation tools
    Hacker,     ///< Advanced: memory viewer, MCP tools, plugin system
};

/// Startup dialog that lets the user pick an operational mode.
/// Shown on first launch or when explicitly triggered from Settings.
class ModeSelector : public QDialog {
    Q_OBJECT

public:
    explicit ModeSelector(QWidget* parent = nullptr);
    ~ModeSelector() override;

    /// Returns the mode the user selected (default: Gamer).
    [[nodiscard]] AppMode SelectedMode() const;

    /// Convenience: returns true when the user checked "remember my choice".
    [[nodiscard]] bool RememberChoice() const;

    /// Loads the last-selected mode from QSettings (or returns Gamer).
    static AppMode LoadSavedMode();

    /// Persists the given mode into QSettings.
    static void SaveMode(AppMode mode);

private slots:
    void OnGamerClicked();
    void OnProgrammerClicked();
    void OnHackerClicked();

private:
    void SetupUi();
    void ApplySelection(AppMode mode);

    AppMode selected_mode_{AppMode::Gamer};
    bool remember_choice_{true};

    // Owned widgets (created in SetupUi, no .ui file needed)
    class QPushButton* btn_gamer_{};
    class QPushButton* btn_programmer_{};
    class QPushButton* btn_hacker_{};
    class QLabel* lbl_description_{};
};
