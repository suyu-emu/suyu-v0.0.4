# Caveats

## Arch Linux

- httplib AUR package is broken. Set `httplib_FORCE_BUNDLED=ON` if you have it installed.
- Eden is also available as an [AUR package](https://aur.archlinux.org/packages/eden-git). If you are unable to build, either use that or compare your process to the PKGBUILD.

## Gentoo Linux

Do not use the system sirit or xbyak packages.

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

## OpenBSD

After configuration, you may need to modify `externals/ffmpeg/CMakeFiles/ffmpeg-build/build.make` to use `-j$(nproc)` instead of just `-j`.

## FreeBSD

Eden is not currently available as a port on FreeBSD, though it is in the works. For now, the recommended method of usage is to compile it yourself.

The available OpenSSL port (3.0.17) is out-of-date, and using a bundled static library instead is recommended; to do so, add `-DYUZU_USE_BUNDLED_OPENSSL=ON` to your CMake configure command.