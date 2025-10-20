# AddJsonPackage

In each directory that utilizes `CPMUtil`, there must be a `cpmfile.json` that defines dependencies in a similar manner to the individual calls.

The cpmfile is an object of objects, with each sub-object being named according to the package's identifier, e.g. `openssl`, which can then be fetched with `AddJsonPackage(<identifier>)`. Options are designed to map closely to the argument names, and are always strings unless otherwise specified.
<!-- TOC -->
- [Options](#options)
- [Examples](#examples)
<!-- /TOC -->

## Options

- `package` -> `NAME` (`PACKAGE` for CI), defaults to the object key
- `repo` -> `REPO`
- `version` -> `VERSION`
- `ci` (bool)

If `ci` is `false`:

- `hash` -> `HASH`
- `hash_suffix` -> `HASH_SUFFIX`
- `sha` -> `SHA`
- `key` -> `KEY`
- `tag` -> `TAG`
  * If the tag contains `%VERSION%`, that part will be replaced by the `git_version`, OR `version` if `git_version` is not specified
- `url` -> `URL`
- `artifact` -> `ARTIFACT`
  * If the artifact contains `%VERSION%`, that part will be replaced by the `git_version`, OR `version` if `git_version` is not specified
  * If the artifact contains `%TAG%`, that part will be replaced by the `tag` (with its replacement already done)
- `git_version` -> `GIT_VERSION`
- `git_host` -> `GIT_HOST`
- `source_subdir` -> `SOURCE_SUBDIR`
- `bundled` -> `BUNDLED_PACKAGE`
- `find_args` -> `FIND_PACKAGE_ARGUMENTS`
- `download_only` -> `DOWNLOAD_ONLY`
- `patches` -> `PATCHES` (array)
- `options` -> `OPTIONS` (array)
- `skip_updates`: Tells `check-updates.sh` to not check for new updates on this package.

Other arguments aren't currently supported. If you wish to add them, see the `AddJsonPackage` function in `CMakeModules/CPMUtil.cmake`.

If `ci` is `true`:

- `name` -> `NAME`, defaults to the object key
- `extension` -> `EXTENSION`, defaults to `tar.zst`
- `min_version` -> `MIN_VERSION`
- `extension` -> `EXTENSION`
- `disabled_platforms` -> `DISABLED_PLATFORMS` (array)

## Examples

In order: OpenSSL CI, Boost (tag + artifact), Opus (options + find_args), discord-rpc (sha + options + patches).

```json
{
    "openssl": {
        "ci": true,
        "package": "OpenSSL",
        "name": "openssl",
        "repo": "crueter-ci/OpenSSL",
        "version": "3.6.0",
        "min_version": "1.1.1",
        "disabled_platforms": [
            "macos-universal"
        ]
    },
    "boost": {
        "package": "Boost",
        "repo": "boostorg/boost",
        "tag": "boost-%VERSION%",
        "artifact": "%TAG%-cmake.7z",
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
    }
}
```