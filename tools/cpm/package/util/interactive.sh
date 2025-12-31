#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# This reads a single-line input from the user and also gives them
# help if needed.
# $1: The prompt itself, without any trailing spaces or whatever
# $2: The help text that gets shown when the user types a question mark
# $3: This is set to "required" if it's necessary,
#     otherwise it can continue without input.
# Stores its output in the "reply" variable
read_single() {
	while :; do
		printf -- "-- %s" "$1"
		[ -n "$2" ] && printf " (? for help, %s)" "$3"
		printf ": "
		if ! IFS= read -r reply; then
			echo
			[ "$3" = "required" ] && continue || reply=""
		fi
		case "$reply" in
		"?") echo "$2" ;;
		"") [ "$3" = "required" ] && continue || return 0 ;;
		*) return 0 ;;
		esac
	done
}

# read_single, but optional
optional() {
	read_single "$1" "$2" "optional"
}

# a
required() {
	read_single "$1" "$2" "required"
}

# Basically the same as the single line function except multiline,
# also it's never "required" so we don't need that handling.
multi() {
	echo "-- $1"
	if [ -n "$2" ]; then
		echo "-- (? on first line for help, Ctrl-D to finish)"
	else
		echo "-- (Ctrl-D to finish)"
	fi
	while :; do
		reply=$(cat)
		if [ "$(echo "$reply" | head -n 1)" = "?" ] && [ -n "$2" ]; then
			echo "$2"
			continue
		fi

		# removes trailing EOF and empty lines
		reply=$(printf '%s\n' "$reply" |
			sed 's/\x04$//' |
			sed '/^[[:space:]]*$/d')

		break
	done
}

# the actual inputs :)

required "Package repository (owner/repo)" \
	"The remote repository this is stored on.
You shouldn't include the host, just owner/repo is enough."

REPO="$reply"

optional "Package name for find_package" \
	"When searching for system packages, this argument will be passed to find_package.
For example, using \"Boost\" here will result in CPMUtil internally calling find_package(Boost)."

PACKAGE="$reply"

optional "Minimum required version" \
	"The minimum required version for this package if it's pulled in by the system."

MIN_VERSION="$reply"

optional "Additional find_package arguments, space-separated" \
	"Extra arguments passed to find_package(), (e.g. CONFIG)"

FIND_ARGS="$reply"

optional "Is this a CI package? [y/N]" \
	"Yes if the package is a prebuilt binary distribution (e.g. crueter-ci),
no if the package is built from source if it's bundled."

case "$reply" in
[Yy]*) CI=true ;;
*) CI=false ;;
esac

if [ "$CI" = "false" ]; then
	optional "Git host (default: github.com)" \
		"The hostname of the Git server, if not GitHub (e.g. codeberg.org, git.crueter.xyz)"

	GIT_HOST="$reply"

	if [ "$TAG" = "1" ]; then
		required "Numeric version of the bundled package" \
			"The semantic version of the bundled package. This is only used for package identification,
and if you use tag/artifact fetching. Do not input the entire tag here; for example, if you're using
tag v1.3.0, then set this to 1.3.0 and set the tag to v%VERSION%."

		GIT_VERSION="$reply"

		optional "Name of the upstream tag. %VERSION% is replaced by the numeric version (default: %VERSION%)" \
			"Most commonly this will be something like v%VERSION% or release-%VERSION%, or just %VERSION%."

		TAGNAME="$reply"
		[ -z "$TAGNAME" ] && TAGNAME="%VERSION%"

		optional "Name of the release artifact to download, if applicable.
-- %VERSION% is replaced by the numeric version and %TAG% is replaced by the tag name" \
			"Download the specified artifact from the release with the previously specified tag.
If unspecified, the source code at the specified tag will be used instead."

		ARTIFACT="$reply"
	else
		required "Commit sha" \
			"The short Git commit sha to use. You're recommended to keep this short, e.g. 10 characters."

		SHA="$reply"
	fi

	multi "Fixed options, one per line (e.g. OPUS_BUILD_TESTING OFF)" \
		"Fixed options passed to the project's CMakeLists.txt. Variadic options
should be set in CMake with AddJsonPackage's OPTIONS parameter."

	OPTIONS="$reply"
else
	required "Version of the CI package (e.g. 3.6.0-9eff87adb1)" \
		"CI artifacts are stored as <name>-<platform>-<version>.tar.zst. This option controls the version."

	VERSION="$reply"

	required "Name of the CI artifact" \
		"CI artifacts are stored as <name>-<platform>-<version>.tar.zst. This option controls the name."

	ARTIFACT="$reply"

	multi "Platforms without a package (one per line)" \
		"Valid platforms:
windows-amd64 windows-arm64
mingw-amd64 mingw-arm64
android-aarch64 android-x86_64
solaris-amd64 freebsd-amd64 openbsd-amd64
linux-amd64 linux-aarch64
macos-universal"

	DISABLED_PLATFORMS="$reply"
fi

# now time to construct the actual json
jq_input='{repo: "'"$REPO"'"}'

# common trivial fields
[ -n "$PACKAGE" ] && jq_input="$jq_input + {package: \"$PACKAGE\"}"
[ -n "$MIN_VERSION" ] && jq_input="$jq_input + {min_version: \"$MIN_VERSION\"}"
[ -n "$FIND_ARGS" ] && jq_input="$jq_input + {find_args: \"$FIND_ARGS\"}"

if [ "$CI" = "true" ]; then
	jq_input="$jq_input + {
        ci: true,
        version: \"$VERSION\",
        artifact: \"$ARTIFACT\"
    }"

	# disabled platforms
	if [ -n "$DISABLED_PLATFORMS" ] && [ -n "$(printf '%s' "$DISABLED_PLATFORMS" | tr -d ' \t\n\r')" ]; then
		disabled_json=$(printf '%s\n' "$DISABLED_PLATFORMS" | jq -R . | jq -s .)
		jq_input="$jq_input + {disabled_platforms: $disabled_json}"
	fi
else
	[ -n "$MIN_VERSION" ] && jq_input="$jq_input + {version: \"$MIN_VERSION\"}"
	jq_input="$jq_input + {hash: \"\"}"

	# options
	if [ -n "$OPTIONS" ] && [ -n "$(printf '%s' "$OPTIONS" | tr -d ' \t\n\r')" ]; then
		options_json=$(printf '%s\n' "$OPTIONS" | jq -R . | jq -s .)
		jq_input="$jq_input + {options: $options_json}"
	fi

	# Git host
	if [ -n "$GIT_HOST" ] && [ "$GIT_HOST" != "github.com" ]; then
		jq_input="$jq_input + {git_host: \"$GIT_HOST\"}"
	fi

	# versioning stuff
	if [ -n "$GIT_VERSION" ]; then
		jq_input="$jq_input + {git_version: \"$GIT_VERSION\"}"
		[ -n "$TAGNAME" ] && jq_input="$jq_input + {tag: \"$TAGNAME\"}"
		[ -n "$ARTIFACT" ] && jq_input="$jq_input + {artifact: \"$ARTIFACT\"}"
	else
		jq_input="$jq_input + {sha: \"$SHA\"}"
	fi
fi

new_json=$(jq -n "$jq_input")

jq --arg key "$PKG" --argjson new "$new_json" \
	'.[$key] = $new' "$CPMFILE" --indent 4 >"${CPMFILE}.tmp" &&
	mv "${CPMFILE}.tmp" "$CPMFILE"

# now correct the hash
if [ "$CI" != true ]; then
	# shellcheck disable=SC1091
	. "$ROOTDIR"/common.sh
	QUIET=true UPDATE=true "$SCRIPTS"/util/fix-hash.sh "$PKG"
fi

echo "Added package $PKG to $CPMFILE. Include it in your project with AddJsonPackage($PKG)"
