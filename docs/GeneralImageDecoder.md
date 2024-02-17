# Texture Manipulation
By default, textures stay the same, once they're set. To write to a texture with a shader, we need to attach it to a framebuffer using [`glFramebufferTexture2D`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glFramebufferTexture.xhtml). If we're trying to write to the same texture we're also reading from, then we have to make special considerations for the memory updates, because it's not always easily predictable, when data is updated. It's easier to set up two framebuffers. One as the source and one as the target. And after each operation, we swap the handles for the two in memory.  

# Flipped Image
If the image data isn't stored as rows going from left to right, starting with the bottom row going up, OpenGL will load the image in the wrong orientation. To flip the texture, we can use the double framebuffer process described previously. The following fragment shader with `Offset = {0, ImageHeight}` and `Stride = {1, -1}` would be equivalent to flipping the image around a vertical axis:
```glsl
uniform ivec2 Offset;
uniform ivec2 Stride;
uniform sampler2D Image;

out vec4 FragmentColor;

void main(void)
{
    ivec2 Target = Stride * ivec2(gl_FragCoord.xy) + Offset;
    FragmentColor = texelFetch(Image, Target, 0);
}
```

# Color Table
There's no straight forward way I know of, to load indexed image data as a full color texture in OpenGL. Instead it's necessary to allocate memory dereference each index ourselves. Here we can either do that through the CPU with straight forward memory operations, or we load the color table as a 1D texture and the indexed image as a 2D sampler buffer to retrieve unnormalized values the shader through [`texelFetch`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/texelFetch.xhtml). Bitwise operations aren't as simple on the GPU as they are on the CPU. I find that it's a good idea to go with the easier approach until this function turns out to be a performance bottle neck.

# Color Channel Masks and Bit Count
Either the image data or the color table have pixel data formatted in a certain way. I chose to represent the formatting with bit masks for each color channel. For example the format `0xAABBGGRR`, with `R` being bits of the red channel, `G` for green, `B` for blue and `A` for alpha, would be represented as the masks `0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000` in `RGBA` order. OpenGL offers some options for loading various pixel data formats as textures. For this purpose I created an array to check against to find a fitting format:
```cpp
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
{0x0000ffff00000000, 0x00000000ffff0000, 0x000000000000ffff, 0xffff000000000000, GL_BGRA, GL_UNSIGNED_SHORT},
```
If none of those formats match, then we need to manually rearrange the channel data into a format OpenGL can read. I chose `0xAABBGGRR` as the target format for manual rearranging. We also assume that each channel bitmask is contiguous. It's unclear how a channel with gaps between used bits should be interpreted. The manual rearranging of the channels is also mostly straight forward, but takes longer than using a format known to OpenGL.
```cpp
u8 GetChannelLocation(u64 ChannelMask)
{
    u8 Offset = 0;
    if(ChannelMask)
    {
        while(!(ChannelMask & 1))
        {
            Offset += 1;
            ChannelMask >>= 1;
        }
    }
    return(Offset);
}
```
```cpp
u8   RedOffset = GetChannelLocation(  RedMask);
u8 GreenOffset = GetChannelLocation(GreenMask);
u8  BlueOffset = GetChannelLocation( BlueMask);
u8 AlphaOffset = GetChannelLocation(AlphaMask);

r32   RedFactor = (  RedMask)?((r32)U8Max / (r32)(  RedMask >>   RedOffset)):0;
r32 GreenFactor = (GreenMask)?((r32)U8Max / (r32)(GreenMask >> GreenOffset)):0;
r32  BlueFactor = ( BlueMask)?((r32)U8Max / (r32)( BlueMask >>  BlueOffset)):0;
r32 AlphaFactor = (AlphaMask)?((r32)U8Max / (r32)(AlphaMask >> AlphaOffset)):0;
u8  AlphaFill   = (AlphaMask)?0:U8Max;

u8 *To  = (u8 *)Target;
u8 *Row = (u8 *)Source;
u32 LinesRemaining = Height;
while(LinesRemaining--)
{
    u8 *From = Row;
    u32 PixelsRemaining = Width;
    while(PixelsRemaining--)
    {
        u64 Pixel = *(u64 *)From;
        u64 Red   = (Pixel &   RedMask) >>   RedOffset;
        u64 Green = (Pixel & GreenMask) >> GreenOffset;
        u64 Blue  = (Pixel &  BlueMask) >>  BlueOffset;
        u64 Alpha = (Pixel & AlphaMask) >> AlphaOffset;
        
        *(To++) = (u8)((r32)Red   *   RedFactor);
        *(To++) = (u8)((r32)Green * GreenFactor);
        *(To++) = (u8)((r32)Blue  *  BlueFactor);
        *(To++) = (u8)((r32)Alpha * AlphaFactor) + AlphaFill;
        From += BytesPerPixel;;
    }
    Row += BytesPerRow;
}
```
Make sure you set the alpha channel to the maximum value, if no alpha mask is present. Otherwise your texture will become invisible when it shouldn't.