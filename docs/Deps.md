# Dependencies

To build Eden, you MUST have a C++ compiler.
* On Linux, this is usually [GCC](https://gcc.gnu.org/) 11+ or [Clang](https://clang.llvm.org/) v14+
  - GCC 12 also requires Clang 14+
* On Windows, we support:
  - **[MSVC](https://visualstudio.microsoft.com/downloads/)** (default)
    - It's STRONGLY RECOMMENDED to use the **Community** option and **Visual Studio 2022**
    - You need to install: **[Desktop development with C++](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170)**
  - **[clang-cl](https://learn.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=msvc-180)**
    - You need to install: **C++ Clang tools for Windows**
  - **[MSYS2](https://www.msys2.org)** (experimental)
* On macOS, this is Apple Clang
  - This can be installed with `xcode-select --install`

The following additional tools are also required:

* **[CMake](https://www.cmake.org/)** 3.22+ - already included with the Android SDK
* **[Git](https://git-scm.com/)** for version control
  - **[Windows installer](https://gitforwindows.org)**
* **[Python3](https://www.python.org/downloads/)** 3.10+ - necessary to download external repositories
* On Windows, you must install the **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** as well
  - *A convenience script to install the latest SDK is provided in:*
    - `tools/windows/install-vulkan-sdk.ps1` (for PowerShell 5+)
    - `tools/windows/install-vulkan-sdk.sh` (for MSYS2, Git Bash, etc)

If you are on desktop and plan to use the Qt frontend, you *must* install Qt 6, and optionally Qt Creator (the **RECOMMENDED** IDE for building)
* On Linux, *BSD and macOS, this can be done by the package manager
  - If you wish to use Qt Creator, append `qtcreator` or `qt-creator` to the commands seen below.
* MSVC/clang-cl users on Windows must install through the official [Qt](https://www.qt.io/download-qt-installer-oss) installer
* Linux and macOS users may choose to use the installer as well.
* MSYS2 can also install Qt 6 via the package manager

If you are on Windows, a convenience script to install MSVC, MSYS2, Qt, all necessary packages for MSYS2, and set up a zsh environment with useful keybinds and aliases can be found [here](https://git.crueter.xyz/scripts/windev).
- For help setting up Qt Creator, run `./install.sh -h qtcreator`

If you are on **Windows** and building with **MSVC** or **clang-cl**, you may go [back home](Build.md) and continue.

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
* [httplib](https://github.com/yhirose/cpp-httplib) - if `ENABLE_UPDATE_CHECKER` or `ENABLE_WEB_SERVICE` are on
  - This package is known to be broken on the AUR.
* [cpp-jwt](https://github.com/arun11299/cpp-jwt) 1.4+ - if `ENABLE_WEB_SERVICE` is on
* [unordered-dense](https://github.com/martinus/unordered_dense)
* [mcl](https://github.com/azahar-emu/mcl) - subject to removal

On amd64:
* [xbyak](https://github.com/herumi/xbyak) - 7.22 or earlier is recommended

On aarch64 OR if `DYNARMIC_TESTS` is on:
* [oaknut](https://github.com/merryhime/oaknut) 2.0.1+

On riscv64:
* [biscuit](https://github.com/lioncash/biscuit) 0.9.1+

## Commands

These are commands to install all necessary dependencies on various Linux and BSD distributions, as well as macOS. Always review what you're running before you hit Enter!

Notes for writers: Include build tools as well, assume user has NOTHING installed (i.e a fresh install) but that they have updated beforehand so no `upgrade && update` or equivalent should be mentioned - except for rolling release systems like Arch.

Click on the arrows to expand.

<details>
<summary>Gentoo Linux</summary>

GURU must be enabled:

```
sudo emerge -a app-eselect/eselect-repository
sudo eselect repository enable guru
sudo emaint sync -r guru
```

Now, install all deps:

```sh
sudo emerge -a \
    app-arch/lz4 app-arch/zstd app-arch/unzip \
    dev-libs/libfmt dev-libs/libusb dev-libs/mcl dev-libs/sirit \
    dev-libs/unordered_dense dev-libs/boost dev-libs/openssl dev-libs/discord-rpc \
    dev-util/spirv-tools dev-util/spirv-headers dev-util/vulkan-headers \
    dev-util/vulkan-utility-libraries dev-util/glslang \
    media-gfx/renderdoc media-libs/libva media-libs/opus media-video/ffmpeg \
    media-libs/VulkanMemoryAllocator media-libs/libsdl2 media-libs/cubeb \
    net-libs/enet net-libs/mbedtls \
    sys-libs/zlib \
    dev-cpp/nlohmann_json dev-cpp/simpleini dev-cpp/cpp-httplib dev-cpp/cpp-jwt \
    games-util/gamemode \
    net-wireless/wireless-tools \
    dev-qt/qtbase:6 dev-libs/quazip \
    virtual/pkgconfig
```

- On `amd64`, also add `dev-libs/xbyak`
- On `riscv64`, also add `dev-libs/biscuit` (currently unavailable)
- On `aarch64`, also add `dev-libs/oaknut`
- If tests are enabled, also add `dev-libs/oaknut` and `dev-cpp/catch`

Required USE flags:
- `dev-qt/qtbase network concurrent dbus gui widgets`
- `dev-libs/quazip qt6`
- `net-libs/mbedtls cmac`
- `media-libs/libsdl2 haptic joystick sound video`
- `dev-cpp/cpp-httplib ssl`

[Caveats](./Caveats.md#gentoo-linux)

</details>

<details>
<summary>Arch Linux</summary>

```sh
sudo pacman -Syu --needed base-devel boost catch2 cmake enet ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt6-base qt6-multimedia sdl2 zlib zstd zip unzip vulkan-headers vulkan-utility-libraries libusb spirv-tools spirv-headers
```

* Building with QT Web Engine requires `qt6-webengine` as well.
* Proper Wayland support requires `qt6-wayland`
* GCC 11 or later is required.
</details>

<details>
<summary>Ubuntu, Debian, Mint Linux</summary>

```sh
sudo apt-get install autoconf cmake g++ gcc git glslang-tools libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qt6-base-private-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev pkg-config zlib1g-dev libva-dev libvdpau-dev qt6-tools-dev libvulkan-dev spirv-tools spirv-headers libusb-1.0-0-dev libxbyak-dev libboost-dev libboost-fiber-dev libboost-context-dev libsdl2-dev libopus-dev libasound2t64 vulkan-utility-libraries-dev
```

* Ubuntu 22.04, Linux Mint 20, or Debian 12 or later is required.
* To enable QT Web Engine, add `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake.
</details>

<details>
<summary>AlmaLinux, Fedora, Red Hat Linux</summary>

Fedora:
```sh
sudo dnf install autoconf cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt6-linguist qt6-qtbase{-private,}-devel qt6-qtwebengine-devel qt6-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel boost jq
```

AlmaLinux (use `YUZU_USE_CPM=ON`):
```sh
# vvv - Only if RPMfusion is not installed or EPEL isn't either
sudo dnf install epel-release dnf-utils
# (run rpmfusion installation afterwards)
# vvv - This will work for most systems
sudo dnf install autoconf cmake libtool libudev cmake gcc gcc-c++ qt6-qtbase-devel zlib-devel openssl-devel boost SDL2 ffmpeg-devel libdrm glslang jq patch
# Qt6 private GUI must be taken from CRB repos
sudo dnf config-manager --enable crb
sudo dnf install qt6-qtbase-private-devel
```

* [RPM Fusion](https://rpmfusion.org/Configuration) is required for `ffmpeg-devel`
* Fedora 32 or later is required.
* Fedora 36+ users with GCC 12 need Clang and should configure CMake with: `cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -B build`
</details>

<details>
<summary>Alpine Linux</summary>

First, enable the community repository; [see here](https://wiki.alpinelinux.org/wiki/Repositories#Enabling_the_community_repository).
```sh
# Enable the community repository
setup-apkrepos -c
# Install
apk add g++ git cmake make mbedtls-dev mbedtls-static mesa-dev qt6-qtbase-dev qt6-qtbase-private-dev libquazip1-qt6 ffmpeg-dev libusb-dev libtool boost-dev sdl2-dev zstd-dev vulkan-utility-libraries spirv-tools-dev openssl-dev nlohmann-json lz4-dev opus-dev jq patch
```

`mbedtls-static` has to be specified otherwise `libeverest.a` and `libp256m.a` will fail to be found.

</details>
<details>
<summary>Void Linux</summary>

```sh
xbps-install -Su git make cmake clang pkg-config patch mbedtls-devel SPIRV-Tools-devel SPIRV-Headers lz4 liblz4-devel boost-devel ffmpeg6-devel catch2 Vulkan-Utility-Libraries Vulkan-Headers glslang openssl-devel SDL2-devel quazip-qt6-devel qt6-base-devel qt6-qt5compat-devel fmt-devel json-c++ libenet-devel libusb-devel
```

Yes, `nlohmann-json` is just named `json-c++`. Why?

</details>

<details>
<summary>NixOS</summary>

A convenience script is provided on the root of this project [shell.nix](../shell.nix). Run the usual `nix-shell`.

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

[Caveats](./Caveats.md#macos).

</details>
<details>
<summary>FreeBSD</summary>

As root run: `pkg install devel/cmake devel/sdl20 devel/boost-libs devel/catch2 devel/libfmt devel/nlohmann-json devel/ninja devel/nasm devel/autoconf devel/pkgconf devel/qt6-base devel/simpleini net/enet multimedia/ffnvcodec-headers multimedia/ffmpeg audio/opus archivers/liblz4 lang/gcc12 graphics/glslang graphics/vulkan-utility-libraries graphics/spirv-tools www/cpp-httplib devel/jwt-cpp devel/unordered-dense mbedtls3 vulkan-headers quazip-qt6`

If using FreeBSD 12 or prior, use `devel/pkg-config` instead.

[Caveats](./Caveats.md#freebsd).

</details>
<details>
<summary>NetBSD</summary>

For NetBSD +10.1: `pkgin install git cmake boost fmtlib SDL2 catch2 libjwt spirv-headers ffmpeg7 libva nlohmann-json jq libopus qt6 mbedtls3 cpp-httplib lz4 vulkan-headers nasm autoconf enet pkg-config libusb1`.

[Caveats](./Caveats.md#netbsd).

</details>
<details>
<summary>OpenBSD</summary>

```sh
pkg_add -u
pkg_add cmake nasm git boost unzip--iconv autoconf-2.72p0 bash ffmpeg glslang gmake llvm-19.1.7p3 qt6 jq fmt nlohmann-json enet boost vulkan-utility-libraries vulkan-headers spirv-headers spirv-tools catch2 sdl2 libusb1-1.0.27
```

[Caveats](./Caveats.md#openbsd).

</details>
<details>
<summary>DragonFlyBSD</summary>

```sh
pkg install gcc14 git cmake unzip nasm autoconf bash pkgconf ffmpeg glslang gmake jq nlohmann-json enet spirv-tools sdl2 vulkan-utility-libraries vulkan-headers catch2 libfmt openssl liblz4 boost-libs cpp-httplib qt6-base quazip-qt6 unordered-dense libva-vdpau-driver libva-utils libva-intel-driver
```

[Caveats](./Caveats.md#dragonflybsd).

</details>
<details>
<summary>Solaris / OpenIndiana</summary>

```sh
sudo pkg install qt6 boost glslang libzip library/lz4 libusb-1 nlohmann-json openssl opus sdl2 zlib compress/zstd unzip pkg-config nasm autoconf mesa library/libdrm header-drm developer/fmt
```

[Caveats](./Caveats.md#solaris).

</details>
<details>
<summary>MSYS2</summary>

* Open the `MSYS2 MinGW 64-bit` shell (`mingw64.exe`)
* Download and install all dependencies:
```
BASE="git make autoconf libtool automake-wrapper jq patch"

MINGW="qt6-base qt6-tools qt6-translations qt6-svg cmake toolchain clang python-pip openssl vulkan-memory-allocator vulkan-devel glslang boost fmt lz4 nlohmann-json zlib zstd enet opus mbedtls libusb unordered_dense"

packages="$BASE"
for pkg in $MINGW; do
    packages="$packages mingw-w64-x86_64-$pkg"
done

pacman -Syuu --needed --noconfirm $packages
```
* Notes:
  - Using `qt6-static` is possible but currently untested.
  - Other environments are entirely untested, but should theoretically work provided you install all the necessary packages.
  - GCC is proven to work better with the MinGW environment. If you choose to use Clang, you *may* be better off using the clang64 environment.
  - Add `qt-creator` to the `MINGW` variable to install Qt Creator. You can then create a Start Menu shortcut to the MinGW Qt Creator by running `powershell "\$s=(New-Object -COM WScript.Shell).CreateShortcut('C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Qt Creator.lnk');\$s.TargetPath='C:\\msys64\\mingw64\\bin\\qtcreator.exe';\$s.Save()"` in Git Bash or MSYS2.
* Add MinGW binaries to the PATH if they aren't already:
  * `echo 'PATH=/mingw64/bin:$PATH' >> ~/.bashrc`
  * or `echo 'PATH=/mingw64/bin:$PATH' >> ~/.zshrc`

[Caveats](./Caveats.md#msys2).

</details>
<details>
<summary>HaikuOS</summary>

```sh
pkgman install git cmake patch libfmt_devel nlohmann_json lz4_devel opus_devel boost1.89_devel vulkan_devel qt6_base_devel libsdl2_devel ffmpeg7_devel libx11_devel enet_devel catch2_devel quazip1_qt6_devel qt6_5compat_devel libusb1_devel libz_devel mbedtls3_devel glslang
```

[Caveats](./Caveats.md#haikuos).

</details>
<details>
<summary>RedoxOS</summary>

```sh
sudo pkg update
sudo pkg install git cmake ffmpeg6 sdl2 zlib llvm18
```

[Caveats](./Caveats.md#redoxos).

</details>

## All Done

You may now return to the **[root build guide](Build.md)**.
