#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package hash [-n|--dry-run] [-a|--all] [PACKAGE]...

Check the hash of a specific package or packages.
If a hash mismatch occurs, this script will update the package's hash.

Options:
    -n, --dry-run	Don't update the package's hash if it's a mismatch
    -a, --all       Operate on all packages in this project.

Note that this procedure will usually take a long time
depending on the number and size of dependencies.

EOF

	exit $RETURN
}

while :; do
	case "$1" in
	-[a-z]*)
		opt=$(printf '%s' "$1" | sed 's/^-//')
		while [ -n "$opt" ]; do
			# cut out first char from the optstring
			char=$(echo "$opt" | cut -c1)
			opt=$(echo "$opt" | cut -c2-)

			case "$char" in
			a) ALL=1 ;;
			n) DRY=1 ;;
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done
		;;
	--dry-run) DRY=1 ;;
	--all) ALL=1 ;;
	--help) usage ;;
	"$0") break ;;
	"") break ;;
	*) packages="$packages $1" ;;
	esac

	shift
done

[ "$ALL" != 1 ] || packages="${LIBS:-$packages}"
[ "$DRY" = 1 ] && UPDATE=false || UPDATE=true
[ -n "$packages" ] || usage

export UPDATE

for pkg in $packages; do
	unset JSON
	echo "-- Package $pkg"

	export PACKAGE="$pkg"

	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh

	[ "$CI" = null ] || continue

	ACTUAL=$("$SCRIPTS"/util/url-hash.sh "$DOWNLOAD")

	if [ "$ACTUAL" != "$HASH" ] && [ "$QUIET" != true ]; then
		echo "-- * Expected $HASH"
		echo "-- * Got      $ACTUAL"

		if [ "$UPDATE" != "true" ]; then
			RETURN=1
			continue
		fi
	fi

	if [ "$UPDATE" = "true" ] && [ "$ACTUAL" != "$HASH" ]; then
		NEW_JSON=$(echo "$JSON" | jq ".hash = \"$ACTUAL\"")

		"$SCRIPTS"/util/replace.sh "$PACKAGE" "$NEW_JSON"
	fi
done

exit $RETURN
