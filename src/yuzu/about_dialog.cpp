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
    const auto description = std::string(Common::g_build_version);
    const auto build_id = std::string(Common::g_build_id);

    std::string yuzu_build;
    if (Common::g_is_dev_build) {
        yuzu_build = fmt::format("Eden Nightly | {}-{}", description, build_id);
    } else {
        yuzu_build = fmt::format("Eden | {}", description);
    }

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
