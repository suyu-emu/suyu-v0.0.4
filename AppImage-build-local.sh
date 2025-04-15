#!/bin/bash

FILE=build/bin/eden
if test -f "$FILE"; then
    # remove any previously made AppImage in the base eden git folder
    rm ./eden.AppImage

    # enter AppImage utility folder
    cd AppImageBuilder

    # run the build script to create the AppImage
    # (usage) ./build.sh [source eden build folder] [destination .AppImage file]
    ./build.sh ../build ./eden.AppImage

    FILE=./eden.AppImage
    if test -f "$FILE"; then
       # move the AppImage to the main eden folder
       mv eden.AppImage ..
       # return to main eden folder
       cd ..
       # show contents of current folder
       echo
       ls
       # show AppImages specifically
       echo
       ls *.AppImage
       echo
       echo "'eden.AppImage' is now located in the current folder."
       echo
    else
       cd ..
       echo "AppImage was not built."
    fi
else
    echo
    echo "$FILE does not exist."
    echo
    echo "No eden executable found in the /eden/build/bin folder!"
    echo
    echo "You must first build a native linux version of eden before running this script!"
    echo
fi
