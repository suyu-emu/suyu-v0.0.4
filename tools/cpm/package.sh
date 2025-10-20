#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# shellcheck disable=SC1091
. tools/cpm/common.sh

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

echo "$TAG" | grep -e "%VERSION%" > /dev/null && HAS_REPLACE=true || HAS_REPLACE=false
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

if [ "$URL" != "null" ]; then
	DOWNLOAD="$URL"
elif [ "$REPO" != "null" ]; then
	GIT_URL="https://$GIT_HOST/$REPO"

	BRANCH=$(value "branch")

	if [ "$TAG" != "null" ]; then
		if [ "$ARTIFACT" != "null" ]; then
			DOWNLOAD="${GIT_URL}/releases/download/${TAG}/${ARTIFACT}"
		else
			DOWNLOAD="${GIT_URL}/archive/refs/tags/${TAG}.tar.gz"
		fi
	elif [ "$SHA" != "null" ]; then
		DOWNLOAD="${GIT_URL}/archive/${SHA}.tar.gz"
	else
		if [ "$BRANCH" = null ]; then
			BRANCH=master
		fi

		DOWNLOAD="${GIT_URL}/archive/refs/heads/${BRANCH}.tar.gz"
	fi
else
	echo "!! No repo or URL defined for $PACKAGE"
	exit 1
fi

export DOWNLOAD
export URL

###############
# Key Parsing #
###############

KEY=$(value "key")

if [ "$KEY" = null ]; then
	if [ "$SHA" != null ]; then
		KEY=$(echo "$SHA" | cut -c1-4)
	elif [ "$GIT_VERSION" != null ]; then
		KEY="$GIT_VERSION"
	elif [ "$TAG" != null ]; then
		KEY="$TAG"
	elif [ "$VERSION" != null ]; then
		KEY="$VERSION"
	else
		echo "!! No valid key could be determined for $PACKAGE. Must define one of: key, sha, tag, version, git_version"
		exit 1
	fi
fi

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