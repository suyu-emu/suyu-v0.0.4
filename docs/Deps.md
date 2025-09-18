# Dependencies

To build Eden, you MUST have a C++ compiler.
* On Linux, this is usually [GCC](https://gcc.gnu.org/) 11+ or [Clang](https://clang.llvm.org/) v14+
  - GCC 12 also requires Clang 14+
* On Windows, this is either:
  - **[MSVC](https://visualstudio.microsoft.com/downloads/)**,
    * *A convenience script to install the **minimal** version (Visual Build Tools) is provided in `.ci/windows/install-msvc.ps1`*
  - clang-cl - can be downloaded from the MSVC installer,
  - or **[MSYS2](https://www.msys2.org)**
* On macOS, this is Apple Clang
  - This can be installed with `xcode-select --install`

The following additional tools are also required:

* **[CMake](https://www.cmake.org/)** 3.22+ - already included with the Android SDK
* **[Git](https://git-scm.com/)** for version control
  - **[Windows installer](https://gitforwindows.org)**
* On Windows, you must install the **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** as well
  - *A convenience script to install the latest SDK is provided in `.ci/windows/install-vulkan-sdk.ps1`*

If you are on desktop and plan to use the Qt frontend, you *must* install Qt 6, and optionally Qt Creator (the recommended IDE for building)
* On Linux, *BSD and macOS, this can be done by the package manager
  - If you wish to use Qt Creator, append `qtcreator` or `qt-creator` to the commands seen below.
* MSVC/clang-cl users on Windows must install through the [official installer](https://www.qt.io/download-qt-installer-oss)
* Linux and macOS users may choose to use the installer as well.
* MSYS2 can also install Qt 6 via the package manager

If you are on Windows and NOT building with MSYS2, you may go [back home](Build.md) and continue.

## Externals
The following are handled by Eden's externals:

* [FFmpeg](https://ffmpeg.org/) (should use `-DYUZU_USE_EXTERNAL_FFMPEG=ON`)
* [SDL2](https://www.libsdl.org/download-2.0.php) 2.0.18+ (should use `-DYUZU_USE_EXTERNAL_SDL2=ON` OR `-DYUZU_USE_BUNDLED_SDL2=ON` to reduce compile time)

All other dependencies will be downloaded and built by [CPM](https://github.com/cpm-cmake/CPM.cmake/) if `YUZU_USE_CPM` is on, but will always use system dependencies if available (UNIX-like only):

* [Boost](https://www.boost.org/users/download/) 1.57.0+
* [Catch2](https://github.com/catchorg/Catch2) 3.0.1 if `YUZU_TESTS` or `DYNARMIC_TESTS` are on
* [fmt](https://fmt.dev/) 8.0.1+
* [lz4](http://www.lz4.org)
* [nlohmann\_json](https://github.com/nlohmann/json) 3.8+
* [OpenSSL](https://www.openssl.org/source/) 1.1.1+
* [ZLIB](https://www.zlib.net/) 1.2+
* [zstd](https://facebook.github.io/zstd/) 1.5+
* [enet](http://enet.bespin.org/) 1.3+
* [Opus](https://opus-codec.org/) 1.3+
* [MbedTLS](https://github.com/Mbed-TLS/mbedtls) 3+

Vulkan 1.3.274+ is also needed:
* [VulkanUtilityLibraries](https://github.com/KhronosGroup/Vulkan-Utility-Libraries)
* [VulkanHeaders](https://github.com/KhronosGroup/Vulkan-Headers)
* [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)
* [SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers)

Certain other dependencies will be fetched by CPM regardless. System packages *can* be used for these libraries, but many are either not packaged by most distributions OR have issues when used by the system:

* [SimpleIni](https://github.com/brofield/simpleini)
* [DiscordRPC](https://github.com/eden-emulator/discord-rpc)
* [cubeb](https://github.com/mozilla/cubeb)
* [libusb](https://github.com/libusb/libusb)
* [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [sirit](https://github.com/eden-emulator/sirit)
* [httplib](https://github.com/yhirose/cpp-httplib) - if `ENABLE_QT_UPDATE_CHECKER` or `ENABLE_WEB_SERVICE` are on
* [cpp-jwt](https://github.com/arun11299/cpp-jwt) 1.4+ - if `ENABLE_WEB_SERVICE` is on
* [unordered-dense](https://github.com/martinus/unordered_dense)
* [mcl](https://github.com/azahar-emu/mcl) - subject to removal

On amd64:
* [xbyak](https://github.com/herumi/xbyak) - 7.22 or earlier is recommended
* [zycore](https://github.com/zyantific/zycore-c)
* [zydis](https://github.com/zyantific/zydis) 4+
* Note: zydis and zycore-c MUST match. Using one as a system dependency and the other as a bundled dependency WILL break things

On aarch64 OR if `DYNARMIC_TESTS` is on:
* [oaknut](https://github.com/merryhime/oaknut) 2.0.1+

On riscv64:
* [biscuit](https://github.com/lioncash/biscuit) 0.9.1+

## Commands

These are commands to install all necessary dependencies on various Linux and BSD distributions, as well as macOS. Always review what you're running before you hit Enter!

Click on the arrows to expand.

<details>
<summary>Arch Linux</summary>

```sh
sudo pacman -Syu --needed base-devel boost catch2 cmake enet ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt6-base qt6-multimedia sdl2 zlib zstd zip unzip zydis zycore vulkan-headers vulkan-utility-libraries libusb spirv-tools spirv-headers
```

* Building with QT Web Engine requires `qt6-webengine` as well.
* Proper Wayland support requires `qt6-wayland`
* GCC 11 or later is required.
</details>

<details>
<summary>Ubuntu, Debian, Mint Linux</summary>

```sh
sudo apt-get install autoconf cmake g++ gcc git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qt6-base-private-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev pkg-config zlib1g-dev libva-dev libvdpau-dev qt6-tools-dev libzydis-dev zydis-tools libzycore-dev
```

* Ubuntu 22.04, Linux Mint 20, or Debian 12 or later is required.
* To enable QT Web Engine, add `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake.
</details>

<details>
<summary>Fedora Linux</summary>

```sh
sudo dnf install autoconf ccache cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt6-linguist qt6-qtbase{-private,}-devel qt6-qtwebengine-devel qt6-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel
```

* Force system libraries via CMake arguments:
  * SDL2: `-DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF`
  * FFmpeg: `-DYUZU_USE_EXTERNAL_FFMPEG=OFF`
* [RPM Fusion](https://rpmfusion.org/) is required for `ffmpeg-devel`
* Fedora 32 or later is required.
* Fedora 36+ users with GCC 12 need Clang and should configure CMake with:
</details>

<details>
<summary>macOS</summary>

Install dependencies from **[Homebrew](https://brew.sh/)**

```sh
brew install autoconf automake boost ffmpeg fmt glslang hidapi libtool libusb lz4 ninja nlohmann-json openssl pkg-config qt@6 sdl2 speexdsp zlib zstd cmake Catch2 molten-vk vulkan-loader spirv-tools
```

If you are compiling on Intel Mac, or are using a Rosetta Homebrew installation, you must replace all references of `/opt/homebrew` with `/usr/local`.

To run with MoltenVK, install additional dependencies:
```sh
brew install molten-vk vulkan-loader
```

</details>

<details>
<summary>FreeBSD</summary>

```
devel/cmake
devel/sdl20
devel/boost-libs
devel/catch2
devel/libfmt
devel/nlohmann-json
devel/ninja
devel/nasm
devel/autoconf
devel/pkgconf
devel/qt6-base

net/enet

multimedia/ffnvcodec-headers
multimedia/ffmpeg

audio/opus

archivers/liblz4

lang/gcc12

graphics/glslang
graphics/vulkan-utility-libraries
```

If using FreeBSD 12 or prior, use `devel/pkg-config` instead.
</details>

<details>
<summary>OpenBSD</summary>

```sh
pkg_add -u
pkg_add cmake nasm git boost unzip--iconv autoconf-2.72p0 bash ffmpeg glslang gmake llvm-19.1.7p3 qt6 jq fmt nlohmann-json enet boost vulkan-utility-libraries vulkan-headers spirv-headers spirv-tools catch2 sdl2 libusb1.1.0.27
```
</details>

<details>
<summary>Solaris / OpenIndiana</summary>

Always consult [the OpenIndiana package list](https://pkg.openindiana.org/hipster/en/index.shtml) to cross-verify availability.

Run the usual update + install of essential toolings: `sudo pkg update && sudo pkg install git cmake`.

- **gcc**: `sudo pkg install developer/gcc-14`.
- **clang**: Version 20 is broken, use `sudo pkg install developer/clang-19`.

Then install the libraries: `sudo pkg install qt6 boost glslang libzip library/lz4 nlohmann-json openssl opus sdl2 zlib compress/zstd unzip pkg-config nasm autoconf mesa library/libdrm header-drm developer/fmt`.
</details>

<details>
<summary>MSYS2</summary>

* Open the `MSYS2 MinGW 64-bit` shell (`mingw64.exe`)
* Download and install all dependencies using:
  * `pacman -Syu git make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-cmake mingw-w64-x86_64-python-pip mingw-w64-x86_64-qt6 mingw-w64-x86_64-toolchain autoconf libtool automake-wrapper`
* Add MinGW binaries to the PATH:
  * `echo 'PATH=/mingw64/bin:$PATH' >> ~/.bashrc`
* Add VulkanSDK to the PATH:
  * `echo 'PATH=$(readlink -e /c/VulkanSDK/*/Bin/):$PATH' >> ~/.bashrc`
</details>

## All Done

You may now return to the **[root build guide](Build.md)**.