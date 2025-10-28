#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

command -v tx-cli && COMMAND=tx-cli
command -v tx && COMMAND=tx

$COMMAND pull -t -f

git add dist/languages/*.ts src/android/app/src/main/res/values*/strings.xml

git commit -m "[dist] update translations from Transifex" -sS
