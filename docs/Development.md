# Development

* **Windows**: [Windows Building Guide](./docs/build/Windows.md)
* **Linux**: [Linux Building Guide](./docs/build/Linux.md)
* **Android**: [Android Building Guide](./docs/build/Android.md)
* **Solaris**: [Solaris Building Guide](./docs/build/Solaris.md)
* **FreeBSD**: [FreeBSD Building Guide](./docs/build/FreeBSD.md)
* **macOS**: [macOS Building Guide](./docs/build/macOS.md)

# Building speedup

If you have an HDD, use ramdisk (build in RAM):
```sh
sudo mkdir /tmp/ramdisk
sudo chmod 777 /tmp/ramdisk
# about 10GB needed
sudo mount -t tmpfs -o size=10G myramdisk /tmp/ramdisk
cmake -B /tmp/ramdisk
cmake --build /tmp/ramdisk -- -j32
sudo umount /tmp/ramdisk
```

# How to test JIT

## gdb

Run `./build/bin/eden-cli -c <path to your config file (see logs where you run eden normally to see where it is)> -d -g <path to game>`

Then hook up an aarch64-gdb (use `yay aarch64-gdb` or `sudo pkg in arch64-gdb` to install)
Then type `target remote localhost:1234` and type `c` (for continue) - and then if it crashes just do a `bt` (backtrace) and `layout asm`.

### gdb cheatsheet

- `mo <cmd>`: Monitor commands, `get info`, `get fastmem` and `get mappings` are available.
- `detach`: Detach from remote (i.e restarting the emulator).
- `c`: Continue
- `p <expr>`: Print variable, `p/x <expr>` for hexadecimal.
- `r`: Run
- `bt`: Print backtrace
- `info threads`: Print all active threads
- `thread <number>`: Switch to the given thread (see `info threads`)
- `layout asm`: Display in assembly mode (TUI)
- `si`: Step assembly instruction
- `s` or `step`: Step over LINE OF CODE (not assembly)
- `display <expr>`: Display variable each step.
- `n`: Next (skips over call frame of a function)
- `frame <number>`: Switches to the given frame (from `bt`)
- `br <expr>`: Set breakpoint at `<expr>`.
- `delete`: Deletes all breakpoints.
- `catch throw`: Breakpoint at throw. Can also use `br __cxa_throw`

Expressions can be `variable_names` or `1234` (numbers) or `*var` (dereference of a pointer) or `*(1 + var)` (computed expression).

For more information type `info gdb` and read [the man page](https://man7.org/linux/man-pages/man1/gdb.1.html).

## Bisecting older commits

Since going into the past can be tricky (especially due to the dependencies from the project being lost thru time). This should "restore" the URLs for the respective submodules.

```sh
#!/bin/sh
cat > .gitmodules <<EOF
[submodule "enet"]
	path = externals/enet
	url = https://github.com/lsalzman/enet.git
[submodule "cubeb"]
	path = externals/cubeb
	url = https://github.com/mozilla/cubeb.git
[submodule "dynarmic"]
	path = externals/dynarmic
	url = https://github.com/lioncash/dynarmic.git
[submodule "libusb"]
	path = externals/libusb/libusb
	url = https://github.com/libusb/libusb.git
[submodule "discord-rpc"]
	path = externals/discord-rpc
	url = https://github.com/yuzu-emu-mirror/discord-rpc.git
[submodule "Vulkan-Headers"]
	path = externals/Vulkan-Headers
	url = https://github.com/KhronosGroup/Vulkan-Headers.git
[submodule "sirit"]
	path = externals/sirit
	url = https://github.com/yuzu-emu-mirror/sirit.git
[submodule "mbedtls"]
	path = externals/mbedtls
	url = https://github.com/yuzu-emu-mirror/mbedtls.git
[submodule "xbyak"]
	path = externals/xbyak
	url = https://github.com/herumi/xbyak.git
[submodule "opus"]
	path = externals/opus
	url = https://github.com/xiph/opus.git
[submodule "SDL"]
	path = externals/SDL
	url = https://github.com/libsdl-org/SDL.git
[submodule "cpp-httplib"]
	path = externals/cpp-httplib
	url = https://github.com/yhirose/cpp-httplib.git
[submodule "ffmpeg"]
	path = externals/ffmpeg/ffmpeg
	url = https://github.com/FFmpeg/FFmpeg.git
[submodule "vcpkg"]
	path = externals/vcpkg
	url = https://github.com/microsoft/vcpkg.git
[submodule "cpp-jwt"]
	path = externals/cpp-jwt
	url = https://github.com/arun11299/cpp-jwt.git
[submodule "libadrenotools"]
	path = externals/libadrenotools
	url = https://github.com/bylaws/libadrenotools.git
[submodule "tzdb_to_nx"]
	path = externals/nx_tzdb/tzdb_to_nx
	url = https://github.com/lat9nq/tzdb_to_nx.git
[submodule "VulkanMemoryAllocator"]
	path = externals/VulkanMemoryAllocator
	url = https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
[submodule "breakpad"]
	path = externals/breakpad
	url = https://github.com/yuzu-emu-mirror/breakpad.git
[submodule "simpleini"]
	path = externals/simpleini
	url = https://github.com/brofield/simpleini.git
[submodule "oaknut"]
	path = externals/oaknut
	url = https://github.com/merryhime/oaknut.git
[submodule "Vulkan-Utility-Libraries"]
	path = externals/Vulkan-Utility-Libraries
	url = https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
[submodule "oboe"]
	path = externals/oboe
	url = https://github.com/google/oboe.git
[submodule "externals/boost-headers"]
	path = externals/boost-headers
	url = https://github.com/boostorg/headers.git
EOF
git submodule sync
update_or_checkout () {
    if [ $0 = 'externals/sirit' ] \
    || [ $0 = 'externals/dynarmic' ] \
    || [ $0 = 'externals/breakpad' ] \
    || [ $0 = 'externals/discord-rpc' ] \
    || [ $0 = 'externals/mbedtls' ]; then
        [ -f $0/CMakeLists.txt ] || git submodule update --force --remote --init -- $0
        echo $0 ':remote' && git submodule update --remote $0
        exit
    elif [ $0 = 'externals/nx_tzdb/tzdb_to_nx' ]; then
        [ -f $0/CMakeLists.txt ] || git submodule update --force --remote --init -- $0
        echo $0 ':remote' && git submodule update --remote $0
    else
        echo $0 ':update' && git submodule update --init $0 && exit
        echo $0 ':remote' && git submodule update --remote $0 && exit
        echo $0 ':failure'
    fi
}
export -f update_or_checkout
grep path .gitmodules | sed 's/.*= //' | xargs -n 1 -I {} bash -c 'update_or_checkout "$@"' {}
# Fix for LLVM builds
sed -i 's/src\/yuzu\/main.cpp/${CMAKE_SOURCE_DIR}\/src\/yuzu\/main.cpp/g' CMakeModules/FindLLVM.cmake
# Only after cloning and everything - fixes issues with Zydis
cat > externals/dynarmic/src/dynarmic/common/x64_disassemble.cpp <<EOF
#include <cstddef>
#include <vector>
#include <string>
namespace Dynarmic::Common {
void DumpDisassembledX64(const void* ptr, size_t size) {}
std::vector<std::string> DisassembleX64(const void* ptr, size_t size) { return {}; }
}
EOF
```

If having issues with older artifacts, then run `rm -r externals/dynarmic/build externals/dynarmic/externals externals/nx_tzdb/tzdb_to_nx/externals externals/sirit/externals`.

Configuring CMake with `-DSIRIT_USE_SYSTEM_SPIRV_HEADERS=1 -DCMAKE_CXX_FLAGS="-Wno-error" -DCMAKE_C_FLAGS="-Wno-error -Wno-array-parameter -Wno-stringop-overflow"` is also recommended.

