// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <QBoxLayout>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStyle>
#include <QWidget>
#include <qabstractbutton.h>
#include <qabstractslider.h>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qnamespace.h>
#include <qsize.h>
#include <qsizepolicy.h>
#include <qsurfaceformat.h>
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_per_game.h"
#include "yuzu/configuration/shared_translation.h"

namespace ConfigurationShared {

Tab::Tab(std::shared_ptr<std::forward_list<Tab*>> group_, QWidget* parent)
    : QWidget(parent), group{group_} {
    if (group != nullptr) {
        group->push_front(this);
    }
}

Tab::~Tab() = default;

} // namespace ConfigurationShared
