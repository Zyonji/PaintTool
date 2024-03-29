#include "types.h"// Definition of variable types.

/*
PAINTTOOL_CODE_VERIFICATION:
- 0 - Optional checks correct execution of code are skipped.
- 1 - Check for the correct execution of code when possible.
*/
#if PAINTTOOL_CODE_VERIFICATION

#define Assert(Expression) if(!(Expression)) {__debugbreak();}

#else

#define Assert(Expression)

#endif

// Platform independent segment with forward declaration of required platform specific functions.
#include "fileprocessor/imageprocessor.h"

void LogError(char*, char*);
void* RequestImageBuffer(u64);
void FreeImageBuffer(void*);
void StoreImage(void*, image_processor_tasks);
void OutputDebugNumber(s32 Number, u8 BitCount);

#include "fileprocessor/imageprocessor.cpp"

// OpenGL segment.

#include <windows.h>
#include <GL/gl.h>
#include "wgl.h"// Declaration of OpenGL function pointers and constants.
#include "openGL_render.cpp"

//Platform specific segment

#include "windows_painttool.h"

static win_global Global;

void
LogError(char *Text, char *Caption)
{
    // TODO(Zyonji): Create a file to document errors for remote debugging.
    MessageBox(NULL, Text, Caption, MB_OK | MB_ICONHAND);
    Assert(false);
}

void
OutputDebugNumber(s32 Number, u8 BitCount)
{
    char Buffer[256];
    wsprintf(Buffer, "Number: %i | %i | %u\n", Number, (Number / 8 + 128),  BitCount);
    OutputDebugStringA(Buffer);
}

static void
SetPixelFormat(HDC WindowDC)
{
    int MatchingPixelFormatIndex = 0;
    GLuint MatchingPixelFormatCount = 0;
    if(wglChoosePixelFormatARB)
    {
        int IntAttribList[] =
        {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            0,
        };
        wglChoosePixelFormatARB(WindowDC, IntAttribList, 0, 1, &MatchingPixelFormatIndex, &MatchingPixelFormatCount);
    }
    
    if(MatchingPixelFormatCount == 0)
    {
        PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
        DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
        DesiredPixelFormat.nVersion = 1;
        DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
        DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
        DesiredPixelFormat.cColorBits = 24;
        DesiredPixelFormat.cDepthBits = 32;
        DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
        
        MatchingPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    }
    
    PIXELFORMATDESCRIPTOR MatchingPixelFormat;
    DescribePixelFormat(WindowDC, MatchingPixelFormatIndex, sizeof(MatchingPixelFormat), &MatchingPixelFormat);
    SetPixelFormat(WindowDC, MatchingPixelFormatIndex, &MatchingPixelFormat);
}

static b32
InitOpenGL(HINSTANCE Instance, HWND Window)
{
    {//Creating a temporaty Window for loading WGL functions.
        WNDCLASSA WindowClass = {};
        WindowClass.lpfnWndProc = DefWindowProcA;
        WindowClass.hInstance = Instance;
        WindowClass.lpszClassName = WGL_LOADER_WINDOW_CLASS_NAME;
        
        if(RegisterClassA(&WindowClass))
        {
            HWND DummyWindow = CreateWindowExA(
                0,
                WindowClass.lpszClassName,
                WGL_LOADER_WINDOW_NAME,
                0,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                WindowClass.hInstance,
                0);
            
            HDC DummyWindowDC = GetDC(DummyWindow);
            SetPixelFormat(DummyWindowDC);
            HGLRC OpenGLRC = wglCreateContext(DummyWindowDC);
            if(wglMakeCurrent(DummyWindowDC, OpenGLRC))
            {
                wglChoosePixelFormatARB = (type_wglChoosePixelFormatARB *)wglGetProcAddress("wglChoosePixelFormatARB");
                wglCreateContextAttribsARB = (type_wglCreateContextAttribsARB *)wglGetProcAddress("wglCreateContextAttribsARB");
                wglMakeCurrent(0, 0);
            }
            
            wglDeleteContext(OpenGLRC);
            ReleaseDC(DummyWindow, DummyWindowDC);
            DestroyWindow(DummyWindow);
        }
    }
    
    HDC WindowDC = GetDC(Window);
    SetPixelFormat(WindowDC);
    
    HGLRC OpenGLRC = 0;
    int Win32OpenGLAttribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };
    
    if(wglCreateContextAttribsARB)
    {
        OpenGLRC = wglCreateContextAttribsARB(WindowDC, 0, Win32OpenGLAttribs);
    }
    if(!OpenGLRC)
    {
        OpenGLRC = wglCreateContext(WindowDC);
    }
    
    if(!wglMakeCurrent(WindowDC, OpenGLRC))
    {
        ReleaseDC(Window, WindowDC);
        LogError("Unable to set OpenGL context.", "OpenGL");
        return(false);
    }
    if(!LoadOpenGLFunctions())
    {
        ReleaseDC(Window, WindowDC);
        LogError("Unable to link to all OpenGL", "OpenGL");
        return(false);
    }
    if(!OpenGLInitPrograms(&Global.OpenGL))
    {
        ReleaseDC(Window, WindowDC);
        LogError("Unable to initialize OpenGL.", "OpenGL");
        return(false);
    }
    
    ReleaseDC(Window, WindowDC);
    
    Global.Initialized = true;
    Global.RenderingContext = OpenGLRC;
    
    return(true);
}

// The returned buffer must be all 0.
void *
RequestImageBuffer(u64 DataSize)
{
    // TODO(Zyonji): Set a maximum Size for image buffers
    // TODO(Zyonji): Make a more complete memory manager.
    void *DataMemory = 0;
    DataMemory = VirtualAlloc(0, DataSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    
    return(DataMemory);
}

void
FreeImageBuffer(void *DataMemory)
{
    VirtualFree(DataMemory, 0, MEM_RELEASE);
}

void
StoreImage(void *Bitmap, image_processor_tasks Processor)
{
    // TODO(Zyonji): Set a maximum size for image buffers.
    if(Global.Initialized)
    {
        SetImageBuffer(&Global.OpenGL, Bitmap, Processor);
    }
}

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
                    u8 *FileEndpoint = (u8 *)FileMemory + FileSize.QuadPart;
                    DisplayImageFromData(FileMemory, FileEndpoint);
                    
                    InvalidateRect(Window, 0, true);
                    UnmapViewOfFile(FileMemory);
                }
                
                CloseHandle(FileMappingHandle);
            }
        }
        
        CloseHandle(FileHandle);
    }
}

static void
DisplayDroppedFile(HDROP DropHandle, HWND Window)
{
    char FilePath[MAX_PATH];
    u32 FileCount = DragQueryFileA(DropHandle, 0xffffffff, FilePath, MAX_PATH);
    
    while(FileCount--)
    {
        DragQueryFileA(DropHandle, FileCount, FilePath, MAX_PATH);
        // TODO(Zyonji): Handle multiple files instead of just the first file.
        
        DisplayImageFromFile(FilePath, Window);
    }
    
    DragFinish(DropHandle);
}

static void
PasteClipboard(HWND Window)
{
    UINT PriorityList[] = 
    {
        CF_DIBV5,
        CF_HDROP
    };
    int ClipboardFormat = GetPriorityClipboardFormat(PriorityList, ArrayCount(PriorityList));
    
    if(ClipboardFormat == 0 || ClipboardFormat == -1)
    {
        return;
    }
    if(!OpenClipboard(Window))
    {
        return;
    }
    
    switch(ClipboardFormat)
    {
        case CF_DIBV5:
        {
            HANDLE ClipboardHandle = GetClipboardData(ClipboardFormat);
            SIZE_T DataSize = GlobalSize(ClipboardHandle);
            void *DataMemory = VirtualAlloc(0, DataSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            void *DataPointer = GlobalLock(ClipboardHandle);
            CopyMemory(DataMemory, DataPointer, DataSize);
            GlobalUnlock(DataPointer);
            
            BMP_Reader((BMP_Win32BitmapHeader *)DataMemory, (u8 *)DataMemory + DataSize, 0);
            
            VirtualFree(DataMemory, 0, MEM_RELEASE);
            InvalidateRect(Window, 0, true);
        } break;
        
        case CF_HDROP:
        {
            DisplayDroppedFile((HDROP)GetClipboardData(ClipboardFormat), Window);
        } break;
    }
    CloseClipboard();
}

LRESULT CALLBACK
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch(Message)
    {
        case WM_DESTROY:
        case WM_CLOSE:
        {
            PostQuitMessage(0);
        } break;
        
        case WM_SIZING:
        {
            InvalidateRect(Window, 0, true);
        } break;
        
        case WM_PAINT:
        {
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            Global.OpenGL.Window.Width = ClientRect.right - ClientRect.left;
            Global.OpenGL.Window.Height = ClientRect.bottom - ClientRect.top;
            
            if(Global.Initialized)
            {
                HDC WindowDC = GetDC(Window);
                wglMakeCurrent(WindowDC, Global.RenderingContext);
                DisplayBuffer(&Global.OpenGL);
                SwapBuffers(WindowDC);
                ReleaseDC(Window, WindowDC);
                
                ValidateRect(Window, 0);
            }
        } break;
        
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            b32 IsCrltDown = GetKeyState(VK_CONTROL) & (1 << 7);
            b32 IsAltDown = LParam & (1 << 29);
            b32 WasKeyAlreadyDown = LParam & (1 << 30);
            switch(WParam)
            {
                case VK_F4:
                {
                    if(IsAltDown && !WasKeyAlreadyDown && !IsCrltDown)
                    {
                        PostMessageA(Window, WM_CLOSE, 0, 0);
                    }
                } break;
                
                case 'V':
                {
                    if(!IsAltDown && !WasKeyAlreadyDown && IsCrltDown)
                    {
                        PasteClipboard(Window);
                    }
                } break;
            }
        } break;
        
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            
        } break;
        
        case WM_DROPFILES:
        {
            DisplayDroppedFile((HDROP)WParam, Window);
        } break;
        
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }
    
    return(Result);
}

int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASS WindowClass = {};
    
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = PAINT_TOOL_WINDOW_CLASS_NAME;
    
    if(!RegisterClassA(&WindowClass))
    {
        LogError("Unable to register a window class.", "Window Constructor");
        return(0);
    }
    
    HWND Window = CreateWindowExA(
        WS_EX_ACCEPTFILES,
        WindowClass.lpszClassName,
        PAINT_TOOL_WINDOW_NAME,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        Instance,
        0);
    
    if(!Window)
    {
        LogError("Unable to create a window.", "Window Constructor");
        return(0);
    }
    
    if(!InitOpenGL(Instance, Window))
    {
        return(0);
    }
    
    if(CommandLine && *CommandLine != '\0')
    {
        // TODO(Zyonji): Handle generic command line parameters.
        DisplayImageFromFile(CommandLine, Window);
    }
    
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
    
    return(0);
}