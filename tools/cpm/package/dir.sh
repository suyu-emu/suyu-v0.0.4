#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091
. "$SCRIPTS"/../common.sh

usage() {
	cat <<EOF
Usage: cpmutil.sh package dir [-a|--all] [PACKAGE]...

Get the local directory for the specified packages.

Options:
    -a, --all       Operate on all packages in this project.

EOF

	exit 0
}

while :; do
	case "$1" in
	-a | --all) ALL=1 ;;
	-h | --help) usage ;;
	"$0") break ;;
	"") break ;;
	*) packages="$packages $1" ;;
	esac

	shift
done

[ "$ALL" != 1 ] || packages="${LIBS:-$packages}"
[ -n "$packages" ] || usage

for pkg in $packages; do
	unset JSON
	export PACKAGE="$pkg"

	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh

	# TODO: common get dir func
	if [ "$CI" = true ]; then
		dir="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}"
	else
		dir="${CPM_SOURCE_CACHE}/${LOWER_PACKAGE}/${KEY}"
	fi

	echo "-- $pkg: $dir"

	if [ ! -d "$dir" ]; then
		echo "-- * Warning: directory does not exist. Use fetch or reset to create it"
	fi
done
