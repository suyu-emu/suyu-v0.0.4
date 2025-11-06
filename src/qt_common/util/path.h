// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_PATH_UTIL_H
#define QT_PATH_UTIL_H

#include "common/common_types.h"
#include <QObject>

namespace QtCommon::Path { bool OpenShaderCache(u64 program_id, QObject *parent); }

#endif // QT_PATH_UTIL_H
