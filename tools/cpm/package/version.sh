#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

usage() {
	cat <<EOF
Usage: cpmutil.sh package version [PACKAGE] [VERSION]

Update a package's version. If the package uses a sha, you must provide a sha,
and if the package uses a tag, you must provide the fully qualified tag.

EOF

	exit 0
}

PACKAGE="$1"
NEW_VERSION="$2"

[ -n "$PACKAGE" ] || usage
[ -n "$NEW_VERSION" ] || usage

export PACKAGE

. "$SCRIPTS"/vars.sh

[ "$REPO" != null ] || exit 0

if [ "$HAS_REPLACE" = "true" ]; then
	# this just extracts the tag prefix
	VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

	# then we strip out the prefix from the new tag, and make that our new git_version
	if [ -n "$VERSION_PREFIX" ]; then
		NEW_VERSION=$(echo "$NEW_VERSION" | sed "s/$VERSION_PREFIX//g")
	fi
fi

if [ "$SHA" != null ]; then
	JSON=$(echo "$JSON" | jq ".sha = \"$NEW_VERSION\"")
elif [ "$CI" = "true" ] || [ "$HAS_REPLACE" = "true" ]; then
	JSON=$(echo "$JSON" | jq ".version = \"$NEW_VERSION\"")
else
	JSON=$(echo "$JSON" | jq ".tag = \"$NEW_VERSION\"")
fi

echo "-- * Updating $PACKAGE to version $NEW_VERSION"

# TODO: ci hash thing please
if [ "$CI" != true ]; then
	echo "-- * -- Updating hash"

	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh
	HASH=$("$SCRIPTS"/util/url-hash.sh "$DOWNLOAD")
	JSON=$(echo "$JSON" | jq ".hash = \"$HASH\"")
fi

"$SCRIPTS"/util/replace.sh "$PACKAGE" "$JSON"
