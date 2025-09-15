// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_frontend_util.h"
#include "qt_common/qt_common.h"

#ifdef YUZU_QT_WIDGETS
#include <QFileDialog>
#endif

namespace QtCommon::Frontend {

StandardButton ShowMessage(
    Icon icon, const QString &title, const QString &text, StandardButtons buttons, QObject *parent)
{
#ifdef YUZU_QT_WIDGETS
    QMessageBox *box = new QMessageBox(icon, title, text, buttons, (QWidget *) parent);
    return static_cast<QMessageBox::StandardButton>(box->exec());
#endif
    // TODO(crueter): If Qt Widgets is disabled...
    // need a way to reference icon/buttons too
}

const QString GetOpenFileName(const QString &title,
                              const QString &dir,
                              const QString &filter,
                              QString *selectedFilter,
                              Options options)
{
#ifdef YUZU_QT_WIDGETS
    return QFileDialog::getOpenFileName((QWidget *) rootObject, title, dir, filter, selectedFilter, options);
#endif
}

} // namespace QtCommon::Frontend
