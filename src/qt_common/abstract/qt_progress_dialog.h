// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QT_PROGRESS_DIALOG_H
#define QT_PROGRESS_DIALOG_H

#include <QWindow>

#ifdef YUZU_QT_WIDGETS
#include <QProgressDialog>
#endif

namespace QtCommon::Frontend {
#ifdef YUZU_QT_WIDGETS

using QtProgressDialog = QProgressDialog;

// TODO(crueter): QML impl
#else
class QtProgressDialog
{
public:
    QtProgressDialog(const QString &labelText,
                     const QString &cancelButtonText,
                     int minimum,
                     int maximum,
                     QObject *parent = nullptr,
                     Qt::WindowFlags f = Qt::WindowFlags());

    bool wasCanceled() const;
    void setWindowModality(Qt::WindowModality modality);
    void setMinimumDuration(int durationMs);
    void setAutoClose(bool autoClose);
    void setAutoReset(bool autoReset);

public slots:
    void setLabelText(QString &text);
    void setRange(int min, int max);
    void setValue(int progress);
    bool close();

    void show();
};
#endif // YUZU_QT_WIDGETS

}
#endif // QT_PROGRESS_DIALOG_H
