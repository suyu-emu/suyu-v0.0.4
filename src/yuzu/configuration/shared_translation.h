// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>
#include <QString>
#include "common/common_types.h"

class QWidget;

namespace ConfigurationShared {
using TranslationMap = std::map<u32, std::pair<QString, QString>>;
using ComboboxTranslations = std::vector<std::pair<u32, QString>>;
using ComboboxTranslationMap = std::map<u32, ComboboxTranslations>;

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent);

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent);

} // namespace ConfigurationShared
