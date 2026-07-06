#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package [command]

Operate on a package or packages.

Commands:
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
    patch           	Add a patch to a package in the cpmfile

EOF

	exit $RETURN
}

SCRIPTS=$(CDPATH='' cd -- "$(dirname -- "$0")/package" && pwd)
CMAKE="$CMAKE/package"

export SCRIPTS CMAKE

filter_args() {
	print_usage=false
	all_flag=false
	commit_flag=false
	filtered=""

	for arg in "$@"; do
		case "$arg" in
		-h | --help) print_usage=true ;;
		-a | --all) all_flag=true ;;
		-c | --commit) commit_flag=true ;;
		-ac | -ca)
			commit_flag=true
			all_flag=true
			;;
		*) filtered="$filtered $arg" ;;
		esac
	done
}

while :; do
	case "$1" in
	dir | fetch | hash | reset | rm | url | version | which | update)
		cmd="$1"
		shift

		filter_args "$@"
		cmake_defines=""

		if $print_usage; then cmake_defines="$cmake_defines -DPRINT_USAGE=TRUE"; fi
		if $all_flag; then cmake_defines="$cmake_defines -DALL_PACKAGES=TRUE"; fi
		if $commit_flag; then cmake_defines="$cmake_defines -DMAKE_COMMIT=TRUE"; fi

		# shellcheck disable=SC2086
		cmake $cmake_defines -P "$CMAKE/$cmd.cmake" -- $filtered
		break
		;;
	add | patch)
		cmd="$1"
		shift
		"$SCRIPTS/$cmd".sh "$@"
		break
		;;
	*) usage ;;
	esac

	shift
done
