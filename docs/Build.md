# Building Eden

> [!WARNING]
> This guide is intended for developers ONLY. If you are not a developer or packager, you are unlikely to receive support.

This is a full-fledged guide to build Eden on all supported platforms.

## Dependencies
First, you must [install some dependencies](Deps.md).

## Clone
Next, you will want to clone Eden via the terminal:

```sh
git clone https://git.eden-emu.dev/eden-emu/eden.git
cd eden
```

Or use Qt Creator (Create Project -> Import Project -> Git Clone).

## Android

Android has a completely different build process than other platforms. See its [dedicated page](build/Android.md).

## Initial Configuration

If the configure phase fails, see the `Troubleshooting` section below. Usually, as long as you followed the dependencies guide, the defaults *should* successfully configure and build.

### Option A: Qt Creator

This is the recommended GUI method for Linux, macOS, and Windows.

<details>
<summary>Click to Open</summary>

> [!WARNING]
> On MSYS2, to use Qt Creator you are recommended to *also* install Qt from the online installer, ensuring to select the "MinGW" version.

Open the CMakeLists.txt file in your cloned directory via File -> Open File or Project (Ctrl+O), if you didn't clone Eden via the project import tool.

Select your desired "kit" (usually, the default is okay). RelWithDebInfo or Release is recommended:

![Qt Creator kits](img/creator-1.png)

Hit "Configure Project", then wait for CMake to finish configuring (may take a while on Windows).

</details>

### Option B: Command Line

<details>
<summary>Click to Open</summary>

> [!WARNING]
>For all systems:
>- *CMake* **MUST** be in your PATH (and also *ninja*, if you are using it as `<GENERATOR>`)
>- You *MUST* be in the cloned *Eden* directory
>
>On Windows:
>  - It's recommended to install **[Ninja](https://ninja-build.org/)**
>  - You must load **Visual C++ development environment**, this can be done by running our convenience script:
>    - `tools/windows/load-msvc-env.ps1` (for PowerShell 5+)
>    - `tools/windows/load-msvc-env.sh` (for MSYS2, Git Bash, etc)

Available `<GENERATOR>`:
- MSYS2: `MSYS Makefiles`
- MSVC: `Ninja` (preferred) or `Visual Studio 17 2022`
- macOS: `Ninja` (preferred) or `Xcode`
- Others: `Ninja` (preferred) or `UNIX Makefiles`

Available `<BUILD_TYPE>`:
- `Release` (default)
- `RelWithDebInfo` (debug symbols--compiled executable will be large)
- `Debug` (if you are using a debugger and annoyed with stuff getting optimized out)

Caveat for Debug Builds:
- If you're building with CCache, you will need to add the environment variable `CL` with the `/FS` flag ([Reference](https://learn.microsoft.com/pt-br/cpp/build/reference/fs-force-synchronous-pdb-writes?view=msvc-170))

Also see the [Options](Options.md) page for additional CMake options.

```sh
cmake -S . -B build -G "<GENERATOR>" -DCMAKE_BUILD_TYPE=<BUILD_TYPE> -DYUZU_TESTS=OFF
```

If you are on Windows and prefer to use Clang:

```sh
cmake -S . -B build -G "<GENERATOR>" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
```

</details>

### Option C: [CLion](https://www.jetbrains.com/clion/)

<details>
<summary>Click to Open</summary>

* Clone the Repository:

<img src="https://user-images.githubusercontent.com/42481638/216899046-0d41d7d6-8e4d-4ed2-9587-b57088af5214.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899061-b2ea274a-e88c-40ae-bf0b-4450b46e9fea.png" width="500">
<img src="https://user-images.githubusercontent.com/42481638/216899076-0e5988c4-d431-4284-a5ff-9ecff973db76.png" width="500">

---

### Building & Setup

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
</details>

## Troubleshooting

If your initial configure failed:
- *Carefully* re-read the [dependencies guide](Deps.md)
- Clear the CPM cache (`.cache/cpm`) and CMake cache (`<build directory>/CMakeCache.txt`)
- Evaluate the error and find any related settings
- See the [CPM docs](CPM.md) to see if you may need to forcefully bundle any packages

Otherwise, feel free to ask for help in Revolt or Discord.

## Caveats

Many platforms have quirks, bugs, and other fun stuff that may cause issues when building OR running. See the [Caveats page](Caveats.md) before continuing.

## Building & Running

### On Qt Creator

Simply hit Ctrl+B, or the "hammer" icon in the bottom left. To run, hit the "play" icon, or Ctrl+R.

### On Command Line

If you are using the `UNIX Makefiles` or `Visual Studio 17 2022` as `<GENERATOR>`, you should also add `--parallel` for faster build times.

```
cmake --build build
```

Your compiled executable will be in:
- `build/bin/eden.exe` for Windows,
- `build/bin/eden.app/Contents/MacOS/eden` for macOS,
- and `build/bin/eden` for others.

## Scripts

Some platforms have convenience scripts provided for building.

- **[Linux](scripts/Linux.md)**
- **[Windows](scripts/Windows.md)**

macOS scripts will come soon.
