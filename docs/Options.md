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
  * `OPTION_NAME` (TYPE DEFAULT) DESCRIPTION

## Options

- `YUZU_USE_CPM` (ON for non-UNIX) Use CPM to fetch system dependencies (fmt, boost, etc) if needed. Externals will still be fetched. See the [CPM](CPM.md) and [Deps](Deps.md) docs for more info.
- `ENABLE_WEB_SERVICE` (ON) Enable multiplayer service
- `ENABLE_WIFI_SCAN` (OFF) Enable WiFi scanning (requires iw on Linux) - experimental
- `YUZU_USE_BUNDLED_FFMPEG` (ON for non-UNIX) Download (Windows, Android) or build (UNIX) bundled FFmpeg
- `ENABLE_CUBEB` (ON) Enables the cubeb audio backend
- `YUZU_TESTS` (ON) Compile tests - requires Catch2
- `YUZU_DOWNLOAD_ANDROID_VVL` (ON) Download validation layer binary for Android
- `YUZU_ENABLE_LTO` (OFF) Enable link-time optimization
  * Not recommended on Windows
  * UNIX may be better off appending `-flto=thin` to compiler args
- `YUZU_DOWNLOAD_TIME_ZONE_DATA` (ON) Always download time zone binaries
  * Currently, build fails without this
- `YUZU_USE_FASTER_LD` (ON) Check if a faster linker is available
  * Only available on UNIX
- `YUZU_USE_BUNDLED_MOLTENVK` (ON, macOS only) Download bundled MoltenVK lib)
- `YUZU_TZDB_PATH` (string) Path to a pre-downloaded timezone database (useful for nixOS)
- `ENABLE_OPENSSL` (ON for Linux and *BSD) Enable OpenSSL backend for the ssl service
  * Always enabled if the web service is enabled
- `YUZU_USE_BUNDLED_OPENSSL` (ON for MSVC) Download bundled OpenSSL build
  * Always on for Android
  * Unavailable on OpenBSD
- `ENABLE_UPDATE_CHECKER` (OFF) Enable update checker for the Qt an Android frontends

The following options are desktop only:
- `ENABLE_SDL2` (ON) Enable the SDL2 desktop, audio, and input frontend (HIGHLY RECOMMENDED!)
  * Unavailable on Android
- `YUZU_USE_EXTERNAL_SDL2` (ON for non-UNIX) Compiles SDL2 from source
- `YUZU_USE_BUNDLED_SDL2` (ON for MSVC) Download a prebuilt SDL2
  * Unavailable on OpenBSD
  * Only enabled if YUZU_USE_CPM and ENABLE_SDL2 are both ON
- `ENABLE_LIBUSB` (ON) Enable the use of the libusb input frontend (HIGHLY RECOMMENDED)
- `ENABLE_OPENGL` (ON) Enable the OpenGL graphics frontend
  * Unavailable on Windows/ARM64 and Android
- `ENABLE_QT` (ON) Enable the Qt frontend (recommended)
- `ENABLE_QT_TRANSLATION` (OFF) Enable translations for the Qt frontend
- `YUZU_USE_BUNDLED_QT` (ON for MSVC) Download bundled Qt binaries
  * Note that using **system Qt** requires you to include the Qt CMake directory in `CMAKE_PREFIX_PATH`, e.g:
    * `-DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6`
- `YUZU_QT_MIRROR` (string) What mirror to use for downloading the bundled Qt libraries
- `YUZU_USE_QT_MULTIMEDIA` (OFF) Use QtMultimedia for camera support
- `YUZU_USE_QT_WEB_ENGINE` (OFF) Use QtWebEngine for web applet implementation (requires the huge QtWebEngine dependency; not recommended)
- `USE_DISCORD_PRESENCE` (OFF) Enables Discord Rich Presence (Qt frontend only)
- `YUZU_ROOM` (ON) Enable dedicated room functionality
- `YUZU_ROOM_STANDALONE` (ON) Enable standalone room executable (eden-room)
  * Requires `YUZU_ROOM`
- `YUZU_CMD` (ON) Compile the SDL2 frontend (eden-cli) - requires SDL2
- `YUZU_CRASH_DUMPS` Compile crash dump (Minidump) support"
  * Currently only available on Windows and Linux

See `src/dynarmic/CMakeLists.txt` for additional options--usually, these don't need changed