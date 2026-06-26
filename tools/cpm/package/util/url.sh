#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# Get the download URL for a package

if [ "$URL" != "null" ]; then
	DOWNLOAD="$URL"
elif [ "$REPO" != "null" ]; then
	GIT_URL="https://$GIT_HOST/$REPO"

	if [ "$SHA" != "null" ]; then
		DOWNLOAD="${GIT_URL}/archive/${SHA}.tar.gz"
	elif [ "$TAG" != "null" ]; then
		if [ "$ARTIFACT" != "null" ]; then
			DOWNLOAD="${GIT_URL}/releases/download/${TAG}/${ARTIFACT}"
		else
			DOWNLOAD="${GIT_URL}/archive/refs/tags/${TAG}.tar.gz"
		fi
	fi
else
	echo "!! No repo or URL defined for $PACKAGE_NAME"
	exit 1
fi

export DOWNLOAD
