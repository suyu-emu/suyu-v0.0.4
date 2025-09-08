Eden is not currently available as a port on FreeBSD, though it is in the works. For now, the recommended method of usage is to compile it yourself. Check back often, as the build process frequently changes.

## Dependencies.
Eden needs the following dependencies:

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

Change into that build directory:
```sh
cd build
```

#### 1. Building in Release Mode (usually preferred and the most performant choice):
```sh
cmake .. -GNinja -DYUZU_TESTS=OFF
```

#### 2. Building in Release Mode with debugging symbols (useful if you want to debug errors for a eventual fix):
```sh
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_TESTS=ON
```

Build the emulator locally:
```sh
ninja
```

Optional: If you wish to install eden globally onto your system issue the following command:
```sh
sudo ninja install
```
OR
```sh
doas -- ninja install
```

## OpenSSL
The available OpenSSL port (3.0.17) is out-of-date, and using a bundled static library instead is recommended; to do so, add `-DYUZU_USE_CPM=ON` to your CMake configure command.