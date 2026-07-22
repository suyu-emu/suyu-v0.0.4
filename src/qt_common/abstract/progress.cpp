// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "progress.h"

namespace QtCommon::Frontend {

QtProgressDialog::QtProgressDialog(const QString&, const QString&, int, int, QObject* parent,
                                   Qt::WindowFlags)
    : QObject(parent) {}

} // namespace QtCommon::Frontend
