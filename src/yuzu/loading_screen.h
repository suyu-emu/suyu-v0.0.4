// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

#if !QT_CONFIG(movie)
#define YUZU_QT_MOVIE_MISSING 1
#endif

namespace Loader {
class AppLoader;
}

namespace Ui {
class LoadingScreen;
}

class QBuffer;
class QByteArray;
class QMovie;

class LoadingScreen : public QWidget {
    Q_OBJECT

public:
    explicit LoadingScreen(QWidget* parent = nullptr);

    ~LoadingScreen();

    /// Call before showing the loading screen to load the widgets with the logo and banner for the
    /// currently loaded application.
    void Prepare(Loader::AppLoader& loader);

    /// After the loading screen is hidden, the owner of this class can call this to clean up any
    /// used resources such as the logo and banner.
    void Clear();

    // In order to use a custom widget with a stylesheet, you need to override the paintEvent
    // See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
    void paintEvent(QPaintEvent* event) override;

    void OnLoadProgress(std::size_t value, std::size_t total);

private:
#ifndef YUZU_QT_MOVIE_MISSING
    std::unique_ptr<QMovie> animation;
    std::unique_ptr<QBuffer> backing_buf;
    std::unique_ptr<QByteArray> backing_mem;
#endif
    std::unique_ptr<Ui::LoadingScreen> ui;
    std::size_t previous_total = 0;
};
