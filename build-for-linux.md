### Dependencies

You'll need to download and install the following to build yuzu:

  * [GCC](https://gcc.gnu.org/) v11+ (for C++20 support) & misc
    * This page is being updated as we transition to GCC 11
  * If GCC 12 is installed, [Clang](https://clang.llvm.org/) v14+ is required for compiling
  * [CMake](https://www.cmake.org/) 3.15+

The following are handled by yuzu's externals:

  * [FFmpeg](https://ffmpeg.org/)
  * [SDL2](https://www.libsdl.org/download-2.0.php) 2.0.18+
  * [opus](https://opus-codec.org/downloads/)

If version 5.15.2 is not already installed, pre-compiled binaries for Qt 5.15.2 will be downloaded from [here](https://github.com/litucks/ext-linux-bin) automatically by CMake:

  * [Qt](https://qt-project.org/downloads) 5.15+

All other dependencies will be downloaded by [vcpkg](https://vcpkg.io/) if needed:

  * [Boost](https://www.boost.org/users/download/) 1.79.0+
  * [Catch2](https://github.com/catchorg/Catch2) 2.13.7 - 2.13.9
  * [fmt](https://fmt.dev/) 8.0.1+
  * [lz4](http://www.lz4.org) 1.8+
  * [nlohmann_json](https://github.com/nlohmann/json) 3.8+
  * [OpenSSL](https://www.openssl.org/source/)
  * [ZLIB](https://www.zlib.net/) 1.2+
  * [zstd](https://facebook.github.io/zstd/) 1.5+

If an ARM64 build is intended, export `VCPKG_FORCE_SYSTEM_BINARIES=1`.

Dependencies are listed here as commands that can be copied/pasted. Of course, they should be inspected before being run.

- Arch / Manjaro:
  - `sudo pacman -Syu --needed base-devel boost catch2 cmake ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt5 sdl2 zlib zstd zip unzip`
  - Building with QT Web Engine needs to be specified when running CMake with the param `-DCMAKE_CXX_FLAGS="-I/usr/include/qt/QtWebEngineWidgets"` with qt5-webengine installed.
  - GCC 11 or later is required.
- Ubuntu / Linux Mint / Debian:
  - `sudo apt-get install autoconf cmake g++-11 gcc-11 git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qtbase5-dev qtbase5-private-dev qtwebengine5-dev qtmultimedia5-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev`
  - Ubuntu 22.04, Linux Mint 20, or Debian Bullseye or later is required.
  -  Users need to manually specify building with QT Web Engine enabled.  This is done using the parameter `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake. 
  - Users need to manually specify building with GCC 11. This can be done by adding the parameters `-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11` when running CMake. i.e.
  - Users need to manually disable building SDL2 from externals if they intend to use the version provided by their system by adding the parameters `-DYUZU_USE_EXTERNAL_SDL2=OFF`

```
git submodule update --init --recursive
cmake .. -GNinja -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
```

- Fedora:
  - `sudo dnf install autoconf ccache cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt5-linguist qt5-qtbase{-private,}-devel qt5-qtwebengine-devel qt5-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel`
  - Fedora 32 or later is required.
  - Due to GCC 12, Fedora 36 or later users need to install `clang`, and configure CMake to use it via `-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang`
  - CMake arguments to force system libraries:
    - SDL2: `-DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF`
    - FFmpeg: `-DYUZU_USE_EXTERNAL_FFMPEG=OFF`
  - [RPM Fusion](https://rpmfusion.org/) (free) is required to install `ffmpeg-devel`
- Gentoo:
  - **\*\*Disclaimer\*\***: this dependency list was written by a novice Gentoo user who first set it up with a DE, and then based this list off of the Fedora dependency list. This may be missing some requirements, or includes too many. Caveat emptor.
  - `emerge --ask app-arch/lz4 dev-libs/boost dev-libs/hidapi dev-libs/libzip dev-libs/openssl dev-qt/linguist dev-qt/qtconcurrent dev-qt/qtcore dev-util/cmake dev-util/glslang dev-vcs/git media-libs/alsa-lib media-libs/opus media-sound/pulseaudio media-video/ffmpeg net-libs/mbedtls sys-libs/zlib x11-libs/libXext`
  - GCC 11 or later is required.
  - Users may need to append `pulseaudio`, `bindist` and `context` to the `USE` flag.

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

### Building yuzu in Release Mode (Optimized)

If you need to run ctests, you can disable `-DYUZU_TESTS=OFF` and install Catch2.

```bash
mkdir build && cd build
cmake .. -GNinja -DYUZU_USE_BUNDLED_VCPKG=ON -DYUZU_TESTS=OFF
ninja
sudo ninja install 
```

Optionally, you can use `cmake-gui ..` to adjust various options (e.g. disable the Qt GUI).

### Building yuzu in Debug Mode (Slow)

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DYUZU_USE_BUNDLED_VCPKG=ON -DYUZU_TESTS=OFF
ninja
```

### Building with debug symbols

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_USE_BUNDLED_VCPKG=ON -DYUZU_TESTS=OFF
ninja
```

### Running without installing

After building, the binaries `yuzu` and `yuzu-cmd` (depending on your build options) will end up in `build/bin/`.

  ```bash
  # SDL
  cd build/bin/
  ./yuzu-cmd

  # Qt
  cd build/bin/
  ./yuzu
  ```

### Debugging

1. Enable CPU debugging
   ![](https://raw.githubusercontent.com/flathub/org.yuzu_emu.yuzu/master/assets/yuzu-settings-2.png)
2. Disable both Host MMU emulation options
   ![](https://raw.githubusercontent.com/flathub/org.yuzu_emu.yuzu/master/assets/yuzu-settings-1.png)
3. Run gdb

```bash
cd data
gdb ../build/bin/yuzu            # Start GDB
(gdb) handle SIGSEGV nostop      # Disable SIGSEGV exceptions, which are used by yuzu for memory access
(gdb) run                        # Run yuzu under GDB
<crash>
(gdb) bt                         # Print a backtrace of the entire callstack to see which codepath the crash occurred on