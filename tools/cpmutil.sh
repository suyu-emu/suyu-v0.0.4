#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/cpm" && pwd)
SCRIPTS="$ROOTDIR"

. "$SCRIPTS"/common.sh

RETURN=0

die() {
	echo "-- $*" >&2
	exit 1
}

usage() {
	cat <<EOF
Usage: $0 [command]

General command-line utility for CPMUtil operations.

Commands:
    package Run operations on a package or packages
    format  Format all cpmfiles
    update  Update CPMUtil and its tooling
    ls      List all cpmfiles
    migrate Convert submodules to a basic cpmfile

Package commands:
    hash    	Verify the hash of a package, and update it if needed
    update  	Check for updates for a package
    fetch   	Fetch a package and place it in the cache
    add     	Add a new package
    rm      	Remove a package
    version 	Change the version of a package
    which   	Find which cpmfile a package is defined in
    download 	Get the download URL for a package

EOF

	exit $RETURN
}

export ROOTDIR

while :; do
	case "$1" in
	-h | --help) usage ;;
	ls)
		echo "$CPMFILES" | tr ' ' '\n'
		break
		;;
	format)
		"$SCRIPTS"/format.sh
		break
		;;
	update)
		"$SCRIPTS"/update.sh
		break
		;;
	migrate)
		"$SCRIPTS"/migrate.sh
		break
		;;
	package)
		shift
		"$SCRIPTS"/package.sh "$@"
		break
		;;
	*) usage ;;
	esac

	shift
done
