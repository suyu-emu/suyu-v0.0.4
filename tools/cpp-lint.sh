#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# tools/../
ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/../" && pwd)
BUILD_DIR="$ROOTDIR"/build
SRC_DIR="$ROOTDIR"/src

die() {
    echo "-- $*" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $0 [command]

Dumb script that serves as a ad-hoc cpp-linter

Commands:
    once    Check for #pragma once prescence in header files
	osdef	Finds OS defines that are not recommended to use.
	inchk   Check includes being valid/toolchain not being stupid
EOF
}

while :; do
	case "$1" in
	once)
        find "$SRC_DIR" -type f -name "*.h" -exec grep -L "#pragma once" {} +
		break
		;;
	osdef)
		# not recommended macros
		PATTERN="ANDROID\|_WIN64\|__linux\|__unix\|APPLE\|__APPLE"
		strings=("ANDROID" "_WIN64" "__linux" "__unix" "APPLE" "__APPLE" "linux" "unix")
		for item in "${strings[@]}"; do
			PATTERN="$PATTERN\|ifdef $item\|($item)"
		done
		# if statements for macros that shouldn't be if
		strings=("_WIN32" "_AIX" "__managarm__" "__unix__" "__linux__" "__FreeBSD__" "__NetBSD__" \
			"__OpenBSD__" "__DragonFly__" "__redox__" "__HAIKU__" "__OHOS__" "__FIREOS__")
		for item in "${strings[@]}"; do
			PATTERN="$PATTERN\|if $item"
		done
        find "$SRC_DIR" -type f -name "*.h" -exec grep -nw "$PATTERN" {} + || echo
		break
		;;
	*) usage ;;
	esac

	shift
done
