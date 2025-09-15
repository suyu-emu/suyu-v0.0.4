#!/bin/sh -e

HEADER="$(cat "$PWD/.ci/license/header.txt")"
HEADER_HASH="$(cat "$PWD/.ci/license/header-hash.txt")"

echo "Getting branch changes"

# BRANCH=`git rev-parse --abbrev-ref HEAD`
# COMMITS=`git log ${BRANCH} --not master --pretty=format:"%h"`
# RANGE="${COMMITS[${#COMMITS[@]}-1]}^..${COMMITS[0]}"
# FILES=`git diff-tree --no-commit-id --name-only ${RANGE} -r`

BASE=`git merge-base master HEAD`
FILES=`git diff --name-only $BASE`

#FILES=$(git diff --name-only master)

echo "Done"

check_header() {
    CONTENT="`head -n3 < $1`"
    case "$CONTENT" in
        "$HEADER"*) ;;
        *) BAD_FILES="$BAD_FILES $1" ;;
    esac
}

check_cmake_header() {
    CONTENT="`head -n3 < $1`"

    case "$CONTENT" in
        "$HEADER_HASH"*) ;;
        *)
            BAD_CMAKE="$BAD_CMAKE $1" ;;
    esac
}
for file in $FILES; do
    [ -f "$file" ] || continue

    if [ `basename -- "$file"` = "CMakeLists.txt" ]; then
        check_cmake_header "$file"
        continue
    fi

    EXTENSION="${file##*.}"
    case "$EXTENSION" in
        kts|kt|cpp|h)
            check_header "$file"
            ;;
        cmake)
            check_cmake_header "$file"
            ;;
    esac
done

if [ "$BAD_FILES" = "" ] && [ "$BAD_CMAKE" = "" ]; then
    echo
    echo "All good."

    exit
fi

if [ "$BAD_FILES" != "" ]; then
    echo "The following source files have incorrect license headers:"
    echo

    for file in $BAD_FILES; do echo $file; done

    cat << EOF

The following license header should be added to the start of all offending SOURCE files:

=== BEGIN ===
$HEADER
===  END  ===

EOF

fi

if [ "$BAD_CMAKE" != "" ]; then
    echo "The following CMake files have incorrect license headers:"
    echo

    for file in $BAD_CMAKE; do echo $file; done

    cat << EOF

The following license header should be added to the start of all offending CMake files:

=== BEGIN ===
$HEADER_HASH
===  END  ===

EOF

fi

cat << EOF
If some of the code in this PR is not being contributed by the original author,
the files which have been exclusively changed by that code can be ignored.
If this happens, this PR requirement can be bypassed once all other files are addressed.
EOF

if [ "$FIX" = "true" ]; then
    echo
    echo "FIX set to true. Fixing headers."
    echo

    for file in $BAD_FILES; do
        cat $file > $file.bak

        cat .ci/license/header.txt > $file
        echo >> $file
        cat $file.bak >> $file

        rm $file.bak

        git add $file
    done

    for file in $BAD_CMAKE; do
        cat $file > $file.bak

        cat .ci/license/header-hash.txt > $file
        echo >> $file
        cat $file.bak >> $file

        rm $file.bak

        git add $file
    done
    echo "License headers fixed."

    if [ "$COMMIT" = "true" ]; then
        echo
        echo "COMMIT set to true. Committing changes."
        echo

        git commit -m "Fix license headers"

        echo "Changes committed. You may now push."
    fi
else
    exit 1
fi
