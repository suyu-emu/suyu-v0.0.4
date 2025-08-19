# Building for Solaris

## Dependencies.  
Always consult [the OpenIndiana package list](https://pkg.openindiana.org/hipster/en/index.shtml) to cross-verify availability.

Run the usual update + install of essential toolings: `sudo pkg update && sudo pkg install git cmake`.

- **gcc**: `sudo pkg install developer/gcc-14`.
- **clang**: Version 20 is broken, use `sudo pkg install developer/clang-19`.

Then install the libraies: `sudo pkg install qt6 boost glslang libzip library/lz4 nlohmann-json openssl opus sdl2 zlib compress/zstd unzip pkg-config nasm autoconf mesa library/libdrm header-drm developer/fmt`.

### Building

Clone eden with git `git clone --recursive https://git.eden-emu.dev/eden-emu/eden`

```sh
# Needed for some dependencies that call cc directly (tz)
echo '#!/bin/sh' >cc
echo 'gcc $@' >>cc
chmod +x cc
export PATH="$PATH:$PWD"
```

Patch for FFmpeg:
```sh
sed -i 's/ make / gmake /' externals/ffmpeg/CMakeFiles/ffmpeg-build.dir/build.make
```

- **Configure**: `cmake -B build -DYUZU_USE_CPM=ON -DCMAKE_CXX_FLAGS="-I/usr/include/SDL2" -DCMAKE_C_FLAGS="-I/usr/include/SDL2"`.
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
- System OpenSSL generally does not work. Instead, use `-DYUZU_USE_CPM=ON` to use a bundled static OpenSSL, or build a system dependency from source.