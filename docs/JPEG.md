# Identifying a JPEG File
Compared to the PNG file format, finding comprehensive documentation for common `.jpg` files proved much more challenging. [JPEG](https://en.wikipedia.org/wiki/JPEG) doesn't refer to a file format, instead it's a compression method used by multiple file formats which share the same suffix naming convention. The most common file formats using this suffix are [Exif](https://en.wikipedia.org/wiki/Exif) and [JFIF](https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format). However, there are other file formats like [JPEG 2000](https://en.wikipedia.org/wiki/JPEG_2000) and [JPEG XL](https://en.wikipedia.org/wiki/JPEG_XL), which use the same core structure, but usually come with a different suffix. In trying to find documentation for various JPEG files, I found the [Documentation by Corkami](https://github.com/corkami/formats/blob/master/image/jpeg.md) to be helpful. Here I once again choose to only write decode file formats I encounter and add more later, when needed. This means for now I'll focus on the two most common formats, which both start with a `0xFF 0xD8` Start of Image marker.

# JPEG Data Structure
Similar to PNG files, JPEG files ([Annex B](https://www.w3.org/Graphics/JPEG/itu-t81.pdf)) are a chain of segments lead by an identify to denote what the purpose of the segment is. Most segments are lead by a 2 byte marker and 2 bytes with the length of the segment. Unfortunately, some segments lack a length. According to the specification in section [B.1.1.2](https://www.w3.org/Graphics/JPEG/itu-t81.pdf) we instead should rely on the `0xff` marker byte that leads every segment. Any `0xff` byte that's not followed by a `0x00` or `0xff` byte marks a new segment. This gives us our first most basic iteration of our file reader, which only checks if the file is made up of a series of known segments, starting with `FF D8` and ending on `FF D9`.
```cpp
b32
JPEG_Reader(void *FileMemory, void *FileEndpoint)
{
    u8 *At = (u8 *)FileMemory;
    if(At + 4 >= FileEndpoint || *(At++) != 0xff || *(At++) != 0xd8 || *(At++) != 0xff)
    {
        return(false);
    }
    
    while(*At != 0xd9)
    {
       while(*At != 0xff)
        {
            if(++At >= FileEndpoint)
            {
                LogError("The image data is incomplete.", "JPG reader");
                return(false);
            }
        }
        
        while(*At == 0xff)
        {
            if(++At >= FileEndpoint)
            {
                LogError("The image data is incomplete.", "JPG reader");
                return(false);
            }
        }
        
        if(*At != 0x00 && (*At < 0xc0 || *At == 0xff))
        {
            LogError("The image contains an unknown marker.", "JPG reader");
            return(false);
        }
    }
    
    if(At + 1 != FileEndpoint)
    {
        LogError("The image contains data after the end of image marker.", "JPG reader");
        return(false);
    }
    
    return(true);
}
```

When testing this code with various `.jpg` files, accepted by other software, the error for encountering an unknown marker is logged. This unknown marker is found within an APP2 `FF E2` segment. The APP2 segment contains length indicating a segment end past the unknown marker, followed by the string `ICC_PROFILE`. The format of this segment, aside of the length parameter, are specified by the [International Color Consortium](https://www.color.org/profile_embedding.xalter). To deal with such segments, we'll add a conditional at the start of our loop, for known segments with a length parameter, to advance the file pointer by the length.
- The segments defined to have a length parameter are: SOFn, DHT, DAC, SOS, DQT, DNL, DRI, DHP, EXP, APPn, COM
- The segments defined to have no length parameter are: RSTm, SOI, EOI
- And segments without a definition are: JPG, JPGn

```cpp
if(*At >= 0xc0 && (*At < 0xd0 || *At > 0xd7) && *At != 0xd8 && *At != 0xd9 && *At != 0xc8 && (*At < 0xf0 || *At > 0xfd))
{
    if(++At + 2 >= FileEndpoint)
    {
        LogError("The image data is incomplete.", "JPG reader");
        return(false);
    }
    At += SwapEndian(*(u16 *)At);
    if(At >= FileEndpoint)
    {
        LogError("The image data is incomplete.", "JPG reader");
        return(false);
    }
}
```

In testing, this addition proves sufficient to avoid all of the errors. 

The next task is to verify the segment order. For that, we follow the order given by [Figure B.2](https://www.w3.org/Graphics/JPEG/itu-t81.pdf) and break up our loop into multiple loops with more strict error checking. It's worth noting that for testing, I went more specific than the documentation because of two assumptions I made. First, the APPn segments usually carry meta data for the image, therefore I assumed that they always appear before the frame header. Secondly, the DNL marker only appears if the height parameter of the Frame Header is 0, therefore I assumed that to make decoding convenient, this format wouldn't be used. Both assumptions prove right in testing.
```cpp
b32
JPEG_Reader(void *FileMemory, void *FileEndpoint)
{
    u8 *At = (u8 *)FileMemory;
    if(At + 4 >= FileEndpoint || *(At++) != 0xff || *(At++) != 0xd8 || *(At++) != 0xff)
    {
        return(false);
    }
    
    while(!IsSOFn(*At))
    {
        At = LocateNextMarker(At, FileEndpoint);
        if(At == 0)
            return(false);
        
        if(*At != 0xc4 && *At != 0xcc && *At != 0xdb && *At != 0xdd && *At != 0xfe && !IsAPPn(*At) && !IsSOFn(*At))
        {
            LogError("Encountered an unexpected marker.", "JPG reader");
            return(false);
        }
    }
    
    while(*At != 0xda)
    {
        At = LocateNextMarker(At, FileEndpoint);
        if(At == 0)
            return(false);
        
        if(*At != 0xc4 && *At != 0xcc && *At != 0xdb && *At != 0xdd && *At != 0xfe && *At != 0xda)
        {
            LogError("Encountered an unexpected marker.", "JPG reader");
            return(false);
        }
    }
    
    while(*At != 0xd9)
    {
        At = LocateNextMarker(At, FileEndpoint);
        if(At == 0)
            return(false);
        
        if(*At != 0xc4 && *At != 0xcc && *At != 0xdb && *At != 0xdd && *At != 0xfe && *At != 0xda && !IsRSTm(*At) && *At != 0xd9)
        {
            LogError("Encountered an unexpected marker.", "JPG reader");
            return(false);
        }
    }
    
    if(At + 1 != FileEndpoint)
    {
        LogError("The image contains data after the end of image marker.", "JPG reader");
        return(false);
    }
    
    return(true);
```

For the actual JPEG reader, all the conditionals leading to errors won't be necessary. Their purpose is to confirm our understanding of the file structure. In the next step, we shall replace all those conditionals with code to process all expected segments. With all those assumptions I created the following loop:
```cpp
b32
JPEG_Reader(void *FileMemory, void *FileEndpoint)
{
    u8 *At = (u8 *)FileMemory;
    if(At + 4 >= FileEndpoint || *(At++) != 0xff || *(At++) != 0xd8 || *(At++) != 0xff)
    {
        return(false);
    }
    u8 *Marker;
    while(!IsSOFn(*At))
    {
        Marker = At;
        At = LocateNextMarker(Marker, FileEndpoint);
        if(At == 0)
            return(false);
        
        // Process the table-specification/miscellaneous section between Marker and At.
    }
    
    Marker = At;
    At = LocateNextMarker(Marker, FileEndpoint);
    if(At == 0)
        return(false);
    
    // Process the frame header section between Marker and At.
    
    while(*At != 0xd9)
    {
        while(*At != 0xda)
        {
            Marker = At;
            At = LocateNextMarker(Marker, FileEndpoint);
            if(At == 0)
                return(false);
            
            // Process the table-specification/miscellaneous section between Marker and At.
        }
        
        Marker = At;
        At = LocateNextMarker(Marker, FileEndpoint);
        if(At == 0)
            return(false);
        
        // Process the scan header section and enthropy encoded section between Marker and At.
        
        while(IsRSTm(*At))
        {
            Marker = At;
            At = LocateNextMarker(Marker, FileEndpoint);
            if(At == 0)
                return(false);
            
            // Process the restart marker and enthropy encoded section between Marker and At.
        }
    }
    
    if(At + 1 != FileEndpoint)
    {
        LogError("The image contains data after the end of image marker.", "JPG reader");
        return(false);
    }
    
    return(true);
```

When testing this loop I came across `.jpg` file, which are read correctly by other software, but trigger an error with this loop.

Error example: The file pointer advances correctly until it reaches the last SOS marker. The parameters are Ls = 8, Ns = 1, Cs1 = 1, Td1 = 0, Ta1 = 0, Ss = 1, Se = 63, Ah = 0, Al = 0. The marker data is valid. What follows is a entropy-coded segment with the first valid marker being 7 bytes before the end of the file in the sequence `FF FE 00 03 00 FF D9`. That marks a COM segment of length 3, containing only the character code `0x00`, followed by the End Of Image marker. According to documentation, a COM segment is a Miscellaneous segment, which should only appear before a Frame Header or Scan Header. 

Because other software views that image as valid, I'll modify the final loop treat the appearance of a Frame Header as optional:
```cpp
while(*At != 0xd9)
{
    while(*At != 0xda && *At != 0xd9)
    {
        Marker = At;
        At = LocateNextMarker(Marker, FileEndpoint);
        if(At == 0)
            return(false);
        
        // Process the table-specification/miscellaneous section between Marker and At.
    }
    
    if(*At == 0xda)
    {
        Marker = At;
        At = LocateNextMarker(Marker, FileEndpoint);
        if(At == 0)
            return(false);
        
        // Process the scan header section and enthropy encoded section between Marker and At.
        
        while(IsRSTm(*At))
        {
            Marker = At;
            At = LocateNextMarker(Marker, FileEndpoint);
            if(At == 0)
                return(false);
            
            // Process the restart marker and enthropy encoded section between Marker and At.
        }
    }
}
```

# Processing the Segments
There are 5 points in the JPEG reader function, where segment data needs to be processed. Two of them are the same table-specification/miscellaneous segments. This gives 4 separate decoding routines.

## Table-Specification and Miscellaneous Segments
These segments, relevant for the decoding process, each contain data to be stored in tables or variables. Multiple segments may write to the same variables. In that case, the decoder uses the values most recently assigned. To avoid unnecessary computation, we can perform a kind of lazy evaluation, were we store pointers to the data and only process that data when it's needed. Storing the data and pointers should be straight forward. In preparation, we want to note for each segment type, which variables they can write to.

### DQT - Quantization Table
```cpp
u8    QuantizationTablesPrecision[4];
void *QuantizationTables[4];
```

### DHT - Huffman Table
```cpp
void *LengthCounts[2][4];
void *CodeValues[2][4];
```
Although the specification say, that there are no default Huffman tables, I've encountered JPEG files with no Huffman tables defined. I those cases, using the example tables in [Section K.3](https://www.w3.org/Graphics/JPEG/itu-t81.pdf) successfully decoded the image as other image software does.

### DAC - Arithmetic Conditioning Table
```cpp
u8 Kx[4];
u8 U[4];
u8 L[4];
```

### DRI - Restart Interval
```cpp
u16 RestartInterval;
```

### COM - Comment
This segment contains comment data without direct impact on the decoding process.

### APPn - Application Data
These 16 segments are specific the actual image file format of the JPEG file. Specifications for those have to be found separately. Here the [Documentation by Corkami](https://github.com/corkami/formats/blob/master/image/jpeg.md) is useful, because lists more formats than I have found in my own testing. For the purpose of decoding the image data, these segments can be ignored. However, they are required to know how to interpret that image data. The APP0 JFIF is most commonly used to specify that the image data is in the YUV color space, but it's not the only possible color space. We'll return to those segments [Later](/docs/JPEG.md#file-format-specific-data).

## Frame Header
A JPEG file should contain exactly one frame header. In the specifications, there a 14 different frame headers defined. However, according to the [Documentation by Corkami](https://github.com/corkami/formats/blob/master/image/jpeg.md), only 5 of them are implemented by libjpeg. Looking at the code of that library, it looks like the arithmetic coding of `SOF9` and `SOF10` also result into an error exit. In testing, I've only encountered `SOF0` and `SOF2`, though `SOF0` and `SOF1` seem to be the same. The documentation for the arithmetic coding is a huge pain to read through, so I'll gladly throw an error for every header type other than the first 3, until I encounter them. That only for `SOF2`, we need to set a flag for progressive coding and otherwise treat the header segments the same.

The frame header specifies the dimensions of the image buffer. It's the height, width, number of color channels and bit depth of the channels. Additionally, it contains specifications for how each channel is mapped to the image data, allowing some channels to share data between adjacent pixels. This gives us a few more variables to store information for decoding process:
```cpp
void *ImageBuffer;
u16 Width;
u16 Height;
u8  BitsPerChannel;
u8  NumberOfChannels;    
u8  HorizontalSamplingFactor[255];
u8  VerticalSamplingFactor[255];
u8  QuantizationTableDestination[255];
```

To keep our memory management clear, we allocate all required buffers when encountering the frame header, next, call into an image decoder for the remaining image data, then hand the image buffer to the general image decoder and finish by freeing all the buffers again. Keeping the memory allocation and freeing close together in code helps with avoiding memory leaks.

## Scan Header
Unlike all segments before, this segment contains information only relevant for this specific scan. After the header follows a sequence of encoded 8 by 8 pixel blocks. The scan header specifies how the data in those blocks is mapped onto the image.

## Entropy-coded data segments
Right after the scan header follows a segment of encoded data without a marker leading it. In this segment, all `0xff` bytes are escaped with a `0x00` byte. The data is either Huffman encoded. Similar to the Huffman encoded segment in a PNG file, we want to treat the data as a bit stream. The difference here is, that instead of skipping to the next data segment, we just need to skip individual stuffing bytes.

### Bit Reader
The bit reader is similar to the one used to decode PNG files. However in the JPEG file the bits are taken from the most significant side first instead. Additionally, instead of advancing to the next chunk, the bit reader needs to skip every `0x00` byte that follows a `0xFF` byte.
```cpp
struct jpeg_bit_reader
{
    u32 Buffer;
    u32 StoredBits;
    u8 *NextByte;
    u8 *SegmentEnd;
};

static void
BufferBits(jpeg_bit_reader *Reader, u32 RequiredBitNumber)
{
    while(Reader->StoredBits < RequiredBitNumber)
    {
        if(Reader->NextByte < Reader->SegmentEnd)
        {
            u32 Byte = *(Reader->NextByte++);
            Reader->Buffer    <<= 8;
            Reader->Buffer     += Byte;
            Reader->StoredBits += 8;
            if(Byte == 0xff)
            {
                Reader->NextByte++;
            }
        }
        else
        {
            Reader->Buffer   <<= 32 - Reader->StoredBits;
            Reader->StoredBits = 32;
        }
    }
}
static u32
PeekBits(jpeg_bit_reader *Reader, u32 BitNumber)
{
    u32 Return = (Reader->Buffer >> (Reader->StoredBits - BitNumber)) & ((1 << BitNumber) - 1);
    return(Return);
}
static void
DropBits(jpeg_bit_reader *Reader, u32 BitNumber)
{
    Reader->StoredBits -= BitNumber;
}
```

### Huffman Decoding
Because of the reversed bit order, we can't use the same specifications as we used in the PNG decoder. Luckily, the JPEG format of the Huffman tables makes processing them easier as well. Because the highest significant bit is read first, subsequent Huffman codes are arithmetically bigger than previous codes. This allows us to create an array with the first code for each code length and a second array with the offset for each code length. That's enough to decode Huffman codes like this:
```cpp
BufferBits(BitReader, 16);
u16 Code = PeekBits(BitReader, 16);

u8 CodeLength = 0;
while(Code > FirstCodeOfLength[CodeLength + 1])
    CodeLength++;

u16 CodeOffset = OffsetOfLength[CodeLength] + ((Code - FirstCodeOfLength[CodeLength]) >> (16 - CodeLength));

u8 Symbol = SymbolList[CodeOffset];

DropBits(BitReader, CodeLength); 
```

The next step is to figure out which of the up to 8 Huffman tables we should use for the current bit stream. The scan is divided into 8 by 8 pixel blocks. One block for each of the 1 to 4 channels defined in the scan header. Each block contains 64 values. For each channel, the scan header defines which of the 4 DC and AC tables are used. Inside of the block, a DC table is used to decode the first bits and thereafter an AC table is used for Huffman decoding within the block. 

The byte decoded with the DC table specifies a number of bits to be consumed from the bit stream. Until we finish the Huffman decoding code, I'll just store the value of those bits and discard them. Each byte decoded by the AC table can be one of two values with special meaning. If it's `0x00`, then the remaining values in the block are all 0. If it's `0xF0`, then the next 16 values are 0. Otherwise the byte is split into two values. The 4 most significant bits specify a number of values to be set to 0. The lower 4 bits are treated the same as the byte that was decoded with the DC table.

That gives us the following loops to decode the entire image data:
```cpp
for(u32 y = 0; y < Height; y += 8)
{
    for(u32 x = 0; x < Width; x += 8)
    {
        for(u32 c = 0; c < ComponentsInScan; c++)
        {
            u8 Byte = ReadNextHuffmanCode(BitReader, DCTables[DCTableIndex[c]]);
            s32 Value = ConsumeBits(BitReader, Byte);
                
            for(u32 i = 1; i < 64; i++)
            {
                Byte = ReadNextHuffmanCode(BitReader, ACTables[ACTableIndex[c]]);
                if(Byte == 0)
                    break;
                
                i += Byte >> 4;
                
                if(i < 64)
                    Value = ConsumeBits(BitReader, Byte & 0xf);
            }
        }
    }
}
```
The decoded values are currently discarded, but the loop is sufficient to test if the data gets decoded cleanly.

A `.jpg` file containing a single pixel with the color `#808080` in the YCrCb color space would be all 0s. That should be encoded as 6 Huffman codes representing 0 bytes. This allows us to test if the Huffman decoders work properly.

A single black pixel `#000000` results in a value other than 0 for the Y channel. That results in the first Huffman code specifying a value greater than 0, which specifies a number of bits to be read before the next Huffman code. The remaining 5 Huffman codes should decode to 0 again. This allows us to confirm, that the right number of bits is read.

A single red pixel `#FF0000` should be computed correctly the same way. That means each DC decoding step should give a number of bits bigger than 0 to read, while each AC decoding step should return a 0 byte to skip to the next channel. 

Because of the cosine transform, the next simplest data is a greyscale image with 8 by 8 pixels, that change horizontally, but are identical vertically. That would only define horizontal waves, which, because of the zig zag structure, are values 0, 1, 5, 6, 14, 15, 27 and 28. As we step through the code while decoding the first channel, `i` should always equal one of those numbers, when a `Value` is set to something other than 0. The other two channels should be all 0s again. This allows us to confirm, that we interpret the bytes decoded by the AC tables correctly.

A colorful image with only horizontal changes should decode in the same manner, but it's still worth confirming. After that, a picture with only vertical changes can be tested, which should follow the sequence 0, 2, 3, 9, 10, 20, 21 and 35.

The next step up is a noisy 8 by 8 greyscale image, which decodes successful, if the last two channels are four 0 bytes again.

### DC Decoding
The DC value in each block is the value for the first cosine function, which is a constant. That means, the DC value effectively specifies the average color of each block. The DC value delta is specified by two variables defined in the Huffman decoding loop.
```cpp
u8  BitCount = ReadNextHuffmanCode(BitReader, DCTables[DCTableIndex[c]]);
s32 Value    = ConsumeBits(BitReader, BitCount);
```
The `Value` behaves like a regular integer if the most significant bit in the `BitCount` length scope is set. Otherwise it behaves like a negative signed integer, if all the bits outside of the scope where set to 1. However, if it's negative, then the value is one higher than this conversion would suggest, so the actual negative value is that conversion plus 1. This mapping allows a value for a bit count to represent the maximum count of numbers directly outside of the scope of the previous bit count. The conversion of the negative number with the highest bit equal 0 can be done like this:
```cpp
if(!(Value >> (Bitcount - 1)))
    Value = (0xffffffff << Bitcount) + Value + 1;
```
The `Value` currently is only a delta. We need to add it to the previous `Value` of the same channel. We can easily achieve that by declaring `Value` as an array outside of the scope of the loop as `s32 DCValue[4] = {}` and writing to `DCValue[c] +=` inside of the loop. Here it's important to note, that the value decoded using the AC table should not be stored in that variable.

That `DCValue` then needs to be multiplied with the first entry of the quantization table associated with the current channel.
```cpp
Block[c][0] = DCValue[c] * Scan[c].QuantizationTables[0];
```
For a greyscale pixel image, `Block[0][0]` should be equal to `(grey - 128) * 8`, where grey would be `0x80` for a `#808080` pixel. 

### AC Decoding
The difference between DC and AC values is, that only DC values are a delta that needs to be added to the previous value. The AC values however don't come in the order of a regular matrix array. Instead the values zigzag diagonally back and forth. The coordinates of index `i` inside of the block is easiest determined with look up tables.
```cpp
const u8 JPEG_ZIGZAG_INDEX_X[64] = {
    0, 1, 0, 0, 1, 2, 3, 2,
    1, 0, 0, 1, 2, 3, 4, 5,
    4, 3, 2, 1, 0, 0, 1, 2,
    3, 4, 5, 6, 7, 6, 5, 4,
    
    3, 2, 1, 0, 1, 2, 3, 4,
    5, 6, 7, 7, 6, 5, 4, 3,
    2, 3, 4, 5, 6, 7, 7, 6,
    5, 4, 5, 6, 7, 7, 6, 7
};

const u8 JPEG_ZIGZAG_INDEX_Y[64] = {
    0, 0, 1, 2, 1, 0, 0, 1,
    2, 3, 4, 3, 2, 1, 0, 0,
    1, 2, 3, 4, 5, 6, 5, 4,
    3, 2, 1, 0, 0, 1, 2, 3,
    
    4, 5, 6, 7, 7, 6, 5, 4,
    3, 2, 1, 2, 3, 4, 5, 6,
    7, 7, 6, 5, 4, 3, 4, 5,
    6, 7, 7, 6, 5, 6, 7, 7
};
```
Each value in the block matrix specifies a factor to multiply with a 2D cosine wave specific to that slot to add to the 8 by 8 pixels in the picture. For testing, I constructed a 1 pixel image that has exactly one of the cosine waves not set to 0. Value 39, at the block position `(4, 4)`, represents a symmetrical wave that results in a checker board pattern with each cell being width 2. To keep the average at `#808080`, I'll make the center 4 pixel `#FFFFFF` and adjacent squares `#010101`. The value of block position `(4, 4)` should be 1016 and every other value should be 0.

### Discrete Cosine Transform
The 8 by 8 block of magnitude values for cosine functions is part of the [Discrete Cosine Transform](https://en.wikipedia.org/wiki/Discrete_cosine_transform). That transformation is not unique to the JPEG compression format. It is also used by the WebP file format. Because of that, I choose to perform the DCT processing to the general image decoder. To save all the data required for that transformation, the image buffer needs it's height and width rounded up to the next multiple of 8. 
```cpp
u32 BufferWidth  = (Width  + 7) & (U32Max << 3);
u32 BufferHeigth = (Heigth + 7) & (U32Max << 3);
```

### Subsampling Mode
If the sampling factor for each channel aren't set to `1` in the frame header, then the image dimensions aren't equal for each color channel. It's common to have the chromaticity in the YCbCr color space half the size as the luminance channel. The sampling factors behave like ratios. Horizontal sampling factors `2`, `1` and `1` mean, that for every 2 luminance values in a row, there is only 1 chromaticity value for each channel. Multiplying the horizontal and vertical factor of a channel gives the count of blocks of that channel that come in sequence and a sequence is at most 10 blocks long. For example, factors `2x2`, `1x1` and `1x1` tells us to decode 4 blocks of the first channel and then 1 block of each of the other channels. To allow our current decoder to read files with subsampling correctly, we need to add one more inner loop after the channel loop and increase the step size in the outer loops by the maximum sampling factors:
```cpp
for(u32 y = 0; y < Height; y += 8 * MaximumVerticalSamplingFactor)
{
    for(u32 x = 0; x < Width; x += 8 * MaximumHorizontalSamplingFactor)
    {
        for(u32 c = 0; c < ComponentsInScan; c++)
        {
            for(u32 s = 0; s < HorizontalSamplingFactor[c] * VerticalSamplingFactor[c]; s++)
            {
                u8 Byte = ReadNextHuffmanCode(BitReader, DCTables[DCTableIndex[c]]);
                s32 Value = ConsumeBits(BitReader, Byte);
```

### Progressive Mode
If frame header SOF2 was encountered earlier, then the image is encoded in progressive mode. A progressive image has the image data spread over multiple scans, which have their data encoded in 4 different ways.

#### Initial DC Scan
If the scan header defines the start and end of selection as 0, then the scan only contains the AC codes of the image. Additionally, the scan header defines a current and previous point transform bit. That number specifies by how many bits the decoded value for each AC code should be shifted up. The previous point transform bit being set to 0 signifies that this is the initial scan. To decode these values, we just need to remove the loop for the 63 AC values from the default decoder.

In testing, I've only seen the point transform bit to 1, meaning the decoded value needs to be doubled to regain the original average block color. This can be tested with an image containing a series of 8 by 8 blocks of different grey tones.

#### DC Refinement Scan
Each scan with start and end of selection set to 0 and the last point transform larger than 0 specifies a refinement scan. The previous point transform should be exactly 1 larger than the current point transform. The scan contains a series of bits, one for each DC value. These then need to be added at the bit position specified by the point transform value.

Because usually only the least significant bit is set in the refinement scan, generating a color that requires that bit set is challenging. The conversion from YCbCr is the following equation:
```cpp
R = Cr / 8.0 * (2.0 - 2.0 * 0.299) + Y / 8.0;
B = Cb / 8.0 * (2.0 - 2.0 * 0.114) + Y / 8.0;
G = (Y / 8.0 - 0.114 * B - 0.299 * R) / 0.587;

R += 128;
G += 128;
B += 128;
```
A block with average color `Y = 1` would be average grey tone `128.125` That's equal to 56 pixels `#808080` and 8 pixels `#818181` inside of an 8 by 8 progressive JPEG. Putting two such blocks in sequence gives the bit sequence `100100`, resulting into the DC colors `(1, 0, 0) (1, 0, 0)`.

#### Initial AC Scan
If a scan header specifies a start of selection larger than 0 and a previous point transform equal to 0, then the section is the initial definition of AC values within the specified selection.

A further restriction for such scans is, that they only contain one component. This influences how subsampling behaves. With multiple channels, where one has a vertical sampling factor of 2 on a image with `(width / 8) % 2 = 1`, it would result into the last blocks decoded per line being discarded, because it lies outside of the image dimensions. If there's only a single channel with sampling factor 2, then no block is discarded, but other channels with sampling factor 1 still contain only half the number of blocks in that dimension.

The number of values per block is `SelectionEnd - SelectionStart + 1`.

The bytes decoded by the Huffman decoder are interpreted differently as well:
- If the lower 4 bits are not 0, then the byte is decoded as usual, with the resulting value being shifted up by the point transform value, like the value in the initial DC scan.
- If the lower 4 bits are 0, and the higher 4 bits are all set, giving the byte value `0xf0`, then the next 16 values in this block scope are 0. This is the same behavior as in sequential scans.
- If the lower 4 bits are 0, and the higher 4 bits equal a value smaller than 15, then the higher 4 bits specify a number of bits to be read from the bit stream. These bits are then appended to a `1` bit to create a number of blocks to be skipped. For example, if the bit count is 3, then the number of blocks to skip in bits is `1XXX`, which is a number between 8(`0b1000`) and 15(`0b1111`). The current block is always the first block to be skipped. That means, if the Huffman decoded byte is `0x00` then the remainder of the current block is skippe/set to 0 and the next data is decoded for the next block.
```cpp
if(EndOfBand > 0)
{
    return(EndOfBand - 1);
}
else
{
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
                Output[Offset] = ACValue << BitPosition;
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
}
```

To test this decoding step, I use the block coordinate `(4, 4)` cosine wave in the shape of a checker pattern again.

A single 8 by 8 block with the checker pattern in colors `#FFFFFF` and `#010101` should decode the same way as before. If the channel isn't the Y channel, or if the scope doesn't include index 39, then the scan data should contain only the Huffman code for value `0x00` to skip the block. Otherwise, it should be a sequence of `0xf0` bytes, until the index is within 15 steps of 39. Then the higher 4 bits should increase the index to 39 and using the lower 4 bits, the value 1016 should be calculated. After that, only `0x00` bytes should be decoded to skip all other blocks.

Next, to test the EndOfBand codes, I create a 24 by 8 image. Two `#808080` blocks and the `#FFFFFF` and `#010101` checker block third. This should be largely the same, but instead of `0x00` to skip one block per scan, we expect `0x10` bytes and a `1` bit from the bit stream, to specify 3 blocks to skip, and in the scan with the Y index 39 value a `0x10` byte with a `0` bit to skip the 2 blocks before the checker block.

Finally, to test the differences in subsampling, I export the same picture with 2:1:1 subsampling. In the AC scans, it should behave exactly the same, but in the DC scan, the Y channel consists of 8 blocks instead of 3.

#### AC Refinement Scan
If a scan header specifies a start of selection larger than 0 and a previous point transform larger than 0, then the section is the refinement of the AC values for the specified scope. The structure of the data for this scan is unfortunately a lot more complicated than the others.

Conceptually, it works like the initial scan to set values that previously haven't been set yet, but as the output pointer advances, for every value that has already been set before, a bit is retrieved from the bit stream to refine that value. However, if the high bits are set to skip a number of values, then that specifies only the number of values that haven't been set previously. To reduce the complexity task, I split it into simpler parts.

##### Only Refining Existing Values
First, the case where only previously set values are refined. In that case, the bit stream should always start with a Huffman code representing a byte with the lower bits set to `0`, but not the bit `0xf0`. That specifies a number of blocks to be skipped. In each skipped block, the previously set values then are refined with one bit from the bit stream. If the bit from the bit stream is `1`, then the specification says, "one shall be added to the (scaled) decoded magnitude of the coefficient." However, when counter checking other implementations, they add a bit shifted up by the point transform, if the existing value is positive and they subtract that bit if the existing value is negative. I will assume that is the correct action, even though it isn't apparent to me from the specification. If a block isn't skipped, then the Huffman decoder should give another byte specifying a number of blocks to skip.

```cpp
u32 i = SelectionStart;

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
        
// This part should not be executed yet.
        
    }
}

if(EndOfBand > 0)
{
    for(; i <= SelectionEnd; i++)
    {
        u32 Offset = Scan->Offset[Sample] + ZigZag[i];
        if(Output[Offset] > 0)
            Output[Offset] += ReadBit(Reader) << BitPosition;
        else if(Output[Offset] < 0)
            Output[Offset] -= ReadBit(Reader) << BitPosition;
    }
    
    rEndOfBand--;
}
```

For the test case of the `#FFFFFF` and `#010101` checker block, the Y channel scan should start with the Huffman decoder outputting a `0x00` byte, because no new cosine wave should have bits set. Then we need to scan the output values within the scope. Only index 39 should have a value that's not 0. On index 39, we retrieve one `0` bit from the bit stream. `0` means the bit at the point transform bit stays 0. The remaining bitstream should be fill `1`s until the next byte boundary.

Next goal is to set one magnitude to a value with the least significant bit set. The pixel `(x, y)` of a cosine wave with the block coordinate `(u, u)` is the magnitude of that coordinate multiplied with the following function:
```cpp
sqrt((clamp(u, 0, 1) + 1) / 8) * cos(u * PI * (x + 0.5) / 8) * sqrt((clamp(v, 0, 1) + 1) / 8) * cos(v * PI * (y + 0.5) / 8)
```
For `(0, 0)`, `(0, 4)`, `(4, 0)` and `(4, 4)` this equates to `1/8` or `-1/8` for all pixels, making all magnitudes multiples of 8 for grey tones. Instead we look at color. The goal is a `Cb` value with a complex fractional part, while `Y` and `Cr` are both 0. That restriction gives us the formula for RGB values `(Cr * (2.0 - 2.0 * 0.114) + 128, 0.114 * (R - 128), 0)`. For RGB value `(255, 113.522, 128)` equates to YCbCr `(0, 71.670428893905191873589164785553, 0)`. That would be ideal, but it's hard to set the green channel to `113.522`. RGB value `(128, 121.01, 163)` equates to YCbCr `(0, 19.751693002257336343115124153499, 0)` is easier to generate. That color then needs to be placed in a checker pattern with the complement around 128, `(128, 134.99, 93)`. This should set the 39 index in decoding to 158, but GIMP seems to set it to 160 instead, apparently rounding the YCbCr color values. Creating a test image will be more complicated.

If we assign to the 4 clean waves, `(0, 0)`, `(0, 4)`, `(4, 0)` and `(4, 4)`, the values `a`, `b`, `c` and `d`, then the block will have these values:
```
a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d   a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d
a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d   a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d
a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d   a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d
a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d   a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d
a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d   a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d
a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d   a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d
a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d   a+b-c-d   a-b-c+d   a-b-c+d   a+b-c-d
a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d   a+b+c+d   a-b+c-d   a-b+c-d   a+b+c+d
```
That gives us 4 pixel colors `a+b+c+d` `a-b+c-d` `a+b-c-d` and `a-b-c+d`. To make things easier, I choose that `b = c = d`, which means that `a+b+c+d = a+3*b` and `a-b+c-d = a+b-c-d = a-b-c+d = a-b`. To give valid RGB values, `a+3*b` and `a-b` must be withing the range of `-128*8` to `127*8`, with `a+3*b = max` and `a-b = min`, combined to `4*b = (maxGrey - minGrey)*8`. That in turn means that `b` is within the range of `-255*2` to `255*2`, the distance between the two used pixel colors. That means that valid pixel values never result into an odd value for `b`, but we can still have the second least significant bit set, by using distance 255. This should set the magnitude for index 10, 14 and 39 to `510` and the AC value to `-514`, for pixel colors `#FFFFFF` and `#000000`. Testing that pattern on a sequentially encoded JPEG gives us those expected values, making it suitable for testing the progressive encoding.

In testing, the initial DC scan with point transform set to the 1 bit retrieves value `-514` for channel Y and `0` for the other two channels. Next, the AC values are set with the point transform bit 2 set. This sets the values for index 10, 14 and 39 to `508`, the second least significant bit yet to be set as expected. Next follows the refinement for the second least significant bit. This should once again start with a Huffman decoded byte `0x00` to not add any new values. Then in the loop at index 10, 14 and 39, because each is `508`, meaning larger than 0, `2` should be added, to set the values to `510`. In the next loop for the least significant bit, no changes should be made.

To test the case for negative AC values, the colors of that pattern just need to be swapped. With `b = -510`, `a` is `506`, because the neutral grey is a bit closer to white than black. The initial values of the AC index 10, 14 and 39 are all set to `-508`. In the refine step, for each of the three non zero values, a `1` bit is read from the bit stream. This confirms, that for negative values, the bit is substracted instead of added to arrive at the value `-510`.

##### Only Defining New Values
The next case we handle is for when initial AC scan sets no values, because all the AC values are small enough to be rounded to 0 in the first point transform. In that case, the refinement scan functions the same as the initial scan, but with the bits retrieved from the bitstream after the Huffman code, being interpreted differently. The bit count contained in the lower 4 bits should only ever be `0x0` or `0x1`. If the bit count is 0, then values are skipped in the same manner as in the initial AC scan. If the bit count is 1, then one bit is retrieved from the bit stream. If the retrieved bit is `1`, then a bit at the point transform position is added. If the retrieved bit is `0`, then a bit at the point transform position is subtracted.

```cpp
u32 i = SelectionStart;

if(EndOfBand == 0)
{
    for(; i <= SelectionEnd; i++)
    {
        u8 Byte = ReadNextHuffmanCode(Reader, Scan->ACTable);
            
        u8 Skip = Byte >> 4;
        u8 Bits = Byte  & 0xf;
        
        s32 NewBit = 0;
        
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
                NewBit =   1  << BitPosition);
            else
                NewBit = (-1) << BitPosition);
        }
        
        for(; i <= SelectionEnd && Skip > 0; i++, Skip--)
        {
            u32 Offset = Scan->Offset[Sample] + ZigZag[i];
            if(Output[Offset] != 0)
            {
// This part should not be executed yet.
            }
        }
        
        if(NewBit != 0 && i <= SelectionEnd)
        {
            u32 Offset = Scan->Offset[Sample] + ZigZag[i];
            Output[Offset] = NewBit;
        }
    }
}

if(EndOfBand > 0)
{
    for(; i <= SelectionEnd; i++)
    {
        u32 Offset = Scan->Offset[Sample] + ZigZag[i];
        if(Output[Offset] > 0)
            Output[Offset] += ReadBit(Reader) << BitPosition;
        else if(Output[Offset] < 0)
            Output[Offset] -= ReadBit(Reader) << BitPosition;
    }
    
    rEndOfBand--;
}
```
To test this case, I use the same pattern, which sets cosine block waves `(0, 0)`, `(0, 4)`, `(4, 0)` and `(4, 4)`. This time, the goal is to have the value at index 10, 14 and 39 be `2`, to ensure it isn't set in the initial scan. This is achieved by making the difference of the grey tones in the colors used `1`, for example by using colors `#808080` and `#818181`.

As expected, the DC value is set to 2 and no AC value is set in the initial scan. In the refinement scan for the second least significant bit, the first decoded byte is `0x91`. This specifies a skip ahead to index 10 and reading one bit from the bitstream. The bit is `1`, meaning the new value is positive. This sets the value at index 10 to `2`. The next decoded byte is `0x31`, skipping ahead to index 14, setting it to positive `2` because of the `1` bit off the bitstream. The next decoded byte is `0xf0`, skipping ahead to index 30, without setting the value. The next decoded byte is `0x81`, skipping ahead to index 39, reading another bit, and setting the value to `2`. Lastly, a `0x00` byte is decoded to skip the remainder of the block. Afterwards, another AC refine is called for the least significant bit. This decodes a `0x00` byte to skip this block, however for the three set values a `0` bit should be read from the bitstream each.

To test negative values initialized in the refine scan, I use the colors `#808080` and `#7F7F7F` to set all values to `-2` instead.

##### Defining New Values Between Previously Defined Values
Finally, we combine the two other cases. New values are initialized, when the Huffman decoded byte has the lowest bit set to `1`. In that case, an additional bit is retrieved from the bit stream to determine the sign of the new value. The 4 high bits specify the number of zero values to be skipped before setting a value to the new value. If any non zero values are encountered before the value to be set, then for each of them a bit is retrieved from the bit stream to refine that value the same way as in the loop for when the `EndOfBand` is greater than 0. To achieve this, we only need to modify the loop we used to advance the index before. Instead of decreasing the `Skip` count with every loop, it should only be decreased, whenever a 0 value is encountered. Additionally, even if the skip count is 0, we need to advance the index until we find the first 0 value. And for each non zero value we advance past, we need to refine that value. For that purpose, we remove the `Skip` compare and decrement from the `for` loop header and instead place it in a else condition that triggers whenever the value for the current index is 0.
```cpp
for(; i <= SelectionEn; i++)
{
    u32 Offset = Scan->Offset[Sample] + ZigZag[i];
    if(Output[Offset] > 0)
    {
        Output[Offset] += ReadBit(Reader) << BitPosition;
    }
    else if(Output[Offset] < 0)
    {
        Output[Offset] -= ReadBit(Reader) << BitPosition;
    }
    else
    {
        if(Skip == 0)
            break;
        Skip--;
    }
}
```
Creating a predictable test images for this case is another complicated problem. Ideally we 2 AC values with an index smaller than 15 set. The first one with a large enough value to be set in the initial scan and the second small enough to be initialized in the refine scan. Additionally, we want the refine bit to be different from the sign bit of the new value. The cosine block waves `(0, 0)`, `(0, 4)`, `(4, 0)` and `(4, 4)` are useful for this purpose again. If all 4 values are `-2` in modulo 8, then the refine bits in the second least significant bit are set to `1`, while the sign bit for new values would be `0` to specify a negative value. The block positions `(0, 4)` and `(4, 0)` are associated with index 14 and 10 respectively. If the value at index 10 specifies a magnitude of `-1018`, and the other three waves have magnitude `-2`, then block matrix has the following magnitudes:
```
-1024   -1016   -1016   -1024   -1024   -1016   -1016   -1024
 1016    1016    1016    1016    1016    1016    1016    1016
 1016    1016    1016    1016    1016    1016    1016    1016
-1024   -1016   -1016   -1024   -1024   -1016   -1016   -1024
-1024   -1016   -1016   -1024   -1024   -1016   -1016   -1024
 1016    1016    1016    1016    1016    1016    1016    1016
 1016    1016    1016    1016    1016    1016    1016    1016
-1024   -1016   -1016   -1024   -1024   -1016   -1016   -1024
```
That's the 3 color values `#000000`, `#010101` and `#FFFFFF`, with DC = `-2`, AC index 10 = `-1018` and AC index 14 and 39 = `-2`. First, I use a sequential JPEG to confirm that the pattern indeed returns those values. In progressive decoding, the DC value is set to `-2`. In the initial AC scan sets the value of index 10 to `-1016`. Inside of the refine scan for the second least significant bit, the first Huffman decoded byte is `0xc1`. That's 12 zero values to be skipped before initializing one. Index 10 is already set, so skipping ahead 12 starting from 1, plus one already set, brings us to index 14 as expected. In the bit stream, the next two bits are `0` first and `1` second. The `0` specifies that the next initialized value is negative. The `1` is used to refine the non zero value. We store the first bit from the bit stream for later and advance the index. Once we reach index 10, we find the associate value to be `-1016`. The `1` from the bit stream adjusts it to `-1018`. After that, the count of zero values to skip reaches 0 when the index reaches 14. Because the lowest bit of the Huffman decoded byte is `1`, we know to use the previously stored bit set the value of index 14 to `-2`, because the stored bit is `0`. The next Huffman decoded byte is `0xf0` This means we should skip 16 zero values ahead. No other values are set, so we reach index 30 uninterrupted. The next Huffman decoded byte is `0x81`. Another 0 bit from the bit stream is stored. We advance to index 39 and set the associated value to `-2`, because of the stored `0` bit. Finally, a `0x00` byte is decoded to signify that the remainder of the block is skipped.  

## Restart Code
The restart markers come in sequence, but the sequence number doesn't contain any information. Whenever a restart marker is encountered, all data stored from decoding needs to be discarded. That means the bit buffer, the last AC values and the end of band counters all need to be set to 0. Restart markers should only appear in the data, if a restart interval was previously set. The restart interval specifies the number complete loops through the component loop have to be made, before the restart marker is encountered. That means, that in decoding, the restart intervals can be skipped, by counting up to the reset interval instead, to reset segment specific buffers.

# File Format Specific Data
After decoding the data, we have discrete cosine transform (DCT) data for up to 4 channels. How those channels need to interpreted depends on the file format of the file. The most JPEG files found online are using the JPEG File Interchange Format. The only other file format that I've found to give information for how to interpret image data is the "adobe" file format.

## JFIF
The [JFIF Specification](https://www.w3.org/Graphics/JPEG/jfif3.pdf) is seperate from the JPEG specification we used so far. The file format specifies, that it can be identified by an APP0 segment containing the null terminated string "JFIF" at the start of it's data. The remaining data of that segment is irrelevant for our purposes. What we care about is the specifications for how to interpret the image data, whenever such a segment is present.

- A JFIF image can contain either 1 or 3 channels. In both cases, the color space is "YCbCr as defined by CCIR 601 (256 levels)."
- The first block in the data always specifies the top left corner.
- If subsampling is used, then anti aliasing should be used to blend reduced channel pixels across the increased pixel count of the final picture.

## Adobe
I was unable to find specifications for this format. However, [LibJPEG](https://libjpeg.sourceforge.net) looks for an APP14 marker containing the "Adobe" string at the start of it's data to make assumptions about how to interpret the image data. The byte at offset 11 in the segment data seems to contain a `transform` index. If an APP14 "Adobe" marker is present, then 5 color spaces seem possible.
- 1 Channel: Y of the YCbCr color space.
- 3 Channels and `transform == 0`: RGB color space.
- 3 Channels and `transform == 1`: YCbCr color space.
- 4 Channels and `transform == 0`: CMYK color space.
- 4 Channels and `transform == 2`: YCCK color space.

## Unknown Format
Looking at the LibJPEG source code, I also noticed that if neither JFIF nor Adobe segments where found, then the decoder checks if the channel indices are `82`, `71` and `66` to spell out "RGB" in ASCII. I'm unaware, what file format would specify that, but it's still worth taking note of. Otherwise Y-Greyscale, YCbCr or CMYK are assumed, based on channel count, if no known file format is encountered.

# Data for the General Decoder
After decoding, we have data containing the magnitudes for a discrete cosine transform. The size of that buffer has different dimensions than the image, so for ease of computing, I'll forward those to the general decoder to signify that the data is DCT encoded.

[General DCT Decoder](/docs/GeneralImageDecoder.md#general-dct-decoder)

Next, if subsampling is enabled, then we need the relative sizes of each color channel. For that purpose, I forward an array of 8 integers to represent by which factor each channel has to be scaled up in each dimension. If the scale for the first channel is 0 in either dimension, I'll treat that as no scaling being required.

[Color Channel Mapping](/docs/GeneralImageDecoder.md#color-channel-mapping)

Finally, we need to forward information for which known color space the data is representing. For this I forward an index to represent the known color spaces. Here it's important to make the default `0` the unknown color space, in case the color space is represented through other data instead.

[Known Color Space Conversion](/docs/GeneralImageDecoder.md#known-color-space-conversion)