# CMake Options

To change these options, add `-DOPTION_NAME=NEWVALUE` to the command line.

- On Qt Creator, go to Project -> Current Configuration

Notes:

- Defaults are marked per-platform.
- "Non-UNIX" just means Windows/MSVC and Android (yes, macOS is UNIX
- Android generally doesn't need to change anything; if you do, go to `src/android/app/build.gradle.kts`
- To set a boolean variable to on, use `ON` for the value; to turn it off, use `OFF`
- If a variable is mentioned as being e.g. "ON" for a specific platform(s), that means it is defaulted to OFF on others
- TYPE is always boolean unless otherwise specified
- Format:
  - `OPTION_NAME` (TYPE DEFAULT) DESCRIPTION

## Options

### Dependencies

These options control dependencies.

- `YUZU_USE_BUNDLED_FFMPEG` (ON for non-UNIX) Download a pre-built and configured FFmpeg
- `YUZU_USE_EXTERNAL_FFMPEG` (ON for Solaris) Build FFmpeg from source
- `YUZU_DOWNLOAD_ANDROID_VVL` (ON) Download validation layer binary for Android
- `YUZU_DOWNLOAD_TIME_ZONE_DATA` (ON) Always download time zone binaries
  - Currently, build fails without this
- `YUZU_TZDB_PATH` (string) Path to a pre-downloaded timezone database (useful for nixOS and Gentoo)
- `YUZU_USE_BUNDLED_MOLTENVK` (ON, macOS only) Download bundled MoltenVK lib
- `YUZU_USE_BUNDLED_OPENSSL` (ON for MSVC, Android, Solaris, and OpenBSD) Download bundled OpenSSL build
- `YUZU_USE_EXTERNAL_SDL2` (OFF) Compiles SDL2 from source
- `YUZU_USE_BUNDLED_SDL2` (ON for MSVC) Download a prebuilt SDL2

### Miscellaneous

- `ENABLE_WEB_SERVICE` (ON) Enable multiplayer service
- `ENABLE_WIFI_SCAN` (OFF) Enable WiFi scanning (requires iw on Linux) - experimental
- `ENABLE_CUBEB` (ON) Enables the cubeb audio backend
  - This option is subject for removal.
- `YUZU_TESTS` (ON) Compile tests - requires Catch2
- `ENABLE_LTO` (OFF) Enable link-time optimization
  - Not recommended on Windows
  - UNIX may be better off appending `-flto=thin` to compiler args
- `USE_FASTER_LINKER` (OFF) Check if a faster linker is available
  - Not recommended outside of Linux

### Flavors

These options control executables and build flavors.

- `YUZU_LEGACY` (OFF): Apply patches to improve compatibility on some older GPUs at the cost of performance
- `NIGHTLY_BUILD` (OFF): This is only used by CI. Do not use this unless you're making your own distribution and know what you're doing.
- `YUZU_STATIC_BUILD` (OFF) Attempt to build using static libraries if possible
  - Not supported on Linux
  - Automatically set if `YUZU_USE_BUNDLED_QT` is on for non-Linux
- `ENABLE_UPDATE_CHECKER` (OFF) Enable update checking functionality
- `YUZU_DISABLE_LLVM` (OFF) Do not attempt to link to the LLVM demangler
  - Really only useful for CI or distribution builds

**Desktop only**:

- `YUZU_CMD` (ON) Compile the SDL2 frontend (eden-cli)
- `YUZU_ROOM` (OFF) Compile dedicated room functionality into the main executable
- `YUZU_ROOM_STANDALONE` (OFF) Compile a separate executable for room functionality
- `YUZU_STATIC_ROOM` (OFF) Compile the room executable *only* as a static, portable executable
  - This is only usable on Alpine Linux.

### Desktop

The following options are desktop only.

- `ENABLE_LIBUSB` (ON) Enable the use of the libusb input frontend (HIGHLY RECOMMENDED)
- `ENABLE_OPENGL` (ON) Enable the OpenGL graphics frontend
  - Unavailable on Windows/ARM64
  - You probably shouldn't turn this off.

### Qt

Also desktop-only, but apply strictly to Qt

- `ENABLE_QT` (ON) Enable the Qt frontend (recommended)
- `ENABLE_QT_TRANSLATION` (OFF) Enable translations for the Qt frontend
- `YUZU_USE_BUNDLED_QT` (ON for MSVC) Download bundled Qt binaries
  - Not recommended on Linux. For Windows and macOS, the provided build is statically linked.
- `YUZU_QT_MIRROR` (string) What mirror to use for downloading the bundled Qt libraries
- `YUZU_USE_QT_MULTIMEDIA` (OFF) Use QtMultimedia for camera support
- `YUZU_USE_QT_WEB_ENGINE` (OFF) Use QtWebEngine for web applet implementation (requires the huge QtWebEngine dependency; not recommended)
- `USE_DISCORD_PRESENCE` (OFF) Enables Discord Rich Presence (Qt frontend only)

### Retired Options

The following options were a part of Eden at one point, but have since been retired.

- `ENABLE_OPENSSL` - MbedTLS was fully replaced with OpenSSL in [#3606](https://git.eden-emu.dev/eden-emu/eden/pulls/3606), because OpenSSL straight-up performs better.
- `ENABLE_SDL2` - While technically possible to *not* use SDL2 on desktop, this is **NOT** a supported configuration under any means, and adding this matrix to our build system was not worth the effort.
- `YUZU_USE_CPM` - This option once had a purpose, but that purpose has long since passed us by. *All* builds use CPMUtil to manage dependencies now.
  - If you want to *force* the usage of system dependencies, use `-DCPMUTIL_FORCE_SYSTEM=ON`.

See `src/dynarmic/CMakeLists.txt` for additional options--usually, these don't need changed
