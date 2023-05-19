// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <iterator>
#include <memory>
#include <QCheckBox>
#include <QComboBox>
#include <QWidget>
#include <qobjectdefs.h>
#include "common/settings.h"
#include "yuzu/configuration/shared_translation.h"

namespace ConfigurationShared {

class Tab : public QWidget {
    Q_OBJECT

public:
    explicit Tab(std::shared_ptr<std::forward_list<Tab*>> group_, QWidget* parent = nullptr);
    ~Tab();

    virtual void ApplyConfiguration() = 0;
    virtual void SetConfiguration() = 0;

private:
    std::shared_ptr<std::forward_list<Tab*>> group;
};

constexpr int USE_GLOBAL_INDEX = 0;
constexpr int USE_GLOBAL_SEPARATOR_INDEX = 1;
constexpr int USE_GLOBAL_OFFSET = 2;

// CheckBoxes require a tracker for their state since we emulate a tristate CheckBox
enum class CheckState {
    Off,    // Checkbox overrides to off/false
    On,     // Checkbox overrides to on/true
    Global, // Checkbox defers to the global state
    Count,  // Simply the number of states, not a valid checkbox state
};

} // namespace ConfigurationShared
