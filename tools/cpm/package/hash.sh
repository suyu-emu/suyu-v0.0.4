#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
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

[ "$ALL" = 1 ] && packages="${LIBS:-$packages}"
[ "$DRY" = 1 ] && UPDATE=false || UPDATE=true
[ -z "$packages" ] && usage

export UPDATE

for pkg in $packages; do
	echo "-- Package $pkg"
	"$SCRIPTS"/util/fix-hash.sh "$pkg" || RETURN=1
done

exit $RETURN
