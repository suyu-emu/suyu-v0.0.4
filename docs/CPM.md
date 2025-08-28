# CPM

CPM (CMake Package Manager) is the preferred method of managing dependencies within Eden.

Global Options:

- `YUZU_USE_CPM` is set by default on MSVC and Android. Other platforms should use this if certain "required" system dependencies (e.g. OpenSSL) are broken or missing
  * If this is `OFF`, required system dependencies will be searched via `find_package`, although certain externals use CPM regardless.
- `CPMUTIL_FORCE_SYSTEM` (default `OFF`): Require all CPM dependencies to use system packages. NOT RECOMMENDED!
  * Many packages, e.g. mcl, sirit, xbyak, discord-rpc, are not generally available as a system package.
  * You may optionally override these (see CPMUtil section)
- `CPMUTIL_FORCE_BUNDLED` (default `ON` on MSVC and Android, `OFF` elsewhere): Require all CPM dependencies to use bundled packages.

## CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

### AddPackage

`AddPackage` is the core of the CPMUtil wrapper, and is generally the lowest level you will need to go when dealing with dependencies.

**Identification/Fetching**

- `NAME` (required): The package name (must be the same as the `find_package` name if applicable)
- `VERSION`: The minimum version of this package that can be used on the system
- `GIT_VERSION`: The version found within git, only used for identification
- `URL`: The URL to fetch.
- `REPO`: The GitHub repo to use (`owner/repo`).
  * Only GitHub is supported for now, though other platforms will see support at some point
- `TAG`: The tag to fetch, if applicable.
- `ARTIFACT`: The name of the artifact, if applicable.
- `SHA`: Commit sha to fetch, if applicable.
- `BRANCH`: Branch to fetch, if applicable.

The following configurations are supported, in descending order of precedence:

- `URL`: Bare URL download, useful for custom artifacts
  * If this is set, `GIT_URL` or `REPO` should be set to allow the dependency viewer to link to the project's Git repository.
  * If this is NOT set, `REPO` must be defined.
- `REPO + TAG + ARTIFACT`: GitHub release artifact
  * The final download URL will be `https://github.com/${REPO}/releases/download/${TAG}/${ARTIFACT}`
  * Useful for prebuilt libraries and prefetched archives
- `REPO + TAG`: GitHub tag archive
  * The final download URL will be `https://github.com/${REPO}/archive/refs/tags/${TAG}.tar.gz`
  * Useful for pinning to a specific tag, better for build identification
- `REPO + SHA`: GitHub commit archive
  * The final download URL will be `https://github.com/${REPO}/archive/${SHA}.zip`
  * Useful for pinning to a specific commit
- `REPO + BRANCH`: GitHub branch archive
  * The final download URL will be `https://github.com/${REPO}/archive/refs/heads/${BRANCH}.zip`
  * Generally not recommended unless the branch is frozen
- `REPO`: GitHub master archive
  * The final download URL will be `https://github.com/${REPO}/archive/refs/heads/master.zip`
  * Generally not recommended unless the project is dead

**Hashing**

Hashing is used for verifying downloads. It's highly recommended to use these.

- `HASH_ALGO` (default `SHA512`): Hash algorithm to use

Hashing strategies, descending order of precedence:

- `HASH`: Bare hash verification, useful for static downloads e.g. commit archives
- `HASH_SUFFIX`: Download the hash as `${DOWNLOAD_URL}.${HASH_SUFFIX}`
  * The downloaded hash *must* match the hash algorithm and contain nothing but the hash; no filenames or extra content.
- `HASH_URL`: Download the hash from a separate URL

**Additional Options**

- `KEY`: Custom cache key to use (stored as `.cache/cpm/${packagename_lower}/${key}`)
  * Default is based on, in descending order of precedence:
    - First 4 characters of the sha
    - `GIT_VERSION`, or `VERSION` if not specified
    - Tag
    - Otherwise, CPM defaults will be used. This is not recommended as it doesn't produce reproducible caches
- `DOWNLOAD_ONLY`: Whether or not to configure the downloaded package via CMake
  * Useful to turn `OFF` if the project doesn't use CMake
- `SOURCE_SUBDIR`: Subdirectory of the project containing a CMakeLists.txt file
- `FIND_PACKAGE_ARGUMENTS`: Arguments to pass to the `find_package` call
- `BUNDLED_PACKAGE`: Set to `ON` to force the usage of a bundled package
- `OPTIONS`: Options to pass to the configuration of the package
- `PATCHES`: Patches to apply to the package, stored in `.patch/${packagename_lower}/0001-patch-name.patch` and so on
- Other arguments can be passed to CPM as well

**Extra Variables**

For each added package, users may additionally force usage of the system/bundled package.

- `${package}_FORCE_SYSTEM`: Require the package to be installed on the system
- `${package}_FORCE_BUNDLED`: Force the package to be fetched and use the bundled version

**Bundled/System Switching**

Descending order of precedence:
- If `${package}_FORCE_SYSTEM` is true, requires the package to be on the system
- If `${package}_FORCE_BUNDLED` is true, forcefully uses the bundled package
- If `CPMUTIL_FORCE_SYSTEM` is true, requires the package to be on the system
- If `CPMUTIL_FORCE_BUNDLED` is true, forcefully uses the bundled package
- If the `BUNDLED_PACKAGE` argument is true, forcefully uses the bundled package
- Otherwise, CPM will search for the package first, and if not found, will use the bundled package

**Identification**

All dependencies must be identifiable in some way for usage in the dependency viewer. Lists are provided in descending order of precedence.

URLs:

- `GIT_URL`
- `REPO` as a GitHub repository
- `URL`

Versions (bundled):

- `SHA`
- `GIT_VERSION`
- `VERSION`
- `TAG`
- "unknown"

If the package is a system package, AddPackage will attempt to determine the package version and append ` (system)` to the identifier. Otherwise, it will be marked as `unknown (system)`

### AddCIPackage

Adds a package that follows crueter's CI repository spec.

- `VERSION` (required): The version to get (the tag will be `v${VERSION}`)
- `NAME` (required): Name used within the artifacts
- `REPO` (required): CI repository, e.g. `crueter-ci/OpenSSL`
- `PACKAGE` (required): `find_package` package name
- `EXTENSION`: Artifact extension (default `tar.zst`)
- `MIN_VERSION`: Minimum version for `find_package`. Only used if platform does not support this package as a bundled artifact
- `DISABLED_PLATFORMS`: List of platforms that lack artifacts for this package. One of:
  * `windows-amd64`
  * `windows-arm64`
  * `android`
  * `solaris`
  * `freebsd`
  * `linux`
  * `linux-aarch64`
- `CMAKE_FILENAME`: Custom CMake filename, relative to the package root (default `${PACKAGE_ROOT}/${NAME}.cmake`)

### AddJsonPackage

This is the recommended method of usage for CPMUtil. In each directory that utilizes `CPMUtil`, there must be a `cpmfile.json` that defines dependencies in a similar manner to the individual calls.

The cpmfile is an object of objects, with each sub-object being named according to the package's identifier, e.g. `openssl`, which can then be fetched with `AddJsonPackage(<identifier>)`. Options are designed to map closely to the argument names, and are always strings unless otherwise specified.

- `package` -> `NAME` (`PACKAGE` for CI), defaults to the object key
- `repo` -> `REPO`
- `version` -> `VERSION`
- `ci` (bool)

If `ci` is `false`:

- `hash` -> `HASH`
- `sha` -> `SHA`
- `tag` -> `TAG`
- `artifact` -> `ARTIFACT`
- `git_version` -> `GIT_VERSION`
- `source_subdir` -> `SOURCE_SUBDIR`
- `bundled` -> `BUNDLED_PACKAGE`
- `find_args` -> `FIND_PACKAGE_ARGUMENTS`
- `patches` -> `PATCHES` (array)
- `options` -> `OPTIONS` (array)

Other arguments aren't currently supported. If you wish to add them, see the `AddJsonPackage` function in `CMakeModules/CPMUtil.cmake`.

If `ci` is `true`:

- `name` -> `NAME`, defaults to the object key
- `extension` -> `EXTENSION`, defaults to `tar.zst`
- `min_version` -> `MIN_VERSION`
- `cmake_filename` -> `CMAKE_FILENAME`
- `extension` -> `EXTENSION`

### Examples

In order: OpenSSL CI, Boost (tag + artifact), discord-rpc (sha + options + patches), Opus (options + find_args)

```json
{
    "openssl": {
        "ci": true,
        "package": "OpenSSL",
        "name": "openssl",
        "repo": "crueter-ci/OpenSSL",
        "version": "3.5.2",
        "min_version": "1.1.1"
    },
    "boost": {
        "package": "Boost",
        "repo": "boostorg/boost",
        "tag": "boost-1.88.0",
        "artifact": "boost-1.88.0-cmake.7z",
        "hash": "e5b049e5b61964480ca816395f63f95621e66cb9bcf616a8b10e441e0e69f129e22443acb11e77bc1e8170f8e4171b9b7719891efc43699782bfcd4b3a365f01",
        "git_version": "1.88.0",
        "version": "1.57"
    },
    "opus": {
        "package": "Opus",
        "repo": "xiph/opus",
        "sha": "5ded705cf4",
        "hash": "0dc89e58ddda1f3bc6a7037963994770c5806c10e66f5cc55c59286fc76d0544fe4eca7626772b888fd719f434bc8a92f792bdb350c807968b2ac14cfc04b203",
        "version": "1.3",
        "find_args": "MODULE",
        "options": [
            "OPUS_BUILD_TESTING OFF",
            "OPUS_BUILD_PROGRAMS OFF",
            "OPUS_INSTALL_PKG_CONFIG_MODULE OFF",
            "OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF"
        ]
    },
    "discord-rpc": {
        "repo": "discord/discord-rpc",
        "sha": "963aa9f3e5",
        "hash": "386e1344e9a666d730f2d335ee3aef1fd05b1039febefd51aa751b705009cc764411397f3ca08dffd46205c72f75b235c870c737b2091a4ed0c3b061f5919bde",
        "options": [
            "BUILD_EXAMPLES OFF"
        ],
        "patches": [
            "0001-cmake-version.patch",
            "0002-no-clang-format.patch",
            "0003-fix-cpp17.patch"
        ]
    },
}
```

### Inclusion

To include CPMUtil:

```cmake
set(CPMUTIL_JSON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cpmfile.json)
include(CPMUtil)
```

You may omit the first line if you are not utilizing cpmfile.

## Prefetching

- To prefetch a CPM dependency (requires cpmfile):
  * `tools/cpm-fetch.sh <packages>`
- To prefetch all CPM dependencies:
  * `tools/cpm-fetch-all.sh`

Currently, `cpm-fetch.sh` defines the following directories for cpmfiles:

`externals src/yuzu/externals externals/ffmpeg src/dynarmic/externals externals/nx_tzdb`

Whenever you add a new cpmfile, update the script accordingly