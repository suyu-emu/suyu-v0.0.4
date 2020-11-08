// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QObject>

#ifdef YUZU_USE_QT_WEB_ENGINE
#include <QWebEngineView>
#endif

#include "core/frontend/applets/web_browser.h"

class GMainWindow;

class QtWebBrowser final : public QObject, public Core::Frontend::WebBrowserApplet {
    Q_OBJECT

public:
    explicit QtWebBrowser(GMainWindow& main_window);
    ~QtWebBrowser() override;
};
