#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# shellcheck disable=SC1091

value() {
	echo "$JSON" | jq -r ".$1"
}

[ -z "$PACKAGE" ] && echo "Package was not specified" && exit 0

# shellcheck disable=SC2153
JSON=$(echo "$PACKAGES" | jq -r ".\"$PACKAGE\" | select( . != null )")

[ -z "$JSON" ] && echo "!! No cpmfile definition for $PACKAGE" >&2 && exit 1

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
export GIT_VERSION="null"
export DOWNLOAD="null"
export URL="null"
export KEY="null"
export HASH="null"
export ORIGINAL_TAG="null"
export HAS_REPLACE="null"
export VERSION_REPLACE="null"
export HASH_URL="null"
export HASH_SUFFIX="null"
export HASH_ALGO="null"

########
# Meta #
########

REPO=$(value "repo")
CI=$(value "ci")

PACKAGE_NAME=$(value "package")
[ "$PACKAGE_NAME" = null ] && PACKAGE_NAME="$PACKAGE"

GIT_HOST=$(value "git_host")
[ "$GIT_HOST" = null ] && GIT_HOST=github.com

export PACKAGE_NAME
export REPO
export CI
export GIT_HOST

######################
# CI Package Parsing #
######################

VERSION=$(value "version")

if [ "$CI" = "true" ]; then
	EXT=$(value "extension")
	[ "$EXT" = null ] && EXT="tar.zst"

	NAME=$(value "name")
	DISABLED=$(echo "$JSON" | jq -j '.disabled_platforms')

	[ "$NAME" = null ] && NAME="$PACKAGE_NAME"

	export EXT
	export NAME
	export DISABLED
	export VERSION

	return 0
fi

##############
# Versioning #
##############

TAG=$(value "tag")
ARTIFACT=$(value "artifact")
SHA=$(value "sha")
GIT_VERSION=$(value "git_version")

[ "$GIT_VERSION" = null ] && GIT_VERSION="$VERSION"

if [ "$GIT_VERSION" != null ]; then
	VERSION_REPLACE="$GIT_VERSION"
else
	VERSION_REPLACE="$VERSION"
fi

echo "$TAG" | grep -e "%VERSION%" >/dev/null &&
	HAS_REPLACE=true || HAS_REPLACE=false

ORIGINAL_TAG="$TAG"

TAG=$(echo "$TAG" | sed "s/%VERSION%/$VERSION_REPLACE/g")
ARTIFACT=$(echo "$ARTIFACT" | sed "s/%VERSION%/$VERSION_REPLACE/g")
ARTIFACT=$(echo "$ARTIFACT" | sed "s/%TAG%/$TAG/g")

export TAG
export ARTIFACT
export SHA
export VERSION
export GIT_VERSION
export ORIGINAL_TAG
export HAS_REPLACE
export VERSION_REPLACE

###############
# URL Parsing #
###############

URL=$(value "url")
BRANCH=$(value "branch")

export BRANCH
export URL

. "$SCRIPTS"/vars/url.sh

export DOWNLOAD

###############
# Key Parsing #
###############

KEY=$(value "key")

. "$SCRIPTS"/vars/key.sh

export KEY

################
# Hash Parsing #
################

HASH_ALGO=$(value "hash_algo")
[ "$HASH_ALGO" = null ] && HASH_ALGO=sha512

HASH=$(value "hash")

if [ "$HASH" = null ]; then
	HASH_SUFFIX="${HASH_ALGO}sum"
	HASH_URL=$(value "hash_url")

	if [ "$HASH_URL" = null ]; then
		HASH_URL="${DOWNLOAD}.${HASH_SUFFIX}"
	fi

	HASH=$(curl "$HASH_URL" -Ss -L -o -)
else
	HASH_URL=null
	HASH_SUFFIX=null
fi

export HASH_URL
export HASH_SUFFIX
export HASH
export HASH_ALGO
export JSON
