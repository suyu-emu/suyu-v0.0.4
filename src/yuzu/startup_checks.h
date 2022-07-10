// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

constexpr char STARTUP_CHECK_ENV_VAR[] = "YUZU_DO_STARTUP_CHECKS";

bool CheckVulkan();
bool StartupChecks();
