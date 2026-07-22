#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

RETURN=0

usage() {
	cat <<EOF
Usage: cpmutil.sh package add [package name]

Add a new package to a cpmfile.

Note that you are still responsible for integrating this into your CMake.

EOF

	exit "$RETURN"
}

die() {
	echo "-- $*" >&2
	RETURN=1 usage
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
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done
		;;
	--help) usage ;;
	"$0") break ;;
	"") break ;;
	*) PKG="$1" ;;
	esac

	shift
done

[ -n "$PKG" ] || die "You must specify a package name."

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
		[ -z "$2" ] || printf " (? for help, %s)" "$3"
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

required "Version of the bundled package" \
	"The tag or commit hash of the bundled package."

VERSION="$reply"

optional "Package name for find_package" \
	"When searching for system packages, this argument will be passed to find_package.
For example, using \"Boost\" here will result in CPMUtil internally calling find_package(Boost).
If unset, defaults to the JSON key."

PACKAGE="$reply"

optional "Minimum required version" \
	"The minimum required version for this package if it's pulled in by the system."

MIN_VERSION="$reply"

optional "Additional find_package arguments, space-separated" \
	"Extra arguments passed to find_package(), (e.g. CONFIG)"

FIND_ARGS="$reply"

optional "Git host (default: github.com)" \
	"The hostname of the Git server, if not GitHub (e.g. codeberg.org, git.crueter.xyz)"

GIT_HOST="$reply"

optional "Is this a CI package? [y/N]" \
	"Yes if the package is a prebuilt binary distribution (e.g. crueter-ci),
no if the package is built from source if it's bundled."

case "$reply" in
[Yy]*) CI=true ;;
*) CI=false ;;
esac

if [ "$CI" = "false" ]; then
	optional "Name of the release artifact to download, if applicable.
-- %VERSION% is replaced by the version ($VERSION)" \
		"Download the specified artifact from the release with the previously specified tag."

	ARTIFACT="$reply"

	multi "Fixed options, one per line (e.g. OPUS_BUILD_TESTING OFF)" \
		"Fixed options passed to the project's CMakeLists.txt. Variadic options
should be set in CMake with AddJsonPackage's OPTIONS parameter."

	OPTIONS="$reply"
else
	required "Name of the CI artifact" \
		"CI artifacts are stored as <name>-<platform>-<version>.tar.zst. This option controls the name."

	ARTIFACT="$reply"

	multi "Platforms without a package (one per line)" \
		"Valid platforms:
windows-amd64 windows-arm64
mingw-amd64 mingw-arm64
android-aarch64 android-x86_64
linux-amd64 linux-aarch64
macos-universal ios-aarch64"

	DISABLED_PLATFORMS="$reply"
fi

# invoke add.cmake for validation
set -- "$@" \
	-DKEY="$PKG" \
	-DREPO="$REPO" \
	-DVERSION="$VERSION" \
	-DCI="$CI"

[ -z "$PACKAGE" ] || set -- "$@" -DPACKAGE="$PACKAGE"
[ -z "$GIT_HOST" ] || set -- "$@" -DGIT_HOST="$GIT_HOST"
[ -z "$MIN_VERSION" ] || set -- "$@" -DMIN_VERSION="$MIN_VERSION"
[ -z "$FIND_ARGS" ] || set -- "$@" -DFIND_ARGS="$FIND_ARGS"
[ -z "$OPTIONS" ] || set -- "$@" -DOPTIONS="$(echo "$OPTIONS" | tr '\n' ';')"
[ -z "$ARTIFACT" ] || set -- "$@" -DARTIFACT="$ARTIFACT"
[ -z "$DISABLED_PLATFORMS" ] || set -- "$@" -DDISABLED_PLATFORMS="$(echo "$DISABLED_PLATFORMS" | tr '\n' ';')"

cmake "$@" -P "$CMAKE"/add.cmake

echo "Added package $PKG to cpmfile.json. Include it in your project with AddJsonPackage($PKG)"
