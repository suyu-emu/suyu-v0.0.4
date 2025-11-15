#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# specify full path if dupes may exist
EXCLUDE_FILES="CPM.cmake CPMUtil.cmake GetSCMRev.cmake renderdoc_app.h tools/cpm tools/shellcheck.sh tools/update-cpm.sh tools/windows/vcvarsall.sh externals/stb externals/glad externals/getopt externals/gamemode externals/FidelityFX-FSR externals/demangle externals/bc_decoder"

# license header constants, please change when needed :))))
YEAR=2025
HOLDER="Eden Emulator Project"
LICENSE="GPL-3.0-or-later"

usage() {
	cat << EOF
Usage: $0 [uc]
Compares the current HEAD to the master branch to check for license
header discrepancies. Each file changed in a branch MUST have a
license header, and this script attempts to enforce that.

Options:
    -u, --update	Fix license headers, if applicable;
                	if the license header exists but has the incorrect
                	year or is otherwise malformed, it will be fixed.

    -c, --commit	Commit changes to Git (requires --update)

Copyright $YEAR $HOLDER
Licensed under $LICENSE

The following files/directories are marked as external
and thus will not have license headers asserted:
EOF

	for file in $EXCLUDE_FILES; do
		echo "- $file"
	done

	exit 0
}

while true; do
	case "$1" in
		(-uc) UPDATE=true; COMMIT=true ;;
		(-u|--update) UPDATE=true ;;
		(-c|--commit) COMMIT=true ;;
		("$0") break ;;
		("") break ;;
		(*) usage ;;
	esac

	shift
done

# human-readable header string
header() {
	header_line1 "$1"
	header_line2 "$1"
}

header_line1() {
	echo "$1 SPDX-FileCopyrightText: Copyright $YEAR $HOLDER"
}

header_line2() {
	echo "$1 SPDX-License-Identifier: $LICENSE"
}

check_header() {
	begin="$1"
	file="$2"

	# separate things out as spaces to make our lives 100000000x easier
    content="$(head -n5 < "$2" | tr '\n' ' ')"

	line1="$(header_line1 "$begin")"
	line2="$(header_line2 "$begin")"

	# perl and awk are actually awful, so to avoid this problem we avoid it by avoiding it
	if ! echo "$content" | grep -o "$line1 $line2" >/dev/null; then
		# SRC_FILES is Kotlin/C++
		# OTHER_FILES is sh, CMake
		case "$begin" in
			"//")
				SRC_FILES="$SRC_FILES $file"
				;;
			"#")
				OTHER_FILES="$OTHER_FILES $file"
				;;
		esac
	fi
}

BASE=$(git merge-base master HEAD)
FILES=$(git diff --name-only "$BASE")

for file in $FILES; do
    [ -f "$file" ] || continue

	# skip files that are third party (crueter's CMake modules, sse2neon, etc)
	for pattern in $EXCLUDE_FILES; do
		case "$file" in
			*"$pattern"*)
				excluded=true
				break
				;;
			*)
				excluded=false
				;;
		esac
	done

	[ "$excluded" = "true" ] && continue

	case "$file" in
		*.cmake|*.sh|CMakeLists.txt)
			begin="#"
			;;
		*.kt*|*.cpp|*.h)
			begin="//"
			;;
		*)
			continue
			;;
    esac

	check_header "$begin" "$file"
done

if [ -z "$SRC_FILES" ] && [ -z "$OTHER_FILES" ]; then
    echo "-- All good."

    exit
fi

echo

if [ "$SRC_FILES" != "" ]; then
    echo "-- The following source files have incorrect license headers:"

	HEADER=$(header "//")

    for file in $SRC_FILES; do echo "-- * $file"; done

    cat << EOF

-- The following license header should be added to the start of these offending files:

=== BEGIN ===
$HEADER
===  END  ===

EOF

fi

if [ "$OTHER_FILES" != "" ]; then
    echo "-- The following CMake and shell scripts have incorrect license headers:"

	HEADER=$(header "#")

    for file in $OTHER_FILES; do echo "-- * $file"; done

    cat << EOF

-- The following license header should be added to the start of these offending files:

=== BEGIN ===
$HEADER
===  END  ===

EOF

fi

cat << EOF
    If some of the code in this PR is not being contributed by the original author,
    the files which have been exclusively changed by that code can be ignored.
    If this happens, this PR requirement can be bypassed once all other files are addressed.

EOF

if [ "$UPDATE" = "true" ]; then
	TMP_DIR=$(mktemp -d)
	echo "-- Fixing headers..."

	for file in $SRC_FILES $OTHER_FILES; do
		case $(basename -- "$file") in
			*.cmake|CMakeLists.txt)
				begin="#"
				shell="false"
				;;
			*.sh)
				begin="#"
				shell=true
				;;
			*.kt*|*.cpp|*.h)
				begin="//"
				shell="false"
				;;
		esac

		# This is fun
		match="$begin SPDX-FileCopyrightText.*$HOLDER"

		# basically if the copyright holder is already defined we can just replace the year
		if head -n5 < "$file" | grep -e "$match" >/dev/null; then
			replace=$(header_line1 "$begin")
			sed "s|$match|$replace|" "$file" > "$file".bak
			mv "$file".bak "$file"
		else
			header "$begin" > "$TMP_DIR"/header

			if [ "$shell" = "true" ]; then
				# grab shebang
				head -n1 "$file" > "$TMP_DIR/shebang"
				echo >> "$TMP_DIR/shebang"

				# remove shebang
				sed '1d' "$file" > "$file".bak
				mv "$file".bak "$file"

				# add to header
				cat "$TMP_DIR"/shebang "$TMP_DIR"/header > "$TMP_DIR"/new-header
				mv "$TMP_DIR"/new-header "$TMP_DIR"/header
			else
				echo >> "$TMP_DIR/header"
			fi

			cat "$TMP_DIR"/header "$file" > "$file".bak
			mv "$file".bak "$file"
		fi

		[ "$shell" = "true" ] && chmod a+x "$file"
		[ "$COMMIT" = "true" ] && git add "$file"
	done

	echo "-- Done"
fi

if [ "$COMMIT" = "true" ]; then
	echo "-- Committing changes"

	git commit -m "Fix license headers"

	echo "-- Changes committed. You may now push."
fi

[ -d "$TMP_DIR" ] && rm -rf "$TMP_DIR"
