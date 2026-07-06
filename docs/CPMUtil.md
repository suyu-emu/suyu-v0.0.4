# CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

- [CPMUtil](#cpmutil)
  - [Global Options](#global-options)
  - [About](#about)
  - [Common Properties](#common-properties)
  - [Standard Packages](#standard-packages)
    - [Versioning](#versioning)
    - [Artifact Naming Errata](#artifact-naming-errata)
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
  - [Addendum: Package-Specific Overrides](#addendum-package-specific-overrides)
  - [Addendum: Supply-Chain Security](#addendum-supply-chain-security)
    - [Checksumming](#checksumming)
    - [Caching](#caching)
    - [Immutable Commit Hashes](#immutable-commit-hashes)

## Global Options

- `CPMUTIL_FORCE_SYSTEM` (default `OFF`): Require all CPM dependencies to use system packages.
  - You may optionally override this for each package: [Package-Specific Overrides](#addendum-package-specific-overrides)
- `CPMUTIL_FORCE_BUNDLED` (default `ON` on MSVC and Android, `OFF` elsewhere): Require all CPM dependencies to use bundled packages.
  - You may optionally override this for each package: [Package-Specific Overrides](#addendum-package-specific-overrides)
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
{
  "fmt": {
      "repo": "fmtlib/fmt",
      "version": "12.1.0",
      "hash": "f0da8...23f2",
      "min_version": "8",
      "options": [
          "FMT_TEST ON"
      ],
      "patches": [
         "0001-disable-reference-copy.patch"
      ]
  }
}
```

Calling `AddJsonPackage(fmt)`:

- Searches for a system package named `fmt` of version 8 or higher (`find_package(fmt 8)`)
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
- `version`: The version of the package. This is required.
  - Generally, this should be a 10-wide Git commit hash or Git tag.
  - Tags must be fully qualified and include prefixes and suffixes, e.g. `boost-1.88.0`
- `git_host`: The Git host the package is stored in, if applicable. Defaults to `github.com`.

## Standard Packages

Normal packages, like the prior `fmt` example, *must* also define:

- `hash`: The SHA512 hash of the downloaded artifact. CPMUtil generally computes this for you--if not, use `tools/cpmutil.sh package hash <JSON key>`
- `min_version`: The minimum required version of the package, if a system package is desired.

And may optionally define:

- `url`: Download from a raw URL.
- `artifact`: A GitHub/Forgejo/Gitea release artifact. Requires `repo` to be set and valid.
- `numeric_version`: Replaces `%NUMERIC_VERSION%` in artifact and version definitions; see [Artifact Naming Errata](#artifact-naming-errata).

See [Versioning](#versioning) for version/artifact information.

The following are optional to define:

- `source_subdir`: A subdirectory containing the `CMakeLists.txt` to configure a project. Useful for projects like `zstd`.
- `bundled`: Force the usage of a bundled package. Useful for packages where the system package is broken or nonexistent; e.g. external fragment shaders or data archives.
  - Note that this will conflict with `CPMUTIL_FORCE_SYSTEM`; for this reason, when using non-library archives, it may be best to allow the user to download and extract the archive manually and specify a local directory to it.
- `find_args`: Additional arguments passed to `find_package`, e.g. `MODULE`
- `patches`: Array of in-tree patches to apply to the downloaded source code. See [#Patches](TODO).
- `options`: Array of CMake options to apply before configuring the package, e.g. `"FMT_TEST ON"`.

### Versioning

When fetching from a Git repository, there are generally three methods of versioning:

- Commit hashes
- Git tags
- Release artifacts

When `repo` is set, `version` field can be set to any commitish value, including commit hashes or Git tags. In the case of artifacts, `version` must be a Git tag, and `artifact` must be set to a release artifact attached to that tag. Many repositories intentionally version the filenames of their release artifacts; for this purpose, CPMUtil allows you to implant the version number into the artifact name. To do so, add `%VERSION%` to the artifact name; CPMUtil will then automatically replace `%VERSION%` with the `version` field. This means that when changing versions, you only need to update the version, not the artifact!

Take Boost as an example. The artifact for Boost 1.90.0 is `boost-1.90.0-cmake.tar.xz`, and the tag is `boost-1.90.0`. Thus, we can set the artifact to `%VERSION%-cmake.tar.xz`:

```json
"boost": {
    "repo": "boostorg/boost",
    "version": "boost-1.90.0",
    "artifact": "%VERSION%-cmake.tar.xz"
}
```

The artifact will then evaluate as `boost-1.90.0-cmake.tar.xz`.

### Artifact Naming Errata

While `%VERSION%` replacement is generally good enough for well-packaged projects, occasionally there may be some problematic packages. Take, for instance, Vulkan Validation Layers:

- Tag (`version`): `vulkan-sdk-1.4.341.0`
- Artifact: `android-binaries-1.4.341.0.zip`

Attempting to add version replacement to the artifact definition **would not work here!** In this case, you must utilize the `numeric_version` field described earlier; we would set `numeric_version` to `1.4.341.0` and add `%NUMERIC_VERSION%` replacements into our artifact and version fields:

```json
"vulkan-validation-layers": {
    "artifact": "android-binaries-%NUMERIC_VERSION%.zip",
    "repo": "KhronosGroup/Vulkan-ValidationLayers",
    "version": "vulkan-sdk-%NUMERIC_VERSION%",
    "numeric_version": "1.4.341.0"
},
```

`artifact` will thus evaluate to `android-binaries-1.4.341.0.zip`, and `version` to `vulkan-sdk-1.4.341.0`. CPMUtil's auto-updater will also account for this and only update `numeric_version`!

### Patches

CPMUtil is able to apply in-place source tree patches to downloaded packages. These are defined in JSON as an array of names, preferably using `git-format-patch`'s scheme of `<4 digit number>-patch-name.patch`.

They are stored in `<CPMUTIL_PATCH_DIR>/<json-key>` (remember that `CPMUTIL_PATCH_DIR` defaults to `$ROOT/.patch`); e.g. `boost` patches would be in `.patch/boost`. Let's say we've made three patches and want to add them; in the Boost JSON definition, we would add:

```json
"patches": [
  "0001-fix-clang-cl-compilation.patch",
  "0002-fix-msvc-arm64-compilation.patch",
  "0003-fix-bsd-linking.patch"
]
```

Then, when Boost is downloaded, it will apply these patches to the source tree in the order they are defined (compound/dependent patches are okay!). Note that when you add, remove, or modify patches, CPMUtil will invalidate your downloaded cache and re-fetch the source.

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

Within these subdirectories, additional directories are created for each individual version, corresponding directly to their `version` field. CI packages use `<platform>-<architecture>-<version>` unconditionally.

To see the cache directory for a given package, use `tools/cpmutil.sh package dir <JSON key>`.

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

Alternatively, if CPMUtil pulls in a package that has no suitable way to install or use a system version, download it separately and pass `-D<PackageName>_CUSTOM_DIR=/path/to/downloaded/dir`.

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
    "version": "v1.4.342"
},
"vulkan-utility-libraries": {
    "repo": "KhronosGroup/Vulkan-Utility-Libraries",
    "package": "VulkanUtilityLibraries",
    "version": "v1.4.342"
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

## Addendum: Package-Specific Overrides

There are three variables that CPMUtil defines for each package; these can be overriden by the user or in your CMake. `package` refers either to the `package` value in the JSON, or the package's JSON key if unset (see `package` in [Common Properties](#common-properties)):

- `<package>_FORCE_BUNDLED`: Forcefully bundle the package. This has the same effect as `CPMUTIL_FORCE_BUNDLED`, but only for this package.
- `<package>_FORCE_BUNDLED`: Forcefully use the system package, failing if it can't be found. This has the same effect as `SYSTEM`, but only for this package.
- `<package>_CUSTOM_DIR`: Path to an extracted copy of the package. CPMUtil will not attempt to download the package and will instead use the custom directory.
  - For an example, see [CPMUtil's test case](https://git.crueter.xyz/CMake/CPMUtil/src/branch/master/tests/dir/CMakeLists.txt)

Additionally, in CMake, you can add `FORCE_BUNDLED_PACKAGE ON` to your `AddJsonPackage` command--note that you will have to use the `NAME <key>` syntax as described in [Module Path Packages](#addendum-module-path-packages). This will overrule *all* other overrides, including `CPMUTIL_FORCE_SYSTEM` and `<package>_FORCE_SYSTEM`--use with caution!

## Addendum: Supply-Chain Security

Many package managers suffer from the issue of supply chain security, specifically in regards to silent overwrites of existing packages or archives, e.g. tag sliding and artifact overwriting. CPMUtil has three methods to protect against this.

### Checksumming

CPMUtil *requires* SHA512 checksums for standard packages, and soon will for CI packages as well. If an attacker or compromised account slides a tag, overwrites a release artifact, or otherwise attempts to compromise anything that CPMUtil may fetch, **CPMUtil will not allow the download to continue!** This means that consumers of your build system can **only** download an artifact if the contents of the artifact are *exactly* indentical to what it was when it was configured--any changes at all will be rejected by CPMUtil.

### Caching

CPMUtil uses a mutable cache system, stored by default in `.cache/cpm`. Dependencies are downloaded and extracted here, and can be reused infinitely. This means that, for instance, if a package is compromised but you already have a cached local copy, you won't have to worry at all!

### Immutable Commit Hashes

CPMUtil is capable of using immutable Git commit hashes for its artifacts. These are (barring SHA1 collisions) completely immune to supply chain attacks--that is, unless the entire root server gets compromised to serve infected artifacts/source code; at which point there are much larger issues to worry about. This means that once you set a package to use a Git commit hash for its version, **it will stay the same forever**. This is useful if you want to ensure that consumers are never faced with download failures stemming from hash mismatches in case of compromised artifacts.

Do note, however, that this will render the package incompatible with CPMUtil's built-in auto-updater, so you will have to manually update the package.
