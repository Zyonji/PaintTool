#define PAINT_TOOL_WINDOW_CLASS_NAME "Zyonji's PaintTool Window"
#define PAINT_TOOL_WINDOW_NAME       "Zyonji's PaintTool"

struct win_global
{
    open_gl OpenGL;
    b32 Initialized;
    HGLRC RenderingContext;
};