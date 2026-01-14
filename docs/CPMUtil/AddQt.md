# AddQt

Simply call `AddQt(<Qt Version>)` before any Qt `find_package` calls and everything will be set up for you. On Linux, the bundled Qt library is built as a shared library, and provided you have OpenSSL and X11, everything should just work.

On Windows, MinGW, and MacOS, Qt is bundled as a static library. No further action is needed, as the provided libraries automatically integrate the Windows/Cocoa plugins, alongside the corresponding Multimedia and Network plugins.

## Modules

The following modules are bundled into these Qt builds:

- Base (Gui, Core, Widgets, Network)
- Multimedia
- Declarative (Quick, QML)
- Linux: Wayland client

Each platform has the corresponding QPA built in and set as the default as well. This means you don't need to add `Q_IMPORT_PLUGIN`!

## Example

See an example in the [`tests/qt`](https://git.crueter.xyz/CMake/CPMUtil/src/branch/master/tests/qt/CMakeLists.txt) directory.

## Versions

The following versions have available builds:

- 6.9.3

See [`crueter-ci/Qt`](https://github.com/crueter-ci/Qt) for an updated list at any time.
