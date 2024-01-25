# Preface
This page documents my process of developing my own standalone painting software. The goal is to create a single executable, which runs on most Windows systems without any additional libraries. This won't be a tutorial to follow along step by step; instead I shall outline my decision process sources I used.

# Development Environment
I like it simple. All code is written in a text editor and then compiled via Visual Studio command line, using a `.bat` file.

#### build.bat

```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set BuildFlags=/Od /GR- /GL /Gw /MT /Oi /nologo /FC /Zi
set WarningFlags=/W4 /WX /wd4100 /wd4201 /wd4310
set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF

IF NOT EXIST %~dp0\..\build mkdir %~dp0\..\build
pushd %~dp0\..\build

cl %BuildFlags% %WarningFlags% %~dp0\windows_painttool.cpp %LinkerFlags%
popd
```

I expect you to understand the [Batch Script](https://en.wikipedia.org/wiki/Batch_file) syntax and the relevant [Windows Commands](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/windows-commands).  The `cl` command defined by calling `vcvarsall.bat` of the Visual Studio version of your choice.

The batch file has three groupings of [Compiler Options](https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options).
- `BuildFlags`: The flags for optimization and debugging.  At the end of development, `Od` shall be replaced with `O2`.
- `WarningFlags`: This sets all warnings to be treated as error and suppress specific [Compiler Warnings](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/c-cpp-build-errors) with `wdXXXX`flags.
- `LinkerFlags`: These flags set the [Linker Options](https://learn.microsoft.com/en-us/cpp/build/reference/linker-options) and later we append the names of libraries necessary for desired Windows functionality.