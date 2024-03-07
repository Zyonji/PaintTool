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
struct flip_texture_program
{
    render_program_base Common;
    
    GLuint OffsetID;
    GLuint StrideID;
};
struct DCT_program
{
    render_program_base Common;
    
    GLuint ChannelCountID;
};
struct stretch_channel_program
{
    render_program_base Common;
    
    GLuint SampleXID;
    GLuint SampleYID;
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
    
    frame_buffer ImageBuffer;
    frame_buffer SwapBuffer;
    
    render_program_base     PaintCheckerProgram;
    render_program_base     PaintTextureProgram;
    render_program_base     YCbCrToRGBProgram;
    flip_texture_program    FlipTextureProgram;
    DCT_program             DCTProgram;
    stretch_channel_program StretchChannelProgram;
};

struct channel_mask_map
{
    u64 RedMask;
    u64 GreenMask;
    u64 BlueMask;
    u64 AlphaMask;
    GLenum DataFormat;
    GLenum DataType;
};

static const u8 BYTE_COUNT_MAP[9] = {0, 3, 11, 13, 23, 23, 25, 25, 27};
static channel_mask_map CHANNEL_MASK_MAP[27] = {
    // 1 byte
    {0x07, 0x38, 0xc0, 0x00, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV},
    {0xc0, 0x38, 0x07, 0x00, GL_RGB, GL_UNSIGNED_BYTE_3_3_2},
    {0xff, 0x00, 0x00, 0x00, GL_RED, GL_UNSIGNED_BYTE},
    // 2 bytes
    {0x000f, 0x00f0, 0x0f00, 0xf000, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV},
    {0xf000, 0x0f00, 0x00f0, 0x000f, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},
    {0x001f, 0x03e0, 0x7c00, 0x8000, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    {0xf800, 0x07c0, 0x003e, 0x0001, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},
    {0x001f, 0x07e0, 0xf800, 0x0000, GL_RGB,  GL_UNSIGNED_SHORT_5_6_5_REV},
    {0xf800, 0x07e0, 0x001f, 0x0000, GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},
    {0x00ff, 0xff00, 0x0000, 0x0000, GL_RG,   GL_UNSIGNED_BYTE},
    {0xffff, 0x0000, 0x0000, 0x0000, GL_RED,  GL_UNSIGNED_SHORT},
    // 3 bytes
    {0x0000ff, 0x00ff00, 0xff0000, 0x000000, GL_RGB, GL_UNSIGNED_BYTE},
    {0xff0000, 0x00ff00, 0x0000ff, 0x000000, GL_BGR, GL_UNSIGNED_BYTE},
    // 4 bytes
    {0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},
    {0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
    {0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},
    {0x0000ff00, 0x00ff0000, 0xff000000, 0x000000ff, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8},
    {0x00000ffc, 0x003ff000, 0xffc00000, 0x00000003, GL_BGRA, GL_UNSIGNED_INT_10_10_10_2},
    {0xffc00000, 0x003ff000, 0x00000ffc, 0x00000003, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2},
    {0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
    {0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV},
    {0x0000ffff, 0xffff0000, 0x00000000, 0x00000000, GL_RG,   GL_UNSIGNED_SHORT},
    {0xffffffff, 0x00000000, 0x00000000, 0x00000000, GL_RED,  GL_UNSIGNED_INT},
    // 5 bytes
    // 6 bytes
    {0x00000000ffff, 0x0000ffff0000, 0xffff00000000, 0x000000000000, GL_RGB, GL_UNSIGNED_SHORT},
    {0xffff00000000, 0x0000ffff0000, 0x00000000ffff, 0x000000000000, GL_BGR, GL_UNSIGNED_SHORT},
    // 7 bytes
    // 8 bytes
    {0x000000000000ffff, 0x00000000ffff0000, 0x0000ffff00000000, 0xffff000000000000, GL_RGBA, GL_UNSIGNED_SHORT},
    {0x0000ffff00000000, 0x00000000ffff0000, 0x000000000000ffff, 0xffff000000000000, GL_BGRA, GL_UNSIGNED_SHORT}
};