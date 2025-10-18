#!/bin/sh

SRC=en_US
TARGET=en_US

# requires fd
SOURCES=`fd . src/yuzu src/qt_common -tf -e ui -e cpp -e h -e plist`

lupdate -source-language $SRC -target-language $TARGET $SOURCES -ts dist/languages/en.ts
