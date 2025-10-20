# CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

Global Options:

- `CPMUTIL_FORCE_SYSTEM` (default `OFF`): Require all CPM dependencies to use system packages. NOT RECOMMENDED!
  * You may optionally override this (section)
- `CPMUTIL_FORCE_BUNDLED` (default `ON` on MSVC and Android, `OFF` elsewhere): Require all CPM dependencies to use bundled packages.

You are highly encouraged to read AddPackage first, even if you plan to only interact with CPMUtil via `AddJsonPackage`.

<!-- TOC -->
- [AddPackage](#addpackage)
- [AddCIPackage](#addcipackage)
- [AddJsonPackage](#addjsonpackage)
- [Lists](#lists)
<!-- /TOC -->

## AddPackage

The core of CPMUtil is the [`AddPackage`](./AddPackage.md) function. [`AddPackage`](./AddPackage.md) itself is fully CMake-based, and largely serves as an interface between CPM and the rest of CPMUtil.

## AddCIPackage

[`AddCIPackage`](./AddCIPackage.md) adds a package that follows [crueter's CI repository spec](https://github.com/crueter-ci).

## AddJsonPackage

[`AddJsonPackage`](./AddJsonPackage.md) is the recommended method of usage for CPMUtil.

## Lists

CPMUtil will create three lists of dependencies where `AddPackage` or similar was used. Each is in order of addition.

- `CPM_PACKAGE_NAMES`: The names of packages included by CPMUtil
- `CPM_PACKAGE_URLS`: The URLs to project/repo pages of packages
- `CPM_PACKAGE_SHAS`: Short version identifiers for each package
    * If the package was included as a system package, ` (system)` is appended thereafter
    * Packages whose versions can't be deduced will be left as `unknown`.

For an example of how this might be implemented in an application, see Eden's implementation:

- [`dep_hashes.h.in`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/dep_hashes.h.in)
- [`GenerateDepHashes.cmake`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/CMakeModules/GenerateDepHashes.cmake)
- [`deps_dialog.cpp`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/yuzu/deps_dialog.cpp)