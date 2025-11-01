Package: boost-cmake:x64-windows@1.88.0

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.29.30159.0
- CMake Version: 3.30.1
-    vcpkg-tool version: 2025-10-16-71538f2694db93da4668782d094768ba74c45991
    vcpkg-scripts version: e3ed41868d 2025-10-31 (22 hours ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
CMake Error at C:/Users/charl/Documents/SuyuEclipse/vcpkg_installed/x64-windows/share/vcpkg-boost/vcpkg-port-config.cmake:3 (include):
  include could not find requested file:

    C:/Users/charl/Documents/SuyuEclipse/vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg-port-config.cmake
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
    "boost-algorithm",
    "boost-asio",
    "boost-bind",
    "boost-config",
    "boost-container",
    "boost-context",
    "vcpkg-cmake",
    "vcpkg-cmake-config",
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
    "zstd"
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
    }
  ]
}

```
</details>
