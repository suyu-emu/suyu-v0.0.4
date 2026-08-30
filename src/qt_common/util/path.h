// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include "common/common_types.h"

namespace QtCommon::Path {
bool OpenShaderCache(u64 program_id, QObject* parent);
}
