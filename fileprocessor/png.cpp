#include "png.h"

static void
AddAlphaToPallet(void *NewPallet, void *OldPallet, u32 PalletSize, void *AlphaData, u32 AlphaSize)
{
    u8 *To  = (u8 *)NewPallet;
    u8 *From = (u8 *)OldPallet;
    u8 *NewAlpha = (u8 *)AlphaData;
    u8 *NewAlphaEnd = NewAlpha + AlphaSize;
    u32 PixelsRemaining = PalletSize;
    while(PixelsRemaining--)
    {
        *(To++) = *(From++);
        *(To++) = *(From++);
        *(To++) = *(From++);
        *(To++) = (NewAlpha < NewAlphaEnd)?(*(NewAlpha++)):U8Max;
    }
}

static b32
AdvanceDataChunk(png_bit_reader *Reader)
{
    u8 *NewSegment = Reader->SegmentEnd + 12;
    if(NewSegment < Reader->FileEnd)
    {
        u32 SegmentLenght = SwapEndian(*(u32 *)(Reader->SegmentEnd + 4));
        Reader->NextByte = NewSegment;
        Reader->SegmentEnd = NewSegment + SegmentLenght;
        
        return(true);
    }
    return(false);
}
static void
BufferBits(png_bit_reader *Reader, u32 RequiredBitNumber)
{
    while(Reader->StoredBits < RequiredBitNumber)
    {
        if(Reader->NextByte < Reader->SegmentEnd)
        {
            Reader->Buffer += ((u32)*(Reader->NextByte++)) << Reader->StoredBits;
            Reader->StoredBits += 8;
        }
        else
        {
            if(!AdvanceDataChunk(Reader))
            {
                Reader->StoredBits = U32Max;
            }
        }
    }
}
static u32
ConsumeBits(png_bit_reader *Reader, u32 BitNumber)
{
    u32 Return = Reader->Buffer & ((1 << BitNumber) - 1);
    Reader->Buffer >>= BitNumber;
    Reader->StoredBits -= BitNumber;
    return(Return);
}
static void
FlushByte(png_bit_reader *Reader)
{
    Reader->Buffer >>= Reader->StoredBits & 0x7;
    Reader->StoredBits -= Reader->StoredBits & 0x7;
}
static void
CopyBytes(png_bit_reader *Reader, u8 *To, u32 Length)// Assumes Reader->Buffer to be empty.
{
    while(Length > 0)
    {
        if(Reader->NextByte < Reader->SegmentEnd)
        {
            Length--;
            *(To++) = *(Reader->NextByte++);
        }
        else
        {
            if(!AdvanceDataChunk(Reader))
            {
                while(Length--)
                    *(To++) = 0;
            }
        }
    }
}
static u32
PeekBits(png_bit_reader *Reader, u32 BitNumber)
{
    u32 Return = Reader->Buffer & ((1 << BitNumber) - 1);
    return(Return);
}
static void
DropBits(png_bit_reader *Reader, u32 BitNumber)
{
    Reader->Buffer >>= BitNumber;
    Reader->StoredBits -= BitNumber;
}

static void
PopulateDictionary(deflate_code *Dictionary, u32 DictionaryLength,
                   u16 *SortingBuffer, u8 *Lengths, u32 SymbolCount)
{
    u16 NumberOfUsedSymbols = 0;
    {
        u8 LengthFrequency[DEFLATE_MAX_LENGTH] = {};
        for(u16 i = 0; i < SymbolCount; i++)
        {
            LengthFrequency[Lengths[i]]++;
        }
        u16 OffsetPerLength[DEFLATE_MAX_LENGTH] = {};
        for(u8 i = 2; i < DEFLATE_MAX_LENGTH; i++)
        {
            OffsetPerLength[i] = OffsetPerLength[i - 1] + LengthFrequency[i - 1];
        }
        NumberOfUsedSymbols = OffsetPerLength[DEFLATE_MAX_LENGTH - 1] + LengthFrequency[DEFLATE_MAX_LENGTH - 1];
        for(u16 i = 0; i < SymbolCount; i++)
        {
            if(Lengths[i])
            {
                SortingBuffer[OffsetPerLength[Lengths[i]]++] = i;
            }
        }
    }
    
    u16 NextCode = 0;
    u16 i = 0; 
    while(i < NumberOfUsedSymbols)
    {
        u16 Symbol = SortingBuffer[i];
        u8 SymbolLength = Lengths[Symbol];
        u16 AliasOffset = 1 << SymbolLength;
        u16 Code = NextCode;
        
        deflate_code CodeData;
        CodeData.Value = Symbol;
        CodeData.Length = SymbolLength;
        while(Code < DictionaryLength)
        {
            Dictionary[Code] = CodeData;
            Code += AliasOffset;
        }
        
        i++;
        
        if(i < NumberOfUsedSymbols)
        {
            u16 Increment = 1 << (SymbolLength - 1);
            while(NextCode & Increment)
            {
                Increment >>= 1;
            }
            Assert(Increment != 0);
            NextCode &= Increment - 1;
            NextCode += Increment;
        }
    }
}

static void
Inflate(png_chunk *Chunk, void *FileEndpoint, png_decoding_buffers *Buffers, u64 BufferSize)
{
    u8 *To = (u8 *)Buffers->DeflateBuffer;
    u8 *BufferEnd = To + BufferSize;
    
    deflate_code *LiteralDictionary  = Buffers->LiteralDictionary;
    deflate_code *DistanceDictionary = Buffers->DistanceDictionary;
    
    u32 ChunkLength = SwapEndian(Chunk->Length);
    u8 *ChunkDataEnd = Chunk->Data + ChunkLength;
    if(ChunkDataEnd > FileEndpoint)
    {
        ChunkDataEnd = (u8 *)FileEndpoint;
    }
    png_bit_reader BitReader = {};
    BitReader.NextByte = Chunk->Data + 2;
    BitReader.SegmentEnd = ChunkDataEnd;
    BitReader.FileEnd = (u8 *)FileEndpoint;
    
    u8 DictionaryPresent = *(Chunk->Data + 1) & 0x20;
    if(DictionaryPresent)
    {
        BitReader.NextByte += 4;
    }
    
    if(BitReader.NextByte >= BitReader.SegmentEnd)
    {
        s64 BytesMissing = BitReader.NextByte - BitReader.SegmentEnd;
        if(!AdvanceDataChunk(&BitReader))
        {
            return;
        }
        BitReader.NextByte += BytesMissing;
    }
    
    b32 LastBlock = false;
    while(!LastBlock)
    {
        BufferBits(&BitReader, 3);
        LastBlock = ConsumeBits(&BitReader, 1);
        u32 CompressionType = ConsumeBits(&BitReader,2);
        switch(CompressionType)
        {
            case 0://Raw data block
            {
                FlushByte(&BitReader);
                BufferBits(&BitReader, 32);
                u16 Length = (u16)ConsumeBits(&BitReader, 16);
                u16 LengthInverse = (u16)ConsumeBits(&BitReader, 16);
                if(Length != (LengthInverse ^ 0xffff))
                {
                    LogError("Invalid raw data block length.", "PNG Reader");
                    return;
                }
                u8 *CopyEnd = To + Length;
                if(CopyEnd > BufferEnd)
                {
                    LogError("The decoded data stream overflows the image buffer.", "PNG Reader");
                    return;
                }
                CopyBytes(&BitReader, To, Length);
                To = CopyEnd;
            } break;
            
            case 3:
            {
                LogError("Invalid compression type used in the deflate compression.", "PNG Reader");
                return;
            } break;
            
            default://1 or 2
            {
                u8 *Lengths = Buffers->Lengths;
                u8 *Distances = Lengths;
                u32 LiteralLength;
                u32 DistanceLength;
                
                if(CompressionType == 1)//Fixed table
                {
                    LiteralLength = 288;
                    DistanceLength = 32;
                    Distances += LiteralLength;
                    u32 n = 0;
                    while(n < 144)
                        Lengths[n++] = 8;
                    while(n < 256)
                        Lengths[n++] = 9;
                    while(n < 280)
                        Lengths[n++] = 7;
                    while(n < 288)
                        Lengths[n++] = 8;
                    while(n < 320)
                        Lengths[n++] = 5;
                }
                else//Dynamic table
                {
                    BufferBits(&BitReader, 14);
                    LiteralLength = ConsumeBits(&BitReader, 5) + 257;
                    DistanceLength = ConsumeBits(&BitReader, 5) + 1;
                    u8 CodeLength = (u8)ConsumeBits(&BitReader, 4) + 4;
                    Distances += LiteralLength;
                    
                    for(u8 i = 0; i < CodeLength; i++)
                    {
                        BufferBits(&BitReader, 3);
                        Lengths[DEFLATE_ORDER[i]] = (u8)ConsumeBits(&BitReader, 3);
                    }
                    for(u8 i = CodeLength; i < 19; i++)
                    {
                        Lengths[DEFLATE_ORDER[i]] = 0;
                    }
                    PopulateDictionary(LiteralDictionary, 1 << 7, Buffers->SortingBuffer, Lengths, 19);
                    
                    u32 CodeCount = LiteralLength + DistanceLength;
                    u32 n = 0;
                    while(n < CodeCount)
                    {
                        BufferBits(&BitReader, 7);
                        deflate_code Code = LiteralDictionary[PeekBits(&BitReader, 7)];
                        DropBits(&BitReader, Code.Length);
                        
                        if(Code.Value < 16)
                        {
                            Lengths[n++] = (u8)Code.Value;
                        }
                        else
                        {
                            u8 RepeatValue;
                            u32 Repeats = 0;
                            if(Code.Value == 16)
                            {
                                if(n == 0)
                                {
                                    LogError("The compressed dynamic talbe started with an invalid code.", "PNG Reader");
                                    return;
                                }
                                BufferBits(&BitReader, 2);
                                RepeatValue = Lengths[n - 1];
                                Repeats = ConsumeBits(&BitReader, 2) + 3;
                            }
                            else if(Code.Value == 17)
                            {
                                BufferBits(&BitReader, 3);
                                RepeatValue = 0;
                                Repeats = ConsumeBits(&BitReader, 3) + 3;
                            }
                            else
                            {
                                BufferBits(&BitReader, 7);
                                RepeatValue = 0;
                                Repeats = ConsumeBits(&BitReader, 7) + 11;
                            }
                            
                            if(n + Repeats > CodeCount)
                            {
                                LogError("The compressed dynamic talbe overflows the lengths.", "PNG Reader");
                                return;
                            }
                            while(Repeats--)
                            {
                                Lengths[n++] = RepeatValue;
                            }
                        }
                    }
                    
                    if(Lengths[256] == 0)
                    {
                        LogError("The dynamic table is missing end of block code.", "PNG Reader");
                        return;
                    }
                }//End of dynamic table
                
                PopulateDictionary(LiteralDictionary, 1 << 15,
                                   Buffers->SortingBuffer, Lengths, LiteralLength);
                PopulateDictionary(DistanceDictionary, 1 << 15,
                                   Buffers->SortingBuffer, Distances, DistanceLength);
                
                deflate_code Code = {};
                while(Code.Value != 256)
                {
                    BufferBits(&BitReader, 15);
                    Code = LiteralDictionary[PeekBits(&BitReader, 15)];
                    DropBits(&BitReader, Code.Length);
                    if(Code.Value < 256)
                    {
                        if(To >= BufferEnd)
                        {
                            LogError("The decoded data stream overflows the image buffer.", "PNG Reader");
                            return;
                        }
                        *(To++) = (u8)Code.Value;
                    }
                    else if(Code.Value > 256)
                    {
                        u16 LengthCode = Code.Value - 257;
                        u8 ExtraBits = DEFLATE_LENGTH_BITS[LengthCode];
                        BufferBits(&BitReader, ExtraBits);
                        u32 BytesToCopy = ConsumeBits(&BitReader, ExtraBits) + DEFLATE_LENGTH_ADD[LengthCode];
                        
                        BufferBits(&BitReader, 15);
                        deflate_code DistanceCode = DistanceDictionary[PeekBits(&BitReader, 15)];
                        DropBits(&BitReader, DistanceCode.Length);
                        ExtraBits = DEFLATE_DISTANCE_BITS[DistanceCode.Value];
                        BufferBits(&BitReader, ExtraBits);
                        u32 Distance = ConsumeBits(&BitReader, ExtraBits) + DEFLATE_DISTANCE_ADD[DistanceCode.Value];
                        u8* From = To - Distance;
                        
                        u8 *CopyEnd = To + BytesToCopy;
                        if(CopyEnd > BufferEnd)
                        {
                            LogError("The decoded data stream overflows the image buffer.", "PNG Reader");
                            return;
                        }
                        while(BytesToCopy--)
                        {
                            *(To++) = *(From++);
                        }
                    }
                }
            } break;//End of compression type 1 and 2
        }
    }
}

static void
UndoFilter(u8 *At, u8 *To, u8 *RowEnd, u8 *LastRow, u32 BytesPerPixel)
{
    switch(*(At++))
    {
        case 0://none
        {
            while(At < RowEnd)
            {
                *(To++) = *(At++);
            }
        } break;
        
        case 1://sub
        {
            u8 *Left = To;
            u8 *SecondPixel = At + BytesPerPixel;
            while(At < SecondPixel)
            {
                *(To++) = *(At++);
            }
            while(At < RowEnd)
            {
                *(To++) = *(At++) + *(Left++);
            }
        } break;
        
        case 2://up
        {
            u8 *Up = LastRow;
            while(At < RowEnd)
            {
                *(To++) = *(At++) + *(Up++);
            }
        } break;
        
        case 3://average
        {
            u8 *Up = LastRow;
            u8 *Left = To;
            u8 *SecondPixel = At + BytesPerPixel;
            while(At < SecondPixel)
            {
                *(To++) = *(At++) + (*(Up++) / 2);
            }
            while(At < RowEnd)
            {
                *(To++) = *(At++) + (u8)(((u16)*(Left++) + (u16)*(Up++)) / 2);
            }
        } break;
        
        case 4://peath
        {
            u8 *a = To;
            u8 *b = LastRow;
            u8 *c = LastRow;
            u8 *SecondPixel = At + BytesPerPixel;
            while(At < SecondPixel)
            {
                *(To++) = *(At++) + *(b++);
            }
            while(At < RowEnd)
            {
                s32 pa = (s32)*b - (s32)*c;
                s32 pb = (s32)*a - (s32)*c;
                s32 pc = Absolute(pa + pb);
                pa     = Absolute(pa);
                pb     = Absolute(pb);
                
                u8 Pr = *a;
                if(pb < pa)
                {
                    pa = pb;
                    Pr = *b;
                }
                if(pc < pa)
                {
                    Pr = *c;
                }
                
                *(To++) = *(At++) + Pr;
                a++;
                b++;
                c++;
            }
        } break;
    }
}

static void
UndoFilters(u8 *Source, u8 *Target, u8 *RowBuffers,
            u32 Width, u32 Height, u32 BitsPerPixel)
{
    u32 BytesPerPixel = (BitsPerPixel + 7) / 8;
    u64 BytesPerRow = ((u64)BitsPerPixel * (u64)Width + 7) / 8;
    
    if(RowBuffers)
    {
        u8 *At = Source;
        u8 *Row = RowBuffers;
        u8 *LastRow = Row + BytesPerRow;
        
        if(BitsPerPixel < 8)
        {
            u32 PixelMask = 0x100 - (1 << (8 - BitsPerPixel));
            for(u8 Cycle = 0; Cycle < 7; Cycle++)
            {
                u32 oX = INTERLACE_X_OFFSET[Cycle];
                u32 iX = INTERLACE_X_INCREMENT[Cycle];
                u32 oY = INTERLACE_Y_OFFSET[Cycle];
                u32 iY = INTERLACE_Y_INCREMENT[Cycle];
                
                if(oX >= Width)
                    continue;
                
                u64 ScanlineIncrement = 1 + ((Width + iX - 1 - oX) / iX * BitsPerPixel + 7) / 8;
                
                for(u32 I = 0; I < BytesPerRow; I++)
                {
                    LastRow[I] = 0;
                }
                
                for(u32 Y = oY; Y < Height; Y += iY)
                {
                    u8 *ScanlineEnd = At + ScanlineIncrement;
                    UndoFilter(At, Row, ScanlineEnd, LastRow, BytesPerPixel);
                    
                    u8* From        = Row;
                    u64 ImageBit    = Y * BytesPerRow * 8 + oX * BitsPerPixel;
                    u8  CurrentByte = 0;
                    u32 BitsShifted = 8;
                    for(u32 X = oX; X < Width; X += iX)
                    {
                        if(BitsShifted >= 8)
                        {
                            CurrentByte = *(From++);
                            BitsShifted = 0;
                        }
                        
                        Target[ImageBit/8] |= (CurrentByte & PixelMask) >> (ImageBit & 7);
                        CurrentByte       <<= BitsPerPixel;
                        BitsShifted        += BitsPerPixel;
                        ImageBit           += iX * BitsPerPixel;
                    }
                    
                    At = ScanlineEnd;
                    u8* Temp = LastRow;
                    LastRow = Row;
                    Row = Temp;
                }
            }
        }
        else
        {
            for(u8 Cycle = 0; Cycle < 7; Cycle++)
            {
                u32 oX = INTERLACE_X_OFFSET[Cycle];
                u32 iX = INTERLACE_X_INCREMENT[Cycle];
                u32 oY = INTERLACE_Y_OFFSET[Cycle];
                u32 iY = INTERLACE_Y_INCREMENT[Cycle];
                
                u64 ScanlineIncrement = 1 + (Width + iX - 1 - oX) / iX * BitsPerPixel / 8;
                
                for(u32 I = 0; I < BytesPerRow; I++)
                {
                    LastRow[I] = 0;
                }
                
                for(u32 Y = oY; Y < Height; Y += iY)
                {
                    u8 *ScanlineEnd = At + ScanlineIncrement;
                    UndoFilter(At, Row, ScanlineEnd, LastRow, BytesPerPixel);
                    
                    u8* From = Row;
                    for(u32 X = oX; X < Width; X += iX)
                    {
                        for(u32 P = 0; P < BytesPerPixel; P++)
                        {
                            Target[P + X * BytesPerPixel + Y * BytesPerRow] = *(From++);
                        }
                    }
                    
                    At = ScanlineEnd;
                    u8* Temp = LastRow;
                    LastRow = Row;
                    Row = Temp;
                }
            }
        }
    }
    else
    {
        u8 *At = Source;
        u8 *Row = Target;
        u8 *LastRow = Row + BytesPerRow;
        
        u64 ScanlineIncrement = 1 + BytesPerRow;
        u32 LinesLeft = Height;
        while(LinesLeft--)
        {
            u8 *ScanlineEnd = At + ScanlineIncrement;
            UndoFilter(At, Row, ScanlineEnd, LastRow, BytesPerPixel);
            At = ScanlineEnd;
            LastRow = Row;
            Row += BytesPerRow;
        }
    }
}

b32
PNG_DataDecoder(void *FileMemory, void *FileEndpoint)
{
    png_file_header *Header = (png_file_header *)FileMemory;
    if(Header + 1 > FileEndpoint || Header->Signature != PNG_SIGNATURE || Header->TypeU32 != PNG_IHDR)
    {
        return(false);
    }
    
    u8 ColorType = Header->ColorType;
    u8 BitDepth = Header->BitDepth;
    u64 ChannelMask = (1 << BitDepth) - 1;
    
    image_processor_tasks Processor = {};
    Processor.Width         = SwapEndian(Header->Width);
    Processor.Height        = SwapEndian(Header->Height);
    Processor.BitsPerPixel  = BitDepth;
    Processor.ByteAlignment = 1;
    
    if(BitDepth > 8)
    {
        Processor.BigEndian = true;
    }
    
    switch(ColorType)
    {
        // For PNG_COLOR_TYPE_PALETTE, the color channel order is set when a pallet chunk is found.
        case PNG_COLOR_TYPE_GRAY:
        {
            Processor.RedMask   = ChannelMask;
            Processor.GreenMask = ChannelMask;
            Processor.BlueMask  = ChannelMask;
        } break;
        
        case PNG_COLOR_TYPE_RGB:
        {
            Processor.RedMask   = ChannelMask;
            Processor.GreenMask = ChannelMask << BitDepth;
            Processor.BlueMask  = ChannelMask << (BitDepth * 2);
            Processor.BitsPerPixel *= 3;
        } break;
        
        case PNG_COLOR_TYPE_GRAY_ALPHA:
        {
            Processor.RedMask   = ChannelMask;
            Processor.GreenMask = ChannelMask;
            Processor.BlueMask  = ChannelMask;
            Processor.AlphaMask = ChannelMask << BitDepth;
            Processor.BitsPerPixel *= 2;
        } break;
        
        case PNG_COLOR_TYPE_RGB_ALPHA:
        {
            Processor.RedMask   = ChannelMask;
            Processor.GreenMask = ChannelMask << BitDepth;
            Processor.BlueMask  = ChannelMask << (BitDepth * 2);
            Processor.AlphaMask = ChannelMask << (BitDepth * 3);
            Processor.BitsPerPixel *= 4;
        } break;
    }
    
    u8 *TransparencyMemory = 0;
    u32 TransparencyLenght = 0;
    
    png_chunk *Chunk = &Header->Chunk;
    u32 Length = SwapEndian(Chunk->Length);
    u8 *NextChunk = Chunk->OffsetBase + Length;
    Chunk = (png_chunk *)(NextChunk);
    
    while(Chunk + 1 <= FileEndpoint && Chunk->TypeU32 != PNG_IDAT)
    {
        Length = SwapEndian(Chunk->Length);
        NextChunk = Chunk->OffsetBase + Length;
        if(NextChunk <= FileEndpoint)
        {
            switch(Chunk->TypeU32)
            {
                case PNG_PLTE:
                {
                    if(ColorType == PNG_COLOR_TYPE_PALETTE)
                    {
                        Processor.PalletData         = Chunk->Data;
                        Processor.PalletSize         = Length / 3;
                        Processor.BitsPerPalletColor = 24;
                        Processor.RedMask            = 0x0000ff;
                        Processor.GreenMask          = 0x00ff00;
                        Processor.BlueMask           = 0xff0000;
                    }
                } break;
                
                case PNG_tRNS:
                {
                    if(ColorType == PNG_COLOR_TYPE_PALETTE)
                    {
                        TransparencyMemory = Chunk->Data;
                        TransparencyLenght = Length;
                    }
                    else if(ColorType == PNG_COLOR_TYPE_GRAY)
                    {
                        if(Processor.BitsPerPixel == 16)
                        {
                            Processor.TransparentColor = *((u16 *)Chunk->Data);
                        }
                        else
                        {
                            Processor.TransparentColor = 
                                *((u16 *)Chunk->Data) >> Processor.BitsPerPixel;
                        }
                    }
                    else if(ColorType == PNG_COLOR_TYPE_RGB)
                    {
                        if(Processor.BitsPerPixel == 48)
                        {
                            Processor.TransparentColor = 
                                ((u64)*(((u16 *)Chunk->Data) + 0) << 32) |
                                ((u64)*(((u16 *)Chunk->Data) + 1) << 16) |
                                ((u64)*(((u16 *)Chunk->Data) + 2)      );
                        }
                        else
                        {
                            Processor.TransparentColor = 
                                (*(((u16 *)Chunk->Data) + 0) << 8) |
                                (*(((u16 *)Chunk->Data) + 1)     ) |
                                (*(((u16 *)Chunk->Data) + 2) >> 8);
                        }
                    }
                } break;
#if 0
                // Colour space information
                case PNG_cHRM:
                case PNG_gAMA:
                case PNG_iCCP:
                case PNG_sBIT:
                case PNG_sRGB:
                case PNG_cICP:
                case PNG_mDCv:
                case PNG_cLLi:
                // Animation information
                case PNG_acTL:
                case PNG_fcTL:
                case PNG_fdAT:
                // Textual information
                case PNG_iTXt:
                case PNG_tEXt:
                case PNG_zTXt:
                // Image nformation, superfluous for decoding
                case PNG_bKGD:
                case PNG_hIST:
                case PNG_pHYs:
                case PNG_sPLT:
                case PNG_eXIf:
                case PNG_tIME:
#endif
            }
        }
        Chunk = (png_chunk *)(NextChunk);
    }
    
    u64 BytesPerRow       = ((u64)Processor.BitsPerPixel * (u64)Processor.Width + 7) / 8;
    u64 ImageBufferSize   = (u64)Processor.Height * BytesPerRow;
    u64 ScanlinePadding   = Processor.Height;
    u64 RowBufferSize     = 0;
    if(Header->Interlace == 1)
    {
        ScanlinePadding += Processor.Height - (Processor.Height / 8);
        RowBufferSize = 2 * BytesPerRow;
        if(Processor.BitsPerPixel < 8)
        {
            ScanlinePadding *= 2;
        }
    }
    u64 PalletBufferSize  = 0;
    if(TransparencyLenght != 0)
    {
        PalletBufferSize = Processor.PalletSize * 4;
    }
    
    u64 DeflateBufferSize  = ImageBufferSize + ScanlinePadding;
    u64 CombinedBufferSize = ImageBufferSize + 
        sizeof(png_decoding_buffers) - 1 + DeflateBufferSize +
        RowBufferSize + PalletBufferSize;
    
    void *Buffer = RequestImageBuffer(CombinedBufferSize);
    png_decoding_buffers *DeflateBuffers = (png_decoding_buffers *)((u8 *)Buffer + ImageBufferSize);
    u8* RowBuffers = 0;
    if(RowBufferSize)
    {
        RowBuffers = DeflateBuffers->DeflateBuffer + DeflateBufferSize;
    }
    if(PalletBufferSize)
    {
        u8* PalletBuffer = DeflateBuffers->DeflateBuffer + DeflateBufferSize + RowBufferSize;
        AddAlphaToPallet(PalletBuffer, Processor.PalletData, Processor.PalletSize,
                         TransparencyMemory, TransparencyLenght);
        Processor.PalletData         = PalletBuffer;
        Processor.BitsPerPalletColor = 32;
        Processor.AlphaMask          = 0xff000000;
    }
    
    Inflate(Chunk, FileEndpoint, DeflateBuffers, DeflateBufferSize);
    UndoFilters(DeflateBuffers->DeflateBuffer, (u8 *)Buffer, RowBuffers,
                Processor.Width, Processor.Height, Processor.BitsPerPixel);
    
    StoreImage(Buffer, Processor);
    
    FreeImageBuffer(Buffer);
    return(true);
}