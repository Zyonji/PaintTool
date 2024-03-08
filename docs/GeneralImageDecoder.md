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

# General DCT Decoder
The [Discrete Cosine Transform](https://en.wikipedia.org/wiki/Discrete_cosine_transform) is a way to represent raster images, used by JPEG and WebP. JPEG uses 8 by 8 blocks of wave magnitudes to represent image data (defined in [A.3.3](https://www.w3.org/Graphics/JPEG/itu-t81.pdf)). OpenGL doesn't support a native function to read images represented in that format, however, the independent blocks make conversion through a shader easy. If the wave magnitudes are stored as floating point numbers, then they can be loaded as a floating point texture that's not clamped to the 0 to 1 scope.
```cpp
glGenTextures(1, &Texture);
glBindTexture(GL_TEXTURE_2D, Texture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, DCTWidth, DCTHeight, 0, GL_RGBA, GL_FLOAT, Data);
```
Each magnitude is multiplied by the sum of the 16 products cosine waves in either direction within the block. The cosine wave for one dimension in one block coordinate is different, depending on if the coordinate within the block in that dimension is 0 or not. Each wave is multiplied by `sqrt(1/8)` at 0 and `sqrt(1/4)` otherwise. The cosine curve is defined as `cos((2x + 1)uπ/16)`. In the fragment shader, the `gl_FragCoord` is the center of a pixel, meaning a whole number plus `0.5`. Because of that, pixel 0 is `x = 0.5` instead. The `0.5 * 2` is equal to the `+ 1` in the definition. That gives us the simplified curve `cos(FragCoord * uπ/8)`. Written out as a shader, it looks like this:
```glsl
#define M_PI 3.1415926535897932384626433832795

uniform sampler2D Image;

out vec4 FragmentColor;

void main(void)
{
    ivec2 Target = ivec2(gl_FragCoord.xy) / 8 * 8;
    
    vec4 Color = vec4(0.0);
    vec4 OffsetX = mod(vec4(gl_FragCoord.x), 8.0);
    vec4 OffsetY = mod(vec4(gl_FragCoord.y), 8.0);
    vec4 ScalarX, ScalarY;
    
    for(int y = 0; y < 8; y++)
    {
        for(int x = 0; x < 8; x++)
        {
            vec4 Texel;
            Texel = texelFetch(Image, Target + ivec2(x, y), 0);
            ScalarX = sqrt((clamp(x, 0.0, 1.0) + 1.0) / 8.0) * cos(x * M_PI * OffsetX / 8.0);
            ScalarY = sqrt((clamp(y, 0.0, 1.0) + 1.0) / 8.0) * cos(y * M_PI * OffsetY / 8.0);
            Color += ScalarX * ScalarY * Texel;
        }
    }
    
    FragmentColor = Color;
}
```
Converting the coordinates to an integer vector and dividing and multiplying it by 8 aligns the coordinate to the block corner. 

# Color Channel Mapping
A JPEG file may have the different color channels at different sizes. In that case, a shader can simply sample the desired color by dividing the coordinate by the size difference between the channels. However, to avoid aliasing, the adjusted channel color should be interpolated with adjacent pixel colors depending on the pixel center position in the scaled up image. This can be achieved by using the fractional part of the virtual pixel position in a mixing function call. That means, for every channel, four color values need to be interpolated:
```glsl
uniform sampler2D Image;
uniform ivec4 SampleX;
uniform ivec4 SampleY;

out vec4 FragmentColor;

void main(void)
{
    vec2 PixelCenter = gl_FragCoord.xy;
    
    vec4 OffsetX = vec4(PixelCenter.x) / SampleX - vec4(0.5);
    vec4 OffsetY = vec4(PixelCenter.y) / SampleY - vec4(0.5);
    
    vec2 TargetR = vec2(OffsetX.r, OffsetY.r);
    vec2 TargetG = vec2(OffsetX.g, OffsetY.g);
    vec2 TargetB = vec2(OffsetX.b, OffsetY.b);
    vec2 TargetA = vec2(OffsetX.a, OffsetY.a);
    
    OffsetX = fract(OffsetX);
    OffsetY = fract(OffsetY);
    
    vec4 Texel00;
    Texel00.r = texelFetch(Image, ivec2(TargetR), 0).r;
    Texel00.g = texelFetch(Image, ivec2(TargetG), 0).g;
    Texel00.b = texelFetch(Image, ivec2(TargetB), 0).b;
    Texel00.a = texelFetch(Image, ivec2(TargetA), 0).a;
    
    vec4 Texel01;
    Texel01.r = texelFetch(Image, ivec2(TargetR + vec2(0, 1)), 0).r;
    Texel01.g = texelFetch(Image, ivec2(TargetG + vec2(0, 1)), 0).g;
    Texel01.b = texelFetch(Image, ivec2(TargetB + vec2(0, 1)), 0).b;
    Texel01.a = texelFetch(Image, ivec2(TargetA + vec2(0, 1)), 0).a;
    
    vec4 Texel10;
    Texel10.r = texelFetch(Image, ivec2(TargetR + vec2(1, 0)), 0).r;
    Texel10.g = texelFetch(Image, ivec2(TargetG + vec2(1, 0)), 0).g;
    Texel10.b = texelFetch(Image, ivec2(TargetB + vec2(1, 0)), 0).b;
    Texel10.a = texelFetch(Image, ivec2(TargetA + vec2(1, 0)), 0).a;
    
    vec4 Texel11;
    Texel11.r = texelFetch(Image, ivec2(TargetR + vec2(1, 1)), 0).r;
    Texel11.g = texelFetch(Image, ivec2(TargetG + vec2(1, 1)), 0).g;
    Texel11.b = texelFetch(Image, ivec2(TargetB + vec2(1, 1)), 0).b;
    Texel11.a = texelFetch(Image, ivec2(TargetA + vec2(1, 1)), 0).a;
    
    vec4 Color = mix(mix(Texel00, Texel01, OffsetY), mix(Texel10, Texel11, OffsetY), OffsetX);
    
    if(SampleX.r * SampleY.r == 0)
        Color.r = 0.0;
    if(SampleX.g * SampleY.g == 0)
        Color.g = 0.0;
    if(SampleX.b * SampleY.b == 0)
        Color.b = 0.0;
    if(SampleX.a * SampleY.a == 0)
        Color.a = 1.0;
    
    FragmentColor = Color;
}
```
Because it is possible that the coordinates might be divided by 0, leading to undefined outcomes, we set the channels to default values if division by 0 happened.

# Known Color Space Conversion
If the image read is in a specific color space, then we can handle color conversion with less effort than if the color space is described with parameters. For example, if the image is using sRGB, then no conversion is required. Here it's important to note that sRGB is not always used the same. In OpenGL it's often used to describe linear RGB, where channel values are relative to luminance. When talking about color spaces, sRGB specifies a standardized color gamut.

## YCbCr
A JPEG file is most commonly containing data in the [YCbCr Color Space](https://en.wikipedia.org/wiki/YCbCr). To convert from this color space to RGB, we multiply the color vector with the inverse color matrix mentioned on the wiki page, with `KR = 0.299`, `KG =  0.587` and `KB = 0.114`. This gives us the following simple conversion shader:

```glsl
uniform sampler2D Image;

out vec4 FragmentColor;

void main(void)
{
    vec4 Texel = texelFetch(Image, ivec2(gl_FragCoord.xy), 0);
    
    vec3 Color;
    Color.r = Texel.z * (2.0 - 2.0 * 0.299) + Texel.x;
    Color.b = Texel.y * (2.0 - 2.0 * 0.114) + Texel.x;
    Color.g = (Texel.x - 0.114 * Color.b - 0.299 * Color.r) / 0.587;
    Color = (Color + 128.0) / 255.0;
    
    FragmentColor = vec4(Color, Texel.a);
}
```
