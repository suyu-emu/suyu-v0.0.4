### Dependencies

You'll need to download and install the following to build Eden:

  * [GCC](https://gcc.gnu.org/) v11+ (for C++20 support) & misc
  * If GCC 12 is installed, [Clang](https://clang.llvm.org/) v14+ is required for compiling
  * [CMake](https://www.cmake.org/) 3.22+

The following are handled by Eden's externals:

  * [FFmpeg](https://ffmpeg.org/)
  * [SDL2](https://www.libsdl.org/download-2.0.php) 2.0.18+
  * [opus](https://opus-codec.org/downloads/) 1.3+
  
All other dependencies will be downloaded and built by [CPM](https://github.com/cpm-cmake/CPM.cmake/) if `YUZU_USE_CPM` is on, but will always use system dependencies if available:

  * [Boost](https://www.boost.org/users/download/) 1.79.0+
  * [Catch2](https://github.com/catchorg/Catch2) 2.13.7 - 2.13.9
  * [fmt](https://fmt.dev/) 8.0.1+
  * [lz4](http://www.lz4.org) 1.8+
  * [nlohmann_json](https://github.com/nlohmann/json) 3.8+
  * [OpenSSL](https://www.openssl.org/source/) 1.1.1+
  * [ZLIB](https://www.zlib.net/) 1.2+
  * [zstd](https://facebook.github.io/zstd/) 1.5+
  * [enet](http://enet.bespin.org/) 1.3+
  * [cubeb](https://github.com/mozilla/cubeb)
  * [SimpleIni](https://github.com/brofield/simpleini)

Certain other dependencies (httplib, jwt, sirit, etc.) will be fetched by CPM regardless. System packages *can* be used for these libraries but this is generally not recommended.

Dependencies are listed here as commands that can be copied/pasted. Of course, they should be inspected before being run.

- Arch / Manjaro:
  - `sudo pacman -Syu --needed base-devel boost catch2 cmake enet ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt6-base qt6-multimedia sdl2 zlib zstd zip unzip`
  - Building with QT Web Engine requires `qt6-webengine` as well.
  - Proper wayland support requires `qt6-wayland`
  - GCC 11 or later is required.
  
- Ubuntu / Linux Mint / Debian:
  - `sudo apt-get install autoconf cmake g++ gcc git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qt6-base-private-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev pkg-config zlib1g-dev libva-dev libvdpau-dev`
  - Ubuntu 22.04, Linux Mint 20, or Debian 12 or later is required.
  - Users need to manually specify building with QT Web Engine enabled.  This is done using the parameter `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake.
  - Users need to manually disable building SDL2 from externals if they intend to use the version provided by their system by adding the parameters `-DYUZU_USE_EXTERNAL_SDL2=OFF`

```sh
git submodule update --init --recursive
cmake .. -GNinja -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
```

- Fedora:
  - `sudo dnf install autoconf ccache cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt6-linguist qt6-qtbase{-private,}-devel qt6-qtwebengine-devel qt6-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel`
  - Fedora 32 or later is required.
  - Due to GCC 12, Fedora 36 or later users need to install `clang`, and configure CMake to use it via `-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang`
  - CMake arguments to force system libraries:
    - SDL2: `-DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF`
    - FFmpeg: `-DYUZU_USE_EXTERNAL_FFMPEG=OFF`
  - [RPM Fusion](https://rpmfusion.org/) (free) is required to install `ffmpeg-devel`

### Cloning Eden with Git

**Master:**

```bash
git clone --recursive https://git.eden-emu.dev/eden-emu/eden
cd eden
```

The `--recursive` option automatically clones the required Git submodules.

### Building Eden in Release Mode (Optimised)

If you need to run ctests, you can disable `-DYUZU_TESTS=OFF` and install Catch2.

```bash
mkdir build && cd build
cmake .. -GNinja -DYUZU_TESTS=OFF
ninja
sudo ninja install 
```
You may also want to include support for Discord Rich Presence by adding `-DUSE_DISCORD_PRESENCE=ON` after `cmake ..`

`-DYUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS=OFF` might be needed if ninja command failed with `undefined reference to symbol 'spvOptimizerOptionsCreate`, reason currently unknown

Optionally, you can use `cmake-gui ..` to adjust various options (e.g. disable the Qt GUI).

### Building Eden in Debug Mode (Slow)

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DYUZU_TESTS=OFF
ninja
```

### Building with debug symbols

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU -DYUZU_TESTS=OFF
ninja
```

### Building with Scripts
A convenience script for building is provided in `.ci/linux/build.sh`. You must provide an arch target for optimization, e.g. `.ci/linux/build.sh amd64`. Valid targets:
- `legacy`: x86_64 generic, only needed for CPUs older than 2013 or so
- `amd64`: x86_64-v3, for CPUs newer than 2013 or so
- `steamdeck` / `zen2`: For Steam Deck or Zen >= 2 AMD CPUs (untested on Intel)
- `rog-ally` / `allyx` / `zen4`: For ROG Ally X or Zen >= 4 AMD CPUs (untested on Intel)
- `aarch64`: For armv8-a CPUs, older than mid-2021 or so
- `armv9`: For armv9-a CPUs, newer than mid-2021 or so
- `native`: Optimize to your native host architecture

Extra flags to pass to CMake should be passed after the arch target.

Additional environment variables can be used to control building:
- `NPROC`: Number of threads to use for compilation (defaults to all)
- `TARGET`: Set to `appimage` to disable standalone `eden-cli` and `eden-room` executables
- `BUILD_TYPE`: Sets the build type to use. Defaults to `Release`

The following environment variables are boolean flags. Set to `true` to enable or `false` to disable:
- `DEVEL` (default FALSE): Disable Qt update checker
- `USE_WEBENGINE` (default FALSE): Enable Qt WebEngine
- `USE_MULTIMEDIA` (default TRUE): Enable Qt Multimedia

After building, an AppImage can be packaged via `.ci/linux/package.sh`. This script takes the same arch targets as the build script. If the build was created in a different directory, you can specify its path relative to the source directory, e.g. `.ci/linux/package.sh amd64 build-appimage`. Additionally, set the `DEVEL` environment variable to `true` to change the app name to `Eden Nightly`.

### Running without installing

After building, the binaries `eden` and `eden-cmd` (depending on your build options) will end up in `build/bin/`.

```bash
# SDL
cd build/bin/
./eden-cmd

# Qt
cd build/bin/
./eden
```
