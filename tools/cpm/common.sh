#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

##################################
# CHANGE THESE FOR YOUR PROJECT! #
##################################

# How many levels to go (3 is 2 subdirs max)
MAXDEPTH=3

# For your project you'll want to change this to define what dirs you have cpmfiles in
# Remember to account for the MAXDEPTH variable!
# Adding ./ before each will help to remove duplicates
[ -z "$CPMFILES" ] && CPMFILES=$(find . ./src -maxdepth "$MAXDEPTH" -name cpmfile.json | sort | uniq)

# shellcheck disable=SC2016
[ -z "$PACKAGES" ] && PACKAGES=$(echo "$CPMFILES" | xargs jq -s 'reduce .[] as $item ({}; . * $item)')

LIBS=$(echo "$PACKAGES" | jq -j 'keys_unsorted | join(" ")')

export PACKAGES
export CPMFILES
export LIBS
export DIRS
export MAXDEPTH

value() {
	echo "$JSON" | jq -r ".$1"
}