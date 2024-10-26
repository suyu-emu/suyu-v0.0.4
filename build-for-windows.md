# Method I: MSVC Build for Windows (MS Visual Studio)

### MSVC: Overview

  * Install Minimal Dependencies (details and setup below)
      * Visual Studio Community 2022
      * CMake
      * Vulkan SDK
      * Python
      * Git for Windows
  * Build via Command Line (simplest if dependencies are installed correctly)
  * (or) Build with GUI Tools (backup graphical interface)


### MSVC: Install Minimal Dependencies

To build torzu on Windows with Visual Studio, you need to install:

  * **[Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)**
    * **Update to the latest version if already installed. (continued below)**

  ![](https://i.imgur.com/0jwV1hW.png)

  * **Visual Studio 2022 Community (continued)**
    * Be sure to select "Desktop development with C++"
    * Select the MSVC components outlined in red below (**especially VS 2019 build tools**.)

  ![](https://i.imgur.com/NtSnqjm.png)

  ![](https://i.imgur.com/YLr1Qw2.png)

  * **[CMake](https://cmake.org/download/)** - Used to generate Visual Studio project files. Choose 32-bit or 64-bit according to your system. 
    * **If it asks to be added to your system PATH, say YES.**

  ![](https://i.imgur.com/7pteS6d.png)

  * **[Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows)** - Make sure to select Latest SDK. 
    * **If it asks to be added to your system PATH, say YES.**

  ![](https://i.imgur.com/aHCJxsR.png)
  
  * **[Python](https://www.python.org/downloads/windows/)** - Select latest stable Windows installer. Choose 32-bit or 64-bit according to your system. 
    * **If it asks to be added to your system PATH, say YES.**

  ![](https://i.imgur.com/xIEuM6R.png)

  * **[Git for Windows](https://gitforwindows.org)** - (see next step)

  ![](https://i.imgur.com/UeSzkBw.png)

  * When installing Git, include it in your system PATH by choosing the "**Git from the command line and also from 3rd-party software**" option. 

  ![](https://i.imgur.com/x0rRs1t.png)

  * **REBOOT YOUR SYSTEM, to be sure all dependencies are registered before proceeding.**

---
---

## MSVC: Build from the Command Line

* Open a command line (cmd.exe), navigate to a directory where you want to download the repo, then pick one option to clone into a subdirectory named "torzu":

**from Notabug repo:**
```
git clone --depth 1 https://notabug.org/litucks/torzu.git
```
**from Torzu repo (assuming Tor is installed as a service):**
```
git -c http.proxy=socks5h://127.0.0.1:9050 clone --depth 1 http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu.git
```

* Assuming all dependencies were installed correctly, you should be able to continue from the above `git clone` and run the following commands to build:

```
cd torzu
git submodule update --init --recursive
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DYUZU_TESTS=OFF
cmake --build . --config Release
```
* You'll find the resulting files in the `build/bin/Release` folder. To make it a portable install with all AppData files local to the torzu folder, add a "user" folder:
```
cd bin
cd Release
mkdir user
```

* **ERRORS:** If you get an error after running the first cmake command (such as a missing library or CMakeLists.txt), first try running `git submodule update --init --recursive` from inside "torzu" folder again. If that doesn't work, try deleting the whole "torzu" folder and recloning via git from the beginning (as sometimes submodules will be incomplete without throwing an error.)

---
---

## MSVC: Build with GUI Tools (Graphical Interface)

* Open a command line (cmd.exe), navigate to a directory where you want to download the repo, then pick one option to clone into a subdirectory named "torzu":

**from Notabug repo:**
```
git clone --depth 1 https://notabug.org/litucks/torzu.git
```
**from Torzu repo (assuming Tor is installed as a service):**
```
git -c http.proxy=socks5h://127.0.0.1:9050 clone --depth 1 http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu.git
```
then download dependencies with:
```
cd torzu
git submodule update --init --recursive
```

<!--* *(Note: yuzu by default downloads to `C:\Users\<user-name>\yuzu` (Master) or `C:\Users\<user-name>\yuzu-mainline` (Mainline)*-->

* Open the CMake GUI application and point it to the `torzu` directory (instead of `yuzu-canary`).

  ![](https://i.imgur.com/qOslIWv.png)

* Use a `/build` subdirectory inside the `torzu` directory or some other directory of your choice. (Tell CMake to create it.)

  ![](https://i.imgur.com/cNnhs22.png)
  ![](https://github.com/yuzu-emu/yuzu/assets/20753089/738efcab-0da6-44ce-889d-becf3712db10)

* Click the "Configure" button and choose `Visual Studio 17 2022`, with `x64` for the optional platform.

  ![](https://i.imgur.com/qJcUeIg.png)

  * *(Note: If you used GitHub's own app to clone, run `git submodule update --init --recursive` to get the remaining dependencies)*

* **ERRORS:** If you get an error about missing packages, enable `YUZU_USE_BUNDLED_VCPKG`, and then click Configure again.

  * *(You may also want to disable `YUZU_TESTS` in this case since Catch2 is not yet supported with this.)*

  ![](https://user-images.githubusercontent.com/22451773/180585999-07316d6e-9751-4d11-b957-1cf57cd7cd58.png)

* **ERRORS:** If you get an error "Unable to find a valid Visual Studio instance", make sure that you installed the required MSVC components displayed above (**especially VS 2019 build tools**) and then try again.

* Click "Generate" to create the project files.

  ![](https://i.imgur.com/5LKg92k.png)

* Open the solution file `yuzu.sln` in Visual Studio 2022, which is located in the `build` directory.

  ![](https://i.imgur.com/208yMml.png)

* Select `yuzu` in the Solution Explorer, right-click and `Set as StartUp Project` (the yuzu, yuzu-cmd and yuzu-room executables will all be built.)

  ![](https://i.imgur.com/nPMajnn.png)  ![](https://i.imgur.com/BDMLzRZ.png)

* Select the appropriate build type, Debug for debug purposes or Release for performance (in case of doubt choose Release).

  ![](https://i.imgur.com/qxg4roC.png)

* Right-click the project you want to build and press Build in the submenu or press F5.

  ![](https://i.imgur.com/CkQgOFW.png)

* After build completed you can find the Torzu program files in a directory specified in the output log (usually `build\bin\Release`.)

  ![](https://i.imgur.com/h78ugDN.png)

---
---
---
---
---

# Method II: MinGW-w64 Build with MSYS2

## Prerequisites to install

* [MSYS2](https://www.msys2.org)
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) - **Make sure to select Latest SDK.**
* Make sure to follow the instructions and update to the latest version by running `pacman -Syu` as many times as needed. 

## Install other dependencies
* Open the `MSYS2 MinGW 64-bit` (mingw64.exe) shell

![](https://i.imgur.com/uZ33O7u.png)

* Download and install all dependencies using: `pacman -Syu git make mingw-w64-x86_64-SDL2 mingw-w64-x86_64-cmake mingw-w64-x86_64-qt5 mingw-w64-x86_64-toolchain`

## Setup environment variables
```
export PATH="<Absolute path to the Bin folder in Vulkan SDK>:$PATH"
export VCPKG_DEFAULT_HOST_TRIPLET=x64-mingw-static
export VCPKG_DEFAULT_TRIPLET=x64-mingw-static
```
We have to manually set some VCPKG variables for some reason.
This issue probably already exists in the original Yuzu.

## Clone the torzu repository with Git

Navigate to a directory where you want the repo, then use one option below to clone into a subdirectory named "torzu":

<!--![](https://i.imgur.com/CcxIAht.png)-->

**from NotABug repo:**
```
git clone --depth 1 https://notabug.org/litucks/torzu.git
```
**from Torzu repo (assuming Tor is installed as a service):**
```
git -c http.proxy=socks5h://127.0.0.1:9050 clone --depth 1 http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu.git
```
then download the submodule dependencies with:
```
cd torzu
git submodule update --init --recursive
```

## Generating makefile
```
mkdir build && cd build
cmake -G "MSYS Makefiles" -DYUZU_USE_BUNDLED_VCPKG=ON -DYUZU_TESTS=OFF -DVCPKG_TARGET_TRIPLET=x64-mingw-static ..
```
`DVCPKG_TARGET_TRIPLET` has to be overriden to `x64-mingw-static` here to generate a static build that doesn't require extra DLLs to be packaged.

## Build torzu
```
make -j$(nproc) yuzu
```
The reason we are not using `make all` is that linker will fail.
This is because Yuzu developer didn't set linker flags properly in their `CMakeLists.txt` for some reason. So we have add something manually.
```
VERBOSE=1 make yuzu
```
This will shows the exact link command, should be something like:
```
cd ***/src/yuzu && /mingw64/bin/c++.exe -O3 -DNDEBUG -Wl,--subsystem,windows -Wl,--whole-archive ...
```
Copy the command line and add the following arguments:
```
-static-libstdc++ -lws2_32 -s -Wl,--Map,../../bin/yuzu.map
```
Explanation of the extra arguments:
- `-static-libstdc++`: Force usage of static libstdc++, without this argument the binary will have no entrypoint.
- `-lws2_32`: Link the ws2_32.a provided by mingw.
- `-s`: Optional, strip the symbols from the output binary.
- `-Wl,--Map,../../bin/yuzu.map`: Optional, output a separated linker map to `../../bin/yuzu.map`
Please note that `-lw2_32` is already added, but the order is not correct and hence cause linking fails.

Now the built executable should work properly. Repeating step 4 should build `yuzu-cmd` as well.
Some DLLs (e.g., Qt) are still required as they cannot being linked statically. Copying those DLLs from the latest release is one option.

## Building without Qt (Optional)

Doesn't require the rather large Qt dependency, but you will lack a GUI frontend:

  * Pass the `-DENABLE_QT=NO` flag to cmake

---
---
---
---
---

# Method III: CLion Environment Setup

## Minimal Dependencies

To build yuzu, you need to install the following:

* [CLion](https://www.jetbrains.com/clion/) - This IDE is not free; for a free alternative, check Method I
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) - Make sure to select the Latest SDK.

## Cloning yuzu with CLion

* Clone the Repository:

![1](https://user-images.githubusercontent.com/42481638/216899046-0d41d7d6-8e4d-4ed2-9587-b57088af5214.png)

* using `https://notabug.org/litucks/torzu.git` (instead of the shown yuzu repo):

![2](https://user-images.githubusercontent.com/42481638/216899061-b2ea274a-e88c-40ae-bf0b-4450b46e9fea.png)
![3](https://user-images.githubusercontent.com/42481638/216899076-0e5988c4-d431-4284-a5ff-9ecff973db76.png)



## Building & Setup

* Once Cloned, You will be taken to a prompt like the image below:

![4](https://user-images.githubusercontent.com/42481638/216899092-3fe4cec6-a540-44e3-9e1e-3de9c2fffc2f.png)

* Set the settings to the image below:
* Change `Build type: Release`
* Change `Name: Release`
* Change `Toolchain Visual Studio`
* Change `Generator: Let CMake decide`
* Change `Build directory: build`

![5](https://user-images.githubusercontent.com/42481638/216899164-6cee8482-3d59-428f-b1bc-e6dc793c9b20.png)

* Click OK; now Clion will build a directory and index your code to allow for IntelliSense. Please be patient.
* Once this process has been completed (No loading bar bottom right), you can now build yuzu
* In the top right, click on the drop-down menu, select all configurations, then select yuzu

![6](https://user-images.githubusercontent.com/42481638/216899226-975048e9-bc6d-4ec1-bc2d-bd8a1e15ed04.png)

* Now run by clicking the play button or pressing Shift+F10, and yuzu will auto-launch once built.

![7](https://user-images.githubusercontent.com/42481638/216899275-d514ec6a-e563-470e-81e2-3e04f0429b68.png)

