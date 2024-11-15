#! /bin/sh
QT_QPA_PLATFORM=xcb QT_PLUGIN_PATH=. exec ./ld-linux-x86-64.so.2 --library-path . ./yuzu
