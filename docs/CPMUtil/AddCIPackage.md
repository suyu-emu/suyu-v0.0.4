# AddPackage

- `VERSION` (required): The version to get (the tag will be `v${VERSION}`)
- `NAME` (required): Name used within the artifacts
- `REPO` (required): CI repository, e.g. `crueter-ci/OpenSSL`
- `PACKAGE` (required): `find_package` package name
- `EXTENSION`: Artifact extension (default `tar.zst`)
- `MIN_VERSION`: Minimum version for `find_package`. Only used if platform does not support this package as a bundled artifact
- `DISABLED_PLATFORMS`: List of platforms that lack artifacts for this package. Options:
  * `windows-amd64`
  * `windows-arm64`
  * `android`
  * `solaris-amd64`
  * `freebsd-amd64`
  * `linux-amd64`
  * `linux-aarch64`
  * `macos-universal`