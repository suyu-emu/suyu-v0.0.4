#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

usage() {
	cat <<EOF
Usage: cpmutil.sh package download [-a|--all] [PACKAGE]...

Get the download URL for the specified packages.

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

[ "$ALL" = 1 ] && packages="${LIBS:-$packages}"
[ -z "$packages" ] && usage

for pkg in $packages; do
	PACKAGE="$pkg"
	export PACKAGE
	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh

	if [ "$CI" = "true" ]; then
		echo "-- $PACKAGE: https://$GIT_HOST/$REPO"
	else
		echo -- "$PACKAGE: $DOWNLOAD"
	fi
done
