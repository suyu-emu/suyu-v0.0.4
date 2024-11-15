#! /bin/sh

# default qt5 location
if [ -d /usr/lib/x86_64-linux-gnu/qt5 ]; then
    exec ./bwrap --dev-bind / / --tmpfs /usr/lib/x86_64-linux-gnu/qt5 ./yuzu.sh
else
    # qt5 on Steam Deck (as qt)
    if [ -d /usr/lib/qt ]; then
        exec ./bwrap --dev-bind / / --tmpfs /usr/lib/qt ./yuzu.sh
    else
        # default qt6 location
        if [ -d /usr/lib/x86_64-linux-gnu/qt6 ]; then
            exec ./bwrap --dev-bind / / --tmpfs /usr/lib/x86_64-linux-gnu/qt6 ./yuzu.sh
        else
            # qt6 on Steam Deck
            if [ -d /usr/lib/qt6 ]; then
                exec ./bwrap --dev-bind / / --tmpfs /usr/lib/qt6 ./yuzu.sh
            fi
        fi
    fi
fi
