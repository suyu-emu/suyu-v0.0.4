#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# env vars:
# - UPDATE: update if available
# - FORCE: forcefully update

# shellcheck disable=SC1091
. tools/cpm/common.sh

RETURN=0

filter() {
	TAGS=$(echo "$TAGS" | jq "[.[] | select(.name | test(\"$1\"; \"i\") | not)]") # vulkan
}

for PACKAGE in "$@"
do
	export PACKAGE
	# shellcheck disable=SC1091
	. tools/cpm/package.sh

    SKIP=$(value "skip_updates")

    [ "$SKIP" = "true" ] && continue

    [ "$REPO" = null ] && continue
    [ "$GIT_HOST" != "github.com" ] && continue # TODO
    # shellcheck disable=SC2153
    [ "$TAG" = null ] && continue

    echo "-- Package $PACKAGE"

	# TODO(crueter): Support for Forgejo updates w/ forgejo_token
    # Use gh-cli to avoid ratelimits lmao
    TAGS=$(gh api --method GET "/repos/$REPO/tags")

    # filter out some commonly known annoyances
	# TODO add more

    filter vulkan-sdk # vulkan
    filter yotta # mbedtls

    # ignore betas/alphas (remove if needed)
	filter alpha
	filter beta
	filter rc

	# Add package-specific overrides here, e.g. here for fmt:
    [ "$PACKAGE" = fmt ] && filter v0.11

    LATEST=$(echo "$TAGS" | jq -r '.[0].name')

    [ "$LATEST" = "$TAG" ] && [ "$FORCE" != "true" ] && echo "-- * Up-to-date" && continue

	RETURN=1

    if [ "$HAS_REPLACE" = "true" ]; then
        # this just extracts the tag prefix
        VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

        # then we strip out the prefix from the new tag, and make that our new git_version
        NEW_GIT_VERSION=$(echo "$LATEST" | sed "s/$VERSION_PREFIX//g")
    fi

    echo "-- * Version $LATEST available, current is $TAG"

    export USE_TAG=true
    HASH=$(tools/cpm/hash.sh "$REPO" "$LATEST")

    echo "-- * New hash: $HASH"

    if [ "$UPDATE" = "true" ]; then
		RETURN=0

        if [ "$HAS_REPLACE" = "true" ]; then
            NEW_JSON=$(echo "$JSON" | jq ".hash = \"$HASH\" | .git_version = \"$NEW_GIT_VERSION\"")
        else
            NEW_JSON=$(echo "$JSON" | jq ".hash = \"$HASH\" | .tag = \"$LATEST\"")
        fi

		export NEW_JSON

		tools/cpm/replace.sh
    fi
done

exit $RETURN