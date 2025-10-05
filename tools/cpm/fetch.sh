#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

[ -z "$CPM_SOURCE_CACHE" ] && CPM_SOURCE_CACHE=$PWD/.cache/cpm

mkdir -p "$CPM_SOURCE_CACHE"

# shellcheck disable=SC1091
. tools/cpm/common.sh

# shellcheck disable=SC1091
. tools/cpm/download.sh

# shellcheck disable=SC2034
ROOTDIR="$PWD"

TMP=$(mktemp -d)

# shellcheck disable=SC2034
for PACKAGE in "$@"
do
	export PACKAGE
	# shellcheck disable=SC1091
	. tools/cpm/package.sh

    if [ "$CI" = "true" ]; then
        ci_package
	else
		echo "-- Downloading regular package $PACKAGE, with key $KEY, from $DOWNLOAD"
	    download_package
    fi
done

rm -rf "$TMP"