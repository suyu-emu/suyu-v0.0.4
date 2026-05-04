// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <unordered_map>
#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyleOption>
#include <QTimer>
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
})";

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

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
        {VideoCore::LoadCallbackStage::Prepare, tr("Loading...")},
        {VideoCore::LoadCallbackStage::Build, tr("Loading Shaders %1 / %2")},
        {VideoCore::LoadCallbackStage::Complete, tr("Launching...")},
    };
    progressbar_style = {
        {VideoCore::LoadCallbackStage::Prepare, PROGRESSBAR_STYLE_PREPARE},
        {VideoCore::LoadCallbackStage::Build, PROGRESSBAR_STYLE_BUILD},
        {VideoCore::LoadCallbackStage::Complete, PROGRESSBAR_STYLE_COMPLETE},
    };
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
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
            ui->banner->setPixmap(map.scaled(QSize(230, 230), Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation));
            ui->banner->setVisible(true);
        }
    } else if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        const int buffer_size = buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
                                     ? std::numeric_limits<int>::max()
                                     : static_cast<int>(buffer.size());
        map.loadFromData(buffer.data(), buffer_size);
        if (!map.isNull()) {
            ui->banner->setPixmap(map.scaled(QSize(230, 230), Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation));
            ui->banner->setVisible(true);
        }
    }
    buffer.clear();

    ui->logo->setVisible(true);
    UpdateSpinner();
    ui->logo->setScaledContents(true);

    slow_shader_compile_start = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
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
        if (diff > seconds{1}) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  (value - slow_shader_first_value) * diff.count());
            estimate =
                tr("Estimated Time %1")
                    .arg(QTime(0, 0, 0, 0)
                             .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                             .toString(QStringLiteral("mm:ss")));
        }
    }

    // update labels and progress bar
    if (stage == VideoCore::LoadCallbackStage::Build) {
        ui->stage->setText(stage_translations[stage].arg(value).arg(total));
        ui->log->setText(tr("Shaders compiled: %1 / %2").arg(value).arg(total));
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        if (!game_title_.isEmpty()) {
            ui->stage->setText(tr("Launching <b>%1</b>").arg(game_title_));
        } else {
            ui->stage->setText(stage_translations[stage]);
        }
        ui->log->setText(QString());
    } else {
        ui->stage->setText(stage_translations[stage]);
        ui->log->setText(QString());
    }
    ui->value->setText(estimate);
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
        painter.fillRect(pattern_pixmap_.rect(), QColor(5, 9, 18, 245));
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
    for (int y = -tile_h; y < r.height(); y += tile_h) {
        for (int x = -background_offset_; x < r.width(); x += tile_w) {
            p.drawPixmap(x, y, pattern_pixmap_);
        }
    }

    QLinearGradient vignette(r.topLeft(), r.bottomLeft());
    vignette.setColorAt(0.0, QColor(0, 0, 0, 25));
    vignette.setColorAt(0.5, QColor(0, 0, 0, 105));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 185));
    p.fillRect(r, vignette);

    QWidget::paintEvent(event);
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
}
