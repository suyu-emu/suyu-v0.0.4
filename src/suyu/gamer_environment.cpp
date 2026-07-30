// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/gamer_environment.h"
#include "suyu/nintendo_account.h"
#include "common/logging.h"

#include <QAbstractItemModel>
#include <QSet>
#include <QApplication>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QBrush>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QPainterPath>
#include <QPixmap>
#include <QTabWidget>
#include <QRadialGradient>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>

#include "suyu/game_list.h"
#include "suyu/uisettings.h"

#ifdef SUYU_USE_QT_WEB_ENGINE
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineHistory>
#include <QDialog>
#endif
#ifdef SUYU_USE_QT_MULTIMEDIA
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#include <QMediaPlayer>
#else
#include <QMediaContent>
#include <QMediaPlayer>
#endif
#endif
#include "suyu/game_list_p.h"
#include "suyu/main.h"

namespace {

#ifdef SUYU_USE_QT_WEB_ENGINE
// The default QWebEnginePage silently drops any navigation that tries to
// open a new window/tab (window.open(), target="_blank", or a JS-driven
// popup) - it does nothing at all unless createWindow() is overridden,
// which is exactly why "Continue with Google" (and other SSO providers,
// which all popup an OAuth window) previously just stopped dead with no
// visible error. Overriding it to hand back a page backed by the SAME
// profile (so it shares login cookies) inside a small dialog lets the
// OAuth flow actually complete; the dialog closes itself and reloads the
// opener once the popup navigates back to reddit.com (the provider
// redirects there on success).
class SuyuWebPopupPage : public QWebEnginePage {
public:
    explicit SuyuWebPopupPage(QWebEngineProfile* profile, QWebEngineView* opener_view)
        : QWebEnginePage(profile, opener_view), opener_view_(opener_view) {}

protected:
    QWebEnginePage* createWindow(QWebEnginePage::WebWindowType /*type*/) override {
        auto* dialog = new QDialog(opener_view_);
        dialog->setWindowTitle(QObject::tr("Sign In"));
        dialog->resize(560, 680);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setModal(true);
        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* popup_view = new QWebEngineView(dialog);
        auto* popup_page = new SuyuWebPopupPage(profile(), popup_view);
        popup_view->setPage(popup_page);
        layout->addWidget(popup_view);

        QObject::connect(popup_page, &QWebEnginePage::urlChanged, dialog,
                          [dialog, this](const QUrl& url) {
                              // The OAuth provider redirects back to reddit.com
                              // once sign-in succeeds - close the popup and
                              // reload the opener so it picks up the new
                              // session cookie.
                              if (url.host().contains(QStringLiteral("reddit.com"))) {
                                  dialog->close();
                                  if (opener_view_) {
                                      opener_view_->reload();
                                  }
                              }
                          });
        dialog->show();
        return popup_page;
    }

    bool acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType type,
                                 bool isMainFrame) override {
        // Back/Refresh/New Post/Music were originally wired via
        // location.hash changes clicked from injected JS, but that touches
        // the page's real navigation history - a hash change IS a history
        // entry, so calling history()->back() right after just undoes that
        // same hash push and never reaches the actual previous real page
        // (confirmed live: Back silently no-op'd, landing back on the same
        // page minus the hash instead of going to r/suyu). Routing pill
        // clicks through a fake https://suyu-action/<name> URL instead and
        // intercepting it here means clicking Back never touches history at
        // all, so QWebEngineHistory reflects only genuine page navigations.
        if (url.host() == QStringLiteral("suyu-action") && action_callback) {
            action_callback(url.path());
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

public:
    std::function<void(const QString&)> action_callback;

private:
    QWebEngineView* opener_view_;
};
#endif

QIcon DecorationToIcon(const QVariant& decoration) {
    if (decoration.canConvert<QIcon>()) {
        const QIcon icon = qvariant_cast<QIcon>(decoration);
        if (!icon.isNull()) {
            return icon;
        }
    }

    if (decoration.canConvert<QPixmap>()) {
        const QPixmap pixmap = qvariant_cast<QPixmap>(decoration);
        if (!pixmap.isNull()) {
            return QIcon(pixmap);
        }
    }

    return {};
}

// The list view's Qt::DecorationRole pixmap is pre-scaled to the small list
// icon size (default 64 logical px). Upscaling that to fill a much larger
// tile card looks soft. Prefer the raw NACP icon bytes (native ~256x256,
// stashed by GameListItemPath) and decode a pixmap sized for this tile
// directly, so the card always renders from full-resolution source data.
QPixmap TileArtwork(const QModelIndex& index, const QSize& target) {
    const QByteArray raw = index.data(GameListItemPath::RawIconRole).toByteArray();
    if (!raw.isEmpty()) {
        QPixmap source;
        if (source.loadFromData(reinterpret_cast<const uchar*>(raw.constData()),
                                static_cast<uint>(raw.size()))) {
            const qreal dpr = QGuiApplication::primaryScreen()
                                  ? QGuiApplication::primaryScreen()->devicePixelRatio()
                                  : 1.0;
            QPixmap scaled = source.scaled(target * dpr, Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(dpr);
            return scaled;
        }
    }

    // Fallback for items with no RawIconRole (e.g. the Library grid's
    // QListWidgetItems, which carry IGDB cover art fetched at a genuinely
    // high resolution via setIcon() - but QIcon::pixmap(target) alone is
    // DPI-unaware and returns a 1x-scaled pixmap on HiDPI displays, then
    // gets stretched to fill the physically-larger tile, causing the same
    // softness the RawIconRole path above exists to avoid.
    const QIcon icon = DecorationToIcon(index.data(Qt::DecorationRole));
    if (icon.isNull()) {
        return {};
    }
    const qreal dpr =
        QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap scaled = icon.pixmap((QSizeF(target) * dpr).toSize());
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

} // namespace

// ─── GameCardDelegate ─────────────────────────────────────────────────────────

GameCardDelegate::GameCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void GameCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Account titles with no local ROM keep a "nintendo://" path. Setting the
    // item's foreground only dims its text, which left the cover art at full
    // strength and made an unplayable entry look identical to a playable one.
    // Dimming the whole card is what actually reads as "you don't have this".
    const bool unplayable =
        index.data(Qt::UserRole).toString().startsWith(QStringLiteral("nintendo://"));
    if (unplayable) {
        painter->setOpacity(0.45);
    }

    const QRect r = option.rect.adjusted(PAD, PAD, -PAD, -PAD);

    // ── Card background ───────────────────────────────────────────────────────
    QPainterPath cardPath;
    cardPath.addRoundedRect(r, 10, 10);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    QColor bg;
    if (selected) {
        bg = QColor(180, 0, 110, 200);
    } else if (hovered) {
        bg = QColor(255, 255, 255, 55);
    } else {
        bg = QColor(255, 255, 255, 28);
    }
    painter->fillPath(cardPath, bg);

    // Subtle border
    painter->setPen(QPen(QColor(255, 255, 255, selected ? 80 : 30), 1.0));
    painter->drawPath(cardPath);

    // ── Cover art ─────────────────────────────────────────────────────────────
    QRect iconRect(r.left(), r.top(), r.width(), ICON_H);
    QPainterPath iconClip;
    iconClip.addRoundedRect(iconRect, 10, 10);

    // Squared bottom of icon rect so it blends into the card body
    QRect sqBottom = iconRect.adjusted(0, iconRect.height() / 2, 0, 0);
    QPainterPath sqPath;
    sqPath.addRect(sqBottom);
    iconClip = iconClip.united(sqPath);
    iconClip = iconClip.intersected(cardPath);

    painter->setClipPath(iconClip);

    const QPixmap pix = TileArtwork(index, QSize(iconRect.width(), ICON_H));
    if (!pix.isNull()) {
        // pix already carries a devicePixelRatio tag from TileArtwork (sized
        // in physical pixels for HiDPI sharpness). QPixmap::scaled()'s target
        // size argument is in raw/physical pixels and does NOT re-derive from
        // an existing DPR tag, so scaling to iconRect.size() (logical) here
        // would silently shrink the image to iconRect.size()/dpr once drawn -
        // exactly the "shrunk into the top-left corner" symptom. Scale in
        // physical pixels explicitly, then (re)tag the result.
        const qreal dpr = pix.devicePixelRatio();
        const QSize physical_target(qRound(iconRect.width() * dpr), qRound(ICON_H * dpr));
        QPixmap scaled = pix.scaled(physical_target, Qt::KeepAspectRatioByExpanding,
                                    Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        const QPoint offset(
            qRound((iconRect.width() - scaled.width() / dpr) / 2.0),
            qRound((ICON_H - scaled.height() / dpr) / 2.0));
        painter->drawPixmap(iconRect.topLeft() + offset, scaled);
    } else {
        // Placeholder gradient when no icon
        QLinearGradient ph(iconRect.topLeft(), iconRect.bottomRight());
        ph.setColorAt(0, QColor(70, 15, 110));
        ph.setColorAt(1, QColor(130, 20, 80));
        painter->fillRect(iconRect, ph);
    }

    painter->setClipping(false);

    // Rounded translucent info panel at the bottom of the card
    const QRect infoPanelRect(r.left() + 6, r.bottom() - 72, r.width() - 12, 64);
    QLinearGradient infoPanelGradient(infoPanelRect.topLeft(), infoPanelRect.bottomLeft());
    infoPanelGradient.setColorAt(0.0, QColor(0, 0, 0, 30));
    infoPanelGradient.setColorAt(1.0, QColor(0, 0, 0, 140));
    painter->setBrush(infoPanelGradient);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(infoPanelRect, 10, 10);

    // Slight top glow inside info panel
    painter->setBrush(QColor(255, 255, 255, 20));
    painter->drawRoundedRect(infoPanelRect.adjusted(0, 0, 0, -infoPanelRect.height() / 2), 10, 10);

    // ── Game title ────────────────────────────────────────────────────────────
    const QString title = index.data(Qt::DisplayRole).toString();
    QFont titleFont = painter->font();
    titleFont.setPixelSize(11);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(Qt::white);

    QRect titleRect(r.left() + 14, r.bottom() - 62, r.width() - 28, 26);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      title);

    // ── Size / play-time / info ─────────────────────────────────────────────────
    const QString info = index.data(Qt::UserRole + 10).toString();
    if (!info.isEmpty()) {
        QFont infoFont = painter->font();
        infoFont.setPixelSize(9);
        infoFont.setBold(false);
        painter->setFont(infoFont);
        painter->setPen(QColor(220, 190, 240, 200));

        QRect infoRect(r.left() + 14, r.bottom() - 34, r.width() - 28, 18);
        painter->drawText(infoRect, Qt::AlignLeft | Qt::AlignVCenter, info);
    }

    // More pill indicator
    const QRect pillRect(r.right() - 64, r.bottom() - 34, 52, 18);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 28));
    painter->drawRoundedRect(pillRect, 9, 9);
    painter->setPen(QColor(255, 255, 255, 180));
    QFont pillFont = painter->font();
    pillFont.setPixelSize(8);
    painter->setFont(pillFont);
    painter->drawText(pillRect, Qt::AlignCenter, tr("More"));

    painter->restore();
}

QSize GameCardDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(CARD_W + PAD * 2, CARD_H + PAD * 2);
}

// ─── GamerEnvironment ─────────────────────────────────────────────────────────

namespace {
QPixmap LoadSidebarBrandLogo(int target_width, int target_height) {
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(cwd).filePath(QStringLiteral("img/suyu_logo_variant_alt.png")),
        QDir(cwd).filePath(QStringLiteral("img/suyu_logo_variant_primary.png")),
        QDir(app_dir).filePath(QStringLiteral("branding/suyu_logo_variant_alt.png")),
        QDir(app_dir).filePath(QStringLiteral("branding/suyu_logo_variant_primary.png")),
        QStringLiteral(":/img/suyu_logo.svg"),
        QStringLiteral(":/img/suyu.svg"),
    };

    for (const QString& path : candidates) {
        if (!path.startsWith(QLatin1String(":/")) && !QFileInfo::exists(path)) {
            continue;
        }
        QPixmap px(path);
        if (!px.isNull()) {
            return px.scaled(target_width, target_height, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
        }
    }

    return {};
}

QString NormalizeLaunchPath(const QString& path) {
    if (path.trimmed().isEmpty() || path.startsWith(QStringLiteral("owned://"))) {
        return {};
    }

    const QFileInfo file_info(path);
    if (!file_info.exists()) {
        return {};
    }

    if (file_info.isDir()) {
        const QString main_path = QDir(path).filePath(QStringLiteral("main"));
        if (QFileInfo::exists(main_path)) {
            return main_path;
        }
        return {};
    }

    return file_info.absoluteFilePath();
}
} // namespace

// Role constants (matching game_list_p.h, without pulling in that header)
static constexpr int kGLItemTypeRole  = Qt::UserRole + 1;  // GameListItem::TypeRole
static constexpr int kGLTitleRole     = Qt::UserRole + 3;  // GameListItemPath::TitleRole
static constexpr int kGLPathRole      = Qt::UserRole + 4;  // GameListItemPath::FullPathRole
static constexpr int kGLGameItemType  = 1001;              // GameListItemType::Game

// ── Constructor / destructor ────────────────────────────────────────────────

GamerEnvironment::GamerEnvironment(GameList* game_list, GMainWindow* parent)
    : QWidget(parent), game_list_(game_list), main_window_(parent) {
    setObjectName(QStringLiteral("GamerEnvironment"));

    // Allow the gradient paint-event background to show through child widgets
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);

    // Slow repaint driving the drifting background marks. 20fps is plenty for
    // something this faint and this slow, and keeps it off the critical path.
    ambient_clock_.start();
    ambient_timer_ = new QTimer(this);
    connect(ambient_timer_, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    ambient_timer_->start(50);

    // Initialize the network manager BEFORE SetupUI() so that BuildSocialPage()
    // can call LoadRedditFeed() and the request will actually be dispatched.
    reddit_network_manager_ = new QNetworkAccessManager(this);
    connect(reddit_network_manager_, &QNetworkAccessManager::finished,
            this, &GamerEnvironment::OnRedditFeedFinished);
    cover_network_manager_ = new QNetworkAccessManager(this);

    SetupUI();

    // Connect to model changes so the grid refreshes automatically
    if (game_list_) {
        auto* model = game_list_->GetModel();
        if (model) {
            connect(model, &QAbstractItemModel::rowsInserted, this,
                    &GamerEnvironment::OnModelRowsInserted,
                    Qt::QueuedConnection);
            connect(model, &QAbstractItemModel::rowsRemoved, this,
                    &GamerEnvironment::OnModelRowsInserted,
                    Qt::QueuedConnection);
            connect(model, &QAbstractItemModel::modelReset, this,
                    &GamerEnvironment::OnModelReset,
                    Qt::QueuedConnection);
        }

        connect(game_list_, &GameList::PopulatingCompleted, this,
                &GamerEnvironment::RefreshGameGrid, Qt::QueuedConnection);
    }

    // Ensure the library grid reflects the current model immediately.
    RefreshGameGrid();
}

GamerEnvironment::~GamerEnvironment() = default;

// ── Public API ───────────────────────────────────────────────────────────────

void GamerEnvironment::RefreshGameGrid() {
    PopulateFromModel();
}

void GamerEnvironment::SetVersionString(const QString& version) {
    version_string_ = version;
    if (version_label_) {
        version_label_->setText(version);
    }
}

QJsonObject GamerEnvironment::GetMcpState() const {
    QJsonObject state;
    state[QStringLiteral("visible_game_count")] = game_grid_ ? game_grid_->count() : 0;
    state[QStringLiteral("search_filter")] = filter_text_;
    state[QStringLiteral("current_view")] = content_stack_ && content_stack_->currentIndex() == 1
        ? QStringLiteral("social")
        : QStringLiteral("library");
    state[QStringLiteral("social_feed_status")] = social_feed_status_;
    state[QStringLiteral("social_feed_error")] = social_feed_error_;
    state[QStringLiteral("social_post_count")] = social_post_count_;
    state[QStringLiteral("social_last_updated")] = social_last_updated_.isValid()
        ? social_last_updated_.toString(Qt::ISODate)
        : QString();
    return state;
}

void GamerEnvironment::RefreshSocialFeed() {
#ifdef SUYU_USE_QT_WEB_ENGINE
    // LoadRedditFeed() below is the legacy OAuth-JSON path used only by the
    // non-WebEngine fallback build (it operates on social_browser_, which is
    // never created when WebEngine is compiled in). This public entry point
    // is called by external automation (e.g. the refresh_social_feed MCP
    // tool), not just the in-UI Refresh button, so it must reload the actual
    // embedded view rather than silently no-op'ing against the wrong widget.
    if (social_web_view_) {
        social_web_view_->reload();
        return;
    }
#endif
    LoadRedditFeed();
}

void GamerEnvironment::DebugNavigateSocial(const QString& url) {
#ifdef SUYU_USE_QT_WEB_ENGINE
    if (social_web_view_) {
        social_web_view_->setUrl(QUrl(url));
    }
#endif
}

// ── UI construction ──────────────────────────────────────────────────────────

void GamerEnvironment::SetupUI() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    SetupSidebar(root);
    SetupMainContent(root);
}

// ─────────────────────────────────────────────────────────────────────────────
// Left navigation sidebar (220 px)
// ─────────────────────────────────────────────────────────────────────────────
void GamerEnvironment::SetupSidebar(QHBoxLayout* root_layout) {
    sidebar_ = new QWidget(this);
    sidebar_->setFixedWidth(240);
    sidebar_->setObjectName(QStringLiteral("gamerSidebar"));
    // Background is drawn per-pixel via GamerEnvironment::paintEvent so the
    // sidebar widget itself should be transparent.
    sidebar_->setAttribute(Qt::WA_TranslucentBackground);

    auto* vl = new QVBoxLayout(sidebar_);
    vl->setContentsMargins(16, 28, 16, 24);
    vl->setSpacing(4);

    // ── Logo / version ───────────────────────────────────────────────────────
    auto* logo = new QLabel(sidebar_);
    logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    const QPixmap logo_px = LoadSidebarBrandLogo(170, 62);
    if (!logo_px.isNull()) {
        logo->setPixmap(logo_px);
    } else {
        logo->setText(QStringLiteral("suyu"));
        QFont f = logo->font();
        f.setPixelSize(26);
        f.setBold(true);
        logo->setFont(f);
        logo->setStyleSheet(QStringLiteral(
            "color: white;"
            "letter-spacing: 2px;"
        ));
    }
    vl->addWidget(logo);

    version_label_ = new QLabel(version_string_, sidebar_);
    version_label_->setStyleSheet(QStringLiteral("color: rgba(255,255,255,140); font-size: 10px;"));
    version_label_->setContentsMargins(2, 0, 0, 0);
    vl->addWidget(version_label_);

    vl->addSpacing(18);

    // ── Primary navigation ───────────────────────────────────────────────────
    auto* lib_btn = CreateNavButton(QStringLiteral("\u25a3"), tr("Library"), /*active=*/true,
                                    QIcon(QStringLiteral(":/icons/folder.svg")));
    connect(lib_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavLibraryClicked);
    vl->addWidget(lib_btn);
    active_nav_btn_ = lib_btn;

    auto* set_btn = CreateNavButton(QStringLiteral("\u2699"), tr("Settings"), /*active=*/false,
                                    QIcon(QStringLiteral(":/icons/settings.svg")));
    connect(set_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavSettingsClicked);
    vl->addWidget(set_btn);

    auto* mp_btn = CreateNavButton(QStringLiteral("\u2295"), tr("Multiplayer"));
    connect(mp_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavMultiplayerClicked);
    vl->addWidget(mp_btn);

    auto* soc_btn = CreateNavButton(QStringLiteral("\u25cf\u25cf"), tr("Social"));
    connect(soc_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavSocialClicked);
    vl->addWidget(soc_btn);

    nav_buttons_ << lib_btn << set_btn << mp_btn << soc_btn;

    vl->addStretch();

    // ── Footer navigation ────────────────────────────────────────────────────
    auto* sep = new QFrame(sidebar_);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: rgba(255,255,255,30);"));
    vl->addWidget(sep);

    vl->addSpacing(6);

    auto* more_btn = CreateNavButton(QStringLiteral("\u22ef"), tr("More Options"));
    connect(more_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavMoreOptionsClicked);
    vl->addWidget(more_btn);

    auto* web_btn = CreateNavButton(QStringLiteral("\u25cb"), tr("Official Website"));
    connect(web_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavWebsiteClicked);
    vl->addWidget(web_btn);

    auto* man_btn = CreateNavButton(QStringLiteral("\u2630"), tr("Manual"));
    connect(man_btn, &QPushButton::clicked, this, &GamerEnvironment::OnNavManualClicked);
    vl->addWidget(man_btn);

    nav_buttons_ << more_btn << web_btn << man_btn;

    root_layout->addWidget(sidebar_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Right content area (stacked widget)
// ─────────────────────────────────────────────────────────────────────────────
void GamerEnvironment::SetupMainContent(QHBoxLayout* root_layout) {
    content_stack_ = new QStackedWidget(this);
    content_stack_->setObjectName(QStringLiteral("gamerContent"));
    content_stack_->setAttribute(Qt::WA_TranslucentBackground);
    content_stack_->setAutoFillBackground(false);

    content_stack_->addWidget(BuildLibraryPage());  // index 0
    content_stack_->addWidget(BuildSocialPage());   // index 1
    content_stack_->setCurrentIndex(0);

    root_layout->addWidget(content_stack_, 1);
}

QWidget* GamerEnvironment::BuildLibraryPage() {
    library_page_ = new QWidget(this);
    library_page_->setAttribute(Qt::WA_TranslucentBackground);
    library_page_->setAutoFillBackground(false);

    auto* vl = new QVBoxLayout(library_page_);
    vl->setContentsMargins(18, 20, 18, 16);
    vl->setSpacing(12);

    auto* heroCard = new QWidget(library_page_);
    heroCard->setStyleSheet(QStringLiteral(
        "background: rgba(255,255,255,0.06);"
        "border: 1px solid rgba(255,255,255,0.12);"
        "border-radius: 22px;"
    ));
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(20, 20, 20, 20);
    heroLayout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(12);

    auto* libraryTitle = new QLabel(tr("Library"), heroCard);
    {
        QFont f = libraryTitle->font();
        f.setPixelSize(28);
        f.setBold(true);
        libraryTitle->setFont(f);
    }
    libraryTitle->setStyleSheet(QStringLiteral("color: white;"));
    titleRow->addWidget(libraryTitle);
    titleRow->addStretch();

    heroLayout->addLayout(titleRow);

    auto* seriesLabel = new QLabel(tr("Your collection, curated."), heroCard);
    seriesLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.72); font-size: 13px;"));
    heroLayout->addWidget(seriesLabel);

    stats_label_ = new QLabel(tr("0 games in your library"), heroCard);
    stats_label_->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.60); font-size: 12px;"));
    heroLayout->addWidget(stats_label_);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(12);

    search_bar_ = new QLineEdit(heroCard);
    search_bar_->setPlaceholderText(tr("Search your games..."));
    search_bar_->setFixedHeight(42);
    search_bar_->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: rgba(255,255,255,0.10);"
        "  border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 21px;"
        "  color: white;"
        "  padding: 0 18px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: rgba(200,120,240,0.9);"
        "  background: rgba(255,255,255,0.18);"
        "}"
    ));
    connect(search_bar_, &QLineEdit::textChanged,
            this, &GamerEnvironment::OnSearchChanged);
    actionRow->addWidget(search_bar_, 1);

    const QString btnStyle = QStringLiteral(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(160,60,200,0.24), stop:1 rgba(255,120,220,0.28));"
        "  border: 1px solid rgba(255,255,255,0.22);"
        "  border-radius: 20px;"
        "  color: white;"
        "  padding: 10px 22px;"
        "  font-size: 13px;"
        "  min-width: 140px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(200,100,240,0.32), stop:1 rgba(255,160,255,0.36));"
        "  border-color: rgba(255,255,255,0.35);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(220,100,240,0.36);"
        "}"
    );

    add_game_btn_  = new QPushButton(tr("Add a Game"),  heroCard);
    load_game_btn_ = new QPushButton(tr("Load a Game"), heroCard);
    add_game_btn_->setIcon(QIcon(QStringLiteral(":/icons/folder.svg")));
    load_game_btn_->setIcon(QIcon(QStringLiteral(":/icons/play.svg")));
    add_game_btn_->setIconSize(QSize(18, 18));
    load_game_btn_->setIconSize(QSize(18, 18));
    add_game_btn_->setCursor(Qt::PointingHandCursor);
    load_game_btn_->setCursor(Qt::PointingHandCursor);
    add_game_btn_->setFixedHeight(42);
    load_game_btn_->setFixedHeight(42);
    add_game_btn_->setStyleSheet(btnStyle);
    load_game_btn_->setStyleSheet(btnStyle);
    connect(add_game_btn_,  &QPushButton::clicked, this, &GamerEnvironment::OnAddGameClicked);
    connect(load_game_btn_, &QPushButton::clicked, this, &GamerEnvironment::OnLoadGameClicked);
    actionRow->addWidget(add_game_btn_);
    actionRow->addWidget(load_game_btn_);

    heroLayout->addLayout(actionRow);
    vl->addWidget(heroCard);

    // ── Game grid ─────────────────────────────────────────────────────────────
    game_grid_ = new QListWidget(library_page_);
    game_grid_->setObjectName(QStringLiteral("gameGrid"));
    game_grid_->setViewMode(QListView::IconMode);
    game_grid_->setResizeMode(QListView::Adjust);
    game_grid_->setWrapping(true);
    game_grid_->setFlow(QListView::LeftToRight);
    game_grid_->setSpacing(14);
    game_grid_->setIconSize(QSize(GameCardDelegate::CARD_W,
                                   GameCardDelegate::ICON_H));
    game_grid_->setGridSize(QSize(GameCardDelegate::CARD_W + GameCardDelegate::PAD * 2 + 14,
                                   GameCardDelegate::CARD_H + GameCardDelegate::PAD * 2 + 14));
    game_grid_->setMovement(QListView::Static);
    game_grid_->setUniformItemSizes(true);
    game_grid_->setContextMenuPolicy(Qt::CustomContextMenu);
    game_grid_->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QListWidget::item { background: transparent; }"
        "QListWidget::item:selected { background: transparent; }"
        "QScrollBar:vertical {"
        "  background: rgba(255,255,255,0.06);"
        "  width: 6px; border-radius: 3px; margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(200,100,200,0.5);"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
    game_grid_->setItemDelegate(new GameCardDelegate(game_grid_));
    game_grid_->setAttribute(Qt::WA_TranslucentBackground);
    game_grid_->setAutoFillBackground(false);
    game_grid_->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    game_grid_->viewport()->setAutoFillBackground(false);
    game_grid_->viewport()->installEventFilter(this);

    connect(game_grid_, &QListWidget::itemDoubleClicked,
            this, &GamerEnvironment::OnGameDoubleClicked);
    connect(game_grid_, &QListWidget::customContextMenuRequested,
            this, &GamerEnvironment::OnGameContextMenu);

    // ── Empty-state label ─────────────────────────────────────────────────────
    empty_label_ = new QLabel(
        tr("No games found.\nClick \u201cAdd a Game\u201d to add a directory."),
        library_page_);
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.45);"
        "font-size: 13px;"
    ));
    empty_label_->setVisible(true);

    vl->addWidget(game_grid_,    1);
    vl->addWidget(empty_label_,  1, Qt::AlignCenter);

    PopulateFromModel();
    return library_page_;
}

QString GamerEnvironment::LoadingScreenTileDataUri() const {
    // Reused so the Social page background matches the loading screen's
    // tiled logo pattern instead of a plain gradient. Built as a runtime
    // QString (not a compile-time raw string literal) specifically because
    // the base64 payload (~127KB) would blow past MSVC's ~16KB limit for a
    // single string literal inside a macro like QStringLiteral(R"(...)")
    // (the exact bug that caused the earlier "string too big" C2026 build
    // error when the JS blob itself got too large).
    static QString cached;
    if (!cached.isEmpty()) {
        return cached;
    }
    QFile mask_file(QStringLiteral(":/img/suyu_tile_mask.png"));
    if (mask_file.open(QIODevice::ReadOnly)) {
        cached = QStringLiteral("data:image/png;base64,") +
                 QString::fromLatin1(mask_file.readAll().toBase64());
    }
    return cached;
}

QWidget* GamerEnvironment::BuildSocialPage() {
    social_page_ = new QWidget(this);
    social_page_->setAttribute(Qt::WA_TranslucentBackground);
    social_page_->setAutoFillBackground(false);

    auto* vl = new QVBoxLayout(social_page_);
    // No outer margins/native header row: Back, Refresh, New Post, Music,
    // and Log In are all rendered as one unified row of pills INSIDE the
    // page itself (see the injected JS below), so nothing sits as a
    // separate native bar above the widget.
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    social_music_enabled_ = UISettings::values.enable_social_music.GetValue();

    auto* redditTab = new QWidget(social_page_);
    auto* redditLayout = new QVBoxLayout(redditTab);
    redditLayout->setContentsMargins(0, 0, 0, 0);
    redditLayout->setSpacing(10);

#ifdef SUYU_USE_QT_WEB_ENGINE
    // reddit.com's current frontend (shreddit-app) renders posts as web
    // components with closed/encapsulated Shadow DOM - injected page CSS
    // cannot reach the actual visual elements inside a shadow root (only
    // ::part()/custom-properties cross that boundary, and Reddit exposes
    // neither for post layout), which is why an earlier attempt to reskin
    // it as CSS-only never visually changed anything. old.reddit.com is
    // still fully supported, still lets you log in and post, and is
    // classic server-rendered HTML with plain class names (.thing, .title,
    // .tagline, .midcol) and NO shadow DOM, so a real reskin is possible.
    // We inject CSS to restyle it as Miiverse-style rounded white "speech
    // bubble" post cards (with a little bubble tail) on a warm purple
    // community backdrop, plus JS to synthesize a circular Mii-style
    // avatar badge per post (old reddit has no avatar images by default).
    auto* profile = new QWebEngineProfile(QStringLiteral("suyuSocialFeed"), redditTab);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    social_web_view_ = new QWebEngineView(redditTab);
    auto* social_page = new SuyuWebPopupPage(profile, social_web_view_);
    social_web_view_->setPage(social_page);
    social_web_view_->setStyleSheet(QStringLiteral(
        "QWebEngineView { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.16); border-radius: 16px; }"));

    // Back/Refresh/New Post/Music pills are plain https://suyu-action/<name>
    // links intercepted by SuyuWebPopupPage::acceptNavigationRequest (see
    // its comment for why - NOT a location.hash bridge, which broke Back by
    // polluting the page's own navigation history).
    social_page->action_callback = [this](const QString& action) {
        if (!social_web_view_) {
            return;
        }
        if (action == QStringLiteral("/refresh")) {
            social_web_view_->reload();
        } else if (action == QStringLiteral("/newpost")) {
            social_web_view_->setUrl(QUrl(QStringLiteral("https://old.reddit.com/r/suyu/submit")));
        } else if (action == QStringLiteral("/back")) {
            // Browser-style back navigation within the page (post -> r/suyu,
            // login -> r/suyu) rather than always leaving the Social tab -
            // only fall back to that when there's nowhere left to go back to.
            LOG_INFO(Frontend, "Social back diag: canGoBack={} count={} currentIndex={} currentUrl={}",
                     social_web_view_->history()->canGoBack(),
                     social_web_view_->history()->count(),
                     social_web_view_->history()->currentItemIndex(),
                     social_web_view_->url().toString().toStdString());
            if (social_web_view_->history()->canGoBack()) {
                social_web_view_->back();
                // Chromium's back/forward-cache restore for a page whose DOM
                // we've heavily mutated via JS (document.body.innerHTML)
                // doesn't reliably re-fire loadFinished, so the Miiverse
                // reskin JS never reapplies and the page comes back as raw
                // unstyled Reddit (confirmed live). A follow-up reload()
                // forces a genuine fresh network load of the now-current
                // (already-back-navigated) URL, which does reliably fire
                // loadFinished and reapply the reskin.
                QTimer::singleShot(50, this, [this] {
                    if (social_web_view_) {
                        social_web_view_->reload();
                    }
                });
            } else {
                OnNavLibraryClicked();
            }
        } else if (action == QStringLiteral("/music")) {
            social_music_enabled_ = !social_music_enabled_;
            UISettings::values.enable_social_music.SetValue(social_music_enabled_);
            if (social_music_enabled_) {
                StartSocialMusic();
            } else {
                StopSocialMusic();
            }
            social_web_view_->page()->runJavaScript(QStringLiteral(
                "(function(){var p=document.getElementById('suyu-music-pill');"
                "if(p){p.textContent='%1';}window.__suyuMusicEnabled=%2;})();")
                .arg(social_music_enabled_ ? QStringLiteral("🎵") : QStringLiteral("🔇"))
                .arg(social_music_enabled_ ? QStringLiteral("true") : QStringLiteral("false")));
        } else if (action == QStringLiteral("/open_external_reddit_login")) {
            // Opens the user's own default system browser to log into Reddit
            // there instead of the embedded view - for users who'd rather
            // use already-saved passwords/passkeys/extensions. This is a
            // fully separate browser session/cookie jar from the embedded
            // QWebEngineProfile, so it doesn't feed a token back into suyu;
            // it's an alternative sign-in surface, not a linked session.
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://old.reddit.com/login")));
        }
    };

    // Reddit's actual login widget (even reached via old.reddit.com/login -
    // Reddit now serves its modern Shadow-DOM login UI there regardless of
    // entry point) completes sign-in via client-side SPA routing/pushState,
    // not a full page navigation - so loadFinished never fires again after
    // a successful login, and the Miiverse reskin (which only runs on
    // loadFinished) never rebuilds to show the now-logged-in state. This
    // was reported live as "doesn't work after signin" for both accounts;
    // Nintendo's was a separate, deeper OAuth bug (see ExchangeSessionTokenCode),
    // but Reddit's is exactly this SPA-navigation gap. urlChanged DOES still
    // fire on pushState navigation even without a real network load, so use
    // it to detect "we just left a login-ish URL" and force a real reload()
    // - a fresh server-rendered old.reddit.com page load that legitimately
    // reflects the new session cookie and fires loadFinished normally.
    connect(social_web_view_, &QWebEngineView::urlChanged, this,
            [this](const QUrl& url) {
                const bool is_login = url.path().contains(QStringLiteral("login"));
                if (social_was_on_login_page_ && !is_login && social_web_view_) {
                    social_web_view_->reload();
                }
                social_was_on_login_page_ = is_login;
            });

    connect(social_web_view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok || !social_web_view_) {
            return;
        }
        // Six different attempts to reskin Reddit's OWN rendered page (CSS
        // overrides, position:fixed detection, MutationObserver, hiding
        // iframes, walking Shadow DOM) each hit some new Reddit-owned DOM
        // quirk (ads, shadow-DOM web components even on old.reddit, nag
        // widgets that survive every removal strategy). Fighting Reddit's
        // live markup is a losing, unbounded game. Instead: fetch the raw
        // post data from Reddit's own JSON API (old.reddit.com still serves
        // it, same-origin, uses the existing session cookies) and render an
        // entirely custom Miiverse-style DOM ourselves - pinned posts as
        // square community tiles up top, regular posts as speech-bubble
        // cards with real fetched profile pictures, Miiverse-style icons for
        // comments/likes. We own 100% of the resulting DOM, so none of
        // Reddit's chrome/ads/nags/shadow-DOM can appear in it at all.
        // Login/submit/comment permalinks are left as real Reddit pages
        // (those are inherently short excursions, not the main feed view).
        static const QString js = QStringLiteral(R"JS(
            (function(){
                var path = location.pathname;
                var isListing = (path === '/r/suyu/' || path === '/r/suyu' || path === '/');
                var isLogin = path.indexOf('login') !== -1;

                // Back/Music must work from EVERY page - a post's comments,
                // the login page, the submit form - not just the main feed,
                // so this floating pill bar is injected unconditionally,
                // regardless of which branch below runs. Back asks C++ to
                // use the view's own navigation history (so "back" from a
                // post returns to r/suyu, matching real browser-back
                // semantics) and only falls back to leaving the Social tab
                // entirely when there's nowhere left to go back to.
                function pill(text, actionName) {
                    var a = document.createElement('a');
                    a.textContent = text;
                    // A fake https://suyu-action/<name> link, intercepted by
                    // SuyuWebPopupPage::acceptNavigationRequest in C++ (which
                    // blocks the navigation and runs the real action instead)
                    // - NOT a location.hash change, because a hash change is
                    // itself a real history entry, and that broke Back:
                    // calling history()->back() right after just undid our
                    // own hash push instead of reaching the actual previous
                    // page (confirmed live).
                    a.href = 'https://suyu-action/' + actionName;
                    a.style.cssText = 'pointer-events:auto;color:#fff;'
                        + 'background:rgba(40,20,70,0.55);backdrop-filter:blur(6px);'
                        + 'padding:6px 16px;border-radius:14px;text-decoration:none;'
                        + 'font-weight:700;font-size:12px;box-shadow:0 2px 0 rgba(0,0,0,0.2);'
                        + 'display:inline-block;';
                    return a;
                }
                // Back always floats top-LEFT on every page. Music floats
                // top-RIGHT too, but only on pages that have no in-page
                // header of their own (post/login) - on the main feed the
                // music pill instead sits inline in that page's own header
                // next to Refresh/New Post/Log In, so the two never fight
                // over the same corner.
                function injectBackPill() {
                    if (document.getElementById('suyu-back-pill')) { return; }
                    var back = pill('⟵ Back', 'back');
                    back.id = 'suyu-back-pill';
                    back.style.cssText += 'position:fixed;top:10px;left:10px;z-index:2147483647;';
                    document.body.appendChild(back);
                }
                function injectFloatingMusicPill() {
                    if (document.getElementById('suyu-music-pill')) { return; }
                    var musicPill = pill(window.__suyuMusicEnabled === false ? '🔇' : '🎵', 'music');
                    musicPill.id = 'suyu-music-pill';
                    musicPill.style.cssText += 'position:fixed;top:10px;right:10px;z-index:2147483647;';
                    document.body.appendChild(musicPill);
                }

                if (isLogin) {
                    // Can't replace the login page's DOM (the actual <form>
                    // must stay functional), so restyle it in place instead:
                    // purple gradient backdrop + a rounded white card
                    // wrapping the form, Miiverse-ish button colors. Also
                    // covers "Continue with Google"/other SSO buttons,
                    // which submit via a normal link/form post on this same
                    // page (the popup case is handled by
                    // SuyuWebPopupPage::createWindow in C++ for providers
                    // that open a separate OAuth window).
                    if (!window.__suyuLoginSkinned) {
                        window.__suyuLoginSkinned = true;
                        var lcss = document.createElement('style');
                        lcss.textContent = ''
                            + 'html,body{background:linear-gradient(180deg,#8f6fd1 0%,#5a3d99 100%) !important;}'
                            + '.content{max-width:400px !important;margin:60px auto !important;'
                            + '  background:#fff !important;border-radius:24px !important;'
                            + '  box-shadow:0 4px 0 rgba(0,0,0,0.2) !important;padding:24px !important;}'
                            + '#login-form-side, .side, #header{display:none !important;}'
                            + '.submit input[type=submit], button[type=submit], .btn, [role="button"]{'
                            + '  background:#3ecf8e !important;color:#fff !important;border:none !important;'
                            + '  border-radius:14px !important;font-weight:700 !important;}';
                        document.head.appendChild(lcss);
                    }
                    injectBackPill();
                    injectFloatingMusicPill();
                    if (!document.getElementById('suyu-external-browser-pill')) {
                        // Alternative for users who'd rather sign in through
                        // their own default system browser (already-saved
                        // Reddit password/passkey, extensions, etc.) instead
                        // of this embedded view - dispatched to C++ via the
                        // same suyu-action:// interception the other pills
                        // use, which calls QDesktopServices::openUrl.
                        var extPill = pill('Sign In via Your Browser', 'open_external_reddit_login');
                        extPill.id = 'suyu-external-browser-pill';
                        // Fixed positioning, same as the other pills - the
                        // login page's DOM structure varies (classic
                        // old.reddit markup vs the modern Shadow-DOM login
                        // widget), so appending into a page-specific
                        // container like '.content' isn't reliable; a
                        // fixed-position node anchored to the viewport
                        // always ends up visible regardless of host layout.
                        extPill.style.cssText += 'position:fixed;bottom:16px;left:50%;'
                            + 'transform:translateX(-50%);z-index:2147483647;';
                        document.body.appendChild(extPill);
                    }
                    return;
                }

                if (!isListing) {
                )JS") + QStringLiteral(R"JS(
                    // A post/comments/submit page. old.reddit's comment page
                    // (unlike the modern login page) is still classic
                    // server-rendered HTML with no Shadow DOM, so - just
                    // like the main listing - a real CSS reskin is possible
                    // here too, instead of leaving it as raw Reddit chrome
                    // (which is what made the Miiverse look "disappear"
                    // whenever a post was opened).
                    if (!window.__suyuPostSkinned) {
                        window.__suyuPostSkinned = true;
                        var pcss = document.createElement('style');
                        pcss.textContent = ''
                            + 'html,body{background:linear-gradient(180deg,#8f6fd1 0%,#5a3d99 100%) '
                            + '  url(__SUYU_TILE_URI__) !important;background-size:auto,220px;'
                            + '  background-blend-mode:normal,overlay;background-repeat:no-repeat,repeat;'
                            + '  font-family:"Segoe UI",sans-serif !important;}'
                            + '#header,.side,.tabmenu,.infobar{display:none !important;}'
                            + '.content{margin:50px auto 24px !important;max-width:760px !important;'
                            + '  background:transparent !important;}'
                            + '.link{background:#fff !important;border-radius:22px !important;'
                            + '  box-shadow:0 3px 0 rgba(0,0,0,0.15) !important;'
                            + '  margin:0 8px 20px 52px !important;padding:14px 18px !important;'
                            + '  border:none !important;position:relative !important;}'
                            + '.link:after{content:"";position:absolute;left:-9px;top:22px;'
                            + '  border-width:8px 9px 8px 0;border-style:solid;'
                            + '  border-color:transparent #fff transparent transparent;}'
                            + '.link .midcol{display:none !important;}'
                            + '.link a.title{color:#3a2a5c !important;font-weight:800 !important;'
                            + '  font-size:16px !important;text-decoration:none !important;}'
                            + '.link p.tagline{color:#9a8fc2 !important;font-size:11px !important;}'
                            + '.commentarea{background:transparent !important;margin:0 8px !important;}'
                            + '.comment{background:#fff !important;border-radius:16px !important;'
                            + '  box-shadow:0 2px 0 rgba(0,0,0,0.12) !important;'
                            + '  margin:0 0 10px 44px !important;padding:10px 14px !important;'
                            + '  border:none !important;position:relative !important;}'
                            + '.comment:after{content:"";position:absolute;left:-8px;top:14px;'
                            + '  border-width:7px 8px 7px 0;border-style:solid;'
                            + '  border-color:transparent #fff transparent transparent;}'
                            + '.comment .midcol{display:none !important;}'
                            + '.comment .tagline{color:#9a8fc2 !important;font-size:11px !important;}'
                            + '.comment .tagline .author{color:#7a5fae !important;font-weight:700 !important;}'
                            + '.usertext-body{color:#4a3a6c !important;font-size:12.5px !important;}'
                            + '.link ul.flat-list.buttons li a, .comment ul.flat-list.buttons li a{'
                            + '  background:#3ecf8e !important;color:#fff !important;'
                            + '  border-radius:12px !important;padding:3px 10px !important;'
                            + '  text-decoration:none !important;font-weight:700 !important;}';
                        document.head.appendChild(pcss);

                        // Synthesize the same circular colored-avatar badges
                        // used on the main feed for the post author and each
                        // commenter (old.reddit has no avatar images).
                        var PALETTE2 = ['#ff8a3d','#ff5f9e','#3ecf8e','#5a9bff','#c77dff','#ffcd3d'];
                        function hashColor2(str) {
                            var h = 0;
                            for (var i = 0; i < str.length; i++) { h = (h * 31 + str.charCodeAt(i)) >>> 0; }
                            return PALETTE2[h % PALETTE2.length];
                        }
                        document.querySelectorAll('.link, .comment').forEach(function(el) {
                            var authorEl = el.querySelector('p.tagline a.author, .tagline a.author');
                            var name = authorEl ? authorEl.textContent.trim() : '?';
                            var badge = document.createElement('div');
                            badge.style.cssText = 'position:absolute;left:-40px;top:10px;width:32px;'
                                + 'height:32px;border-radius:50%;display:flex;align-items:center;'
                                + 'justify-content:center;color:#fff;font-weight:800;font-size:13px;'
                                + 'box-shadow:0 2px 0 rgba(0,0,0,0.2);background:' + hashColor2(name) + ';';
                            badge.textContent = name.charAt(0).toUpperCase();
                            el.appendChild(badge);
                        });
                    }
                    // A post/comments/submit page - leave Reddit's real
                    // content alone (comments need to stay functional) but
                    // still surface Back/Music so navigation is consistent.
                    injectBackPill();
                    injectFloatingMusicPill();
                    return;
                }

                if (window.__suyuMiiverseBuilt) { injectBackPill(); return; }
                window.__suyuMiiverseBuilt = true;

                var PALETTE = ['#ff8a3d','#ff5f9e','#3ecf8e','#5a9bff','#c77dff','#ffcd3d'];
                function hashColor(str) {
                    var h = 0;
                    for (var i = 0; i < str.length; i++) { h = (h * 31 + str.charCodeAt(i)) >>> 0; }
                    return PALETTE[h % PALETTE.length];
                }
                function esc(s) {
                    var d = document.createElement('div');
                    d.textContent = s || '';
                    return d.innerHTML;
                }
                function unescapeHtml(s) {
                    var d = document.createElement('div');
                    d.innerHTML = s || '';
                    return d.textContent;
                }

                function renderMiiverse(pinned, regular, username, avatars) {
                    var css = document.createElement('style');
                    css.textContent = ''
                        // Light mode: white/grey "liquid glass" - frosted
                        // translucent cards over a soft diagonal grey-white
                        // backdrop. Dark mode: the loading screen's own
                        // purple gradient, so Social visually matches it.
                        // The tile-mask image is the same suyu logo pattern
                        // tiled on the loading screen - substituted in from
                        // C++ (a __SUYU_TILE_URI__ token) rather than
                        // embedded as a literal here, since the base64
                        // payload is ~127KB and would blow past MSVC's
                        // ~16KB single-string-literal limit (the exact bug
                        // that caused the earlier "string too big" error).
                        + '@media (prefers-color-scheme: light) {'
                        + '  html,body{background:'
                        + '    linear-gradient(135deg,rgba(244,244,247,0.94) 0%,rgba(220,220,228,0.94) 100%),'
                        + '    url(__SUYU_TILE_URI__) !important;'
                        + '    background-size:auto,220px;background-blend-mode:normal,soft-light;'
                        + '    background-repeat:no-repeat,repeat;}'
                        + '  .mv-card{background:rgba(255,255,255,0.72) !important;'
                        + '    backdrop-filter:blur(14px) saturate(160%);color:#2a2140;}'
                        + '  .mv-card:after{border-color:rgba(255,255,255,0.72) transparent transparent transparent !important;}'
                        + '  .mv-title, .mv-meta b{color:#4b3b78 !important;}'
                        + '  .mv-header a, .mv-announce{color:#2a2140 !important;}'
                        + '  .mv-header a{background:rgba(60,40,100,0.08) !important;}'
                        + '  .mv-announce{background:rgba(255,255,255,0.72) !important;}'
                        + '}'
                        + '@media (prefers-color-scheme: dark) {'
                        + '  html,body{background:'
                        + '    linear-gradient(180deg,rgba(123,95,199,0.94) 0%,rgba(58,42,92,0.94) 100%),'
                        + '    url(__SUYU_TILE_URI__) !important;'
                        + '    background-size:auto,220px;background-blend-mode:normal,overlay;'
                        + '    background-repeat:no-repeat,repeat;}'
                        + '}'
                        + 'html,body{background:'
                        + '  linear-gradient(180deg,rgba(143,111,209,0.94) 0%,rgba(90,61,153,0.94) 100%),'
                        + '  url(__SUYU_TILE_URI__);'
                        + '  background-size:auto,220px;background-blend-mode:normal,overlay;'
                        + '  background-repeat:no-repeat,repeat;'
                        + '  font-family:"Segoe UI",sans-serif !important;margin:0 !important;padding:0 !important;}'
                        + '.mv-header{display:flex;align-items:center;justify-content:flex-end;gap:10px;padding:10px 14px;}'
                        + '.mv-header a{color:#fff;background:rgba(255,255,255,0.18);padding:6px 16px;'
                        + '  border-radius:14px;text-decoration:none;font-weight:700;font-size:12px;'
                        + '  cursor:pointer;}'
                        + '.mv-header a:hover{background:rgba(255,255,255,0.28);}'
                        // Announcements carousel (was small pinned tiles) -
                        // one big card at a time, auto-advancing.
                        + '.mv-announce-wrap{position:relative;margin:4px 16px 20px;height:150px;}'
                        + '.mv-announce{position:absolute;inset:0;border-radius:20px;background:#fff;'
                        + '  box-shadow:0 3px 0 rgba(0,0,0,0.15);display:flex;flex-direction:column;'
                        + '  justify-content:center;padding:18px 24px;text-decoration:none;color:#3a2a5c;'
                        + '  opacity:0;transition:opacity 0.6s ease;pointer-events:none;}'
                        + '.mv-announce.active{opacity:1;pointer-events:auto;}'
                        + '.mv-announce .badge{display:inline-block;background:#ff8a3d;color:#fff;'
                        + '  font-size:10px;font-weight:800;padding:3px 10px;border-radius:10px;'
                        + '  margin-bottom:8px;width:fit-content;}'
                        + '.mv-announce .atitle{font-size:18px;font-weight:800;}'
                        + '.mv-announce-dots{position:absolute;bottom:8px;left:0;right:0;'
                        + '  display:flex;justify-content:center;gap:6px;}'
                        + '.mv-announce-dots span{width:7px;height:7px;border-radius:50%;'
                        + '  background:rgba(255,255,255,0.4);}'
                        + '.mv-announce-dots span.active{background:#fff;}'
                        + '.mv-feed{padding:0 16px 24px;max-width:720px;margin:0 auto;}'
                        + '.mv-card{background:#fff;border-radius:22px;box-shadow:0 3px 0 rgba(0,0,0,0.15);'
                        + '  margin:0 0 22px 44px;padding:14px 18px;position:relative;}'
                        // Tail now points LEFT, toward the avatar (which
                        // sits to the card's upper-left), instead of down
                        // at nothing in particular.
                        + '.mv-card:after{content:"";position:absolute;left:-9px;top:26px;'
                        + '  border-width:8px 9px 8px 0;border-style:solid;'
                        + '  border-color:transparent #fff transparent transparent;}'
                        + '.mv-avatar{position:absolute;left:-50px;top:12px;width:40px;height:40px;'
                        + '  border-radius:50%;display:flex;align-items:center;justify-content:center;'
                        + '  color:#fff;font-weight:800;font-size:16px;box-shadow:0 2px 0 rgba(0,0,0,0.2);'
                        + '  background-size:cover;background-position:center;}'
                        + '.mv-title{color:#3a2a5c;font-weight:800;font-size:15px;text-decoration:none;display:block;margin-bottom:4px;}'
                        + '.mv-meta{color:#9a8fc2;font-size:11px;margin-bottom:8px;}'
                        + '.mv-meta b{color:#7a5fae;}'
                        + '.mv-body{color:#4a3a6c;font-size:12.5px;line-height:1.5;margin-bottom:10px;'
                        + '  max-height:4.5em;overflow:hidden;}'
                        + '.mv-embed{margin-bottom:10px;border-radius:14px;overflow:hidden;background:#f2effa;}'
                        + '.mv-embed img, .mv-embed video{width:100%;max-height:280px;object-fit:cover;display:block;}'
                        + '.mv-linkcard{display:flex;align-items:center;gap:10px;margin-bottom:10px;'
                        + '  border-radius:14px;background:#f2effa;padding:8px;text-decoration:none;color:#4a3a6c;}'
                        + '.mv-linkcard img{width:56px;height:56px;border-radius:10px;object-fit:cover;flex-shrink:0;}'
                        + '.mv-linkcard .domain{font-size:10px;color:#9a8fc2;font-weight:700;}'
                        + '.mv-actions{display:flex;gap:8px;}'
                        + '.mv-actions a{background:#3ecf8e;color:#fff;border-radius:14px;padding:5px 14px;'
                        + '  font-weight:700;font-size:11px;text-decoration:none;display:inline-flex;'
                        + '  align-items:center;gap:4px;}'
                        + '.mv-actions a.yeah{background:#ff8a3d;}';
                    document.head.appendChild(css);
                )JS") + QStringLiteral(R"JS(
                    var loginPill = username
                        ? '<a href="/user/' + encodeURIComponent(username) + '">' + esc(username) + '</a>'
                        : '<a href="/login?dest=' + encodeURIComponent(location.href) + '">Log In</a>';
                    var musicIcon = window.__suyuMusicEnabled === false ? '🔇' : '🎵';
                    var header = '<div class="mv-header">'
                        + '<a href="https://suyu-action/refresh">⟳ Refresh</a>'
                        + '<a href="https://suyu-action/newpost">✏️ New Post</a>'
                        + '<a id="suyu-music-pill" href="https://suyu-action/music">' + musicIcon + '</a>'
                        + loginPill + '</div>';

                    var announceHtml = '';
                    if (pinned.length) {
                        announceHtml += '<div class="mv-announce-wrap" id="mv-announce-wrap">';
                        pinned.forEach(function(p, i) {
                            announceHtml += '<a class="mv-announce' + (i === 0 ? ' active' : '') + '" '
                                + 'data-idx="' + i + '" href="' + esc(p.permalink) + '">'
                                + '<span class="badge">📣 Announcement</span>'
                                + '<span class="atitle">' + esc(p.title) + '</span></a>';
                        });
                        if (pinned.length > 1) {
                            announceHtml += '<div class="mv-announce-dots">';
                            pinned.forEach(function(p, i) {
                                announceHtml += '<span' + (i === 0 ? ' class="active"' : '') + '></span>';
                            });
                            announceHtml += '</div>';
                        }
                        announceHtml += '</div>';
                    }

                    function renderEmbed(p) {
                        if (p.is_video && p.media && p.media.reddit_video && p.media.reddit_video.fallback_url) {
                            return '<div class="mv-embed"><video src="' + esc(p.media.reddit_video.fallback_url)
                                + '" controls preload="metadata"></video></div>';
                        }
                        if (p.preview && p.preview.images && p.preview.images[0] && p.preview.images[0].source) {
                            var imgUrl = unescapeHtml(p.preview.images[0].source.url);
                            return '<div class="mv-embed"><img src="' + esc(imgUrl) + '" loading="lazy"></div>';
                        }
                        if (p.thumbnail && p.thumbnail.indexOf('http') === 0 && !p.is_self) {
                            var domain = p.domain || '';
                            return '<a class="mv-linkcard" href="' + esc(p.url || p.permalink) + '" target="_blank">'
                                + '<img src="' + esc(p.thumbnail) + '">'
                                + '<div><div class="domain">' + esc(domain) + '</div>'
                                + '<div>' + esc((p.title || '').slice(0, 60)) + '</div></div></a>';
                        }
                        return '';
                    }

                    var feedHtml = '<div class="mv-feed">';
                    regular.forEach(function(p) {
                        var avatarUrl = avatars[p.author];
                        var avatarStyle = avatarUrl
                            ? 'background-image:url(\'' + avatarUrl + '\');'
                            : 'background:' + hashColor(p.author) + ';';
                        var avatarLetter = avatarUrl ? '' : esc((p.author || '?').charAt(0).toUpperCase());
                        var bodyText = p.selftext ? p.selftext.slice(0, 220) : '';
                        feedHtml += '<div class="mv-card">'
                            + '<div class="mv-avatar" style="' + avatarStyle + '">' + avatarLetter + '</div>'
                            + '<a class="mv-title" href="' + esc(p.permalink) + '">' + esc(p.title) + '</a>'
                            + '<div class="mv-meta">by <b>' + esc(p.author) + '</b> &middot; '
                            + esc(String(p.num_comments)) + ' comments</div>'
                            + renderEmbed(p)
                            + (bodyText ? '<div class="mv-body">' + esc(bodyText) + '</div>' : '')
                            + '<div class="mv-actions">'
                            + '<a class="yeah" href="' + esc(p.permalink) + '">👍 Yeah! ' + esc(String(p.score)) + '</a>'
                            + '<a href="' + esc(p.permalink) + '">💬 ' + esc(String(p.num_comments)) + '</a>'
                            + '</div></div>';
                    });
                    feedHtml += '</div>';

                    document.body.innerHTML = header + announceHtml + feedHtml;
                    injectBackPill();

                    // Auto-advancing slideshow for announcements - pauses on
                    // hover/interaction, resumes after the mouse leaves.
                    if (pinned.length > 1) {
                        var wrap = document.getElementById('mv-announce-wrap');
                        var slides = wrap.querySelectorAll('.mv-announce');
                        var dots = wrap.querySelectorAll('.mv-announce-dots span');
                        var idx = 0;
                        var timer = null;
                        function show(next) {
                            slides[idx].classList.remove('active');
                            dots[idx].classList.remove('active');
                            idx = next;
                            slides[idx].classList.add('active');
                            dots[idx].classList.add('active');
                        }
                        function start() {
                            stop();
                            timer = setInterval(function() { show((idx + 1) % slides.length); }, 4000);
                        }
                        function stop() { if (timer) { clearInterval(timer); timer = null; } }
                        wrap.addEventListener('mouseenter', stop);
                        wrap.addEventListener('mouseleave', start);
                        start();
                    }
                }

                function fetchAvatars(authors) {
                    var unique = Array.from(new Set(authors)).slice(0, 15);
                    return Promise.all(unique.map(function(name) {
                        return fetch('/user/' + encodeURIComponent(name) + '/about.json', { credentials: 'include' })
                            .then(function(r) { return r.ok ? r.json() : null; })
                            .then(function(d) {
                                var url = d && d.data && d.data.icon_img;
                                return [name, url ? url.split('?')[0] : null];
                            })
                            .catch(function() { return [name, null]; });
                    })).then(function(pairs) {
                        var map = {};
                        pairs.forEach(function(p) { if (p[1]) { map[p[0]] = p[1]; } });
                        return map;
                    });
                }

                Promise.all([
                    fetch('/r/suyu/.json?raw_json=1&limit=25', { credentials: 'include' }).then(function(r) { return r.json(); }),
                    fetch('/api/me.json', { credentials: 'include' }).then(function(r) { return r.ok ? r.json() : null; }).catch(function() { return null; })
                ]).then(function(results) {
                    var listing = results[0];
                    var me = results[1];
                    var username = me && me.data && me.data.name ? me.data.name : null;
                    var posts = listing.data.children.map(function(c) { return c.data; });
                    var pinned = posts.filter(function(p) { return p.stickied; });
                    var regular = posts.filter(function(p) { return !p.stickied; });
                    fetchAvatars(posts.map(function(p) { return p.author; })).then(function(avatars) {
                        renderMiiverse(pinned, regular, username, avatars);
                    });
                }).catch(function() {
                    window.__suyuMiiverseBuilt = false;
                });
            })();
        )JS");
        // Substitute the tile-mask data URI at runtime (a plain QString
        // replace, not a compile-time literal) - see LoadingScreenTileDataUri()'s
        // comment for why this can't just be embedded directly in the R"JS(...)"
        // block above.
        QString final_js = js;
        final_js.replace(QStringLiteral("__SUYU_TILE_URI__"), LoadingScreenTileDataUri());
        social_web_view_->page()->runJavaScript(final_js);
    });

    social_web_view_->setUrl(QUrl(QStringLiteral("https://old.reddit.com/r/suyu/")));
    redditLayout->addWidget(social_web_view_, 1);
    vl->addWidget(redditTab, 1);
    return social_page_;
#endif

    social_browser_ = new QTextBrowser(redditTab);
    social_browser_->setOpenExternalLinks(true);
    social_browser_->setStyleSheet(QStringLiteral(
        "QTextBrowser {"
        "  background: rgba(255,255,255,0.08);"
        "  border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 16px;"
        "  color: white;"
        "  font-size: 12px;"
        "  padding: 16px;"
        "}"
        "a { color: #a585ff; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "body { background: transparent; font-family: 'Segoe UI', sans-serif; }"
        ".bubble { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.16); border-radius: 18px; padding: 14px; margin-bottom: 12px; }"
        ".bubble-header { color: rgba(255,255,255,0.72); font-size: 10pt; margin-bottom: 8px; }"
        ".bubble-title { color: white; font-size: 12.5pt; font-weight: 600; margin-bottom: 6px; }"
        ".bubble-body { color: rgba(255,255,255,0.88); font-size: 11pt; line-height: 1.5; }"
        ".bubble-meta { color: rgba(255,255,255,0.55); font-size: 9pt; margin-top: 10px; }"
        "hr { border: none; border-bottom: 1px solid rgba(255,255,255,0.12); margin: 12px 0; }"
    ));
    social_browser_->setHtml(QStringLiteral(
        "<div class='bubble-header'>Loading r/suyu posts...</div>"));
    redditLayout->addWidget(social_browser_);
    connect(social_refresh_btn_, &QPushButton::clicked, this, &GamerEnvironment::LoadRedditFeed);

    vl->addWidget(redditTab, 1);

    LoadRedditFeed();
    return social_page_;
}

void GamerEnvironment::StartSocialMusic() {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!UISettings::values.enable_social_music.GetValue()) {
        return;
    }
    if (!social_music_player_) {
        // A user-supplied custom track (Settings) takes priority over the
        // bundled "Midnight Tokyo" lofi track.
        const QString custom_path =
            QString::fromStdString(UISettings::values.social_music_path.GetValue());
        const QUrl source_url = custom_path.isEmpty()
            ? QUrl(QStringLiteral("qrc:/audio/midnight_tokyo_lofi.mp3"))
            : QUrl::fromLocalFile(custom_path);
        social_music_player_ = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        social_music_output_ = new QAudioOutput(this);
        social_music_output_->setVolume(0.5);
        social_music_player_->setAudioOutput(social_music_output_);
        social_music_player_->setSource(source_url);
#else
        social_music_player_->setVolume(50);
        social_music_player_->setMedia(QUrl(source_url));
#endif
        connect(social_music_player_, &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status) {
                    if (status == QMediaPlayer::EndOfMedia && social_music_player_) {
                        social_music_player_->setPosition(0);
                        social_music_player_->play();
                    }
                });
    }
    social_music_player_->play();
#endif
}

void GamerEnvironment::StopSocialMusic() {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (social_music_player_) {
        social_music_player_->pause();
    }
#endif
}

void GamerEnvironment::LoadRedditFeed() {
    if (!reddit_network_manager_) {
        return;
    }

    if (reddit_reply_) {
        reddit_reply_->deleteLater();
        reddit_reply_ = nullptr;
    }

    social_feed_status_ = QStringLiteral("loading");
    social_feed_error_.clear();
    social_post_count_ = 0;

    if (social_browser_) {
        social_browser_->setHtml(QStringLiteral(
            "<div style='color:#ccc;font-family:Segoe UI, sans-serif;padding:16px;'>"
            "Loading r/suyu posts..."
            "</div>"));
    }

    // Reddit deprecated unauthenticated .json access on 2026-05-28 - every
    // request now needs an OAuth bearer token, even for public read-only
    // listings. See UISettings::values.reddit_client_id's doc comment.
    const QString client_id =
        QString::fromStdString(UISettings::values.reddit_client_id.GetValue());
    if (client_id.isEmpty()) {
        social_feed_status_ = QStringLiteral("unconfigured");
        social_feed_error_.clear();
        if (social_browser_) {
            social_browser_->setHtml(QStringLiteral(
                "<div style='color:#f5f5f5;font-family:Segoe UI, sans-serif;padding:16px;'>"
                "<h3>Reddit feed not configured</h3>"
                "<p>Reddit now requires an OAuth app ID to read even public posts. "
                "Create a free one at "
                "<a href='https://www.reddit.com/prefs/apps' style='color:#a585ff;'>"
                "reddit.com/prefs/apps</a> (type \"installed app\", no secret needed), "
                "then set it in Settings &rarr; UI &rarr; Reddit Client ID.</p>"
                "<p><a href='https://www.reddit.com/r/suyu/new/' style='color:#a585ff;'>"
                "Open r/suyu in browser instead &rarr;</a></p>"
                "</div>"));
        }
        return;
    }

    if (reddit_access_token_.isEmpty()) {
        FetchRedditAccessToken();
        return;
    }

    const QUrl url(QStringLiteral("https://oauth.reddit.com/r/suyu/hot.json?raw_json=1&limit=10"));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent",
                         QByteArrayLiteral("windows:suyu-emulator:v0.04 (by /u/suyu-emu)"));
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(reddit_access_token_).toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reddit_reply_ = reddit_network_manager_->get(request);
}

void GamerEnvironment::FetchRedditAccessToken() {
    if (!reddit_network_manager_) {
        return;
    }

    const QString client_id =
        QString::fromStdString(UISettings::values.reddit_client_id.GetValue());
    if (client_id.isEmpty()) {
        return;
    }

    if (reddit_token_reply_) {
        reddit_token_reply_->deleteLater();
        reddit_token_reply_ = nullptr;
    }

    // Installed-app client-credentials grant: no client secret, just the
    // public client ID, per Reddit's OAuth docs for apps that can't keep a
    // secret confidential (desktop/mobile clients).
    const QUrl url(QStringLiteral("https://www.reddit.com/api/v1/access_token"));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent",
                         QByteArrayLiteral("windows:suyu-emulator:v0.04 (by /u/suyu-emu)"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    const QByteArray basic_auth =
        (client_id + QStringLiteral(":")).toUtf8().toBase64();
    request.setRawHeader("Authorization", QByteArray("Basic ") + basic_auth);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"),
                      QStringLiteral("https://oauth.reddit.com/grants/installed_client"));
    body.addQueryItem(QStringLiteral("device_id"), QStringLiteral("DO_NOT_TRACK_THIS_DEVICE"));

    reddit_token_reply_ =
        reddit_network_manager_->post(request, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reddit_token_reply_, &QNetworkReply::finished, this, [this]() {
        auto* reply = reddit_token_reply_;
        if (!reply) {
            return;
        }
        const bool success = reply->error() == QNetworkReply::NoError;
        const QByteArray bytes = reply->readAll();
        reply->deleteLater();
        reddit_token_reply_ = nullptr;

        if (!success) {
            social_feed_status_ = QStringLiteral("error");
            social_feed_error_ = QStringLiteral("Failed to authenticate with Reddit");
            if (social_browser_) {
                social_browser_->setHtml(QStringLiteral(
                    "<div style='color:#f5f5f5;font-family:Segoe UI, sans-serif;padding:16px;'>"
                    "<h3 style='color:#ff7070;'>Unable to authenticate with Reddit</h3>"
                    "<p>Check that the configured Reddit Client ID is a valid \"installed app\" ID.</p>"
                    "</div>"));
            }
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        const QString token =
            doc.object().value(QStringLiteral("access_token")).toString();
        if (token.isEmpty()) {
            social_feed_status_ = QStringLiteral("error");
            social_feed_error_ = QStringLiteral("Reddit returned no access token");
            return;
        }

        reddit_access_token_ = token;
        LoadRedditFeed();
    });
}

void GamerEnvironment::OnRedditFeedFinished(QNetworkReply* reply) {
    if (!reply) {
        return;
    }

    const bool success = reply->error() == QNetworkReply::NoError;
        const QString error_text = success ? QString{} : reply->errorString();
        const QByteArray bytes = reply->readAll();
    reply->deleteLater();
    if (reply == reddit_reply_) {
        reddit_reply_ = nullptr;
    }

    if (!social_browser_) {
        return;
    }

    if (!success) {
        social_feed_status_ = QStringLiteral("error");
        social_feed_error_ = error_text;
        social_last_updated_ = QDateTime::currentDateTimeUtc();
        social_browser_->setHtml(QStringLiteral(
            "<div style='color:#f5f5f5;font-family:Segoe UI, sans-serif;padding:16px;'>"
                "<h3 style='color:#ff7070;'>Unable to load r/suyu</h3>"
                "<p>%1</p>"
                "<p>The subreddit may be unavailable or your connection is offline.</p>"
                "<p><a href='https://www.reddit.com/r/suyu/new/' style='color:#a585ff;'>"
                "Open r/suyu in browser \u2192</a></p>"
                "</div>").arg(error_text.toHtmlEscaped()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
        social_feed_status_ = QStringLiteral("parse_error");
        social_feed_error_ = QStringLiteral("Invalid JSON payload from Reddit");
        social_last_updated_ = QDateTime::currentDateTimeUtc();
        social_browser_->setHtml(QStringLiteral(
            "<div style='color:#f5f5f5;font-family:Segoe UI, sans-serif;padding:16px;'>"
            "<h3>Unable to parse Reddit response</h3>"
            "</div>"));
        return;
    }

    const QJsonArray children = doc.object().value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("children")).toArray();

    if (children.isEmpty()) {
        social_feed_status_ = QStringLiteral("empty");
        social_feed_error_.clear();
        social_post_count_ = 0;
        social_last_updated_ = QDateTime::currentDateTimeUtc();
        social_browser_->setHtml(QStringLiteral(
            "<div style='color:#f5f5f5;font-family:Segoe UI, sans-serif;padding:16px;'>"
            "<h3>r/suyu</h3>"
            "<p>No recent posts were found.</p>"
            "</div>"));
        return;
    }

    QString html = QStringLiteral(
        "<style>"
        "body { margin: 0; padding: 0; font-family: 'Segoe UI', sans-serif; color: #f3f3f3; background: transparent; }"
        "a { color: #a58aff; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        ".bubble { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.14); border-radius: 18px; padding: 14px; margin-bottom: 14px; }"
        ".bubble-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }"
        ".bubble-title { font-size: 13pt; font-weight: 700; margin: 0 0 8px 0; }"
        ".bubble-meta { color: rgba(255,255,255,0.64); font-size: 10pt; }"
        ".bubble-body { color: rgba(255,255,255,0.88); font-size: 11pt; line-height: 1.5; margin: 0; }"
        "</style>"
        "<div style='padding: 14px;'>"
        "<div style='margin-bottom: 12px;'>"
        "<div style='font-size:15pt;font-weight:700;margin-bottom:4px;'>r/suyu</div>"
        "<div style='color:rgba(255,255,255,0.65);font-size:10pt;'>Latest posts from the subreddit in a chat-inspired community sidebar.</div>"
        "</div>");

    const QDateTime now = QDateTime::currentDateTimeUtc();
    social_feed_status_ = QStringLiteral("loaded");
    social_feed_error_.clear();
    social_post_count_ = children.size();
    social_last_updated_ = now;
    for (const QJsonValue& childValue : children) {
        const QJsonObject post = childValue.toObject().value(QStringLiteral("data")).toObject();
        const QString title = post.value(QStringLiteral("title")).toString();
        const QString author = post.value(QStringLiteral("author")).toString();
        const int score = post.value(QStringLiteral("score")).toInt();
        const int comments = post.value(QStringLiteral("num_comments")).toInt();
        const qint64 createdUtc = post.value(QStringLiteral("created_utc")).toDouble();
        const QString permalink = post.value(QStringLiteral("permalink")).toString();
        const QString url = QStringLiteral("https://www.reddit.com%1").arg(permalink);
        const QString selftext = post.value(QStringLiteral("selftext")).toString();

        const QDateTime created = QDateTime::fromSecsSinceEpoch(createdUtc, Qt::UTC);
        const QString age = created.secsTo(now) < 3600 ? tr("%1 minutes ago").arg(created.secsTo(now) / 60)
            : created.secsTo(now) < 86400 ? tr("%1 hours ago").arg(created.secsTo(now) / 3600)
            : tr("%1 days ago").arg(created.secsTo(now) / 86400);

        // Miiverse-style avatar: a colored circle with the author's initial,
        // colored deterministically from their username.
        const QChar initial = author.isEmpty() ? QChar(u'?') : author.at(0).toUpper();
        static const QStringList avatar_colors{
            QStringLiteral("#ff8a3d"), QStringLiteral("#ff5f9e"), QStringLiteral("#a58aff"),
            QStringLiteral("#4fd1c5"), QStringLiteral("#63c96b"), QStringLiteral("#5b9dff")};
        const int color_index = static_cast<uint>(qHash(author)) % avatar_colors.size();
        const QString avatar_color = avatar_colors[color_index];

        html += QStringLiteral(
            "<div class='bubble'>"
            "<table style='width:100%;border-collapse:collapse;'><tr>"
            "<td style='width:40px;vertical-align:top;'>"
            "<div style='width:32px;height:32px;border-radius:16px;background:%1;"
            "color:white;text-align:center;line-height:32px;font-weight:700;'>%2</div>"
            "</td>"
            "<td style='vertical-align:top;'>"
            "<div class='bubble-header'>"
            "<div class='bubble-meta'>%3 • %4</div>"
            "</div>"
            "<div class='bubble-title'><a href='%5'>%6</a></div>"
            ).arg(avatar_color, initial, author.toHtmlEscaped(), age, url, title.toHtmlEscaped());

        if (!selftext.trimmed().isEmpty()) {
            QString body = selftext;
            if (body.size() > 240) {
                body = body.left(240) + QStringLiteral("...");
            }
            html += QStringLiteral("<div class='bubble-body'>%1</div>").arg(body.toHtmlEscaped());
        }

        // Miiverse-style "stamp" row in place of like/comment buttons.
        html += QStringLiteral(
            "<div class='bubble-meta' style='margin-top:8px;'>"
            "\U0001F44D %1 &nbsp;&nbsp; \U0001F4AC %2"
            "</div>"
            "</td></tr></table>"
            "</div>").arg(QString::number(score), QString::number(comments));
    }

    html += QStringLiteral("<div style='color:rgba(255,255,255,0.65);font-size:10pt;margin-top:10px;'>Click a post to open it in your browser. Tap New Post to share on r/suyu.</div></div>");
    social_browser_->setHtml(html);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a styled nav button
// ─────────────────────────────────────────────────────────────────────────────
QPushButton* GamerEnvironment::CreateNavButton(const QString& icon_text,
                                               const QString& label,
                                               bool active,
                                               const QIcon& svg_icon) {
    auto* btn = new QPushButton(sidebar_);
    if (!svg_icon.isNull()) {
        // Use a proper SVG icon; show only the label as text.
        btn->setIcon(svg_icon);
        btn->setIconSize(QSize(18, 18));
        btn->setText(QStringLiteral("  ") + label);
    } else {
        // Fall back to embedded Unicode glyph as a pseudo-icon.
        btn->setText(QStringLiteral("  ") + icon_text + QStringLiteral("  ") + label);
    }
    btn->setObjectName(QStringLiteral("navBtn"));
    btn->setProperty("active", active);
    btn->setCheckable(false);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(44);

    auto UpdateStyle = [btn]() {
        const bool a = btn->property("active").toBool();
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 12px;"
            "  text-align: left;"
            "  padding: 0 14px;"
            "  font-size: 14px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.14); }"
        ).arg(a ? QStringLiteral("rgba(255,255,255,0.16)")
                : QStringLiteral("transparent"),
              a ? QStringLiteral("white")
                : QStringLiteral("rgba(255,255,255,0.78)")));
    };
    UpdateStyle();

    return btn;
}

void GamerEnvironment::ApplyNavSelection(QPushButton* btn) {
    if (active_nav_btn_ && active_nav_btn_ != btn) {
        active_nav_btn_->setProperty("active", false);
        // Re-apply style for old button
        const QString inactiveStyle = QStringLiteral(
            "QPushButton {"
            "  background: transparent;"
            "  color: rgba(255,255,255,0.70);"
            "  border: none;"
            "  border-radius: 8px;"
            "  text-align: left;"
            "  padding: 0 10px;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.12); }");
        active_nav_btn_->setStyleSheet(inactiveStyle);
    }
    active_nav_btn_ = btn;
    btn->setProperty("active", true);
    const QString activeStyle = QStringLiteral(
        "QPushButton {"
        "  background: rgba(255,255,255,0.20);"
        "  color: white;"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 12px;"
        "  text-align: left;"
        "  padding: 0 14px;"
        "  font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,0.26); }" );
    btn->setStyleSheet(activeStyle);
}

// ─────────────────────────────────────────────────────────────────────────────
// paintEvent — gradient background + sidebar overlay
// ─────────────────────────────────────────────────────────────────────────────
void GamerEnvironment::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ── Dark gradient background ──────────────────────────────────────────────
    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.00, QColor(10,  0, 28));
    bg.setColorAt(0.45, QColor(18,  0, 45));
    bg.setColorAt(1.00, QColor(30,  0, 55));
    p.fillRect(rect(), bg);

    // ── Primary glow orb (large, magenta, centre-left) ────────────────────────
    {
        const qreal ox = width() * 0.30;
        const qreal oy = height() * 0.62;
        const qreal r  = qMax(width(), height()) * 0.60;
        QRadialGradient orb(ox, oy, r);
        orb.setColorAt(0.00, QColor(170,   0, 110,  90));
        orb.setColorAt(0.30, QColor(120,   0,  90,  60));
        orb.setColorAt(0.65, QColor( 60,   0,  80,  30));
        orb.setColorAt(1.00, QColor(  0,   0,   0,   0));
        p.fillRect(rect(), orb);
    }

    // ── Secondary glow orb (smaller, purple, upper-right) ────────────────────
    {
        const qreal ox = width() * 0.72;
        const qreal oy = height() * 0.30;
        const qreal r  = qMax(width(), height()) * 0.35;
        QRadialGradient orb(ox, oy, r);
        orb.setColorAt(0.00, QColor( 90,   0, 150,  55));
        orb.setColorAt(0.50, QColor( 60,   0, 110,  25));
        orb.setColorAt(1.00, QColor(  0,   0,   0,   0));
        p.fillRect(rect(), orb);
    }

    // ── Ambient drifting suyu marks ───────────────────────────────────────────
    // Faint, slow-moving logos so the background has some life rather than
    // being a flat gradient. Positions come from a fixed pseudo-random spread
    // driven by an elapsed-time phase, so it loops smoothly and costs nothing
    // to keep running.
    {
        static const QPixmap mark =
            QIcon(QStringLiteral(":/img/suyu_logo.svg")).pixmap(QSize(160, 160));
        if (!mark.isNull()) {
            const qreal t = ambient_clock_.isValid()
                                ? qreal(ambient_clock_.elapsed()) / 1000.0
                                : 0.0;
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            for (int i = 0; i < 7; ++i) {
                // Each mark gets its own size, speed and drift direction.
                const qreal seed = qreal(i) * 1.7;
                const qreal scale = 0.35 + 0.10 * qreal((i * 37) % 5);
                const qreal speed = 6.0 + qreal((i * 13) % 9);
                const qreal x = width() * (0.5 + 0.55 * std::sin(t / speed + seed));
                const qreal y = height() * (0.5 + 0.55 * std::cos(t / (speed * 1.3) + seed * 2));
                const int size = int(mark.width() * scale);
                p.setOpacity(0.05);
                p.drawPixmap(QRect(int(x) - size / 2, int(y) - size / 2, size, size), mark);
            }
            p.setOpacity(1.0);
        }
    }

    // ── Sidebar translucent overlay ───────────────────────────────────────────
    if (sidebar_) {
        const QRect sr(0, 0, sidebar_->width(), height());
        p.fillRect(sr, QColor(8, 0, 25, 185));
        p.setPen(QColor(255, 255, 255, 22));
        p.drawLine(sr.right(), 0, sr.right(), height());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Game grid population
// ─────────────────────────────────────────────────────────────────────────────
void GamerEnvironment::PopulateFromModel() {
    if (!game_list_) return;

    game_grid_->setUpdatesEnabled(false);
    game_grid_->clear();

    // Titles already placed in the grid, normalised for comparison. Guards
    // against the same game being listed twice when it's reachable from more
    // than one configured scan directory (e.g. a Steam install folder that is
    // also covered by a deep scan of its parent).
    QSet<QString> seen_titles;

    auto* model = game_list_->GetModel();
    if (model) {
        std::function<void(const QModelIndex&)> traverse = [&](const QModelIndex& parent) {
            const int rows = model->rowCount(parent);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex idx = model->index(row, 0, parent);
                const int itemType = idx.data(kGLItemTypeRole).toInt();

                const QString raw_path = idx.data(kGLPathRole).toString();
                const QString path = NormalizeLaunchPath(raw_path);
                const bool looks_like_game_entry =
                    itemType == kGLGameItemType || !raw_path.isEmpty();

                if (looks_like_game_entry) {
                    QString title = idx.data(kGLTitleRole).toString();
                    if (title.isEmpty()) title = idx.data(Qt::DisplayRole).toString();
                    if (title.trimmed().isEmpty()) {
                        continue;
                    }

                    static const QRegularExpression hash_like_title(
                        QStringLiteral("^[0-9a-fA-F]{16,}$"));
                    if (hash_like_title.match(title.trimmed()).hasMatch()) {
                        continue;
                    }

                    if (raw_path.endsWith(QStringLiteral(".nca"), Qt::CaseInsensitive) ||
                        raw_path.contains(QStringLiteral(".cnmt.nca"), Qt::CaseInsensitive)) {
                        continue;
                    }

                    const QString display_path = path.isEmpty() ? raw_path : path;
                    if (display_path.isEmpty()) {
                        continue;
                    }

                    const QIcon icon = DecorationToIcon(idx.data(Qt::DecorationRole));

                    // Apply search filter
                    if (!filter_text_.isEmpty() &&
                        !title.contains(filter_text_, Qt::CaseInsensitive)) {
                        continue;
                    }

                    const QString title_key = title.trimmed().toLower();
                    if (seen_titles.contains(title_key)) {
                        continue;
                    }
                    seen_titles.insert(title_key);

                    auto* item = new QListWidgetItem(title);
                    item->setData(Qt::UserRole, display_path);
                    if (!icon.isNull()) {
                        item->setIcon(icon);
                    }
                    item->setSizeHint(
                        QSize(GameCardDelegate::CARD_W + GameCardDelegate::PAD * 2 + 8,
                              GameCardDelegate::CARD_H + GameCardDelegate::PAD * 2 + 8));
                    game_grid_->addItem(item);
                    RequestCoverArtwork(display_path, title);
                }
                if (model->hasChildren(idx)) traverse(idx);
            }
        };
        traverse(QModelIndex());
    }

    // Titles from the linked Nintendo Account that we have no local dump of.
    // They can't be launched, but showing them is the point of linking the
    // account - the card carries a nintendo:// path so the launch handler can
    // tell them apart and prompt to locate a ROM.
    for (const auto& owned : LoadNintendoOwnedLibrary()) {
        if (owned.title.trimmed().isEmpty()) {
            continue;
        }
        const QString title_key = owned.title.trimmed().toLower();
        if (seen_titles.contains(title_key)) {
            continue;
        }
        if (!filter_text_.isEmpty() &&
            !owned.title.contains(filter_text_, Qt::CaseInsensitive)) {
            continue;
        }
        seen_titles.insert(title_key);

        // If the user already pointed us at a ROM for this title, or one of
        // the scanned directories contains it, treat it as a normal local
        // game rather than an unplayable account entry.
        const QString located = LookUpLocatedRom(owned.title_id);

        auto* item = new QListWidgetItem(owned.title);
        if (located.isEmpty()) {
            item->setData(Qt::UserRole, QStringLiteral("nintendo://%1").arg(owned.title_id));
            // Greyed out to show it can't be launched as-is; double-clicking
            // offers to locate a ROM.
            item->setForeground(QBrush(QColor(150, 150, 160)));
            item->setToolTip(tr("From your Nintendo Account - no local copy found. "
                                "Double-click to select its ROM."));
        } else {
            item->setData(Qt::UserRole, located);
        }
        item->setSizeHint(QSize(GameCardDelegate::CARD_W + GameCardDelegate::PAD * 2 + 8,
                                GameCardDelegate::CARD_H + GameCardDelegate::PAD * 2 + 8));
        game_grid_->addItem(item);
        // Account entries carry Nintendo's own icon URL, which names the exact
        // title. Prefer it over the name-based IGDB search used for local
        // games: that search has to guess from a title string and simply fails
        // for a lot of these, leaving the card blank when correct art was
        // already to hand.
        if (!owned.icon_url.isEmpty()) {
            RequestCoverArtworkFromUrl(item->data(Qt::UserRole).toString(), owned.title,
                                       owned.icon_url);
        } else {
            RequestCoverArtwork(item->data(Qt::UserRole).toString(), owned.title);
        }
    }

    game_grid_->setUpdatesEnabled(true);

    const int game_count = game_grid_->count();
    if (stats_label_) {
        stats_label_->setText(game_count == 1
            ? tr("1 game in your library")
            : tr("%1 games in your library").arg(game_count));
    }

    const bool hasGames = game_count > 0;
    game_grid_->setVisible(hasGames);
    empty_label_->setVisible(!hasGames);
}

QString GamerEnvironment::CoverCachePathForTitle(const QString& title) const {
    const QString cache_root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        QStringLiteral("/cover_cache");
    QDir().mkpath(cache_root);

    const QByteArray key = QCryptographicHash::hash(title.trimmed().toUtf8(),
                                                    QCryptographicHash::Sha1)
                               .toHex();
    return QDir(cache_root).filePath(QString::fromLatin1(key) + QStringLiteral(".png"));
}

QString GamerEnvironment::ExtractIgdbImageUrl(const QString& html) const {
    // Prefer CDN image references from IGDB search results.
    static const QRegularExpression img_re(
        QStringLiteral(R"(((?:https?:)?//images\.igdb\.com/igdb/image/upload/[^"'\s<>]+))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = img_re.match(html);
    if (!m.hasMatch()) {
        return {};
    }

    QString url = m.captured(1);
    if (url.startsWith(QStringLiteral("//"))) {
        url.prepend(QStringLiteral("https:"));
    }
    // Request the ORIGINAL uploaded resolution (IGDB's "t_original" variant
    // is uncapped, unlike the fixed 528x748 ceiling of t_cover_big_2x used
    // previously) so the gamer grid isn't limited to a size smaller than
    // what the cover was actually uploaded at.
    static const QRegularExpression size_re(QStringLiteral(R"(/t_[^/]+/)"));
    url.replace(size_re, QStringLiteral("/t_original/"));
    return url;
}

void GamerEnvironment::ApplyCoverToItem(const QString& game_path, const QIcon& icon) {
    if (!game_grid_ || icon.isNull()) {
        return;
    }

    for (int i = 0; i < game_grid_->count(); ++i) {
        auto* item = game_grid_->item(i);
        if (!item) {
            continue;
        }
        if (item->data(Qt::UserRole).toString() == game_path) {
            item->setIcon(icon);
            return;
        }
    }
}

void GamerEnvironment::RequestCoverArtworkFromUrl(const QString& game_path,
                                                  const QString& title,
                                                  const QString& url) {
    if (!cover_network_manager_ || title.trimmed().isEmpty() || url.isEmpty()) {
        return;
    }

    const QString key = title.trimmed().toLower();
    if (cover_icon_cache_.contains(key)) {
        ApplyCoverToItem(game_path, cover_icon_cache_.value(key));
        return;
    }
    if (cover_requests_in_flight_.contains(key)) {
        return;
    }

    const QString cache_path = CoverCachePathForTitle(title);
    if (QFileInfo::exists(cache_path)) {
        QPixmap px(cache_path);
        if (!px.isNull()) {
            const QIcon cached_icon(px);
            cover_icon_cache_.insert(key, cached_icon);
            ApplyCoverToItem(game_path, cached_icon);
            return;
        }
    }

    cover_requests_in_flight_.insert(key);

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = cover_network_manager_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, key, game_path, cache_path]() {
                reply->deleteLater();
                cover_requests_in_flight_.remove(key);
                if (reply->error() != QNetworkReply::NoError) {
                    return;
                }
                QPixmap px;
                if (!px.loadFromData(reply->readAll()) || px.isNull()) {
                    return;
                }
                px.save(cache_path, "PNG");
                const QIcon icon(px);
                cover_icon_cache_.insert(key, icon);
                ApplyCoverToItem(game_path, icon);
            });
}

void GamerEnvironment::RequestCoverArtwork(const QString& game_path, const QString& title) {
    if (!cover_network_manager_ || title.trimmed().isEmpty()) {
        return;
    }

    const QString key = title.trimmed().toLower();
    if (cover_icon_cache_.contains(key)) {
        ApplyCoverToItem(game_path, cover_icon_cache_.value(key));
        return;
    }
    if (cover_requests_in_flight_.contains(key)) {
        return;
    }

    const QString cache_path = CoverCachePathForTitle(title);
    if (QFileInfo::exists(cache_path)) {
        QPixmap px(cache_path);
        if (!px.isNull()) {
            const QIcon cached_icon(px);
            cover_icon_cache_.insert(key, cached_icon);
            ApplyCoverToItem(game_path, cached_icon);
            return;
        }
    }

    cover_requests_in_flight_.insert(key);

    QUrl search_url(QStringLiteral("https://www.igdb.com/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("q"), title + QStringLiteral(" nintendo switch switch 2"));
    search_url.setQuery(q);

    QNetworkRequest search_req(search_url);
    search_req.setRawHeader("User-Agent",
                            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) suyu/1.0");
    search_req.setRawHeader("Accept",
                            "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

    QNetworkReply* search_reply = cover_network_manager_->get(search_req);
    connect(search_reply, &QNetworkReply::finished, this,
            [this, search_reply, key, game_path, title, cache_path]() {
                const QByteArray html_bytes = search_reply->readAll();
                search_reply->deleteLater();

                if (html_bytes.isEmpty()) {
                    cover_requests_in_flight_.remove(key);
                    return;
                }

                const QString image_url = ExtractIgdbImageUrl(QString::fromUtf8(html_bytes));
                if (image_url.isEmpty()) {
                    cover_requests_in_flight_.remove(key);
                    return;
                }

                QNetworkRequest img_req{QUrl(image_url)};
                img_req.setRawHeader("User-Agent",
                                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) suyu/1.0");
                QNetworkReply* img_reply = cover_network_manager_->get(img_req);
                connect(img_reply, &QNetworkReply::finished, this,
                        [this, img_reply, key, game_path, title, cache_path]() {
                            const QByteArray img_bytes = img_reply->readAll();
                            img_reply->deleteLater();
                            cover_requests_in_flight_.remove(key);

                            QPixmap px;
                            if (!px.loadFromData(img_bytes) || px.isNull()) {
                                return;
                            }

                            const QIcon icon(px);
                            cover_icon_cache_.insert(key, icon);
                            ApplyCoverToItem(game_path, icon);

                            QFile f(cache_path);
                            if (f.open(QIODevice::WriteOnly)) {
                                px.toImage().save(&f, "PNG");
                            }

                            LOG_INFO(Frontend, "Fetched IGDB cover art for '{}'", title.toStdString());
                        });
            });
}

void GamerEnvironment::OnGameDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString stored = item->data(Qt::UserRole).toString();

    // Titles imported from the Nintendo Account have no local dump behind
    // them, so there is nothing to launch. Offer to point at a ROM instead of
    // failing silently, and remember the choice so the entry becomes a normal
    // launchable card from then on.
    if (stored.startsWith(QStringLiteral("nintendo://"))) {
        const QString title = item->text();
        const auto answer = QMessageBox::question(
            this, tr("Locate ROM"),
            tr("\"%1\" comes from your Nintendo Account and no local copy has been found.\n\n"
               "Would you like to select the ROM for it now?")
                .arg(title),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes) {
            return;
        }
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select ROM for %1").arg(title), QString(),
            tr("Switch games (*.nsp *.xci *.nca *.nro);;All files (*)"));
        if (file.isEmpty()) {
            return;
        }
        RememberLocatedRom(stored.mid(QStringLiteral("nintendo://").size()), file);
        item->setData(Qt::UserRole, file);
        item->setForeground(QBrush());
        emit GameLaunchRequested(NormalizeLaunchPath(file));
        return;
    }

    const QString path = NormalizeLaunchPath(stored);
    if (!path.isEmpty()) {
        emit GameLaunchRequested(path);
    }
}

void GamerEnvironment::RememberLocatedRom(const QString& title_id, const QString& rom_path) {
    // Stored next to the rest of the Nintendo account data so the mapping
    // survives restarts and library rescans.
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("suyu"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.beginGroup(QStringLiteral("LocatedRoms"));
    settings.setValue(title_id, rom_path);
    settings.endGroup();
    settings.endGroup();
}

QString GamerEnvironment::LookUpLocatedRom(const QString& title_id) {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("suyu"));
    settings.beginGroup(QStringLiteral("NintendoAccount"));
    settings.beginGroup(QStringLiteral("LocatedRoms"));
    const QString path = settings.value(title_id).toString();
    settings.endGroup();
    settings.endGroup();
    return QFile::exists(path) ? path : QString();
}

bool GamerEnvironment::eventFilter(QObject* watched, QEvent* event) {
    if (game_grid_ != nullptr && watched == game_grid_->viewport() && event != nullptr &&
        event->type() == QEvent::MouseButtonRelease) {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() == Qt::LeftButton) {
            if (QListWidgetItem* item = game_grid_->itemAt(mouse_event->pos())) {
                const QRect item_rect = game_grid_->visualItemRect(item);
                const QRect card_rect = item_rect.adjusted(GameCardDelegate::PAD,
                                                           GameCardDelegate::PAD,
                                                           -GameCardDelegate::PAD,
                                                           -GameCardDelegate::PAD);
                const QRect more_rect(card_rect.right() - 64, card_rect.bottom() - 34, 52, 18);
                if (more_rect.contains(mouse_event->pos())) {
                    ShowGameMenu(item, game_grid_->viewport()->mapToGlobal(mouse_event->pos()));
                    return true;
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GamerEnvironment::ShowGameMenu(QListWidgetItem* item, const QPoint& global_pos) {
    if (!item) {
        return;
    }

    const QString stored_path = item->data(Qt::UserRole).toString();
    const QString launch_path = NormalizeLaunchPath(stored_path);
    const QString title = item->text();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: rgba(20,5,50,230);"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 8px;"
        "  color: white;"
        "  padding: 4px;"
        "}"
        "QMenu::item { padding: 6px 18px; border-radius: 4px; }"
        "QMenu::item:selected { background: rgba(200,80,200,0.3); }"
    ));

    QAction* launch_action = menu.addAction(tr("Launch \"%1\"").arg(title));
    launch_action->setEnabled(!launch_path.isEmpty());
    connect(launch_action, &QAction::triggered, this, [this, launch_path]() {
        if (!launch_path.isEmpty()) {
            emit GameLaunchRequested(launch_path);
        }
    });

    // Nintendo-owned titles with no matching local file previously had no
    // way forward here - "Launch" just sits disabled forever, which is what
    // "doesn't work, library isn't dynamic" actually meant: an owned entry
    // can never become playable without a bridge from "I own this" to
    // "here's the actual file". Locating it once launches immediately AND
    // registers the folder as a normal scan directory, so every future
    // library populate finds it as a real entry through the exact same path
    // a manually-added game uses (real icon/artwork included).
    if (stored_path.startsWith(QStringLiteral("owned://"))) {
        QAction* locate_action = menu.addAction(tr("Locate ROM..."));
        connect(locate_action, &QAction::triggered, this, [this, title]() {
            const QString rom_path = QFileDialog::getOpenFileName(
                this, tr("Locate ROM for %1").arg(title), QString(),
                tr("Switch ROM (*.nsp *.xci *.nca);;All Files (*)"));
            if (rom_path.isEmpty()) {
                return;
            }
            const QString dir_path = QFileInfo(rom_path).absolutePath();
            const UISettings::GameDir game_dir{dir_path.toStdString(), true, true};
            if (!UISettings::values.game_dirs.contains(game_dir) && game_list_) {
                UISettings::values.game_dirs.append(game_dir);
                game_list_->PopulateAsync(UISettings::values.game_dirs);
                QTimer::singleShot(500, this, &GamerEnvironment::RefreshGameGrid);
            }
            emit GameLaunchRequested(rom_path);
        });
    }

    QAction* open_location_action = menu.addAction(tr("Open Game Location"));
    open_location_action->setEnabled(!stored_path.isEmpty());
    connect(open_location_action, &QAction::triggered, this, [stored_path]() {
        const QFileInfo file_info(stored_path);
        const QString target = file_info.isDir() ? file_info.absoluteFilePath()
                                                 : file_info.absolutePath();
        if (!target.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(target));
        }
    });

    menu.exec(global_pos);
}

void GamerEnvironment::OnGameContextMenu(const QPoint& pos) {
    QListWidgetItem* item = game_grid_->itemAt(pos);
    if (!item) return;
    ShowGameMenu(item, game_grid_->viewport()->mapToGlobal(pos));
}

void GamerEnvironment::OnSearchChanged(const QString& text) {
    filter_text_ = text;
    PopulateFromModel();
}

void GamerEnvironment::OnAddGameClicked() {
    emit AddDirectoryRequested();
    // Refresh after a short delay to let the directory scan complete
    QTimer::singleShot(500, this, &GamerEnvironment::RefreshGameGrid);
}

void GamerEnvironment::OnLoadGameClicked() {
    emit LoadFileRequested();
}

void GamerEnvironment::OnNavLibraryClicked() {
    StopSocialMusic();
    ApplyNavSelection(nav_buttons_[0]);
    content_stack_->setCurrentIndex(0);
    PopulateFromModel();
}

void GamerEnvironment::OnNavSettingsClicked() {
    StopSocialMusic();
    emit OpenSettingsRequested();
}

void GamerEnvironment::OnNavMultiplayerClicked() {
    StopSocialMusic();
    emit OpenMultiplayerRequested();
}

void GamerEnvironment::OnNavSocialClicked() {
    ApplyNavSelection(nav_buttons_[3]);
    content_stack_->setCurrentIndex(1);
    LoadRedditFeed();
    StartSocialMusic();
}

void GamerEnvironment::OnNavMoreOptionsClicked() {
    // Show a small context menu with extra options
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: rgba(20,5,50,230);"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 8px;"
        "  color: white; padding: 4px;"
        "}"
        "QMenu::item { padding: 6px 18px; border-radius: 4px; }"
        "QMenu::item:selected { background: rgba(200,80,200,0.3); }"
    ));
    menu.addAction(tr("Refresh game list"), this, [this]() { PopulateFromModel(); });

    if (main_window_ != nullptr) {
        menu.addSeparator();
        menu.addAction(tr("Install Decryption Keys"), this, [this]() {
            QMetaObject::invokeMethod(main_window_, "OnInstallDecryptionKeys",
                                      Qt::QueuedConnection);
        });
        menu.addAction(tr("Configure External Decryption"), this, [this]() {
            QMetaObject::invokeMethod(main_window_, "OnConfigureExternalDecryption",
                                      Qt::QueuedConnection);
        });
        menu.addAction(tr("Install Firmware"), this, [this]() {
            QMetaObject::invokeMethod(main_window_, "OnInstallFirmware",
                                      Qt::QueuedConnection);
        });
        menu.addAction(tr("Verify Installed Contents"), this, [this]() {
            QMetaObject::invokeMethod(main_window_, "OnVerifyInstalledContents",
                                      Qt::QueuedConnection);
        });
    }

    menu.exec(QCursor::pos());
}

void GamerEnvironment::OnNavWebsiteClicked() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://suyu-emu.github.io/website")));
}

void GamerEnvironment::OnNavManualClicked() {
    emit OpenUserManualRequested();
}

void GamerEnvironment::OnModelRowsInserted(const QModelIndex& /*parent*/,
                                           int /*first*/, int /*last*/) {
    // Delay slightly so the model finishes updating before we re-read it
    QTimer::singleShot(200, this, &GamerEnvironment::RefreshGameGrid);
}

void GamerEnvironment::OnModelReset() {
    QTimer::singleShot(200, this, &GamerEnvironment::RefreshGameGrid);
}
