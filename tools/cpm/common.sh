#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

: "${CPM_SOURCE_CACHE:=$PWD/.cache/cpm}"
: "${CPMUTIL_PATCH_DIR:=$PWD/.patch}"

# TODO: cache cpmfile defs?

cmd_exists() {
	command -v "$1" >/dev/null 2>&1
}

must_install() {
	for cmd in "$@"; do
		cmd_exists "$cmd" || { echo "-- $cmd must be installed" && exit 1; }
	done
}

# Random integer between 100000 and 999999
_randint() {
	awk 'BEGIN { srand(); print int(100000 + rand() * 900000) }'
}

# Use mktemp if available, use a local temp dir otherwise
make_temp_dir() {
	if cmd_exists mktemp; then
		mktemp -d
	else
		TMP="$PWD/.cpm/tmp-$(_randint)"
		mkdir -p "$TMP"
		echo "$TMP"
	fi
}

# must_install jq find mktemp tar 7z unzip sha512sum git patch curl

if [ ! -s cpmfile.json ]; then
	# TODO: actually make it a no-op
	echo "-- Warning: cpmfile.json does not exist or is empty, most commands will be no-ops"
else
	LIBS=$(jq -j 'keys_unsorted | join(" ")' cpmfile.json)
	export LIBS
fi
