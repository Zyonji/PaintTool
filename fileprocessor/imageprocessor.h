#define COLOR_SPACE_UNKNOWN 0x00
#define COLOR_SPACE_sRGB    0x01
#define COLOR_SPACE_YCbCr   0x02
#define COLOR_SPACE_CMYK    0x03
#define COLOR_SPACE_YCCK    0x04

struct image_processor_tasks
{
    u32 Width;
    u32 Height;
    bool FlippedX;
    bool FlippedY;
    bool BigEndian;
    u32 BitsPerPixel;
    u32 ByteAlignment;
    void *PalletData;
    u32 PalletSize;
    u32 BitsPerPalletColor;
    u64 RedMask;
    u64 GreenMask;
    u64 BlueMask;
    u64 AlphaMask;
    u64 TransparentColor;
    u32 DCTWidth;
    u32 DCTHeight;
    u8  DCTChannelCount;
    u8  ChannelStretchFactors[4][2];
    u32 ColorSpace;
};

struct channel_location
{
    u8 BitCount;
    u8 Offset;
};