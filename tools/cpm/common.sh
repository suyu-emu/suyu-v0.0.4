#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

##################################
# CHANGE THESE FOR YOUR PROJECT! #
##################################

# Which directories to search
DIRS=". src"

# How many levels to go (3 is 2 subdirs max)
MAXDEPTH=3

# shellcheck disable=SC2038
# shellcheck disable=SC2016
# shellcheck disable=SC2086
[ -z "$PACKAGES" ] && PACKAGES=$(find $DIRS -maxdepth "$MAXDEPTH" -name cpmfile.json | xargs jq -s 'reduce .[] as $item ({}; . * $item)')

# For your project you'll want to change the PACKAGES call to include whatever locations you may use (externals, src, etc.)
# Always include .
LIBS=$(echo "$PACKAGES" | jq -j 'keys_unsorted | join(" ")')

export PACKAGES
export LIBS
export DIRS
export MAXDEPTH

value() {
	echo "$JSON" | jq -r ".$1"
}