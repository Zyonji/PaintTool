#include "openGL_render.h"
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

static GLuint
CompileYCbCrToRGBProgram(open_gl *OpenGL, render_program_base *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, YCbCrToRGB_VertexCode, YCbCrToRGB_FragmentCode, Result);
    
    return(Program);
}

static GLuint
CompileFlipTextureProgram(open_gl *OpenGL, flip_texture_program *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, FlipTexture_VertexCode, FlipTexture_FragmentCode, &Result->Common);
    
    Result->OffsetID = glGetUniformLocation(Program, "Offset");
    Result->StrideID = glGetUniformLocation(Program, "Stride");
    
    return(Program);
}

static GLuint
CompileDCTProgram(open_gl *OpenGL, DCT_program *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, DCT_VertexCode, DCT_FragmentCode, &Result->Common);
    
    Result->ChannelCountID = glGetUniformLocation(Program, "ChannelCount");
    
    return(Program);
}

static GLuint
CompileStretchChannelProgram(open_gl *OpenGL, stretch_channel_program *Result)
{
    GLuint Program = OpenGLCreateProgram(GlobalBasicShaderHeaderCode, StretchChannel_VertexCode, StretchChannel_FragmentCode, &Result->Common);
    
    Result->SampleXID = glGetUniformLocation(Program, "SampleX");
    Result->SampleYID = glGetUniformLocation(Program, "SampleY");
    
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
    
    if(!CompileYCbCrToRGBProgram(OpenGL, &OpenGL->YCbCrToRGBProgram))
    {
        LogError("Unable to compile the YCbCr to RGB converting program.", "OpenGL");
        return(false);
    }
    
    if(!CompileFlipTextureProgram(OpenGL, &OpenGL->FlipTextureProgram))
    {
        LogError("Unable to compile the paint texture program.", "OpenGL");
        return(false);
    }
    
    if(!CompileDCTProgram(OpenGL, &OpenGL->DCTProgram))
    {
        LogError("Unable to compile the DCT covnersion program.", "OpenGL");
        return(false);
    }
    
    if(!CompileStretchChannelProgram(OpenGL, &OpenGL->StretchChannelProgram))
    {
        LogError("Unable to compile the stretch channel program.", "OpenGL");
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

static void
OpenGLProgramBegin(flip_texture_program *Program, s32 OffsetX, s32 OffsetY, s32 StrideX, s32 StrideY)
{
    OpenGLProgramBegin(&Program->Common);
    
    glUniform2i(Program->OffsetID, OffsetX, OffsetY);
    glUniform2i(Program->StrideID, StrideX, StrideY);
}
static void
OpenGLProgramEnd(flip_texture_program *Program)
{
    OpenGLProgramEnd(&Program->Common);
}

static void
OpenGLProgramBegin(DCT_program *Program, u8 ChannelCount)
{
    OpenGLProgramBegin(&Program->Common);
    
    glUniform1i(Program->ChannelCountID, ChannelCount);
}
static void
OpenGLProgramEnd(DCT_program *Program)
{
    OpenGLProgramEnd(&Program->Common);
}

static void
OpenGLProgramBegin(stretch_channel_program *Program, u8 *Sample)
{
    OpenGLProgramBegin(&Program->Common);
    
    glUniform4i(Program->SampleXID, Sample[0], Sample[2], Sample[4], Sample[6]);
    glUniform4i(Program->SampleYID, Sample[1], Sample[3], Sample[5], Sample[7]);
}
static void
OpenGLProgramEnd(stretch_channel_program *Program)
{
    OpenGLProgramEnd(&Program->Common);
}

static GLuint
GenerateImageTexture(open_gl *OpenGL, GLenum Format, GLenum Type, u32 Width, u32 Height, void *Data)
{
    GLuint Result = 0;
    
    glGenTextures(1, &Result);
    glBindTexture(GL_TEXTURE_2D, Result);
    // TODO(Zyonji): GL_RGBA32F might be overkill. I currently use it to make the DCT transforms more convenient.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Width, Height, 0, Format, Type, Data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return(Result);
}

static frame_buffer
CreateFramebuffer(open_gl *OpenGL, GLenum DataFormat, GLenum DataType, u32 Width, u32 Height, void *Data)
{
    frame_buffer Buffer = {};
    
    Buffer.Size = {Width, Height};
    
    glGenFramebuffers(1, &Buffer.FramebufferHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, Buffer.FramebufferHandle);
    
    Buffer.ColorHandle = GenerateImageTexture(OpenGL, DataFormat, DataType, Width, Height, Data);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Buffer.ColorHandle, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return(Buffer);
}

static void
DeleteFramebuffer(frame_buffer Framebuffer)
{
    if(Framebuffer.FramebufferHandle)
    {
        glDeleteFramebuffers(1, &Framebuffer.FramebufferHandle);
    }
    if(Framebuffer.ColorHandle)
    {
        glDeleteTextures(1, &Framebuffer.ColorHandle);
    }
}

void
SetImageBuffer(open_gl *OpenGL, void *Data, image_processor_tasks Processor)
{
    DeleteFramebuffer(OpenGL->ImageBuffer);
    DeleteFramebuffer(OpenGL->SwapBuffer);
    
    frame_buffer Buffer;
    frame_buffer SwapBuffer;
    
    glViewport(0, 0, Processor.Width,   Processor.Height);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, Processor.BigEndian);
    glPixelStorei(GL_UNPACK_ALIGNMENT,   Processor.ByteAlignment);
    
    if(Processor.DCTWidth > 0 && Processor.DCTHeight > 0)
    {
        GLuint Texture = 0;
        
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                     Processor.DCTWidth, Processor.DCTHeight, 0, GL_RGBA, GL_FLOAT, Data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        
        Buffer = CreateFramebuffer(OpenGL, GL_RGBA, GL_UNSIGNED_BYTE,
                                   Processor.Width, Processor.Height, 0);
        
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Buffer.FramebufferHandle);
        glBindTexture(GL_TEXTURE_2D, Texture);
        
        OpenGLProgramBegin(&OpenGL->DCTProgram, Processor.DCTChannelCount);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->DCTProgram);
        
        glDeleteTextures(1, &Texture);
        
        glBindTexture(GL_TEXTURE_2D, Buffer.ColorHandle);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        // TODO(Zyonji): If subsampling was used, interpolate the stretched channels.
    }
    else
    {
        GLenum DataFormat = 0;
        GLenum DataType = 0;
        
        if(Processor.BitsPerPixel >= 8 && Processor.TransparentColor == 0)
        {
            u32 BytesPerPixel = Processor.BitsPerPixel / 8;
            for(u8 i = BYTE_COUNT_MAP[BytesPerPixel - 1]; i < BYTE_COUNT_MAP[BytesPerPixel]; i++)
            {
                if(Processor.RedMask   == CHANNEL_MASK_MAP[i].RedMask   &&
                   Processor.GreenMask == CHANNEL_MASK_MAP[i].GreenMask &&
                   Processor.BlueMask  == CHANNEL_MASK_MAP[i].BlueMask  &&
                   Processor.AlphaMask == CHANNEL_MASK_MAP[i].AlphaMask)
                {
                    DataFormat = CHANNEL_MASK_MAP[i].DataFormat;
                    DataType = CHANNEL_MASK_MAP[i].DataType;
                    break;
                }
            }
        }
        
        if(DataType && Processor.PalletSize == 0)
        {
            Buffer = CreateFramebuffer(OpenGL, DataFormat, DataType,
                                       Processor.Width, Processor.Height, Data);
        }
        else
        {
            if(Processor.BigEndian)
            {
                glPixelStorei(GL_UNPACK_SWAP_BYTES, false);
                Processor.RedMask   = SwapEndian(Processor.RedMask);
                Processor.GreenMask = SwapEndian(Processor.GreenMask);
                Processor.BlueMask  = SwapEndian(Processor.BlueMask);
                Processor.AlphaMask = SwapEndian(Processor.AlphaMask);
            }
            
            u32 MaximumChannelSize = 0;
            channel_location RedLocation = GetChannelLocation(Processor.RedMask);
            if(RedLocation.BitCount > MaximumChannelSize)
            {
                MaximumChannelSize = RedLocation.BitCount;
            }
            channel_location GreenLocation = GetChannelLocation(Processor.GreenMask);
            if(GreenLocation.BitCount > MaximumChannelSize)
            {
                MaximumChannelSize = GreenLocation.BitCount;
            }
            channel_location BlueLocation = GetChannelLocation(Processor.BlueMask);
            if(BlueLocation.BitCount > MaximumChannelSize)
            {
                MaximumChannelSize = BlueLocation.BitCount;
            }
            channel_location AlphaLocation = GetChannelLocation(Processor.AlphaMask);
            if(AlphaLocation.BitCount > MaximumChannelSize)
            {
                MaximumChannelSize = AlphaLocation.BitCount;
            }
            
#if 0
            // TODO(Zyonji): Currently the textures are stored as 8 bit per channel, making higher precision superfluous. Reconsider after having settled on a texture storage format.
            if(MaximumChannelSize > 8)
                RearrangeChannelsToU64();
            else
                RearrangeChannelsToU32();
#endif
            
            // Expects the byte alignment to be a power of 2.
            u32 BitMask = Processor.ByteAlignment - 1;
            u32 BytesPerRow = ((Processor.BitsPerPixel * Processor.Width + 7) / 8 + BitMask) & (~BitMask);
            
            if(Processor.PalletSize == 0)
            {
                u64 DataSize = Processor.Width * Processor.Height * 4;
                void *NewData = RequestImageBuffer(DataSize);
                
                RearrangeChannelsToU32(Data, NewData,
                                       Processor.RedMask,   Processor.GreenMask,
                                       Processor.BlueMask,  Processor.AlphaMask, 
                                       RedLocation.Offset,  GreenLocation.Offset,
                                       BlueLocation.Offset, AlphaLocation.Offset,
                                       Processor.Width, Processor.Height, BytesPerRow,
                                       Processor.BitsPerPixel, Processor.BigEndian);
                
                if(Processor.TransparentColor)
                {
                    u32 TransparentColor;
                    RearrangeChannelsToU32(&Processor.TransparentColor, &TransparentColor,
                                           Processor.RedMask,   Processor.GreenMask,
                                           Processor.BlueMask,  Processor.AlphaMask, 
                                           RedLocation.Offset,  GreenLocation.Offset,
                                           BlueLocation.Offset, AlphaLocation.Offset,
                                           1, 1, 0,
                                           Processor.BitsPerPixel, Processor.BigEndian);
                    
                    void *LastPixel = (u8 *)NewData + DataSize;
                    for(u32 *Pixel = (u32 *)NewData; Pixel < LastPixel; Pixel++)
                    {
                        if(*Pixel == TransparentColor)
                        {
                            *Pixel &= 0xffffff;
                        }
                    }
                }
                
                Buffer = CreateFramebuffer(OpenGL, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                           Processor.Width, Processor.Height, NewData);
                
                FreeImageBuffer(NewData);
            }
            else
            {
                u64 ImageSize = Processor.Width * Processor.Height;
                u64 DataSize = ImageSize * 4 + Processor.PalletSize * 4;
                void *NewData = RequestImageBuffer(DataSize);
                u32 *NewPalletData = ((u32 *)NewData) + ImageSize;
                
                RearrangeChannelsToU32(Processor.PalletData, NewPalletData,
                                       Processor.RedMask,  Processor.GreenMask,
                                       Processor.BlueMask, Processor.AlphaMask, 
                                       RedLocation.Offset,  GreenLocation.Offset,
                                       BlueLocation.Offset, AlphaLocation.Offset,
                                       Processor.PalletSize, 1, 0, 
                                       Processor.BitsPerPalletColor, Processor.BigEndian);
                
                DereferenceColorIndex(Data, NewData, NewPalletData, Processor.PalletSize,
                                      Processor.Width, Processor.Height, BytesPerRow, Processor.BitsPerPixel);
                
                Buffer = CreateFramebuffer(OpenGL, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                           Processor.Width, Processor.Height, NewData);
                
                FreeImageBuffer(NewData);
            }
        }
    }
    
    SwapBuffer = CreateFramebuffer(OpenGL, GL_RGBA, GL_UNSIGNED_BYTE, Processor.Width, Processor.Height, 0);
    
    if(Processor.ChannelStretchFactors[0][0] > 0 && Processor.ChannelStretchFactors[0][1] > 0)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SwapBuffer.FramebufferHandle);
        glBindTexture(GL_TEXTURE_2D, Buffer.ColorHandle);
        
        OpenGLProgramBegin(&OpenGL->StretchChannelProgram, (u8 *)&Processor.ChannelStretchFactors);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->StretchChannelProgram);
        
        glBindTexture(GL_TEXTURE_2D, SwapBuffer.ColorHandle);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        frame_buffer TempBuffer = Buffer;
        Buffer = SwapBuffer;
        SwapBuffer = TempBuffer;
    }
    
    if(Processor.FlippedX || Processor.FlippedY)
    {
        s32 OffsetX = 0;
        s32 StrideX = 1;
        s32 OffsetY = 0;
        s32 StrideY = 1;
        
        if(Processor.FlippedX)
        {
            OffsetX = Processor.Width - 1;
            StrideX = -1;
        }
        if(Processor.FlippedY)
        {
            OffsetY = Processor.Height - 1;
            StrideY = -1;
        }
        
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SwapBuffer.FramebufferHandle);
        glBindTexture(GL_TEXTURE_2D, Buffer.ColorHandle);
        
        OpenGLProgramBegin(&OpenGL->FlipTextureProgram, OffsetX, OffsetY, StrideX, StrideY);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->FlipTextureProgram);
        
        glBindTexture(GL_TEXTURE_2D, SwapBuffer.ColorHandle);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        frame_buffer TempBuffer = Buffer;
        Buffer = SwapBuffer;
        SwapBuffer = TempBuffer;
    }
    
    if(Processor.ColorSpace == COLOR_SPACE_YCbCr)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SwapBuffer.FramebufferHandle);
        glBindTexture(GL_TEXTURE_2D, Buffer.ColorHandle);
        
        OpenGLProgramBegin(&OpenGL->YCbCrToRGBProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->YCbCrToRGBProgram);
        
        glBindTexture(GL_TEXTURE_2D, SwapBuffer.ColorHandle);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        frame_buffer TempBuffer = Buffer;
        Buffer = SwapBuffer;
        SwapBuffer = TempBuffer;
    }
    
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    OpenGL->ImageBuffer = Buffer;
}

void
DisplayBuffer(open_gl *OpenGL)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, OpenGL->Window.Width, OpenGL->Window.Height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if(OpenGL->ImageBuffer.ColorHandle)
    {
        r32 Ratio = (r32)OpenGL->ImageBuffer.Size.Width / (r32)OpenGL->ImageBuffer.Size.Height;
        if((r32)OpenGL->Window.Width / (r32)OpenGL->Window.Height > Ratio)
        {
            glViewport(0, 0, (u32)(OpenGL->Window.Height * Ratio), OpenGL->Window.Height);
        }
        else
        {
            glViewport(0, 0, OpenGL->Window.Width, (u32)(OpenGL->Window.Width / Ratio));
        }
        
        glBindTexture(GL_TEXTURE_2D, OpenGL->ImageBuffer.ColorHandle);
        
        OpenGLProgramBegin(&OpenGL->PaintTextureProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->PaintTextureProgram);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {
        OpenGLProgramBegin(&OpenGL->PaintCheckerProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        OpenGLProgramEnd(&OpenGL->PaintCheckerProgram);
    }
}