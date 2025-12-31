#!/bin/sh

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# Required vars: URL, GIT_HOST, REPO, TAG, ARTIFACT, BRANCH, SHA
if [ "$URL" != "null" ]; then
	DOWNLOAD="$URL"
elif [ "$REPO" != "null" ]; then
	GIT_URL="https://$GIT_HOST/$REPO"

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
	echo "!! No repo or URL defined for $PACKAGE_NAME"
	exit 1
fi

export DOWNLOAD
