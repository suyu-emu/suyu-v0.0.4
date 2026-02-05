// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Common::Log {

struct Entry;

/// Formats a log entry into the provided text buffer.
std::string FormatLogMessage(const Entry& entry) noexcept;
/// Prints the same message as `PrintMessage`, but colored according to the severity level.
void PrintColoredMessage(const Entry& entry) noexcept;
/// Formats and prints a log entry to the android logcat.
void PrintMessageToLogcat(const Entry& entry) noexcept;
} // namespace Common::Log
