// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <QIcon>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>

class GameList;
class GMainWindow;
class QNetworkAccessManager;
class QNetworkReply;
#ifdef SUYU_USE_QT_WEB_ENGINE
class QWebEngineView;
#endif
#ifdef SUYU_USE_QT_MULTIMEDIA
class QMediaPlayer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioOutput;
#endif
#endif

// Custom delegate that renders each game as a card with cover art + info
class GameCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit GameCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    static constexpr int CARD_W = 168;
    static constexpr int CARD_H = 248;
    static constexpr int ICON_H = 180;
    static constexpr int PAD    = 6;
};

// Full-screen Gamer mode widget with gradient background, left nav sidebar, and game card grid.
// Reads game data from the hidden GameList model so it doesn't need its own file-watching logic.
class GamerEnvironment : public QWidget {
    Q_OBJECT

public:
    explicit GamerEnvironment(GameList* game_list, GMainWindow* parent = nullptr);
    ~GamerEnvironment() override;

    // Re-populate the game grid from the underlying GameList model.
    void RefreshGameGrid();

    // Display e.g. "suyu 0.0.1" under the logo
    void SetVersionString(const QString& version);

    // Expose current UI state for MCP inspection.
    QJsonObject GetMcpState() const;
    void RefreshSocialFeed();
    // Test-only: navigate the Social page's embedded view directly, to
    // verify page-specific behavior (e.g. the login page skin) without
    // needing to click an in-page link.
    void DebugNavigateSocial(const QString& url);

signals:
    void GameLaunchRequested(const QString& path);
    void AddDirectoryRequested();
    void LoadFileRequested();
    void OpenSettingsRequested();
    void OpenMultiplayerRequested();
    void OpenUserManualRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void OnGameDoubleClicked(QListWidgetItem* item);
    void OnGameContextMenu(const QPoint& pos);
    void OnSearchChanged(const QString& text);
    void OnAddGameClicked();
    void OnLoadGameClicked();
    void OnNavLibraryClicked();
    void OnNavSettingsClicked();
    void OnNavMultiplayerClicked();
    void OnNavSocialClicked();
    void OnNavMoreOptionsClicked();
    void OnNavWebsiteClicked();
    void OnNavManualClicked();
    void OnRedditFeedFinished(QNetworkReply* reply);
    void OnModelRowsInserted(const QModelIndex& parent, int first, int last);
    void OnModelReset();

private:
    void SetupUI();
    void SetupSidebar(QHBoxLayout* root_layout);
    void SetupMainContent(QHBoxLayout* root_layout);
    QWidget* BuildLibraryPage();
    QWidget* BuildSocialPage();
    void LoadRedditFeed();
    void FetchRedditAccessToken();
    QPushButton* CreateNavButton(const QString& icon_text, const QString& label,
                                 bool active = false,
                                 const QIcon& svg_icon = QIcon());
    void StartSocialMusic();
    void StopSocialMusic();
    QString LoadingScreenTileDataUri() const;
    void ApplyNavSelection(QPushButton* btn);
    void PopulateFromModel();
    void ShowGameMenu(QListWidgetItem* item, const QPoint& global_pos);
    void RequestCoverArtwork(const QString& game_path, const QString& title);
    void ApplyCoverToItem(const QString& game_path, const QIcon& icon);
    QString CoverCachePathForTitle(const QString& title) const;
    QString ExtractIgdbImageUrl(const QString& html) const;
    // ── Sidebar ──────────────────────────────────────────────────────────────
    QWidget*              sidebar_{};
    QVector<QPushButton*> nav_buttons_;
    QPushButton*          active_nav_btn_{};
    QLabel*               version_label_{};

    // ── Content stack ────────────────────────────────────────────────────────
    QStackedWidget* content_stack_{};

    // Library page (index 0)
    QWidget*    library_page_{};
    QLineEdit*  search_bar_{};
    QListWidget* game_grid_{};
    QLabel*     stats_label_{};
    QLabel*     empty_label_{};

    // Social page (index 1)
    QWidget*      social_page_{};
    QTextBrowser* social_browser_{};
    QPushButton*  social_refresh_btn_{};
    QPushButton*  social_post_btn_{};
    // Back/Refresh/New Post/Music are all rendered as pills INSIDE the
    // custom Miiverse header (in the QWebEngineView's own page), not as
    // separate native widgets above it, so there is no visible native
    // bar sitting outside the widget. Music playback is still driven by
    // a native QMediaPlayer (JS can't touch OS audio), so its on/off
    // state is tracked here rather than via a native checkable button.
    bool social_music_enabled_{true};
    bool social_was_on_login_page_{false};
#ifdef SUYU_USE_QT_MULTIMEDIA
    QMediaPlayer* social_music_player_{};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput* social_music_output_{};
#endif
#endif
    QNetworkAccessManager* reddit_network_manager_{};
    QNetworkReply* reddit_reply_{};
    QNetworkReply* reddit_token_reply_{};
    QString reddit_access_token_{};
#ifdef SUYU_USE_QT_WEB_ENGINE
    QWebEngineView* social_web_view_{};
#endif
    QNetworkAccessManager* cover_network_manager_{};
    QHash<QString, QIcon> cover_icon_cache_{};
    QSet<QString> cover_requests_in_flight_{};
    QString social_feed_status_{QStringLiteral("idle")};
    QString social_feed_error_{};
    int social_post_count_{};
    QDateTime social_last_updated_{};

    // Bottom action buttons (inside library page)
    QPushButton* add_game_btn_{};
    QPushButton* load_game_btn_{};

    // ── Data ─────────────────────────────────────────────────────────────────
    GameList*    game_list_{};
    GMainWindow* main_window_{};
    QString      filter_text_;
    QString      version_string_{QStringLiteral("suyu")};
};
