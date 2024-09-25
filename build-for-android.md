## Using Android Studio

**Note: These build instructions are a work-in-progress.**

### Dependencies
* [Android Studio](https://developer.android.com/studio)
* [NDK 26.3.11579264 and CMake 3.22.1](https://developer.android.com/studio/projects/install-ndk#default-version)
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
sudo sdkmanager "ndk;26.3.11579264" "platforms;android-34" "build-tools;34.0.0" "cmake;3.22.1" "platform-tools;34.0.5"
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

## Building an installable turnip driver package

Adapted from the mesa documentation [here](https://docs.mesa3d.org/android.html#building-using-the-android-ndk) and the released zip in [this](https://github.com/K11MCH1/AdrenoToolsDrivers) GitHub repo.

### Dependencies

Follow the steps to setup a CLI build above and additionally run
```
sudo apt-get install -y python3-mako python3-yaml byacc flex
```
to install some additional required packages.

Create a file `~/.local/share/meson/cross/android-aarch64` with the following content:
```
[binaries]
ar = '/opt/android-sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar'
c = ['/opt/android-sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android30-clang']
cpp = ['/opt/android-sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android30-clang++', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-static-libstdc++']
c_ld = 'lld'
cpp_ld = 'lld'
strip = '/opt/android-sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip'
pkg-config = ['env', 'PKG_CONFIG_LIBDIR=/opt/android-sdk/ndk/26.3.11579264/toolchains/llvm/prebuilt/linux-x86_64/python3/lib/pkgconfig', '/usr/bin/pkg-config']
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
```
This informs `meson` (the build system used by mesa) about the Android NDK tools.
If you need to modify this file with newer versions you can use a command like `find /opt/android-sdk -name "aarch64-linux-android*-clang"` to find the new paths.

### Clone the mesa git repo
```
git clone https://gitlab.freedesktop.org/mesa/mesa.git
```
Then use `git checkout <version>` to checkout the version you want to build (for example `mesa-24.2.0`).

### Compiling the turnip driver
```
meson setup build-android-aarch64 --cross-file android-aarch64 -Dplatforms=android -Dplatform-sdk-version=30 -Dandroid-stub=true -Dgallium-drivers= -Dvulkan-drivers=freedreno -Dfreedreno-kmds=kgsl -Dbuildtype=release
meson compile -C build-android-aarch64
```

### Create an installable driver package
Check the file `VERSION` for the Mesa version and the file `include/vulkan/vulkan_core.h` and look for `VK_HEADER_VERSION_COMPLETE` for the Vulkan version.

Create a file named `meta.json` with the following content (replacing the `<placeholders>`)
```
{
  "schemaVersion": 1,
  "name": "Mesa Turnip Driver <mesa_version>",
  "description": "Compiled from source.",
  "author": "<name>",
  "packageVersion": "1",
  "vendor": "Mesa",
  "driverVersion": "Vulkan <vulkan_version>",
  "minApi": 30,
  "libraryName": "libvulkan_freedreno.so"
}
```
and run these commands to create the package
```
mv ./build-android-aarch64/src/freedreno/vulkan/libvulkan_freedreno.so libvulkan_freedreno.so
zip turnip.zip meta.json libvulkan_freedreno.so
```
