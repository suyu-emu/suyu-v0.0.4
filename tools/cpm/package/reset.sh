#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091
. "$SCRIPTS"/../common.sh

usage() {
	cat <<EOF
Usage: cpmutil.sh package reset [a|--all] [PACKAGE]...

Reset a locally fetched package to its original state.
This is most useful for dropping any changes you've made.

EOF

	exit 0
}

while :; do
	case "$1" in
	-h | --help) usage ;;
	-a | --all) ALL=1 ;;
	"$0") break ;;
	"") break ;;
	*) packages="$packages $1" ;;
	esac

	shift
done

[ "$ALL" != 1 ] || packages="${LIBS:-$packages}"
[ -n "$packages" ] || usage

for PACKAGE in $packages; do
	unset JSON
	export PACKAGE

	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh

	if [ "$CI" = true ]; then
		dir="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}"
	else
		dir="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
	fi

	echo "-- Removing $dir"
	rm -rf "$dir"

	# shellcheck disable=SC1091
	. "$SCRIPTS"/util/fetch.sh

	fetch_package
done
