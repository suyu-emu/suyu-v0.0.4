// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "suyu/nintendo_account.h"

#include "common/logging/log.h"

#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSettings>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef SUYU_USE_QT_WEB_ENGINE
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineView>
#endif

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace {
// Real client_id used by Nintendo Switch Online / nxapi-style console-linking
// flows (public, not a secret - PKCE public clients don't have one).
constexpr auto kNintendoClientId = "71b963c1b7b6d119";

#ifdef SUYU_USE_QT_WEB_ENGINE

QString GeneratePkceVerifier() {
    QByteArray bytes(32, Qt::Uninitialized);
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString PkceChallengeFromVerifier(const QString& verifier) {
    const QByteArray hash =
        QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
// Real Nintendo login redirects to a custom npf<client_id>://auth URI scheme
// (meant for a console's embedded webview to intercept, not a normal
// browser) carrying session_token_code in the URL fragment. QtWebEngine
// won't actually navigate to an unknown scheme, but acceptNavigationRequest
// still fires with the attempted URL first, which is the only hook that can
// observe it.
class NintendoLoginPage : public QWebEnginePage {
public:
    NintendoLoginPage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent) {}
    std::function<void(const QUrl&)> on_redirect;

protected:
    bool acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType,
                                 bool) override {
        if (url.scheme().startsWith(QStringLiteral("npf")) && on_redirect) {
            on_redirect(url);
            return false;
        }
        return true;
    }
};
#endif
} // namespace

// XOR key derived from application identity (not cryptographic — obfuscation only)
static constexpr char kObfuscationKey[] = "SuyuEclipse2024NintendoLink";
static constexpr auto kSettingsOrganization = "suyu";
static constexpr auto kSettingsApplication = "suyu";
static constexpr auto kLegacySettingsApplication = "SuyuEclipse";

void MigrateNintendoAccountSettingsIfNeeded() {
    static bool migrated = false;
    if (migrated) {
        return;
    }

    migrated = true;

    QSettings current(QString::fromLatin1(kSettingsOrganization),
                      QString::fromLatin1(kSettingsApplication));
    QSettings legacy(QString::fromLatin1(kSettingsOrganization),
                     QString::fromLatin1(kLegacySettingsApplication));

    current.beginGroup(QStringLiteral("NintendoAccount"));
    const bool has_current_values = !current.allKeys().isEmpty();
    current.endGroup();
    if (has_current_values) {
        return;
    }

    legacy.beginGroup(QStringLiteral("NintendoAccount"));
    const QStringList legacy_keys = legacy.allKeys();
    if (legacy_keys.isEmpty()) {
        legacy.endGroup();
        return;
    }

    current.beginGroup(QStringLiteral("NintendoAccount"));
    for (const auto& key : legacy_keys) {
        current.setValue(key, legacy.value(key));
    }
    current.endGroup();
    legacy.endGroup();
}

QSettings OpenNintendoSettings() {
    MigrateNintendoAccountSettingsIfNeeded();
    return QSettings(QString::fromLatin1(kSettingsOrganization),
                     QString::fromLatin1(kSettingsApplication));
}

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
        obj[QStringLiteral("icon_url")] = game.icon_url;
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool IsPlausibleSwitchTitleId(const QString& title_id) {
    // Switch application IDs are 16 hex digits and begin 0100. An earlier
    // HTML-scraping sync stored qHash() values here instead, which produced a
    // cache full of invented entries ("My Mario", "Super Mario", ...) that
    // then showed up in the library as though they were owned games. Anything
    // that isn't shaped like a real application ID is from that broken path
    // and gets dropped on load.
    static const QRegularExpression re(QStringLiteral("^0100[0-9a-fA-F]{12}$"));
    return re.match(title_id.trimmed()).hasMatch();
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
        game.icon_url = obj.value(QStringLiteral("icon_url")).toString();
        if (!game.title.isEmpty() && IsPlausibleSwitchTitleId(game.title_id)) {
            library.push_back(std::move(game));
        }
    }
    return library;
}

std::vector<NintendoOwnedGame> LoadNintendoOwnedLibrary() {
    QSettings settings = OpenNintendoSettings();
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    const QString json = settings.value(QStringLiteral("library"), QStringLiteral("[]")).toString();
    settings.endGroup();
    return OwnedLibraryFromJson(json);
}

void StoreNintendoOwnedLibrary(const std::vector<NintendoOwnedGame>& library) {
    QSettings settings = OpenNintendoSettings();
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.setValue(QStringLiteral("library"), OwnedLibraryToJson(library));
    settings.endGroup();
}

void ClearNintendoOwnedLibrary() {
    QSettings settings = OpenNintendoSettings();
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
    browser_login_button = new QPushButton(tr("One-Click Sign In"), this);
    browser_login_button->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #e60012; color: white; font-size: 14px; "
                        "font-weight: bold; padding: 10px 20px; border-radius: 6px; } "
                        "QPushButton:hover { background-color: #ff1a2d; }"));
    layout->addWidget(browser_login_button);
    connect(browser_login_button, &QPushButton::clicked, this,
            &NintendoAccountDialog::OpenBrowserLogin);

    // Alternative for users who'd rather sign in with their own default
    // browser (already-saved passwords/passkeys, extensions, etc.) instead
    // of the embedded WebEngine view. Opens the real system browser and
    // relies on the existing manual token-paste flow below to link the
    // resulting session - no OAuth app registration or loopback listener
    // needed, matching how the non-WebEngine fallback already behaves.
    external_browser_button = new QPushButton(tr("Sign In via Your Browser"), this);
    external_browser_button->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #333; color: white; font-size: 12px; "
                        "padding: 8px 16px; border-radius: 6px; } "
                        "QPushButton:hover { background-color: #444; }"));
    layout->addWidget(external_browser_button);
    connect(external_browser_button, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://accounts.nintendo.com")));
        status_label->setText(
            tr("Browser opened - after signing in, copy the session_token cookie "
               "and paste it below, then click 'Link Saved Session'"));
        status_label->setStyleSheet(
            QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
        token_input->setFocus();
    });

    layout->addSpacing(6);

    // Instructions
    instructions_label = new QLabel(
        tr("Click 'One-Click Sign In' to log in directly.\n\n"
           "This is the fastest path: sign in once and suyu will try to refresh\n"
           "your Nintendo web purchase history automatically.\n\n"
           "If the embedded browser is unavailable, you can manually\n"
           "paste a session token instead (raw token, full cookie string,\n"
           "or a URL/query containing session_token=...):\n"
           "1. Log in to accounts.nintendo.com in your browser\n"
           "2. Open Developer Tools (F12) > Application > Cookies\n"
           "3. Copy the 'session_token' cookie value\n"
           "4. Paste it below and click 'Link Saved Session'\n\n"
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
    link_button = new QPushButton(tr("Link Saved Session"), this);
    verify_button = new QPushButton(tr("Check Status"), this);
    unlink_button = new QPushButton(tr("Unlink"), this);
    button_row->addWidget(link_button);
    button_row->addWidget(verify_button);
    button_row->addWidget(unlink_button);
    layout->addLayout(button_row);

    sync_library_button = new QPushButton(tr("Sync Game Library"), this);
    sync_library_button->setVisible(false);
    layout->addWidget(sync_library_button);
    connect(sync_library_button, &QPushButton::clicked, this,
            &NintendoAccountDialog::OnSyncLibraryClicked);

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
        external_browser_button->setVisible(false);
        instructions_label->setVisible(false);
        sync_library_button->setVisible(true);
        sync_library_button->setEnabled(true);

        if (!owned_library_.empty()) {
            library_summary_label->setText(tr("Nintendo library contains %n title(s)", "",
                                            static_cast<int>(owned_library_.size())));
            library_summary_label->setVisible(true);
        } else {
            library_summary_label->setText(
                tr("No titles imported yet. Press \"Sync Game Library\" to pull the games "
                   "registered to your consoles from Nintendo."));
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
        external_browser_button->setVisible(true);
        sync_library_button->setVisible(false);
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
    QSettings settings = OpenNintendoSettings();
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.setValue(QStringLiteral("session_token"),
                     Obfuscate(session_token.toUtf8()).toBase64());
    settings.setValue(QStringLiteral("nickname"), nickname);
    settings.setValue(QStringLiteral("user_id"), user_id);
    settings.setValue(QStringLiteral("linked"), true);
    settings.endGroup();
}

void NintendoAccountDialog::LoadCredentials() {
    QSettings settings = OpenNintendoSettings();
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
    QSettings settings = OpenNintendoSettings();
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

void NintendoAccountDialog::ExchangeSessionTokenCode(const QString& session_token_code) {
    progress_bar->setVisible(true);
    status_label->setText(tr("Finishing sign-in..."));
    status_label->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));

    // Second leg of the real PKCE flow: session_token_code + the verifier
    // that produced its challenge -> the actual long-lived session_token,
    // which VerifySessionToken() then exchanges for an access_token exactly
    // as it already did for the manual-paste path.
    const QUrl url(QStringLiteral("https://accounts.nintendo.com/connect/1.0.0/api/session_token"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Host", "accounts.nintendo.com");

    QJsonObject body;
    body[QStringLiteral("client_id")] = QLatin1String(kNintendoClientId);
    body[QStringLiteral("session_token_code")] = session_token_code;
    body[QStringLiteral("session_token_code_verifier")] = pending_code_verifier_;

    if (!network_manager_) {
        network_manager_ = new QNetworkAccessManager(this);
    }
    QNetworkReply* reply =
        network_manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        pending_code_verifier_.clear();
        pending_state_.clear();

        if (reply->error() != QNetworkReply::NoError) {
            progress_bar->setVisible(false);
            status_label->setText(tr("Sign-in failed: network error"));
            status_label->setStyleSheet(
                QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString session_token = obj[QStringLiteral("session_token")].toString();
        if (session_token.isEmpty()) {
            progress_bar->setVisible(false);
            status_label->setText(tr("Sign-in failed: Nintendo did not return a session token"));
            status_label->setStyleSheet(
                QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
            return;
        }

        VerifySessionToken(session_token);
    });
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

void NintendoAccountDialog::FetchNintendoOwnedLibrary(const QString& /*token*/) {
    // Linking alone doesn't populate the library: the game list comes from the
    // Virtual Game Card portal, which is a separate signed-in web page rather
    // than something this OAuth token can query. It's driven by the explicit
    // "Sync Game Library" button so the user isn't surprised by a second
    // browser window appearing right after sign-in.
    if (library_summary_label) {
        library_summary_label->setText(
            tr("Nintendo Account linked. Press \"Sync Game Library\" to import your "
               "Virtual Game Cards."));
        library_summary_label->setVisible(true);
    }
    if (sync_library_button) {
        sync_library_button->setVisible(true);
        sync_library_button->setEnabled(true);
    }
}

void NintendoAccountDialog::StartVgcSync() {
#ifdef SUYU_USE_QT_WEB_ENGINE
    // The Virtual Game Card portal is an ordinary signed-in Nintendo web page.
    // Rather than lifting cookies out into a QNetworkAccessManager (which is
    // what the equivalent Playnite extension has to do, and is easy to get
    // subtly wrong), load it in a WebEngine view and run the portal's own
    // GraphQL query from inside the page - the session then applies by
    // construction.
    //
    // VGCs rather than the purchase/transaction list because they cover free
    // titles and anything added without a transaction, and each entry carries
    // a real applicationId plus Nintendo-hosted icon art.
    vgc_dialog_ = new QDialog(this);
    vgc_dialog_->setWindowTitle(tr("Sync Nintendo Game Library"));
    vgc_dialog_->resize(900, 700);
    vgc_dialog_->setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(vgc_dialog_);
    auto* hint = new QLabel(
        tr("Sign in if prompted. Your Virtual Game Cards will be imported automatically "
           "once the page loads - this window closes by itself when it's done."),
        vgc_dialog_);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #999; font-size: 11px; padding: 4px;"));
    layout->addWidget(hint);

    // Reuse the same named profile as the sign-in flow so an existing session
    // is picked up and the user usually isn't asked to log in twice.
    auto* profile = new QWebEngineProfile(QStringLiteral("NintendoLogin"), vgc_dialog_);
    vgc_view_ = new QWebEngineView(vgc_dialog_);
    vgc_view_->setPage(new QWebEnginePage(profile, vgc_view_));
    layout->addWidget(vgc_view_, 1);

    connect(vgc_view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok || !vgc_view_) {
            return;
        }
        const QString url = vgc_view_->url().toString();
        if (!url.contains(QStringLiteral("/portal/vgcs"))) {
            // Still on a login/interstitial page - wait for the user.
            return;
        }
        // Inject the collector. It stashes its result on window rather than
        // returning it, because runJavaScript() resolves with the expression's
        // immediate value and would otherwise just hand back a pending Promise.
        static const char* kCollector = R"JS(
(function(){
  if (window.__suyu_vgc_running) { return; }
  window.__suyu_vgc_running = true;
  window.__suyu_vgc_result = null;
  const q = `query getVgcs($idToken: String!, $country: CountryCode!, $language: LanguageCode!,
    $shopId: Int!, $limit: Int!, $nasLanguage: String!, $offset: Int!,
    $order: RequestableVgcViewOrder!, $sortBy: RequestableVgcViewSortBy!)
    @inContext(country: $country, language: $language, shopId: $shopId) {
    account { vgc { vgcViews(idToken: $idToken, limit: $limit, nasLanguage: $nasLanguage,
      offset: $offset, order: $order, sortBy: $sortBy, isHidden: false) {
      offsetInfo { total offset }
      views { applicationId applicationName publisher icon { url } }
    } } } }`;
  (async function(){
    try {
      const el = document.querySelector('#data');
      if (!el) { window.__suyu_vgc_result = JSON.stringify({error:'portal layout changed'}); return; }
      const d = JSON.parse(el.getAttribute('data-json'));
      const all = [];
      let offset = 0, total = 0;
      const LIMIT = 50;
      do {
        const r = await fetch(d.shopGraphQLApiUrl, {
          method: 'POST',
          credentials: 'include',
          headers: {
            'Content-Type': 'application/json',
            'x-nintendo-savanna-client-id': d.savannaClientId
          },
          body: JSON.stringify({ query: q, variables: {
            idToken: d.idToken, country: d.country || 'GB', language: d.language || 'en',
            shopId: d.shopId || 3, limit: LIMIT, nasLanguage: d.nasLanguage || 'en-GB',
            offset: offset, order: 'ASC', sortBy: 'ACTIVATED_DATE'
          }})
        });
        const j = await r.json();
        const vv = j && j.data && j.data.account && j.data.account.vgc &&
                   j.data.account.vgc.vgcViews;
        if (!vv) { window.__suyu_vgc_result = JSON.stringify({error: JSON.stringify(j).slice(0,400)}); return; }
        total = vv.offsetInfo.total;
        all.push.apply(all, vv.views);
        offset += LIMIT;
      } while (offset < total && all.length < 2000);
      window.__suyu_vgc_result = JSON.stringify({games: all});
    } catch (e) {
      window.__suyu_vgc_result = JSON.stringify({error: String(e)});
    }
  })();
})();
)JS";
        vgc_view_->page()->runJavaScript(QString::fromUtf8(kCollector));

        // Poll for the stashed result.
        if (!vgc_poll_timer_) {
            vgc_poll_timer_ = new QTimer(this);
            connect(vgc_poll_timer_, &QTimer::timeout, this,
                    &NintendoAccountDialog::PollVgcResult);
        }
        vgc_poll_attempts_ = 0;
        vgc_poll_timer_->start(500);
        library_summary_label->setText(tr("Reading your Virtual Game Cards..."));
    });

    vgc_view_->setUrl(QUrl(QStringLiteral(
        "https://accounts.nintendo.com/portal/vgcs/?sort=activated_date&order=desc")));
    vgc_dialog_->show();
    vgc_dialog_->raise();
    vgc_dialog_->activateWindow();
#else
    library_summary_label->setText(
        tr("Library sync needs the Qt WebEngine build of suyu to sign in to Nintendo."));
    library_summary_label->setVisible(true);
    sync_library_button->setEnabled(true);
#endif
}

void NintendoAccountDialog::PollVgcResult() {
#ifdef SUYU_USE_QT_WEB_ENGINE
    if (!vgc_view_ || !vgc_view_->page()) {
        if (vgc_poll_timer_) vgc_poll_timer_->stop();
        return;
    }
    // ~60s ceiling; a very large library still finishes well inside this.
    if (++vgc_poll_attempts_ > 120) {
        vgc_poll_timer_->stop();
        library_summary_label->setText(
            tr("Timed out reading your Virtual Game Cards. Please try again."));
        sync_library_button->setEnabled(true);
        if (vgc_dialog_) vgc_dialog_->close();
        return;
    }
    vgc_view_->page()->runJavaScript(
        QStringLiteral("window.__suyu_vgc_result"), [this](const QVariant& v) {
            const QString json = v.toString();
            if (json.isEmpty() || json == QStringLiteral("null")) {
                return; // not finished yet
            }
            if (vgc_poll_timer_) vgc_poll_timer_->stop();
            ApplyVgcJson(json);
        });
#endif
}

void NintendoAccountDialog::ApplyVgcJson(const QString& json) {
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();

    if (root.contains(QStringLiteral("error"))) {
        library_summary_label->setText(
            tr("Could not read your Virtual Game Cards: %1")
                .arg(root.value(QStringLiteral("error")).toString()));
        library_summary_label->setVisible(true);
        sync_library_button->setEnabled(true);
        if (vgc_dialog_) vgc_dialog_->close();
        return;
    }

    std::vector<NintendoOwnedGame> collected;
    for (const auto& entry : root.value(QStringLiteral("games")).toArray()) {
        const QJsonObject view = entry.toObject();
        NintendoOwnedGame game;
        game.title = view.value(QStringLiteral("applicationName")).toString();
        game.title_id = view.value(QStringLiteral("applicationId")).toString();
        game.platform = QStringLiteral("Nintendo Switch");
        game.is_digital = true;
        game.icon_url = view.value(QStringLiteral("icon"))
                            .toObject()
                            .value(QStringLiteral("url"))
                            .toString();
        if (game.title.trimmed().isEmpty()) {
            continue;
        }
        // The same title can hold several cards (e.g. lent copies); one entry
        // per game is what the library wants.
        const bool dup = std::any_of(collected.begin(), collected.end(),
                                     [&](const NintendoOwnedGame& g) {
                                         return g.title_id == game.title_id;
                                     });
        if (!dup) {
            collected.push_back(std::move(game));
        }
    }

    owned_library_ = std::move(collected);
    StoreNintendoOwnedLibrary(owned_library_);
    sync_library_button->setEnabled(true);

    if (owned_library_.empty()) {
        library_summary_label->setText(
            tr("Nintendo returned no Virtual Game Cards for this account."));
    } else {
        library_summary_label->setText(
            tr("Imported %1 title(s) from your Nintendo Account.").arg(owned_library_.size()));
    }
    library_summary_label->setVisible(true);
    emit OwnedLibraryUpdated(static_cast<int>(owned_library_.size()));

    if (vgc_dialog_) {
        vgc_dialog_->close();
        vgc_dialog_ = nullptr;
        vgc_view_ = nullptr;
    }
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
    if (FILE* f = fopen("C:\\Users\\charl\\Documents\\SuyuEclipse\\nnid_raw_diag.txt", "a")) {
        fprintf(f, "OpenBrowserLogin entered\n");
        fclose(f);
    }
    LOG_INFO(Frontend, "NNID diag: OpenBrowserLogin() entered");
#ifdef SUYU_USE_QT_WEB_ENGINE
    LOG_INFO(Frontend, "NNID diag: SUYU_USE_QT_WEB_ENGINE branch taken");
    // Constructing a QWebEngineProfile/QWebEngineView synchronously inside
    // the click handler crashed live (0xc0000005 in Qt6Core.dll) - clicking
    // One-Click Sign In from inside this dialog's own nested exec() loop
    // hits WebEngine init/GPU-process bootstrap re-entrancy that isn't safe
    // to do directly from within the click's own call stack. Deferring to
    // the next event loop iteration via a 0ms singleShot lets the click
    // event finish unwinding first, which is the standard fix for this
    // class of QtWebEngine re-entrancy crash.
    if (FILE* f = fopen("C:\\Users\\charl\\Documents\\SuyuEclipse\\nnid_raw_diag.txt", "a")) {
        fprintf(f, "before singleShot schedule, this->isVisible=%d\n", this->isVisible());
        fclose(f);
    }
    QTimer::singleShot(0, this, [this]() {
    if (FILE* f = fopen("C:\\Users\\charl\\Documents\\SuyuEclipse\\nnid_raw_diag.txt", "a")) {
        fprintf(f, "singleShot lambda fired, this->isVisible=%d\n", this->isVisible());
        fclose(f);
    }
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Nintendo Account Sign-In"));
    dialog->resize(900, 700);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    // Neither this dialog nor its parent NintendoAccountDialog should be
    // Qt::ApplicationModal here - both are now shown via show(), not
    // exec(), and stacking two ApplicationModal top-levels (even
    // sequentially, one per QTimer::singleShot(0) tick) reproduced the same
    // silent-disappearance bug that a genuinely non-modal child under an
    // exec()-driven application-modal parent did in an earlier version of
    // this code. Leaving both non-modal (or window-modal at most) avoids
    // Qt's application-modal stack entirely.

    auto* layout = new QVBoxLayout(dialog);

    auto* hint = new QLabel(
        tr("Sign in to your Nintendo Account below. The window will close automatically "
           "once your session token is captured and your library sync begins."),
        dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #999; font-size: 11px; padding: 4px;"));
    layout->addWidget(hint);

    auto* profile = new QWebEngineProfile(QStringLiteral("NintendoLogin"), dialog);
    auto* web_view = new QWebEngineView(dialog);
    layout->addWidget(web_view, 1);

    // Real Nintendo login is OAuth/PKCE, not a "session_token" cookie on
    // accounts.nintendo.com (that cookie never gets set by the real login
    // flow - confirmed this was the actual reason sign-in silently never
    // completed even after the earlier crash/dialog-lifetime fixes). Build
    // the real authorize URL and intercept the npf<client_id>://auth
    // redirect it produces on success.
    pending_code_verifier_ = GeneratePkceVerifier();
    pending_state_ = GeneratePkceVerifier();
    const QString challenge = PkceChallengeFromVerifier(pending_code_verifier_);

    QUrl authorize_url(QStringLiteral("https://accounts.nintendo.com/connect/1.0.0/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("state"), pending_state_);
    query.addQueryItem(QStringLiteral("redirect_uri"),
                        QStringLiteral("npf%1://auth").arg(QLatin1String(kNintendoClientId)));
    query.addQueryItem(QStringLiteral("client_id"), QLatin1String(kNintendoClientId));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("openid user user.mii"));
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("session_token_code"));
    query.addQueryItem(QStringLiteral("session_token_code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("session_token_code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("theme"), QStringLiteral("login_form"));
    authorize_url.setQuery(query);

    auto* login_page = new NintendoLoginPage(profile, web_view);
    web_view->setPage(login_page);
    login_page->on_redirect = [this, dialog](const QUrl& redirect_url) {
        // Fragment, not query - session_token_code arrives after the '#'.
        QUrlQuery fragment(redirect_url.fragment());
        const QString state = fragment.queryItemValue(QStringLiteral("state"));
        const QString session_token_code =
            fragment.queryItemValue(QStringLiteral("session_token_code"));
        dialog->close();
        if (state != pending_state_ || session_token_code.isEmpty()) {
            status_label->setText(tr("Sign-in failed: invalid response from Nintendo"));
            status_label->setStyleSheet(
                QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
            return;
        }
        ExchangeSessionTokenCode(session_token_code);
    };

    web_view->setUrl(authorize_url);
    if (FILE* f = fopen("C:\\Users\\charl\\Documents\\SuyuEclipse\\nnid_raw_diag.txt", "a")) {
        fprintf(f, "about to call dialog->show()\n");
        fclose(f);
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    if (FILE* f = fopen("C:\\Users\\charl\\Documents\\SuyuEclipse\\nnid_raw_diag.txt", "a")) {
        fprintf(f, "after show(), isVisible=%d x=%d y=%d w=%d h=%d\n",
                dialog->isVisible(), dialog->x(), dialog->y(), dialog->width(), dialog->height());
        fclose(f);
    }
    });
#else
    // No WebEngine — open external browser and let user paste token manually
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://accounts.nintendo.com")));
    status_label->setText(
        tr("Browser opened — finish sign-in, then paste the session token below"));
    status_label->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold; color: #ff9800;"));
    token_input->setFocus();
#endif
}

void NintendoAccountDialog::OnSyncLibraryClicked() {
    if (!linked_) {
        return;
    }
    sync_library_button->setEnabled(false);
    library_summary_label->setText(tr("Opening your Nintendo library..."));
    library_summary_label->setVisible(true);

    // Deferred like OpenBrowserLogin(): constructing a WebEngine view directly
    // inside the click handler re-enters WebEngine bootstrap and crashes.
    QTimer::singleShot(0, this, [this]() { StartVgcSync(); });
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
