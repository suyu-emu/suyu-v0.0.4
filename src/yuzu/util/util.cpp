// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>
#include <QPainter>

#include "applets/qt_profile_select.h"
#include "common/logging.h"
#include "core/frontend/applets/profile_select.h"
#include "core/hle/service/acc/profile_manager.h"
#include "frontend_common/data_manager.h"
#include "qt_common/qt_common.h"
#include "yuzu/util/util.h"

#ifdef _WIN32
#include <windows.h>
#include "common/fs/file.h"
#endif

QFont GetMonospaceFont() {
    QFont font(QStringLiteral("monospace"));
    // Automatic fallback to a monospace font on on platforms without a font called "monospace"
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

QString ReadableByteSize(qulonglong size) {
    return QString::fromStdString(FrontendCommon::DataManager::ReadableBytesSize(size));
}

QPixmap CreateCirclePixmapFromColor(const QColor& color) {
    QPixmap circle_pixmap(16, 16);
    circle_pixmap.fill(Qt::transparent);
    QPainter painter(&circle_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(color);
    painter.setBrush(color);
    painter.drawEllipse({circle_pixmap.width() / 2.0, circle_pixmap.height() / 2.0}, 7.0, 7.0);
    return circle_pixmap;
}

const std::optional<Common::UUID> GetProfileID() {
    // if there's only a single profile, the user probably wants to use that... right?
    const auto& profiles = QtCommon::system->GetProfileManager().FindExistingProfileUUIDs();
    if (profiles.size() == 1) {
        return profiles[0];
    }

    const auto select_profile = [] {
        const Core::Frontend::ProfileSelectParameters parameters{
            .mode = Service::AM::Frontend::UiMode::UserSelector,
            .invalid_uid_list = {},
            .display_options = {},
            .purpose = Service::AM::Frontend::UserSelectionPurpose::General,
        };
        QtProfileSelectionDialog dialog(*QtCommon::system, QtCommon::rootObject, parameters);
        dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                              Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
        dialog.setWindowModality(Qt::WindowModal);

        if (dialog.exec() == QDialog::Rejected) {
            return -1;
        }

        return dialog.GetIndex();
    };

    const auto index = select_profile();
    if (index == -1) {
        return std::nullopt;
    }

    const auto uuid =
        QtCommon::system->GetProfileManager().GetUser(static_cast<std::size_t>(index));
    ASSERT(uuid);

    return uuid;
}
std::string GetProfileIDString() {
    const auto uuid = GetProfileID();
    if (!uuid)
        return "";

    auto user_id = uuid->AsU128();

    return fmt::format("{:016X}{:016X}", user_id[1], user_id[0]);
}
