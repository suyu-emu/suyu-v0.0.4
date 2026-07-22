// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

class MigrationDialog : public QDialog {
    Q_OBJECT
public:
    MigrationDialog(QWidget* parent = nullptr);

    virtual ~MigrationDialog();

    void setText(const QString& text);
    void addBox(QWidget* box);
    QAbstractButton* addButton(const QString& text, const bool reject = false);

    QAbstractButton* clickedButton() const;

private:
    QLabel* m_text;
    QVBoxLayout* m_boxes;
    QHBoxLayout* m_buttons;

    QAbstractButton* m_clickedButton;
};
