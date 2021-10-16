// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QAction;
class QHideEvent;
class QShowEvent;
class PlayerControlPreview;

namespace Core {
class System;
}

namespace InputCommon {
class InputSubsystem;
}

class ControllerDialog : public QWidget {
    Q_OBJECT

public:
    explicit ControllerDialog(Core::System& system, QWidget* parent = nullptr);

    /// Returns a QAction that can be used to toggle visibility of this dialog.
    QAction* toggleViewAction();

    // Disables events from the emulated controller
    void UnloadController();

protected:
    void showEvent(QShowEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private:
    QAction* toggle_view_action = nullptr;
    PlayerControlPreview* widget;
};
