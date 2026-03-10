// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "frontend.h"

namespace QtCommon::Frontend {

const QString GetOpenFileName(const QString& title, const QString& dir, const QString& filter,
                        QString* selectedFilter) {
    return QFileDialog::getOpenFileName(rootObject, title, dir, filter, selectedFilter);
}

const QStringList GetOpenFileNames(const QString& title, const QString& dir, const QString& filter,
                             QString* selectedFilter) {
    return QFileDialog::getOpenFileNames(rootObject, title, dir, filter, selectedFilter);
}

const QString GetSaveFileName(const QString& title, const QString& dir, const QString& filter,
                        QString* selectedFilter) {
    return QFileDialog::getSaveFileName(rootObject, title, dir, filter, selectedFilter);
}

const QString GetExistingDirectory(const QString& caption, const QString& dir) {
    return QFileDialog::getExistingDirectory(rootObject, caption, dir);
}

} // namespace QtCommon::Frontend
