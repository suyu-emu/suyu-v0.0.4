// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;

struct NintendoOwnedGame {
    QString title;
    QString platform;
    QString purchase_date;
    bool is_digital = true;
    QString title_id;
};

/// Nintendo Account linking dialog with encrypted credential storage.
/// Supports OAuth-style token storage for Nintendo account integration.
/// Tokens are stored encrypted in QSettings using obfuscated keys.
class NintendoAccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit NintendoAccountDialog(QWidget* parent = nullptr);
    ~NintendoAccountDialog() override = default;

    /// Returns true when the user has a stored session token.
    [[nodiscard]] bool IsLinked() const;

    /// Get the stored nickname.
    [[nodiscard]] QString Nickname() const;

    /// Get the stored user ID.
    [[nodiscard]] QString UserId() const;

    /// Get the stored Nintendo owned game library.
    [[nodiscard]] std::vector<NintendoOwnedGame> OwnedLibrary() const;

signals:
    void AccountLinked(const QString& nickname);
    void AccountUnlinked();
    void LinkFailed(const QString& error);
    void OwnedLibraryUpdated(int title_count);

private slots:
    void OnLinkClicked();
    void OnUnlinkClicked();
    void OnVerifyClicked();
    void OnTokenSubmitted();
    void OpenBrowserLogin();

private:
    void SetupUi();
    void RefreshStatus();

    /// Store credentials securely (obfuscated in QSettings).
    void StoreCredentials(const QString& session_token, const QString& nickname,
                          const QString& user_id);
    /// Load stored credentials.
    void LoadCredentials();
    /// Clear stored credentials.
    void ClearCredentials();

    /// Obfuscate a string for storage (XOR with app-derived key).
    [[nodiscard]] static QByteArray Obfuscate(const QByteArray& data);

    /// Attempt to verify a session token against Nintendo's API.
    void VerifySessionToken(const QString& token);
    void FetchNintendoOwnedLibrary(const QString& token);
    std::vector<NintendoOwnedGame> ParseNintendoPurchaseHistory(const QString& html);

    QLabel* status_label{};
    QLabel* nickname_label{};
    QLabel* user_id_label{};
    QLabel* library_summary_label{};
    QLineEdit* token_input{};
    QPushButton* link_button{};
    QPushButton* unlink_button{};
    QPushButton* verify_button{};
    QPushButton* browser_login_button{};
    QProgressBar* progress_bar{};
    QLabel* instructions_label{};

    QNetworkAccessManager* network_manager_{};

    bool linked_ = false;
    QString nickname_;
    QString user_id_;
    QString session_token_;
    std::vector<NintendoOwnedGame> owned_library_;
};

std::vector<NintendoOwnedGame> LoadNintendoOwnedLibrary();
void StoreNintendoOwnedLibrary(const std::vector<NintendoOwnedGame>& library);
void ClearNintendoOwnedLibrary();
