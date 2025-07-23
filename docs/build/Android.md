# Note: These build instructions are a work-in-progress.

## Dependencies
* [Android Studio](https://developer.android.com/studio)
* [NDK 25.2.9519653 and CMake 3.22.1](https://developer.android.com/studio/projects/install-ndk#default-version)
* [Git](https://git-scm.com/download)

### WINDOWS ONLY - Additional Dependencies
  * **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)** - **Make sure to select "Desktop development with C++" support in the installer. Make sure to update to the latest version if already installed.**
  * **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** - **Make sure to select Latest SDK.**
    - A convenience script to install the latest SDK is provided in `.ci\windows\install-vulkan-sdk.ps1`.

## Cloning Eden with Git
```
git clone --recursive https://git.eden-emu.dev/eden-emu/eden.git
```
Eden by default will be cloned into -
* `C:\Users\<user-name>\eden` on Windows
* `~/eden` on Linux
* And wherever on macOS

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
5. Then Build with `./gradlew assemblerelWithDebInfo`.
6. To build the optimised build use `./gradlew assembleGenshinSpoofRelWithDebInfo`.

### Script
A convenience script for building is provided in `.ci/android/build.sh`. The built APK can be put into an `artifacts` directory via `.ci/android/package.sh`. On Windows, these must be done in the Git Bash or MinGW terminal.

### Additional Resources
https://developer.android.com/studio/intro
