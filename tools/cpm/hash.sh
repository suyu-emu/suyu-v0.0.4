#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# usage: hash.sh repo tag-or-sha
# env vars: GIT_HOST, USE_TAG (use tag instead of sha), ARTIFACT (download artifact with that name instead of src archive)

REPO="$1"
[ -z "$GIT_HOST" ] && GIT_HOST=github.com
GIT_URL="https://$GIT_HOST/$REPO"

if [ "$USE_TAG" = "true" ]; then
    if [ -z "$ARTIFACT" ] || [ "$ARTIFACT" = "null" ]; then
        URL="${GIT_URL}/archive/refs/tags/$2.tar.gz"
    else
        URL="${GIT_URL}/releases/download/$2/${ARTIFACT}"
    fi
else
    URL="${GIT_URL}/archive/$2.zip"
fi

SUM=$(wget -q "$URL" -O - | sha512sum)

echo "$SUM" | cut -d " " -f1
