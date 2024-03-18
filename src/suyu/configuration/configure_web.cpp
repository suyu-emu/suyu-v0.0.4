// SPDX-FileCopyrightText: 2017 Citra Emulator Project & 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QIcon>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include "common/settings.h"
#include "suyu/configuration/configure_web.h"
#include "suyu/uisettings.h"
#include "ui_configure_web.h"
#include "web_service/verify_login.h"

static constexpr char token_delimiter{':'};

static std::string GenerateDisplayToken(const std::string& username, const std::string& token) {
    if (username.empty() || token.empty()) {
        return {};
    }

    const std::string unencoded_display_token{username + token_delimiter + token};
    QByteArray b{unencoded_display_token.c_str()};
    QByteArray b64 = b.toBase64();
    return b64.toStdString();
}

static std::string UsernameFromDisplayToken(const std::string& display_token) {
    const std::string unencoded_display_token{
        QByteArray::fromBase64(display_token.c_str()).toStdString()};
    return unencoded_display_token.substr(0, unencoded_display_token.find(token_delimiter));
}

static std::string TokenFromDisplayToken(const std::string& display_token) {
    const std::string unencoded_display_token{
        QByteArray::fromBase64(display_token.c_str()).toStdString()};
    return unencoded_display_token.substr(unencoded_display_token.find(token_delimiter) + 1);
}

ConfigureWeb::ConfigureWeb(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureWeb>()) {
    ui->setupUi(this);
    connect(ui->button_verify_login, &QPushButton::clicked, this, &ConfigureWeb::VerifyLogin);
    connect(&verify_watcher, &QFutureWatcher<bool>::finished, this, &ConfigureWeb::OnLoginVerified);

#ifndef USE_DISCORD_PRESENCE
    ui->discord_group->setVisible(false);
#endif

    SetConfiguration();
    RetranslateUI();
}

ConfigureWeb::~ConfigureWeb() = default;

void ConfigureWeb::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureWeb::RetranslateUI() {
    ui->retranslateUi(this);

    ui->web_signup_link->setText(
        tr("<a href='https://profile.suyu.dev/'><span style=\"text-decoration: underline; "
           "color:#039be5;\">Sign up</span></a>"));

    ui->web_token_info_link->setText(
        tr("<a href='https://suyu.dev/wiki/suyu-web-service/'><span style=\"text-decoration: "
           "underline; color:#039be5;\">What is my token?</span></a>"));
}

void ConfigureWeb::SetConfiguration() {
    ui->web_credentials_disclaimer->setWordWrap(true);

    ui->web_signup_link->setOpenExternalLinks(true);
    ui->web_token_info_link->setOpenExternalLinks(true);

    if (Settings::values.suyu_username.GetValue().empty()) {
        ui->username->setText(tr("Unspecified"));
    } else {
        ui->username->setText(QString::fromStdString(Settings::values.suyu_username.GetValue()));
    }

    ui->edit_token->setText(QString::fromStdString(GenerateDisplayToken(
        Settings::values.suyu_username.GetValue(), Settings::values.suyu_token.GetValue())));

    // Connect after setting the values, to avoid calling OnLoginChanged now
    connect(ui->edit_token, &QLineEdit::textChanged, this, &ConfigureWeb::OnLoginChanged);

    user_verified = true;

    ui->toggle_discordrpc->setChecked(UISettings::values.enable_discord_presence.GetValue());
}

void ConfigureWeb::ApplyConfiguration() {
    UISettings::values.enable_discord_presence = ui->toggle_discordrpc->isChecked();
    if (user_verified) {
        Settings::values.suyu_username =
            UsernameFromDisplayToken(ui->edit_token->text().toStdString());
        Settings::values.suyu_token = TokenFromDisplayToken(ui->edit_token->text().toStdString());
    } else {
        QMessageBox::warning(
            this, tr("Token not verified"),
            tr("Token was not verified. The change to your token has not been saved."));
    }
}

void ConfigureWeb::OnLoginChanged() {
    if (ui->edit_token->text().isEmpty()) {
        user_verified = true;
        // Empty = no icon
        ui->label_token_verified->setPixmap(QPixmap());
        ui->label_token_verified->setToolTip(QString());
    } else {
        user_verified = false;

        // Show an info icon if it's been changed, clearer than showing failure
        const QPixmap pixmap = QIcon::fromTheme(QStringLiteral("info")).pixmap(16);
        ui->label_token_verified->setPixmap(pixmap);
        ui->label_token_verified->setToolTip(
            tr("Unverified, please click Verify before saving configuration", "Tooltip"));
    }
}

void ConfigureWeb::VerifyLogin() {
    ui->button_verify_login->setDisabled(true);
    ui->button_verify_login->setText(tr("Verifying..."));
    ui->label_token_verified->setPixmap(QIcon::fromTheme(QStringLiteral("sync")).pixmap(16));
    ui->label_token_verified->setToolTip(tr("Verifying..."));
    verify_watcher.setFuture(QtConcurrent::run(
        [username = UsernameFromDisplayToken(ui->edit_token->text().toStdString()),
         token = TokenFromDisplayToken(ui->edit_token->text().toStdString())] {
#ifdef ENABLE_WEB_SERVICE
            return WebService::VerifyLogin(Settings::values.web_api_url.GetValue(), username,
                                           token);
#else
            return false;
#endif
        }));
}

void ConfigureWeb::OnLoginVerified() {
    ui->button_verify_login->setEnabled(true);
    ui->button_verify_login->setText(tr("Verify"));
    if (verify_watcher.result()) {
        user_verified = true;

        ui->label_token_verified->setPixmap(QIcon::fromTheme(QStringLiteral("checked")).pixmap(16));
        ui->label_token_verified->setToolTip(tr("Verified", "Tooltip"));
        ui->username->setText(
            QString::fromStdString(UsernameFromDisplayToken(ui->edit_token->text().toStdString())));
    } else {
        ui->label_token_verified->setPixmap(QIcon::fromTheme(QStringLiteral("failed")).pixmap(16));
        ui->label_token_verified->setToolTip(tr("Verification failed", "Tooltip"));
        ui->username->setText(tr("Unspecified"));
        QMessageBox::critical(this, tr("Verification failed"),
                              tr("Verification failed. Check that you have entered your token "
                                 "correctly, and that your internet connection is working."));
    }
}

void ConfigureWeb::SetWebServiceConfigEnabled(bool enabled) {
    ui->label_disable_info->setVisible(!enabled);
    ui->groupBoxWebConfig->setEnabled(enabled);
}
