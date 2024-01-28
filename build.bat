@echo off
call "GLSL\GLSL_packer.exe"

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set DebugFlags=/DPAINTTOOL_CODE_VERIFICATION=1 /Od
set CommonFlags=/GR- /GL /Gw /MT /Oi /nologo /FC /Zi
set WarningFlags=/W4 /WX /wd4100 /wd4201 /wd4310
set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF User32.lib Shell32.lib Kernel32.lib Gdi32.lib Opengl32.lib

IF NOT EXIST %~dp0\..\build mkdir %~dp0\..\build
pushd %~dp0\..\build

REM cl /O2 %CommonFlags% %WarningFlags% %~dp0\GLSL_packer.cpp /link /INCREMENTAL:NO /OPT:REF User32.lib

cl %DebugFlags% %CommonFlags% %WarningFlags% %~dp0\windows_painttool.cpp %LinkerFlags%
popd