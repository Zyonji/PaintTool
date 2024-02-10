#define BMP_SIGNATURE TWOCC("BM")

#define BMP_COMPRESSION_RGB            0
#define BMP_COMPRESSION_RLE8           1
#define BMP_COMPRESSION_RLE4           2
#define BMP_COMPRESSION_BITFIELDS      3
#define BMP_COMPRESSION_JPEG           4
#define BMP_COMPRESSION_PNG            5
#define BMP_COMPRESSION_ALPHABITFIELDS 6

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
    u16 BitsPerPixe;
};

struct BMP_Os2BitmapHeader
{
    u32 Size;
    u32 Width;
    u32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 ImageDataSize;
    u32 XResolution;
    u32 YResolution;
    u32 ColorsUsed;
    u32 ColorsImportant;
    u16 Units;
    u16 Reserved;
    u16 Recording;
    u16 Rendering;
    u32 Size1;
    u32 Size2;
    u32 ColorEncoding;
    u32 Identifier;
};

struct BMP_Win32BitmapHeader
{
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 ImageDataSize;
    s32 PixelsPerMeterX;
    s32 PixelsPerMeterY;
    u32 ColorsUsed;
    u32 ColorsImportant;// Last in Info Header
    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;       // Last in V2 Header
    u32 AlphaMask;      // Last in V3 Header
    u32 ColorSpaceType;
    s32 RedX;
    s32 RedY;
    s32 RedZ;
    s32 GreenX;
    s32 GreenY;
    s32 GreenZ;
    s32 BlueX;
    s32 BlueY;
    s32 BlueZ;
    u32 GammaRed;
    u32 GammaGreen;
    u32 GammaBlue;      // Last in V4 Header
    u32 Intent;
    u32 ProfileData;
    u32 ProfileSize;
    u32 Reserved;       // Last in V5 Header
};

#pragma pack(pop)