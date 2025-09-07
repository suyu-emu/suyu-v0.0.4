# Building for OpenBSD

```sh
pkg_add -u
pkg_add cmake nasm git boost unzip--iconv autoconf-2.72p0 bash ffmpeg glslang gmake llvm-19.1.7p3 qt6 jq
git --recursive https://git.eden-emu.dev/eden-emu/eden
cmake -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DDYNARMIC_USE_PRECOMPILED_HEADERS=OFF -DCMAKE_BUILD_TYPE=Debug -DENABLE_QT=OFF -DENABLE_OPENSSL=OFF -DENABLE_WEB_SERVICE=OFF -B /usr/obj/eden
```

- Modify `externals/ffmpeg/CMakeFiles/ffmpeg-build/build.make` to use `-j$(nproc)` instead of just `-j`.
