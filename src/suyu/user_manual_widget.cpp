// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QFrame>
#include <QLabel>
#include <QSplitter>
#include <QTextCursor>
#include <QUrl>

#ifdef SUYU_USE_QT_WEB_ENGINE
#include <QWebEngineView>
#endif

#include "suyu/user_manual_widget.h"

UserManualWidget::UserManualWidget(QWidget* parent) : QWidget(parent, Qt::Window) {
    setWindowTitle(QStringLiteral("User Manual"));
    resize(1100, 820);
    SetupUi();
    LoadDefaultContent();
    LoadDocsUrl(QUrl(QStringLiteral("https://suyu-emu.github.io/website/docs")));
}

UserManualWidget::~UserManualWidget() = default;

void UserManualWidget::SetupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    btn_back_ = new QPushButton(QStringLiteral("<"), this);
    btn_forward_ = new QPushButton(QStringLiteral(">"), this);
    btn_reload_ = new QPushButton(tr("Reload Docs"), this);
    btn_open_external_ = new QPushButton(tr("Open Docs in Browser"), this);
    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("Search built-in guide..."));
    docs_status_label_ = new QLabel(this);
    docs_status_label_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    docs_status_label_->setWordWrap(false);

    toolbar->addWidget(btn_back_);
    toolbar->addWidget(btn_forward_);
    toolbar->addWidget(btn_reload_);
    toolbar->addWidget(btn_open_external_);
    toolbar->addWidget(docs_status_label_, 1);
    toolbar->addWidget(search_box_);
    layout->addLayout(toolbar);

    auto* intro_label = new QLabel(
        tr("Live docs are embedded above when WebEngine is available. The built-in guide below stays available offline and covers keys, game setup, Steam shortcuts, Nintendo account sync, and troubleshooting."),
        this);
    intro_label->setWordWrap(true);
    intro_label->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(intro_label);

    content_splitter_ = new QSplitter(Qt::Vertical, this);

#ifdef SUYU_USE_QT_WEB_ENGINE
    docs_view_ = new QWebEngineView(content_splitter_);
    docs_view_->setUrl(QUrl(QStringLiteral("about:blank")));
    content_splitter_->addWidget(docs_view_);
#else
    auto* docs_fallback = new QFrame(content_splitter_);
    auto* docs_fallback_layout = new QVBoxLayout(docs_fallback);
    auto* docs_fallback_label = new QLabel(
        tr("Qt WebEngine is not available in this build, so the online docs cannot be embedded here. Use the button above to open the public docs site in your browser."),
        docs_fallback);
    docs_fallback_label->setWordWrap(true);
    docs_fallback_label->setAlignment(Qt::AlignCenter);
    docs_fallback_layout->addStretch();
    docs_fallback_layout->addWidget(docs_fallback_label);
    docs_fallback_layout->addStretch();
    content_splitter_->addWidget(docs_fallback);
#endif

    browser_ = new QTextBrowser(content_splitter_);
    browser_->setOpenExternalLinks(true);
    browser_->setOpenLinks(true);
    content_splitter_->addWidget(browser_);
    content_splitter_->setStretchFactor(0, 3);
    content_splitter_->setStretchFactor(1, 2);
    content_splitter_->setChildrenCollapsible(false);
    layout->addWidget(content_splitter_, 1);

    // Connections
#ifdef SUYU_USE_QT_WEB_ENGINE
    connect(btn_back_, &QPushButton::clicked, this, [this] {
        if (docs_view_ != nullptr) {
            docs_view_->back();
        }
    });
    connect(btn_forward_, &QPushButton::clicked, this, [this] {
        if (docs_view_ != nullptr) {
            docs_view_->forward();
        }
    });
    connect(btn_reload_, &QPushButton::clicked, this, [this] {
        if (docs_view_ != nullptr) {
            docs_view_->reload();
        }
    });
    connect(docs_view_, &QWebEngineView::urlChanged, this, [this](const QUrl& url) {
        docs_status_label_->setText(url.toString());
    });
#else
    connect(btn_back_, &QPushButton::clicked, browser_, &QTextBrowser::backward);
    connect(btn_forward_, &QPushButton::clicked, browser_, &QTextBrowser::forward);
    connect(btn_reload_, &QPushButton::clicked, this, [this] {
        LoadDocsUrl(docs_url_);
    });
#endif
    connect(btn_open_external_, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(docs_url_);
    });
    connect(search_box_, &QLineEdit::textChanged, this, &UserManualWidget::OnSearchTextChanged);

    setLayout(layout);
}

void UserManualWidget::LoadDefaultContent() {
    browser_->setHtml(QStringLiteral(
        "<h1>suyu Manual</h1>"
        "<p><b>What you are looking at:</b> the top pane loads the public Suyu docs site inside the app when WebEngine support is available. This lower pane keeps the local reference guide available even if the site is offline or the current build does not ship WebEngine.</p>"
        "<h2>Quick Start</h2>"
        "<ol>"
        "<li>In Gamer mode, open <b>Library</b> and click <b>Add a Game</b> to add a game directory, or use <b>File → Load File</b> for a single ROM/NSP file.</li>"
        "<li>Install <b>prod.keys</b> from <b>Tools → Install Decryption Keys</b>. You can also configure an external decryption tool if you prefer that workflow.</li>"
        "<li>Use the game library to select and launch your game. The gamer grid and list view both support search and context menus.</li>"
        "<li>Configure input from <b>Emulation → Configure → Input</b>.</li>"
        "<li>Visit <b>Emulation → Configure → Graphics</b> to tune performance.</li>"
        "</ol>"
        "<h2>Keys, Firmware, and Decryption</h2>"
        "<p>Built-in key installation is available again from <b>Tools → Install Decryption Keys</b>. Place <b>prod.keys</b> and optional <b>title.keys</b> there to restore local decryption. External decryption remains available, but it is no longer the only path.</p>"
        "<ul>"
        "<li>If a title still reports missing firmware or decryption, verify the keys are in the Suyu keys directory and restart the scan.</li>"
        "<li>If you update keys while the library is open, suyu now refreshes the content providers and repopulates the game list safely.</li>"
        "</ul>"
        "<h2>Game Library</h2>"
        "<p>The library view shows imported games, allows sorting, and offers search by title or file path. Drag-and-drop support makes it easy to add games. Gamer mode also pulls cover art from online sources and caches it for reuse.</p>"
        "<h2>Nintendo Account</h2>"
        "<p>The Nintendo Account dialog supports browser-based sign-in when Qt WebEngine is present. Linked accounts can verify the stored session token and fetch the Nintendo web purchase history used for owned-title indicators.</p>"
        "<ul>"
        "<li>Use <b>One-Click Sign In</b> for the streamlined path.</li>"
        "<li>If embedded sign-in is unavailable in your build, the same flow can still be completed in your external browser and pasted back into the dialog.</li>"
        "</ul>"
        "<h2>Steam Integration</h2>"
        "<p>suyu detects Steam automatically and can add your games as non-Steam shortcuts. This allows launcher and overlay support without requiring a Steam API key.</p>"
        "<p>Shortcut export will search for Steam artwork, cache it locally, and register the game with launch arguments that return directly to the selected title.</p>"
        "<p>To add a game to Steam:</p>"
        "<ul>"
        "<li>Right-click a game entry and choose the Steam shortcut action.</li>"
        "<li>Select the artwork style you want to use.</li>"
        "<li>Steam shortcuts are written directly into your local Steam userdata configuration.</li>"
        "</ul>"
        "<h2>Controllers and Input</h2>"
        "<p>Configure controllers in the Input settings. Both physical controllers and keyboard layouts can be mapped."
        "If a controller is not recognized, reconnect it and restart the emulator.</p>"
        "<h2>Graphics and Audio</h2>"
        "<p>Use the Graphics settings to select the renderer, toggle VSync, and adjust resolution scaling. "
        "Audio settings are available in the Audio panel for volume, latency, and output device selection.</p>"
        "<h2>Save States</h2>"
        "<p>suyu supports save states for quick progress snapshots. Use the <b>Save State</b> and <b>Load State</b> "
        "buttons in the emulator toolbar.</p>"
        "<h2>Networking and Multiplayer</h2>"
        "<p>If multiplayer features are enabled, use the dedicated room server to host or join sessions. "
        "Multiplayer rooms can be announced through the web service when available.</p>"
        "<h2>Troubleshooting</h2>"
        "<p>If the emulator fails to start, check the log file from <b>Help → Open Log Location</b>. Live log flushing is enabled again, so title-specific debugging should write out while the emulator is running instead of only at process shutdown.</p>"
        "<ul>"
        "<li>Missing games in gamer mode often means the source path was not added to the library scan list yet.</li>"
        "<li>Owned Nintendo titles are informational until the corresponding ROM or dumped title is available locally.</li>"
        "<li>Steam shortcut export depends on a detected Steam userdata folder. If Steam is portable or installed in a custom location, confirm the Steam path is readable.</li>"
        "</ul>"
        "<h2>Advanced Tips</h2>"
        "<ul>"
        "<li>Use the embedded docs pane for the public online docs and this lower guide for suyu-specific behavior.</li>"
        "<li>For the best performance, enable the correct renderer for your GPU.</li>"
        "<li>Keep your Steam client up to date to ensure compatibility with Steam shortcut import.</li>"
        "</ul>"
        "<h2>Additional Resources</h2>"
        "<ul>"
        "<li><a href='https://suyu-emu.github.io/website/docs'>Public Suyu docs site</a></li>"
        "<li><a href='qrc:/docs/README.md'>Project docs resource paths</a> if your build packages them</li>"
        "</ul>"));
}

void UserManualWidget::LoadContent(const QString& resource_path) {
    const QUrl url(resource_path);
    if (url.isValid() && !url.scheme().isEmpty() &&
        (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))) {
        LoadDocsUrl(url);
        return;
    }

    browser_->setSource(url);
}

void UserManualWidget::LoadDocsUrl(const QUrl& url) {
    docs_url_ = url;
    docs_status_label_->setText(url.toString());

#ifdef SUYU_USE_QT_WEB_ENGINE
    if (docs_view_ != nullptr) {
        docs_view_->setUrl(url);
    }
#endif
}

void UserManualWidget::OnSearchTextChanged(const QString& text) {
    if (text.isEmpty()) {
        browser_->moveCursor(QTextCursor::Start);
        return;
    }
    browser_->find(text);
}
