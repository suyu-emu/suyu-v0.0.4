// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>
#include <QString>
#include <QWidget>
#include <QtGlobal>

#if !QT_CONFIG(movie)
#define SUYU_QT_MOVIE_MISSING 1
#endif

namespace Loader {
class AppLoader;
}

namespace Ui {
class LoadingScreen;
}

namespace VideoCore {
enum class LoadCallbackStage;
}

class QBuffer;
class QByteArray;
class QHideEvent;
class QGraphicsOpacityEffect;
class QMediaPlayer;
class QMovie;
class QPropertyAnimation;
class QVariantAnimation;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioOutput;
#endif

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

    /// Slot used to update the status of the progress bar
    void OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total);
    void UpdateSpinner();

    /// Hides the LoadingScreen with a fade out effect
    void OnLoadComplete();

    // In order to use a custom widget with a stylesheet, you need to override the paintEvent
    // See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void LoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total);
    /// Signals that this widget is completely hidden now and should be replaced with the other
    /// widget
    void Hidden();

private:
    void StartLoadingMusic();
    void FadeLoadingMusicTo(qreal target_volume, int duration_ms, bool stop_after_fade);
    void StopLoadingMusic(bool immediate = true);
    void ResetLoadingMusicSource();
    qreal LoadingMusicVolume() const;
    void SetLoadingMusicVolume(qreal volume);

#ifndef SUYU_QT_MOVIE_MISSING
    std::unique_ptr<QMovie> animation;
    std::unique_ptr<QBuffer> backing_buf;
    std::unique_ptr<QByteArray> backing_mem;
#endif
#ifdef SUYU_USE_QT_MULTIMEDIA
    std::unique_ptr<QMediaPlayer> loading_music_player_;
    std::unique_ptr<QByteArray> loading_music_data_;
    std::unique_ptr<QBuffer> loading_music_buffer_;
    std::unique_ptr<QVariantAnimation> loading_music_fade_animation_;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    std::unique_ptr<QAudioOutput> loading_music_output_;
#endif
    bool loading_music_stop_pending_ = false;
#endif
    std::unique_ptr<Ui::LoadingScreen> ui;
    std::size_t previous_total = 0;
    VideoCore::LoadCallbackStage previous_stage;

    QGraphicsOpacityEffect* opacity_effect = nullptr;
    std::unique_ptr<QPropertyAnimation> fadeout_animation;
    QTimer* spinner_timer_ = nullptr;
    QTimer* background_timer_ = nullptr;
    qreal logo_angle_ = 0.0;
    int background_offset_ = 0;
    QPixmap spinner_pixmap_;
    QPixmap pattern_pixmap_;
    QString game_title_;

    // Definitions for the differences in text and styling for each stage
    std::unordered_map<VideoCore::LoadCallbackStage, const char*> progressbar_style;
    std::unordered_map<VideoCore::LoadCallbackStage, QString> stage_translations;

    // newly generated shaders are added to the end of the file, so when loading and compiling
    // shaders, it will start quickly but end slow if new shaders were added since previous launch.
    // These variables are used to detect the change in speed so we can generate an ETA
    bool slow_shader_compile_start = false;
    std::chrono::steady_clock::time_point slow_shader_start;
    std::chrono::steady_clock::time_point previous_time;
    std::size_t slow_shader_first_value = 0;
};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(VideoCore::LoadCallbackStage);
#endif
