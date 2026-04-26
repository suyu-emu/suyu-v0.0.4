// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QLabel;
class QProgressBar;
class QPushButton;

/// Guides users through the decryption key setup workflow.
/// Does NOT perform decryption — points users to external tools.
class DecryptionFlowWidget : public QWidget {
    Q_OBJECT

public:
    explicit DecryptionFlowWidget(QWidget* parent = nullptr);
    ~DecryptionFlowWidget() override;

    /// Returns true when valid keys are detected in the expected location.
    [[nodiscard]] bool HasValidKeys() const;

    /// Refresh key detection status.
    void Refresh();

signals:
    void KeysStatusChanged(bool valid);

public slots:
    void OnBrowseKeys();
    void OnRefreshStatus();

private:
    void SetupUi();
    void DetectKeys();

    QLabel* lbl_status_{};
    QLabel* lbl_instructions_{};
    QPushButton* btn_browse_{};
    QPushButton* btn_refresh_{};
    QProgressBar* progress_{};
    bool keys_valid_{false};
};
