// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>
#include "video_core/debug_utils/debug_utils.h"

/**
 * Utility class which forwards calls to OnMaxwellBreakPointHit and OnMaxwellResume to public slots.
 * This is because the Maxwell breakpoint callbacks are called from a non-GUI thread, while
 * the widget usually wants to perform reactions in the GUI thread.
 */
class BreakPointObserverDock : public QDockWidget,
                               protected Tegra::DebugContext::BreakPointObserver {
    Q_OBJECT

public:
    BreakPointObserverDock(std::shared_ptr<Tegra::DebugContext> debug_context, const QString& title,
                           QWidget* parent = nullptr);

    void OnMaxwellBreakPointHit(Tegra::DebugContext::Event event, void* data) override;
    void OnMaxwellResume() override;

signals:
    void Resumed();
    void BreakPointHit(Tegra::DebugContext::Event event, void* data);

private:
    virtual void OnBreakPointHit(Tegra::DebugContext::Event event, void* data) = 0;
    virtual void OnResumed() = 0;
};
