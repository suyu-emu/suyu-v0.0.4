// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "yuzu/about_dialog.h"
#include <QIcon>
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"
#include <fmt/ranges.h>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , ui{std::make_unique<Ui::AboutDialog>()}
{
    static const std::string build_id = std::string{Common::g_build_id};
    static const std::string yuzu_build = fmt::format("{} | {} | {}",
        std::string{Common::g_build_name},
        std::string{Common::g_build_version},
        std::string{Common::g_compiler_id}
    );

    const auto override_build = fmt::format(fmt::runtime(
                                                std::string(Common::g_title_bar_format_idle)),
                                            build_id);
    const auto yuzu_build_version = override_build.empty() ? yuzu_build : override_build;

    ui->setupUi(this);
    // Try and request the icon from Qt theme (Linux?)
    const QIcon yuzu_logo = QIcon::fromTheme(QStringLiteral("org.yuzu_emu.yuzu"));
    if (!yuzu_logo.isNull()) {
        ui->labelLogo->setPixmap(yuzu_logo.pixmap(200));
    }
    ui->labelBuildInfo->setText(
        ui->labelBuildInfo->text().arg(QString::fromStdString(yuzu_build_version),
                                       QString::fromUtf8(Common::g_build_date).left(10)));
}

AboutDialog::~AboutDialog() = default;
