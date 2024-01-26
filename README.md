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

# File Receiving Window
The first functionality I decided to add to this software is to display image files. For this purpose we need a window which can display the image and receive file handles through drag and drop interactions.

To work with Windows, we need to `#include <windows.h>` and enter our code inside of [`WinMain`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-winmain) instead of `main`. In there we first [Register a Window Class](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa) and then [Create a Window](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa). We require the [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) for the `WS_EX_ACCEPTFILES` flag to enable drag and drop inputs. At the bottom of the documentation of the two functions we now utilize, you'll find a [Requirements](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa#requirements) section. Here it's noted that they both require the library `User32.lib`. We need to append it to our `LinkerFlags` like this:
```bat
set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF User32.lib
```
What's left to make the window functional is to create a message dispatching loop:
```cpp
for(;;)
{
    MSG Message;
    BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);
    if(MessageResult > 0)
    {
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
    }
    else
    {
        break;
    }
}
```
To respond to user inputs, we now need to define the [Window Processor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc) assigned to the Window Class. For now we shall only process [`WM_DROPFILES`](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles) messages and let everything else be processed by [DefWindowProcA](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowproca). To process the dropped files we'll retrieve the first file address using [`DragQueryFileA`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea) and hand it to a dummy function see if the drag and drop works. Make sure to note that [DragQueryFileA](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea#requirements) and [DragFinish](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragfinish#requirements) require `Shell32.lib`.
```cpp
case WM_DROPFILES:
{
    HDROP DropHandle = (HDROP)WParam;
    char FilePath[MAX_PATH];
    DragQueryFileA(DropHandle, 0, FilePath, MAX_PATH);
    DragFinish(DropHandle);
    
    ReadImageFromFile(FilePath, Window);
} break;
```
To make debugging easier in the future, we'll also forward the command line parameters to `ReadImageFromFile`.
