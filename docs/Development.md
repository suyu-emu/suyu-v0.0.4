# Development guidelines

## License Headers
All commits must have proper license header accreditation.

You can easily add all necessary license headers by running:
```sh
git fetch origin master:master
.ci/license-header.sh -u -c
git push
```

Alternatively, you may omit `-c` and do an amend commit:
```sh
git fetch origin master:master
.ci/license-header.sh
git commit --amend -a --no-edit
```

If the work is licensed/vendored from other people or projects, you may omit the license headers. Additionally, if you wish to retain authorship over a piece of code, you may attribute it to yourself; however, the code may be changed at any given point and brought under the attribution of Eden.

For more information on the license header script, run `.ci/license-header.sh -h`.

## Pull Requests
Pull requests are only to be merged by core developers when properly tested and discussions conclude on Discord or other communication channels. Labels are recommended but not required. However, all PRs MUST be namespaced and optionally typed:
```
[cmake] refactor: CPM over submodules
[desktop] feat: implement firmware install from ZIP
[hle] stub fw20 functions
[core] test: raise maximum CPU cores to 6
[cmake, core] Unbreak FreeBSD Building Process
```

- The level of namespacing is generally left to the committer's choice.
- However, we never recommend going more than two levels *except* in `hle`, in which case you may go as many as four levels depending on the specificity of your changes.
- Ocassionally, up to two additional namespaces may be provided for more clarity.
  * Changes that affect the entire project (sans CMake changes) should be namespaced as `meta`.
- Maintainers are permitted to change namespaces at will.
- Commits within PRs are not required to be namespaced, but it is highly recommended.

## Adding new settings

When adding new settings, use `tr("Setting:")` if the setting is meant to be a field, otherwise use `tr("Setting")` if the setting is meant to be a Yes/No or checkmark type of setting, see [this short style guide](https://learn.microsoft.com/en-us/style-guide/punctuation/colons#in-ui).

- The majority of software must work with the default option selected for such setting. Unless the setting significantly degrades performance.
- Debug settings must never be turned on by default.
- Provide reasonable bounds (for example, a setting controlling the amount of VRAM should never be 0).
- The description of the setting must be short and concise, if the setting "does a lot of things" consider splitting the setting into multiple if possible.
- Try to avoid excessive/redundant explainations "recommended for most users and games" can just be "(recommended)".
- Try to not write "slow/fast" options unless it clearly degrades/increases performance for a given case, as most options may modify behaviour that result in different metrics accross different systems. If for example the option is an "accuracy" option, writing "High" is sufficient to imply "Slow". No need to write "High (Slow)".

Some examples:
- "[...] negatively affecting image quality", "[...] degrading image quality": Same wording but with less filler.
- "[...] this may cause some glitches or crashes in some games", "[...] this may cause soft-crashes": Crashes implies there may be glitches (as crashes are technically a form of a fatal glitch). The entire sentence is structured as "may cause [...] on some games", which is redundant, because "may cause [...] in games" has the same semantic meaning ("may" is a chance that it will occur on "some" given set).
- "FIFO Relaxed is similar to FIFO [...]", "FIFO Relaxed [...]": The name already implies similarity.
- "[...] but may also reduce performance in some cases", "[...] but may degrade performance": Again, "some cases" and "may" implies there is a probability.
- "[...] it can [...] in some cases", "[...] it can [...]": Implied probability.

Before adding a new setting, consider:
- Does the piece of code that the setting pertains to, make a significant difference if it's on/off?
- Can it be auto-detected?

# IDE setup

## VSCode
Copy this to `.vscode/settings.json`, get CMake tools and it should be ready to build:
```json
{
    "editor.tabSize": 4,
    "files.watcherExclude": {
        "**/target": true
    },
    "files.associations": {
      "*.inc": "cpp"
    },
    "git.enableCommitSigning": true,
    "git.alwaysSignOff": true
}
```

You may additionally need the `Qt Extension Pack` extension if building Qt.

# Build speedup

If you have an HDD, use ramdisk (build in RAM):
```sh
sudo mkdir /tmp/ramdisk
sudo chmod 777 /tmp/ramdisk
# about 8GB needed
sudo mount -t tmpfs -o size=8G myramdisk /tmp/ramdisk
cmake -B /tmp/ramdisk
cmake --build /tmp/ramdisk -- -j32
sudo umount /tmp/ramdisk
```

# Assets and large files

A general rule of thumb, before uploading files:
- PNG files: Use [optipng](https://web.archive.org/web/20240325055059/https://optipng.sourceforge.net/).
- SVG files: Use [svgo](https://github.com/svg/svgo).

May not be used but worth mentioning nonethless:
- OGG files: Use [OptiVorbis](https://github.com/OptiVorbis/OptiVorbis).
- Video files: Use ffmpeg, preferably re-encode as AV1.

# Bisecting older commits

Since going into the past can be tricky (especially due to the dependencies from the project being lost thru time). This should "restore" the URLs for the respective submodules.

```sh
#!/bin/sh -e
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
