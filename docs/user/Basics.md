# User Handbook - The Basics

## Introduction

Eden is a very complicated piece of software, and as such there are many knobs and toggles that can be configured. Most of these are invisible to normal users, however power users may be able to leverage them to their advantage.

This handbook primarily describes such knobs and toggles. Normal configuration options are described within the emulator itself and will not be covered in detail.

## Requirements

The emulator is very demanding on hardware, and as such requires a decent mid-range computer/cellphone.

See [the requirements page](https://archive.is/sv83h) for recommended and minimum specs.

The CPU must support FMA for an optimal gameplay experience. The GPU needs to support OpenGL 4.6 ([compatibility list](https://opengl.gpuinfo.org/)), or Vulkan 1.1 ([compatibility list](https://vulkan.gpuinfo.org/)).

If your GPU doesn't support or is just behind by a minor version, see Mesa environment variables below (*nix only).

## Releases and versions

- Stable releases/Versioned releases: Has a version number and it's the versions we expect 3rd party repositories to host (package managers and such), these are, well, stable, have low amount of regressions (wrt. to master and nightlies) and generally focus on "keeping things without regressions", recommended for the average user.
    - RC releases: Release candidate, generally "less stable but still stable" versions.
    - Full release: "The stablest possible you could get".
- Nightly: Builds done around 2PM UTC (if there are any changes), generally stable, but not recommended for the average user. These contain daily updates and may contain critical fixes for some games.
- Master: Unstable builds, can lead from a game working exceptionally fine to absolute crashing in some systems because someone forgot to check if NixOS or Solaris worked. These contain straight from the oven fixes, please don't use them unless you plan to contribute something! They're very experimental! Still 95% of the time it will work just fine.
- PR builds: Highly experimental builds, testers may grab from these. The average user should treat them the same as master builds, except sometimes they straight up don't build/work.

## User configuration

### Configuration directories

Eden will store configuration files in the following directories:

- **Windows**: `%AppData%\Roaming`.
- **Android**: Data is stored internally.
- **Linux, macOS, FreeBSD, Solaris, OpenBSD**: `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`, `$XDG_CONFIG_HOME`.
- **HaikuOS**: `/boot/home/config/settings/eden`

If a `user` directory is present in the current working directory, that will override all global configuration directories and the emulator will use that instead.

### Environment variables

Throughout the handbook, environment variables are mentioned. These are often either global (system wide) or local (set in a script, bound only to the current session). It's heavily recommended to use them in a local context only, as this allows you to rollback changes easily (if for example, there are regressions setting them).

The recommended way is to create a `.bat` file alongside the emulator `.exe`; contents of which could resemble something like:

```bat
set "__GL_THREADED_OPTIMIZATIONS=1"
set "SOME_OTHER_VAR=1"
eden.exe
```

Android doesn't have a convenient way to set environment variables.

For other platforms, the recommended method is using a shell script:

```sh
export __GL_THREADED_OPTIMIZATIONS=1
export SOME_OTHER_VAR=1
./eden
```

Then just running `chmod +x script.sh && source script.sh`.

## Compatibility list

Eden doesn't mantain a compatibility list. However, [EmuReady](https://www.emuready.com/) has a more fine-grained compatibility information for multiple emulators/forks as well.
