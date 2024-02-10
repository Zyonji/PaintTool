b32 DisplayImageFromData(void*, void*);

#include "bmp.cpp"

b32
DisplayImageFromData(void *FileMemory, void *FileEndpoint)
{
    return(BMP_DataDecoder(FileMemory, FileEndpoint));
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
Dereference1BitColorIndex(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                          u32 Width, u32 Height, u32 BytesPerRow)
{
    u32 *To = (u32 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From = Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining)
        {
            u8 Index[8];
            Index[0] = (*From) >> 7;
            Index[1] = ((*From) >> 6) & 1;
            Index[2] = ((*From) >> 5) & 1;
            Index[3] = ((*From) >> 4) & 1;
            Index[4] = ((*From) >> 3) & 1;
            Index[5] = ((*From) >> 2) & 1;
            Index[6] = ((*From) >> 1) & 1;
            Index[7] = (*From) & 1;
            
            From++;
            u8 *CurrentIndex = Index;
            u8 PixelsInByte = 8;
            while(PixelsRemaining && PixelsInByte--)
            {
                PixelsRemaining--;
                *(To++) = PalletData[*(CurrentIndex++)];
                
            }
        }
        Row += BytesPerRow;
    }
}

void
Dereference2BitColorIndex(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                          u32 Width, u32 Height, u32 BytesPerRow)
{
    u32 *To = (u32 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From = Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining)
        {
            u8 Index[4];
            Index[0] = (*From) >> 6;
            Index[1] = ((*From) >> 4) & 3;
            Index[2] = ((*From) >> 2) & 3;
            Index[3] = (*From) & 3;
            
            From++;
            u8 *CurrentIndex = Index;
            u8 PixelsInByte = 4;
            while(PixelsRemaining && PixelsInByte--)
            {
                PixelsRemaining--;
                *(To++) = PalletData[*(CurrentIndex++)];
                
            }
        }
        Row += BytesPerRow;
    }
}

void
Dereference4BitColorIndex(void *Source, void *Target, u32 *PalletData, u32 PalletSize,
                          u32 Width, u32 Height, u32 BytesPerRow)
{
    u32 *To = (u32 *)Target;
    u8 *Row = (u8 *)Source;
    u32 LinesRemaining = Height;
    while(LinesRemaining--)
    {
        u8 *From = Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining)
        {
            u8 Index[2];
            Index[0] = (*From) >> 4;
            Index[1] = (*From) & 0xf;
            
            From++;
            u8 *CurrentIndex = Index;
            u8 PixelsInByte = 2;
            while(PixelsRemaining && PixelsInByte--)
            {
                PixelsRemaining--;
                *(To++) = PalletData[*(CurrentIndex++)];
                
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
            *(To++) = PalletData[*(From++)];
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
    else if(BitsPerPixel == 4)
    {
        Dereference4BitColorIndex(Source, Target, PalletData, PalletSize,
                                  Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 2)
    {
        Dereference2BitColorIndex(Source, Target, PalletData, PalletSize,
                                  Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 1)
    {
        Dereference1BitColorIndex(Source, Target, PalletData, PalletSize,
                                  Width, Height, BytesPerRow);
    }
    else
    {
        // TODO(Zyonji): Implement indexed color decoders that aren't byte aligned.
        LogError("The indexed image data uses an unsupported index size.", "Image Decoder");
    }
}

void
RearrangeChannelsU8ToU32(void *Source, void *Target,
                         u8   RedMask, u8   GreenMask, u8   BlueMask, u8   AlphaMask, 
                         u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                         u32 Width, u32 Height, u32 BytesPerRow)
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
            u8 Red   = (*From &   RedMask) >>   RedOffset;
            u8 Green = (*From & GreenMask) >> GreenOffset;
            u8 Blue  = (*From &  BlueMask) >>  BlueOffset;
            u8 Alpha = (*From & AlphaMask) >> AlphaOffset;
            
            From++;
            *(To++) = (u8)((r32)Red   *   RedFactor);
            *(To++) = (u8)((r32)Green * GreenFactor);
            *(To++) = (u8)((r32)Blue  *  BlueFactor);
            *(To++) = (u8)((r32)Alpha * AlphaFactor) + AlphaFill;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsU16ToU32(void *Source, void *Target,
                          u16  RedMask, u16  GreenMask, u16  BlueMask, u16  AlphaMask, 
                          u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                          u32 Width, u32 Height, u32 BytesPerRow)
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
        u16 *From = (u16 *)Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining--)
        {
            u16 Red   = (*From &   RedMask) >>   RedOffset;
            u16 Green = (*From & GreenMask) >> GreenOffset;
            u16 Blue  = (*From &  BlueMask) >>  BlueOffset;
            u16 Alpha = (*From & AlphaMask) >> AlphaOffset;
            
            From++;
            *(To++) = (u8)((r32)Red   *   RedFactor);
            *(To++) = (u8)((r32)Green * GreenFactor);
            *(To++) = (u8)((r32)Blue  *  BlueFactor);
            *(To++) = (u8)((r32)Alpha * AlphaFactor) + AlphaFill;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsU32ToU32(void *Source, void *Target,
                          u32  RedMask, u32  GreenMask, u32  BlueMask, u32  AlphaMask, 
                          u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                          u32 Width, u32 Height, u32 BytesPerRow)
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
        u32 *From = (u32 *)Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining--)
        {
            u32 Red   = (*From &   RedMask) >>   RedOffset;
            u32 Green = (*From & GreenMask) >> GreenOffset;
            u32 Blue  = (*From &  BlueMask) >>  BlueOffset;
            u32 Alpha = (*From & AlphaMask) >> AlphaOffset;
            
            From++;
            *(To++) = (u8)((r32)Red   *   RedFactor);
            *(To++) = (u8)((r32)Green * GreenFactor);
            *(To++) = (u8)((r32)Blue  *  BlueFactor);
            *(To++) = (u8)((r32)Alpha * AlphaFactor) + AlphaFill;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsU64ToU32(void *Source, void *Target,
                          u64  RedMask, u64  GreenMask, u64  BlueMask, u64  AlphaMask, 
                          u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                          u32 Width, u32 Height, u32 BytesPerRow)
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
        u64 *From = (u64 *)Row;
        u32 PixelsRemaining = Width;
        while(PixelsRemaining--)
        {
            u64 Red   = (*From &   RedMask) >>   RedOffset;
            u64 Green = (*From & GreenMask) >> GreenOffset;
            u64 Blue  = (*From &  BlueMask) >>  BlueOffset;
            u64 Alpha = (*From & AlphaMask) >> AlphaOffset;
            
            From++;
            *(To++) = (u8)((r32)Red   *   RedFactor);
            *(To++) = (u8)((r32)Green * GreenFactor);
            *(To++) = (u8)((r32)Blue  *  BlueFactor);
            *(To++) = (u8)((r32)Alpha * AlphaFactor) + AlphaFill;
        }
        Row += BytesPerRow;
    }
}

void
RearrangeChannelsToU32(void *Source, void *Target,
                       u64  RedMask, u64  GreenMask, u64  BlueMask, u64  AlphaMask, 
                       u8 RedOffset, u8 GreenOffset, u8 BlueOffset, u8 AlphaOffset,
                       u32 Width, u32 Height, u32 BytesPerRow, u32 BitsPerPixel)
{
    if(BitsPerPixel == 8)
    {
        RearrangeChannelsU8ToU32(Source, Target,
                                 (u8)RedMask, (u8)GreenMask, (u8)BlueMask, (u8)AlphaMask, 
                                 RedOffset,   GreenOffset,   BlueOffset,   AlphaOffset,
                                 Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 16)
    {
        RearrangeChannelsU16ToU32(Source, Target,
                                  (u16)RedMask, (u16)GreenMask, (u16)BlueMask, (u16)AlphaMask, 
                                  RedOffset,    GreenOffset,    BlueOffset,    AlphaOffset,
                                  Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 32)
    {
        RearrangeChannelsU32ToU32(Source, Target,
                                  (u32)RedMask, (u32)GreenMask, (u32)BlueMask, (u32)AlphaMask, 
                                  RedOffset,    GreenOffset,    BlueOffset,    AlphaOffset,
                                  Width, Height, BytesPerRow);
    }
    else if(BitsPerPixel == 64)
    {
        RearrangeChannelsU64ToU32(Source, Target,
                                  RedMask,   GreenMask,   BlueMask,   AlphaMask, 
                                  RedOffset, GreenOffset, BlueOffset, AlphaOffset,
                                  Width, Height, BytesPerRow);
    }
    else
    {
        // TODO(Zyonji): If the Pixel doesn't align with a basic integer type, then we need special consideration for endian of the data. 
        LogError("The image uses an unsupported byte unaligned pixel format.", "Image Decoder");
    }
}