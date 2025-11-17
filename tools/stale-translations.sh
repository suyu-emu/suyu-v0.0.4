#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

ANDROID=src/android/app/src/main
STRINGS=$ANDROID/res/values/strings.xml

TMP_DIR=$(mktemp -d)

SRC="$TMP_DIR"/src
LOCALES="$TMP_DIR"/locales
TRANSLATED="$TMP_DIR"/translated
COMBINED="$TMP_DIR"/combined

# We start out by getting the list of source strings so we can compare all locale strings to this
grep -e "string name" $STRINGS | grep -v 'translatable="false"' | cut -d'"' -f2 > "$SRC"

# ... then we determine what locales to check
find $ANDROID/res/values-* -name 'strings.xml' > "$LOCALES"

while IFS= read -r locale; do
	echo "Locale $(echo "$locale" | cut -d"-" -f2- | cut -d"/" -f1)"
	grep -e "string name" "$locale" | cut -d'"' -f2 > "$TRANSLATED"
	cat "$TRANSLATED" "$SRC" | sort | uniq -u > "$COMBINED"

	# This will also include strings that are present in the source but NOT translated, so we filter those out now:
	while IFS= read -r unused; do
		{ grep -e "string name=\"$unused\"" "$STRINGS" >/dev/null; } || \
		{ echo "-- Removing unused translation $unused" && sed "/string name=\"$unused\"/d" "$locale" > "$locale.new" && mv "$locale.new" "$locale"; }
	done < "$COMBINED"
done < "$LOCALES"

rm -rf "$TMP_DIR"