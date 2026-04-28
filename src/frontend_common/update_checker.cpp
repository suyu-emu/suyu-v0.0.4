// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef NIGHTLY_BUILD
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#endif

#include <fmt/format.h>
#include "common/net/net.h"
#include "common/scm_rev.h"
#include "update_checker.h"

#include "common/logging.h"

std::optional<Common::Net::Release> UpdateChecker::GetUpdate() {
    const auto latest = Common::Net::GetLatestRelease();
    if (!latest) return std::nullopt;

    LOG_INFO(Frontend, "Received update {}", latest->title);

#ifdef NIGHTLY_BUILD
    std::vector<std::string> result;

    boost::split(result, latest->tag, boost::is_any_of("."));
    if (result.size() != 2)
        return std::nullopt;

    const std::string tag = result[1];

    boost::split(result, std::string{Common::g_build_version}, boost::is_any_of("-"));
    if (result.empty())
        return std::nullopt;

    const std::string build = result[0];
#else
    const std::string tag = latest->tag;
    const std::string build = Common::g_build_version;
#endif

    if (tag != build)
        return latest;

    return std::nullopt;
}
