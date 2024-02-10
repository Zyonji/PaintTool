struct image_processor_tasks
{
    u32 Width;
    u32 Height;
    bool FlippedX;
    bool FlippedY;
    u32 BitsPerPixel;
    u32 ByteAlignment;
    u32 *PalletData;
    u32 PalletSize;
    u64 RedMask;
    u64 GreenMask;
    u64 BlueMask;
    u64 AlphaMask;
};

struct channel_location
{
    u8 BitCount;
    u8 Offset;
};