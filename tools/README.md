# Tools

Tools for Eden and other subprojects. When adding new scripts please use `#!/bin/sh -e` or `#!/usr/bin/env <interpreter>` (for `.py`, `.rb`, or `.perl`). Keep scripts POSIX compliant (i.e not require hard `bash` to run, just plain old `sh`).

## Third-Party

- [CPMUtil Scripts](./cpm)

## Binaries

- `maxwell-spirv`: Converts Maxwell shaders (dumped from `.ash` files) into SPIR-V code (emitted into STDOUT).
- `maxwell-disas`: Dumb raw Maxwell dissasembler.
- `maxwell-ir`: Dump generated IR of Maxwell shaders.

## Scripts

- `generate_converters.py`: Generates converters for given formats of textures (C++ helper).
- `svc_generator.py`: Generates the files `src/core/hle/kernel/svc.cpp` and `src/core/hle/kernel/svc.h` based off prototypes.
- `shellcheck.sh`: Ensure POSIX compliance (and syntax sanity) for all tools in this directory and subdirectories.
- `llvmpipe-run.sh`: Sets environment variables needed to run any command (or Eden) with llvmpipe.
- `optimize-assets.sh`: Optimizes PNG assets with OptiPng.
- `update-cpm.sh`: Updates CPM.cmake to the latest version.
- `update-icons.sh`: Rebuild all icons (macOS, Windows, bitmaps) based on the master SVG file (`dist/dev.eden_emu.eden.svg`)
    * Also optimizes the master SVG
    * Requires: `png2icns` (libicns), ImageMagick, [`svgo`](https://github.com/svg/svgo)
- `dtrace-tool.sh`
- `lanczos-gen.pl`: Generates constants for the Lanczos filter.
- `clang-format.sh`: Runs `clang-format` on the entire codebase.
    * Requires: clang
- `find-unused-strings.sh`: Find any unused strings in the Android app (XML -> Kotlin).

## Android
It's recommended to run these scritps after almost any Android change, as they are relatively fast and important both for APK bloat and CI.

- `unused-strings.sh`: Finds unused strings in `strings.xml` files.
- `stale-translations.sh`: Finds translated strings that aren't present in the source `strings.xml` file.

## Translations

- [Translation Scripts](./translations)
