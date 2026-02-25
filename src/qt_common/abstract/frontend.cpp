// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QLineEdit>
#include "frontend.h"
#include "qt_common/qt_common.h"

#ifdef YUZU_QT_WIDGETS
#include <QFileDialog>
#endif

#include <QAbstractButton>
#include <QInputDialog>

namespace QtCommon::Frontend {

StandardButton ShowMessage(Icon icon, const QString& title, const QString& text,
                           StandardButtons buttons, QObject* parent) {
#ifdef YUZU_QT_WIDGETS
    QMessageBox* box = new QMessageBox(icon, title, text, buttons, (QWidget*)parent);
    return static_cast<QMessageBox::StandardButton>(box->exec());
#endif
    // TODO(crueter): If Qt Widgets is disabled...
    // need a way to reference icon/buttons too
}

const QString GetOpenFileName(const QString& title, const QString& dir, const QString& filter,
                              QString* selectedFilter, Options options) {
#ifdef YUZU_QT_WIDGETS
    return QFileDialog::getOpenFileName(rootObject, title, dir, filter, selectedFilter, options);
#endif
}

const QStringList GetOpenFileNames(const QString& title, const QString& dir, const QString& filter,
                                   QString* selectedFilter, Options options) {
#ifdef YUZU_QT_WIDGETS
    return QFileDialog::getOpenFileNames(rootObject, title, dir, filter, selectedFilter, options);
#endif
}

const QString GetSaveFileName(const QString& title, const QString& dir, const QString& filter,
                              QString* selectedFilter, Options options) {
#ifdef YUZU_QT_WIDGETS
    return QFileDialog::getSaveFileName(rootObject, title, dir, filter, selectedFilter, options);
#endif
}

const QString GetExistingDirectory(const QString& caption, const QString& dir, Options options) {
#ifdef YUZU_QT_WIDGETS
    return QFileDialog::getExistingDirectory(rootObject, caption, dir, options);
#endif
}

int Choice(const QString& title, const QString& caption, const QStringList& options) {
    QMessageBox box(rootObject);
    box.setText(caption);
    box.setWindowTitle(title);

    for (const QString& opt : options) {
        box.addButton(opt, QMessageBox::AcceptRole);
    }

    box.addButton(QMessageBox::Cancel);

    box.exec();
    auto button = box.clickedButton();
    return options.indexOf(button->text());
}

const QString GetTextInput(const QString& title, const QString& caption,
                           const QString& defaultText) {
    return QInputDialog::getText(rootObject, title, caption, QLineEdit::Normal, defaultText);
}

} // namespace QtCommon::Frontend
