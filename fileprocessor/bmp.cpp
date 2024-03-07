#include "bmp.h"

void
RLE8(void *SourceData, void *DataEndpoint, void *PalletData, u32 PalletSize,
     void *TargetData, void *TargetEndpoint, u32 Width)
{
    u8 *At = (u8 *)SourceData;
    u32 *Pallet = (u32 *)PalletData;
    u32 *Pixel = (u32 *)TargetData;
    u32 X = 0;
    
    for(;;)
    {
        if(*At == 0)
        {
            if(++At >= DataEndpoint)
                return;
            
            if(*At == 0)
            {
                if(++At >= DataEndpoint)
                    return;
                
                while(X > Width)
                    X -= Width;
                
                Pixel += Width - X;
                X = 0;
                
                if(Pixel >= TargetEndpoint)
                    return;
            }
            else if(*At == 1)
            {
                return;
            }
            else if(*At == 2)
            {
                if(++At >= DataEndpoint)
                    return;
                
                Pixel += *At;
                X += *At;
                
                if(++At >= DataEndpoint)
                    return;
                
                Pixel += *At * Width;
                
                if(++At >= DataEndpoint)
                    return;
                if(Pixel >= TargetEndpoint)
                    return;
            }
            else
            {
                u32 PixelsToCopy = *At;
                b32 IsOddCount = 1 & PixelsToCopy;
                
                if(++At >= DataEndpoint)
                    return;
                
                while(PixelsToCopy--)
                {
                    if(*At >= PalletSize)
                        return;
                    *Pixel = Pallet[*At];
                    
                    if(++At >= DataEndpoint)
                        return;
                    if(++Pixel >= TargetEndpoint)
                        return;
                    X++;
                }
                
                if(IsOddCount)
                {
                    if(++At >= DataEndpoint)
                        return;
                }
            }
        }
        else
        {
            u32 PixelsToCopy = *At;
            
            if(++At >= DataEndpoint)
                return;
            
            if(*At >= PalletSize)
                return;
            u32 Color = Pallet[*At];
            
            if(++At >= DataEndpoint)
                return;
            
            while(PixelsToCopy--)
            {
                *Pixel = Color;
                
                if(++Pixel >= TargetEndpoint)
                    return;
                X++;
            }
        }
    }
}

void
RLE4(void *SourceData, void *DataEndpoint, void *PalletData, u32 PalletSize,
     void *TargetData, void *TargetEndpoint, u32 Width)
{
    u8 *At = (u8 *)SourceData;
    u32 *Pallet = (u32 *)PalletData;
    u32 *Pixel = (u32 *)TargetData;
    u32 X = 0;
    
    for(;;)
    {
        if(*At == 0)
        {
            if(++At >= DataEndpoint)
                return;
            
            if(*At == 0)
            {
                if(++At >= DataEndpoint)
                    return;
                
                while(X > Width)
                    X -= Width;
                
                Pixel += Width - X;
                X = 0;
                
                if(Pixel >= TargetEndpoint)
                    return;
            }
            else if(*At == 1)
            {
                return;
            }
            else if(*At == 2)
            {
                if(++At >= DataEndpoint)
                    return;
                
                Pixel += *At;
                X += *At;
                
                if(++At >= DataEndpoint)
                    return;
                
                Pixel += *At * Width;
                
                if(++At >= DataEndpoint)
                    return;
                if(Pixel >= TargetEndpoint)
                    return;
            }
            else
            {
                u32 PixelsToCopy = *At;
                b32 IsOddByteCount = 2 & (PixelsToCopy + 1);
                
                if(++At >= DataEndpoint)
                    return;
                
                while(PixelsToCopy--)
                {
                    u8 High = (*At) >> 4;
                    if(High >= PalletSize)
                        return;
                    *Pixel = Pallet[High];
                    
                    if(++Pixel >= TargetEndpoint)
                        return;
                    X++;
                    
                    if(PixelsToCopy)
                    {
                        PixelsToCopy--;
                        
                        u8 Low = (*At) & 0xf;
                        if(Low >= PalletSize)
                            return;
                        *Pixel = Pallet[Low];
                        
                        if(++Pixel >= TargetEndpoint)
                            return;
                        X++;
                    }
                    
                    if(++At >= DataEndpoint)
                        return;
                }
                
                if(IsOddByteCount)
                {
                    if(++At >= DataEndpoint)
                        return;
                }
            }
        }
        else
        {
            u32 PixelsToCopy = *At;
            
            if(++At >= DataEndpoint)
                return;
            
            u8 High = (*At) >> 4;
            u8 Low = (*At) & 0xf;
            
            if(High >= PalletSize)
                return;
            u32 ColorHigh = Pallet[High];
            u32 ColorLow = 0;
            
            if(PixelsToCopy > 1)
            {
                if(Low >= PalletSize)
                    return;
                ColorLow = Pallet[Low];
            }
            
            if(++At >= DataEndpoint)
                return;
            
            while(PixelsToCopy--)
            {
                *Pixel = ColorHigh;
                
                if(++Pixel >= TargetEndpoint)
                    return;
                X++;
                
                if(PixelsToCopy)
                {
                    PixelsToCopy--;
                    *Pixel = ColorLow;
                    
                    if(++Pixel >= TargetEndpoint)
                        return;
                    X++;
                    
                }
            }
        }
    }
}

b32
BMP_Reader(BMP_CoreBitmapHeader *BitmapHeader, void *FileEndpoint, void *BitmapData)
{
    // TODO(Zyonji): Implement a decoder for the core bitmap format.
    LogError("The BMP file uses an unsupported core bitmap format.", "BMP Reader");
    return(false);
}

b32
BMP_Reader(BMP_Os2BitmapHeader *BitmapHeader, void *FileEndpoint, void *BitmapData)
{
    // TODO(Zyonji): Implement a decoder for the OS/2 specific format.
    LogError("The BMP file uses an unsupported OS/2 specific format.", "BMP Reader");
    return(false);
}

b32
BMP_Reader(BMP_Win32BitmapHeader *BitmapHeader, void *FileEndpoint, void *BitmapData)
{
    u8 *HeaderStart = (u8 *)BitmapHeader;
    if(HeaderStart + BitmapHeader->Size >= FileEndpoint)
    {
        LogError("Invalid BMP header size.", "BMP Reader");
        return(false);
    }
    
    u8 *PaletteMemory = HeaderStart + BitmapHeader->Size;
    if(BitmapHeader->Compression == BMP_COMPRESSION_BITFIELDS)
    {
        PaletteMemory += 12;
    }
    else if(BitmapHeader->Compression == BMP_COMPRESSION_ALPHABITFIELDS)
    {
        PaletteMemory += 16;
    }
    
    u32 PalletSize = BitmapHeader->ColorsUsed;
    if(BitmapHeader->BitsPerPixel < 16 && BitmapHeader->ColorsUsed == 0)
    {
        PalletSize = 1 << BitmapHeader->BitsPerPixel;
    }
    
    if(PaletteMemory + PalletSize * 4 > FileEndpoint)
    {
        LogError("Invalid BMP pallet size.", "BMP Reader");
        return(false);
    }
    
    if(BitmapData == 0)
    {
        BitmapData = PaletteMemory + PalletSize * 4;
    }
    
    image_processor_tasks Processor = {};
    
    Processor.Width = BitmapHeader->Width;
    Processor.Height = Absolute(BitmapHeader->Height);
    if(BitmapHeader->Height > 0)
    {
        Processor.FlippedY = true;
    }
    Processor.BitsPerPixel = BitmapHeader->BitsPerPixel;
    Processor.ByteAlignment = 4;
    
    if(BitmapHeader->BitsPerPixel <= 8)
    {
        Processor.PalletData         = PaletteMemory;
        Processor.PalletSize         = PalletSize;
        Processor.BitsPerPalletColor = 32;
    }
    
    if(BitmapHeader->Compression == BMP_COMPRESSION_BITFIELDS || BitmapHeader->Compression == BMP_COMPRESSION_ALPHABITFIELDS)
    {
        Processor.RedMask   = BitmapHeader->RedMask;
        Processor.GreenMask = BitmapHeader->GreenMask;
        Processor.BlueMask  = BitmapHeader->BlueMask;
    }
    else
    {
        if(BitmapHeader->Compression == BMP_COMPRESSION_RGB && BitmapHeader->BitsPerPixel == 16)
        {
            Processor.RedMask   = 0x7c00;
            Processor.GreenMask = 0x3f0;
            Processor.BlueMask  = 0x1f;
        }
        else
        {
            Processor.RedMask   = 0xff0000;
            Processor.GreenMask = 0xff00;
            Processor.BlueMask  = 0xff;
        }
    }
    
    if(BitmapHeader->Size > 52)
    {
        if(BitmapHeader->Compression == BMP_COMPRESSION_BITFIELDS || BitmapHeader->Compression == BMP_COMPRESSION_ALPHABITFIELDS)
        {
            Processor.AlphaMask = BitmapHeader->AlphaMask;
        }
        else
        {
            if(BitmapHeader->Compression != BMP_COMPRESSION_RGB ||
               (BitmapHeader->BitsPerPixel != 16 && BitmapHeader->BitsPerPixel != 24))
            {
                Processor.AlphaMask = 0xff000000;
            }
        }
    }
    
    // TODO(Zyonji): Forward information for color correction.
    
    if(BitmapHeader->BitsPerPixel == 0 || BitmapHeader->Compression == BMP_COMPRESSION_JPEG || BitmapHeader->Compression == BMP_COMPRESSION_PNG)
    {
        LogError("The recursive JPEG/PNG decoding step is untested.", "BMP Reader");
        return(DisplayImageFromData(BitmapData, FileEndpoint));
    }
    else if(BitmapHeader->Compression == BMP_COMPRESSION_RLE8)
    {
        // TODO(Zyonji): Set a maximum size for image buffers
        u64 BufferSize = 4 * (u64)Processor.Width * (u64)Processor.Height;
        void *BitmapBuffer = RequestImageBuffer(BufferSize);
        void *BitmapBufferEndpoint = (u8 *)BitmapBuffer + BufferSize;
        
        if(BitmapBuffer == 0)
        {
            LogError("Unable to to allocate a BitmapBuffer.", "BMP Reader");
            return(false);
        }
        
        RLE8(BitmapData, FileEndpoint, PaletteMemory, PalletSize,
             BitmapBuffer, BitmapBufferEndpoint, BitmapHeader->Width);
        
        Processor.BitsPerPixel       = 32;
        Processor.PalletData         = 0;
        Processor.PalletSize         = 0;
        Processor.BitsPerPalletColor = 0;
        
        StoreImage(BitmapBuffer, Processor);
        FreeImageBuffer(BitmapBuffer);
    }
    else if(BitmapHeader->Compression == BMP_COMPRESSION_RLE4)
    {
        // TODO(Zyonji): Set a maximum size for image buffers
        u64 BufferSize = 4 * (u64)Processor.Width * (u64)Processor.Height;
        void *BitmapBuffer = RequestImageBuffer(BufferSize);
        void *BitmapBufferEndpoint = (u8 *)BitmapBuffer + BufferSize;
        
        if(BitmapBuffer == 0)
        {
            LogError("Unable to to allocate a BitmapBuffer.", "BMP Reader");
            return(false);
        }
        
        RLE4(BitmapData, FileEndpoint, PaletteMemory, PalletSize,
             BitmapBuffer, BitmapBufferEndpoint, BitmapHeader->Width);
        
        Processor.BitsPerPixel       = 32;
        Processor.PalletData         = 0;
        Processor.PalletSize         = 0;
        Processor.BitsPerPalletColor = 0;
        
        StoreImage(BitmapBuffer, Processor);
        FreeImageBuffer(BitmapBuffer);
    }
    else
    {
        u64 BitMask = (u64)Processor.ByteAlignment - 1;
        u64 BytesPerRow = (((u64)Processor.BitsPerPixel * (u64)Processor.Width + 7) / 8 + BitMask) & (~BitMask);
        if((u8 *)BitmapData + BytesPerRow * (u64)Processor.Height > FileEndpoint)
        {
            LogError("The file is too small to contain the bitmap.", "BMP Reader");
            return(false);
        }
        
        StoreImage(BitmapData, Processor);
    }
    
    return(true);
}

b32
BMP_Reader(void *FileMemory, void *FileEndpoint)
{
    BMP_FileHeader *FileHeader = (BMP_FileHeader *)FileMemory;
    if(FileHeader + 1 > FileEndpoint || FileHeader->Signature != BMP_SIGNATURE || FileHeader->Reserved != 0)
    {
        return(false);
    }
    
    u8 *BitmapHeader = (u8 *)&FileHeader->BitmapHeaderSize;
    u8 *BitmapData = (u8 *)FileHeader + FileHeader->BitmapOffset;
    
    if(FileHeader->BitmapHeaderSize == 12)
    {
        return(BMP_Reader((BMP_CoreBitmapHeader *)BitmapHeader, FileEndpoint, BitmapData));
    }
    else if(FileHeader->BitmapHeaderSize == 16 || FileHeader->BitmapHeaderSize == 64)
    {
        return(BMP_Reader((BMP_Os2BitmapHeader *)BitmapHeader, FileEndpoint, BitmapData));
    }
    else
    {
        return(BMP_Reader((BMP_Win32BitmapHeader *)BitmapHeader, FileEndpoint, BitmapData));
    }
}