#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

value() {
	echo "$JSON" | jq -r ".$1"
}

if [ -z "$JSON" ]; then
	[ -n "$PACKAGE" ] || { echo "Package was not specified" && exit 0; }

	# shellcheck disable=SC2153
	JSON=$(jq -r ".\"$PACKAGE\" | select( . != null )" cpmfile.json)

	if [ -z "$JSON" ]; then
		echo "!! No cpmfile definition for $PACKAGE" >&2
		exit 1
	fi
fi

# unset stuff
export PACKAGE_NAME="null"
export REPO="null"
export CI="null"
export GIT_HOST="null"
export EXT="null"
export NAME="null"
export DISABLED="null"
export TAG="null"
export ARTIFACT="null"
export SHA="null"
export VERSION="null"
export MIN_VERSION="null"
export DOWNLOAD="null"
export URL="null"
export KEY="null"
export HASH="null"
export ORIGINAL_TAG="null"
export HAS_REPLACE="null"
export VERSION_REPLACE="null"

########
# Meta #
########

REPO=$(value "repo")
CI=$(value "ci")

PACKAGE_NAME=$(value "package")
[ "$PACKAGE_NAME" != null ] || PACKAGE_NAME="$PACKAGE"

GIT_HOST=$(value "git_host")
[ "$GIT_HOST" != null ] || GIT_HOST=github.com

# used for cache key
LOWER_PACKAGE=$(echo "$PACKAGE_NAME" | tr '[:upper:]' '[:lower:]')

export PACKAGE_NAME
export LOWER_PACKAGE
export REPO
export CI
export GIT_HOST

######################
# CI Package Parsing #
######################

MIN_VERSION=$(value "min_version")
VERSION=$(value "version")

export VERSION
export MIN_VERSION

if [ "$CI" = "true" ]; then
	EXT=$(value "extension")
	[ "$EXT" != null ] || EXT="tar.zst"

	NAME=$(value "name")
	DISABLED=$(echo "$JSON" | jq -j '.disabled_platforms')

	[ "$NAME" != null ] || NAME="$PACKAGE_NAME"

	export EXT
	export NAME
	export DISABLED

	return 0
fi

##############
# Versioning #
##############

TAG=$(value "tag")
ARTIFACT=$(value "artifact")
SHA=$(value "sha")

if echo "$TAG" | grep -e "%VERSION%" >/dev/null; then
	HAS_REPLACE=true
else
	HAS_REPLACE=false
fi

ORIGINAL_TAG="$TAG"

TAG=$(echo "$TAG" | sed "s/%VERSION%/$VERSION/g")
ARTIFACT=$(echo "$ARTIFACT" | sed "s/%VERSION%/$VERSION/g")
ARTIFACT=$(echo "$ARTIFACT" | sed "s/%TAG%/$TAG/g")

export TAG
export ARTIFACT
export SHA
export VERSION
export ORIGINAL_TAG
export HAS_REPLACE

###############
# URL Parsing #
###############

URL=$(value "url")

. "$SCRIPTS"/util/url.sh

export DOWNLOAD

###############
# Key Parsing #
###############

if [ "$SHA" != null ]; then
	KEY=$(echo "$SHA" | cut -c1-4)
elif [ "$VERSION" != null ]; then
	KEY="$VERSION"
elif [ "$TAG" != null ]; then
	KEY="$TAG"
else
	echo "!! No valid key could be determined for $PACKAGE_NAME. Must define one of: sha, tag, version"
	exit 1
fi

export KEY

################
# Hash Parsing #
################

HASH=$(value "hash")

if [ "$HASH" = null ]; then
	echo "!! No hash defined for $PACKAGE_NAME" >&2
fi

export HASH
export JSON
