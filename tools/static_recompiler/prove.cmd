@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul
cd /d C:\Users\charl\Documents\SuyuEclipse\tools\static_recompiler

echo === build recompiler tool ===
cl /nologo /EHsc /O2 /std:c++17 suyu_recomp.cpp /Fe:suyu_recomp.exe || exit /b 1

echo === craft test AArch64 program (movz x0,#5; movz x1,#7; add x2,x0,x1; svc #0) ===
powershell -NoProfile -Command "[byte[]]$b=0xA0,0x00,0x80,0xD2, 0xE1,0x00,0x80,0xD2, 0x02,0x00,0x01,0x8B, 0x01,0x00,0x00,0xD4; [IO.File]::WriteAllBytes('test.bin',$b)"

echo === run recompiler: test.bin @0x1000 -> out\ ===
suyu_recomp.exe test.bin 0x1000 out || exit /b 1

echo === compile generated native project ===
cd out
cl /nologo /O2 main.c recompiled.c recomp_runtime.c /Fe:recompiled.exe || exit /b 1

echo === RUN recompiled native binary ===
recompiled.exe
echo === exit code %errorlevel% ===
endlocal
