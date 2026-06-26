#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# check if a package is defined

echo "$LIBS" | grep "$1" >/dev/null 2>&1
