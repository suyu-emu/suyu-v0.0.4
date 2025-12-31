#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package rm [PACKAGE]...

Delete a package or packages' cpmfile definition(s).

EOF

	exit $RETURN
}

[ $# -lt 1 ] && usage

for pkg in "$@"; do
	JSON=$("$SCRIPTS"/which.sh "$pkg") || {
		echo "!! No cpmfile definition for $pkg"
		continue
	}

	jq --indent 4 "del(.\"$pkg\")" "$JSON" >"$JSON".tmp
	mv "$JSON".tmp "$JSON"
	echo "-- Removed $pkg from $JSON" || :
done
