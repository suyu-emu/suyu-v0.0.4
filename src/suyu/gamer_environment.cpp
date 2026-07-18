// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/gamer_environment.h"
#include "common/logging.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
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
#include <QWebEngineView>
#endif
#include "suyu/game_list_p.h"
#include "suyu/main.h"

namespace {

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
        // Scale to fill, centre-crop
        QPixmap scaled = pix.scaled(iconRect.size(), Qt::KeepAspectRatioByExpanding,
                                    Qt::SmoothTransformation);
        QPoint offset((iconRect.width() - scaled.width()) / 2,
                      (ICON_H - scaled.height()) / 2);
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
    LoadRedditFeed();
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

QWidget* GamerEnvironment::BuildSocialPage() {
    social_page_ = new QWidget(this);
    social_page_->setAttribute(Qt::WA_TranslucentBackground);
    social_page_->setAutoFillBackground(false);

    auto* vl = new QVBoxLayout(social_page_);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* headingRow = new QHBoxLayout();
    headingRow->setContentsMargins(0, 0, 0, 0);
    headingRow->setSpacing(12);

    auto* heading = new QLabel(tr("Community"), social_page_);
    {
        QFont f = heading->font();
        f.setPixelSize(22);
        f.setBold(true);
        heading->setFont(f);
    }
    heading->setStyleSheet(QStringLiteral("color: white;"));
    headingRow->addWidget(heading);
    headingRow->addStretch();

    social_refresh_btn_ = new QPushButton(tr("Refresh"), social_page_);
    social_refresh_btn_->setCursor(Qt::PointingHandCursor);
    social_refresh_btn_->setFixedHeight(36);
    social_refresh_btn_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: white;"
        "  background: rgba(255,255,255,0.12);"
        "  border: 1px solid rgba(255,255,255,0.18);"
        "  border-radius: 18px;"
        "  padding: 0 16px;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,0.18); }"
    ));
    headingRow->addWidget(social_refresh_btn_);

    social_post_btn_ = new QPushButton(tr("New Post"), social_page_);
    social_post_btn_->setCursor(Qt::PointingHandCursor);
    social_post_btn_->setFixedHeight(36);
    social_post_btn_->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color: white;"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #ff8a3d, stop:1 #ff5f9e);"
        "  border: none;"
        "  border-radius: 18px;"
        "  padding: 0 18px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #ff9a52, stop:1 #ff73ab); }"
    ));
    connect(social_post_btn_, &QPushButton::clicked, this, [] {
        // Posting requires a Reddit account and write-scope OAuth, which suyu
        // does not hold server-side secrets for. Open Reddit's own submit
        // page instead of building a credentialed API integration.
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.reddit.com/r/suyu/submit")));
    });
    headingRow->addWidget(social_post_btn_);
    vl->addLayout(headingRow);

    auto* redditTab = new QWidget(social_page_);
    auto* redditLayout = new QVBoxLayout(redditTab);
    redditLayout->setContentsMargins(0, 0, 0, 0);
    redditLayout->setSpacing(10);

#ifdef SUYU_USE_QT_WEB_ENGINE
    // Reddit's official public embed widget (embed.reddit.com) is meant for
    // external sites and needs no OAuth - but it's still gated by an
    // anti-bot check that a plain HTTP client can't pass (verified: curl
    // gets a 403 challenge page even here). A real Chromium engine can
    // execute that challenge's JS like any browser would, so load the
    // embed in an actual QWebEngineView instead of fetching JSON by hand.
    // Custom "frontend": we wrap the embed iframe in our own dark-themed
    // HTML shell and inject CSS matching suyu's palette once it loads.
    social_web_view_ = new QWebEngineView(redditTab);
    social_web_view_->setStyleSheet(QStringLiteral(
        "QWebEngineView { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.16); border-radius: 16px; }"));
    const QString embed_shell = QStringLiteral(
        "<html><head><style>"
        "html,body{margin:0;padding:0;background:#150a2e;height:100%;}"
        "iframe{width:100%;height:100%;border:none;}"
        "</style></head><body>"
        "<iframe src='https://embed.reddit.com/r/suyu?theme=dark&ref_source=embed' "
        "sandbox='allow-scripts allow-same-origin allow-popups'></iframe>"
        "</body></html>");
    social_web_view_->setHtml(embed_shell, QUrl(QStringLiteral("https://suyu-emu.local/")));
    redditLayout->addWidget(social_web_view_, 1);
    connect(social_refresh_btn_, &QPushButton::clicked, this, [this] {
        if (social_web_view_) {
            social_web_view_->reload();
        }
    });
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
    // Request a higher resolution cover variant so the gamer grid is not limited by the
    // initial thumbnail URL embedded in the search results.
    static const QRegularExpression size_re(QStringLiteral(R"(/t_[^/]+/)"));
    url.replace(size_re, QStringLiteral("/t_cover_big_2x/"));
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
    const QString path = NormalizeLaunchPath(item->data(Qt::UserRole).toString());
    if (!path.isEmpty()) {
        emit GameLaunchRequested(path);
    }
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
    ApplyNavSelection(nav_buttons_[0]);
    content_stack_->setCurrentIndex(0);
    PopulateFromModel();
}

void GamerEnvironment::OnNavSettingsClicked() {
    emit OpenSettingsRequested();
}

void GamerEnvironment::OnNavMultiplayerClicked() {
    emit OpenMultiplayerRequested();
}

void GamerEnvironment::OnNavSocialClicked() {
    ApplyNavSelection(nav_buttons_[3]);
    content_stack_->setCurrentIndex(1);
    LoadRedditFeed();
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
