// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef _WIN32

#include <signal.h>

namespace Common {

// Android's ART overrides sigaction with its own wrapper. This is problematic for SIGSEGV
// in particular, because ARTs handler access TPIDR_EL0, so this extracts the libc version
// and calls it directly.
int SigAction(int signum, const struct sigaction* act, struct sigaction* oldact);

} // namespace Common

#endif
