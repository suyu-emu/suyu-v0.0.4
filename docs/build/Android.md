# Android

## Dependencies

* [Android Studio](https://developer.android.com/studio)
* [NDK 27+ and CMake 3.22.1](https://developer.android.com/studio/projects/install-ndk#default-version)
* [Git](https://git-scm.com/download)

## WINDOWS ONLY - Additional Dependencies

* **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)** - **Make sure to select "Desktop development with C++" support in the installer. Make sure to update to the latest version if already installed.**
* **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** - **Make sure to select Latest SDK.**
  * A convenience script to install the latest SDK is provided in `.ci\windows\install-vulkan-sdk.ps1`.

## Cloning Eden with Git

```sh
git clone --recursive https://git.eden-emu.dev/eden-emu/eden.git
```

Eden by default will be cloned into:

* `C:\Users\<user-name>\eden` on Windows
* `~/eden` on Linux and macOS

## Building

1. Start Android Studio, on the startup dialog select `Open`.
2. Navigate to the `eden/src/android` directory and click on `OK`.
3. In `Build > Select Build Variant`, select `release` or `relWithDebInfo` as the "Active build variant".
4. Build the project with `Build > Make Project` or run it on an Android device with `Run > Run 'app'`.

## Building with Terminal

1. Download the SDK and NDK from Android Studio.
2. Navigate to SDK and NDK paths.
3. Then set ANDROID_SDK_ROOT and ANDROID_NDK_ROOT in terminal via
`export ANDROID_SDK_ROOT=path/to/sdk`
`export ANDROID_NDK_ROOT=path/to/ndk`.
4. Navigate to `eden/src/android`.
5. Then Build with `./gradlew assembleRelWithDebInfo`.
6. To build the optimised build use `./gradlew assembleGenshinSpoofRelWithDebInfo`.
7. You can pass extra variables to cmake via `-PYUZU_ANDROID_ARGS="-D..."`

Remember to have a Java SDK installed if not already, on Debian and similar this is done with `sudo apt install openjdk-17-jdk`.

### Script

A convenience script for building is provided in `.ci/android/build.sh`. On Windows, this must be run in Git Bash or MSYS2. This script provides the following options:

```txt
Usage: build.sh [-c|--chromeos] [-t|--target FLAVOR] [-b|--build-type BUILD_TYPE]
                [-h|--help] [-r|--release] [extra options]

Build script for Android.
Associated variables can be set outside the script,
and will apply both to this script and the packaging script.
bool values are "true" or "false"

Options:
    -c, --chromeos          Build for ChromeOS (x86_64) (variable: CHROMEOS, bool)
                            Default: false
    -r, --release           Enable update checker. If set, sets the DEVEL bool variable to false.
                            By default, DEVEL is true.
    -t, --target <FLAVOR>   Build flavor (variable: TARGET)
                            Valid values are: legacy, optimized, standard
                            Default: standard
    -b, --build-type <TYPE> Build type (variable: TYPE)
                            Valid values are: Release, RelWithDebInfo, Debug
                            Default: Debug

Extra arguments are passed to CMake (e.g. -DCMAKE_OPTION_NAME=VALUE)
Set the CCACHE variable to "true" to enable build caching.
The APK and AAB will be output into "artifacts".
```

Examples:

* Build legacy release with update checker:
  * `.ci/android/build.sh -r -t legacy`
* Build standard release with debug info without update checker for phones:
  * `.ci/android/build.sh -b RelWithDebInfo`
* Build optimized release with update checker for ChromeOS:
  * `.ci/android/build.sh -c -r -t optimized`

### Additional Resources

<https://developer.android.com/studio/intro>
