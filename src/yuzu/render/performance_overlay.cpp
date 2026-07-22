// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/perf_stats.h"
#include "performance_overlay.h"
#include "ui_performance_overlay.h"

#include "main_window.h"

#include <QChart>
#include <QChartView>
#include <QGraphicsLayout>
#include <QLineSeries>
#include <QMouseEvent>
#include <QPainter>
#include <QValueAxis>

// TODO(crueter): Reset samples when user changes turbo, slow, etc.
PerformanceOverlay::PerformanceOverlay(MainWindow* parent)
    : QWidget(parent), m_mainWindow{parent}, ui(new Ui::PerformanceOverlay) {
    ui->setupUi(this);

    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    raise();

    // chart setup
    m_fpsSeries = new QLineSeries(this);

    QPen pen(Qt::red);
    pen.setWidth(2);
    m_fpsSeries->setPen(pen);

    m_fpsChart = new QChart;
    m_fpsChart->addSeries(m_fpsSeries);
    m_fpsChart->legend()->hide();
    m_fpsChart->setBackgroundBrush(Qt::black);
    m_fpsChart->setBackgroundVisible(true);
    m_fpsChart->layout()->setContentsMargins(2, 2, 2, 2);
    m_fpsChart->setMargins(QMargins{4, 4, 4, 4});

    // axes
    m_fpsX = new QValueAxis(this);
    m_fpsX->setRange(0, NUM_FPS_SAMPLES);
    m_fpsX->setVisible(false);

    m_fpsY = new QValueAxis(this);
    m_fpsY->setRange(0, 60);
    m_fpsY->setLabelFormat(QStringLiteral("%d"));
    m_fpsY->setLabelsColor(Qt::white);

    QFont axisFont = m_fpsY->labelsFont();
    axisFont.setPixelSize(10);
    m_fpsY->setLabelsFont(axisFont);
    m_fpsY->setTickCount(3);

    // gray-ish label w/ white lines
    m_fpsY->setLabelsVisible(true);
    m_fpsY->setGridLineColor(QColor(50, 50, 50));
    m_fpsY->setLinePenColor(Qt::white);

    m_fpsChart->addAxis(m_fpsX, Qt::AlignBottom);
    m_fpsChart->addAxis(m_fpsY, Qt::AlignLeft);
    m_fpsSeries->attachAxis(m_fpsX);
    m_fpsSeries->attachAxis(m_fpsY);

    // chart view
    m_fpsChartView = new QChartView(m_fpsChart, this);
    m_fpsChartView->setRenderHint(QPainter::Antialiasing);
    m_fpsChartView->setMinimumHeight(100);

    ui->verticalLayout->addWidget(m_fpsChartView, 1);

    // thanks Debian.
    QFont font = ui->fps->font();
    font.setWeight(QFont::DemiBold);

    ui->fps->setFont(font);
    ui->frametime->setFont(font);

    // pos/stats
    resetPosition(m_mainWindow->pos());
    connect(parent, &MainWindow::positionChanged, this, &PerformanceOverlay::resetPosition);
    connect(m_mainWindow, &MainWindow::statsUpdated, this, &PerformanceOverlay::updateStats);
}

PerformanceOverlay::~PerformanceOverlay() {
    delete ui;
}

void PerformanceOverlay::resetPosition(const QPoint& _) {
    auto pos = m_mainWindow->pos();
    move(pos.x() + m_offset.x(), pos.y() + m_offset.y());
}

void PerformanceOverlay::updateStats(const Core::PerfStatsResults& results,
                                     const VideoCore::ShaderNotify& shaders) {
    auto fps = results.average_game_fps;
    if (!std::isnan(fps)) {
        // don't sample measurements < 3 fps because they are probably outliers or freezes
        static constexpr double FPS_SAMPLE_THRESHOLD = 3.0;

        QString fpsText = tr("%1 fps").arg(std::round(fps), 0, 'f', 0);
        // if (!m_fpsSuffix.isEmpty()) fpsText = fpsText % QStringLiteral(" (%1)").arg(m_fpsSuffix);
        ui->fps->setText(fpsText);

        // sampling
        if (fps > FPS_SAMPLE_THRESHOLD) {
            m_fpsSamples.push_back(fps);
            m_fpsPoints.push_back(QPointF{m_xPos++, fps});
        }

        if (m_fpsSamples.size() > NUM_FPS_SAMPLES) {
            m_fpsSamples.pop_front();
            m_fpsPoints.pop_front();
        }

        // For the average only go back 10 samples max
        if (m_fpsSamples.size() >= 2) {
            const int back_search = std::min(size_t(10), m_fpsSamples.size() - 1);
            double sum = std::accumulate(m_fpsSamples.end() - back_search, m_fpsSamples.end(), 0.0);
            double avg = sum / back_search;

            ui->fps_avg->setText(tr("Avg: %1").arg(avg, 0, 'f', 0));
        }

        // chart it :)
        if (!m_fpsPoints.empty()) {
            auto [min_it, max_it] = std::minmax_element(m_fpsSamples.begin(), m_fpsSamples.end());
            double min_fps = *min_it;
            double max_fps = *max_it;

            ui->fps_min->setText(tr("Min: %1").arg(min_fps, 0, 'f', 0));
            ui->fps_max->setText(tr("Max: %1").arg(max_fps, 0, 'f', 0));

            m_fpsSeries->replace(QList<QPointF>(m_fpsPoints.begin(), m_fpsPoints.end()));

            qreal x_min = std::max(0.0, m_xPos - NUM_FPS_SAMPLES);
            qreal x_max = std::max(qreal(10), m_xPos);
            m_fpsX->setRange(x_min, x_max);
            m_fpsY->setRange(0.0, max_fps);
        }
    }

    auto ft = results.frametime;
    if (!std::isnan(ft)) {
        // don't sample measurements > 500 ms because they are probably outliers
        static constexpr double FT_SAMPLE_THRESHOLD = 500.0;

        double ft_ms = results.frametime * 1000.0;
        ui->frametime->setText(tr("%1 ms").arg(ft_ms, 0, 'f', 2));

        // sampling
        if (ft_ms <= FT_SAMPLE_THRESHOLD)
            m_frametimeSamples.push_back(ft_ms);

        if (m_frametimeSamples.size() > NUM_FRAMETIME_SAMPLES)
            m_frametimeSamples.pop_front();

        if (!m_frametimeSamples.empty()) {
            auto [min_it, max_it] =
                std::minmax_element(m_frametimeSamples.begin(), m_frametimeSamples.end());
            ui->ft_min->setText(tr("Min: %1").arg(*min_it, 0, 'f', 1));
            ui->ft_max->setText(tr("Max: %1").arg(*max_it, 0, 'f', 1));
        }

        // For the average only go back 10 samples max
        if (m_frametimeSamples.size() >= 2) {
            const int back_search = std::min(size_t(10), m_frametimeSamples.size() - 1);
            double sum = std::accumulate(m_frametimeSamples.end() - back_search,
                                         m_frametimeSamples.end(), 0.0);
            double avg = sum / back_search;

            ui->ft_avg->setText(tr("Avg: %1").arg(avg, 0, 'f', 1));
        }
    }
}

void PerformanceOverlay::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(m_background);
    painter.setPen(Qt::NoPen);

    painter.drawRoundedRect(rect(), 10.0, 10.0);
}

void PerformanceOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_drag_start_pos = event->pos();
    }
}

void PerformanceOverlay::mouseMoveEvent(QMouseEvent* event) {
    // drag
    if (event->buttons() & Qt::LeftButton) {
        QPoint new_global_pos = event->globalPosition().toPoint() - m_drag_start_pos;
        m_offset = new_global_pos - m_mainWindow->pos();
        move(new_global_pos);
    }
}

void PerformanceOverlay::closeEvent(QCloseEvent* event) {
    emit closed();
}
