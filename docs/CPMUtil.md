# CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

- [CPMUtil](#cpmutil)
  - [Global Options](#global-options)
  - [About](#about)
  - [Common Properties](#common-properties)
  - [Standard Packages](#standard-packages)
    - [Versioning](#versioning)
    - [Patches](#patches)
  - [Pre-built CI Packages](#pre-built-ci-packages)
  - [Usage](#usage)
  - [Addendum: Cache Storage](#addendum-cache-storage)
  - [Addendum: Making Patches](#addendum-making-patches)
  - [Addendum: Package Identification Lists](#addendum-package-identification-lists)
  - [Addendum: Notes for Packagers](#addendum-notes-for-packagers)
    - [Network Sandbox](#network-sandbox)
    - [Unsandboxed](#unsandboxed)
  - [Addendum: Dependent Packages](#addendum-dependent-packages)
    - [Example: Vulkan](#example-vulkan)
  - [Addendum: Module Path Packages](#addendum-module-path-packages)
    - [Example: OpenSSL](#example-openssl)
  - [Addendum: Adding Qt](#addendum-adding-qt)

## Global Options

- `CPMUTIL_FORCE_SYSTEM` (default `OFF`): Require all CPM dependencies to use system packages. NOT RECOMMENDED!
  - You may optionally override this (section)
- `CPMUTIL_FORCE_BUNDLED` (default `ON` on MSVC and Android, `OFF` elsewhere): Require all CPM dependencies to use bundled packages.
- `CPMUTIL_PATCH_DIR` (default `${PROJECT_SOURCE_DIR}/.patch`): Path to patches used in packages. Stored as `<PATCH DIR>/json-package-name/0001-patch-name.patch`, etc.
- `CPM_SOURCE_CACHE` (default `${PROJECT_SOURCE_DIR}/.cache/cpm`): Where downloaded dependencies get stored.

## About

CPMUtil works by defining dependencies in a JSON file, `cpmfile.json`, and calling `AddJsonPackage`. These dependencies generally must define, at minimum:

- The repository and Git host
- A release artifact, commit, or tag archive to download
- A SHA512 sum for the downloaded artifact

And may optionally define other properties like:

- The minimum version for system packages
- The package name used for system packages (this defaults to the json key if undefined)
- In-tree source patches
- Options passed to CMake
- Options passed to find_package

For instance:

```json
"fmt": {
    "repo": "fmtlib/fmt",
    "tag": "12.1.0",
    "hash": "f0da82c545b01692e9fd30fdfb613dbb8dd9716983dcd0ff19ac2a8d36f74beb5540ef38072fdecc1e34191b3682a8542ecbf3a61ef287dbba0a2679d4e023f2",
    "min_version": "8",
    "options": [
      "FMT_TEST ON"
    ],
    "patches": [
      "0001-disable-reference-copy.patch"
    ]
}
```

Calling `AddJsonPackage(fmt)`:

- Searches for a system package named `fmt` of version 8 or higher
- If found, uses the system package and caches it for future use
- If not found:
  - Downloads fmt 12.1.0 from the GitHub Archive into `.cache/cpm/fmt/12.1.0`
  - Verifies the hash
  - Applies the `0001-disable-reference-copy.patch` patch to the source tree
  - Sets `FMT_TEST` to `ON`
  - Adds the downloaded directory to CMake
  - Now, future `find_package(fmt)` calls will use the downloaded package

There are two types of packages CPMUtil can define: standard and prebuilt CI packages. Some properties are common to both types, however.

## Common Properties

These JSON properties are used by standard and CI packages alike.

- `package`: The package name used by `find_package` to check for the existence of a system package.
  - If unset, defaults to the JSON key
- `repo`: The Git repository the package is stored in, if applicable.
- `version`: The version of the package to download. This is required.
- `min_version`: The minimum required version of the package, if a system package is desired.
- `git_host`: The Git host the package is stored in, if applicable. Defaults to `github.com`.

## Standard Packages

Normal packages, like the prior `fmt` example, *must* also define:

- `hash`: The SHA512 hash of the downloaded artifact. CPMUtil generally computes this for you.
- A valid version/URL identifier:
  - `url`: Download from a raw URL.
  - `sha`: A short or fully-qualified Git commit sha. CPMUtil recommends using 10-character wide shas.
  - `tag`: A Git tag. See [Versioning](#versioning) for its relation to `version`.
  - `artifact`: A GitHub/Forgejo/Gitea release artifact (requires `tag`). See [Versioning](#versioning) for its relation to `tag` and `version`.

The following are optional to define:

- `source_subdir`: A subdirectory containing the `CMakeLists.txt` to configure a project. Useful for projects like `zstd`.
- `bundled`: Force the usage of a bundled package. Useful for packages where the system package is broken or nonexistent; e.g. including external fragment shaders.
- `find_args`: Additional arguments passed to `find_package`, e.g. `MODULE`
- `patches`: Array of in-tree patches to apply to the downloaded source code. See [#Patches](TODO).
- `options`: Array of CMake options to apply before configuring the package, e.g. `"FMT_TEST ON"`.

### Versioning

When using tags or artifacts, it may be cumbersome to repeat the version multiple times; especially if it's constantly changing. For this purpose, `tag` and `artifact` both support basic version text replacement.

`tag` can use `%VERSION%` to have its version replaced with the `version` defined for the package, e.g. for OpenSSL; when downloading, `tag` will evaluate to `openssl-3.6.2`:

```json
"openssl": {
    "repo": "openssl/openssl",
    "version": "3.6.2",
    "tag": "openssl-%VERSION%"
}
```

`artifact` also supports `%VERSION%` replacement, and can also use `%TAG%` to be replaced by the computed tag. Take this Boost definition:

```json
"boost": {
    "repo": "boostorg/boost",
    "tag": "boost-%VERSION%",
    "version": "1.90.0"
}
```

Boost's artifact for this version is stored in `boost-1.90.0-cmake.tar.xz`. Notice that the computed tag,`boost-1.90.0`, is in the name of the artifact! Thus, `artifact` can be either:

- `boost-%VERSION%-cmake.tar.xz`
- Or, even simpler: `%TAG%-cmake.tar.xz`

Future updates need only change the `version` identifier, and the artifact and tag will automatically be updated!

### Patches

CPMUtil is able to apply in-place source tree patches to downloaded packages. These are defined in JSON as an array of names, preferably using `git-format-patch`'s scheme of `<4 digit number>-patch-name.patch`. These are stored in `<CPMUTIL_PATCH_DIR>/<json-key>` (remember that `CPMUTIL_PATCH_DIR` defaults to `$ROOT/.patch`); e.g. `boost` patches would be in `.patch/boost`. Let's say we've made three patches and want to add them; in the Boost JSON definition, we would add:

```json
"patches": [
  "0001-fix-clang-cl-compilation.patch",
  "0002-fix-msvc-arm64-compilation.patch",
  "0003-fix-bsd-linking.patch"
]
```

Then, when Boost is downloaded, it will apply these patches in order to the source tree.

To learn how to make patches, see [Addendum: Making Patches](#addendum-making-patches).

## Pre-built CI Packages

The definition and usage of CI packages is subject to change in the very near future.

CI packages are, in essence, prebuilt binary distributions for libraries. They exist for a few reasons:

- Creating static libraries for system packages that normally lack them, e.g. Qt/SDL
- Reducing duplicated compilation effort on rarely-changing externals, e.g. SDL
- Creating debloated prebuilt packages specifically for your project to reduce binary size, e.g. FFmpeg

CPMUtil is specifically designed to work with a small subset of prebuilt CI packages; namely, those that follow the format of the [crueter-ci spec](https://github.com/crueter-ci/spec/blob/master/README.md).

To use them, you must add `ci: true` to your package definition. Alongside the common properties, CI packages define the following:

- `name`: The artifacts' name prefix (required), e.g. `openssl`
- `extension`: The artifacts' extension (default `tar.zst`)
- `disabled_platforms`: CPMUtil-supported platforms that are not built into this repository.
  - This is subject to change.

**Note that `package` is subject to removal here.**

Example:

```json
"sdl2": {
    "repo": "crueter-ci/SDL2",
    "package": "SDL2",
    "min_version": "2.26.4",
    "ci": true,
    "version": "2.32.10-a65111bd2d",
    "artifact": "SDL2"
},
```

## Usage

Once you've defined your package in `cpmfile.json`, simply call `AddJsonPackage(<JSON key>)` and go from there! Specific instructions differ between individual packages, so you're on your own from here.

If you're only concerned with basic usage, you can stop reading. For more advanced use cases and package management, read these addenda.

## Addendum: Cache Storage

CPMUtil stores downloaded packages within `.cache/cpm` by default (see `CPM_SOURCE_CACHE`). Subdirectories stored within are lowercase representations of the `find_package` name for the package; for instance, a `vulkan-headers` definition with `package: "VulkanHeaders"` would be stored in `.cache/cpm/vulkanheaders`.

Within these subdirectories, additional directories are created for each individual version:

- A four-character shorthand of `sha`, if defined
- If `sha` is not defined, the fully qualified `version` is used

CI packages use `<platform>-<architecture>-<version>` unconditionally.

## Addendum: Making Patches

CPMUtil has a dedicated command for making patches. You're recommended to have Git and a command line editor installed, but CPMUtil is able to work without either. To do so, follow these steps, noting your package's JSON key:

- Clean-fetch your package: `tools/cpmutil.sh package reset <package>`
- Make any necessary modifications to the package source.
  - You can access the package source directory via `tools/cpmutil.sh package dir <package>`.
- Create the patch: `tools/cpmutil.sh package patch <package>`
  - Follow the on-screen prompts. If you have Git installed, an editor will be opened so you can type your commit message. If not, just type a one-line description.

And you're done! CPMUtil will automatically create and name the patch, and add it to the list of patches in the JSON definition.

## Addendum: Package Identification Lists

CPMUtil will create three lists of dependencies where `AddPackage` or similar was used. Each is in order of addition.

- `CPM_PACKAGE_NAMES`: The names of packages included by CPMUtil
- `CPM_PACKAGE_URLS`: The URLs to project/repo pages of packages
- `CPM_PACKAGE_SHAS`: Short version identifiers for each package
  - If the package was included as a system package, `(system)` is appended thereafter
  - Packages whose versions can't be deduced will be left as `unknown`.

For an example of how this might be implemented in an application, see Eden's implementation:

- [`dep_hashes.h.in`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/dep_hashes.h.in)
- [`GenerateDepHashes.cmake`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/CMakeModules/GenerateDepHashes.cmake)
- [`deps_dialog.cpp`](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/yuzu/deps_dialog.cpp)

## Addendum: Notes for Packagers

If you are packaging a project that uses CPMUtil, read this!

### Network Sandbox

For sandboxed environments (e.g. Gentoo, nixOS) you must install all dependencies to the system beforehand and set `-DCPMUTIL_FORCE_SYSTEM=ON`. If a dependency is missing, get creating!

Alternatively, if CPMUtil pulls in a package that has no suitable way to install or use a system version, download it separately and pass `-DPackageName_DIR=/path/to/downloaded/dir` (e.g. shaders)

### Unsandboxed

For others (AUR, MPR, etc). CPMUtil will handle everything for you, including if some of the project's dependencies are missing from your distribution's repositories. See [`eden-git`](https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=eden-git) for an example.

## Addendum: Dependent Packages

Consider the following scenario: the Vulkan Headers and Vulkan Utility libraries are both pulled in by my project. In order for both to compile cleanly, their versions *must* match. However, a user may have the Vulkan Headers installed, but *not* the Vulkan Utility Libraries! This can cause a version mismatch where the Utility Libraries expect a much newer version of the Vulkan Headers than the user has installed.

To solve this, CPMUtil has an `AddDependentPackages` command. This takes a list of JSON package keys that *must* either ALL be installed to the system, or ALL be bundled.

### Example: Vulkan

Using the prior Vulkan example:

```json
"vulkan-headers": {
    "repo": "KhronosGroup/Vulkan-Headers",
    "package": "VulkanHeaders",
    "min_version": "1.4.317",
    "version": "1.4.342",
    "tag": "v%VERSION%"
},
"vulkan-utility-libraries": {
    "repo": "KhronosGroup/Vulkan-Utility-Libraries",
    "package": "VulkanUtilityLibraries",
    "version": "1.4.342",
    "tag": "v%VERSION%"
}
```

In CMake:

```cmake
AddDependentPackages(vulkan-headers vulkan-utility-libraries)
```

Possible scenarios:

- The user has both Vulkan Headers and Vulkan Utility Libraries installed to the system, and both are new enough.
  - Configuration proceeds without issue.
- The user has neither installed, or has a too-old version of Vulkan Headers installed
  - Configuration proceeds without issue.
- The user has a valid Vulkan Headers installed, but not Vulkan Utility Libraries.
  - CPMUtil instructs the user to either force bundled Vulkan Headers, or install Vulkan Utility Libraries.
- The user has both installed, but Vulkan Headers are too old.
  - CPMUtil instructs the user to install a valid version of Vulkan Headers, or force bundled Vulkan Utility Libraries.

## Addendum: Module Path Packages

Sometimes, a prebuilt CI package may be packed in such a way that it's meant to be used in the context of a system install (e.g. pkgconfig or CMakeConfig files). In this case, CPMUtil normally will be unable to configure the downloaded subdirectory. To solve this, you can use `AddJsonPackage`'s `MODULE_PATH` mode, which adds the downloaded source directory to the `CMAKE_MODULE_PATH`.

### Example: OpenSSL

Say an OpenSSL CI package is packed to contain its CMake config files rather than a root `CMakeLists.txt`; in this case, you would call:

```cmake
AddJsonPackage(NAME openssl MODULE_PATH)
```

The `NAME` argument is also required, as the parsing is different from the standard single-argument function signature.

From here, calling `find_package(OpenSSL)` will use the bundled OpenSSL.

## Addendum: Adding Qt

If you'd like to use customized Qt builds, CPMUtil provides a convenience function that allows you to add Qt builds. This usage and setup is subject to change.

See [crueter-ci/Qt](https://github.com/crueter-ci/Qt) for an example of how one might build customized Qt. To add a Qt build to your project, use `AddQt(<repository> <version>)`, e.g.:

```cmake
AddQt(QDash-CI/Qt 6.11.1)
```

Then, call `find_package(Qt6 ...)` and it will pull Qt from your downloaded source.
