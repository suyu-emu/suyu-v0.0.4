# `AddPackage`

<!-- TOC -->
- [Identification/Fetching](#identificationfetching)
- [Hashing](#hashing)
- [Other Options](#other-options)
- [Extra Variables](#extra-variables)
- [System/Bundled Packages](#systembundled-packages)
- [Identification](#identification)
<!-- /TOC -->

## Identification/Fetching

- `NAME` (required): The package name (must be the same as the `find_package` name if applicable)
- `VERSION`: The minimum version of this package that can be used on the system
- `GIT_VERSION`: The "version" found within git
- `URL`: The URL to fetch.
- `REPO`: The repo to use (`owner/repo`).
- `GIT_HOST`: The Git host to use
  * Defaults to `github.com`. Do not include the protocol, as HTTPS is enforced.
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

## Hashing

Hashing is used for verifying downloads. It's highly recommended to use these.

- `HASH_ALGO` (default `SHA512`): Hash algorithm to use

Hashing strategies, descending order of precedence:

- `HASH`: Bare hash verification, useful for static downloads e.g. commit archives
- `HASH_SUFFIX`: Download the hash as `${DOWNLOAD_URL}.${HASH_SUFFIX}`
  * The downloaded hash *must* match the hash algorithm and contain nothing but the hash; no filenames or extra content.
- `HASH_URL`: Download the hash from a separate URL

## Other Options

- `KEY`: Custom cache key to use (stored as `.cache/cpm/${packagename_lower}/${key}`)
  * Default is based on, in descending order of precedence:
    - First 4 characters of the sha
    - `GIT_VERSION`
    - Tag
    - `VERSION`
    - Otherwise, CPM defaults will be used. This is not recommended as it doesn't produce reproducible caches
- `DOWNLOAD_ONLY`: Whether or not to configure the downloaded package via CMake
  * Useful to turn `OFF` if the project doesn't use CMake
- `SOURCE_SUBDIR`: Subdirectory of the project containing a CMakeLists.txt file
- `FIND_PACKAGE_ARGUMENTS`: Arguments to pass to the `find_package` call
- `BUNDLED_PACKAGE`: Set to `ON` to default to the bundled package
- `FORCE_BUNDLED_PACKAGE`: Set to `ON` to force the usage of the bundled package, regardless of CPMUTIL_FORCE_SYSTEM or `<package>_FORCE_SYSTEM`
- `OPTIONS`: Options to pass to the configuration of the package
- `PATCHES`: Patches to apply to the package, stored in `.patch/${packagename_lower}/0001-patch-name.patch` and so on
- Other arguments can be passed to CPM as well

## Extra Variables

For each added package, users may additionally force usage of the system/bundled package.

- `${package}_FORCE_SYSTEM`: Require the package to be installed on the system
- `${package}_FORCE_BUNDLED`: Force the package to be fetched and use the bundled version

## System/Bundled Packages

Descending order of precedence:
- If `${package}_FORCE_SYSTEM` is true, requires the package to be on the system
- If `${package}_FORCE_BUNDLED` is true, forcefully uses the bundled package
- If `CPMUTIL_FORCE_SYSTEM` is true, requires the package to be on the system
- If `CPMUTIL_FORCE_BUNDLED` is true, forcefully uses the bundled package
- If the `BUNDLED_PACKAGE` argument is true, forcefully uses the bundled package
- Otherwise, CPM will search for the package first, and if not found, will use the bundled package

## Identification

All dependencies must be identifiable in some way for usage in the dependency viewer. Lists are provided in descending order of precedence.

URLs:

- `GIT_URL`
- `REPO` as a Git repository
  * You may optionally specify `GIT_HOST` to use a custom host, e.g. `GIT_HOST git.crueter.xyz`. Note that the git host MUST be GitHub-like in its artifact/archive downloads, e.g. Forgejo
  * If `GIT_HOST` is unspecified, defaults to `github.com`
- `URL`

Versions (bundled):

- `SHA`
- `GIT_VERSION`
- `VERSION`
- `TAG`
- "unknown"

If the package is a system package, AddPackage will attempt to determine the package version and append ` (system)` to the identifier. Otherwise, it will be marked as `unknown (system)`
