# Identifying a Portable Network Graphics File
For this file format, I'll rely on the [Documentation by the World Wide Web Consortium](https://www.w3.org/TR/2023/CR-png-3-20230921/). Here we learn, that every PNG file is headed by a [PNG Signature](https://www.w3.org/TR/2023/CR-png-3-20230921/#3PNGsignature), which is equivalent to the eight character codes `"\211PNG\r\n\032\n"`. If that signature is present, then we can continue decoding the file as a PNG.

# PNG Data Structure
The remaining data of a PNG file after the signature is a sequence of chunks with no padding in between.

## Chunk Layout
Each chunk is made up of 3 variables and a data segment. In order they are `4` bytes containing a `Length` variable, `4` bytes containing the name of the chunk `Type`, a `Data` segment containing a number of bytes described in the `Length` variable and `4` bytes containing a `CRC` value. This makes the shortest possible chunk `12` bytes long, if the `Data` is length 0.

## Byte Order
At this point it's important to note, that variables in PNG files are stored with the most significant byte first (big endian) contrary to what most PCs do. That means, when reading the length of the `Data` segment, or any other variable, then we need to swap the byte order before performing arithmetic operations with the variable.

## Cyclic Redundancy Code
The `CRC` can be used to check the integrity of the chunk data. If we accessed the data through an unreliable feed, then could request a corrupted chunk to be sent again. For a painting software loading locally stored files, it's adequate to delegate detection of data corruption to the end user and try to interpret the data in whichever form it arrives.

## Chunk Types
Each chunk `Type` contains 4 character codes. Aside of identifying how the chunk should be processed, the formatting also contains information about the chunk type. If the first character is capitalized, then the chunk is required to correctly interpret the image data. If the decoder encounters an unknown type with the first character capitalized, then it should inform the user that the decoded image may be faulty.

## Chunk Type Order
The order has certain restrictions allowing us to simplify the decoding process. The restrictions are found in this [Table](https://www.w3.org/TR/2023/CR-png-3-20230921/#table53). 

If a chunk should be unique per file, then we don't need to protect against repeat declarations, because then there would be no correct interpretation. Instead we can allow the user to decide if they are satisfied with the interpretation of the corrupt file.

The a PNG file is laid out in a way to allow portions of it to be already displayed, even if the whole file isn't transmitted yet. That means that all information required to interpret pixel data is already defined, before the first `IDAT` chunk containing any pixel data. Additionally the first chunk is always `IHDR`, meaning that our PNG decoder consists of two loops. First, a loop reading chunks after `IHDR` until it encounters a `IDAT` chunk to collect data for how to interpret pixel data. And Second, a loop processing data chunks until either the allocated buffer is full, or the end of the file data is reached.

# Encoded Image Data
With the data contained in the `IHDR` chunk, we know large the stored image should be, but the data is currently [Compressed](https://www.w3.org/TR/2023/CR-png-3-20230921/#10Compression). Debugging a decoder is particularly challenging, because the corrupted output data doesn't necessarily reveal how it got corrupted. Instead I'll use [GIMP](https://www.gimp.org) to generate basic image files for testing.

## 1 Pixel No Compression
First I create a simple 1 pixel PNG with the colors `#FFAA00` and export it with compression level 0. This fills the data of the [`IDAT`](https://www.w3.org/TR/2023/CR-png-3-20230921/#11IDAT) chunk with `15` bytes: 

`0x08 0x1d 0x01 0x04 0x00 0xfb 0xff 0x00 0xff 0xaa 0x00 0x04 0x55 0x01 0xaa`

This data can be interpreted according to the [ZLIB](https://www.rfc-editor.org/rfc/rfc1950) and [Deflate](https://www.rfc-editor.org/rfc/rfc1951) documentation. The zlib format is defines the first 2 and last 4 bytes. The first two bytes can be divided into the following variables:
```cpp
CompressionMethod     =  Bytes[0] &  0xf           // 8 => Deflate
CompressionInfo       =  Bytes[0] >> 4             // 0
CompressionWindowSize = 1 << (CompressionInfo + 8) // 256
CheckValue            =  Bytes[1] &  0x1f          // 29 => 0x081d => 2077 % 31 == 0
DictionaryPresent     = (Bytes[1] >> 5) & 0x1      // 0 => no dictionary
CompressionLevel      =  Bytes[1] >> 6             // 0 => compressor used fastest algorithm
```
The `CompressionWindowSize` may be used to optimize memory requirements or ignored by assuming the maximum value. If `DictionaryPresent` is set, then the next 4 bytes are used for calculating the checksum. Because we don't care about file integrity, we discard those 4 bytes and the final 4 bytes containing the checksum. This leaves us in our example with the following data:

`0x01 0x04 0x00 0xfb 0xff 0x00 0xff 0xaa 0x00`

The first bit `Bytes[0] & 0x1 => 1` signifies that the current block is the last block. The two next bits `(Bytes[0] >> 1) & 0x3 => 0` signifies a block without compression. For a block without compression, the current byte is discarded.

`0x04 0x00 0xfb 0xff 0x00 0xff 0xaa 0x00`

The first 4 bytes give us the following 2 variables:
```cpp
Lenght           = Bytes[1] << 8 + Bytes[0] // 0x0004
LengthComplement = Bytes[3] << 8 + Bytes[2] // 0xfffb
```
This means that the next 4 bytes of our buffer are uncompressed data, which matches the size of the remainder of our data:

`0x00 0xff 0xaa 0x00`

This now is equivalent to our 1 pixel wide scanline. The first byte of which specifies the filter method `0`, meaning no filter was used. This means the 3 channel pixel values with 8 bit per channel are: `0xff 0xaa 0x00`, which are equivalent to the color `#FFAA00` we set in GIMP.

## 1 Pixel With Compression
This time we export the same 1 pixel PNG again, this time with compression enabled. This gives us the following data without the zlib format bytes:

`0x63 0xf8 0xbf 0x8a 0x1 0x0` or `000000000000000110001010101111111111100001100011` as a bit stream from right to left.

The first bit `Bytes[0] & 0x1 => 1` again signifies that the current block is the last block. The two next bits `(Bytes[0] >> 1) & 0x3 => 1` however signify a compression with fixed Huffman codes instead. That means we need to look up the next 15 bits `111111100001100` in the default Huffman Table. This matches the table entry length 8 `00001100` for code value `0x00`. This gives us the following output buffer and bitstream:

Output: `0x00` Bitstream: `0000000000000001100010101011111111111`

The next matching table entry is length 9 `111111111` for value `0xff`.

Output: `0x00 0xff` Bitstream: `0000000000000001100010101011`

The next matching table entry is length 9 `010101011` for value `0xaa`.

Output: `0x00 0xff 0xaa` Bitstream: `0000000000000001100`

And then again the length 8 `00001100` for code value `0x00`.

Output: `0x00 0xff 0xaa 0x00` Bitstream: `00000000000`

And finally, table entry length 7 `0000000` for code `256` which signifies the end of the decoding block, meaning we discard the final for bits `0000`. Our output `0x00 0xff 0xaa 0x00` is as expected the same scanline as before.

### Huffman Table Generator
This case is mostly trivial aside of the code needed to generate the Huffman Table. The Fixed Table can be constructed the same way as a Dynamic Table with the count of Literal/Length codes, the count of Distance codes and an array of `320` Length values defined like this:
```cpp
u8  Lengths[288];
u32 n = 0;
while(n < 144)
    Lengths[n++] = 8;
while(n < 256)
    Lengths[n++] = 9;
while(n < 280)
    Lengths[n++] = 7;
while(n < 288)
    Lengths[n++] = 8;

u8  Distances[32];
n = 0;
while(n < 32)
    Distances[n++] = 5;
```
Those arrays are then used to generate two Huffman Tables. To generate a table, we first need to sort the codes by length:
```cpp
u16 SortingBuffer[288];
u16 SymbolCount = 288;
u16 NumberOfUsedSymbols = 0;
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
        SortingBuffer[OffsetPerLength[Lengths[i]]++] = i;
}
```
After the sorting is done, `SortingBuffer` contains the indices of the `Lengths` array sorted from shortest length to longest. It would be enough to assign each of the symbols a Huffman Code in order, however to make it easier to look up the codes, I fill an array with all possible codes of the maximum code length with entries for the associated symbol and actual length of the code.
```cpp
u16 DictionaryLength = 1 << 15;
deflate_code Dictionary[DictionaryLength];
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
        NextCode &= Increment - 1;
        NextCode += Increment;
    }
}
```
The code segment after the `i++` increment may not be readily apparent for it's function. It's purpose is to increment an integer in reverse bit order. While a normal increment takes a byte `11010011` and turns it into `11010100`, this function turns it into `00110011` instead.

## 21x1 PNG with Dynamic Table
To test the dynamic table, we need a picture with a limited number of byte values that aren't repeating. For this purpose I constructed the following 21 RGB pixels:

Pixel data:
```
#000101 #000202 #000404 #000808 #001010 #002020 #004040 #010001 #020002 #040004 #080008 #100010 #200020 #400040 #010100 #020200 #040400 #080800 #101000 #202000 #404000
```
Compressed data:
```
0x65 0xca 0x31 0x11 0x00 0x30 0x08 0x00 0xb1 0x7f 0x8e 0x81 0x11 0x09 0xf8 0x57 0x01 0xce 0x2a 0xa0 0x99 0x23 0x8a 0x12 0x41 0x26 0x55 0x74 0x33 0xe3 0x9e 0x18 0x44 0x92 0x45 0x35 0x3d 0xcc 0xb9 0x5f 0xe4 0x01 0x82 0x2e 0x04 0x82 0x16 0x43
```

As desired, after the first bit for final block flag, the next two are set to 2, specifying a dynamic table. The dynamic table block starts with 14 `111001'01001100` bits split into 3 variables.
```cpp
BufferBits(14);
LiteralLength  = ConsumeBits(5) + 257 // 01100 = 12 => 12 + 257 = 269
DistanceLength = ConsumeBits(5) + 1   // 01010 = 10 => 10 + 1   = 11
CodeLength     = ConsumeBits(4) + 4   //  1110 = 14 => 14 + 4   = 18
```
The `CodeLength` tells us that the next `3 * 18 = 54` bits are the lengths for our first Huffman Table. Here are the values read with the right most value being the first:
```
011 000 100 000 000 000 010 000 011 000 000 000 000 000 100 010 011 000
 3   0   4   0   0   0   2   0   3   0   0   0   0   0   4   2   3   0
``` 
The lengths come in the order `16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15`. Value 15 isn't included in our bit stream, so it should be set to 0 instead. This gives us these length array values:
```cpp
Lengths[19] = {4, 3, 4, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2};
```
We can use the same algorithm we used to generate the Huffman Table as we used for the fixed Table. This gives us the following codes:
```
  00 =>  4
  10 => 18
 001 =>  1
 101 =>  5
 011 => 17
0111 =>  0
1111 =>  2
```
Most values don't have a code associated, because they aren't needed to decode the following bit stream with the first bit right end. The code values 16, 17 and 18 read an additional 2, 3 and 7 bits, which I noted in square brackets `[]`.
```
001 110 011 001 101 0000000 10 101 0110011 10 00 00 0001010 10 1111111 10 00 0000100 10 00 100 011 00 000 011 00 0111 00 1111 1111
 1  [6] 17   1   5    [0]   18  5   [51]   18  4  4  [10]   18  [127]  18  4   [4]   18  4 [4]  17  4 [0]  17  4   0   4   2    2
```
Values `17` and `18` are used to skip a number of entries. This sets the following values in our Lengths array:
```cpp
Lengths[  0] = 2
Lengths[  1] = 2
Lengths[  2] = 4
Lengths[  4] = 4
Lengths[  8] = 4
Lengths[ 16] = 4
Lengths[ 32] = 4
Lengths[192] = 4
Lengths[193] = 4
Lengths[256] = 5
Lengths[268] = 5
Lengths[269] = 1
Lengths[279] = 1
```
Entries past `LiteralLength`, in this case `269` are distance codes for a separate Huffman Table for distance codes. Those Length codes can then once again be converted into two Huffman Code tables:
```
   00 =>   0
   10 =>   1
 0001 =>   2
 1001 =>   4
 0101 =>   8
 1101 =>  16
 0011 =>  32
 1011 => 192
 0111 => 193
01111 => 256
11111 => 268
```
```
0 =>  0
1 => 10
```
We can then use those tables to decode the remaining bit stream until we encounter the code `01111 => 256`. You'll note that the final 6 bits `000000` are padding. From `00000001111001000101111110111001110011000011110100110101010001011001001001000100000110001001111011100011001100110111010001010101001001100100000100010010100010100010` to `0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x04, 0x00, 0x08, 0x08, 0x00, 0x10, 0x10, 0x00, 0x20, 0x20, 0x01, 0xc0, 0xc1, 0x01, 0x00, 0x01, 0x02, 0x00, 0x02, 0x04, 0x00, 0x04, 0x08, 0x00, 0x08, 0x10, 0x00, 0x10, 0x20, 0x00, 0x20, 0xc1, 0x01, 0xc0, 0x01, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x04, 0x00, 0x08, 0x08, 0x00, 0x10, 0x10, 0x00, 0x20, 0x20, 0x00`.

Including the first byte declaring the subtract filter method, the first 13 bytes `0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x02, 0x02, 0x00, 0x04, 0x04` should be equivalent to the first 4 pixels `#000101 #000202 #000404 #000808`. Without the filter, it would be the color values `#000101 #000101 #000202 #000404`. Progressively adding the previous pixel values gives us the right image.

### Bit Reader
In the decoding process, we often request bits or bytes from the input stream. Either a series of bits, up to `16` bits long, or individual bytes from the byte stream. Instead of worrying about data chunk boundaries for every read, I decided to write functions that handle operations on the data stream, as if it was infinite. With a valid PNG file stored locally on the PC, the byte stream should never end before the image is filled. However, to protect against malicious data, we have to consider that case too. It is simplest to return 0 for requests outside of the given data.

In the Deflate decoder we have 6 different ways we want to interact with the bit reader:
1. Read bytes from the byte stream into the buffer, until it contains at least n bits.
2. Consume up to `16` bits from the buffer and return them as a value.
3. Drop bits until the buffer is byte aligned.
4. Consume a number of bytes from the byte stream and store them in another buffer.
5. Return the value of the next up to `15` bits from the buffer without consuming them.
6. Drop up to `15` bits from the buffer.
```cpp
struct deflate_bit_reader
{
    u32 Buffer;
    u32 StoredBits;
    u8 *NextByte;
    u8 *SegmentEnd;
    u8 *FileEnd;
};
static void
BufferBits(deflate_bit_reader *Reader, u32 RequiredBitNumber)
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
ConsumeBits(deflate_bit_reader *Reader, u32 BitNumber)
{
    u32 Return = Reader->Buffer & ((1 << BitNumber) - 1);
    Reader->Buffer >>= BitNumber;
    Reader->StoredBits -= BitNumber;
    return(Return);
}
static void
FlushByte(deflate_bit_reader *Reader)
{
    Reader->Buffer >>= Reader->StoredBits & 0x7;
    Reader->StoredBits -= Reader->StoredBits & 0x7;
}
static void
CopyBytes(deflate_bit_reader *Reader, u8 *To, u32 Length)// Assumes Reader->Buffer to be empty.
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
PeekBits(deflate_bit_reader *Reader, u32 BitNumber)
{
    u32 Return = Reader->Buffer & ((1 << BitNumber) - 1);
    return(Return);
}
static void
DropBits(deflate_bit_reader *Reader, u32 BitNumber)
{
    Reader->Buffer >>= BitNumber;
    Reader->StoredBits -= BitNumber;
}
```

## PNG of Unknown Size
There are two changes necessary to handle arbitrary sizes. First, we need to undo the filter on each individual row. Secondly, we need to allocate enough memory for the entire decoding process.

### Row Filter
The [Filter](https://www.w3.org/TR/2023/CR-png-3-20230921/#9Filters) operations themselves are mostly straight forward. The complication arises from how we read data from the Deflate decoding buffer. For example, if we limit the buffer size to 32768 bytes, then we need to treat it as a ring buffer like this: `DeflateBuffer[ReadPoint & 0x7fff]`. However, if the buffer is enough to contain the entire decoded data, then we can work with simple memory pointers. Until the memory usage becomes a problem, I'll go with that method. Here's the example with only the averaging filter:
```cpp
static void
UndoFilter(u8 *At, u8 *To, u8 *RowEnd, u8 *LastRow, u32 BytesPerPixel)
{
    switch(*(At++))
    {
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
    }
}
```

### Buffer Size
In the decoding process there are various steps that buffers with data.

We need to fill `2` or `3` arrays with code lengths. For the dynamic table, an array with `19`, which can be overwritten by the time the other two need to be filled. For both, the fixed and dynamic table, a combined array with up to `320` for the literal/length and distance codes.

Each length array passes through the same function to construct a Huffman Table, with the longest possible array being `288` entries long. To create the table, first we create a second array of those `288` codes sorted by length. This creates three tables. The first with `7` bit codes which results in `128` entries. The other two with `15` bit codes resulting in `32768` entries each. The first table isn't needed anymore by the point the array of `320` length codes is filled, allowing us to overwrite it with the other tables.

The bit stream decoder decodes a number of bytes equal to the bytes contained in the image data, plus a filter type byte for every scan line. (Interlaced images have more scan lines than the image height.) Each scan line could be processed the moment it's fully decoded, however, the decoder can reference a number of already decoded bytes set in the `CompressionWindowSize` parameter. That means for the largest window size, we need to keep at least the last `32767` decoded bytes in a buffer. Until the memory size becomes a problem and requires more optimization, I'll keep the entire decoded data in buffer.

And finally, we need a buffer to store the image data, which we want to hand to the general image decoder. This leaves us with the following minimum buffer sizes in the worst case scenarios:
- An image buffer of `Height * Width` pixels
- A Deflate decoding buffer of the size of the image buffer plus a number of bytes equal to the number of lines. 
- A code length buffer with `320` entries
- A sorting buffer with `288` entries
- A Huffman Code table with `32768` for literals and lengths
- A Huffman Code table with `32768` for distances

In the code it looks like this:
```cpp
struct png_decoding_buffers
{
    u8 Lengths[320];
    u16 SortingBuffer[288];
    deflate_code LiteralDictionary[1 << 15];
    deflate_code DistanceDictionary[1 << 15];
};

u64 BytesPerRow        = ((u64)Processor.BitsPerPixel * (u64)Processor.Width + 7) / 8;
u64 ImageBufferSize    = (u64)Processor.Height * BytesPerRow;
u64 DeflateBufferSize  = ImageBufferSize + Processor.Height;
u64 CombinedBufferSize = ImageBufferSize + sizeof(png_decoding_buffers) + DeflateBufferSize;
```

## Interlaced PNG
The first problem to solve with an interlaced image is determining the buffer size. I generate an image size 72 by 72 with 1 bit colors. An interlaced image consists of 7 reduced images. The first in this case would be 9 by 9 pixels big. As each pixel is 1 bit, the reduced image 9 rows of 2 bytes. And each row comes with an additional byte containing the filter mode. The 7 reduced images come in the formats `9x9`, `9x9`, `18x9`, `18x18`, `36x18`, `36x36` and `72x36`. The sizes in memory for each are `27`, `27`, `36`, `72`, `108`, `216` and `360`. That makes a decoding buffer size of `846` and an image buffer size of `648`. If it wasn't interlaced, the decoding buffer would only need `720`bytes. That's because in addition to the `72` lines with a filter byte each, the reduced images have `2*height - height/8` lines with individual filter bytes. But that's only `135` of the `198` additional bytes. The remaining `63` bytes come from the padding added to the reduced images. 

Here we have three options. We can add more complexity into the buffer size allocation and the loop that handles the filters per scan line. We add complexity to the decoder to use a ring buffer and handle the filters as soon as a scan line is decoded. Or we integrate the filter code into the Deflate decoder and take every byte the decoder gives us, apply a filter and immediately place it into the image buffer, before the next byte is decoded.

For now, I'll stick with the same decoding structure as before and add two times the scanline count to the decoding buffer. This covers the worst case, where every scanline gains a filter byte and a padding byte. To undo the filter on each scanline, we need to calculate the length of each scanline. The length of the scanline is the width of the image, minus the offset of the reduced image pixel, divided by the offset between the pixels in the reduced image, rounded up, multiplied by the number of bits per pixel, divided 8 rounded up and finally plus 1 byte for the filter type. 
```cpp
RowIncrement = ceil(ceil((Width - ReducedImageOffset) / PixelOffset) * BitsPerPixel / 8) + 1
```
This equation assumes that all variables are floating point numbers. In a loop with only integers, it would look like this:
```cpp
static const u8 INTERLACE_X_INCREMENT[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const u8 INTERLACE_Y_INCREMENT[7] = { 8, 8, 8, 4, 4, 2, 2 };
static const u8 INTERLACE_X_OFFSET[7]    = { 0, 4, 0, 2, 0, 1, 0 };
static const u8 INTERLACE_Y_OFFSET[7]    = { 0, 0, 4, 0, 2, 0, 1 };

for(u8 Cycle = 0; Cycle < 7; Cycle++)
{
    u32 oX = INTERLACE_X_OFFSET[Cycle];
    u32 iX = INTERLACE_X_INCREMENT[Cycle];
    u32 oY = INTERLACE_Y_OFFSET[Cycle];
    u32 iY = INTERLACE_Y_INCREMENT[Cycle];
    
    u64 RowIncrement = 1 + ((Width + iX - 1 - oX) / iX * BitsPerPixel + 7) / 8;
    
    for(u32 Y = oY; Y < Height; Y += iY)
    {
        u8 *RowEnd = At + RowIncrement;
        UndoFilter(At, Row, RowEnd, LastRow, BytesPerPixel);
        
        for(u32 X = oX; X < Width;)
        {
            // Transfer the pixel data from the row to the image buffer.
        }
        
        At = RowEnd;
        u8* Temp = LastRow;
        LastRow = Row;
        Row = Temp;
    }
}
```
The loop to transfer pixel data works in two different ways, depending on if the pixels are byte aligned or not. Here's the byte aligned version first:
```cpp
u8* From = Row;
for(u32 X = oX; X < Width; X += iX)
{
    for(u32 P = 0; P < BytesPerPixel; P++)
    {
        ImageBuffer[P + X * BytesPerPixel + Y * BytesPerRow] = *(From++);
    }
}
```
When there are multiple pixels per byte, then need to bit shift each individual pixel value to or it into the image data:
```cpp
u8* From = Row;
for(u32 X = oX; X < Width;)
{
    u8 Byte = *(From++);
    for(u32 P = 0; P < PixelsPerByte; P++)
    {
        u8 Bits = (Byte >> (PixelsPerByte - 1)) << (8 - BitsPerPixel * ((X % PixelsPerByte) + 1));
        Target[X / PixelsPerByte + Y * BytesPerRow] |= Bits;
        Byte <<= BitsPerPixel;
        X += iX;
    }
}
```

## Indexed Color Image
If the color type is set to 3, then the pixel values in the image data are indexes to a color table. Our general image decoder is already set up to handle RGB color tables as the one contained in the `PLTE` chunk. However, if additionally a `tRNS` chunk is present, then the color table entries are in two separate places in memory. To further complicate the matter, the tables for the colors and the transparency don't even need to be the same length. In that case I decided to generate a new pallet table and hand that one to the general decoder instead:
```cpp
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
```

## Transparency Color
If a `tRNS` chunk is present without the color type being set to indexed, then it specifies a specific color that's the only color to be transparent. However, as far as I know, GIMP offers no option to export images in that format. Luckily, the PNG documentation provides a [Link](http://www.schaik.com/pngsuite/) to a collection of test images. To apply the transparency, we want to first convert the image data to a buffer with alpha channel and then loop over that buffer and zero out the alpha bits, when the color matches the transparency color.

## 16 Bit per Channel Image
The final specific PNG file we want to test is one with 16 bits per color channel to ensure that the right byte order. In OpenGL, we want to call `glPixelStorei(GL_UNPACK_SWAP_BYTES, true);` before we call `glTexImage2D`, when loading big endian data like PNG file format uses.

After the generated 16 bit per channel images are decoded successfully, I also test various `.png` files from different sources. They all display correctly with a few exceptions, where even though the suffix is `.png`, the contained image is a `JFIF` file.