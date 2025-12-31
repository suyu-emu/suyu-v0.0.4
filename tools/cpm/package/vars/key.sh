#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

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
		echo "!! No valid key could be determined for $PACKAGE_NAME. Must define one of: key, sha, tag, version, git_version"
		exit 1
	fi
fi

export KEY
