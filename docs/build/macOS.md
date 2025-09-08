Please note this article is intended for development, and Eden on macOS is not currently ready for regular use.

This article was written for developers. Eden support for macOS is not ready for casual use.

## Dependencies
Install dependencies from Homebrew:
```sh
brew install autoconf automake boost ffmpeg fmt glslang hidapi libtool libusb lz4 ninja nlohmann-json openssl pkg-config qt@6 sdl2 speexdsp zlib zstd cmake Catch2 molten-vk vulkan-loader spirv-tools
```

If you are compiling on Intel Mac, or are using a Rosetta Homebrew installation, you must replace all references of `/opt/homebrew` with `/usr/local`.

Now, clone the repo:
```sh
git clone --recursive https://git.eden-emu.dev/eden-emu/eden
cd eden
```

## Method I: ninja

---
Build for release
```sh
export Qt6_DIR="/opt/homebrew/opt/qt@6/lib/cmake"
export LIBVULKAN_PATH=/opt/homebrew/lib/libvulkan.dylib
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_TESTS=OFF -DENABLE_WEB_SERVICE=ON -DENABLE_LIBUSB=OFF -DCLANG_FORMAT=ON -DSDL2_DISABLE_INSTALL=ON -DSDL_ALTIVEC=ON
ninja
```

You may also want to include support for Discord Rich Presence by adding `-DUSE_DISCORD_PRESENCE=ON`
```sh
export Qt6_DIR="/opt/homebrew/opt/qt@6/lib/cmake"
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_TESTS=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_LIBUSB=OFF
ninja
```

Run the output:
```
bin/eden.app/Contents/MacOS/eden
```

## Method II: Xcode

---
Build for release
```sh
export Qt6_DIR="/opt/homebrew/opt/qt@6/lib/cmake"
export LIBVULKAN_PATH=/opt/homebrew/lib/libvulkan.dylib
# Only if having errors about Xcode 15.0
sudo /usr/bin/xcode-select --switch /Users/admin/Downloads/Xcode.ap
cmake -B build -GXcode -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_TESTS=OFF -DENABLE_WEB_SERVICE=ON -DENABLE_LIBUSB=OFF -DCLANG_FORMAT=ON -DSDL2_DISABLE_INSTALL=ON -DSDL_ALTIVEC=ON
xcodebuild build -project yuzu.xcodeproj -scheme "yuzu" -configuration "RelWithDebInfo"
```

Build with debug symbols:
```sh
export Qt6_DIR="/opt/homebrew/opt/qt@6/lib/cmake"
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU_TESTS=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_LIBUSB=OFF
ninja
```

Run the output:
```
bin/eden.app/Contents/MacOS/eden
```

---

To run with MoltenVK, install additional dependencies:
```sh
brew install molten-vk vulkan-loader
```

Run with Vulkan loader path:
```sh
export LIBVULKAN_PATH=/opt/homebrew/lib/libvulkan.dylib
bin/eden.app/Contents/MacOS/eden
```
