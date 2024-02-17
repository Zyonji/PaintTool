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
};

struct channel_location
{
    u8 BitCount;
    u8 Offset;
};