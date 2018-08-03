// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMetaType>
#include "yuzu/debugger/graphics/graphics_breakpoint_observer.h"

BreakPointObserverDock::BreakPointObserverDock(std::shared_ptr<Tegra::DebugContext> debug_context,
                                               const QString& title, QWidget* parent)
    : QDockWidget(title, parent), BreakPointObserver(debug_context) {
    qRegisterMetaType<Tegra::DebugContext::Event>("Tegra::DebugContext::Event");

    connect(this, &BreakPointObserverDock::Resumed, this, &BreakPointObserverDock::OnResumed);

    // NOTE: This signal is emitted from a non-GUI thread, but connect() takes
    //       care of delaying its handling to the GUI thread.
    connect(this, &BreakPointObserverDock::BreakPointHit, this,
            &BreakPointObserverDock::OnBreakPointHit, Qt::BlockingQueuedConnection);
}

void BreakPointObserverDock::OnMaxwellBreakPointHit(Tegra::DebugContext::Event event, void* data) {
    emit BreakPointHit(event, data);
}

void BreakPointObserverDock::OnMaxwellResume() {
    emit Resumed();
}
