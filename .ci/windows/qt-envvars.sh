# This is specific to the CI runner and is unlikely to work on your machine.
QTDIR="/c/Qt/6.9.0/msvc2022_64"

export QT_ROOT_DIR="$QTDIR"
export QT_PLUGIN_PATH="$QTDIR/plugins"
export QML2_IMPORT_PATH="$QTDIR/qml"
export PATH="${PATH};$QTDIR/bin"