<!--
SPDX-FileCopyrightText: 2018 yuzu Emulator Project - 2024 torzu Emulator Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

<h1 align="center">
  <br>
  <a href="http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu"><img src="https://raw.githubusercontent.com/litucks/torzu/master/dist/yuzu.bmp" alt="torzu" width="200"></a>
  <br>
  <b>torzu</b>
  <br>
</h1>

<h4 align="center"><b>torzu</b> is a fork of yuzu, the world's most popular, open-source, Nintendo Switch emulator.
<br>
It is written in C++ with portability in mind, and I plan to actively maintain builds for Linux, Android and Windows.
</h4>

<p align="center">
  <a href="#compatibility">Compatibility</a> |
  <a href="#development">Development</a> |
  <a href="#building">Building</a> |
  <a href="#download">Download</a> |
  <a href="#license">License</a>
</p>

## Compatibility

The emulator is capable of running most commercial games at full speed, provided you meet the [necessary hardware requirements](http://web.archive.org/web/20240130133811/https://yuzu-emu.org/help/quickstart/#hardware-requirements).

It runs most Nintendo Switch games released until the date of the Yuzu takedown.

## Changes

Following are the changes made since forking from Yuzu:

 - Added option to optimize generated SPIR-V shaders via spirv-opt
 - Added option to synchronize CPU clock to render speed limit
 - Added option to launch home menu from Switch firmware
 - Fixed crash when switching away from null graphics per-game
 - Fixed controller UI being cut off at the bottom
 - Removed analytics and authentication as they'd be useless (perhaps even outright dangerous) now
 - Minor improvements and additions

## Goals

The first and foremost goal is long-term maintenance. Even if I stop commiting new features I will always do my best to keep the emulator functional and third party dependencies updated. This also means most of the changes made will eventually be bug fixes.
Essentially, the main goal is that you can still use this emulator on modern systems in 20 years.
It is very important to me that this project is going to be a good base to fork once grass has grown over the whole legal dilemma and people are willing to do real work on this emulator non-anonymously.

A secondary goal is the improvement of usability on low-end systems. This includes both improving the performance of the emulator as well as making games more playable below 100% speed whenever possible (the sync CPU to render speed limit option already helps with that in few cases).

Android support is low priority but would be a nice bonus.

## Development

Most of the development happens on [Dark Git](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/). It's also where [our central repository](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu) is hosted.

Currently, development is somewhat limited to external commits as I'm working on improvements in dynarmic! This will increase emulation speed in CPU-bound scenarios.

If you want to contribute, please take a look at the [Contributor's Guide](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Contributing) and [Developer Information](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Developer-Information).

To clone this git repository, you can use these commands given tor is installed and running:

    git -c http.proxy=socks5h://127.0.0.1:9050 clone http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu.git
    cd torzu
    git submodule update --init --depth 1 --recursive

Alternatively, you can clone from the [GitHub mirror repository](https://github.com/litucks/torzu):

    git clone https://github.com/litucks/torzu.git --depth 1 --recursive

Note that above repository may be taken down any time. Do not rely on its existence in production. In case the GitHub mirror goes down, a mirror will be set up on Gitlab.

This project incorporates several commits from the [Suyu](https://suyu.dev) and [Sudachi](https://github.com/sudachi-emu/sudachi) forks (but cleaned up due to the typically mediocre code/commit quality from both projects) as well as changes listed in **Changes**.

## Building

* __Linux__: [Linux Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Building-for-Linux)
* __Windows__: [Windows Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Building-for-Windows)
* ~~__Android__: [Android Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Building-For-Android)~~
* ~~__macOS__: [macOS Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/torzu-emu/torzu/wiki/Building-for-macOS)~~

(Only Linux and Windows builds are available for now. Android is planned eventually.)

## Download

Precompiled build are available **in the releases section**.

## License

yuzu is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](https://github.com/yuzu-emu/yuzu/blob/master/LICENSE.txt) file.
