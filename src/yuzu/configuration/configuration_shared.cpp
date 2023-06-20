// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <type_traits>
#include "yuzu/configuration/configuration_shared.h"

namespace ConfigurationShared {

Tab::Tab(std::shared_ptr<std::forward_list<Tab*>> group, QWidget* parent) : QWidget(parent) {
    if (group != nullptr) {
        group->push_front(this);
    }
}

Tab::~Tab() = default;

} // namespace ConfigurationShared
