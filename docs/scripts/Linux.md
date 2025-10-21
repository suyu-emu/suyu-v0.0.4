# Linux Build Scripts

* Provided script: `.ci/linux/build.sh`
* Must specify arch target, e.g.: `.ci/linux/build.sh amd64`
* Valid targets:
  * `native`: Optimize to your native host architecture
  * `legacy`: x86\_64 generic, only needed for CPUs older than 2013 or so
  * `amd64`: x86\_64-v3, for CPUs newer than 2013 or so
  * `steamdeck` / `zen2`: For Steam Deck or Zen >= 2 AMD CPUs (untested on Intel)
  * `rog-ally` / `allyx` / `zen4`: For ROG Ally X or Zen >= 4 AMD CPUs (untested on Intel)
  * `aarch64`: For armv8-a CPUs, older than mid-2021 or so
  * `armv9`: For armv9-a CPUs, newer than mid-2021 or so
* Extra CMake flags go after the arch target.

### Environment Variables

* `NPROC`: Number of compilation threads (default: all cores)
* `TARGET`: Set `appimage` to disable standalone `eden-cli` and `eden-room`
* `BUILD_TYPE`: Build type (default: `Release`)

Boolean flags (set `true` to enable, `false` to disable):

* `DEVEL` (default `FALSE`): Disable Qt update checker
* `USE_WEBENGINE` (default `FALSE`): Enable Qt WebEngine
* `USE_MULTIMEDIA` (default `FALSE`): Enable Qt Multimedia

* AppImage packaging script: `.ci/linux/package.sh`

  * Accepts same arch targets as build script
  * Use `DEVEL=true` to rename app to `Eden Nightly` and disable the update checker
  * This should generally not be used unless in a tailor-made packaging environment (e.g. Actions/CI)