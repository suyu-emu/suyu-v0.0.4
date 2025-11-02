# Caveats

<!-- TOC -->
- [Caveats](#caveats)
  - [Arch Linux](#arch-linux)
  - [Gentoo Linux](#gentoo-linux)
  - [macOS](#macos)
  - [Solaris](#solaris)
  - [HaikuOS](#haikuos)
  - [OpenBSD](#openbsd)
  - [FreeBSD](#freebsd)
  - [NetBSD](#netbsd)
  - [MSYS2](#msys2)
<!-- /TOC -->

## Arch Linux

Eden is also available as an [AUR package](https://aur.archlinux.org/packages/eden-git). If you are unable to build, either use that or compare your process to the PKGBUILD.

## Gentoo Linux

[`games-emulation/eden`](https://gitweb.gentoo.org/repo/proj/guru.git/tree/games-emulation/eden) is available in the GURU. This repository also contains some additional dependencies, such as mcl, sirit, oaknut, etc.

If you're having issues with building, always consult that ebuild.

## macOS

macOS is largely untested. Expect crashes, significant Vulkan issues, and other fun stuff.

## Solaris

Qt Widgets appears to be broken. For now, add `-DENABLE_QT=OFF` to your configure command. In the meantime, a Qt Quick frontend is in the works--check back later!

This is needed for some dependencies that call cc directly (tz):

```sh
echo '#!/bin/sh' >cc
echo 'gcc $@' >>cc
chmod +x cc
export PATH="$PATH:$PWD"
```

Default MESA is a bit outdated, the following environment variables should be set for a smoother experience:
```sh
export MESA_GL_VERSION_OVERRIDE=4.6
export MESA_GLSL_VERSION_OVERRIDE=460
export MESA_EXTENSION_MAX_YEAR=2025
export MESA_DEBUG=1
export MESA_VK_VERSION_OVERRIDE=1.3
# Only if nvidia/intel drm drivers cause crashes, will severely hinder performance
export LIBGL_ALWAYS_SOFTWARE=1
```

- Modify the generated ffmpeg.make (in build dir) if using multiple threads (base system `make` doesn't use `-j4`, so change for `gmake`).
- If using OpenIndiana, due to a bug in SDL2's CMake configuration, audio driver defaults to SunOS `<sys/audioio.h>`, which does not exist on OpenIndiana. Using external or bundled SDL2 may solve this.
- System OpenSSL generally does not work. Instead, use `-DYUZU_USE_BUNDLED_OPENSSL=ON` to use a bundled static OpenSSL, or build a system dependency from source.

## HaikuOS

It's recommended to do a `pkgman full-sync` before installing. See [HaikuOS: Installing applications](https://www.haiku-os.org/guides/daily-tasks/install-applications/). Sometimes the process may be interrupted by an error like "Interrupted syscall". Simply firing the command again fixes the issue. By default `g++` is included on the default installation.

GPU support is generally lacking/buggy, hence it's recommended to only install `pkgman install mesa_lavapipe`. Performance is acceptable for most homebrew applications and even some retail games.

For reasons unberknownst to any human being, `glslangValidator` will crash upon trying to be executed, the solution to this is to build `glslang` yourself. Apply the patch in `.patch/glslang/0001-haikuos-fix.patch`. The main issue is `ShFinalize()` is deallocating already destroyed memory; the "fix" in question is allowing the program to just leak memory and the OS will take care of the rest. See [this issue](https://web.archive.org/web/20251021183604/https://github.com/haikuports/haikuports/issues/13083).

For this reason this patch is NOT applied to default on all platforms (for obvious reasons) - instead this is a HaikuOS specific patch, apply with `git apply <absolute path to patch>` after cloning SPIRV-Tools then `make -C build` and add the resulting binary (in `build/StandAlone/glslang`) into PATH.

`cubeb_devel` will also not work, either disable cubeb or uninstall it.

Still will not run flawlessly until `mesa-24` is available. Modify CMakeCache.txt with the `.so` of libGL and libGLESv2 by doing the incredibly difficult task of copy pasting them (`cp /boot/system/lib/libGL.so .`)

## OpenBSD

After configuration, you may need to modify `externals/ffmpeg/CMakeFiles/ffmpeg-build/build.make` to use `-j$(nproc)` instead of just `-j`.

`-lc++-experimental` doesn't exist in OpenBSD but the LLVM driver still tries to link against it, to solve just symlink `ln -s /usr/lib/libc++.a /usr/lib/libc++experimental.a`.

If clang has errors, try using `g++-11`.

## FreeBSD

Eden is not currently available as a port on FreeBSD, though it is in the works. For now, the recommended method of usage is to compile it yourself.

The available OpenSSL port (3.0.17) is out-of-date, and using a bundled static library instead is recommended; to do so, add `-DYUZU_USE_BUNDLED_OPENSSL=ON` to your CMake configure command.

## NetBSD

Install `pkgin` if not already `pkg_add pkgin`, see also the general [pkgsrc guide](https://www.netbsd.org/docs/pkgsrc/using.html). For NetBSD 10.1 provide `echo 'PKG_PATH="https://cdn.netbsd.org/pub/pkgsrc/packages/NetBSD/x86_64/10.0_2025Q3/All/"' >/etc/pkg_install.conf`. If `pkgin` is taking too much time consider adding the following to `/etc/rc.conf`:
```sh
ip6addrctl=YES
ip6addrctl_policy=ipv4_prefer
```

System provides a default `g++-10` which doesn't support the current C++ codebase; install `clang-19` with `pkgin install clang-19`. Then build with `cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -B build`.

Make may error out when generating C++ headers of SPIRV shaders, hence it's recommended to use `gmake` over the default system one.

glslang is not available on NetBSD, to circumvent this simply build glslang by yourself:
```sh
pkgin python313
git clone --depth=1 https://github.com/KhronosGroup/glslang.git
cd glslang
python3.13 ./update_glslang_sources.py
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j`nproc`
cmake --install build
```

## MSYS2

`qt6-static` isn't supported yet.

Only the `MINGW64` environment is tested; however, all of the others should work (in theory) sans `MINGW32`.

Currently, only FFmpeg can be used as a system dependency; the others will result in linker errors.

When packaging an MSYS2 build, you will need to copy all dependent DLLs recursively alongside the `windeployqt6`; for example:

```sh
# MSYS_TOOLCHAIN is typically just mingw64
# since Windows is case-insensitive, you can set this to $MSYSTEM
# or, if cross-compiling from Linux, set it to usr/x86_64-w64-mingw32
export PATH="/${MSYS_TOOLCHAIN}/bin:$PATH"

# grab deps of a dll or exe and place them in the current dir
deps() {
    # string parsing is fun
    objdump -p "$1" | awk '/DLL Name:/ {print $3}' | while read -r dll; do
        [ -z "$dll" ] && continue

        # bin directory is used for DLLs, so we can do a quick "hack"
        # and use command to find the path of the DLL
        dllpath=$(command -v "$dll" 2>/dev/null || true)

        [ -z "$dllpath" ] && continue

        # explicitly exclude system32/syswow64 deps
        # these aren't needed to be bundled, as all systems already have them
        case "$dllpath" in
            *System32* | *SysWOW64*) continue ;;
        esac

        # avoid copying deps multiple times
        if [ ! -f "$dll" ]; then
            echo "$dllpath"
            cp "$dllpath" "$dll"

            # also grab the dependencies of the dependent DLL; e.g.
            # double-conversion is a dep of Qt6Core.dll but NOT eden.exe
            deps "$dllpath"
        fi
    done
}

# NB: must be done in a directory containing eden.exe
deps eden.exe

# deploy Qt plugins and such
windeployqt6 --no-compiler-runtime --no-opengl-sw --no-system-dxc-compiler \
    --no-system-d3d-compiler eden.exe

# grab deps for Qt plugins
find ./*/ -name "*.dll" | while read -r dll; do deps "$dll"; done
```