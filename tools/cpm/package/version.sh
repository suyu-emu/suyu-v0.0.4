#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
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

[ -z "$PACKAGE" ] && usage
[ -z "$NEW_VERSION" ] && usage

export PACKAGE

. "$SCRIPTS"/vars.sh

[ "$REPO" = null ] && exit 0

if [ "$HAS_REPLACE" = "true" ]; then
	# this just extracts the tag prefix
	VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

	# then we strip out the prefix from the new tag, and make that our new git_version
	if [ -z "$VERSION_PREFIX" ]; then
		NEW_GIT_VERSION="$NEW_VERSION"
	else
		NEW_GIT_VERSION=$(echo "$NEW_VERSION" | sed "s/$VERSION_PREFIX//g")
	fi
fi

if [ "$SHA" != null ]; then
	NEW_JSON=$(echo "$JSON" | jq ".sha = \"$NEW_VERSION\"")
elif [ "$CI" = "true" ]; then
	NEW_JSON=$(echo "$JSON" | jq ".version = \"$NEW_VERSION\"")
elif [ "$HAS_REPLACE" = "true" ]; then
	NEW_JSON=$(echo "$JSON" | jq ".git_version = \"$NEW_GIT_VERSION\"")
else
	NEW_JSON=$(echo "$JSON" | jq ".tag = \"$NEW_VERSION\"")
fi

echo "-- * -- Updating $PACKAGE to version $NEW_VERSION"
"$SCRIPTS"/util/replace.sh "$PACKAGE" "$NEW_JSON"

[ "$CI" = "true" ] && exit 0
echo "-- * -- Fixing hash"
. "$ROOTDIR"/common.sh
UPDATE=true QUIET=true "$SCRIPTS"/util/fix-hash.sh
