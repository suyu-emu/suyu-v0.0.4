#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package rm [PACKAGE]...

Delete a package or packages' cpmfile definition.

EOF

	exit $RETURN
}

[ $# -ge 1 ] || usage

for pkg in "$@"; do
	"$SCRIPTS"/which.sh "$pkg" || {
		echo "!! No cpmfile definition for $pkg"
		continue
	}

	jq --indent 4 "del(.\"$pkg\")" cpmfile.json >cpmfile.json.new
	mv cpmfile.json.new cpmfile.json
	echo "-- Removed $pkg"
done
