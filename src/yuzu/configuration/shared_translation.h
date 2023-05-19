// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>
#include <QString>

class QWidget;

namespace ConfigurationShared {
using TranslationMap = std::map<u32, std::pair<QString, QString>>;
using ComboboxTranslations = std::vector<std::pair<u32, QString>>;
using ComboboxTranslationMap = std::map<std::type_index, ComboboxTranslations>;

std::unique_ptr<TranslationMap> InitializeTranslations(QWidget* parent);

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QWidget* parent);

} // namespace ConfigurationShared
