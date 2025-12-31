#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

for file in $CPMFILES; do
	jq --indent 4 <"$file" >"$file".new
	mv "$file".new "$file"
done
