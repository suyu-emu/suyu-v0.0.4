// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

namespace Common::ADPF {

enum class Session {
    Render,
    Background,
};

bool IsSessionSupported(Session session);

bool AddCurrentThread(Session session);
void RemoveCurrentThread();

void SetTargetWorkDuration(std::chrono::nanoseconds target);

void ReportFrameInterval();

void Shutdown();

} // namespace Common::ADPF
