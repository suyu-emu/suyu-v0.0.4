#!/bin/sh

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

SUM=$(wget -q "$1" -O - | sha512sum)
echo "$SUM" | cut -d " " -f1
