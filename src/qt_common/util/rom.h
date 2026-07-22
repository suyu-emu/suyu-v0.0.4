// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include "qt_common/qt_common.h"

namespace QtCommon::ROM {
bool RomFSRawCopy(size_t total_size, size_t& read_size, QtProgressCallback callback, const FileSys::VirtualDir& src, const FileSys::VirtualDir& dest, bool full);
}
