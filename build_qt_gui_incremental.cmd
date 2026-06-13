@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64
set PATH=%PATH%;C:\Users\charl\AppData\Local\Microsoft\WinGet\Links
cd /d C:\Users\charl\Documents\SuyuEclipse\build-qt-gui
echo === NINJA START ===
ninja -j%NUMBER_OF_PROCESSORS% suyu
set NINJA_RC=%errorlevel%
echo === NINJA EXIT %NINJA_RC% ===
exit /b %NINJA_RC%
