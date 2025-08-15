# Building for Solaris

## Dependencies.  
Always consult [the OpenIndiana package list](https://pkg.openindiana.org/hipster/en/index.shtml) to cross-verify availability.

Run the usual update + install of essential toolings: `sudo pkg update && sudo pkg install git cmake`.

- **gcc**: `sudo pkg install developer/gcc-14`.
- **clang**: Version 20 is broken, use `sudo pkg install developer/clang-19`.

Then install the libraies: `sudo pkg install qt6 boost glslang libzip library/lz4 nlohmann-json openssl opus sdl2 zlib compress/zstd unzip pkg-config nasm autoconf mesa library/libdrm header-drm`.

fmtlib is not available on repositories and has to be manually built:
```sh
git clone --recurisve --depth=1 https://github.com/fmtlib/fmt.git
cd fmt
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
sudo cmake --install build
```

pkg lz4 doesn't provide a proper CMakeFile to find the library, has to also be manually built:
```sh
git clone --depth=1 https://github.com/lz4/lz4.git
cd lz4
gmake
sudo gmake install
```

Same goes for zstd:
```sh
git clone --depth=1 https://github.com/facebook/zstd.git
cd zstd
cmake -DCMAKE_BUILD_TYPE=Release -B build0 -S build/cmake
cmake --build build0
cd build0
sudo gmake install
```

pkg SDL2 is also not nice to work with on CMake, save yourself some pain and compile it yourself:
```sh
git clone --depth=1 --branch=release-2.32.8 https://github.com/libsdl-org/SDL
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
sudo cmake --install build
```

Audio is broken in OpenIndiana [see this issue](https://github.com/libsdl-org/SDL/issues/13405), go into `SDL/CMakeLists.txt` and comment out lines 1468:
```diff
+#        set(SDL_AUDIO_DRIVER_SUNAUDIO 1)
+#        file(GLOB SUN_AUDIO_SOURCES ${SDL2_SOURCE_DIR}/src/audio/sun/*.c)
+#        list(APPEND SOURCE_FILES ${SUN_AUDIO_SOURCES})
+#        set(HAVE_SDL_AUDIO TRUE)
```
For Solaris this issue does not exist - however PulseAudio crashes on Solaris - so use a different backend.

---

### Build preparations:  
Run the following command to clone eden with git:
```sh
git clone --recursive https://git.eden-emu.dev/eden-emu/eden
```
You usually want to add the `--recursive` parameter as it also takes care of the external dependencies for you.

Now change into the eden directory and create a build directory there:
```sh
cd eden
mkdir build
```

Change into that build directory: `cd build`

Now choose one option either 1 or 2, but not both as one option overwrites the other.

### Building

```sh
# Needed for some dependencies that call cc directly (tz)
echo '#!/bin/sh' >cc
echo 'gcc $@' >>cc
chmod +x cc
export PATH="$PATH:$PWD"
```

- **Configure**: `cmake -B build -DYUZU_TESTS=OFF -DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF -DYUZU_USE_LLVM_DEMANGLE=OFF -DYUZU_USE_QT_MULTIMEDIA=OFF -DYUZU_USE_QT_WEB_ENGINE=OFF -DYUZU_USE_BUNDLED_VCPKG=OFF -DYUZU_USE_BUNDLED_QT=OFF -DENABLE_QT=OFF -DSDL_AUDIO=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_QT_UPDATE_CHECKER=OFF`.
- **Build**: `cmake --build build`.
- **Installing**: `sudo cmake --install build`.

### Running

Default Mesa is a bit outdated, the following environment variables should be set for a smoother experience:
```sh
export MESA_GL_VERSION_OVERRIDE=4.6
export MESA_GLSL_VERSION_OVERRIDE=460
export MESA_EXTENSION_MAX_YEAR=2025
export MESA_DEBUG=1
export MESA_VK_VERSION_OVERRIDE=1.3
# Only if nvidia/intel drm drivers cause crashes, will severely hinder performance
export LIBGL_ALWAYS_SOFTWARE=1
```

### Notes

- Modify the generated ffmpeg.make (in build dir) if using multiple threads (base system `make` doesn't use `-j4`, so change for `gmake`).
- If using OpenIndiana, due to a bug in SDL2 cmake configuration; Audio driver defaults to SunOS `<sys/audioio.h>`, which does not exist on OpenIndiana.
- Enabling OpenSSL requires compiling OpenSSL manually instead of using the provided one from repositores.
