# ⚠️ This guide is for developers ONLY! Support will be provided to developers ONLY.

## 📋 Current building methods:

* [ Minimal Dependencies](#minimal-dependencies)
* [⚡ Method I: MSVC Build for Windows](#method-i-msvc-build-for-windows)
* [🐧 Method II: MinGW-w64 Build with MSYS2](#method-ii-mingw-w64-build-with-msys2)
* [🖥️ Method III: CLion Environment Setup](#method-iii-clion-environment-setup)
* [💻 Building from the command line with MSVC](#building-from-the-command-line-with-msvc)
* [📜 Building with Scripts](#building-with-scripts)

---


## Minimal Dependencies

On Windows, **all** library dependencies are **automatically included** within the `externals` folder.

You still need to install:

* **[CMake](https://cmake.org/download/)** - Used to generate Visual Studio project files.
* **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** - Make sure to select **Latest SDK**.

  * *A convenience script to install the latest SDK is provided in `.ci/windows/install-vulkan-sdk.ps1`*
* **[Git for Windows](https://gitforwindows.org)** - We recommend installing Git for command line use and version control integration.

  <img src="https://i.imgur.com/x0rRs1t.png" width="500">

  * *While installing Git Bash, select "Git from the command line and also from 3rd-party software". If missed, manually set `git.exe` path in CMake.*

---

## ⚡ Method I: MSVC Build for Windows

### a. Prerequisites to MSVC Build

* **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)** - Make sure to **select C++ support** in the installer, or **update to the latest version** if already installed.

  * *A convenience script to install the **minimal** version (Visual Build Tools) is provided in `.ci/windows/install-msvc.ps1`*

---

### b. Clone the eden repository with Git

Open Terminal on 

```cmd
git clone https://git.eden-emu.dev/eden-emu/eden
cd eden
```

* *By default `eden` downloads to `C:\Users\<user-name>\eden`*

---

### c. Building

* Open the CMake GUI application and point it to the `eden`

  <img src="https://i.imgur.com/qOslIWv.png" width="500">

* For the build directory, use a `build/` subdirectory inside the source directory or some other directory of your choice. (Tell CMake to create it.)

* Click the "Configure" button and choose `Visual Studio 17 2022`, with `x64` for the optional platform.

  <img src="https://i.imgur.com/DKiREaK.png" width="500">

  * *(You may also want to disable `YUZU_TESTS` in this case since Catch2 is not yet supported with this.)*

  <img src="https://user-images.githubusercontent.com/22451773/180585999-07316d6e-9751-4d11-b957-1cf57cd7cd58.png" width="500">

* Click "Generate" to create the project files.

  <img src="https://i.imgur.com/5LKg92k.png" width="500">

* Open the solution file `yuzu.sln` in Visual Studio 2022, which is located in the build folder.

  <img src="https://i.imgur.com/208yMml.png" width="500">

* * Depending on whether you want a graphical user interface or not, select in the Solution Explorer:
    * `eden` (GUI)
    * `eden-cmd` (command-line only)
  * Then **right-click** and choose `Set as StartUp Project`.

  <img src="https://i.imgur.com/nPMajnn.png" height="500">
  <img src="https://i.imgur.com/BDMLzRZ.png" height="500">

* Select the appropriate build type, `Debug` for debug purposes or `Release` for performance (in case of doubt choose `Release`).

  <img src="https://i.imgur.com/qxg4roC.png" width="500">

* **Right-click** the project you want to build and press **Build** in the submenu or press `F5`.

  <img src="https://i.imgur.com/CkQgOFW.png" height="500">

---

## 🐧 Method II: MinGW-w64 Build with MSYS2

### a. Prerequisites to MinGW-w64

* **[MSYS2](https://www.msys2.org)** - A versatile and up-to-date development environment for Windows, providing a Unix-like shell, package manager, and toolchain.

---

### b. Install eden dependencies for MinGW-w64

* Open the `MSYS2 MinGW 64-bit` shell (`mingw64.exe`)
* Download and install all dependencies using:
  * `pacman -Syu git make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-cmake mingw-w64-x86_64-python-pip mingw-w64-x86_64-qt6 mingw-w64-x86_64-toolchain autoconf libtool automake-wrapper`
* Add MinGW binaries to the PATH:
  * `echo 'PATH=/mingw64/bin:$PATH' >> ~/.bashrc`
* Add VulkanSDK to the PATH:
  * `echo 'PATH=$(readlink -e /c/VulkanSDK/*/Bin/):$PATH' >> ~/.bashrc`

---

### c. Clone the eden repository with Git

```cmd
git clone https://git.eden-emu.dev/eden-emu/eden
cd eden
```

---

### d. Building dynamically-linked eden

* This process will generate a *dynamically* linked build

```bash
# Make build dir and enter
mkdir build && cd build

# Generate CMake Makefiles
cmake .. -G "MSYS Makefiles" -DYUZU_TESTS=OFF

# Build
make -j$(nproc)

# Run eden!
./bin/eden.exe
```

* *Warning: This build is not a **static** build meaning that you **need** to include all of the DLLs with the .exe in order to use it or other systems!*

---

### Additional notes


* Eden doesn't require the rather large Qt dependency, but you will lack a GUI frontend

```bash
# ...

# Generate CMake Makefiles (withou QT)
cmake .. -G "MSYS Makefiles" -DYUZU_TESTS=OFF -DENABLE_QT=no

$ ...
```

* Running programs from inside `MSYS2 MinGW x64` shell has a different `%PATH%` than directly from explorer.
  * This different `%PATH%` has the locations of the other DLLs required.

    <img src="https://user-images.githubusercontent.com/190571/165000848-005e8428-8a82-41b1-bb4d-4ce7797cdac8.png" width="500">

---

## 🖥️ Method III: CLion Environment Setup

### a. Prerequisites to CLion

* [CLion](https://www.jetbrains.com/clion/) - This IDE is not free; for a free alternative, check Method I

---

### b. Cloning eden with CLion

* Clone the Repository:

<img src="https://user-images.githubusercontent.com/42481638/216899046-0d41d7d6-8e4d-4ed2-9587-b57088af5214.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899061-b2ea274a-e88c-40ae-bf0b-4450b46e9fea.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899076-0e5988c4-d431-4284-a5ff-9ecff973db76.png" width="500">

---

### c. Building & Setup

* Once Cloned, You will be taken to a prompt like the image below:

<img src="https://user-images.githubusercontent.com/42481638/216899092-3fe4cec6-a540-44e3-9e1e-3de9c2fffc2f.png" width="500">

* Set the settings to the image below:
* Change `Build type: Release`
* Change `Name: Release`
* Change `Toolchain Visual Studio`
* Change `Generator: Let CMake decide`
* Change `Build directory: build`

<img src="https://user-images.githubusercontent.com/42481638/216899164-6cee8482-3d59-428f-b1bc-e6dc793c9b20.png" width="500">

* Click OK; now Clion will build a directory and index your code to allow for IntelliSense. Please be patient.
* Once this process has been completed (No loading bar bottom right), you can now build eden
* In the top right, click on the drop-down menu, select all configurations, then select eden

<img src="https://user-images.githubusercontent.com/42481638/216899226-975048e9-bc6d-4ec1-bc2d-bd8a1e15ed04.png" height="500" >

* Now run by clicking the play button or pressing Shift+F10, and eden will auto-launch once built.

<img src="https://user-images.githubusercontent.com/42481638/216899275-d514ec6a-e563-470e-81e2-3e04f0429b68.png" width="500">

---

## 💻 Building from the command line with MSVC

```cmd
# Clone eden and enter
git clone https://git.eden-emu.dev/eden-emu/eden
cd eden

# Make build dir and enter
mkdir build && cd build

# Generate CMake Makefiles
cmake .. -G "Visual Studio 17 2022" -A x64 -DYUZU_TESTS=OFF

# Build
cmake --build . --config Release
```

## 📜 Building with Scripts

* A convenience script for building is provided in `.ci/windows/build.sh`.
* You must run this with Bash, e.g. Git Bash or MinGW TTY.
* To use this script, you must have `windeployqt` installed (usually bundled with Qt) and set the `WINDEPLOYQT` environment variable to its canonical Bash location:
  * `WINDEPLOYQT="/c/Qt/6.9.1/msvc2022_64/bin/windeployqt6.exe" .ci/windows/build.sh`.
* You can use `aqtinstall`, more info on <https://github.com/miurahr/aqtinstall> and <https://ddalcino.github.io/aqt-list-server/>


* Extra CMake flags should be placed in the arguments of the script.

#### Additional environment variables can be used to control building:

* `BUILD_TYPE` (default `Release`): Sets the build type to use.

* The following environment variables are boolean flags. Set to `true` to enable or `false` to disable:

  * `DEVEL` (default FALSE): Disable Qt update checker
  * `USE_WEBENGINE` (default FALSE): Enable Qt WebEngine
  * `USE_MULTIMEDIA` (default TRUE): Enable Qt Multimedia
  * `BUNDLE_QT` (default FALSE): Use bundled Qt

  * Note that using **system Qt** requires you to include the Qt CMake directory in `CMAKE_PREFIX_PATH`
    * `.ci/windows/build.sh -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64/lib/cmake/Qt6`

* After building, a zip can be packaged via `.ci/windows/package.sh`. You must have 7-zip installed and in your PATH.
  * The resulting zip will be placed into `artifacts` in the source directory.

