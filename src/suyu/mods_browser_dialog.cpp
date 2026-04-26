// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/mods_browser_dialog.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QWidget>

#ifdef SUYU_USE_QT_WEB_ENGINE
#include <QAction>
#include <QToolBar>
#include <QWebEngineView>
#include <QStyle>
#endif

namespace {
constexpr auto kModsBrowserUrl = "https://github.com/exefer/ns-emu-mod-downloader";
}

ModBrowserDialog::ModBrowserDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Mod Browser"));
    resize(1100, 760);

#ifdef SUYU_USE_QT_WEB_ENGINE
    auto* layout = new QVBoxLayout(this);
    auto* toolbar = new QToolBar(this);

    QAction* back_action = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"));
    QAction* forward_action = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Forward"));
    QAction* reload_action = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload"));
    QAction* open_external_action = toolbar->addAction(tr("Open in Browser"));

    auto* url_label = new QLabel(QString::fromLatin1(kModsBrowserUrl), this);
    url_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    url_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(url_label);

    layout->addWidget(toolbar);

    auto* web_view = new QWebEngineView(this);
    web_view->setUrl(QUrl(QString::fromLatin1(kModsBrowserUrl)));
    layout->addWidget(web_view, 1);

    connect(back_action, &QAction::triggered, web_view, &QWebEngineView::back);
    connect(forward_action, &QAction::triggered, web_view, &QWebEngineView::forward);
    connect(reload_action, &QAction::triggered, web_view, &QWebEngineView::reload);
    connect(open_external_action, &QAction::triggered, this, [web_view] {
        QDesktopServices::openUrl(web_view->url());
    });
    connect(web_view, &QWebEngineView::urlChanged, this, [url_label](const QUrl& url) {
        url_label->setText(url.toString());
    });
#else
    auto* layout = new QVBoxLayout(this);
    auto* message = new QLabel(
        tr("Qt WebEngine is not available in this build. Install Qt WebEngine to browse mods inside the app."),
        this);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto* external_button = new QPushButton(tr("Open Mod Browser in External Browser"), this);
    connect(external_button, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kModsBrowserUrl)));
    });
    layout->addWidget(external_button);
#endif
}
