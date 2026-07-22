@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64
set PATH=%PATH%;C:\Users\charl\AppData\Local\Microsoft\WinGet\Links
cd /d C:\Users\charl\Documents\SuyuEclipse
cmake -S . -B build-ninja -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_TOOLCHAIN_FILE=C:/Users/charl/Documents/SuyuEclipse/externals/vcpkg/scripts/buildsystems/vcpkg.cmake -DENABLE_QT=OFF -DSUYU_TESTS=OFF -DENABLE_WEB_SERVICE=ON -DSUYU_USE_EXTERNAL_SDL2=OFF -DSUYU_CMD=ON -DSUYU_USE_BUNDLED_FFMPEG=OFF
if errorlevel 1 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)
echo CMAKE CONFIGURE SUCCESS

cd /d C:\Users\charl\Documents\SuyuEclipse\build-ninja
ninja -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD SUCCESS
