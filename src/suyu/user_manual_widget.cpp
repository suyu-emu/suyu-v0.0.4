// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "suyu/user_manual_widget.h"

UserManualWidget::UserManualWidget(QWidget* parent) : QWidget(parent, Qt::Window) {
    setWindowTitle(QStringLiteral("User Manual"));
    resize(700, 500);
    SetupUi();
    LoadDefaultContent();
}

UserManualWidget::~UserManualWidget() = default;

void UserManualWidget::SetupUi() {
    auto* layout = new QVBoxLayout(this);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    btn_back_ = new QPushButton(QStringLiteral("<"), this);
    btn_forward_ = new QPushButton(QStringLiteral(">"), this);
    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("Search manual..."));

    toolbar->addWidget(btn_back_);
    toolbar->addWidget(btn_forward_);
    toolbar->addWidget(search_box_);
    layout->addLayout(toolbar);

    // Browser
    browser_ = new QTextBrowser(this);
    browser_->setOpenExternalLinks(true);
    layout->addWidget(browser_);

    // Connections
    connect(btn_back_, &QPushButton::clicked, browser_, &QTextBrowser::backward);
    connect(btn_forward_, &QPushButton::clicked, browser_, &QTextBrowser::forward);
    connect(search_box_, &QLineEdit::textChanged, this, &UserManualWidget::OnSearchTextChanged);

    setLayout(layout);
}

void UserManualWidget::LoadDefaultContent() {
    browser_->setHtml(QStringLiteral(
        "<h1>SuyuEclipse User Manual</h1>"
        "<h2>Overview</h2>"
        "<p>SuyuEclipse is a Windows front end for the Suyu emulator. It combines a game library, "
        "controller configuration, Steam shortcut support, and debugging tools in one interface.</p>"
        "<h2>Quick Start</h2>"
        "<ol>"
        "<li>In Gamer mode, open <b>Library</b> and click <b>Add a Game</b> to add a game directory, or use <b>File → Load File</b> for a single ROM/NSP file.</li>"
        "<li>Install <b>prod.keys</b> from <b>Tools → Install Decryption Keys</b>. You can also configure an external decryption tool if you prefer that workflow.</li>"
        "<li>Use the game library to select and launch your game.</li>"
        "<li>Configure input from <b>Emulation → Configure → Input</b>.</li>"
        "<li>Visit <b>Emulation → Configure → Graphics</b> to tune performance.</li>"
        "</ol>"
        "<h2>Game Library</h2>"
        "<p>The library view shows imported games, allows sorting, and offers search by title or file path. "
        "Drag-and-drop support makes it easy to add games.</p>"
        "<h2>Steam Integration</h2>"
        "<p>SuyuEclipse detects Steam automatically and can add your games as non-Steam shortcuts. "
        "This allows launcher and overlay support without requiring a Steam API key.</p>"
        "<p>To add a game to Steam:</p>"
        "<ul>"
        "<li>Open the Steam Integration panel from the emulator menu.</li>"
        "<li>Select the game entry and choose <b>Add to Steam</b>.</li>"
        "<li>Steam shortcuts are written to your local Steam userdata files.</li>"
        "</ul>"
        "<h2>Controllers and Input</h2>"
        "<p>Configure controllers in the Input settings. Both physical controllers and keyboard layouts can be mapped."
        "If a controller is not recognized, reconnect it and restart the emulator.</p>"
        "<h2>Graphics and Audio</h2>"
        "<p>Use the Graphics settings to select the renderer, toggle VSync, and adjust resolution scaling. "
        "Audio settings are available in the Audio panel for volume, latency, and output device selection.</p>"
        "<h2>Save States</h2>"
        "<p>SuyuEclipse supports save states for quick progress snapshots. Use the <b>Save State</b> and <b>Load State</b> "
        "buttons in the emulator toolbar.</p>"
        "<h2>Networking and Multiplayer</h2>"
        "<p>If multiplayer features are enabled, use the dedicated room server to host or join sessions. "
        "Multiplayer rooms can be announced through the web service when available.</p>"
        "<h2>Troubleshooting</h2>"
        "<p>If the emulator fails to start, check the log file from <b>Help → Open Log Location</b>. Common issues include incorrect firmware, unsupported file formats, or missing decryption keys.</p>"
        "<h2>Advanced Tips</h2>"
        "<ul>"
        "<li>Use the <b>Manual</b> tab inside the game library to see context-sensitive help.</li>"
        "<li>For the best performance, enable the correct renderer for your GPU.</li>"
        "<li>Keep your Steam client up to date to ensure compatibility with Steam shortcut import.</li>"
        "</ul>"
        "<h2>Additional Resources</h2>"
        "<p>Visit the project documentation in <b>docs/README.md</b> for build details and developer notes.</p>"));
}

void UserManualWidget::LoadContent(const QString& resource_path) {
    browser_->setSource(QUrl(resource_path));
}

void UserManualWidget::OnSearchTextChanged(const QString& text) {
    if (text.isEmpty()) {
        browser_->moveCursor(QTextCursor::Start);
        return;
    }
    browser_->find(text);
}
