#include "GLSL\shader_globals.cpp"

static GLuint
OpenGLCreateProgram(char *HeaderCode, char *VertexCode, char *FragmentCode, render_program_base *Result)
{
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLchar *VertexShaderCode[] =
    {
        HeaderCode,
        VertexCode,
    };
    glShaderSource(VertexShaderID, ArrayCount(VertexShaderCode), VertexShaderCode, 0);
    glCompileShader(VertexShaderID);
    
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    GLchar *FragmentShaderCode[] =
    {
        HeaderCode,
        FragmentCode,
    };
    glShaderSource(FragmentShaderID, ArrayCount(FragmentShaderCode), FragmentShaderCode, 0);
    glCompileShader(FragmentShaderID);
    
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    
#if PAINTTOOL_CODE_VERIFICATION
    glValidateProgram(ProgramID);
    GLint Linked = false;
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Linked);
    if(!Linked)
    {
        GLsizei Ignored;
        char VertexErrors[4096];
        char FragmentErrors[4096];
        char ProgramErrors[4096];
        glGetShaderInfoLog(VertexShaderID, sizeof(VertexErrors), &Ignored, VertexErrors);
        glGetShaderInfoLog(FragmentShaderID, sizeof(FragmentErrors), &Ignored, FragmentErrors);
        glGetProgramInfoLog(ProgramID, sizeof(ProgramErrors), &Ignored, ProgramErrors);
        
        LogError(ProgramErrors, "OpenGL");
        LogError(VertexErrors, "OpenGL");
        LogError(FragmentErrors, "OpenGL");
        return(0);
    }
#endif
    
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
    
    Result->Handle = ProgramID;
    Result->PID = glGetAttribLocation(ProgramID, "VertP");
    Result->UVID = glGetAttribLocation(ProgramID, "VertUV");
    
    return(ProgramID);
}

static char *GlobalBasicShaderHeaderCode = R"glsl(
#version 330

)glsl";

static GLuint
CompilePaintCheckerProgram(open_gl *OpenGL, render_program_base *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, PaintChecker_VertexCode, PaintChecker_FragmentCode, Result);
    
    return(Program);
}

static GLuint
CompilePaintTextureProgram(open_gl *OpenGL, render_program_base *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, PaintTexture_VertexCode, PaintTexture_FragmentCode, Result);
    
    return(Program);
}

b32
OpenGLInitPrograms(open_gl *OpenGL)
{
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
    
    if(!CompilePaintCheckerProgram(OpenGL, &OpenGL->PaintCheckerProgram))
    {
        LogError("Unable to compile the paint checker program.", "OpenGL");
        return(false);
    }
    
    if(!CompilePaintTextureProgram(OpenGL, &OpenGL->PaintTextureProgram))
    {
        LogError("Unable to compile the paint texture program.", "OpenGL");
        return(false);
    }
    
    return(true);
}

static void
OpenGLProgramBegin(render_program_base *Program)
{
    glUseProgram(Program->Handle);
    
    GLuint PArray = Program->PID;
    GLuint UVArray = Program->UVID;
    
    if(PArray != OPEN_GL_INVALID_ATTRIBUTE)
    {
        glEnableVertexAttribArray(PArray);
        glVertexAttribPointer(PArray, 4, GL_FLOAT, false, sizeof(common_vertex), (void *)OffsetOf(common_vertex, P));
    }
    if(UVArray != OPEN_GL_INVALID_ATTRIBUTE)
    {
        glEnableVertexAttribArray(UVArray);
        glVertexAttribPointer(UVArray, 2, GL_FLOAT, false, sizeof(common_vertex), (void *)OffsetOf(common_vertex, UV));
    }
}

static void
OpenGLProgramEnd(render_program_base *Program)
{
    glUseProgram(0);
    
    GLuint PArray = Program->PID;
    GLuint UVArray = Program->UVID;
    
    if(PArray != OPEN_GL_INVALID_ATTRIBUTE)
    {
        glDisableVertexAttribArray(PArray);
    }
    if(UVArray != OPEN_GL_INVALID_ATTRIBUTE)
    {
        glDisableVertexAttribArray(UVArray);
    }
}

void
SetTexture(open_gl *OpenGL, u32 Width, u32 Height, void *Memory)
{
    if(OpenGL->ImageTextureHandle)
    {
        glDeleteTextures(1, &OpenGL->ImageTextureHandle);
    }
    
    OpenGL->ImageSize = {Width, Height};
    
    glGenTextures(1, &OpenGL->ImageTextureHandle);
    glBindTexture(GL_TEXTURE_2D, OpenGL->ImageTextureHandle);
    // TODO(Zyonji): Check if GL_RGBA16 or GL_RGBA32F are more desireable internal formats.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_BGRA, GL_UNSIGNED_BYTE, Memory);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void
DisplayBuffer(open_gl *OpenGL)
{
    glViewport(0, 0, OpenGL->Window.Width, OpenGL->Window.Height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if(OpenGL->ImageTextureHandle)
    {
        r32 Ratio = (r32)OpenGL->ImageSize.Width / (r32)OpenGL->ImageSize.Height;
        if((r32)OpenGL->Window.Width / (r32)OpenGL->Window.Height > Ratio)
        {
            glViewport(0, 0, (u32)(OpenGL->Window.Height * Ratio), OpenGL->Window.Height);
        }
        else
        {
            glViewport(0, 0, OpenGL->Window.Width, (u32)(OpenGL->Window.Width / Ratio));
        }
        
        OpenGLProgramBegin(&OpenGL->PaintTextureProgram);
        glBindTexture(GL_TEXTURE_2D, OpenGL->ImageTextureHandle);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        OpenGLProgramEnd(&OpenGL->PaintTextureProgram);
    }
    else
    {
        OpenGLProgramBegin(&OpenGL->PaintCheckerProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->PaintCheckerProgram);
    }
}