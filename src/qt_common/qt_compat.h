// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
#define STATE_CHANGED stateChanged
#define CHECKSTATE_TYPE int
#else
#define STATE_CHANGED checkStateChanged
#define CHECKSTATE_TYPE Qt::CheckState
#endif
