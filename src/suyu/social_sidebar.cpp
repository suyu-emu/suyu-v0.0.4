// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QSslSocket>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "common/logging/log.h"
#include "suyu/social_sidebar.h"

SocialSidebar::SocialSidebar(QWidget* parent)
    : QDockWidget(QStringLiteral("Community"), parent) {
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    SetupUi();
}

SocialSidebar::~SocialSidebar() = default;

void SocialSidebar::SetupUi() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    // Subreddit selector
    auto* toolbar = new QHBoxLayout();
    cmb_subreddit_ = new QComboBox(this);
    cmb_subreddit_->setEditable(false);
    cmb_subreddit_->addItems({
        QStringLiteral("r/suyu"),
    });
    toolbar->addWidget(cmb_subreddit_);

    btn_refresh_ = new QPushButton(QStringLiteral("Refresh"), this);
    toolbar->addWidget(btn_refresh_);
    layout->addLayout(toolbar);

    // Content area — always use QTextBrowser with parsed JSON data
    text_view_ = new QTextBrowser(this);
    text_view_->setOpenExternalLinks(true);
    text_view_->setHtml(QStringLiteral(
        "<h3>Community Sidebar</h3>"
        "<p>Click <b>Refresh</b> to load posts from the selected subreddit.</p>"));
    layout->addWidget(text_view_);

    container->setLayout(layout);
    setWidget(container);

    // Network manager for Reddit JSON API
    network_manager_ = new QNetworkAccessManager(this);
    connect(network_manager_, &QNetworkAccessManager::finished, this,
            &SocialSidebar::OnNetworkReply);

    connect(cmb_subreddit_, &QComboBox::currentTextChanged, this,
            &SocialSidebar::OnSubredditChanged);
    connect(btn_refresh_, &QPushButton::clicked, this, &SocialSidebar::OnRefreshClicked);

    // Automatically load the subreddit feed when the sidebar is created.
    Refresh();
}

void SocialSidebar::SetSubreddit(const QString& subreddit) {
    cmb_subreddit_->setCurrentText(subreddit);
    Refresh();
}

void SocialSidebar::Refresh() {
    const QString sub = cmb_subreddit_->currentText().trimmed();
    const QString sub_path =
        sub.startsWith(QLatin1String("r/")) ? sub : QStringLiteral("r/") + sub;

    text_view_->setHtml(
        QStringLiteral("<h3>%1</h3><p><i>Loading posts...</i></p>").arg(sub_path));

#ifdef _WIN32
    LOG_INFO(Frontend, "SocialSidebar using PowerShell fetch path for {}",
             sub_path.toStdString());
    if (TryLoadWithPowerShell(sub_path)) {
        return;
    }
    LOG_WARNING(Frontend, "SocialSidebar PowerShell fetch path failed for {}; falling back to Qt network",
                sub_path.toStdString());
#endif

    if (!QSslSocket::supportsSsl()) {
        LOG_WARNING(Frontend, "Qt SSL unavailable for SocialSidebar; trying PowerShell fallback for {}",
                    sub_path.toStdString());
        if (!TryLoadWithPowerShell(sub_path)) {
            LOG_ERROR(Frontend, "SocialSidebar PowerShell fallback failed for {}",
                      sub_path.toStdString());
            text_view_->setHtml(
                QStringLiteral("<h3>%1</h3>"
                               "<p style='color:red;'>TLS initialization failed in Qt and fallback fetch failed.</p>"
                               "<p>Install OpenSSL runtime DLLs or verify PowerShell internet access.</p>")
                    .arg(sub_path));
        }
        return;
    }

    // Use Reddit's public JSON endpoint (no API key required for read-only listing)
    const QUrl url(QStringLiteral("https://www.reddit.com/%1.json?limit=25&raw_json=1")
                       .arg(sub_path));

    QNetworkRequest request(url);
    // Reddit requires a descriptive User-Agent for JSON endpoints
    request.setRawHeader("User-Agent", "suyu/1.0 (emulator; community sidebar)");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    network_manager_->get(request);
}

void SocialSidebar::OnNetworkReply(QNetworkReply* reply) {
    reply->deleteLater();

    const QString sub = cmb_subreddit_->currentText().trimmed();

    if (reply->error() != QNetworkReply::NoError) {
        const bool is_tls_error = reply->error() == QNetworkReply::SslHandshakeFailedError ||
                                  reply->errorString().contains(QStringLiteral("TLS"),
                                                                Qt::CaseInsensitive) ||
                                  reply->errorString().contains(QStringLiteral("SSL"),
                                                                Qt::CaseInsensitive);
        if (is_tls_error) {
            const QString sub_path =
                sub.startsWith(QLatin1String("r/")) ? sub : QStringLiteral("r/") + sub;
            LOG_WARNING(Frontend,
                        "SocialSidebar HTTPS request failed with Qt TLS for {}; trying PowerShell fallback: {}",
                        sub_path.toStdString(), reply->errorString().toStdString());
            if (TryLoadWithPowerShell(sub_path)) {
                return;
            }
            LOG_ERROR(Frontend, "SocialSidebar PowerShell fallback failed after Qt TLS error for {}",
                      sub_path.toStdString());
            text_view_->setHtml(QStringLiteral("<h3>%1</h3>"
                                               "<p style='color:red;'>Secure connection failed: %2</p>"
                                               "<p>Fallback fetch also failed. Install the required SSL runtime libraries, then retry.</p>")
                                   .arg(sub, reply->errorString().toHtmlEscaped()));
            return;
        }
        text_view_->setHtml(
            QStringLiteral("<h3>%1</h3><p style='color:red;'>Failed to load: %2</p>"
                           "<p>Check your internet connection or try again later.</p>")
                .arg(sub, reply->errorString().toHtmlEscaped()));
        return;
    }

    ParseAndRenderPayload(reply->readAll(), sub, QStringLiteral("Qt network response"));
}

bool SocialSidebar::TryLoadWithPowerShell(const QString& subreddit_path) {
#ifdef _WIN32
    if (powershell_process_) {
        powershell_process_->kill();
        powershell_process_->deleteLater();
        powershell_process_ = nullptr;
    }

    powershell_process_ = new QProcess(this);
    powershell_process_->setProgram(QStringLiteral("powershell"));

    const QString url =
        QStringLiteral("https://www.reddit.com/%1.json?limit=25&raw_json=1").arg(subreddit_path);
    const QString command =
        QStringLiteral("$ProgressPreference='SilentlyContinue';"
                       "$u='%1';"
                       "$r=Invoke-WebRequest -UseBasicParsing -Headers @{ 'User-Agent'='suyu/1.0 (social sidebar fallback)'; 'Accept'='application/json'} -Uri $u;"
                       "$r.Content")
            .arg(url);

    powershell_process_->setArguments(
        {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"),
         QStringLiteral("Bypass"), QStringLiteral("-Command"), command});

    text_view_->setHtml(QStringLiteral("<h3>%1</h3><p><i>Loading posts...</i></p>")
                            .arg(subreddit_path));

    connect(powershell_process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, subreddit_path](int exit_code, QProcess::ExitStatus exit_status) {
                if (!powershell_process_) {
                    return;
                }

                const QByteArray payload = powershell_process_->readAllStandardOutput();
                const QByteArray stderr_text = powershell_process_->readAllStandardError();

                const bool success =
                    exit_status == QProcess::NormalExit && exit_code == 0 &&
                    ParseAndRenderPayload(payload, subreddit_path,
                                          QStringLiteral("PowerShell fallback response"));
                if (success) {
                    LOG_INFO(Frontend, "SocialSidebar PowerShell fallback loaded posts for {}",
                             subreddit_path.toStdString());
                } else {
                    LOG_ERROR(Frontend,
                              "SocialSidebar PowerShell fallback failed for {} (status={}, code={}, stderr={})",
                              subreddit_path.toStdString(), static_cast<int>(exit_status), exit_code,
                              QString::fromUtf8(stderr_text).left(256).toStdString());
                    text_view_->setHtml(
                        QStringLiteral("<h3>%1</h3><p style='color:red;'>Failed to load posts.</p>")
                            .arg(subreddit_path));
                }

                powershell_process_->deleteLater();
                powershell_process_ = nullptr;
            });

    powershell_process_->start();
    return true;
#else
    Q_UNUSED(subreddit_path);
    return false;
#endif
}

bool SocialSidebar::ParseAndRenderPayload(const QByteArray& payload, const QString& subreddit_label,
                                          const QString& failure_context) {
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        text_view_->setHtml(QStringLiteral("<h3>%1</h3>"
                                           "<p style='color:red;'>Invalid response from Reddit (%2).</p>")
                               .arg(subreddit_label, failure_context.toHtmlEscaped()));
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject listing = root[QStringLiteral("data")].toObject();
    const QJsonArray children = listing[QStringLiteral("children")].toArray();

    if (children.isEmpty()) {
        text_view_->setHtml(QStringLiteral("<h3>%1</h3><p>No posts found.</p>").arg(subreddit_label));
        return true;
    }

    text_view_->setHtml(RenderPostsHtml(children));
    return true;
}

QString SocialSidebar::RenderPostsHtml(const QJsonArray& posts) const {
    const QString sub = cmb_subreddit_->currentText().trimmed();

    QString html = QStringLiteral(
        "<style>"
        "body { font-family: sans-serif; margin: 4px; }"
        ".post { border-bottom: 1px solid #ddd; padding: 6px 0; }"
        ".score { color: #ff4500; font-weight: bold; margin-right: 6px; }"
        ".title a { color: #1a0dab; text-decoration: none; }"
        ".meta { color: #888; font-size: 0.85em; }"
        ".flair { background: #eee; border-radius: 3px; padding: 1px 4px; font-size: 0.8em; }"
        ".selftext { color: #555; font-size: 0.9em; margin-top: 2px; }"
        "</style>"
        "<h3>%1</h3>")
                       .arg(sub);

    for (const QJsonValue& child : posts) {
        const QJsonObject post = child.toObject()[QStringLiteral("data")].toObject();
        const QString title = post[QStringLiteral("title")].toString().toHtmlEscaped();
        const int score = post[QStringLiteral("score")].toInt();
        const int comments = post[QStringLiteral("num_comments")].toInt();
        const QString permalink = post[QStringLiteral("permalink")].toString();
        const QString url = post[QStringLiteral("url")].toString();
        const QString flair = post[QStringLiteral("link_flair_text")].toString().toHtmlEscaped();
        const bool is_self = post[QStringLiteral("is_self")].toBool();

        const QString full_link =
            QStringLiteral("https://www.reddit.com%1").arg(permalink);

        html += QStringLiteral("<div class='post'>");

        html += QStringLiteral("<span class='score'>%1</span>").arg(score);

        if (!flair.isEmpty()) {
            html += QStringLiteral("<span class='flair'>%1</span> ").arg(flair);
        }

        const QString& link_target = is_self ? full_link : url;
        html += QStringLiteral("<span class='title'><a href='%1'>%2</a></span><br/>")
                    .arg(link_target.toHtmlEscaped(), title);

        html += QStringLiteral(
                    "<span class='meta'><a href='%1'>%2 comments</a></span>")
                    .arg(full_link.toHtmlEscaped(), QString::number(comments));

        html += QStringLiteral("</div>");
    }

    html += QStringLiteral(
        "<p class='meta' style='text-align:center;'>"
        "<a href='https://www.reddit.com/%1'>View on Reddit</a></p>")
                .arg(sub);

    return html;
}

void SocialSidebar::OnSubredditChanged(const QString& text) {
    Q_UNUSED(text);
    // Don't auto-refresh on every keystroke — wait for the refresh button
}

void SocialSidebar::OnRefreshClicked() {
    Refresh();
}
