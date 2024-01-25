@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set BuildFlags=/Od /GR- /GL /Gw /MT /Oi /nologo /FC /Zi
set WarningFlags=/W4 /WX /wd4100 /wd4201 /wd4310
set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF

IF NOT EXIST %~dp0\..\build mkdir %~dp0\..\build
pushd %~dp0\..\build

cl %BuildFlags% %WarningFlags% %~dp0\windows_painttool.cpp %LinkerFlags%
popd