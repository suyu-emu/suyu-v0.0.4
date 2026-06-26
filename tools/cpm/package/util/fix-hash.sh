#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

: "${PACKAGE:=$1}"

# re-read json files
# shellcheck disable=SC2016
. "$SCRIPTS"/vars.sh

[ "$CI" = null ] || exit 0

ACTUAL=$("$SCRIPTS"/util/url-hash.sh "$DOWNLOAD")

if [ "$ACTUAL" != "$HASH" ] && [ "$QUIET" != true ]; then
	echo "-- * Expected $HASH"
	echo "-- * Got      $ACTUAL"
	[ "$UPDATE" != "true" ] && exit 1
fi

if [ "$UPDATE" = "true" ] && [ "$ACTUAL" != "$HASH" ]; then
	NEW_JSON=$(echo "$JSON" | jq ".hash = \"$ACTUAL\"")

	"$SCRIPTS"/util/replace.sh "$PACKAGE" "$NEW_JSON"
fi
