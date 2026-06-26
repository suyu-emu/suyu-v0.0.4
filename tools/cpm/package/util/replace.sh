#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# Replace a specified package with a modified json.

jq --indent 4 --argjson repl "$2" ".\"$1\" *= \$repl" cpmfile.json >cpmfile.json.new
mv cpmfile.json.new cpmfile.json

echo "-- * -- Updated cpmfile.json"
