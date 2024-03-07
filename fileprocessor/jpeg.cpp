#include "jpeg.h"

static u16
ReadBigEndianU16(void *Source, void *FileEndpoint)
{
    u16 *At = (u16 *)Source;
    if(At + 1 >= FileEndpoint)
    {
        LogError("The image data is incomplete.", "JPG reader");
        return(0);
    }
    return(SwapEndian(*At));
}

static void
InitializeHuffmanTable(jpeg_huffman_specification *Spec, jpeg_huffman_table *Table)
{
    u8 *LengthCounts = (u8 *)Spec->LengthCounts;
    
    Table->FirstCodeOfLength[0] = 0;
    Table->OffsetOfLength[0]    = 0;
    Table->Values               = (u8 *)Spec->CodeValues;
    
    u32 Offset = 0;
    u32 Code   = 0;
    for(u32 i  = 0; i < 16; i++)
    {
        Table->OffsetOfLength[i + 1]    = Offset;
        Table->FirstCodeOfLength[i + 1] = Code << (15 - i);
        
        Offset += LengthCounts[i];
        Code    = (Code + LengthCounts[i]) << 1;
    }
    for(u32 i = 15; LengthCounts[i] == 0; i--)
    {
        Table->FirstCodeOfLength[i + 1] = U32Max;
    }
    Table->FirstCodeOfLength[17] = U32Max;
    
    Spec->LengthCounts = 0;
}

static void
InitializeQuantizationTable(jpeg_quantization_table *Spec, s32 *Table)
{
    if(Spec->Precision)
    {
        u16 *Source = (u16 *)Spec->Table;
        for(u32 i  = 0; i < 64; i++)
        {
            *(Table++) = SwapEndian(*(Source++));
        }
    }
    else
    {
        u8 *Source = (u8 *)Spec->Table;
        for(u32 i  = 0; i < 64; i++)
        {
            *(Table++) = *(Source++);
        }
    }
    Spec->Table = 0;
}

static void
BufferBits(jpeg_bit_reader *Reader, u32 RequiredBitNumber)
{
    if(RequiredBitNumber > 32)
    {
        LogError("Bad bit length", "JPG reader");
        return;
    }
    
    while(Reader->StoredBits < RequiredBitNumber)
    {
        if(Reader->NextByte < Reader->SegmentEnd)
        {
            
            u8 Byte = *(Reader->NextByte++);
            if(Byte == 0xff)
            {
                Reader->NextByte++;
            }
            
            Reader->Buffer    <<= 8;
            Reader->Buffer     += Byte;
            Reader->StoredBits += 8;
        }
        else
        {
            Reader->Buffer   <<= 32 - Reader->StoredBits;
            Reader->StoredBits = 32;
            return;
        }
    }
}

static u8
ReadNextHuffmanCode(jpeg_bit_reader *Reader, jpeg_huffman_table *Table)
{
    if(Table->FirstCodeOfLength[17] != U32Max)
    {
        LogError("Undefined Huffman Table.", "JPG reader");
        return(0);
    }
    
    BufferBits(Reader, 16);
    u32 Code = (Reader->Buffer >> (Reader->StoredBits - 16)) & 0xffff;
    
    u8 CodeLength = 0;
    while(Code >= Table->FirstCodeOfLength[CodeLength + 1])
        CodeLength++;
    
    u32 CodeOffset =
        Table->OffsetOfLength[CodeLength] +
        ((Code - Table->FirstCodeOfLength[CodeLength]) >> (16 - CodeLength));
    
    u8 Symbol = Table->Values[CodeOffset];
    
    Reader->StoredBits -= CodeLength;
    
    return(Symbol);
}

static s32
ReadMagnitude(jpeg_bit_reader *Reader, u8 BitCount)
{
    BufferBits(Reader, BitCount);
    s32 Value = (Reader->Buffer >> (Reader->StoredBits - BitCount)) & ((1 << BitCount) - 1);
    Reader->StoredBits -= BitCount;
    
    if((Value >> (BitCount - 1)) == 0)
        Value = Value - (1 << BitCount) + 1;
    
    return(Value);
}

static s32
ReadBits(jpeg_bit_reader *Reader, u8 BitCount)
{
    BufferBits(Reader, BitCount);
    s32 Value = (Reader->Buffer >> (Reader->StoredBits - BitCount)) & ((1 << BitCount) - 1);
    Reader->StoredBits -= BitCount;
    
    return(Value);
}

static u8
ReadBit(jpeg_bit_reader *Reader)
{
    BufferBits(Reader, 1);
    u8 Value = (Reader->Buffer >> (Reader->StoredBits - 1)) & 1;
    Reader->StoredBits -= 1;
    
    return(Value);
}

static s32
DecodeBlockWhole(r32 *Output, jpeg_bit_reader *Reader, jpeg_scan_data *Scan, u32 *ZigZag,
                 s32 LastDCValue, u32 Component, u32 Sample,
                 u8 SelectionStart, u8 SelectionEnd, u8 BitPosition)
{
    u8 Byte     = ReadNextHuffmanCode(Reader, Scan->DCTable);
    s32 DCValue = ReadMagnitude(Reader, Byte) + LastDCValue;
    
    u32 Offset = Scan->Offset[Sample];
    Output[Offset] = (r32)(DCValue * Scan->QuantiTable[0]);
    
    for(u32 i = 1; i < 64; i++)
    {
        Byte = ReadNextHuffmanCode(Reader, Scan->ACTable);
        if(Byte == 0)
            break;
        
        i += Byte >> 4;
        
        if(i < 64)
        {
            s32 ACValue = ReadMagnitude(Reader, Byte & 0xf);
            
            Offset = Scan->Offset[Sample] + ZigZag[i];
            Output[Offset] = (r32)(ACValue * Scan->QuantiTable[i]);
        }
    }
    
    return(DCValue);
}

static s32
DecodeDCBlockBase(r32 *Output, jpeg_bit_reader *Reader, jpeg_scan_data *Scan, u32 *ZigZag,
                  s32 LastDCValue, u32 Component, u32 Sample,
                  u8 SelectionStart, u8 SelectionEnd, u8 BitPosition)
{
    u8 Byte     = ReadNextHuffmanCode(Reader, Scan->DCTable);
    s32 DCValue = ReadMagnitude(Reader, Byte) + LastDCValue;
    
    u32 Offset = Scan->Offset[Sample];
    Output[Offset] = (r32)(DCValue * Scan->QuantiTable[0] << BitPosition);
    
    return(DCValue);
}

static s32
DecodeDCBlockRefine(r32 *Output, jpeg_bit_reader *Reader, jpeg_scan_data *Scan, u32 *ZigZag,
                    s32 LastDCValue, u32 Component, u32 Sample,
                    u8 SelectionStart, u8 SelectionEnd, u8 BitPosition)
{
    u32 Offset = Scan->Offset[Sample];
    Output[Offset] += (r32)(ReadBit(Reader) * Scan->QuantiTable[0] << BitPosition);
    
    return(LastDCValue);
}

static s32
DecodeACBlockBase(r32 *Output, jpeg_bit_reader *Reader, jpeg_scan_data *Scan, u32 *ZigZag,
                  s32 EndOfBand, u32 Component, u32 Sample,
                  u8 SelectionStart, u8 SelectionEnd, u8 BitPosition)
{
    if(EndOfBand > 0)
    {
        return(EndOfBand - 1);
    }
    
    for(u32 i = SelectionStart; i <= SelectionEnd; i++)
    {
        u8 Byte = ReadNextHuffmanCode(Reader, Scan->ACTable);
        
        u8 Skip = Byte >> 4;
        u8 Bits = Byte  & 0xf;
        if(Bits > 0)
        {
            i += Skip;
            
            if(i <= SelectionEnd)
            {
                s32 ACValue = ReadMagnitude(Reader, Bits);
                
                u32 Offset = Scan->Offset[Sample] + ZigZag[i];
                Output[Offset] = (r32)(ACValue * Scan->QuantiTable[i] << BitPosition);
            }
        }
        else
        {
            if(Skip == 15)
            {
                i += 15;
            }
            else
            {
                EndOfBand = (1 << Skip) - 1;
                if(Skip > 0)
                    EndOfBand += ReadBits(Reader, Skip);
                
                break;
            }
        }
    }
    
    return(EndOfBand);
}

static s32
DecodeACBlockRefine(r32 *Output, jpeg_bit_reader *Reader, jpeg_scan_data *Scan, u32 *ZigZag,
                    s32 EndOfBand, u32 Component, u32 Sample,
                    u8 SelectionStart, u8 SelectionEnd, u8 BitPosition)
{
    u32 i = SelectionStart;
    r32  PlusBit = (r32)(  1  << BitPosition);
    r32 MinusBit = (r32)((-1) << BitPosition);
    r32 NewBit = 0;
    
    if(EndOfBand == 0)
    {
        for(; i <= SelectionEnd; i++)
        {
            u8 Byte = ReadNextHuffmanCode(Reader, Scan->ACTable);
            
            u8 Skip = Byte >> 4;
            u8 Bits = Byte  & 0xf;
            
            if(Bits == 0)
            {
                if(Skip != 15)
                {
                    EndOfBand = 1 << Skip;
                    if(Skip > 0)
                        EndOfBand += ReadBits(Reader, Skip);
                    
                    break;
                }
            }
            else
            {
                if(ReadBit(Reader) == 1)
                    NewBit = PlusBit;
                else
                    NewBit = MinusBit;
            }
            
            do
            {
                u32 Offset = Scan->Offset[Sample] + ZigZag[i];
                r32 *Coefficient = Output + Offset;
                if(*Coefficient > 0)
                    *Coefficient += ReadBit(Reader) * Scan->QuantiTable[i] << BitPosition;
                
                else if(*Coefficient < 0)
                    *Coefficient -= ReadBit(Reader) * Scan->QuantiTable[i] << BitPosition;
                
                else if(Skip-- == 0)
                    break;
                
                i++;
            } while(i <= SelectionEnd);
            
            if(Bits != 0 && i <= SelectionEnd)
            {
                u32 Offset = Scan->Offset[Sample] + ZigZag[i];
                Output[Offset] = NewBit * Scan->QuantiTable[i];
            }
        }
    }
    
    if(EndOfBand > 0)
    {
        for(; i <= SelectionEnd; i++)
        {
            u32 Offset = Scan->Offset[Sample] + ZigZag[i];
            r32 *Coefficient = Output + Offset;
            if(*Coefficient > 0)
                *Coefficient += ReadBit(Reader) * Scan->QuantiTable[i] << BitPosition;
            else if(*Coefficient < 0)
                *Coefficient -= ReadBit(Reader) * Scan->QuantiTable[i] << BitPosition;
        }
        
        return(EndOfBand - 1);
    }
    
    return(EndOfBand);
}

static u8 *
LocateNextMarker(void *CurrentMarker, void *FileEndpoint, u32 Length)
{
    u8 *At = (u8 *)CurrentMarker;
    
    if(Length)
    {
        At += 1 + Length;
        if(At >= FileEndpoint)
        {
            LogError("The image data is incomplete.", "JPG reader");
            return(0);
        }
    }
    
    for(;;)
    {
        while(*At != 0xff)
        {
            if(++At >= FileEndpoint)
            {
                LogError("The image data is incomplete.", "JPG reader");
                return(0);
            }
        }
        
        while(*At == 0xff)
        {
            if(++At >= FileEndpoint)
            {
                LogError("The image data is incomplete.", "JPG reader");
                return(0);
            }
        }
        
        if(*At != 0x00)
        {
            return(At);
        }
    }
}

static void
ReadDQT(u8 *Segment, void *SegmentEnd, jpeg_quantization_table *Targets)
{
    u8 *At = Segment;
    
    while(At < SegmentEnd)
    {
        u8 Precision   = *At >> 4;
        u8 Destination = *At  & 3;
        
        u8 *NextTable  = ++At + 64;
        if(Precision != 0)
            NextTable += 64;
        
        if(NextTable <= SegmentEnd)
        {
            Targets[Destination].Precision = Precision;
            Targets[Destination].Table     = At;
        }
        
        At = NextTable;
    }
}

static void
ReadDHT(u8 *Segment, void *SegmentEnd, jpeg_huffman_specification *Targets)
{
    u8 *At = Segment;
    
    while(At < SegmentEnd)
    {
        u8 TableClass  = 4 * ((*At >> 4) & 1);
        u8 Destination =  *At       & 3;
        
        u8 *NextTable  = ++At + 16;
        if(NextTable < SegmentEnd)
        {
            for(u32 i = 0; i < 16; i++)
            {
                NextTable += At[i];
            }
            
            if(NextTable <= SegmentEnd)
            {
                Targets[TableClass + Destination].LengthCounts = At;
                Targets[TableClass + Destination].CodeValues   = At + 16;
            }
        }
        
        At = NextTable;
    }
}

static void
ReadDAC(u8 *Segment, void *SegmentEnd, jpeg_arithmetic_conditioning_table *Targets)
{
    
    u8 *At = Segment;
    
    while(At < SegmentEnd)
    {
        u8 TableClass  = *At >> 4;
        u8 Destination = *At  & 3;
        
        At++;
        if(At < SegmentEnd)
        {
            if(TableClass == 0)
            {
                Targets[Destination].U = *At >> 4;
                Targets[Destination].L = *At  & 0xf;
            }
            else
            {
                Targets[Destination].Kx = *At;
            }
        }
        At++;
    }
}

static void
ReadDRI(u8 *Segment, void *SegmentEnd, u16 *RestartInterval)
{
    u16 *At = (u16 *)Segment;
    if(At + 1 <= SegmentEnd)
    {
        *RestartInterval = SwapEndian(*At);
    }
}

static void
ReadAPP0(u8 *Segment, void *SegmentEnd, u8 *JFIFPresent)
{
    u8 *At = Segment;
    if(*(At++) == 'J' && *(At++) == 'F' && *(At++) == 'I' && *(At++) == 'F' && *(At++) == 0)
        *JFIFPresent = true;
}

static void
ReadAPP14(u8 *Segment, void *SegmentEnd, u8 *AdobeTransform)
{
    u8 *At = Segment;
    if(*(At++) == 'A' && *(At++) == 'd' && *(At++) == 'o' && *(At++) == 'b' && *(At++) == 'e')
        *AdobeTransform = *(At + 6) + 1;
}

static void
ReadMiscTableSegment(u8 *Marker, u32 Length, jpeg_decoder_state *State)
{
    u8 *SegmentEnd = Marker + 1 + Length;
    switch(*Marker)
    {
        case JPEG_DQT:
        {
            ReadDQT(Marker + 3, SegmentEnd, State->QuantizationTables);
        } break;
        
        case JPEG_DHT:
        {
            ReadDHT(Marker + 3, SegmentEnd, State->HuffmanSpecification);
        } break;
        
        case JPEG_DAC:
        {
            ReadDAC(Marker + 3, SegmentEnd, State->ArithmeticConditioningTable);
        } break;
        
        case JPEG_DRI:
        {
            ReadDRI(Marker + 3, SegmentEnd, &State->RestartInterval);
        } break;
        
        case JPEG_APP0:
        {
            ReadAPP0(Marker + 3, SegmentEnd, &State->JFIFPresent);
        } break;
        
        case JPEG_APP14:
        {
            ReadAPP14(Marker + 3, SegmentEnd, &State->AdobeTransform);
        } break;
        // TODO(Zyonji): Look at APPn segments that could be useful for color managment.
    }
}

static void
DecodeImageData(u8 *ScanStart, void *FileEndpoint,void *ImageBuffer,
                jpeg_decoder_state *State, jpeg_decoder_buffers *DecoderBuffers,
                image_processor_tasks *Processor)
{
    jpeg_bit_reader *BitReader = &DecoderBuffers->BitReader;
    u8 *At = ScanStart;
    
    for(;;)
    {
        if(*At == JPEG_EOI)
            return;
        
        while(*At != JPEG_SOS)
        {
            u8 *Marker = At;
            u32 Length = ReadBigEndianU16(++At, FileEndpoint);
            At         = LocateNextMarker(Marker, FileEndpoint, Length);
            if(At == 0 || Length == 0 || *At == JPEG_EOI)
                return;
            
            ReadMiscTableSegment(Marker, Length, State);
        }
        
        u8 *SOSMarker = At;
        
        u32 Length     = ReadBigEndianU16(++At, FileEndpoint);
        u8 *NextMarker = LocateNextMarker(SOSMarker, FileEndpoint, Length);
        if(NextMarker == 0 || Length < 8)
            return;
        
        At += 2;
        u8 ComponentCount = *(At++);
        if(Length != (u32)(6 + 2 * ComponentCount))
            return;
        
        u8 MinHSamples = 4;
        u8 MinVSamples = 4;
        jpeg_scan_data *Scan = DecoderBuffers->Scan;
        for(u32 i = 0; i < ComponentCount; i++)
        {
            u8 ComponentIndex   = *(At++);
            u8 TableIndices     = *(At++);
            u8 DCTableIndex     = (TableIndices >> 4) & 0x3;
            u8 ACTableIndex     =  TableIndices       & 0x3;
            u8 QuantiTableIndex =  State->Components[ComponentIndex].QuantizationTableDestination;
            
            jpeg_huffman_table *DCTable = DecoderBuffers->DCTables + DCTableIndex;
            jpeg_huffman_table *ACTable = DecoderBuffers->ACTables + ACTableIndex;
            s32 *QuantiTable = DecoderBuffers->QuantizationTables[QuantiTableIndex];
            u8      HSamples = State->Components[ComponentIndex].HorizontalSamplingFactor;
            u8      VSamples = State->Components[ComponentIndex].VerticalSamplingFactor;
            
            Scan[i].Component     = ComponentIndex;
            Scan[i].DCTable       = DCTable;
            Scan[i].ACTable       = ACTable;
            Scan[i].QuantiTable   = QuantiTable;
            Scan[i].Samples       = HSamples * VSamples;
            Scan[i].HSamples      = HSamples;
            Scan[i].VSamples      = VSamples;
            Scan[i].ChannelOffset = State->Components[ComponentIndex].Offset;
            
            jpeg_huffman_specification  *DCSpec = State->DCHuffmanSpecification + DCTableIndex;
            jpeg_huffman_specification  *ACSpec = State->ACHuffmanSpecification + ACTableIndex;
            jpeg_quantization_table *QuantiSpec = State->QuantizationTables     + QuantiTableIndex;
            
            if(DCSpec->LengthCounts)
                InitializeHuffmanTable(DCSpec, DCTable);
            if(ACSpec->LengthCounts)
                InitializeHuffmanTable(ACSpec, ACTable);
            if(QuantiSpec->Table)
                InitializeQuantizationTable(QuantiSpec, QuantiTable);
            
            if(MinHSamples > HSamples)
                MinHSamples = HSamples;
            if(MinVSamples > VSamples)
                MinVSamples = VSamples;
            
            if(DCTable->FirstCodeOfLength[17] != U32Max || ACTable->FirstCodeOfLength[17] != U32Max)
            {
                LogError("Missing Huffman Tables required for decoding.", "JPG reader");
                return;
            }
        }
        
        u8 SelectionStart  = *(At++);
        u8 SelectionEnd    = *(At++);
        
        u8 BitPositionByte = *(At++);
        u8 LastBitPosition = BitPositionByte >> 4;
        u8 BitPosition     = BitPositionByte & 0xf;
        
        r32 *OutputStart   = (r32 *)ImageBuffer;
        u32  OutputWidth   = Processor->DCTWidth * 4;
        u32  YStep         = 8 * State->MaxVSamples / MinVSamples;
        u32  XStep         = 8 * State->MaxHSamples / MinHSamples;
        u32  Linecount     = (DecoderBuffers->Height + YStep - 1) / YStep;
        u32  SampleCount   = (DecoderBuffers->Width  + XStep - 1) / XStep;
        
        for(u32 c = 0; c < ComponentCount; c++)
        {
            u32 s = 0;
            for(u32 y = 0; y < Scan[c].VSamples; y++)
            {
                for(u32 x = 0; x < Scan[c].HSamples; x++)
                {
                    Scan[c].Offset[s++] = Scan[c].ChannelOffset +
                        x * 4 * 8 * Scan[c].HSamples / State->MaxHSamples +
                        y     * 8 * Scan[c].VSamples / State->MaxVSamples * OutputWidth;
                }
            }
            
            Scan[c].HSamples /= MinHSamples;
            Scan[c].VSamples /= MinVSamples;
        }
        
        u32 ZigZag[64];
        for(u32 i = 0; i < 64; i++)
        {
            ZigZag[i] = JPEG_ZIGZAG_INDEX_X[i] * 4 + JPEG_ZIGZAG_INDEX_Y[i] * OutputWidth;
        }
        
        s32 (*BlockDecoder)(r32 *, jpeg_bit_reader *, jpeg_scan_data *, u32 *,
                            s32, u32, u32, u8, u8, u8);
        
        if(LastBitPosition == 0)
        {
            if(SelectionStart == 0)
            {
                if(SelectionEnd == 63)
                    BlockDecoder = &DecodeBlockWhole;
                else
                    BlockDecoder = &DecodeDCBlockBase;
            }
            else
                BlockDecoder = &DecodeACBlockBase;
        }
        else
        {
            if(SelectionStart == 0)
                BlockDecoder = &DecodeDCBlockRefine;
            else
                BlockDecoder = &DecodeACBlockRefine;
        }
        
        u32 MinimumLineAdvancement = 8 * OutputWidth;
        u32 LineReturnMaximum   = Linecount;
        
        u32 RemainingMCUs       = Linecount * SampleCount;
        u32 MCUInterval         = (State->RestartInterval > 0) ? State->RestartInterval : RemainingMCUs;
        u32 RemainingMCUsInLine = SampleCount;
        u32 LineReturnCount     = 0;
        
        r32 *ChannelOutput[4] = {OutputStart, OutputStart, OutputStart, OutputStart};
        
        for(;;)
        {
            BitReader->NextByte   = At;
            BitReader->SegmentEnd = NextMarker - 1;
            BitReader->StoredBits = 0;
            s32 LastValue[4]      = {};
            
            if(MCUInterval > RemainingMCUs)
                MCUInterval = RemainingMCUs;
            
            RemainingMCUs -= MCUInterval;
            u32 RemainingMCUsInInterval = MCUInterval;
            
            while(RemainingMCUsInInterval-- > 0)
            {
                if(RemainingMCUsInLine-- == 0)
                {
                    if(++LineReturnCount == LineReturnMaximum)
                        break;
                    ChannelOutput[0] = OutputStart + LineReturnCount * Scan[0].VSamples * MinimumLineAdvancement;
                    ChannelOutput[1] = OutputStart + LineReturnCount * Scan[1].VSamples * MinimumLineAdvancement;
                    ChannelOutput[2] = OutputStart + LineReturnCount * Scan[2].VSamples * MinimumLineAdvancement;
                    ChannelOutput[3] = OutputStart + LineReturnCount * Scan[3].VSamples * MinimumLineAdvancement;
                    RemainingMCUsInLine = SampleCount - 1;
                }
                
                for(u32 c = 0; c < ComponentCount; c++)
                {
                    r32 *Output = ChannelOutput[c];
                    ChannelOutput[c] += 4 * 8 * Scan[c].HSamples;
                    
                    for(u32 s = 0; s < (u32)Scan[c].VSamples * Scan[c].HSamples; s++)
                    {
                        LastValue[c] = (*BlockDecoder)(Output, BitReader, Scan + c, ZigZag,
                                                       LastValue[c], c, s,
                                                       SelectionStart, SelectionEnd, BitPosition);
                    }
                }
            }
            
#if 0
            // TODO(Zyonji): Temporary test.
            if(BitReader->NextByte != BitReader->SegmentEnd)
                LogError("The scan data read was incomplete.", "JPG reader");
#endif
            
            At = NextMarker;
            
            if(!IsRSTm(*At))
                break;
            
            u8 *Marker = At++;
            NextMarker = LocateNextMarker(Marker, FileEndpoint, 0);
            if(At == 0)
                return;
        }
    }
}

b32
JPEG_Reader(void *FileMemory, void *FileEndpoint)
{
    u8 *At = (u8 *)FileMemory;
    if(At + 4 >= FileEndpoint || *(At++) != 0xff || *(At++) != JPEG_SOI || *(At++) != 0xff)
    {
        return(false);
    }
    
    jpeg_decoder_state State = {};
    State.DCHuffmanSpecification[0].LengthCounts = JPEG_DHT_DEFAULT_DC_LENGTHS_0;
    State.DCHuffmanSpecification[0].CodeValues   = JPEG_DHT_DEFAULT_DC_VALUES__0;
    State.DCHuffmanSpecification[1].LengthCounts = JPEG_DHT_DEFAULT_DC_LENGTHS_1;
    State.DCHuffmanSpecification[1].CodeValues   = JPEG_DHT_DEFAULT_DC_VALUES__1;
    State.ACHuffmanSpecification[0].LengthCounts = JPEG_DHT_DEFAULT_AC_LENGTHS_0;
    State.ACHuffmanSpecification[0].CodeValues   = JPEG_DHT_DEFAULT_AC_VALUES__0;
    State.ACHuffmanSpecification[1].LengthCounts = JPEG_DHT_DEFAULT_AC_LENGTHS_1;
    State.ACHuffmanSpecification[1].CodeValues   = JPEG_DHT_DEFAULT_AC_VALUES__1;
    
    while(!IsSOFn(*At))
    {
        u8 *Marker = At;
        u32 Length = ReadBigEndianU16(++At, FileEndpoint);
        At         = LocateNextMarker(Marker, FileEndpoint, Length);
        if(At == 0 || Length == 0 || *At == JPEG_EOI)
            return(false);
        
        ReadMiscTableSegment(Marker, Length, &State);
    }
    
    u8 *Marker     = At++;
    u8 *Segment    = At;
    u32 Length     = ReadBigEndianU16(Segment, FileEndpoint);
    u8 *NextMarker = LocateNextMarker(Marker, FileEndpoint, Length);
    if(NextMarker == 0 || Length < 8)
        return(false);
    
    if(*Marker != JPEG_SOF0 && *Marker != JPEG_SOF2)
        LogError("Compression format not tested.", "JPG reader");
    
    // TODO(Zyonji): Set the correct pixel size later.
    //u8 SamplePrecision = *(Segment + 2);
    u32 Height         = SwapEndian(*(u16 *)(Segment + 3));
    u32 Width          = SwapEndian(*(u16 *)(Segment + 5));
    u8  ComponentCount =                   *(Segment + 7);
    
    if(ComponentCount > 4 || Length != (u32)(8 + 3 * ComponentCount))
        return(false);
    
    u8 ChannelOffset   = 0;
    u8 UsedChannels[4] = {};
    for(u32 i = 8; i < Length;)
    {
        u8 Index           = *(Segment + i++);
        u8 SamplingFactors = *(Segment + i++);
        u8 HSamples        = SamplingFactors >> 4;
        u8 VSamples        = SamplingFactors  & 0xf;
        
        UsedChannels[ChannelOffset] = Index;
        if(HSamples == 0)
            HSamples = 1;
        if(VSamples == 0)
            VSamples = 1;
        
        State.Components[Index].HorizontalSamplingFactor     = HSamples;
        State.Components[Index].VerticalSamplingFactor       = VSamples;
        State.Components[Index].QuantizationTableDestination = *(Segment + i++) & 3;
        State.Components[Index].Offset                       = ChannelOffset++;
        
        if(State.MaxHSamples < HSamples)
            State.MaxHSamples = HSamples;
        if(State.MaxVSamples < VSamples)
            State.MaxVSamples = VSamples;
        
        if((SamplingFactors >> 4 == 3) || (SamplingFactors & 0xf) == 3)
        {
            LogError("Factor 3 subsampling isn't tested yet.", "JPG reader");
        }
    }
    
    u32  WidthMask = 8 * State.MaxHSamples - 1;
    u32 HeightMask = 8 * State.MaxVSamples - 1;
    
    image_processor_tasks Processor = {};
    Processor.Width           = Width;
    Processor.Height          = Height;
    Processor.DCTWidth        = ((Width  + WidthMask)  & ~WidthMask);
    Processor.DCTHeight       = ((Height + HeightMask) & ~HeightMask);
    Processor.ByteAlignment   = 1;
    Processor.DCTChannelCount = ChannelOffset;
    
    if(ComponentCount == 4)
    {
        if(State.AdobeTransform == 1)
            Processor.ColorSpace = COLOR_SPACE_CMYK;
        else
            Processor.ColorSpace = COLOR_SPACE_YCCK;
    }
    else if(ComponentCount == 3 && !State.JFIFPresent && State.AdobeTransform == 1)
        Processor.ColorSpace = COLOR_SPACE_sRGB;
    else if(ComponentCount == 2)
        Processor.ColorSpace = COLOR_SPACE_UNKNOWN;
    else
        Processor.ColorSpace = COLOR_SPACE_YCbCr;
    
    if(State.MaxHSamples > 1 || State.MaxVSamples > 1)
    {
        for(u32 i = 0; i < ChannelOffset; i++)
        {
            u8 Index = UsedChannels[i];
            
            Processor.ChannelStretchFactors[State.Components[Index].Offset][0] = State.MaxHSamples / State.Components[Index].HorizontalSamplingFactor;
            Processor.ChannelStretchFactors[State.Components[Index].Offset][1] = State.MaxVSamples / State.Components[Index].VerticalSamplingFactor;
        }
    }
    
    // TODO(Zyonji): Try out how OpenGL treats signed values. Format GL_RGBA_INTEGER instead of GL_RGBA.
    u64 ImageBufferSize    = (u64)Processor.DCTWidth * (u64)Processor.DCTHeight * sizeof(r32) * 4;
    u64 CombinedBufferSize = ImageBufferSize + sizeof(jpeg_decoder_buffers);
    
    
    
    void *Buffer = RequestImageBuffer(CombinedBufferSize);
    
    jpeg_decoder_buffers *DecoderBuffers = (jpeg_decoder_buffers *)((u8 *)Buffer + ImageBufferSize);
    DecoderBuffers->Width  = Width;
    DecoderBuffers->Height = Height;
    DecodeImageData(NextMarker, FileEndpoint, Buffer, &State, DecoderBuffers, &Processor);
    
    StoreImage(Buffer, Processor);
    
    FreeImageBuffer(Buffer);
    return(true);
}