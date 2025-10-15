// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_ROM_UTIL_H
#define QT_ROM_UTIL_H

#include "qt_common/qt_common.h"
#include <cstddef>

namespace QtCommon::ROM {

bool RomFSRawCopy(size_t total_size,
                  size_t& read_size,
                  QtProgressCallback callback,
                  const FileSys::VirtualDir& src,
                  const FileSys::VirtualDir& dest,
                  bool full);

}
#endif // QT_ROM_UTIL_H
