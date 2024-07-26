## Using Android Studio

**Note: These build instructions are a work-in-progress.**

### Dependencies
* [Android Studio](https://developer.android.com/studio)
* [NDK 25.2.9519653 and CMake 3.22.1](https://developer.android.com/studio/projects/install-ndk#default-version)
* [Git](https://git-scm.com/download)

#### WINDOWS ONLY - Additional Dependencies
  * **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)** - **Make sure to select "Desktop development with C++" support in the installer. Make sure to update to the latest version if already installed.**
  * **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** - **Make sure to select Latest SDK.**

### Cloning yuzu with Git
**from Codeberg repo (the `--recursive` option automatically clones the required Git submodules):**
```
git clone --depth 1 --recursive https://codeberg.org/litucks/torzu.git
cd torzu
```
**from Torzu repo (assuming Tor is installed as a service):**
```
git -c http.proxy=socks5h://127.0.0.1:9050 clone --depth 1 http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu.git
cd torzu
git submodule update --init --recursive
```
yuzu by default will be cloned into:
* `C:\Users\<user-name>\torzu` on Windows
* `~/torzu` on Linux
* And wherever on macOS

### Building
1. Start Android Studio, on the startup dialog select `Open`.
2. Navigate to the `torzu/src/android` directory and click on `OK`.
3. In `Build > Select Build Variant`, select `release` or `relWithDebInfo` as the "Active build variant".
4. Build the project with `Build > Make Project` or run it on an Android device with `Run > Run 'app'`.

### Additional Resources
https://developer.android.com/studio/intro

## Using CLI

**Note: These build instructions are for building on Debian Bookworm or newer**

### Dependencies
```
sudo apt-get update
sudo apt-get install -y sdkmanager openjdk-17-jdk build-essential curl git pkg-config glslang-tools zip
sudo sdkmanager "ndk;26.1.10909125" "platforms;android-34" "build-tools;33.0.1" "cmake;3.22.1" "platform-tools"
sudo update-alternatives --config java # Select Java 17 here if possible
```

### Cloning Yuzu with Git
Follow clone instructions for Android Studio above

### Build
```
./externals/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export ANDROID_HOME=/opt/android-sdk
cd src/android
./gradlew assembleRelease
```

The APK will be at `src/android/app/build/outputs/apk/mainline/release/app-mainline-release.apk`
