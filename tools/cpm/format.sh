#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# shellcheck disable=SC1091
. tools/cpm/common.sh

for file in $CPMFILES; do
    jq --indent 4 < "$file" > "$file".new
    mv "$file".new "$file"
done
