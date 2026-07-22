#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

printf -- "-- Enter a patch description: "
if ! read -r description; then
	echo "" >&2
	echo "-- Patch cancelled" >&2
	rm -rf "$TMP"
	exit 1
fi

cmake -DDESCRIPTION="$description" -P "$CMAKE"/patch.cmake "$@"
