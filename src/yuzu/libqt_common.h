// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "qt_common/abstract/progress.h"

#include <QProgressDialog>

namespace QtCommon::Frontend {

class WidgetsProgressDialog : public QtProgressDialog {
    Q_OBJECT
public:
    WidgetsProgressDialog(const QString& labelText, const QString& cancelButtonText, int minimum,
                          int maximum, QWidget* parent = nullptr, Qt::WindowFlags f = {});

    bool wasCanceled() const override;
    void setWindowModality(Qt::WindowModality modality) override;
    void setMinimumDuration(int durationMs) override;
    void setAutoClose(bool autoClose) override;
    void setAutoReset(bool autoReset) override;

public slots:
    void setTitle(QString title) override;
    void setLabelText(QString text) override;
    void setMinimum(int min) override;
    void setMaximum(int max) override;
    void setValue(int value) override;
    bool close() override;
    void show() override;

private:
    QProgressDialog* m_dialog;
};

}
