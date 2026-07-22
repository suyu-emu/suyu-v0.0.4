// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <QObject>

namespace QtCommon::Frontend {

class QtProgressDialog : public QObject {
    Q_OBJECT
public:
    QtProgressDialog(const QString& labelText, const QString& cancelButtonText, int minimum,
                     int maximum, QObject* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    virtual ~QtProgressDialog() = default;

    virtual bool wasCanceled() const = 0;
    virtual void setWindowModality(Qt::WindowModality modality) = 0;
    virtual void setMinimumDuration(int durationMs) = 0;
    virtual void setAutoClose(bool autoClose) = 0;
    virtual void setAutoReset(bool autoReset) = 0;

public slots:
    virtual void setTitle(QString title) = 0;
    virtual void setLabelText(QString text) = 0;
    virtual void setMinimum(int min) = 0;
    virtual void setMaximum(int max) = 0;
    virtual void setValue(int value) = 0;

    virtual bool close() = 0;
    virtual void show() = 0;
};

std::unique_ptr<QtProgressDialog> newProgressDialog(const QString& labelText,
                                                    const QString& cancelButtonText, int minimum,
                                                    int maximum,
                                                    Qt::WindowFlags f = Qt::WindowFlags());

QtProgressDialog* newProgressDialogPtr(const QString& labelText, const QString& cancelButtonText,
                                       int minimum, int maximum,
                                       Qt::WindowFlags f = Qt::WindowFlags());

} // namespace QtCommon::Frontend
