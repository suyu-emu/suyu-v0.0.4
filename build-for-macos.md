# This article was written for developers. yuzu support for macOS is not ready for casual use.

# NOTE: yuzu officially discontinued support for MacOS when Apple deprecated OpenGL... this is provided as-is but may not work.
---

Install dependencies from Homebrew:
```sh
brew install autoconf automake boost@1.76 ccache ffmpeg fmt glslang hidapi libtool libusb lz4 ninja nlohmann-json openssl pkg-config qt@5 sdl2 speexdsp zlib zlib zstd
```

Build with debug symbols (vcpkg is not currently used due to broken boost-context library):
```sh
mkdir build && cd build
export Qt5_DIR="/opt/homebrew/opt/qt@5/lib/cmake"
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_USE_BUNDLED_VCPKG=OFF -DYUZU_TESTS=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_LIBUSB=OFF
ninja
```

TODO: use newer vcpkg. This fixes the boost context library, and allows using the web service
TODO: figure out what's up with libusb
- TODO: use newer vcpkg. This fixes the boost context library, and allows using the web service
- TODO: figure out what's up with libusb

Run the output:
```
bin/yuzu.app/Contents/MacOS/yuzu
```

---

To run with MoltenVK, install additional dependencies:
```sh
brew install molten-vk vulkan-loader
```

Run with Vulkan loader path:
```sh
export LIBVULKAN_PATH=/opt/homebrew/lib/libvulkan.dylib
bin/yuzu.app/Contents/MacOS/yuzu
```