@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
set D=%~dp0
set D=%D:~0,-1%
cd /d "%D%"

echo === craft test AArch64 program: movz x0,#5; movz x1,#7; add x2,x0,x1; svc #0 ===
powershell -NoProfile -Command "[byte[]]$b=0xA0,0x00,0x80,0xD2,0xE1,0x00,0x80,0xD2,0x02,0x00,0x01,0x8B,0x01,0x00,0x00,0xD4; [IO.File]::WriteAllBytes('%D%\test.bin',$b)"

echo === run static recompiler ===
"%D%\suyu_recomp.exe" "%D%\test.bin" 0x1000 "%D%\out"
echo recomp_exit=%errorlevel%

echo === generated files ===
dir /b "%D%\out"

echo === compile generated native project ===
cd /d %D%\out
cl /nologo /O2 main.c recompiled.c recomp_runtime.c /Fe:recompiled.exe
echo cl_exit=%errorlevel%

echo === RUN recompiled native executable ===
"%D%\out\recompiled.exe"
echo run_exit=%errorlevel%
