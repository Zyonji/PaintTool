# Preface
This page documents my process of developing my own standalone painting software. The goal is to create a single executable, which runs on most Windows systems without any additional libraries. This won't be a tutorial to follow along step by step; instead I shall outline my decision process sources I used.

# Development Environment
I like it simple. All code is written in a text editor and then compiled via Visual Studio command line, using a [`build.bat`](/build.bat) file like this:

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

# Platform Code
In this section I create a generic window, which accepts files through drag and drop and renders a simple screenshot from the clipboard using GLSL shaders through OpenGL. If you got a library to handle all that, feel free to skip.

[Platform Code](/docs/PlatformCode.md)

# Image File Decoders
Once the image data is in RAM, we'll test if it's one of the image formats we know one by one. Here it's important to not just rely on the suffix of the file name; it's not uncommon to find a `JFIF` file with the suffix `.png`. The decoding shall happen in two steps. First a file format specific step that returns a data structure and memory for an image. And then a universal step converts that data into a texture in our desired color space.

We'll start with the following formats and add more when needed:
- [`.bmp` Decoder](/docs/Bitmap.md)
- [`.png` Decoder](/docs/PNG.md)
- [`.jpg` Decoder](/docs/JPEG.md)
- `.webp` Decoder

In each section we prepare the data to be processed by a [General Image Decoder](/docs/GeneralImageDecoder.md).