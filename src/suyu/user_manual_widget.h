// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class QTextBrowser;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QLabel;
class QSplitter;

#ifdef SUYU_USE_QT_WEB_ENGINE
class QWebEngineView;
#endif

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
    void LoadDocsUrl(const QUrl& url);

    QTextBrowser* browser_{};
    QLineEdit* search_box_{};
    QPushButton* btn_back_{};
    QPushButton* btn_forward_{};
    QPushButton* btn_reload_{};
    QPushButton* btn_open_external_{};
    QLabel* docs_status_label_{};
    QSplitter* content_splitter_{};
    QUrl docs_url_;

#ifdef SUYU_USE_QT_WEB_ENGINE
    QWebEngineView* docs_view_{};
#endif
};
