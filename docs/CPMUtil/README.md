# CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

Global Options:

- `CPMUTIL_FORCE_SYSTEM` (default `OFF`): Require all CPM dependencies to use system packages. NOT RECOMMENDED!
  * You may optionally override this (section)
- `CPMUTIL_FORCE_BUNDLED` (default `ON` on MSVC and Android, `OFF` elsewhere): Require all CPM dependencies to use bundled packages.

You are highly encouraged to read AddPackage first, even if you plan to only interact with CPMUtil via `AddJsonPackage`.

- [AddPackage](#addpackage)
- [AddCIPackage](#addcipackage)
- [AddJsonPackage](#addjsonpackage)
- [Lists](#lists)
- [For Packagers](#for-packagers)
  - [Network Sandbox](#network-sandbox)
  - [Unsandboxed](#unsandboxed)

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

## For Packagers

If you are packaging a project that uses CPMUtil, read this!

### Network Sandbox

For sandboxed environments (e.g. Gentoo, nixOS) you must install all dependencies to the system beforehand and set `-DCPMUTIL_FORCE_SYSTEM=ON`. If a dependency is missing, get creating!

### Unsandboxed

For others (AUR, MPR, etc). CPMUtil will handle everything for you, including if some of the project's dependencies are missing from your distribution's repositories. That is pretty much half the reason I created this behemoth, after all.