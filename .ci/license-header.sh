#!/bin/sh -e

HEADER="$(cat "$PWD/.ci/license/header.txt")"

echo "Getting branch changes"

# I created this cursed POSIX abomination only to discover a better solution
#BRANCH=`git rev-parse --abbrev-ref HEAD`
#COMMITS=`git log ${BRANCH} --not master --pretty=format:"%h"`
#RANGE="${COMMITS[${#COMMITS[@]}-1]}^..${COMMITS[0]}"
#FILES=`git diff-tree --no-commit-id --name-only ${RANGE} -r`

FILES=$(git diff --name-only master)

echo "Done"

for file in $FILES; do
    EXTENSION="${file##*.}"
    case "$EXTENSION" in
        kts|kt|cpp|h)
            CONTENT="`cat $file`"
            case "$CONTENT" in
                "$HEADER"*) ;;
                *) BAD_FILES="$BAD_FILES $file" ;;
            esac
            ;;
    esac
done

if [ "$BAD_FILES" = "" ]; then
    echo
    echo "All good."

    exit
fi

echo "The following files have incorrect license headers:"
echo

for file in $BAD_FILES; do echo $file; done

cat << EOF

The following license header should be added to the start of all offending files:

=== BEGIN ===
$HEADER
===  END  ===

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
