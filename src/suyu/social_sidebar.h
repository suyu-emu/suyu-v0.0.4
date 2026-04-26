// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTextBrowser;
class QComboBox;
class QPushButton;
class QString;

/// Social sidebar that fetches Reddit posts via JSON API and renders them locally.
class SocialSidebar : public QDockWidget {
    Q_OBJECT

public:
    explicit SocialSidebar(QWidget* parent = nullptr);
    ~SocialSidebar() override;

    /// Set the subreddit to display (e.g. "r/SuyuEclipse").
    void SetSubreddit(const QString& subreddit);

    /// Refresh the feed content by fetching from the Reddit JSON API.
    void Refresh();

public slots:
    void OnSubredditChanged(const QString& text);
    void OnRefreshClicked();

private:
    void SetupUi();
    void OnNetworkReply(QNetworkReply* reply);
    bool TryLoadWithPowerShell(const QString& subreddit_path);
    bool ParseAndRenderPayload(const QByteArray& payload, const QString& subreddit_label,
                               const QString& failure_context);
    QString RenderPostsHtml(const QJsonArray& posts) const;

    QComboBox* cmb_subreddit_{};
    QPushButton* btn_refresh_{};
    QTextBrowser* text_view_{};
    QNetworkAccessManager* network_manager_{};
    QProcess* powershell_process_{};
};
