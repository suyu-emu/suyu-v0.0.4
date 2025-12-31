#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# Replace a specified package with a modified json.

FILE=$(echo "$CPMFILES" | xargs grep -l "\"$1\"")

jq --indent 4 --argjson repl "$2" ".\"$1\" *= \$repl" "$FILE" >"$FILE".new
mv "$FILE".new "$FILE"

echo "-- * -- Updated $FILE"
