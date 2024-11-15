#! /bin/sh

# NOTE: the `ld-linux-aarch64.so.1` filename came from a pi debian 11 installation,
#       this may be incorrect for a different or more up-to-date system.
# Can find out the correct filename using command "ldd yuzu" on the non-AppImage app
QT_QPA_PLATFORM=xcb QT_PLUGIN_PATH=. exec ./ld-linux-aarch64.so.1 --library-path . ./yuzu
