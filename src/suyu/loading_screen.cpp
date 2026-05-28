// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyleOption>
#include <QTimer>
#include <QVariantAnimation>
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "suyu/loading_screen.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"

// Mingw seems to not have QMovie at all. If QMovie is missing then use a single frame instead of an
// showing the full animation
#if !SUYU_QT_MOVIE_MISSING
#include <QMovie>
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

namespace {

constexpr char LOADING_MUSIC_RESOURCE[] = ":/audio/suyu_loading.mp3";
constexpr bool LOADING_MUSIC_ENABLED = false;
constexpr int LOADING_MUSIC_FADE_IN_MS = 420;
constexpr int LOADING_MUSIC_FADE_OUT_MS = 320;
constexpr qreal LOADING_MUSIC_VOLUME_SCALE = 0.25;
constexpr qreal LOADING_MUSIC_VOLUME_CAP = 0.35;

qreal GetLoadingMusicTargetVolume() {
    if (Settings::values.audio_muted.GetValue()) {
        return 0.0;
    }

    return std::min(static_cast<qreal>(Settings::values.volume.GetValue()) / 100.0 *
                        LOADING_MUSIC_VOLUME_SCALE,
                    LOADING_MUSIC_VOLUME_CAP);
}

QPixmap CreateArtworkPixmap(const QPixmap& source, const QSize& target_size, qreal radius) {
    if (source.isNull() || !target_size.isValid()) {
        return {};
    }

    const QPixmap scaled =
        source.scaled(target_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap framed(target_size);
    framed.fill(Qt::transparent);

    QPainter painter(&framed);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip_path;
    clip_path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(target_size)), radius, radius);
    painter.setClipPath(clip_path);

    const QPoint offset((target_size.width() - scaled.width()) / 2,
                        (target_size.height() - scaled.height()) / 2);
    painter.drawPixmap(offset, scaled);
    painter.setClipping(false);

    painter.setPen(QPen(QColor(255, 255, 255, 60), 1.5));
    painter.drawRoundedRect(QRectF(0.75, 0.75, target_size.width() - 1.5, target_size.height() - 1.5),
                            radius, radius);
    painter.end();

    return framed;
}

} // namespace

constexpr char PROGRESSBAR_STYLE_PREPARE[] = R"(
QProgressBar {}
QProgressBar::chunk {})";

constexpr char PROGRESSBAR_STYLE_BUILD[] = R"(
QProgressBar {
    background-color: rgba(10, 14, 24, 170);
    border: 2px solid rgba(255, 255, 255, 70);
    border-radius: 14px;
    padding: 3px;
}
QProgressBar::chunk {
    background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,
                                                                        stop:0 rgba(235, 70, 104, 255),
                                                                        stop:0.55 rgba(170, 79, 192, 255),
                                                                        stop:1 rgba(64, 126, 255, 255));
    border-radius: 10px;
})";

constexpr char PROGRESSBAR_STYLE_COMPLETE[] = R"(
QProgressBar {
    background-color: rgba(10, 14, 24, 170);
    border: 2px solid rgba(255, 255, 255, 70);
    border-radius: 14px;
  padding: 4px;
}
QProgressBar::chunk {
    background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0,
                                                                        stop:0 rgba(235, 70, 104, 255),
                                                                        stop:0.55 rgba(170, 79, 192, 255),
                                                                        stop:1 rgba(64, 126, 255, 255));
    border-radius: 10px;
})";

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);

    ui->fade_parent->setAttribute(Qt::WA_TranslucentBackground, true);
    ui->fade_parent->setStyleSheet(QStringLiteral("background: transparent;"));
    ui->contentLayout->setAlignment(ui->progressLayout, Qt::AlignVCenter);
    ui->progressLayout->setAlignment(Qt::AlignVCenter);
    ui->stage->setWordWrap(true);
    ui->stage->setMaximumWidth(980);
    ui->stage->setStyleSheet(QStringLiteral(
        "background-color: transparent; color: rgb(246, 248, 255); font: 700 22pt \"Ubuntu\";"));
    ui->progress_bar->setMaximumWidth(980);
    ui->value->setMaximumWidth(980);
    ui->log->setMaximumWidth(980);

    spinner_pixmap_ = QPixmap(QStringLiteral(":/img/suyu.svg")).scaled(120, 120,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    spinner_timer_ = new QTimer(this);
    spinner_timer_->setInterval(33);
    connect(spinner_timer_, &QTimer::timeout, this, [this] {
        logo_angle_ += 1.2;
        if (logo_angle_ >= 360.0) {
            logo_angle_ -= 360.0;
        }
        UpdateSpinner();
    });
    spinner_timer_->start();

    background_timer_ = new QTimer(this);
    background_timer_->setInterval(30);
    connect(background_timer_, &QTimer::timeout, this, [this] {
        background_offset_ = (background_offset_ + 1) % 260;
        update();
    });
    background_timer_->start();

    // Create a fade out effect to hide this loading screen widget.
    // When fading opacity, it will fade to the parent widgets background color, which is why we
    // create an internal widget named fade_widget that we use the effect on, while keeping the
    // loading screen widget's background color black. This way we can create a fade to black effect
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutBack);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);
    qRegisterMetaType<VideoCore::LoadCallbackStage>();

    stage_translations = {
        {VideoCore::LoadCallbackStage::Prepare, tr("Preparing game assets")},
        {VideoCore::LoadCallbackStage::Build, tr("Building pipelines and shaders")},
        {VideoCore::LoadCallbackStage::Complete, tr("Finalizing emulation session")},
    };
    progressbar_style = {
        {VideoCore::LoadCallbackStage::Prepare, PROGRESSBAR_STYLE_PREPARE},
        {VideoCore::LoadCallbackStage::Build, PROGRESSBAR_STYLE_BUILD},
        {VideoCore::LoadCallbackStage::Complete, PROGRESSBAR_STYLE_COMPLETE},
    };

#ifdef SUYU_USE_QT_MULTIMEDIA
    if constexpr (LOADING_MUSIC_ENABLED) {
        QFile music_resource(QString::fromLatin1(LOADING_MUSIC_RESOURCE));
        if (music_resource.open(QIODevice::ReadOnly)) {
            loading_music_data_ = std::make_unique<QByteArray>(music_resource.readAll());
            if (!loading_music_data_->isEmpty()) {
                loading_music_buffer_ = std::make_unique<QBuffer>(loading_music_data_.get());
                loading_music_player_ = std::make_unique<QMediaPlayer>(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                loading_music_output_ = std::make_unique<QAudioOutput>(this);
                loading_music_output_->setVolume(0.0);
                loading_music_player_->setAudioOutput(loading_music_output_.get());
#else
                loading_music_player_->setVolume(0);
#endif

                loading_music_fade_animation_ = std::make_unique<QVariantAnimation>(this);
                connect(loading_music_fade_animation_.get(), &QVariantAnimation::valueChanged, this,
                        [this](const QVariant& value) { SetLoadingMusicVolume(value.toReal()); });
                connect(loading_music_fade_animation_.get(), &QVariantAnimation::finished, this,
                        [this] {
                            if (loading_music_stop_pending_) {
                                StopLoadingMusic(true);
                            }
                        });
                connect(loading_music_player_.get(), &QMediaPlayer::mediaStatusChanged, this,
                        [this](QMediaPlayer::MediaStatus status) {
                            if (status == QMediaPlayer::EndOfMedia && !loading_music_stop_pending_) {
                                ResetLoadingMusicSource();
                                if (loading_music_player_) {
                                    loading_music_player_->play();
                                }
                            }
                        });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                connect(loading_music_player_.get(), &QMediaPlayer::errorOccurred, this,
                        [](QMediaPlayer::Error error, const QString& error_string) {
                            if (error != QMediaPlayer::NoError) {
                                LOG_WARNING(Frontend, "Loading screen music playback error: {}",
                                            error_string.toStdString());
                            }
                        });
#else
                connect(loading_music_player_.get(),
                        QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
                        [this](QMediaPlayer::Error error) {
                            if (error != QMediaPlayer::NoError && loading_music_player_) {
                                LOG_WARNING(Frontend, "Loading screen music playback error: {}",
                                            loading_music_player_->errorString().toStdString());
                            }
                        });
#endif
            } else {
                LOG_WARNING(Frontend, "Loading screen music resource is empty: {}",
                            LOADING_MUSIC_RESOURCE);
            }
        } else {
            LOG_WARNING(Frontend, "Loading screen music resource could not be opened: {}",
                        LOADING_MUSIC_RESOURCE);
        }
    }
#endif
}

LoadingScreen::~LoadingScreen() {
    StopLoadingMusic(true);
}

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    fadeout_animation->stop();
    opacity_effect->setOpacity(1);

    std::vector<u8> buffer;
    std::string title;
    if (loader.ReadTitle(title) == Loader::ResultStatus::Success) {
        game_title_ = QString::fromStdString(title);
    }

    ui->banner->clear();
    ui->banner->setVisible(false);

    if (loader.ReadIcon(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        const int buffer_size = buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
                                     ? std::numeric_limits<int>::max()
                                     : static_cast<int>(buffer.size());
        map.loadFromData(buffer.data(), buffer_size);
        if (!map.isNull()) {
            ui->banner->setPixmap(CreateArtworkPixmap(map, QSize(230, 230), 28.0));
            ui->banner->setVisible(true);
        }
    } else if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        const int buffer_size = buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
                                     ? std::numeric_limits<int>::max()
                                     : static_cast<int>(buffer.size());
        map.loadFromData(buffer.data(), buffer_size);
        if (!map.isNull()) {
            ui->banner->setPixmap(CreateArtworkPixmap(map, QSize(230, 230), 28.0));
            ui->banner->setVisible(true);
        }
    }
    buffer.clear();

    ui->logo->setVisible(true);
    UpdateSpinner();
    ui->logo->setScaledContents(true);

    slow_shader_compile_start = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
    StartLoadingMusic();
}

void LoadingScreen::OnLoadComplete() {
    FadeLoadingMusicTo(0.0, fadeout_animation->duration(), true);
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::hideEvent(QHideEvent* event) {
    FadeLoadingMusicTo(0.0, LOADING_MUSIC_FADE_OUT_MS, true);
    QWidget::hideEvent(event);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    // reset the timer if the stage changes
    if (stage != previous_stage) {
        ui->progress_bar->setStyleSheet(QString::fromUtf8(progressbar_style[stage]));
        // Hide the progress bar during the prepare stage
        if (stage == VideoCore::LoadCallbackStage::Prepare) {
            ui->progress_bar->hide();
        } else {
            ui->progress_bar->show();
        }
        previous_stage = stage;
        // reset back to fast shader compiling since the stage changed
        slow_shader_compile_start = false;
    }
    // update the max of the progress bar if the number of shaders change
    if (total != previous_total) {
        const int safe_total = total > static_cast<std::size_t>(std::numeric_limits<int>::max())
                                   ? std::numeric_limits<int>::max()
                                   : static_cast<int>(total);
        ui->progress_bar->setMaximum(safe_total);
        previous_total = total;
    }
    // Reset the progress bar ranges if compilation is done
    if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 0);
    }

    QString estimate;
    // If there's a drastic slowdown in the rate, then display an estimate
    if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
        if (!slow_shader_compile_start) {
            slow_shader_start = steady_clock::now();
            slow_shader_compile_start = true;
            slow_shader_first_value = value;
        }
        // only calculate an estimate time after a second has passed since stage change
        const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
        const auto progress_delta = value - slow_shader_first_value;
        if (diff > seconds{1} && progress_delta > 0) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  progress_delta * diff.count());
            estimate =
                tr("Estimated Time %1")
                    .arg(QTime(0, 0, 0, 0)
                             .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                             .toString(QStringLiteral("mm:ss")));
        }
    }

    QString stage_text;
    if (game_title_.isEmpty()) {
        stage_text = stage_translations[stage];
    } else {
        switch (stage) {
        case VideoCore::LoadCallbackStage::Prepare:
            stage_text = tr("Preparing <b>%1</b>").arg(game_title_);
            break;
        case VideoCore::LoadCallbackStage::Build:
            stage_text = tr("Building pipelines for <b>%1</b>").arg(game_title_);
            break;
        case VideoCore::LoadCallbackStage::Complete:
            stage_text = tr("Starting <b>%1</b>").arg(game_title_);
            break;
        }
    }

    QString detail_text;
    if (stage == VideoCore::LoadCallbackStage::Build) {
        detail_text = total > 0 ? tr("Compiling shaders, pipelines, and GPU state: %1 / %2")
                                      .arg(value)
                                      .arg(total)
                                : tr("Preparing GPU pipelines and shader cache");
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        detail_text =
            tr("Finalizing renderer, filesystem, input, audio, applets, and networking");
    } else {
        detail_text = game_title_.isEmpty()
                          ? tr("Reading program metadata, icon, patches, firmware, keys, and cache state")
                          : tr("Reading metadata, icon, patches, firmware, keys, and cache state for %1")
                                .arg(game_title_);
    }

    QString progress_value = estimate;
    if (progress_value.isEmpty()) {
        if (stage == VideoCore::LoadCallbackStage::Build && total > 0) {
            const auto percent = static_cast<int>((value * 100) / std::max<std::size_t>(1, total));
            progress_value = tr("%1% complete").arg(percent);
        } else if (stage == VideoCore::LoadCallbackStage::Complete) {
            progress_value = tr("Handing control to the game");
        } else {
            progress_value = tr("Collecting launch data");
        }
    }

    // update labels and progress bar
    ui->stage->setText(stage_text);
    ui->log->setText(detail_text);
    ui->value->setText(progress_value);
    const int safe_value = value > static_cast<std::size_t>(std::numeric_limits<int>::max())
                               ? std::numeric_limits<int>::max()
                               : static_cast<int>(value);
    ui->progress_bar->setValue(safe_value);
    previous_time = now;
}

void LoadingScreen::UpdateSpinner() {
    if (spinner_pixmap_.isNull() || !ui->logo) {
        return;
    }

    const int size = qMin(ui->logo->width(), ui->logo->height());
    if (size <= 0) {
        return;
    }

    QPixmap rotated(size, size);
    rotated.fill(Qt::transparent);

    QPainter painter(&rotated);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(size / 2.0, size / 2.0);
    painter.rotate(logo_angle_);
    painter.translate(-spinner_pixmap_.width() / 2.0, -spinner_pixmap_.height() / 2.0);
    painter.drawPixmap(0, 0, spinner_pixmap_);
    painter.end();

    ui->logo->setPixmap(rotated);
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    bg.setColorAt(0.00, QColor(7, 10, 20));
    bg.setColorAt(0.35, QColor(7, 11, 24));
    bg.setColorAt(1.00, QColor(5, 8, 18));
    p.fillRect(r, bg);

    if (pattern_pixmap_.isNull()) {
        const int tile_size = 180;
        pattern_pixmap_ = QPixmap(tile_size, tile_size);
        pattern_pixmap_.fill(Qt::transparent);
        QPainter painter(&pattern_pixmap_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor blue_ring(70, 130, 255, 55);
        const QColor red_ring(245, 80, 110, 45);
        for (int y = 32; y < tile_size; y += 64) {
            for (int x = 32; x < tile_size; x += 64) {
                const QRect outer(x - 20, y - 20, 40, 40);
                painter.setPen(QPen((x + y) % 2 == 0 ? blue_ring : red_ring, 1.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(outer);

                painter.setPen(Qt::NoPen);
                painter.setBrush((x + y) % 2 == 0 ? QColor(70, 130, 255, 25)
                                                  : QColor(245, 80, 110, 22));
                painter.drawPie(outer, 35 * 16, 145 * 16);
                painter.setBrush((x + y) % 2 == 0 ? QColor(245, 80, 110, 22)
                                                  : QColor(70, 130, 255, 25));
                painter.drawPie(outer, 215 * 16, 145 * 16);
            }
        }
        painter.end();
    }

    const int tile_w = pattern_pixmap_.width();
    const int tile_h = pattern_pixmap_.height();
    p.setOpacity(0.9);
    for (int y = -tile_h; y < r.height() * 0.58; y += tile_h) {
        for (int x = -background_offset_; x < r.width(); x += tile_w) {
            p.drawPixmap(x, y, pattern_pixmap_);
        }
    }
    p.setOpacity(1.0);

    QLinearGradient pattern_fade(r.topLeft(), QPointF(r.left(), r.height() * 0.72));
    pattern_fade.setColorAt(0.0, QColor(0, 0, 0, 0));
    pattern_fade.setColorAt(0.52, QColor(0, 0, 0, 85));
    pattern_fade.setColorAt(1.0, QColor(0, 0, 0, 220));
    p.fillRect(QRect(r.left(), r.top(), r.width(), static_cast<int>(r.height() * 0.72)),
               pattern_fade);

    QLinearGradient vignette(r.topLeft(), r.bottomLeft());
    vignette.setColorAt(0.0, QColor(0, 0, 0, 25));
    vignette.setColorAt(0.5, QColor(0, 0, 0, 105));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 185));
    p.fillRect(r, vignette);
}

void LoadingScreen::Clear() {
#ifndef SUYU_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
    game_title_.clear();
    ui->banner->clear();
    ui->banner->setVisible(false);

#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!loading_music_stop_pending_) {
        StopLoadingMusic(true);
    }
#endif
}

void LoadingScreen::StartLoadingMusic() {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!loading_music_player_ || !loading_music_buffer_) {
        return;
    }

    const qreal target_volume = GetLoadingMusicTargetVolume();
    if (target_volume <= 0.0) {
        StopLoadingMusic(true);
        return;
    }

    loading_music_stop_pending_ = false;
    if (loading_music_fade_animation_) {
        loading_music_fade_animation_->stop();
    }

    SetLoadingMusicVolume(0.0);
    ResetLoadingMusicSource();
    loading_music_player_->play();
    FadeLoadingMusicTo(target_volume, LOADING_MUSIC_FADE_IN_MS, false);
#endif
}

void LoadingScreen::FadeLoadingMusicTo(qreal target_volume, int duration_ms, bool stop_after_fade) {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!loading_music_player_) {
        return;
    }

    loading_music_stop_pending_ = stop_after_fade && target_volume <= 0.0;
    if (!loading_music_fade_animation_) {
        SetLoadingMusicVolume(target_volume);
        if (loading_music_stop_pending_) {
            StopLoadingMusic(true);
        }
        return;
    }

    loading_music_fade_animation_->stop();
    const qreal current_volume = LoadingMusicVolume();
    if (duration_ms <= 0 || qFuzzyCompare(current_volume + 1.0, target_volume + 1.0)) {
        SetLoadingMusicVolume(target_volume);
        if (loading_music_stop_pending_) {
            StopLoadingMusic(true);
        }
        return;
    }

    loading_music_fade_animation_->setDuration(duration_ms);
    loading_music_fade_animation_->setStartValue(current_volume);
    loading_music_fade_animation_->setEndValue(target_volume);
    loading_music_fade_animation_->start();
#else
    Q_UNUSED(target_volume);
    Q_UNUSED(duration_ms);
    Q_UNUSED(stop_after_fade);
#endif
}

void LoadingScreen::StopLoadingMusic(bool immediate) {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!loading_music_player_) {
        return;
    }

    if (!immediate) {
        FadeLoadingMusicTo(0.0, LOADING_MUSIC_FADE_OUT_MS, true);
        return;
    }

    loading_music_stop_pending_ = false;
    if (loading_music_fade_animation_) {
        loading_music_fade_animation_->stop();
    }
    SetLoadingMusicVolume(0.0);
    loading_music_player_->stop();
    if (loading_music_buffer_) {
        loading_music_buffer_->seek(0);
    }
#else
    Q_UNUSED(immediate);
#endif
}

void LoadingScreen::ResetLoadingMusicSource() {
#ifdef SUYU_USE_QT_MULTIMEDIA
    if (!loading_music_player_ || !loading_music_buffer_) {
        return;
    }

    if (!loading_music_buffer_->isOpen()) {
        loading_music_buffer_->open(QIODevice::ReadOnly);
    }
    loading_music_buffer_->seek(0);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    loading_music_player_->setSourceDevice(loading_music_buffer_.get(),
                                           QUrl(QStringLiteral("qrc:/audio/suyu_loading.mp3")));
#else
    loading_music_player_->setMedia(QMediaContent(), loading_music_buffer_.get());
#endif
#endif
}

qreal LoadingScreen::LoadingMusicVolume() const {
#ifdef SUYU_USE_QT_MULTIMEDIA
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (loading_music_output_) {
        return loading_music_output_->volume();
    }
#else
    if (loading_music_player_) {
        return static_cast<qreal>(loading_music_player_->volume()) / 100.0;
    }
#endif
#endif
    return 0.0;
}

void LoadingScreen::SetLoadingMusicVolume(qreal volume) {
    const qreal clamped_volume = std::max<qreal>(0.0, std::min<qreal>(volume, 1.0));

#ifdef SUYU_USE_QT_MULTIMEDIA
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (loading_music_output_) {
        loading_music_output_->setVolume(clamped_volume);
    }
#else
    if (loading_music_player_) {
        loading_music_player_->setVolume(qRound(clamped_volume * 100.0));
    }
#endif
#else
    Q_UNUSED(clamped_volume);
#endif
}
