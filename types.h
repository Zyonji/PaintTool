#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

typedef int8 s8;
typedef int8 s08;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef uint8 u8;
typedef uint8 u08;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 r32;
typedef real64 r64;
typedef real32 f32;
typedef real64 f64;

typedef uintptr_t umm;
typedef intptr_t smm;

#define S32Min ((s32)0x80000000)
#define S32Max ((s32)0x7fffffff)
#define U8Max  ((u8)-1)
#define U16Max ((u16)-1)
#define U32Max ((u32)-1)
#define U64Max ((u64)-1)
#define F32Max FLT_MAX
#define F32Min -FLT_MAX

#define Pi32 3.14159265359f

#define U32FromPointer(Pointer) ((u32)(memory_index)(Pointer))
#define PointerFromU32(type, Value) (type *)((memory_index)Value)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define OffsetOf(type, Member) (umm)&(((type *)0)->Member)

#define TWOCC(String) (*(u16 *)(String))
#define FOURCC(String) (*(u32 *)(String))
#define EIGHTCC(String) (*(u64 *)(String))

inline s32 Absolute(s32 Number)
{
    return((Number<0)?(-Number):(Number));
}

inline u64 SwapEndian(u64 Value)
{
    return( (Value >> 56) |
           ((Value >> 40) & 0xff00) |
           ((Value >> 24) & 0xff0000) |
           ((Value >>  8) & 0xff000000) |
           ((Value & 0xff000000) <<  8) |
           ((Value & 0xff0000)   << 24) |
           ((Value & 0xff00)     << 40) |
           ((Value << 56)));
}

inline u32 SwapEndian(u32 Value)
{
    return((Value >> 24) | ((Value >> 8) & 0xff00) | ((Value & 0xff00) << 8) | ((Value << 24)));
}

inline u16 SwapEndian(u16 Value)
{
    return((Value >> 8) | (Value << 8));
}

union v2
{
    struct
    {
        r32 x, y;
    };
    struct
    {
        r32 u, v;
    };
    struct
    {
        r32 Width, Height;
    };
    r32 E[2];
};

union v2s
{
    struct
    {
        s32 x, y;
    };
    struct
    {
        s32 Width, Height;
    };
    s32 E[2];
};

union v2u
{
    struct
    {
        u32 x, y;
    };
    struct
    {
        u32 Width, Height;
    };
    u32 E[2];
};

union v3
{
    struct
    {
        r32 x, y, z;
    };
    struct
    {
        r32 r, g, b;
    };
    struct
    {
        v2 xy;
        r32 Ignored0_;
    };
    struct
    {
        r32 Ignored1_;
        v2 yz;
    };
    r32 E[3];
};

union v3u
{
    struct
    {
        u32 x, y, z;
    };
    struct
    {
        u32 r, g, b;
    };
    struct
    {
        v2u xy;
        u32 Ignored0_;
    };
    struct
    {
        u32 Ignored1_;
        v2u yz;
    };
    u32 E[3];
};

union v4
{
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                r32 x, y, z;
            };
        };
        
        r32 w;
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                r32 r, g, b;
            };
        };
        
        r32 a;
    };
    struct
    {
        v2 xy;
        r32 Ignored0_;
        r32 Ignored1_;
    };
    struct
    {
        r32 Ignored2_;
        v2 yz;
        r32 Ignored3_;
    };
    struct
    {
        r32 Ignored4_;
        r32 Ignored5_;
        v2 zw;
    };
    struct
    {
        r32 Right;
        r32 Left;
        r32 Top;
        r32 Bottom;
    };
    r32 E[4];
};

union v4u
{
    struct
    {
        union
        {
            v3u xyz;
            struct
            {
                u32 x, y, z;
            };
        };
        
        u32 w;
    };
    struct
    {
        union
        {
            v3u rgb;
            struct
            {
                u32 r, g, b;
            };
        };
        
        u32 a;
    };
    struct
    {
        v2u xy;
        u32 Ignored0_;
        u32 Ignored1_;
    };
    struct
    {
        u32 Ignored2_;
        v2u yz;
        u32 Ignored3_;
    };
    struct
    {
        u32 Ignored4_;
        u32 Ignored5_;
        v2u zw;
    };
    struct
    {
        u32 Right;
        u32 Left;
        u32 Top;
        u32 Bottom;
    };
    u32 E[4];
};