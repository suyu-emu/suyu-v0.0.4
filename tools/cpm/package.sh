#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package [command]

Operate on a package or packages.

Commands:
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

SCRIPTS=$(CDPATH='' cd -- "$(dirname -- "$0")/package" && pwd)
export SCRIPTS

while :; do
	case "$1" in
	hash | update | fetch | add | rm | version | which | download)
		cmd="$1"
		shift
		"$SCRIPTS/$cmd".sh "$@"
		break
		;;
	*) usage ;;
	esac

	shift
done
