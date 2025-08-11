#!/bin/sh

SUM=`wget -q https://github.com/$1/archive/$2.zip -O - | sha512sum`
echo "$SUM" | cut -d " " -f1
