#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

SUBMODULES="$(git submodule status --recursive | cut -c2-)"

[ -z "$SUBMODULES" ] && echo "No submodules defined!" && exit 0

tmp=$(mktemp)
printf '{}' >"$tmp"

IFS="
"
for i in $SUBMODULES; do
	sha=$(echo "$i" | cut -d" " -f1 | cut -c1-10)
	ver=$(echo "$i" | cut -d" " -f3 | tr -d '()')

	path=$(echo "$i" | cut -d" " -f2)
	name=$(echo "$path" | awk -F/ '{print $NF}')

	remote=$(git -C "$path" remote get-url origin)

	host=$(echo "$remote" | cut -d"/" -f3)
	[ "$host" = github.com ] && host=

	repo=$(echo "$remote" | cut -d"/" -f4-5 | cut -d'.' -f1)

	entry=$(jq -n --arg name "$name" \
		--arg sha "$sha" \
		--arg ver "$ver" \
		--arg repo "$repo" \
		--arg host "$host" \
		'{
        ($name): {
            sha: $sha,
            git_version: $ver,
            repo: $repo
        } + (if $host != "" then {git_host: $host} else {} end)
        }')

	jq --argjson new "$entry" '. + $new' "$tmp" >"${tmp}.new"
	mv "$tmp.new" "$tmp"
done

jq '.' "$tmp" >cpmfile.json
rm -f "$tmp"
