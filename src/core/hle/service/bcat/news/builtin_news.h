// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

namespace Service::News {

void EnsureBuiltinNewsLoaded();

    std::vector<u8> BuildMsgpack(std::string_view title, std::string_view body,
                                 std::string_view topic_name,
                                 u64 published_at,
                                 u64 pickup_limit,
                                 u32 priority,
                                 const std::vector<std::string>& languages,
                                 const std::string& author_name,
                                 const std::vector<std::pair<std::string, std::string>>& assets,
                                 const std::string& html_url,
                                 std::optional<u32> override_news_id = std::nullopt);

} // namespace Service::News
