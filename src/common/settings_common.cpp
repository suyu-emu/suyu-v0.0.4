// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include "common/settings_common.h"

namespace Settings {

BasicSetting::BasicSetting(Linkage& linkage, const std::string& name, enum Category category_,
                           bool save_, bool runtime_modifiable_)
    : label{name}, category{category_}, id{linkage.count}, save{save_}, runtime_modifiable{
                                                                            runtime_modifiable_} {
    linkage.by_category[category].push_front(this);
    linkage.count++;
}

BasicSetting::~BasicSetting() = default;

std::string BasicSetting::ToStringGlobal() const {
    return this->ToString();
}

bool BasicSetting::UsingGlobal() const {
    return true;
}

void BasicSetting::SetGlobal(bool global) {}

bool BasicSetting::Save() const {
    return save;
}

bool BasicSetting::RuntimeModfiable() const {
    return runtime_modifiable;
}

Category BasicSetting::Category() const {
    return category;
}

const std::string& BasicSetting::GetLabel() const {
    return label;
}

static bool configuring_global = true;

bool IsConfiguringGlobal() {
    return configuring_global;
}

void SetConfiguringGlobal(bool is_global) {
    configuring_global = is_global;
}

} // namespace Settings
