// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QAbstractButton>

#include "libqt_common.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/abstract/progress.h"
#include "qt_common/qt_common.h"

namespace QtCommon::Frontend {

StandardButton ShowMessage(
    Icon icon, const QString &title, const QString &text, StandardButtons buttons, QObject *parent)
{
    QMessageBox *box = new QMessageBox(QMessageBox::Icon(int(icon)), title, text, QMessageBox::StandardButtons(int(buttons)), (QWidget *) parent);
    return StandardButton(box->exec());
}

WidgetsProgressDialog::WidgetsProgressDialog(const QString& labelText,
                                             const QString& cancelButtonText, int minimum,
                                             int maximum, QWidget* parent, Qt::WindowFlags f)
    : QtProgressDialog(labelText, cancelButtonText, minimum, maximum, parent, f),
      m_dialog(new QProgressDialog(labelText, cancelButtonText, minimum, maximum, parent, f)) {
    m_dialog->setAutoClose(false);
    m_dialog->setAutoReset(false);
    m_dialog->setMinimumDuration(100);
    m_dialog->setWindowModality(Qt::WindowModal);
}

bool WidgetsProgressDialog::wasCanceled() const {
    return m_dialog->wasCanceled();
}

void WidgetsProgressDialog::setWindowModality(Qt::WindowModality modality) {
    m_dialog->setWindowModality(modality);
}

void WidgetsProgressDialog::setMinimumDuration(int durationMs) {
    m_dialog->setMinimumDuration(durationMs);
}

void WidgetsProgressDialog::setAutoClose(bool autoClose) {
    m_dialog->setAutoClose(autoClose);
}

void WidgetsProgressDialog::setAutoReset(bool autoReset) {
    m_dialog->setAutoReset(autoReset);
}

void WidgetsProgressDialog::setTitle(QString title) {
    m_dialog->setWindowTitle(title);
}

void WidgetsProgressDialog::setLabelText(QString text) {
    m_dialog->setLabelText(text);
}

void WidgetsProgressDialog::setMinimum(int min) {
    m_dialog->setMinimum(min);
}

void WidgetsProgressDialog::setMaximum(int max) {
    m_dialog->setMaximum(max);
}

void WidgetsProgressDialog::setValue(int value) {
    m_dialog->setValue(value);
}

bool WidgetsProgressDialog::close() {
    m_dialog->close();
    return true;
}

void WidgetsProgressDialog::show() {
    m_dialog->show();
}

std::unique_ptr<QtProgressDialog> newProgressDialog(const QString& labelText, const QString& cancelButtonText,
                                                    int minimum, int maximum, Qt::WindowFlags f) {
    return std::make_unique<WidgetsProgressDialog>(labelText, cancelButtonText, minimum, maximum,
                                                   (QWidget*)rootObject, f);
}

QtProgressDialog* newProgressDialogPtr(const QString& labelText, const QString& cancelButtonText,
                                       int minimum, int maximum,
                                       Qt::WindowFlags f) {
    return new WidgetsProgressDialog(labelText, cancelButtonText, minimum, maximum,
                                     (QWidget*)rootObject, f);
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
