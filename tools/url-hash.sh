#!/bin/sh

SUM=`wget -q $1 -O - | sha512sum`
echo "$SUM" | cut -d " " -f1
