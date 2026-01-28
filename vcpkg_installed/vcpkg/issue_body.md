Package: dynarmic:x64-windows@6.7.2

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.29.30159.0
- CMake Version: 3.30.1
-    vcpkg-tool version: 2025-10-16-71538f2694db93da4668782d094768ba74c45991
    vcpkg-scripts version: e3ed41868d 2025-10-31 (3 months ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
-- Using cached yuzu-mirror-dynarmic-ba8192d.tar.gz
-- Cleaning sources at C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/buildtrees/dynarmic/src/ba8192d-d65af4d927.clean. Use --editable to skip cleaning for the packages you specify.
-- Extracting source C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/downloads/yuzu-mirror-dynarmic-ba8192d.tar.gz
-- Using source at C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/buildtrees/dynarmic/src/ba8192d-d65af4d927.clean
-- Configuring x64-windows
CMake Error at scripts/cmake/vcpkg_execute_required_process.cmake:127 (message):
    Command failed: C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/downloads/tools/ninja/1.13.1-windows/ninja.exe -v
    Working Directory: C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/buildtrees/dynarmic/x64-windows-rel/vcpkg-parallel-configure
    Error code: 1
    See logs for more information:
      C:\Users\charl\Documents\SuyuEclipse\externals\vcpkg\buildtrees\dynarmic\config-x64-windows-err.log

Call Stack (most recent call first):
  C:/Users/charl/Documents/SuyuEclipse/vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg_cmake_configure.cmake:252 (vcpkg_execute_required_process)
  C:/Users/charl/Documents/SuyuEclipse/vcpkg-overlays/dynarmic/portfile.cmake:9 (vcpkg_cmake_configure)
  scripts/ports.cmake:206 (include)



```

<details><summary>C:\Users\charl\Documents\SuyuEclipse\externals\vcpkg\buildtrees\dynarmic\config-x64-windows-err.log</summary>

```
ninja: error: build.ninja:5: bad $-escape (literal $ must be written as $$)

```
</details>

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "suyu",
  "version": "1.0",
  "builtin-baseline": "a42af01b72c28a8e1d7b48107b33e4f286a55ef6",
  "dependencies": [
    "cpp-httplib",
    "cpp-jwt",
    "cubeb",
    "dynarmic",
    "enet",
    "fmt",
    "libusb",
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
    "boost-variant"
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
      "version": "3.3.1"
    },
    {
      "name": "fmt",
      "version": "10.1.1"
    }
  ]
}

```
</details>
