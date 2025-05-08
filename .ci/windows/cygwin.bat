echo off

call C:\tools\cygwin\cygwinsetup.exe -q -P autoconf,automake,libtool,make,pkg-config

REM Create wrapper batch files for Cygwin tools in a directory that will be in PATH
REM uncomment this for first-run only
REM call mkdir C:\cygwin-wrappers

REM Create autoconf.bat wrapper
call echo @echo off > C:\cygwin-wrappers\autoconf.bat
call echo C:\tools\cygwin\bin\bash.exe -l -c "autoconf %%*" >> C:\cygwin-wrappers\autoconf.bat

REM Add other wrappers if needed for other Cygwin tools
call echo @echo off > C:\cygwin-wrappers\automake.bat
call echo C:\tools\cygwin\bin\bash.exe -l -c "automake %%*" >> C:\cygwin-wrappers\automake.bat

REM Add the wrappers directory to PATH
call echo C:\cygwin-wrappers>>"%GITHUB_PATH%"
call echo C:\tools\cygwin\bin>>"%GITHUB_PATH%"
