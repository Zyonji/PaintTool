#define PAINT_TOOL_WINDOW_CLASS_NAME "Zyonji's PaintTool Window"
#define PAINT_TOOL_WINDOW_NAME       "Zyonji's PaintTool"
#define WGL_LOADER_WINDOW_CLASS_NAME "Zyonji's PaintTool WGL Loading Window"
#define WGL_LOADER_WINDOW_NAME       "Zyonji's PaintTool's WGL Loader"

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013

#define WGL_FULL_ACCELERATION_ARB               0x2027
#define WGL_TYPE_RGBA_ARB                       0x202B

#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126

#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001

#define GL_BGR                                  0x80E0
#define GL_BGRA                                 0x80E1
#define GL_CLAMP_TO_EDGE                        0x812F

#define GL_ARRAY_BUFFER                         0x8892
#define GL_STATIC_DRAW                          0x88E4

#define GL_FRAGMENT_SHADER                      0x8B30
#define GL_VERTEX_SHADER                        0x8B31
#define GL_LINK_STATUS                          0x8B82
#define GL_DRAW_FRAMEBUFFER                     0x8CA9
#define GL_COLOR_ATTACHMENT0                    0x8CE0
#define GL_FRAMEBUFFER                          0x8D40

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

typedef BOOL WINAPI type_wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef HGLRC WINAPI type_wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList);

typedef GLuint WINAPI type_glCreateShader(GLenum type);
typedef void WINAPI type_glShaderSource(GLuint shader, GLsizei count, GLchar **string, GLint *length);
typedef void WINAPI type_glCompileShader(GLuint shader);
typedef void WINAPI type_glAttachShader(GLuint program, GLuint shader);typedef void WINAPI type_glDeleteShader(GLuint shader);
typedef void WINAPI type_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

typedef GLuint WINAPI type_glCreateProgram(void);
typedef void WINAPI type_glLinkProgram(GLuint program);
typedef void WINAPI type_glValidateProgram(GLuint program);
typedef void WINAPI type_glUseProgram(GLuint program);
typedef void WINAPI type_glGetProgramiv(GLuint program, GLenum pname, GLint *params);
typedef void WINAPI type_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

typedef GLint WINAPI type_glGetAttribLocation(GLuint program, const GLchar *name);
typedef void WINAPI type_glEnableVertexAttribArray(GLuint index);
typedef void WINAPI type_glDisableVertexAttribArray(GLuint index);
typedef void WINAPI type_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLint WINAPI type_glGetUniformLocation(GLuint program, const GLchar *name);

typedef void WINAPI type_glGenBuffers(GLsizei n, GLuint *buffers);
typedef void WINAPI type_glBindBuffer(GLenum target, GLuint buffer);
typedef void WINAPI type_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);

typedef void WINAPI type_glGenFramebuffers(GLsizei n, GLuint *framebuffers);
typedef void WINAPI type_glBindFramebuffer(GLenum target, GLuint framebuffer);
typedef void WINAPI type_glDeleteFramebuffers(GLsizei n, GLuint *framebuffers);
typedef void WINAPI type_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

typedef void WINAPI type_glGenVertexArrays(GLsizei n, GLuint *arrays);
typedef void WINAPI type_glBindVertexArray(GLuint array);

typedef void WINAPI type_glGenerateMipmap(GLenum target);

#define OpenGLGlobalFunction(Name) static type_##Name *Name

OpenGLGlobalFunction(wglChoosePixelFormatARB);
OpenGLGlobalFunction(wglCreateContextAttribsARB);

OpenGLGlobalFunction(glCreateShader);
OpenGLGlobalFunction(glShaderSource);
OpenGLGlobalFunction(glCompileShader);
OpenGLGlobalFunction(glAttachShader);
OpenGLGlobalFunction(glDeleteShader);
OpenGLGlobalFunction(glGetShaderInfoLog);

OpenGLGlobalFunction(glCreateProgram);
OpenGLGlobalFunction(glLinkProgram);
OpenGLGlobalFunction(glValidateProgram);
OpenGLGlobalFunction(glUseProgram);
OpenGLGlobalFunction(glGetProgramiv);
OpenGLGlobalFunction(glGetProgramInfoLog);

OpenGLGlobalFunction(glGetAttribLocation);
OpenGLGlobalFunction(glEnableVertexAttribArray);
OpenGLGlobalFunction(glDisableVertexAttribArray);
OpenGLGlobalFunction(glVertexAttribPointer);
OpenGLGlobalFunction(glGetUniformLocation);

OpenGLGlobalFunction(glGenBuffers);
OpenGLGlobalFunction(glBindBuffer);
OpenGLGlobalFunction(glBufferData);

OpenGLGlobalFunction(glGenFramebuffers);
OpenGLGlobalFunction(glBindFramebuffer);
OpenGLGlobalFunction(glDeleteFramebuffers);
OpenGLGlobalFunction(glFramebufferTexture2D);

OpenGLGlobalFunction(glGenVertexArrays);
OpenGLGlobalFunction(glBindVertexArray);

OpenGLGlobalFunction(glGenerateMipmap);

#undef OpenGLGlobalFunction

static b32
LoadOpenGLFunctions()
{
#define WGLGetOpenGLFunction(Name) if((Name = (type_##Name *)wglGetProcAddress(#Name)) == 0) return false
    
    WGLGetOpenGLFunction(glCreateShader);
    WGLGetOpenGLFunction(glShaderSource);
    WGLGetOpenGLFunction(glCompileShader);
    WGLGetOpenGLFunction(glAttachShader);
    WGLGetOpenGLFunction(glDeleteShader);
    WGLGetOpenGLFunction(glGetShaderInfoLog);
    
    WGLGetOpenGLFunction(glCreateProgram);
    WGLGetOpenGLFunction(glLinkProgram);
    WGLGetOpenGLFunction(glValidateProgram);
    WGLGetOpenGLFunction(glUseProgram);
    WGLGetOpenGLFunction(glGetProgramiv);
    WGLGetOpenGLFunction(glGetProgramInfoLog);
    
    WGLGetOpenGLFunction(glGetAttribLocation);
    WGLGetOpenGLFunction(glEnableVertexAttribArray);
    WGLGetOpenGLFunction(glDisableVertexAttribArray);
    WGLGetOpenGLFunction(glVertexAttribPointer);
    WGLGetOpenGLFunction(glGetUniformLocation);
    
    WGLGetOpenGLFunction(glGenBuffers);
    WGLGetOpenGLFunction(glBindBuffer);
    WGLGetOpenGLFunction(glBufferData);
    
    WGLGetOpenGLFunction(glGenFramebuffers);
    WGLGetOpenGLFunction(glBindFramebuffer);
    WGLGetOpenGLFunction(glDeleteFramebuffers);
    WGLGetOpenGLFunction(glFramebufferTexture2D);
    
    WGLGetOpenGLFunction(glGenVertexArrays);
    WGLGetOpenGLFunction(glBindVertexArray);
    
    WGLGetOpenGLFunction(glGenerateMipmap);
    
#undef WGLGetOpenGLFunction
    return true;
}

struct win_open_gl : open_gl
{
    b32 Initialized;
    HGLRC RenderingContext;
};