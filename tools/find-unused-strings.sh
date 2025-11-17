#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

ANDROID=src/android/app/src/main
STRINGS=$ANDROID/res/values/strings.xml

SRC=$(mktemp)

# We start out by getting the list of source strings...
grep -e "string name" $STRINGS | cut -d'"' -f2 > "$SRC"

# ... then search for each string as R.string. or @string/
while IFS= read -r str; do
	grep -qre "R.string.$str\|@string/$str" "$ANDROID" && continue

	echo "-- Removing unused string $str"
	sed "/string name=\"$str\"/d" "$STRINGS" > "$STRINGS.new"
	mv "$STRINGS.new" "$STRINGS"
done < "$SRC"

rm -rf "$TMP_DIR"