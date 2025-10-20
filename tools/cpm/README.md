# CPMUtil Tools

These are supplemental shell scripts for CPMUtil aiming to ease maintenance burden for sanity checking, updates, prefetching, formatting, and standard operations done by these shell scripts, all in one common place.

All scripts are POSIX-compliant. If something doesn't work on your shell, ensure it's POSIX-compliant.
* If your shell doesn't support `$(...)` syntax, you've got bigger problems to worry about.
<!-- TOC -->
- [Meta](#meta)
- [Simple Utilities](#simple-utilities)
- [Functional Utilities](#functional-utilities)
<!-- /TOC -->

## Meta

These scripts are generally reserved for internal use.

- `common.sh`: Grabs all available cpmfiles and aggregates them together.
    * Outputs:
        - `PACKAGES`: The aggregated cpmfile
        - `LIBS`: The list of individual libraries contained within each cpmfile
        - `value`: A function that grabs a key from the `JSON` variable (typically the package key)
- `download.sh`: Utility script to handle downloading of regular and CI packages.
    * Generally only used by the fetch scripts.
- `package.sh`: The actual package parser.
    * Inputs:
        - `PACKAGE`: The package key
    * Outputs:
        - Basically everything. You're best off reading the code rather than me poorly explaining it.
- `which.sh`: Find which cpmfile a package is located in.
    * Inputs:
        - The package key
- `replace.sh`: Replace a package's cpmfile definition.
    * Inputs:
        - `PACKAGE`: The package key
        - `NEW_JSON`: All keys to replace/add
    * Keys not found in the new json are not touched. Keys cannot currently be deleted.

## Simple Utilities

These scripts don't really have any functionality, they just help you out a bit yknow?

- `format.sh`: Format all cpmfiles (4-space indent is enforced)
    * In the future, these scripts will have options for spacing
- `hash.sh`: Determine the hash of a specific package.
    * Inputs:
        - The repository (e.g. fmtlib/fmt)
        - The sha or tag (e.g. v1.0.1)
        - `-g <GIT_HOST>` or `--host <GIT_HOST>`: What git host to use (default github.com)
        - `-a <ARTIFACT>` or `--artifact <ARTIFACT>`: The artifact to download. Set to null or empty to use a source archive instead
    * Output: the SHA512 sum of the package
- `url-hash.sh`: Determine the hash of a URL
    * Input: the URL
    * Output: the SHA512 sum of the URL

## Functional Utilities

These modify the CPM cache or cpmfiles. Each allows you to input all the packages to act on, as well as a `<scriptname>-all.sh` that acts upon all available packages.

Beware: if a hash is `cf83e1357...` that means you got a 404 error!

- `fetch.sh`: Prefetch a package according to its cpmfile definition
    * Packages are fetched to the `.cache/cpm` directory by default, following the CPMUtil default.
    * Already-fetched packages will be skipped. You can invalidate the entire cache with `rm -rf .cache/cpm`, or invalidate a specific package with e.g. `rm -rf .cache/cpm/packagename` to force a refetch.
    * In the future, a force option will be added
    * Note that full prefetching will take a long time depending on your internet, the amount of dependencies, and the size of each dependency.
- `check-updates.sh`: Check a package for available updates
    * This only applies to packages that utilize tags.
    * If the tag is a format string, the `git_version` is acted upon instead.
    * Specifying `-f` or `--force` will forcefully update the package and its hash, even if it's on on the latest version.
    * Alternatively, only specify `-u` or `--update` to update packages that have new versions available.
    * This script generally runs fast.
    * Packages that should skip updates (e.g. older versions, OR packages with poorly-made tag structures... looking at you mbedtls) may specify `"skip_updates": true` in their cpmfile definition. This is unnecessary for untagged (e.g. sha or bare URL) packages.
- `check-hashes.sh`: Check a package's hash
    * Specifying `-f` or `--force` will update the package's hash even if it's not mismatched.
    * Alternatively, specify `-u` or `--update` to only fix mismatched hashes.
    * This only applies to packages with hardcoded hashes, NOT ones that use hash URLs.
    * This script will take a long time. This is operationally equivalent to a prefetch, and thus checking all hashes will take a while--but it's worth it! Just make sure you're not using dial-up.

You are recommended to run sanity hash checking for every pull request and commit, and weekly update checks.