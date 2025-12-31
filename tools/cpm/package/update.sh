#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

filter_out() {
	TAGS=$(echo "$TAGS" | jq "[.[] | select(.name | test(\"$1\"; \"i\") | not)]")
}

filter_in() {
	TAGS=$(echo "$TAGS" | jq "[.[] | select(.name | test(\"$1\"; \"i\"))]")
}

usage() {
	cat <<EOF
Usage: cpmutil.sh package update [-n|--dry-run] [-a|--all] [PACKAGE]...

Check a specific package or packages for updates.

Options:
    -n, --dry-run 	Do not update the package if it has an update available
    -a, --all       Operate on all packages in this project.

EOF

	exit 0
}

while :; do
	case "$1" in
	-[a-z]*)
		opt=$(printf '%s' "$1" | sed 's/^-//')
		while [ -n "$opt" ]; do
			# cut out first char from the optstring
			char=$(echo "$opt" | cut -c1)
			opt=$(echo "$opt" | cut -c2-)

			case "$char" in
			a) ALL=1 ;;
			n) DRY=1 ;;
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done
		;;
	--dry-run) DRY=1 ;;
	--all) ALL=1 ;;
	--help) usage ;;
	"$0") break ;;
	"") break ;;
	*) packages="$packages $1" ;;
	esac

	shift
done

[ "$ALL" = 1 ] && packages="${LIBS:-$packages}"
[ "$DRY" = 1 ] && UPDATE=false || UPDATE=true
[ -z "$packages" ] && usage

for pkg in $packages; do
	PACKAGE="$pkg"
	export PACKAGE
	# shellcheck disable=SC1091
	. "$SCRIPTS"/vars.sh

	SKIP=$(value "skip_updates")

	[ "$SKIP" = "true" ] && continue

	[ "$REPO" = null ] && continue
	[ "$GIT_HOST" != "github.com" ] && continue # TODO

	[ "$CI" = "true" ] && continue

	# shellcheck disable=SC2153
	[ "$TAG" = null ] && continue

	echo "-- Package $PACKAGE"

	# TODO(crueter): Support for Forgejo updates w/ forgejo_token
	# Use gh-cli to avoid ratelimits lmao
	TAGS=$(gh api --method GET "/repos/$REPO/tags")

	# filter out some commonly known annoyances
	# TODO add more

	if [ "$PACKAGE" = "vulkan-validation-layers" ]; then
		filter_in vulkan-sdk
	else
		filter_out vulkan-sdk
	fi

	filter_out yotta # mbedtls

	# ignore betas/alphas (remove if needed)
	filter_out alpha
	filter_out beta
	filter_out rc

	# Add package-specific overrides here, e.g. here for fmt:
	[ "$PACKAGE" = fmt ] && filter_out v0.11

	LATEST=$(echo "$TAGS" | jq -r '.[0].name')

	[ "$LATEST" = "null" ] && echo "-- * Up-to-date" && continue
	[ "$LATEST" = "$TAG" ] && [ "$FORCE" != "true" ] && echo "-- * Up-to-date" && continue

	if [ "$HAS_REPLACE" = "true" ]; then
		# this just extracts the tag prefix
		VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

		# then we strip out the prefix from the new tag, and make that our new git_version
		if [ -z "$VERSION_PREFIX" ]; then
			NEW_GIT_VERSION="$LATEST"
		else
			NEW_GIT_VERSION=$(echo "$LATEST" | sed "s/$VERSION_PREFIX//g")
		fi
	fi

	echo "-- * Version $LATEST available, current is $TAG"

	if [ "$UPDATE" = "true" ]; then
		if [ "$HAS_REPLACE" = "true" ]; then
			NEW_JSON=$(echo "$JSON" | jq ".git_version = \"$NEW_GIT_VERSION\"")
		else
			NEW_JSON=$(echo "$JSON" | jq ".tag = \"$LATEST\"")
		fi

		"$SCRIPTS"/util/replace.sh "$PACKAGE" "$NEW_JSON"

		QUIET=true "$SCRIPTS"/util/fix-hash.sh
	fi
done
