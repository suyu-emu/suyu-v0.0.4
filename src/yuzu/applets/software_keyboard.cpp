// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "yuzu/applets/software_keyboard.h"
#include "yuzu/main.h"

QtSoftwareKeyboardValidator::QtSoftwareKeyboardValidator() {}

QValidator::State QtSoftwareKeyboardValidator::validate(QString& input, int& pos) const {}

QtSoftwareKeyboardDialog::QtSoftwareKeyboardDialog(QWidget* parent) : QDialog(parent) {}

QtSoftwareKeyboardDialog::~QtSoftwareKeyboardDialog() = default;

QtSoftwareKeyboard::QtSoftwareKeyboard(GMainWindow& main_window) {}

QtSoftwareKeyboard::~QtSoftwareKeyboard() = default;
