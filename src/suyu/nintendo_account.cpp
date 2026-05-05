// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "suyu/nintendo_account.h"

#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSettings>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef SUYU_USE_QT_WEB_ENGINE
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineView>
#endif

// XOR key derived from application identity (not cryptographic — obfuscation only)
static constexpr char kObfuscationKey[] = "SuyuEclipse2024NintendoLink";

QString ExtractSessionToken(const QString& input) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const auto cleanup = [](QString token) {
        token = token.trimmed();
        if (token.startsWith(QLatin1Char('"')) && token.endsWith(QLatin1Char('"')) && token.size() >= 2) {
            token = token.mid(1, token.size() - 2);
        }
        token = token.trimmed();
        token = QString::fromUtf8(QByteArray::fromPercentEncoding(token.toUtf8()));
        return token;
    };

    const QUrl as_url(trimmed);
    if (as_url.isValid() && !as_url.scheme().isEmpty()) {
        const QUrlQuery query(as_url);
        const QString token_from_query = query.queryItemValue(QStringLiteral("session_token"));
        if (!token_from_query.isEmpty()) {
            return cleanup(token_from_query);
        }
    }

    const QRegularExpression token_pattern(
        QStringLiteral(R"((?:^|[;\s?&])session_token=([^;\s&#]+))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = token_pattern.match(trimmed);
    if (match.hasMatch()) {
        return cleanup(match.captured(1));
    }

    return cleanup(trimmed);
}

QString OwnedLibraryToJson(const std::vector<NintendoOwnedGame>& library) {
    QJsonArray array;
    for (const auto& game : library) {
        QJsonObject obj;
        obj[QStringLiteral("title")] = game.title;
        obj[QStringLiteral("platform")] = game.platform;
        obj[QStringLiteral("purchase_date")] = game.purchase_date;
        obj[QStringLiteral("is_digital")] = game.is_digital;
        obj[QStringLiteral("title_id")] = game.title_id;
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

std::vector<NintendoOwnedGame> OwnedLibraryFromJson(const QString& json) {
    std::vector<NintendoOwnedGame> library;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return library;
    }
    const QJsonArray array = doc.array();
    for (const auto& entry : array) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject obj = entry.toObject();
        NintendoOwnedGame game;
        game.title = obj.value(QStringLiteral("title")).toString();
        game.platform = obj.value(QStringLiteral("platform")).toString();
        game.purchase_date = obj.value(QStringLiteral("purchase_date")).toString();
        game.is_digital = obj.value(QStringLiteral("is_digital")).toBool(true);
        game.title_id = obj.value(QStringLiteral("title_id")).toString();
        if (!game.title.isEmpty()) {
            library.push_back(std::move(game));
        }
    }
    return library;
}

std::vector<NintendoOwnedGame> LoadNintendoOwnedLibrary() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    const QString json = settings.value(QStringLiteral("library"), QStringLiteral("[]")).toString();
    settings.endGroup();
    return OwnedLibraryFromJson(json);
}

void StoreNintendoOwnedLibrary(const std::vector<NintendoOwnedGame>& library) {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.setValue(QStringLiteral("library"), OwnedLibraryToJson(library));
    settings.endGroup();
}

void ClearNintendoOwnedLibrary() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.remove(QStringLiteral("library"));
    settings.endGroup();
}

NintendoAccountDialog::NintendoAccountDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Nintendo Account"));
    setMinimumSize(480, 380);
    network_manager_ = new QNetworkAccessManager(this);
    SetupUi();
    LoadCredentials();
    owned_library_ = LoadNintendoOwnedLibrary();
    RefreshStatus();
    if (linked_ && session_token_.isEmpty() == false && owned_library_.empty()) {
        FetchNintendoOwnedLibrary(session_token_);
    }
}

void NintendoAccountDialog::SetupUi() {
    auto* layout = new QVBoxLayout(this);

    // Status section
    status_label = new QLabel(this);
    status_label->setAlignment(Qt::AlignCenter);
    status_label->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layout->addWidget(status_label);

    nickname_label = new QLabel(this);
    nickname_label->setAlignment(Qt::AlignCenter);
    nickname_label->setStyleSheet(QStringLiteral("font-size: 13px; color: #aaa;"));
    layout->addWidget(nickname_label);

    user_id_label = new QLabel(this);
    user_id_label->setAlignment(Qt::AlignCenter);
    user_id_label->setStyleSheet(QStringLiteral("font-size: 11px; color: #666;"));
    layout->addWidget(user_id_label);

    library_summary_label = new QLabel(this);
    library_summary_label->setAlignment(Qt::AlignCenter);
    library_summary_label->setStyleSheet(QStringLiteral("font-size: 11px; color: #999;"));
    layout->addWidget(library_summary_label);

    layout->addSpacing(10);

    // Browser login button (preferred method)
    browser_login_button = new QPushButton(tr("Sign in with Browser"), this);
    browser_login_button->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #e60012; color: white; font-size: 14px; "
                        "font-weight: bold; padding: 10px 20px; border-radius: 6px; } "
                        "QPushButton:hover { background-color: #ff1a2d; }"));
    layout->addWidget(browser_login_button);
    connect(browser_login_button, &QPushButton::clicked, this,
            &NintendoAccountDialog::OpenBrowserLogin);

    layout->addSpacing(6);

    // Instructions
    instructions_label = new QLabel(
        tr("Click 'Sign in with Browser' to log in directly.\n\n"
           "If the embedded browser is unavailable, you can manually\n"
           "paste a session token instead (raw token, full cookie string,\n"
           "or a URL/query containing session_token=...):\n"
           "1. Log in to accounts.nintendo.com in your browser\n"
           "2. Open Developer Tools (F12) > Application > Cookies\n"
           "3. Copy the 'session_token' cookie value\n"
           "4. Paste it below and click 'Verify & Link'\n\n"
           "Your token is stored locally with obfuscation. It is never sent\n"
           "to any third-party server."),
        this);
    instructions_label->setWordWrap(true);
    instructions_label->setStyleSheet(QStringLiteral("color: #999; font-size: 11px;"));
    layout->addWidget(instructions_label);

    // Token input section
    auto* token_group = new QGroupBox(tr("Session Token"), this);
    auto* token_layout = new QVBoxLayout(token_group);

    token_input = new QLineEdit(this);
    token_input->setPlaceholderText(tr("Paste session_token here..."));
    token_input->setEchoMode(QLineEdit::Password);
    token_layout->addWidget(token_input);

    progress_bar = new QProgressBar(this);
    progress_bar->setRange(0, 0); // indeterminate
    progress_bar->setVisible(false);
    token_layout->addWidget(progress_bar);

    layout->addWidget(token_group);

    layout->addStretch();

    // Button row
    auto* button_row = new QHBoxLayout();
    link_button = new QPushButton(tr("Verify && Link"), this);
    verify_button = new QPushButton(tr("Check Status"), this);
    unlink_button = new QPushButton(tr("Unlink"), this);
    button_row->addWidget(link_button);
    button_row->addWidget(verify_button);
    button_row->addWidget(unlink_button);
    layout->addLayout(button_row);

    connect(link_button, &QPushButton::clicked, this, &NintendoAccountDialog::OnLinkClicked);
    connect(unlink_button, &QPushButton::clicked, this, &NintendoAccountDialog::OnUnlinkClicked);
    connect(verify_button, &QPushButton::clicked, this, &NintendoAccountDialog::OnVerifyClicked);
    connect(token_input, &QLineEdit::returnPressed, this,
            &NintendoAccountDialog::OnTokenSubmitted);
}

void NintendoAccountDialog::RefreshStatus() {
    if (linked_) {
        status_label->setText(tr("Account Linked"));
        status_label->setStyleSheet(
            QStringLiteral("font-size: 16px; font-weight: bold; color: #4CAF50;"));
        nickname_label->setText(tr("Nickname: %1").arg(nickname_));
        user_id_label->setText(tr("User ID: %1").arg(user_id_));
        link_button->setEnabled(false);
        unlink_button->setEnabled(true);
        verify_button->setEnabled(true);
        token_input->setEnabled(false);
        browser_login_button->setVisible(false);
        instructions_label->setVisible(false);

        if (!owned_library_.empty()) {
            library_summary_label->setText(tr("Nintendo library contains %n owned title(s)", "",
                                            static_cast<int>(owned_library_.size())));
            library_summary_label->setVisible(true);
        } else {
            library_summary_label->setText(tr("Nintendo library not loaded yet. Click Verify & Link to refresh."));
            library_summary_label->setVisible(true);
        }
    } else {
        status_label->setText(tr("No Account Linked"));
        status_label->setStyleSheet(
            QStringLiteral("font-size: 16px; font-weight: bold; color: #f44336;"));
        nickname_label->clear();
        user_id_label->clear();
        library_summary_label->clear();
        library_summary_label->setVisible(false);
        link_button->setEnabled(true);
        unlink_button->setEnabled(false);
        verify_button->setEnabled(false);
        token_input->setEnabled(true);
        token_input->clear();
        browser_login_button->setVisible(true);
        instructions_label->setVisible(true);
    }
}

bool NintendoAccountDialog::IsLinked() const {
    return linked_;
}

QString NintendoAccountDialog::Nickname() const {
    return nickname_;
}

std::vector<NintendoOwnedGame> NintendoAccountDialog::OwnedLibrary() const {
    return owned_library_;
}

QString NintendoAccountDialog::UserId() const {
    return user_id_;
}

QByteArray NintendoAccountDialog::Obfuscate(const QByteArray& data) {
    QByteArray result = data;
    const int key_len = static_cast<int>(sizeof(kObfuscationKey) - 1);
    for (int i = 0; i < result.size(); ++i) {
        result[i] = result[i] ^ kObfuscationKey[i % key_len];
    }
    return result;
}

void NintendoAccountDialog::StoreCredentials(const QString& session_token,
                                              const QString& nickname, const QString& user_id) {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.setValue(QStringLiteral("session_token"),
                     Obfuscate(session_token.toUtf8()).toBase64());
    settings.setValue(QStringLiteral("nickname"), nickname);
    settings.setValue(QStringLiteral("user_id"), user_id);
    settings.setValue(QStringLiteral("linked"), true);
    settings.endGroup();
}

void NintendoAccountDialog::LoadCredentials() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    linked_ = settings.value(QStringLiteral("linked"), false).toBool();
    if (linked_) {
        const QByteArray stored =
            QByteArray::fromBase64(settings.value(QStringLiteral("session_token")).toByteArray());
        session_token_ = QString::fromUtf8(Obfuscate(stored));
        nickname_ = settings.value(QStringLiteral("nickname")).toString();
        user_id_ = settings.value(QStringLiteral("user_id")).toString();
    }
    settings.endGroup();
}

void NintendoAccountDialog::ClearCredentials() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.remove(QString());
    settings.endGroup();
    linked_ = false;
    session_token_.clear();
    nickname_.clear();
    user_id_.clear();
    owned_library_.clear();
    ClearNintendoOwnedLibrary();
}

void NintendoAccountDialog::VerifySessionToken(const QString& token) {
    progress_bar->setVisible(true);
    link_button->setEnabled(false);

    // Nintendo's accounts API endpoint to get user info from session token
    // POST https://accounts.nintendo.com/connect/1.0.0/api/token
    // with grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer-session-token
    const QUrl token_url(
        QStringLiteral("https://accounts.nintendo.com/connect/1.0.0/api/token"));
    QNetworkRequest request(token_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Host", "accounts.nintendo.com");

    QJsonObject body;
    body[QStringLiteral("client_id")] = QStringLiteral("71b963c1b7b6d119");
    body[QStringLiteral("session_token")] = token;
    body[QStringLiteral("grant_type")] =
        QStringLiteral("urn:ietf:params:oauth:grant-type:jwt-bearer-session-token");

    QNetworkReply* reply =
        network_manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, token]() {
        reply->deleteLater();
        progress_bar->setVisible(false);

        if (reply->error() != QNetworkReply::NoError) {
            link_button->setEnabled(true);
            emit LinkFailed(tr("Network error: %1").arg(reply->errorString()));
            status_label->setText(tr("Link failed: network error"));
            status_label->setStyleSheet(
                QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("error"))) {
            link_button->setEnabled(true);
            const QString error = obj[QStringLiteral("error_description")].toString();
            emit LinkFailed(tr("Auth error: %1").arg(error));
            status_label->setText(tr("Link failed: invalid token"));
            status_label->setStyleSheet(
                QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
            return;
        }

        const QString access_token = obj[QStringLiteral("access_token")].toString();
        if (access_token.isEmpty()) {
            link_button->setEnabled(true);
            emit LinkFailed(tr("No access token in response"));
            return;
        }

        // Now fetch user info with the access token
        const QUrl user_url(QStringLiteral("https://api.accounts.nintendo.com/2.0.0/users/me"));
        QNetworkRequest user_request(user_url);
        user_request.setRawHeader("Authorization",
                                  QStringLiteral("Bearer %1").arg(access_token).toUtf8());
        user_request.setRawHeader("Accept", "application/json");

        QNetworkReply* user_reply = network_manager_->get(user_request);
        connect(user_reply, &QNetworkReply::finished, this, [this, user_reply, token]() {
            user_reply->deleteLater();

            if (user_reply->error() != QNetworkReply::NoError) {
                link_button->setEnabled(true);
                emit LinkFailed(tr("Failed to fetch user info"));
                return;
            }

            const QJsonDocument user_doc = QJsonDocument::fromJson(user_reply->readAll());
            const QJsonObject user_obj = user_doc.object();

            const QString nick = user_obj[QStringLiteral("nickname")].toString();
            const QString uid = user_obj[QStringLiteral("id")].toString();

            session_token_ = token;
            nickname_ = nick.isEmpty() ? QStringLiteral("Nintendo User") : nick;
            user_id_ = uid;
            linked_ = true;

            StoreCredentials(session_token_, nickname_, user_id_);
            FetchNintendoOwnedLibrary(token);
            RefreshStatus();
            emit AccountLinked(nickname_);
        });
    });
}

void NintendoAccountDialog::FetchNintendoOwnedLibrary(const QString& token) {
    const QUrl orders_url(QStringLiteral("https://www.nintendo.com/us/orders/"));
    QNetworkRequest request(orders_url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                     "AppleWebKit/537.36 (KHTML, like Gecko) "
                                     "Chrome/120.0.0.0 Safari/537.36"));
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    request.setRawHeader("Cookie",
                         QStringLiteral("session_token=%1").arg(token).toUtf8());

    progress_bar->setVisible(true);
    library_summary_label->setText(tr("Syncing Nintendo digital library..."));
    library_summary_label->setVisible(true);
    QNetworkReply* orders_reply = network_manager_->get(request);
    connect(orders_reply, &QNetworkReply::finished, this, [this, orders_reply]() {
        orders_reply->deleteLater();
        progress_bar->setVisible(false);

        if (orders_reply->error() != QNetworkReply::NoError) {
            library_summary_label->setText(
                tr("Account linked. Library fetch is temporarily unavailable; you can still use local games."));
            library_summary_label->setVisible(true);
            return;
        }

        const QString html = QString::fromUtf8(orders_reply->readAll());
        owned_library_ = ParseNintendoPurchaseHistory(html);
        StoreNintendoOwnedLibrary(owned_library_);
        if (owned_library_.empty()) {
            library_summary_label->setText(
                tr("Account linked. No owned titles were detected from the web profile yet."));
            library_summary_label->setVisible(true);
        }
        RefreshStatus();
        emit OwnedLibraryUpdated(static_cast<int>(owned_library_.size()));
    });
}

std::vector<NintendoOwnedGame> NintendoAccountDialog::ParseNintendoPurchaseHistory(const QString& html) {
    std::vector<NintendoOwnedGame> library;
    const QRegularExpression order_block_regex(
        QStringLiteral(R"(<(?:div|li)[^>]*(?:order|purchase)[^>]*>.*?</(?:div|li)>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression title_regex(
        QStringLiteral(R"((?:<h[1-6][^>]*>|<span[^>]*class="[^"]*(?:title|product|name)[^"]*"[^>]*>)([^<]{3,120})</(?:h[1-6]|span)>)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression date_regex(
        QStringLiteral(R"((\d{1,2}/\d{1,2}/\d{4}|\d{4}-\d{2}-\d{2}))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression platform_regex(
        QStringLiteral(R"((Nintendo Switch|3DS|Wii U|Wii))"),
        QRegularExpression::CaseInsensitiveOption);

    auto order_it = order_block_regex.globalMatch(html);
    while (order_it.hasNext()) {
        const QRegularExpressionMatch match = order_it.next();
        const QString block = match.captured(0);

        QRegularExpressionMatch title_match = title_regex.match(block);
        if (!title_match.hasMatch()) {
            continue;
        }

        NintendoOwnedGame game;
        game.title = title_match.captured(1).trimmed();

        QRegularExpressionMatch date_match = date_regex.match(block);
        if (date_match.hasMatch()) {
            game.purchase_date = date_match.captured(1);
        }

        QRegularExpressionMatch platform_match = platform_regex.match(block);
        if (platform_match.hasMatch()) {
            game.platform = platform_match.captured(1);
        } else {
            game.platform = QStringLiteral("Nintendo Switch");
        }

        game.is_digital = true;
        game.title_id = QString::number(qHash(game.title), 16);

        library.push_back(std::move(game));
    }

    if (library.empty()) {
        const QRegularExpression fallback_title_regex(
            QStringLiteral(R"((?:<h[1-6][^>]*>|<span[^>]*>([^<]*(?:Mario|Zelda|Pokemon|Metroid|Kirby|Splatoon|Animal Crossing|Fire Emblem|Xenoblade)[^<]*)</(?:h[1-6]|span)>))"),
            QRegularExpression::CaseInsensitiveOption);
        auto title_it = fallback_title_regex.globalMatch(html);
        while (title_it.hasNext()) {
            const QRegularExpressionMatch title_match = title_it.next();
            NintendoOwnedGame game;
            game.title = title_match.captured(1).trimmed();
            game.platform = QStringLiteral("Nintendo Switch");
            game.is_digital = true;
            game.title_id = QString::number(qHash(game.title), 16);
            library.push_back(std::move(game));
        }
    }

    return library;
}

void NintendoAccountDialog::OnLinkClicked() {
    OnTokenSubmitted();
}

void NintendoAccountDialog::OnTokenSubmitted() {
    const QString token = ExtractSessionToken(token_input->text());
    if (token.isEmpty()) {
        status_label->setText(tr("Please enter a session token"));
        status_label->setStyleSheet(
            QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
        return;
    }
    VerifySessionToken(token);
}

void NintendoAccountDialog::OpenBrowserLogin() {
#ifdef SUYU_USE_QT_WEB_ENGINE
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Nintendo Account Sign-In"));
    dialog->resize(900, 700);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(dialog);

    auto* hint = new QLabel(
        tr("Sign in to your Nintendo Account below. The window will close automatically "
           "once your session token is captured."),
        dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #999; font-size: 11px; padding: 4px;"));
    layout->addWidget(hint);

    auto* profile = new QWebEngineProfile(QStringLiteral("NintendoLogin"), dialog);
    auto* web_view = new QWebEngineView(profile, dialog);
    layout->addWidget(web_view, 1);

    // Monitor cookies for session_token
    auto* cookie_store = profile->cookieStore();
    connect(cookie_store, &QWebEngineCookieStore::cookieAdded, dialog,
            [this, dialog](const QNetworkCookie& cookie) {
                if (cookie.name() == "session_token" &&
                    cookie.domain().contains(QStringLiteral("nintendo.com"))) {
                    const QString token = QString::fromUtf8(cookie.value());
                    if (!token.isEmpty()) {
                        dialog->close();
                        VerifySessionToken(token);
                    }
                }
            });

    web_view->setUrl(QUrl(QStringLiteral("https://accounts.nintendo.com")));
#else
    // No WebEngine — open external browser and let user paste token manually
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://accounts.nintendo.com")));
    status_label->setText(tr("Browser opened — paste session token below after signing in"));
    status_label->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
    token_input->setFocus();
#endif
}

void NintendoAccountDialog::OnVerifyClicked() {
    if (!linked_ || session_token_.isEmpty()) {
        return;
    }
    VerifySessionToken(session_token_);
}

void NintendoAccountDialog::OnUnlinkClicked() {
    ClearCredentials();
    RefreshStatus();
    emit AccountUnlinked();
}
