#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# check which file a package is in

JSON=$(echo "$CPMFILES" | xargs grep -l "\"$1\"")

[ -z "$JSON" ] && echo "!! No cpmfile definition for $1" >&2 && exit 1

echo "$JSON"
