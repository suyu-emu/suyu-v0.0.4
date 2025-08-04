#!/bin/sh

for i in $@; do
    SUM=`wget -q $i -O - | sha512sum`
    echo "$i"
    echo "URL_HASH SHA512=$SUM" | cut -d " " -f1-2
done
