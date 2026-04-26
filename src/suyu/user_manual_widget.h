// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QTextBrowser;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

/// In-app user manual widget with searchable help content.
class UserManualWidget : public QWidget {
    Q_OBJECT

public:
    explicit UserManualWidget(QWidget* parent = nullptr);
    ~UserManualWidget() override;

    /// Load manual content from a bundled resource path.
    void LoadContent(const QString& resource_path);

public slots:
    void OnSearchTextChanged(const QString& text);

private:
    void SetupUi();
    void LoadDefaultContent();

    QTextBrowser* browser_{};
    QLineEdit* search_box_{};
    QPushButton* btn_back_{};
    QPushButton* btn_forward_{};
};
