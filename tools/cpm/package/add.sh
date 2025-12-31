#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package add [-s|--sha] [-t|--tag]
           [-c|--cpmfile CPMFILE] [package name]

Add a new package to a cpmfile.

Options:
    -t, --tag            	Use tag versioning, instead of the default,
                           	commit sha versioning.
    -c, --cpmfile <CPMFILE>	Use the specified cpmfile instead of the root cpmfile

Note that you are still responsible for integrating this into your CMake.

EOF

	exit $RETURN
}

die() {
	echo "-- $*" >&2
	exit 1
}

_cpmfile() {
	[ -z "$1" ] && die "You must specify a valid cpmfile."
	CPMFILE="$1"
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
			t) TAG=1 ;;
			c)
				_cpmfile "$2"
				shift
				;;
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done
		;;
	--tag) TAG=1 ;;
	--cpmfile)
		_cpmfile "$2"
		shift
		;;
	--help) usage ;;
	"$0") break ;;
	"") break ;;
	*) PKG="$1" ;;
	esac

	shift
done

: "${CPMFILE:=$PWD/cpmfile.json}"

[ -z "$PKG" ] && die "You must specify a package name."

export PKG
export CPMFILE
export TAG

"$SCRIPTS"/util/interactive.sh
