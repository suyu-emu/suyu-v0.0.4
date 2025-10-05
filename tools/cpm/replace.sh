#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# Replace a specified package with a modified json.

# env vars:
# - PACKAGE: The package key to act on
# - NEW_JSON: The new json to use

[ -z "$PACKAGE" ] && echo "You must provide the PACKAGE environment variable." && return 1
[ -z "$NEW_JSON" ] && echo "You must provide the NEW_JSON environment variable." && return 1

FILE=$(tools/cpm/which.sh "$PACKAGE")

jq --indent 4 --argjson repl "$NEW_JSON" ".\"$PACKAGE\" *= \$repl" "$FILE" > "$FILE".new
mv "$FILE".new "$FILE"

echo "-- * -- Updated $FILE"
