// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QValidator>

#include "core/frontend/applets/software_keyboard.h"

class GMainWindow;

class QtSoftwareKeyboardValidator final : public QValidator {
public:
    explicit QtSoftwareKeyboardValidator();
    State validate(QString& input, int& pos) const override;
};

class QtSoftwareKeyboardDialog final : public QDialog {
    Q_OBJECT

public:
    QtSoftwareKeyboardDialog(QWidget* parent);
    ~QtSoftwareKeyboardDialog() override;
};

class QtSoftwareKeyboard final : public QObject, public Core::Frontend::SoftwareKeyboardApplet {
    Q_OBJECT

public:
    explicit QtSoftwareKeyboard(GMainWindow& parent);
    ~QtSoftwareKeyboard() override;
};
