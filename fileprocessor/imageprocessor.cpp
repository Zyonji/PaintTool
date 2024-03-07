b32 DisplayImageFromData(void*, void*);

#include "png.cpp"
#include "jpeg.cpp"
#include "bmp.cpp"

b32
DisplayImageFromData(void *FileMemory, void *FileEndpoint)
{
    if(PNG_Reader(FileMemory, FileEndpoint))
        return(true);
    
    if(JPEG_Reader(FileMemory, FileEndpoint))
        return(true);
    
    if(BMP_Reader(FileMemory, FileEndpoint))
        return(true);
    
    return(false);
}

// Channel masks are expected to be contiguous.
channel_location
GetChannelLocation(u64 ChannelMask)
{
    channel_location Location = {};
    if(ChannelMask)
    {
        while(!(ChannelMask & 1))
        {
            Location.Offset += 1;
            ChannelMask >>= 1;
        }
        while(ChannelMask & 1)
        {
            Location.BitCount += 1;
            ChannelMask >>= 1;
        }
    }
    return(Location);
}

// TODO(Zyonji): For future optimization, consider in memory operations and SIMD operations.

void
DereferenceColorIndexByte(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                          u32 Width, u32 Height, u32 BytesPerRow, u8 BitsPerPixel)
{
    u8 BitMask = (1 << BitsPerPixel) - 1;
    u32 *To = (u32 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From            = Row;
        u32 PixelsRemaining = Width;
        u8  CurrentByte     = 0;
        u8  Shifts          = 0;
        while(PixelsRemaining--)
        {
            if(Shifts == 0)
            {
                CurrentByte = *(From++);
                Shifts = 8;
            }
            
            Shifts -= BitsPerPixel;
            u8 Index = (CurrentByte >> Shifts) & BitMask;
            if(Index < PalletSize)
            {
                *(To++) = PalletData[Index];
            }
        }
        Row += BytesPerRow;
    }
}

void
Dereference8BitColorIndex(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                          u32 Width, u32 Height, u32 BytesPerRow)
{
    u32 *To = (u32 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From = Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining--)
        {
            if(*From < PalletSize)
            {
                *(To++) = PalletData[*From];
            }
            From++;
        }
        Row += BytesPerRow;
    }
}

void
DereferenceColorIndex(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                      u32 Width, u32 Height, u32 BytesPerRow, u32 BitsPerPixel)
{
    if(BitsPerPixel == 8)
    {
        Dereference8BitColorIndex(Source, Target, PalletData, PalletSize,
                                  Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 4 || BitsPerPixel == 2 || BitsPerPixel == 1)
    {
        DereferenceColorIndexByte(Source, Target, PalletData, PalletSize,
                                  Width, Height, BytesPerRow, (u8)BitsPerPixel);
    }
    else
    {
        // TODO(Zyonji): Implement indexed color decoders that aren't byte aligned.
        LogError("The indexed image data uses an unsupported index size.", "Image Decoder");
    }
}

void
RearrangeChannelsBitsToU32(void *Source, void *Target,
                           u8   RedMask, u8   GreenMask, u8   BlueMask, u8   AlphaMask, 
                           u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                           u32 Width, u32 Height, u32 BytesPerRow, u8 BitsPerPixel)
{
    r32   RedFactor =   (RedMask)?((r32)U8Max / (r32)(RedMask   >>   RedOffset)):0;
    r32 GreenFactor = (GreenMask)?((r32)U8Max / (r32)(GreenMask >> GreenOffset)):0;
    r32  BlueFactor =  (BlueMask)?((r32)U8Max / (r32)(BlueMask  >>  BlueOffset)):0;
    r32 AlphaFactor = (AlphaMask)?((r32)U8Max / (r32)(AlphaMask >> AlphaOffset)):0;
    u8 AlphaFill = (AlphaMask)?0:U8Max;
    
    u8 BitMask = (1 << BitsPerPixel) - 1;
    u8 *To  = (u8 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From            = Row;
        u32 PixelsRemaining = Width;
        u8  CurrentByte     = 0;
        u8  Shifts          = 0;
        while(PixelsRemaining--)
        {
            if(Shifts == 0)
            {
                CurrentByte = *(From++);
                Shifts = 8;
            }
            
            Shifts -= BitsPerPixel;
            u8 Pixel = (CurrentByte >> Shifts) & BitMask;
            
            u8 Red   = (Pixel &   RedMask) >>   RedOffset;
            u8 Green = (Pixel & GreenMask) >> GreenOffset;
            u8 Blue  = (Pixel &  BlueMask) >>  BlueOffset;
            u8 Alpha = (Pixel & AlphaMask) >> AlphaOffset;
            
            *(To++) = (u8)((r32)Red   *   RedFactor + 0.5);
            *(To++) = (u8)((r32)Green * GreenFactor + 0.5);
            *(To++) = (u8)((r32)Blue  *  BlueFactor + 0.5);
            *(To++) = (u8)((r32)Alpha * AlphaFactor + 0.5) + AlphaFill;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsBytesToU32(void *Source, void *Target,
                            u64  RedMask, u64  GreenMask, u64  BlueMask, u64  AlphaMask, 
                            u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                            u32 Width, u32 Height, u32 BytesPerRow, u8 BytesPerPixel)
{
    r32   RedFactor =   (RedMask)?((r32)U8Max / (r32)(RedMask   >>   RedOffset)):0;
    r32 GreenFactor = (GreenMask)?((r32)U8Max / (r32)(GreenMask >> GreenOffset)):0;
    r32  BlueFactor =  (BlueMask)?((r32)U8Max / (r32)(BlueMask  >>  BlueOffset)):0;
    r32 AlphaFactor = (AlphaMask)?((r32)U8Max / (r32)(AlphaMask >> AlphaOffset)):0;
    u8 AlphaFill = (AlphaMask)?0:U8Max;
    
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
            
            *(To++) = (u8)((r32)Red   *   RedFactor + 0.5);
            *(To++) = (u8)((r32)Green * GreenFactor + 0.5);
            *(To++) = (u8)((r32)Blue  *  BlueFactor + 0.5);
            *(To++) = (u8)((r32)Alpha * AlphaFactor + 0.5) + AlphaFill;
            From += BytesPerPixel;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsBigEndianBytesToU32(void *Source, void *Target,
                                     u64  RedMask, u64  GreenMask, u64  BlueMask, u64  AlphaMask, 
                                     u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                                     u32 Width, u32 Height, u32 BytesPerRow, u8 BytesPerPixel)
{
    r32   RedFactor =   (RedMask)?((r32)U8Max / (r32)(RedMask   >>   RedOffset)):0;
    r32 GreenFactor = (GreenMask)?((r32)U8Max / (r32)(GreenMask >> GreenOffset)):0;
    r32  BlueFactor =  (BlueMask)?((r32)U8Max / (r32)(BlueMask  >>  BlueOffset)):0;
    r32 AlphaFactor = (AlphaMask)?((r32)U8Max / (r32)(AlphaMask >> AlphaOffset)):0;
    u8 AlphaFill = (AlphaMask)?0:U8Max;
    
    u8 *To  = (u8 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From = Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining--)
        {
            u64 Pixel = SwapEndian(*(u64 *)From);
            u64 Red   = (Pixel &   RedMask) >>   RedOffset;
            u64 Green = (Pixel & GreenMask) >> GreenOffset;
            u64 Blue  = (Pixel &  BlueMask) >>  BlueOffset;
            u64 Alpha = (Pixel & AlphaMask) >> AlphaOffset;
            
            *(To++) = (u8)((r32)Red   *   RedFactor + 0.5);
            *(To++) = (u8)((r32)Green * GreenFactor + 0.5);
            *(To++) = (u8)((r32)Blue  *  BlueFactor + 0.5);
            *(To++) = (u8)((r32)Alpha * AlphaFactor + 0.5) + AlphaFill;
            From += BytesPerPixel;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsToU32(void *Source, void *Target,
                       u64  RedMask, u64  GreenMask, u64  BlueMask, u64  AlphaMask, 
                       u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                       u32 Width, u32 Height, u32 BytesPerRow,
                       u32 BitsPerPixel, bool BigEndian)
{
    if((BitsPerPixel & 7) == 0)
    {
        if(BigEndian)
        {
            RearrangeChannelsBigEndianBytesToU32(Source, Target,
                                                 RedMask,   GreenMask,   BlueMask,   AlphaMask, 
                                                 RedOffset, GreenOffset, BlueOffset, AlphaOffset,
                                                 Width, Height, BytesPerRow, (u8)(BitsPerPixel / 8));
        }
        else
        {
            RearrangeChannelsBytesToU32(Source, Target,
                                        RedMask,   GreenMask,   BlueMask,   AlphaMask, 
                                        RedOffset, GreenOffset, BlueOffset, AlphaOffset,
                                        Width, Height, BytesPerRow, (u8)(BitsPerPixel / 8));
        }
    }
    else if(BitsPerPixel == 4 || BitsPerPixel == 2 || BitsPerPixel == 1)
    {
        RearrangeChannelsBitsToU32(Source, Target,
                                   (u8)RedMask, (u8)GreenMask, (u8)BlueMask, (u8)AlphaMask, 
                                   RedOffset,   GreenOffset,   BlueOffset,   AlphaOffset,
                                   Width, Height, BytesPerRow, (u8)BitsPerPixel);
    }
    else
    {
        // TODO(Zyonji): Implement color channel decoders that aren't byte aligned.
        LogError("The image uses an unsupported byte unaligned pixel format.", "Image Decoder");
    }
}