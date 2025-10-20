#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# shellcheck disable=SC1091
. tools/cpm/common.sh

RETURN=0

usage() {
	cat << EOF
Usage: $0 [uf] [PACKAGE]...
Check the hash of a specific package or packages.
If a hash mismatch occurs, this script will print the corrected hash of the package.

Options:
    -u, --update	Correct the package's hash if it's a mismatch

    -f, --force 	Update the package's hash anyways (implies -u)

Note that this procedure will usually take a long time
depending on the number and size of dependencies.

This project has defined the following as valid cpmfiles:
EOF

	for file in $CPMFILES; do
		echo "- $file"
	done

	exit $RETURN
}

while true; do
	case "$1" in
		(-uf|-f|--force) UPDATE=true; FORCE=true; shift; continue ;;
		(-u|--update) UPDATE=true; shift; continue ;;
		(-h) usage ;;
		("$0") break ;;
		("") break ;;
	esac

	PACKAGE="$1"

	shift

	export PACKAGE
	. tools/cpm/package.sh

    if [ "$CI" != null ]; then
        continue
    fi

    [ "$HASH_URL" != null ] && continue
    [ "$HASH_SUFFIX" != null ] && continue

    echo "-- Package $PACKAGE"

    [ "$HASH" = null ] && echo "-- * Warning: no hash specified" && continue

    export USE_TAG=true
    ACTUAL=$(tools/cpm/url-hash.sh "$DOWNLOAD")

    if [ "$ACTUAL" != "$HASH" ]; then
		echo "-- * Expected $HASH"
		echo "-- * Got      $ACTUAL"
		[ "$UPDATE" != "true" ] && RETURN=1
	fi

    if { [ "$UPDATE" = "true" ] && [ "$ACTUAL" != "$HASH" ]; } || [ "$FORCE" = "true" ]; then
        NEW_JSON=$(echo "$JSON" | jq ".hash = \"$ACTUAL\"")
		export NEW_JSON

		tools/cpm/replace.sh
    fi
done

exit $RETURN