@echo off

set chain=%1

if not defined DevEnvDir (
	CALL "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" %chain%
)

CALL mkdir build

CALL cmake -S . -B build\%chain% ^
-DCMAKE_BUILD_TYPE=Release ^
-DENABLE_QT_TRANSLATION=ON ^
-DUSE_DISCORD_PRESENCE=ON ^
-DYUZU_USE_BUNDLED_QT=ON ^
-DYUZU_USE_QT_MULTIMEDIA=ON ^
-DYUZU_USE_QT_WEB_ENGINE=ON ^
-DYUZU_USE_BUNDLED_VCPKG=ON ^
-DYUZU_USE_BUNDLED_SDL2=ON ^
-DYUZU_ENABLE_LTO=ON ^
-G "Ninja" ^
-DYUZU_TESTS=OFF 

CALL cmake --build build\%chain%
