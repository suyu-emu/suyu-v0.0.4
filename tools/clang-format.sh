#! /bin/sh

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

exec find src -iname "*.h" -o -iname "*.cpp" | xargs clang-format -i -style=file:src/.clang-format
