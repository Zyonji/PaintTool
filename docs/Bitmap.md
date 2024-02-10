# Identifying a Bitmap File
A [Bitmap](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmaps) of a [Bitmap File](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage) is headed by a [Bitmap File Header](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader). Instead of just relying on the documentation, I used [GIMP](https://www.gimp.org) to generate a bitmap file, containing an image, which makes identifying errors easy. I then hand it's file path to the command line argument in my debugger and cast the file memory pointer to a bitmap file header, to check if all the values are what I'd expect. Because our image decoders are meant to load external data, I find frequent sanity tests like that to be good practice. If `bfType`, `bfReserved1` and `bfReserved2` are the wrong value, then we assume that the file is no bitmap. 

# Bitmap Data Structure
A bitmap file always starts with a [Bitmap File Header](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader) followed by version specific [Bitmap Header](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types) and an unstructured data segment after that. The documentation by Microsoft notes 3 possible header versions, however, the [Wikipedia Article](https://en.wikipedia.org/wiki/BMP_file_format#DIB_header_(bitmap_information_header)) on the file format lists 7 known header versions. Those headers fall into 3 categories, core headers, OS/2 headers and Windows NT headers. Header versions in each category are all subsections of the same structure. That means, if the header size is `12`, then we cast the header pointer to a BITMAPCOREHEADER. If it's `16` and `64` instead, then we cast the header pointer to a OS22XBITMAPHEADER. Otherwise we cast it to a BITMAPV5HEADER. Then when process the header attributes from the top down and exit the process early, if the attribute lies outside of the header size.

## Header Structures
We'll use 4 different header structures. The file header, the core bitmap header, the largest Windows NT header and the OS/2 header. These structs need to be tightly packed in memory. To achieve this, I use `#pragma pack(push, 1)` and `#pragma pack(pop)`, which are compiler specific commands. This is compatible with VisualC++ and GCC. Other compilers may require different syntax. I'll also add the first 32 bits shared by all the bitmap header types to the file header structure for convenience. We need the size stored there to know what header type we're dealing with.

#### bmp.h
```cpp
#pragma pack(push, 1)
struct BMP_FileHeader
{
    u16 Signature;
    u32 Size;
    u32 Reserved;
    u32 BitmapOffset;
    u32 BitmapHeaderSize;
};
struct BMP_CoreBitmapHeader
{
    u32 Size;
    u16 Width;
    u16 Height;
    u16 Planes;
    u16 BitCount;
};
struct BMP_Os2BitmapHeader
{
    u32 Size;
    u32 Width;
    u32 Height;
    u16 Planes;
 [...]
    u32 ColorEncoding;
    u32 Identifier;
};
struct BMP_Win32BitmapHeader
{
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
 [...]
    u32 ProfileSize;
    u32 Reserved;
};
#pragma pack(pop)
```

## Obsolete Formats
Trying to decode OS/2 headers poses an unexpected challenge. This format defines 4 different compression methods, we would like to test in our decoder. The Wikipedia article gives us a link to one BMP using such a header, but doesn't name software which writes this format. Here I ask myself, if nobody uses it, is there a point in supporting it? I think it's more important to focus on core functionality before tackling all of the obscure use cases. For now I'll leave a comment marking this section as `TODO` for later and throw a specific error to notify me if it comes up in testing.
```cpp
// TODO(Zyonji): Implement a decoder for the OS/2 specific format.
LogError("The BMP file uses an unsupported OS/2 specific format.", "BMP Reader");
```

## Decoding Options of the BitmapV5Header
I want to split the decoding operations into two groups, CPU based and GPU based. Everything that calculates pixel values based on the decoded pixel values of previous pixels needs to happen __consecutively__ and is done best on the CPU. If an operation happens on every pixel without knowledge of the decoding results of other pixels, then it can be easily parallelized on the GPU using a __shader__.
- __Size:__ _No direct effect._
- __Width:__ Sets the width of the final texture containing the image.
- __Height:__ Sets the height of the final texture containing the image. If the height is negative, then the texture needs to flipped on the Y axis. The image can be flipped in a  __shader__.
- __Planes:__ _No direct effect._
- __BitsPerPixel:__ [`glTexImage2D`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml) only supports a `type` matching uncompressed `16` and `32` bits per pixel. `0` requires us to forward the data to another decoder. For `12` we can calculate a color mask in a __shader__. Otherwise we use load the color table as a texture and fetch pixel colors from it in a __shader__.
- __Compression:__ `RGB` is straight forward. `RLE8` and `RLE4` need to be decoded __consecutively__. `JPEG` and `PNG` requires us to forward the data to another decoder. For `BITFIELDS` and `ALPHABITFIELDS` we can use the color mask in a __shader__.
- __ImageDataSize:__ _No direct effect._
- __PixelsPerMeterX and Y:__ _No direct effect._
- __ColorsUsed:__ Sets the size of the color table we load into the __shader__.
- __ColorsImportant:__ _No direct effect._
- __Red, Green, Blue and AlphaMask:__ Sets the color mask we can use in a __shader__.
- __ColorSpaceType with XYZEndpoints, Gamma or Profile:__ This contains color space information, we can use for __color correction__. It's not required yet to display the image, but we will want to take it into account at a later point.
- __Intent:__ Can be used for __color correction__.
- __Reserved:__ _No direct effect._

## Decoding Process
This gives us the following decoding process:

First we look at the `Compression` parameter. For `JPEG` and `PNG` we leave a `TODO` and error log to link to those decoders later. If the value is `RLE8` or `RLE4` instead, then we need to first allocate memory large enough to contain the uncompressed image data. Then we decode the data contained in the file's bitmap data using the specified [`Algorithm`](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-compression) and store it in the new buffer.

All other steps can be performed in a __shader__, so instead of processing those options here, I package them as a set of instructions for a [General Image Decoder](/docs/GeneralImageDecoder.md). I'll leave __color correction__ for later. That leaves the following options to be set for the image decoder:
- a pointer to the image data
- height and width of the picture
- bits used per pixel
- the byte alignment of the row length
- the mask of each color channel and the alpha channel
- a pointer to and size of a color table, if present
- a flag denoting if the image is flipped