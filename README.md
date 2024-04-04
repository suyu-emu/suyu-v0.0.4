<!--
SPDX-FileCopyrightText: 2018 yuzu Emulator Project - 2024 torzu Emulator Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

<h4 align="center"><b>torzu</b> is a fork of yuzu, the world's most popular, open-source, Nintendo Switch emulator — started by the creators of <a href="https://citra-emu.org" target="_blank">Citra</a>.
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

## Development

Most of the development happens on [Dark Git](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/). It's also where [our central repository](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu) is hosted.

If you want to contribute, please take a look at the [Contributor's Guide](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Contributing) and [Developer Information](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Developer-Information).

To clone this git repository, you can use `git -c http.proxy=socks5h://127.0.0.1:9050 clone http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu.git --depth 1 --recursive` given tor is installed and running.

## Building

* __Linux__: [Linux Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Building-for-Linux)
* __Windows__: [Windows Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Building-for-Windows)
* __Android__: [Android Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Building-For-Android)
* __macOS__: [macOS Build](http://y2nlvhmmk5jnsvechppxnbyzmmv3vbl7dvzn6ltwcdbpgxixp3clkgqd.onion/darktux/torzu/wiki/Building-for-macOS)

## Download

There are no precompiled releases available (yet).

## License

yuzu is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](https://github.com/yuzu-emu/yuzu/blob/master/LICENSE.txt) file.
