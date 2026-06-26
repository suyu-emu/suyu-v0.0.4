#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package add [package name]

Add a new package to a cpmfile.

Note that you are still responsible for integrating this into your CMake.

EOF

	exit "$RETURN"
}

die() {
	echo "-- $*" >&2
	RETURN=1 usage
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
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done
		;;
	--help) usage ;;
	"$0") break ;;
	"") break ;;
	*) PKG="$1" ;;
	esac

	shift
done

[ -n "$PKG" ] || die "You must specify a package name."

export PKG

"$SCRIPTS"/util/interactive.sh
