#! /bin/sh
if [ -d /usr/lib/aarch64-linux-gnu/qt5 ]; then
    exec ./bwrap --dev-bind / / --tmpfs /usr/lib/aarch64-linux-gnu/qt5 ./yuzu.sh
else
    if [ -d /usr/lib/aarch64-linux-gnu/qt6 ]; then
        exec ./bwrap --dev-bind / / --tmpfs /usr/lib/aarch64-linux-gnu/qt6 ./yuzu.sh
    fi
fi
