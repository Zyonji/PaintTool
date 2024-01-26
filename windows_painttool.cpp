#include <windows.h>

void
LogError(char *Text, char *Caption)
{
    MessageBox(NULL, Text, Caption, MB_OK | MB_ICONHAND);
}

#define PAINT_TOOL_WINDOW_CLASS_NAME "Zyonji's PaintTool Window"
#define PAINT_TOOL_WINDOW_NAME       "Zyonji's PaintTool"

static void
ReadImageFromFile(char *FilePath, HWND Window)
{
    LogError(FilePath, "file to read");
}

LRESULT CALLBACK
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch(Message)
    {
        case WM_DROPFILES:
        {
            HDROP DropHandle = (HDROP)WParam;
            char FilePath[MAX_PATH];
            DragQueryFileA(DropHandle, 0, FilePath, MAX_PATH);
            DragFinish(DropHandle);
            
            ReadImageFromFile(FilePath, Window);
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
        PAINT_TOOL_WINDOW_CLASS_NAME,
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
    
    if(CommandLine && *CommandLine != '\0')
    {
        ReadImageFromFile(CommandLine, Window);
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