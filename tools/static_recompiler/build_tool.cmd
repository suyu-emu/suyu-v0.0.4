@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64
cd /d C:\Users\charl\Documents\SuyuEclipse\tools\static_recompiler
cl /nologo /EHsc /O2 /std:c++17 suyu_recomp.cpp /Fe:suyu_recomp.exe
echo CL_EXIT=%errorlevel%
