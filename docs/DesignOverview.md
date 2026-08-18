# Design Overview

Modern game consoles require heavy power to be emulated appropriatedly. This is why the emulator uses an approach known as HLE (High-Level-Emulation), in a nuthsell: Instead of accurately emulating every subsystem that forms part of a component, emulate the resulting visible I/O interface instead.

For example, take a disk write, instead of emulating a proper SD card we instead use the C++ standard library for I/O. Additionally we use the abstractions provided by the `fs` service to "lie" to programs about certain SD card properties. Notably this includes making up sizes for the fake SD card, giving "realistic" values or expected outputs for a given card, and so on. And instead of writing to an actual SD card, the emulator simply writes to a file.

This also means grand part of the emulator consists of just re-implementing firmware but using HLE primitives; for example audio doesn't go to an emulated audio device, but rather gets processed on the fly by a dedicated service and then passed to SDL3/cubeb/etc.

As such, many of the systems implemented are not 100% accurate to the original software, but they're "good enough" to pass as being so. While we do strive to maintain high compatibility (especially with homebrew), there are realistic limitations to these approaches.

No formal fuzzing or formal verification has been done on the emulator as a whole, this means there may be hundreds of bugs hiding in each subsystem. As such, output may also not correctly match due to those aforementioned issues.

## src/android/

Entire Android frontend, written mostly in Kotlin and generally having asinine hacks (thanks Android) due to the particularly horrific (and particular way) to do things.

## src/audio_core/

Handles everything related to audio, this is where most of the filtering and processing occurs (CPU intensive task!).

## src/common/

The [common](../src/common) folder contains just your basic pollyfills for whatever missing functionality. We heavily encourage new PRs to make use of one of the dependencies, or the standard C++ library. Minimizing the amount of things we reinvent the wheel for is always a good thing.

## src/core/

The [core folder](../src/core) is the main heart of the emulator, while we don't officially support having other frontends out of the box (RetroArch, for example); any prospective developer can reuse this library (compiled as one by CMake) to stitch up their own RetroArch core, for example.

## src/dynarmic/

See [Dynarmic](./dynarmic).

## src/hid_core/

Input, joysticks, controllers and everything related to input devices.

## src/shader_recompiler/

Dedicated shader recompiler to translate Maxwell assembly code to either SPIR-V, GLSL or GLASM.

## src/video_core/

Most of the things here have their own dedicated section. In short this is basically the entire Tegra NVIDIA Maxwell GPU emulation. Additionally it includes some [extra effects](../src/video_core/host_ahders) to emulate MSAA, D24 copies or as polyfill.

Available backends are: Null, Vulkan, and OpenGL.

See [NVIDIA GPU](./NvidiaGpu.md).
