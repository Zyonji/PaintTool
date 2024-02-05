# File Receiving Window
The first functionality I decided to add to this software is the ability to display image files. For this purpose we want a window which can display the image and receive file handles through drag and drop interactions.

To work with Windows, we need to `#include <windows.h>` and enter our code inside of [`WinMain`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-winmain) instead of `main`. In there, we first [Register a Window Class](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa) and then [Create a Window](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa). We require the [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) for the `WS_EX_ACCEPTFILES` flag to enable drag and drop inputs. At the bottom of the documentation of the windows functions we now utilize, you'll find a [Requirements](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa#requirements) section. Here it's noted that they both require the library `User32.lib`. We need to append it to our `LinkerFlags` like this:
```bat
set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF User32.lib
```
What's left to make the window functional is to create a message dispatching loop:
```cpp
while(GetMessageA(&Message, 0, 0, 0) > 0)
{
    TranslateMessage(&Msg);
    DispatchMessage(&Msg);
}
```
To respond to user inputs, we now need to define the [Window Message Processor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc) assigned to the Window Class. For now we shall only process [`WM_DESTROY`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-destroy), [`WM_CLOSE`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-close) and [`WM_DROPFILES`](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles) messages. Every other message should be processed by [`DefWindowProcA`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowproca). The destroy and close messages should evoke [`PostQuitMessage`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage) to terminate our application code. To process the dropped files we'll retrieve the first file address using [`DragQueryFileA`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea) and hand it to a dummy function see if the drag and drop works. Make sure to note that [`DragQueryFileA`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragqueryfilea#requirements) and [`DragFinish`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragfinish#requirements) require `Shell32.lib`.
```cpp
case WM_CLOSE:
case WM_DESTROY:
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
I decided to tackle loading shaders through OpenGL next instead of decoding an image file. That way it'll be easier to spot decoding errors by looking at the rendered image.

## Linking to the OpenGL functions
Windows by default only links to OpenGL version 1.1 functions. To get newer functions we need to use [`wglGetProcAddress`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress) to look up their function pointers. However, `wglGetProcAddress` requires a rendering context selected through [`wglMakeCurrent`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent), which in turn requires a call to [`wglCreateContext`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext), which requires
[`SetPixelFormat`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat), which notes in it's [Remarks](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setpixelformat#remarks) that setting the pixel format of a window more than once is disallowed. That means, if we want to use newer functions for setting a pixel format, then our actual first step is to create another temporary window, which's rendering context we use for linking to [`wglChoosePixelFormatARB`](https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt) and [`wglCreateContextAttribsARB`](https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt).
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
The next goal will be to draw to the window with a custom shader using [`glDrawArrays`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml). The [Shaders](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCreateShader.xhtml) need to be attached to a [Program](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glCreateProgram.xhtml). To draw with the program, we need a [Vertex Array](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenVertexArrays.xhtml) of which we need to associate [Attributes](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml) with pointers in our program.

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
Writing code in string form like that is a little inconvenient.  Because of that I'll write a [Simple Executable](/GLSL_packer.cpp) to call before compiling, which takes `.glsl` files from a directory and packs them into `.cpp` file as strings. It would be possible to dynamically load the shaders from such files at runtime, but then our painting software wouldn't work as standalone executable anymore.

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
SwapBuffers(WindowDC);
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

# Displaying an Image
Instead of dealing with the challenge of figuring out the file format of a given file, I'll start with a simpler case where the data format is specified. The next goal will be to move a screenshot from the [Clipboard](https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard) into a [Texture](https://registry.khronos.org/OpenGL-Refpages/gl4/html/texture.xhtml), accessible by the fragment shader, to paint it into the window.

## Handling Keyboard Input
Copy, cut and paste operations are initialized by the user through keyboard inputs.  Those are split into four different messages: [`WM_KEYDOWN`](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown), [`WM_KEYUP`](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keyup), [`WM_SYSKEYDOWN`](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeydown) and [`WM_SYSKEYUP`](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeyup). The two "SYS" variants are sent, while the `ALT` key is being held and specifically if `F10` is pressed. That is because the [`Default Window Procedure`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowproca) is switching the focus to the window menu when `F10` is pressed; an inconvenient functionality for our window without a menu. If we handle all four messages ourselves, then we can make free use of the `F10` key later.  This however also removes the `ALT + F4` shortcut to close the window as well, so we need to implement that ourselves. Another peculiarity of those messages is that Windows will repeatedly send them if a key is held down for long enough. To only process actual key presses, we need to look at the bits of the `LParam`. The [Keystroke Message Flags](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags) are the same for all four message types, allowing us to handle multiple with the same code:
```cpp
case WM_KEYDOWN:
case WM_SYSKEYDOWN:
{
    bool IsCrltDown = GetKeyState(VK_CONTROL) & (1 << 7);
    bool IsAltDown = LParam & (1 << 29);
    bool WasKeyAlreadyDown = LParam & (1 << 30);
    switch(WParam)
    {
        case VK_F4:
            if(IsAltDown && !WasKeyAlreadyDown && !IsCrltDown)
                PostMessageA(Window, WM_CLOSE, 0, 0);
        break;

        case 'V':
            if(!IsAltDown && !WasKeyAlreadyDown && IsCrltDown)
                PasteClipboard(Window);
        break;
    }
} break;
```

## Accessing the Windows Clipboard
Now we can focus on retrieving an image from the [Clipboard](https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard). Windows stores data of the clipboard in multiple formats at once, some [Standard Formats](https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats) and others defined by applications. We want to retrieve an image, so we'll use [`GetPriorityClipboardFormat`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getpriorityclipboardformat) to choose the format best fit for our goals. To get started, we'll look for the device independent bitmap format `CF_DIBV5` (Windows automatically generates it when `CF_BITMAP` or `CF_DIB` is available) and the drop handle format `CF_HDROP`, which we can forward to the drag and drop code we've already written. For the bitmap, the handle given by [`GetClipboardData`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata) needs to be passed to [`GlobalLock`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globallock) to access the memory associated with it.
```cpp
UINT PriorityList[] = {CF_DIBV5, CF_HDROP};
int ClipboardFormat = GetPriorityClipboardFormat(PriorityList, ArrayCount(PriorityList));

if(ClipboardFormat == 0 || ClipboardFormat == -1 || !OpenClipboard(Window))
    return;

switch(ClipboardFormat)
{
    case CF_DIBV5:
    {
        HANDLE ClipboardHandle = GetClipboardData(ClipboardFormat);
        void *DataPointer = GlobalLock(ClipboardHandle);
        DisplayBMP((BITMAPV5HEADER *)DataPointer, GlobalSize(ClipboardHandle), Window);
        GlobalUnlock(DataPointer);
    } break;
    
    case CF_HDROP:
    {
       DisplayDroppedFile((HDROP)GetClipboardData(ClipboardFormat), Window);
    } break;
}
CloseClipboard();
```
What's left is to look through members of the [`BITMAPV5HEADER`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header) data structure to see how each should be handled.

## Displaying a Texture
Our shader accesses image data as a [Texture](https://registry.khronos.org/OpenGL-Refpages/gl4/html/texture.xhtml) which we set by binding a texture buffer using [`glBindTexture`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindTexture.xhtml). To put our image into the GPU memory, we first request a name for where it's stored with [`glGenTextures`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenTextures.xhtml) and fill it with [`glTexImage2D`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml). For a simple screenshot taken with `PrintScreen` we set the texture like this:
```cpp
glGenTextures(1, &ImageTextureHandle);
glBindTexture(GL_TEXTURE_2D, ImageTextureHandle);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_BGRA, GL_UNSIGNED_BYTE, Memory);
```
After this, we'd also want to call [`glGenerateMipmap`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGenerateMipmap.xhtml) and [`glTexParameteri`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml) to make sure the picture is displayed right, even if it doesn't match the window size. To show the texture, we ask windows to redraw the window using [`InvalidateRect`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-invalidaterect). Something to look out for is that a BMP with positive height draws from the bottom up, contrary to how most other image formats work. A negative height draws from the top down.

## Rendering with the Correct Aspect Ratio
To finish this section up, I'll add a few lines to render the image in the right aspect ratio. Having it always match the window size makes it more difficult to spot errors of the decoding process. We simply use a second [Viewport](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glViewport.xhtml) call after clearing the window to only paint a subsection, which matches the aspect ratio of the image. We also want to [`InvalidateRect`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-invalidaterect) for each `WM_SIZING` message, so we can test the code we just wrote.

## Loading a File into Memory
We currently load image data into memory through the device independent bitmap of the clipboard. This is the data of a common BMP file without the file header. Through the clipboard we could also access the data of a tagged-image file (TIFF) and even a metafile picture format structure (an old image format I personally haven't seen used anywhere). What's left to handle is all the other input methods that give us null terminated strings containing a file path.

[`CreateFile`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea) takes such a file path and returns a file handle. The size of the file associated with the handle can be determined using [`GetFileSizeEx`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex). From here we have two options. The straight forward way is to [Allocate Memory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc), with the size of the file, and then store the file data in that memory using [`ReadFile`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile). With large files, we then would need to either read the entirety of the file into memory all at once, or write additional code to load more data when needed.

Alternatively, we can use [File Mapping](https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping) to page a read only reference of the file data into memory, which gets loaded Windows memory management when we try to access it. Our previous dummy function `DisplayImageFromFile` gets replaced with this function calling a new dummy function `DisplayImageFromData`:
```cpp
static void
DisplayImageFromFile(char *FilePath, HWND Window)
{
    HANDLE FileHandle = CreateFileA(FilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle)
    {
        LARGE_INTEGER FileSize;
        GetFileSizeEx(FileHandle, &FileSize);
        if(FileSize.QuadPart)
        {
            HANDLE FileMappingHandle = CreateFileMappingA(FileHandle, 0, PAGE_READONLY, 0, 0, 0);
            if(FileMappingHandle)
            {
                void *FileMemory = MapViewOfFile(FileMappingHandle, FILE_MAP_READ, 0, 0, 0);
                if(FileMemory)
                {
                    DisplayImageFromData(FileMemory, FileSize.QuadPart);
                    
                    UnmapViewOfFile(FileMemory);
                }
                CloseHandle(FileMappingHandle);
            }
        }
        CloseHandle(FileHandle);
    }
}
```