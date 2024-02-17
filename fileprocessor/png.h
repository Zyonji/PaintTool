#define PNG_SIGNATURE 0xa1a0a0d474e5089// EIGHTCC("\211PNG\r\n\032\n")

#define PNG_COLOR_TYPE_GRAY       0
#define PNG_COLOR_TYPE_PALETTE    3
#define PNG_COLOR_TYPE_RGB        2
#define PNG_COLOR_TYPE_RGB_ALPHA  6
#define PNG_COLOR_TYPE_GRAY_ALPHA 4

#define PNG_IHDR 'RDHI'
#define PNG_PLTE 'ETLP'
#define PNG_IDAT 'TADI'
#define PNG_IEND 'DNEI'

#define PNG_tRNS 'SNRt'

#define PNG_cHRM 'MRHc'
#define PNG_gAMA 'AMAg'
#define PNG_iCCP 'PCCi'
#define PNG_sBIT 'TIBs'
#define PNG_sRGB 'BGRs'
#define PNG_cICP 'PCIc'
#define PNG_mDCv 'vCDm'
#define PNG_cLLi 'iLLc'

#define PNG_acTL 'LTca'
#define PNG_fcTL 'LTcf'
#define PNG_fdAT 'TAdf'

#define PNG_iTXt 'tXTi'
#define PNG_tEXt 'tXEt'
#define PNG_zTXt 'tXTz'

#define PNG_bKGD 'DGKb'
#define PNG_hIST 'TSIh'
#define PNG_pHYs 'sYHp'
#define PNG_sPLT 'TLPs'
#define PNG_eXIf 'fIXe'
#define PNG_tIME 'EMIt'

#pragma pack(push, 1)

struct png_chunk
{
    u32 Length;
    union
    {
        u32 TypeU32;
        char Type[4];
    };
    u8 Data[4];
    u8 OffsetBase[1];
};

struct png_file_header
{
    u64 Signature;
    union
    {
        png_chunk Chunk;
        struct
        {
            u32 Length;
            union
            {
                u32 TypeU32;
                char Type[4];
            };
            u32 Width;
            u32 Height;
            u8 BitDepth;
            u8 ColorType;
            u8 Compression;
            u8 Filter;
            u8 Interlace;
        };
    };
    u32 CRC;
};

#pragma pack(pop)

struct png_bit_reader
{
    u32 Buffer;
    u32 StoredBits;
    u8 *NextByte;
    u8 *SegmentEnd;
    u8 *FileEnd;
};

struct deflate_code
{
    u16 Value;
    u8 Length;
};

struct png_decoding_buffers
{
    u8 Lengths[320];
    u16 SortingBuffer[288];
    deflate_code LiteralDictionary[1 << 15];
    deflate_code DistanceDictionary[1 << 15];
    u8 DeflateBuffer[1];
};

#define DEFLATE_MAX_LENGTH (1 << 4)

static const u16 DEFLATE_ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static const u8  DEFLATE_LENGTH_BITS[32] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0, 0};
static const u16 DEFLATE_LENGTH_ADD[32] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0, 0};
static const u8  DEFLATE_DISTANCE_BITS[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0};
static const u16 DEFLATE_DISTANCE_ADD[32] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0};

static const u8 INTERLACE_X_INCREMENT[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const u8 INTERLACE_Y_INCREMENT[7] = { 8, 8, 8, 4, 4, 2, 2 };
static const u8 INTERLACE_X_OFFSET[7]    = { 0, 4, 0, 2, 0, 1, 0 };
static const u8 INTERLACE_Y_OFFSET[7]    = { 0, 0, 4, 0, 2, 0, 1 };
