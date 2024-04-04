#! /bin/sh

exec find src -iname *.h -o -iname *.cpp | xargs clang-format-15 -i -style=file:src/.clang-format
