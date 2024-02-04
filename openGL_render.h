#define OPEN_GL_INVALID_ATTRIBUTE -1

struct common_vertex
{
    v4 P;
    v2 UV;
};

struct render_program_base
{
    GLuint Handle;
    
    GLuint PID;
    GLuint UVID;
};

struct frame_buffer
{
    GLuint FramebufferHandle;
    GLuint ColorHandle;
    v2u Size;
};

struct open_gl
{
    v2u Window;
    
    GLuint ImageTextureHandle;
    v2u ImageSize;
    
    render_program_base PaintCheckerProgram;
    render_program_base PaintTextureProgram;
};