Package: boost-cmake:x64-linux@1.88.0

**Host Environment**

- Host: x64-linux
- Compiler: GNU 13.3.0
- CMake Version: 3.30.1
-    vcpkg-tool version: 2025-10-16-71538f2694db93da4668782d094768ba74c45991
    vcpkg-scripts version: e3ed41868d 2025-10-31 (3 days ago)

**To Reproduce**

`vcpkg install --clean-after-build`

**Failure logs**

```
CMake Error at /workspaces/SuyuEclipse/vcpkg_installed/x64-linux/share/vcpkg-boost/vcpkg-port-config.cmake:3 (include):
  include could not find requested file:

    /workspaces/SuyuEclipse/vcpkg_installed/x64-linux/share/vcpkg-cmake/vcpkg-port-config.cmake
Call Stack (most recent call first):
  scripts/ports.cmake:199 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "suyu",
  "version": "1.0",
  "dependencies": [
    "vcpkg-cmake",
    "vcpkg-cmake-config",
    "cpp-httplib",
    "cpp-jwt",
    "cubeb",
    "dynarmic",
    "enet",
    "fmt",
    "libusb",
    "llvm",
    "lz4",
    "nlohmann-json",
    "opus",
    "renderdoc-api",
    "simpleini",
    "stb",
    "vulkan-memory-allocator",
    "xbyak",
    "zlib",
    "zstd",
    "boost-algorithm",
    "boost-asio",
    "boost-bind",
    "boost-config",
    "boost-container",
    "boost-context",
    "boost-crc",
    "boost-functional",
    "boost-heap",
    "boost-icl",
    "boost-intrusive",
    "boost-mpl",
    "boost-process",
    "boost-range",
    "boost-spirit",
    "boost-test",
    "boost-timer",
    "boost-variant",
    "boost-cobalt"
  ],
  "features": {
    "suyu-tests": {
      "description": "Compile tests",
      "dependencies": [
        {
          "name": "catch2"
        }
      ]
    },
    "web-service": {
      "description": "Enable web services (telemetry, etc.)",
      "dependencies": [
        {
          "name": "openssl",
          "platform": "windows"
        }
      ]
    },
    "android": {
      "description": "Enable Android dependencies",
      "dependencies": [
        {
          "name": "oboe",
          "platform": "android"
        }
      ]
    }
  },
  "overrides": [
    {
      "name": "vcpkg-cmake",
      "version": "2024-04-23"
    },
    {
      "name": "vcpkg-cmake-config",
      "version": "2024-05-23"
    },
    {
      "name": "catch2",
      "version": "3.4.0"
    },
    {
      "name": "fmt",
      "version": "10.1.1"
    },
    {
      "name": "llvm",
      "version": "17.0.6"
    },
    {
      "name": "boost-algorithm",
      "version": "1.88.0"
    },
    {
      "name": "boost-asio",
      "version": "1.88.0"
    },
    {
      "name": "boost-bind",
      "version": "1.88.0"
    },
    {
      "name": "boost-config",
      "version": "1.88.0"
    },
    {
      "name": "boost-container",
      "version": "1.88.0"
    },
    {
      "name": "boost-context",
      "version": "1.88.0"
    },
    {
      "name": "boost-crc",
      "version": "1.88.0"
    },
    {
      "name": "boost-functional",
      "version": "1.88.0"
    },
    {
      "name": "boost-heap",
      "version": "1.88.0"
    },
    {
      "name": "boost-icl",
      "version": "1.88.0"
    },
    {
      "name": "boost-intrusive",
      "version": "1.88.0"
    },
    {
      "name": "boost-mpl",
      "version": "1.88.0"
    },
    {
      "name": "boost-process",
      "version": "1.88.0"
    },
    {
      "name": "boost-range",
      "version": "1.88.0"
    },
    {
      "name": "boost-spirit",
      "version": "1.88.0"
    },
    {
      "name": "boost-test",
      "version": "1.88.0"
    },
    {
      "name": "boost-timer",
      "version": "1.88.0"
    },
    {
      "name": "boost-variant",
      "version": "1.88.0"
    },
    {
      "name": "boost-cobalt",
      "version": "1.88.0"
    }
  ]
}

```
</details>
