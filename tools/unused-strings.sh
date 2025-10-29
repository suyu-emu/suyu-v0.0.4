#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

ANDROID=src/android/app/src/main
STRINGS=$ANDROID/res/values/strings.xml

TMP_DIR=$(mktemp -d)

USED="$TMP_DIR"/used
UNUSED="$TMP_DIR"/unused
FILTERED="$TMP_DIR"/filtered
ALL_STRINGS="$TMP_DIR"/all

# First we find what files to search and what strings are defined
find $ANDROID -type f -iname '*.xml' -a -not -iname '*strings.xml' -o -iname '*.kt' | grep -v drawable >"$TMP_DIR"/files
grep -e "string name" $STRINGS | cut -d'"' -f2 >"$ALL_STRINGS"

# then get a list of all strings that are actually used
set +e
while IFS= read -r file; do
    grep -o 'R\.string\.[a-zA-Z0-9_]\+' "$file" >> "$USED"
    grep -o '@string/[a-zA-Z0-9_]\+' "$file" >> "$USED"
done <"$TMP_DIR"/files
set -e

# filter out "@string/" and "R.string." from the strings to get the raw names
sed 's/R.string.//' "$USED" | sed 's/@string\///' \
    | sort -u | grep -v app_name_suffixed > "$FILTERED"

# now we run a sort + uniq -u pass - this basically removes all strings that are
# present in BOTH the used strings list AND strings.xml
# thus giving us strings that are either ONLY used in strings.xml OR in the files

# NB: This also gives strings that may be found in comments but nowhere else,
# as well as some android builtins, but sed-ing those out of strings.xml is a no-op

# e.g. appbar_scrolling_view_behavior
cat "$FILTERED" "$ALL_STRINGS" | sort | uniq -u > "$UNUSED"

# finally, print out all unused strings and remove them from ALL strings.xml definitions
while IFS= read -r string; do
	echo "$string"
    find $ANDROID/res -iname '*strings.xml' | while IFS= read -r file; do
        sed "/string name=\"$string\"/d" "$file" > "$file.new"
		mv "$file.new" "$file"
	done
done < "$UNUSED"
