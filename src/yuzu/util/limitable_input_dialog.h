// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;

/// A QDialog that functions similarly to QInputDialog, however, it allows
/// restricting the minimum and total number of characters that can be entered.
class LimitableInputDialog final : public QDialog {
    Q_OBJECT
public:
    explicit LimitableInputDialog(QWidget* parent = nullptr);
    ~LimitableInputDialog() override;

    static QString GetText(QWidget* parent, const QString& title, const QString& text,
                           int min_character_limit, int max_character_limit);

private:
    void CreateUI();
    void ConnectEvents();

    QLabel* text_label;
    QLineEdit* text_entry;
    QDialogButtonBox* buttons;
};
