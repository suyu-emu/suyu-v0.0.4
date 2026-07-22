#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

SUBMODULES="$(git submodule status --recursive | cut -c2-)"
: "${SUBMODULES:?No submodules defined!}"

IFS="
"
for i in $SUBMODULES; do
	commit=$(echo "$i" -d" " -f1 | cut -c1-10)
	short_commit=$(echo "$i" -d" " -f1 | cut -c1-7)

	ref=$(echo "$i" | cut -d" " -f3 | tr -d '()')

	case "$ref" in
	# ref == commit, use commit versioning
	"$short_commit") version="$commit" ;;
	# ref is a branch, use commit versioning
	heads/*) version="$commit" ;;
	# ref is (probably) a tag, use tag versioning
	*) version="$ref" ;;
	esac

	path=$(echo "$i" | cut -d" " -f2)
	name=$(echo "$path" | awk -F/ '{print $NF}')

	remote=$(git -C "$path" remote get-url origin)

	host=$(echo "$remote" | cut -d"/" -f3)

	repo=$(echo "$remote" | cut -d"/" -f4-5 | cut -d'.' -f1)

	cmake -DKEY="$name" -DVERSION="$version" -DREPO="$repo" -DGIT_HOST="$host" \
		-P "$CMAKE/package/add.cmake"
done
