#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# check which file a package is in
# shellcheck disable=SC1091
. tools/cpm/common.sh

# shellcheck disable=SC2086
JSON=$(echo "$CPMFILES" | xargs grep -l "$1")

[ -z "$JSON" ] && echo "!! No cpmfile definition for $1" >&2 && exit 1

echo "$JSON"