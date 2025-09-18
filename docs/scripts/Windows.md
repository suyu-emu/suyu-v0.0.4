# Windows Build Scripts

* A convenience script for building is provided in `.ci/windows/build.sh`.
* You must run this with Bash, e.g. Git Bash or the MinGW TTY.
* To use this script, you must have `windeployqt` installed (usually bundled with Qt) and set the `WINDEPLOYQT` environment variable to its canonical Bash location:
  * `WINDEPLOYQT="/c/Qt/6.9.1/msvc2022_64/bin/windeployqt6.exe" .ci/windows/build.sh`.
* You can use `aqtinstall`, more info on <https://github.com/miurahr/aqtinstall> and <https://ddalcino.github.io/aqt-list-server/>


* Extra CMake flags should be placed in the arguments of the script.

#### Additional environment variables can be used to control building:

* `BUILD_TYPE` (default `Release`): Sets the build type to use.

* The following environment variables are boolean flags. Set to `true` to enable or `false` to disable:

  * `DEVEL` (default FALSE): Disable Qt update checker
  * `USE_WEBENGINE` (default FALSE): Enable Qt WebEngine
  * `USE_MULTIMEDIA` (default FALSE): Enable Qt Multimedia
  * `BUNDLE_QT` (default FALSE): Use bundled Qt

  * Note that using **system Qt** requires you to include the Qt CMake directory in `CMAKE_PREFIX_PATH`
    * `.ci/windows/build.sh -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6`

* After building, a zip can be packaged via `.ci/windows/package.sh`. You must have 7-zip installed and in your PATH.
  * The resulting zip will be placed into `artifacts` in the source directory.


