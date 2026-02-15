// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <QWidget>

namespace VideoCore {
class ShaderNotify;
}
namespace Core {
struct PerfStatsResults;
}
namespace Ui {
class PerformanceOverlay;
}

class QLineSeries;
class QChart;
class QChartView;
class QValueAxis;
class MainWindow;

class PerformanceOverlay : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceOverlay(MainWindow* parent = nullptr);
    ~PerformanceOverlay();

protected:
    void paintEvent(QPaintEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void resetPosition(const QPoint& pos);
    void updateStats(const Core::PerfStatsResults &results, const VideoCore::ShaderNotify &shaders);

    MainWindow *m_mainWindow = nullptr;
    Ui::PerformanceOverlay* ui;

    // colors
    QColor m_background{127, 127, 127, 190};

    QPoint m_offset{25, 75};

    // frametime
    const size_t NUM_FRAMETIME_SAMPLES = 300;
    std::deque<double> m_frametimeSamples;

    // fps
    const size_t NUM_FPS_SAMPLES = 120;
    qreal m_xPos = 0;
    std::deque<double> m_fpsSamples;
    std::deque<QPointF> m_fpsPoints;

    // drag
    QPoint m_drag_start_pos;

    // fps chart
    QLineSeries *m_fpsSeries = nullptr;
    QChart *m_fpsChart = nullptr;
    QChartView *m_fpsChartView = nullptr;
    QValueAxis *m_fpsX = nullptr;
    QValueAxis *m_fpsY = nullptr;

signals:
    void closed();
};
