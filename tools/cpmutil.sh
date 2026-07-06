#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/cpm" && pwd)
SCRIPTS="$ROOTDIR"
CMAKE="$ROOTDIR/cmake"

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
    format  Format cpmfile
    ls  	List all packages in the cpmfile
    update  Update CPMUtil and its tooling
    migrate Convert submodules to a basic cpmfile

Package commands:
    hash            	Verify the hash of a package, and update it if needed
    update          	Check for updates for a package
    fetch           	Fetch a package and place it in the cache
    add             	Add a new package
    rm              	Remove a package
    version         	Change the version of a package
    which           	Check if a package is defined
    url             	Get the download URL for a package
    dir             	Get the local directory for a package
    reset           	Reset a fetched package to its original state
    patch           	Create an in-tree patch based on local modifications

EOF

	exit $RETURN
}

export ROOTDIR CMAKE SCRIPTS

while :; do
	case "$1" in
	format | update)
		cmake -P "$CMAKE/$1".cmake
		break
		;;
	ls)
		shift
		print_usage=false
		filtered=""
		for arg in "$@"; do
			case "$arg" in
			-h | --help) print_usage=true ;;
			-n) ;;
			*) filtered="$filtered $arg" ;;
			esac
		done
		cmake_defines=""
		$print_usage && cmake_defines="-DPRINT_USAGE=TRUE"
		# shellcheck disable=SC2086
		cmake $cmake_defines -P "$CMAKE/ls.cmake" -- $filtered
		break
		;;
	migrate)
		"$SCRIPTS"/migrate.sh
		break
		;;
	package)
		cmd="$1"
		shift
		"$SCRIPTS/$cmd".sh "$@"
		break
		;;
	*) usage ;;
	esac

	shift
done
