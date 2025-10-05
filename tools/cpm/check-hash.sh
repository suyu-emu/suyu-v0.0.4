#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# env vars:
# - UPDATE: fix hashes if needed

# shellcheck disable=SC1091
. tools/cpm/common.sh

RETURN=0

for PACKAGE in "$@"
do
	export PACKAGE
	# shellcheck disable=SC1091
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

    # shellcheck disable=SC2028
    [ "$ACTUAL" != "$HASH" ] && echo "-- * Expected $HASH" && echo "-- * Got      $ACTUAL" && [ "$UPDATE" != "true" ] && RETURN=1

    if [ "$UPDATE" = "true" ] && [ "$ACTUAL" != "$HASH" ]; then
        # shellcheck disable=SC2034
        NEW_JSON=$(echo "$JSON" | jq ".hash = \"$ACTUAL\"")
		export NEW_JSON

		tools/cpm/replace.sh
    fi
done

exit $RETURN