# CPMUtil

CPMUtil is a wrapper around CPM that aims to reduce boilerplate and add useful utility functions to make dependency management a piece of cake.

See more in [its repository](https://git.crueter.xyz/CMake/CPMUtil)

Eden-specific options:

- `YUZU_USE_CPM` is set by default on MSVC and Android. Other platforms should use this if certain "required" system dependencies (e.g. OpenSSL) are broken or missing
  * If this is `OFF`, required system dependencies will be searched via `find_package`, although most externals use CPM regardless.
- Force system libraries via CMake arguments:
  * SDL2: `YUZU_USE_BUNDLED_SDL2` and `YUZU_USE_EXTERNAL_SDL2`
  * FFmpeg: `YUZU_USE_EXTERNAL_FFMPEG`

## Tooling

See the [tooling docs](../tools/cpm)