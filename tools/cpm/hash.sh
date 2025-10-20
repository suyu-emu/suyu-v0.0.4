#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# usage: hash.sh repo tag-or-sha
# env vars: GIT_HOST, USE_TAG (use tag instead of sha), ARTIFACT (download artifact with that name instead of src archive)

RETURN=0

usage() {
	cat <<EOF
Usage: $0 [-a|--artifact ARTIFACT] [-g|--host GIT_HOST] [REPO] [REF]
Get the hash of a package.

REPO must be in the form of OWNER/REPO, and REF must be a commit sha, branch, or tag.

Options:
	-g, --host <GIT_HOST>  		What Git host to use (defaults to github.com)

	-a, --artifact <ARTIFACT>	The artifact to download (implies -t)
                            	If -t is specified but not -a, fetches a tag archive.
								If ARTIFACT is specified but is null,

EOF
	exit "$RETURN"
}

die() {
	echo "$@" >&2
	RETURN=1 usage
}

artifact() {
	if [ $# -lt 2 ]; then
		die "You must specify a valid artifact."
	fi

	shift

	ARTIFACT="$1"
}

host() {
	if [ $# -lt 2 ]; then
		die "You must specify a valid Git host."
	fi

	shift

	GIT_HOST="$1"
}

# this is a semi-hacky way to handle long/shortforms
while true; do
	case "$1" in
	-[a-z]*)
		opt=$(echo "$1" | sed 's/^-//')
		while [ -n "$opt" ]; do
			# cut out first char from the optstring
			char=$(echo "$opt" | cut -c1)
			opt=$(echo "$opt" | cut -c2-)

			case "$char" in
			a) artifact "$@" ;;
			g) host "$@" ;;
			h) usage ;;
			*) die "Invalid option -$char" ;;
			esac
		done

		;;
	--artifact) artifact "$@" ;;
	--host) host "$@" ;;
	--help) usage ;;
	--*) die "Invalid option $1" ;;
	"$0" | "") break ;;
	*)
		{ [ -z "$REPO" ] && REPO="$1"; } || REF="$1"
		;;
	esac

	shift
done

[ -z "$REPO" ] && die "A valid repository must be provided."
[ -z "$REF" ] && die "A valid reference must be provided."

GIT_HOST=${GIT_HOST:-github.com}
GIT_URL="https://$GIT_HOST/$REPO"

if [ -z "$ARTIFACT" ] || [ "$ARTIFACT" = "null" ]; then
	URL="${GIT_URL}/archive/$REF.tar.gz"
else
	URL="${GIT_URL}/releases/download/$REF/$ARTIFACT"
fi

SUM=$(wget -q "$URL" -O - | sha512sum)
echo "$SUM" | cut -d " " -f1
