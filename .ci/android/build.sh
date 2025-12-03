#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

NUM_JOBS=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
export CMAKE_BUILD_PARALLEL_LEVEL="${NUM_JOBS}"
ARTIFACTS_DIR="$PWD/artifacts"

: "${CCACHE:=false}"
RETURN=0

usage() {
    cat <<EOF
Usage: $0 [-t|--target FLAVOR] [-b|--build-type BUILD_TYPE]
       [-h|--help] [-r|--release] [extra options]

Build script for Android.
Associated variables can be set outside the script,
and will apply both to this script and the packaging script.
bool values are "true" or "false"

Options:
    -r, --release        	Enable update checker. If set, sets the DEVEL bool variable to false.
                         	By default, DEVEL is true.
    -t, --target <FLAVOR> 	Build flavor (variable: TARGET)
                          	Valid values are: legacy, optimized, standard, chromeos
                          	Default: standard
    -b, --build-type <TYPE>	Build type (variable: TYPE)
                          	Valid values are: Release, RelWithDebInfo, Debug
                          	Default: Debug

Extra arguments are passed to CMake (e.g. -DCMAKE_OPTION_NAME=VALUE)
Set the CCACHE variable to "true" to enable build caching.
The APK and AAB will be output into "artifacts".

EOF

    exit "$RETURN"
}

die() {
	echo "-- ! $*" >&2
	RETURN=1 usage
}

target() {
    [ -z "$1" ] && die "You must specify a valid target."

    TARGET="$1"
}

type() {
    [ -z "$1" ] && die "You must specify a valid type."

    TYPE="$1"
}

while true; do
	case "$1" in
		-r|--release) DEVEL=false ;;
		-t|--target) target "$2"; shift ;;
		-b|--build-type) type "$2"; shift ;;
		-h|--help) usage ;;
		*) break ;;
	esac

	shift
done

: "${TARGET:=standard}"
: "${TYPE:=Release}"
: "${DEVEL:=true}"

TARGET_LOWER=$(echo "$TARGET" | tr '[:upper:]' '[:lower:]')

case "$TARGET_LOWER" in
	legacy) FLAVOR=Legacy ;;
	optimized) FLAVOR=GenshinSpoof ;;
	standard) FLAVOR=Mainline ;;
    chromeos) FLAVOR=ChromeOS ;;
	*) die "Invalid build flavor $TARGET."
esac

case "$TYPE" in
	RelWithDebInfo|Release|Debug) ;;
	*) die "Invalid build type $TYPE."
esac

LOWER_FLAVOR=$(echo "$FLAVOR" | sed 's/./\L&/')
LOWER_TYPE=$(echo "$TYPE" | sed 's/./\L&/')

if [ -n "${ANDROID_KEYSTORE_B64}" ]; then
    export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
    echo "${ANDROID_KEYSTORE_B64}" | base64 --decode > "${ANDROID_KEYSTORE_FILE}"
	SHA1SUM=$(keytool -list -v -storepass "${ANDROID_KEYSTORE_PASS}" -keystore "${ANDROID_KEYSTORE_FILE}" | grep SHA1 | cut -d " " -f3)
	echo "-- Keystore SHA1 is ${SHA1SUM}"
fi

cd src/android
chmod +x ./gradlew

set -- "$@" -DUSE_CCACHE="${CCACHE}"
[ "$DEVEL" != "true" ] && set -- "$@" -DENABLE_UPDATE_CHECKER=ON

echo "-- building..."

./gradlew "copy${FLAVOR}${TYPE}Outputs" \
    -Dorg.gradle.caching="${CCACHE}" \
    -Dorg.gradle.parallel="${CCACHE}" \
    -Dorg.gradle.workers.max="${NUM_JOBS}" \
    -PYUZU_ANDROID_ARGS="$*" \
    --info

if [ -n "${ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi

echo "-- Done! APK and AAB artifacts are in ${ARTIFACTS_DIR}"

ls -l "${ARTIFACTS_DIR}/"
