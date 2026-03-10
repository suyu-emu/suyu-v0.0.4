#! /bin/sh

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Only clang-formats Qt stuff. :)
find src/qt_common src/yuzu -iname "*.h" -o -iname "*.cpp" | xargs clang-format -i -style=file:src/.clang-format
