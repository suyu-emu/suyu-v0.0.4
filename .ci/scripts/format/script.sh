#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

shopt -s nullglob globstar

if git grep -nrI '\s$' src **/*.yml **/*.txt **/*.md Doxyfile .gitignore .gitmodules .ci* dist/*.desktop dist/*.svg dist/*.xml; then
    echo Trailing whitespace found, aborting
    exit 1
fi

# Default clang-format points to default 3.5 version one
CLANG_FORMAT="${CLANG_FORMAT:-clang-format-15}"
"$CLANG_FORMAT" --version

# Turn off tracing for this because it's too verbose
set +x

# Check everything for branch pushes
FILES_TO_LINT="$(find src/ -name '*.cpp' -or -name '*.h')"

for f in $FILES_TO_LINT; do
    echo "$f"
    "$CLANG_FORMAT" -i "$f"
done

DIFF=$(git -c core.fileMode=false diff)

if [ ! -z "$DIFF" ]; then
    echo "!!! Not compliant to coding style, here is the fix:"
    echo "$DIFF"
    exit 1
fi

cd src/android
./gradlew ktlintCheck
