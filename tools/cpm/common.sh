#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

##################################
# CHANGE THESE FOR YOUR PROJECT! #
##################################

# TODO: cache cpmfile defs

must_install() {
	for cmd in "$@"; do
		command -v "$cmd" >/dev/null 2>&1 || { echo "-- $cmd must be installed" && exit 1; }
	done
}

must_install jq find mktemp tar 7z unzip sha512sum git patch curl xargs

# How many levels to go (3 is 2 subdirs max)
MAXDEPTH=3

# For your project you'll want to change this to define what dirs you have cpmfiles in
# Remember to account for the MAXDEPTH variable!
# Adding ./ before each will help to remove duplicates
CPMFILES=$(find . src -maxdepth "$MAXDEPTH" -name cpmfile.json | sort | uniq)

# shellcheck disable=SC2016
PACKAGES=$(echo "$CPMFILES" | xargs jq -s 'reduce .[] as $item ({}; . * $item)')

LIBS=$(echo "$PACKAGES" | jq -j 'keys_unsorted | join(" ")')

export PACKAGES
export CPMFILES
export LIBS
export DIRS
export MAXDEPTH
