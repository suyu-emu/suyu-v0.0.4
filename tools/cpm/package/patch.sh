#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091
. "$SCRIPTS"/../common.sh

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package patch [PACKAGE]

Create an in-tree patch for the specified package.

EOF

	exit "$RETURN"
}

die() {
	echo "-- $*" >&2
	RETURN=1 usage
}

while :; do
	case "$1" in
	-h | --help) usage ;;
	"$0") break ;;
	"") break ;;
	*)
		if [ -n "$PACKAGE" ]; then
			die "You may only specify one package."
		else
			PACKAGE="$1"
		fi
		;;
	esac

	shift
done

[ -n "$PACKAGE" ] || usage

unset JSON
export PACKAGE

# shellcheck disable=SC1091
. "$SCRIPTS"/vars.sh

if [ "$CI" = true ]; then
	die "CI packages do not support in-tree patching"
fi

patch_dir="$PWD/.patch/$PACKAGE"
TMP=$(make_temp_dir)

tmp_package="${TMP}/${LOWER_PACKAGE}/${KEY}"

# First fetch the package...

# shellcheck disable=SC1091
. "$SCRIPTS"/util/fetch.sh

local_dir="$CPM_SOURCE_CACHE/$LOWER_PACKAGE/$KEY"

if [ ! -d "$local_dir" ]; then
	echo "-- $PACKAGE_NAME is not fetched locally" >&2
	exit 1
fi

src_dir="$tmp_package/$LOWER_PACKAGE/$KEY"

CPM_SOURCE_CACHE="$tmp_package" fetch_package

mkdir -p "$patch_dir"

existing=$(find "$patch_dir" -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]-*.patch' 2>/dev/null | sort | tail -1)
if [ -n "$existing" ]; then
	last_num=$(basename "$existing" | cut -c1-4)
	last_num=$(echo "$last_num" | sed 's/^0*//')
	last_num="${last_num:-0}"
	next_num=$(printf "%04d" $((last_num + 1)))
else
	next_num="0001"
fi

echo "-- Creating patch for $PACKAGE..."

orig_dir="$PWD"
if cmd_exists git; then
	cd "$src_dir"

	git init >/dev/null 2>&1
	git add -A >/dev/null 2>&1
	git commit -m "base" >/dev/null 2>&1

	git --work-tree="$local_dir" add -A #>/dev/null 2>&1

	if git diff --cached --quiet; then
		echo "-- No differences found between local and source"
		cd "$orig_dir"
		rm -rf "$TMP"
		exit 0
	fi

	if ! git commit -v; then
		echo "-- Patch cancelled" >&2
		cd "$orig_dir"
		rm -rf "$TMP"
		exit 1
	fi

	description=$(git log -1 --format="%s")

	git format-patch -1 HEAD --stdout >"$patch_dir/tmp.patch"
	rm -rf .git

	cd "$orig_dir"
	unset orig_dir
else
	if diff -ruN "$src_dir" "$local_dir" >/dev/null 2>&1; then
		echo "-- No differences found between local and source"
		rm -rf "$TMP"
		exit 0
	fi

	printf -- "-- Enter a patch description: "
	if ! read -r description; then
		echo "" >&2
		echo "-- Patch cancelled" >&2
		rm -rf "$TMP"
		exit 1
	fi

	mkdir -p "$TMP/patchdir/a" "$TMP/patchdir/b"
	cp -r "$src_dir/." "$TMP/patchdir/a/"
	cp -r "$local_dir/." "$TMP/patchdir/b/"
	cd "$TMP/patchdir"
	{
		echo "Subject: [PATCH] $description" | fold -s -w 80
		echo "---"
		diff -ruN a b || true
	} >"$patch_dir/tmp.patch"

	cd "$orig_dir"
	unset orig_dir
fi

rm -rf "$TMP"

if [ ! -s "$patch_dir/tmp.patch" ]; then
	rm -f "$patch_dir/tmp.patch"
	echo "-- No differences found between local and source for $PACKAGE"
	exit 0
fi

name_part=$(printf "%s" "$description" | sed 's/^[0-9]\{4\}-//' | sed 's/ /-/g; s/[^a-zA-Z0-9-]//g; s/--*/-/g; s/^-//; s/-$//')
[ -n "$name_part" ] || name_part="patch"

# max is 60 chars, minus 5 for number, minus 6 for patch suffix
max_name_len=49
name_part=$(printf "%s" "$name_part" | cut -c1-"$max_name_len" | sed 's/-$//')

patch_name="${next_num}-${name_part}.patch"

mv "$patch_dir/tmp.patch" "$patch_dir/$patch_name"

NEW_JSON=$(echo "$JSON" | jq ".patches += [\"$patch_name\"]")
"$SCRIPTS"/util/replace.sh "$PACKAGE" "$NEW_JSON"

echo "-- Patch created at $patch_dir/$patch_name"
