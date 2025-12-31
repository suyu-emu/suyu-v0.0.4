#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

SUM=$(curl -Ls "$1" -o - | sha512sum)
echo "$SUM" | cut -d " " -f1
