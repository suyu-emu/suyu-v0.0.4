// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdio>
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
    /// Nintendo-hosted cover art URL (the Virtual Game Card's icon.url).
    /// Empty when the entry came from a source that doesn't carry artwork.
    QString icon_url;
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

    /// Test-only hook: invokes the same slot the One-Click Sign In button's
    /// clicked() signal is connected to, bypassing widget click delivery, so
    /// automation can tell whether the handler itself runs versus the click
    /// never reaching the button.
    void TriggerOneClickSignInForTesting() {
        OpenBrowserLogin();
    }

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
    /// Imports the account's Virtual Game Cards. See StartVgcSync().
    void OnSyncLibraryClicked();

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

    /// Real Nintendo login is OAuth/PKCE, not a plain cookie: navigating to
    /// the authorize URL below and intercepting the npf...://auth redirect
    /// yields a session_token_code, which is exchanged here (with the PKCE
    /// verifier) for the actual long-lived session_token that
    /// VerifySessionToken() already knows how to use.
    void ExchangeSessionTokenCode(const QString& session_token_code);
    QString pending_code_verifier_;
    QString pending_state_;

    /// Virtual Game Card sync. Nintendo's VGC portal is a normal signed-in web
    /// page, so rather than driving an API we load it in the same WebEngine
    /// profile the user signed in through and run the portal's own GraphQL
    /// query from inside the page - session cookies then apply automatically
    /// and there's no token/cookie plumbing to get wrong. VGCs cover free
    /// titles too, which a purchase/transaction list misses.
    void StartVgcSync();
    void PollVgcResult();
    void ApplyVgcJson(const QString& json);
    class QWebEngineView* vgc_view_ = nullptr;
    class QDialog* vgc_dialog_ = nullptr;
    class QTimer* vgc_poll_timer_ = nullptr;
    int vgc_poll_attempts_ = 0;

    QLabel* status_label{};
    QLabel* nickname_label{};
    QLabel* user_id_label{};
    QLabel* library_summary_label{};
    QLineEdit* token_input{};
    QPushButton* link_button{};
    QPushButton* unlink_button{};
    QPushButton* verify_button{};
    QPushButton* browser_login_button{};
    QPushButton* external_browser_button{};
    QPushButton* sync_library_button{};
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
