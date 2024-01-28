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
To respond to user inputs, we now need to define the [Window Message Processor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc) assigned to the Window Class. For now we shall only process [`WM_DESTROY`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-destroy), [`WM_CLOSE`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close) and [`WM_DROPFILES`](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles) messages and let everything else be processed by [`DefWindowProcA`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowproca). The Destroy and close messages should evoke [`PostQuitMessage`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage) to terminate our application code. To process the dropped files we'll retrieve the first file address using [`DragQueryFileA`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea) and hand it to a dummy function see if the drag and drop works. Make sure to note that [`DragQueryFileA`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea#requirements) and [`DragFinish`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragfinish#requirements) require `Shell32.lib`.
```cpp
case WM_DESTROY:
case WM_CLOSE:
{
    PostQuitMessage(0);
} break;

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

# Initializing OpenGL
I decided to tackle loading shaders through OpenGL next instead of decoding an image file. That way it'll be easier to spot errors in the decoding by looking at the rendered image.

## Linking to the OpenGL functions
Windows by default only links to OpenGL version 1.1 functions. To get newer functions we need to use [`wglGetProcAddress`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress) to look up their function pointers. However, `wglGetProcAddress` requires a rendering context selected through [`wglMakeCurrent`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent), which in turn requires a call to [`wglCreateContext`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext), which requires
[`SetPixelFormat`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat), which notes in it's [Remarks](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat#remarks) that setting the pixel format of a window more than once is disallowed. That means, if we want to load functions used for setting a pixel format, then our actual first step is to create another temporary window, who's rendering context we use for linking to [`wglChoosePixelFormatARB`](https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt) and [`wglCreateContextAttribsARB`](https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt).
```cpp
HDC DummyWindowDC = GetDC(DummyWindow);

PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
DesiredPixelFormat.nVersion = 1;
DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
DesiredPixelFormat.cColorBits = 24;
DesiredPixelFormat.cDepthBits = 32;
DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
MatchingPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);

PIXELFORMATDESCRIPTOR MatchingPixelFormat;
DescribePixelFormat(WindowDC, MatchingPixelFormatIndex, sizeof(MatchingPixelFormat), &MatchingPixelFormat);
SetPixelFormat(WindowDC, MatchingPixelFormatIndex, &MatchingPixelFormat);
    
HGLRC OpenGLRC = wglCreateContext(DummyWindowDC);
if(wglMakeCurrent(WindowDC, OpenGLRC))
{
    wglChoosePixelFormatARB = (type_wglChoosePixelFormatARB *)wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (type_wglCreateContextAttribsARB *)wglGetProcAddress("wglCreateContextAttribsARB");
    wglMakeCurrent(0, 0);
}
            
wglDeleteContext(OpenGLRC);
ReleaseDC(DummyWindow, DummyWindowDC);
DestroyWindow(DummyWindow);
```
Now we set a rendering context on the main window using those functions and link to other functions we need using `wglGetProcAddress` again.

## Initializing GLSL shader programs
The next goal will be to draw to the window with a custom shader using [`glDrawArrays`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml). The [Shaders](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCreateShader.xhtml) need to be attached to a [Program](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCreateProgram.xhtml). To draw with the program, we need a [Vertex Array](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenVertexArrays.xhtml) of which we need to associate [Attributes](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml) with pointers in our Program.

For now, we'll work with a simple vertex array, with 4 corners that cover the render space and UV space.
```cpp
GLuint VertexArray;
glGenVertexArrays(1, &VertexArray);
glBindVertexArray(VertexArray);

GLuint VertexBuffer;
glGenBuffers(1, &VertexBuffer);
glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
common_vertex Vertices[4] =
{
    {{-1, -1, 0, 1}, {0, 1}},
    {{-1,  1, 0, 1}, {0, 0}},
    {{ 1, -1, 0, 1}, {1, 1}},
    {{ 1,  1, 0, 1}, {1, 0}}
};
glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
```
And we load the shaders from null terminated strings.
```cpp
GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(VertexShaderID, ArrayCount(&VertexShaderCode), &VertexShaderCode, 0);
glCompileShader(VertexShaderID);

GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(FragmentShaderID, ArrayCount(&FragmentShaderCode), &FragmentShaderCode, 0);
glCompileShader(FragmentShaderID);

GLuint ProgramID = glCreateProgram();
glAttachShader(ProgramID, VertexShaderID);
glAttachShader(ProgramID, FragmentShaderID);
glLinkProgram(ProgramID);

glDeleteShader(VertexShaderID);
glDeleteShader(FragmentShaderID);
```
The shader code is a null terminated string containing code written in GLSL.  We use [Raw String Literals](https://learn.microsoft.com/en-us/cpp/cpp/string-and-character-literals-cpp) encode it like this:
```cpp
char *VertexShaderCode = R"glsl(
#version 330

in vec4 VertP;
in vec2 VertUV;

smooth out vec2 FragUV;

void main(void)
{
    gl_Position = VertP;
    FragUV = VertUV;
}
)glsl";
```
Writing code in string form like that is a little inconvenient.  Because of that I'll write a simple executable to call before compiling, which takes `.glsl` files from a directory and packs them into `.cpp` file as strings. It would be possible to dynamically load the shaders from such files at runtime, but then our painting software wouldn't work as standalone executable anymore.

## Paint the window using the Shaders
Now it's time to back to our window message processor and listen for [`WM_PAINT`](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint) requests to paint the window. Windows expects you to draw on a specified subsection of the window and then validate it, so that Windows then can check what other subsections of the window still need to be redrawn.  This kind of optimization is not necessary for us, so instead we'll redraw the entire window and also [Validate](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-validaterect) all of it with every paint request.
```cpp
RECT ClientRect;
GetClientRect(Window, &ClientRect);
int Width = ClientRect.right - ClientRect.left;
int Height = ClientRect.bottom - ClientRect.top;

HDC WindowDC = GetDC(Window);
wglMakeCurrent(WindowDC, OpenGLRenderingContext);
/* OpenGL rendering code goes here */
SwapBuffers(wglGetCurrentDC());
ReleaseDC(Window, WindowDC);
ValidateRect(Window, 0);
```
The OpenGL rendering call happens in a few steps. First we need to set the [Viewport](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glViewport.xhtml) to the current size of the window, because it's set to the size the window was on initialization. Next we [Enable Vertex Attributes](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glEnableVertexAttribArray.xhtml) used by our shader and [Link those Attributes](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml) to our Shader code. Then we draw our default vertex array using [`glDrawArrays`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml). If we want to use other programs, then we want to [Disable Vertex Attributes](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glEnableVertexAttribArray.xhtml) before the next program we use. Once [`SwapBuffers`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-swapbuffers) is called, the output of our shader should be visible. You can draw a simple checker pattern with this fragment shader code:
```glsl
out vec4 FragmentColor;

void main(void)
{
    float Checkered = 0.25 + 0.5 * mod(floor(gl_FragCoord.x / 8) + floor(gl_FragCoord.y / 8), 2);

    FragmentColor = vec4(vec3(Checkered), 1.0);
}
```
