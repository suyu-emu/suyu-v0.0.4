echo off

set chain=%1

if not defined DevEnvDir (
	"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" %chain%
)

mkdir build

cmake -S . -B build\%chain% ^
-DCMAKE_BUILD_TYPE=Release ^
-DYUZU_USE_BUNDLED_QT=ON ^
-DENABLE_QT_TRANSLATION=ON ^
-DUSE_DISCORD_PRESENCE=ON ^
-DYUZU_USE_BUNDLED_VCPKG=ON ^
-DYUZU_USE_BUNDLED_SDL2=ON ^
-G "Ninja" ^
-DYUZU_TESTS=OFF ^
-DCMAKE_C_COMPILER_LAUNCHER=ccache ^
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache ^
-DCMAKE_TOOLCHAIN_FILE="%CD%\CMakeModules\MSVCCache.cmake" ^
-DUSE_CCACHE=ON

cmake --build build\%chain%

ccache -s -v