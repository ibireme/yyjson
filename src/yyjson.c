/*==============================================================================
 * Created by Yaoyuan on 2019/3/9.
 * Copyright (C) 2019 Yaoyuan <ibireme@gmail.com>.
 *
 * Released under the MIT License:
 * https://github.com/ibireme/yyjson/blob/master/LICENSE
 *============================================================================*/

#include "yyjson.h"
#include <stdio.h>
#include <math.h>



/*==============================================================================
 * Compile Hint Begin
 *============================================================================*/

/* warning suppress begin */
#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-function"
#   pragma clang diagnostic ignored "-Wunused-parameter"
#   pragma clang diagnostic ignored "-Wunused-label"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-function"
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#   pragma GCC diagnostic ignored "-Wunused-label"
#elif defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable:4100) /* unreferenced formal parameter */
#   pragma warning(disable:4102) /* unreferenced label */
#   pragma warning(disable:4127) /* conditional expression is constant */
#endif



/*==============================================================================
 * Flags
 *============================================================================*/

/* gcc version check */
#ifndef yyjson_gcc_available
#   define yyjson_gcc_available(major, minor, patch) \
        ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= \
        (major * 10000 + minor * 100 + patch))
#endif

/* real gcc check */
#ifndef yyjson_is_real_gcc
#   if !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__ICC) && \
        defined(__GNUC__) && defined(__GNUC_MINOR__)
#       define yyjson_is_real_gcc 1
#   endif
#endif

/* msvc intrinsic */
#if _MSC_VER >= 1400
#   include <intrin.h>
#   if defined(_M_AMD64) || defined(_M_ARM64)
#       define MSC_HAS_BIT_SCAN_64 1
#       pragma intrinsic(_BitScanForward64)
#       pragma intrinsic(_BitScanReverse64)
#   endif
#   if defined(_M_AMD64) || defined(_M_ARM64) || \
        defined(_M_IX86) || defined(_M_ARM)
#       define MSC_HAS_BIT_SCAN 1
#       pragma intrinsic(_BitScanForward)
#       pragma intrinsic(_BitScanReverse)
#   endif
#   if defined(_M_AMD64)
#       define MSC_HAS_UMUL128 1
#       pragma intrinsic(_umul128)
#   endif
#endif

/* gcc builtin */
#if yyjson_has_builtin(__builtin_clzll) || yyjson_gcc_available(3, 4, 0)
#   define GCC_HAS_CLZLL 1
#endif

#if yyjson_has_builtin(__builtin_ctzll) || yyjson_gcc_available(3, 4, 0)
#   define GCC_HAS_CTZLL 1
#endif

/* int128 type */
#ifndef YYJSON_HAS_INT128
#   if (__SIZEOF_INT128__ == 16) && \
       (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER))
#       define YYJSON_HAS_INT128 1
#   else
#       define YYJSON_HAS_INT128 0
#   endif
#endif

/* IEEE 754 floating-point binary representation */
#ifndef YYJSON_HAS_IEEE_754
#   if __STDC_IEC_559__
#       define YYJSON_HAS_IEEE_754 1
#   elif (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && \
         (DBL_MIN_EXP == -1021) && (DBL_MAX_EXP == 1024) && \
         (DBL_MIN_10_EXP == -307) && (DBL_MAX_10_EXP == 308)
#       define YYJSON_HAS_IEEE_754 1
#   else
#       define YYJSON_HAS_IEEE_754 0
#       if __FAST_MATH__ || __USE_FAST_MATH__
#           warning "-ffast-math" may break the nan/inf check
#       endif
#   endif
#endif

/*
 On the x86 architecture, some compilers may use x87 FPU instructions for
 floating-point arithmetic. The x87 FPU loads all floating point number as
 80-bit double-extended precision internally, then rounds the result to original
 precision, which may produce inaccurate results. For a more detailed
 explanation, see the paper: https://arxiv.org/abs/cs/0701192
 
 Here are some examples of double precision calculation error:
 
     2877.0 / 1e6 == 0.002877, but x87 returns 0.0028770000000000002
     43683.0 * 1e21 == 4.3683e25, but x87 returns 4.3683000000000004e25
 
 Here are some examples of compiler flags to generate x87 instructions on x86:
 
     clang -m32 -mno-sse
     gcc/icc -m32 -mfpmath=387
     msvc /arch:SSE or /arch:IA32
 
 If we are sure that there's no similar error described above, we can define the 
 YYJSON_DOUBLE_MATH_CORRECT as 1 to enable the fast path calculation. This is
 not an accurate detection, it's just try to avoid the error at compiler time. 
 An accurate detection can be done at runtime:
 
     bool is_double_math_correct(void) {
         volatile double r = 43683.0;
         r *= 1e21;
         return r == 4.3683e25;
     }
 
 */
#ifndef YYJSON_DOUBLE_MATH_CORRECT
#   if !defined(FLT_EVAL_METHOD) && defined(__FLT_EVAL_METHOD__)
#       define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#   endif
#   if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0 && FLT_EVAL_METHOD != 1
#       define YYJSON_DOUBLE_MATH_CORRECT 0
#   elif defined(i386) || defined(__i386) || defined(__i386__) || \
        defined(_X86_) || defined(__X86__) || defined(_M_IX86) || \
        defined(__I86__) || defined(__IA32__) || defined(__THW_INTEL)
#       if (defined(_MSC_VER) && _M_IX86_FP == 2) || __SSE2_MATH__
#           define YYJSON_DOUBLE_MATH_CORRECT 1
#       else
#           define YYJSON_DOUBLE_MATH_CORRECT 0
#       endif
#   elif defined(__x86_64) || defined(__x86_64__) || \
        defined(__amd64) || defined(__amd64__) || \
        defined(_M_AMD64) || defined(_M_X64) || \
        defined(__ia64) || defined(_IA64) || defined(__IA64__) ||  \
        defined(__ia64__) || defined(_M_IA64) || defined(__itanium__) || \
        defined(__arm64) || defined(__arm64__) || \
        defined(__aarch64__) || defined(_M_ARM64) || \
        defined(__arm) || defined(__arm__) || defined(_ARM_) || \
        defined(_ARM) || defined(_M_ARM) || defined(__TARGET_ARCH_ARM) || \
        defined(mips) || defined(__mips) || defined(__mips__) || \
        defined(MIPS) || defined(_MIPS_) || defined(__MIPS__) || \
        defined(_ARCH_PPC64) || defined(__PPC64__) || \
        defined(__ppc64__) || defined(__powerpc64__) || \
        defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || \
        defined(__ppc__) || defined(__ppc) || defined(__PPC__) || \
        defined(__sparcv9) || defined(__sparc_v9__) || \
        defined(__sparc) || defined(__sparc__) || defined(__sparc64__) || \
        defined(__alpha) || defined(__alpha__) || defined(_M_ALPHA) || \
        defined(__or1k__) || defined(__OR1K__) || defined(OR1K) || \
        defined(__hppa) || defined(__hppa__) || defined(__HPPA__) || \
        defined(__riscv) || defined(__riscv__) || \
        defined(__s390__) || defined(__avr32__) || defined(__SH4__) || \
        defined(__e2k__) || defined(__arc__) || defined(__EMSCRIPTEN__)
#       define YYJSON_DOUBLE_MATH_CORRECT 1
#   else
#       define YYJSON_DOUBLE_MATH_CORRECT 0 /* unknown */
#   endif
#endif

/* endian */
#if yyjson_has_include(<sys/types.h>)
#    include <sys/types.h>
#endif

#if yyjson_has_include(<endian.h>)
#    include <endian.h>
#elif yyjson_has_include(<machine/endian.h>)
#    include <machine/endian.h>
#elif yyjson_has_include(<sys/endian.h>)
#    include <sys/endian.h>
#endif

#define YYJSON_BIG_ENDIAN       4321
#define YYJSON_LITTLE_ENDIAN    1234

#if __BYTE_ORDER__
#   if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#       define YYJSON_ENDIAN YYJSON_BIG_ENDIAN
#   elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#       define YYJSON_ENDIAN YYJSON_LITTLE_ENDIAN
#   endif

#elif __BYTE_ORDER
#   if __BYTE_ORDER == __BIG_ENDIAN
#       define YYJSON_ENDIAN YYJSON_BIG_ENDIAN
#   elif __BYTE_ORDER == __LITTLE_ENDIAN
#       define YYJSON_ENDIAN YYJSON_LITTLE_ENDIAN
#   endif

#elif BYTE_ORDER
#   if BYTE_ORDER == BIG_ENDIAN
#       define YYJSON_ENDIAN YYJSON_BIG_ENDIAN
#   elif BYTE_ORDER == LITTLE_ENDIAN
#       define YYJSON_ENDIAN YYJSON_LITTLE_ENDIAN
#   endif

#elif (__LITTLE_ENDIAN__ == 1) || \
    defined(__i386) || defined(__i386__) || \
    defined(_X86_) || defined(__X86__) || \
    defined(_M_IX86) || defined(__THW_INTEL__) || \
    defined(__x86_64) || defined(__x86_64__) || \
    defined(__amd64) || defined(__amd64__) || \
    defined(_M_AMD64) || defined(_M_X64) || \
    defined(__ia64) || defined(_IA64) || defined(__IA64__) ||  \
    defined(__ia64__) || defined(_M_IA64) || defined(__itanium__) || \
    defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
    defined(__alpha) || defined(__alpha__) || defined(_M_ALPHA) || \
    defined(__riscv) || defined(__riscv__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
#   define YYJSON_ENDIAN YYJSON_LITTLE_ENDIAN

#elif (__BIG_ENDIAN__ == 1) || \
    defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
    defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__) || \
    defined(_ARCH_PPC) || defined(_ARCH_PPC64) || \
    defined(__ppc) || defined(__ppc__) || \
    defined(__sparc) || defined(__sparc__) || defined(__sparc64__) || \
    defined(__or1k__) || defined(__OR1K__)
#   define YYJSON_ENDIAN YYJSON_BIG_ENDIAN

#else
#   undef YYJSON_ENDIAN /* unknown endian */
#endif

/* An estimated initial ratio of the pretty JSON (data_size / value_count). */
/* This value is used to avoid frequent memory allocation calls for reader. */
#ifndef YYJSON_READER_ESTIMATED_PRETTY_RATIO
#   define YYJSON_READER_ESTIMATED_PRETTY_RATIO 16
#endif

/* An estimated initial ratio of the minify JSON (data_size / value_count). */
/* This value is used to avoid frequent memory allocation calls for reader. */
#ifndef YYJSON_READER_ESTIMATED_MINIFY_RATIO
#   define YYJSON_READER_ESTIMATED_MINIFY_RATIO 6
#endif

/* An estimated initial ratio of the pretty JSON (data_size / value_count). */
/* This value is used to avoid frequent memory allocation calls for writer. */
#ifndef YYJSON_WRITER_ESTIMATED_PRETTY_RATIO
#   define YYJSON_WRITER_ESTIMATED_PRETTY_RATIO 32
#endif

/* An estimated initial ratio of the minify JSON (data_size / value_count). */
/* This value is used to avoid frequent memory allocation calls for writer. */
#ifndef YYJSON_WRITER_ESTIMATED_MINIFY_RATIO
#   define YYJSON_WRITER_ESTIMATED_MINIFY_RATIO 18
#endif



/*==============================================================================
 * Macros
 *============================================================================*/

/* Macros used for loop unrolling and other purpose. */
#define repeat2(x)  { x x }
#define repeat3(x)  { x x x }
#define repeat4(x)  { x x x x }
#define repeat8(x)  { x x x x x x x x }
#define repeat16(x) { x x x x x x x x x x x x x x x x }

#define repeat4_incr(x)  { x(0) x(1) x(2) x(3) }

#define repeat8_incr(x)  { x(0) x(1) x(2) x(3) x(4) x(5) x(6) x(7) }

#define repeat16_incr(x) { x(0) x(1) x(2) x(3) x(4) x(5) x(6) x(7) \
                           x(8) x(9) x(10) x(11) x(12) x(13) x(14) x(15) }

#define repeat_in_1_18(x) { x(1) x(2) x(3) x(4) x(5) x(6) x(7) \
                            x(8) x(9) x(10) x(11) x(12) x(13) x(14) x(15) \
                            x(16) x(17) x(18) }

/* Macros used to provide branch prediction information for compiler. */
#undef  likely
#define likely(x)       yyjson_likely(x)
#undef  unlikely
#define unlikely(x)     yyjson_unlikely(x)

/* Macros used to provide inline information for compiler. */
#undef  static_inline
#define static_inline   static yyjson_inline
#undef  static_noinline
#define static_noinline static yyjson_noinline

/* Macros for min and max. */
#undef  yyjson_min
#define yyjson_min(x, y) ((x) < (y) ? (x) : (y))
#undef  yyjson_max
#define yyjson_max(x, y) ((x) > (y) ? (x) : (y))

/* Used to write u64 literal for C89 which doesn't support "ULL" suffix. */
#undef  U64
#define U64(hi, lo) ((((u64)hi##UL) << 32U) + lo##UL)



/*==============================================================================
 * Integer Constants
 *============================================================================*/

/* U64 constant values */
#undef  U64_MAX
#define U64_MAX         U64(0xFFFFFFFF, 0xFFFFFFFF)
#undef  I64_MAX
#define I64_MAX         U64(0x7FFFFFFF, 0xFFFFFFFF)
#undef  USIZE_MAX
#define USIZE_MAX       ((usize)(~(usize)0))

/* Maximum number of digits for reading u64 safety. */
#undef  U64_SAFE_DIG
#define U64_SAFE_DIG    19

/* Padding size for JSON reader input. */
#undef  PADDING_SIZE
#define PADDING_SIZE    4



/*==============================================================================
 * IEEE-754 Double Number Constants
 *============================================================================*/

/* Inf raw value (positive) */
#define F64_RAW_INF U64(0x7FF00000, 0x00000000)

/* NaN raw value (positive, without payload) */
#define F64_RAW_NAN U64(0x7FF80000, 0x00000000)

/* double number bits */
#define F64_BITS 64

/* double number exponent part bits */
#define F64_EXP_BITS 11

/* double number significand part bits */
#define F64_SIG_BITS 52

/* double number significand part bits (with 1 hidden bit) */
#define F64_SIG_FULL_BITS 53

/* double number significand bit mask */
#define F64_SIG_MASK U64(0x000FFFFF, 0xFFFFFFFF)

/* double number exponent bit mask */
#define F64_EXP_MASK U64(0x7FF00000, 0x00000000)

/* double number exponent bias */
#define F64_EXP_BIAS 1023

/* double number significant digits count in decimal */
#define F64_DEC_DIG 17

/* max significant digits count in decimal when reading double number */
#define F64_MAX_DEC_DIG 768

/* maximum decimal power of normal number (1.7976931348623157e308) */
#define F64_MAX_DEC_EXP 308

/* minimum decimal power of normal number (4.9406564584124654e-324) */
#define F64_MIN_DEC_EXP (-324)

/* maximum binary power of normal number */
#define F64_MAX_BIN_EXP 1024

/* minimum binary power of normal number */
#define F64_MIN_BIN_EXP (-1021)



/*==============================================================================
 * Types
 *============================================================================*/

/** Type define for primitive types. */
typedef float       f32;
typedef double      f64;
typedef int8_t      i8;
typedef uint8_t     u8;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;
typedef size_t      usize;
#if YYJSON_HAS_INT128
__extension__ typedef __int128          i128;
__extension__ typedef unsigned __int128 u128;
#endif

/** 16/32/64-bit vector */
typedef struct v16 { char c1, c2; } v16;
typedef struct v32 { char c1, c2, c3, c4; } v32;
typedef struct v64 { char c1, c2, c3, c4, c5, c6, c7, c8; } v64;

/** 16/32/64-bit vector union, used for unaligned memory access on modern CPU */
typedef union v16_uni { v16 v; u16 u; } v16_uni;
typedef union v32_uni { v32 v; u32 u; } v32_uni;
typedef union v64_uni { v64 v; u64 u; } v64_uni;

/** 64-bit floating point union, used to avoid the type-based aliasing rule */
typedef union { u64 u; f64 f; } f64_uni;



/*==============================================================================
 * Character Utils
 *============================================================================*/

static_inline void byte_move_1(void *dst, void *src) {
    memmove(dst, src, 1);
}

static_inline void byte_move_2(void *dst, void *src) {
    memmove(dst, src, 2);
}

static_inline void byte_move_4(void *dst, void *src) {
    memmove(dst, src, 4);
}

static_inline void byte_move_8(void *dst, void *src) {
    memmove(dst, src, 8);
}

static_inline void byte_move_16(void *dst, void *src) {
    memmove(dst, src, 16);
}

static_inline bool byte_match_1(void *buf, const char *pat) {
    return *(u8 *)buf == (u8)*pat;
}

static_inline bool byte_match_2(void *buf, const char *pat) {
    v16_uni u1, u2;
    u1.v = *(v16 *)pat;
    u2.v = *(v16 *)buf;
    return u1.u == u2.u;
}

static_inline bool byte_match_4(void *buf, const char *pat) {
    v32_uni u1, u2;
    u1.v = *(v32 *)pat;
    u2.v = *(v32 *)buf;
    return u1.u == u2.u;
}

static_inline u16 byte_load_2(void *src) {
    v16_uni uni;
    uni.v = *(v16 *)src;
    return uni.u;
}

static_inline u32 byte_load_4(void *src) {
    v32_uni uni;
    uni.v = *(v32 *)src;
    return uni.u;
}

static_inline u64 byte_load_8(void *src) {
    v64_uni uni;
    uni.v = *(v64 *)src;
    return uni.u;
}

#if (__STDC__ >= 1 && __STDC_VERSION__ >= 199901L)

#define v16_make(c1, c2) \
    ((v16){c1, c2})

#define v32_make(c1, c2, c3, c4) \
    ((v32){c1, c2, c3, c4})

#define v64_make(c1, c2, c3, c4, c5, c6, c7, c8) \
    ((v64){c1, c2, c3, c4, c5, c6, c7, c8})

#else

static_inline v16 v16_make(char c1, char c2) {
    v16 v;
    v.c1 = c1;
    v.c2 = c2;
    return v;
}

static_inline v32 v32_make(char c1, char c2, char c3, char c4) {
    v32 v;
    v.c1 = c1;
    v.c2 = c2;
    v.c3 = c3;
    v.c4 = c4;
    return v;
}

static_inline v64 v64_make(char c1, char c2, char c3, char c4,
                           char c5, char c6, char c7, char c8) {
    v64 v;
    v.c1 = c1;
    v.c2 = c2;
    v.c3 = c3;
    v.c4 = c4;
    v.c5 = c5;
    v.c6 = c6;
    v.c7 = c7;
    v.c8 = c8;
    return v;
}

#endif



/*==============================================================================
 * Number Utils
 *============================================================================*/

/** Convert raw binary to double. */
static_inline f64 f64_from_raw(u64 u) {
    f64_uni uni;
    uni.u = u;
    return uni.f;
}

/** Convert double to raw binary. */
static_inline u64 f64_to_raw(f64 f) {
    f64_uni uni;
    uni.f = f;
    return uni.u;
}

/** Get raw 'infinity' with sign. */
static_inline u64 f64_raw_get_inf(bool sign) {
#if YYJSON_HAS_IEEE_754
    return F64_RAW_INF | ((u64)sign << 63);
#elif defined(INFINITY)
    return f64_to_raw(sign ? -INFINITY : INFINITY);
#else
    return f64_to_raw(sign ? -HUGE_VAL : HUGE_VAL);
#endif
}

/** Get raw 'nan' with sign. */
static_inline u64 f64_raw_get_nan(bool sign) {
#if YYJSON_HAS_IEEE_754
    return F64_RAW_NAN | ((u64)sign << 63);
#elif defined(NAN)
    return f64_to_raw(sign ? (f64)-NAN : (f64)NAN);
#else
    return f64_to_raw((sign ? -0.0 : 0.0) / 0.0);
#endif
}

/** Checks if the given double has finite value. */
static_inline bool f64_isfinite(f64 f) {
#if defined(isfinite)
    return !!isfinite(f);
#else
    f64 f1 = f;
    f64 f2 = f - f;
    return (f1 == f1) && (f2 == f2);
#endif
}

/** Checks if the given double is infinite. */
static_inline bool f64_isinf(f64 f) {
#if defined(isinf)
    return !!isinf(f);
#else
    f64 f1 = f;
    f64 f2 = f - f;
    return (f1 == f1) && (f2 != f2);
#endif
}

/** Checks if the given double is nan. */
static_inline bool f64_isnan(f64 f) {
#if defined(isnan)
    return !!isnan(f);
#else
    return f != f;
#endif
}



/*==============================================================================
 * Size Utils
 *============================================================================*/

/** Returns whether the size is overflow after increment. */
static_inline bool size_add_is_overflow(usize size, usize add) {
    usize val = size + add;
    return (val < size) | (val < add);
}

/** Returns whether the size is power of 2 (size should not be 0). */
static_inline bool size_is_pow2(usize size) {
    return (size & (size - 1)) == 0;
}

/** Align size upwards (may overflow). */
static_inline usize size_align_up(usize size, usize align) {
    if (size_is_pow2(align)) {
        return (size + (align - 1)) & ~(align - 1);
    } else {
        return size + align - (size + align - 1) % align - 1;
    }
}

/** Align size downwards. */
static_inline usize size_align_down(usize size, usize align) {
    if (size_is_pow2(align)) {
        return size & ~(align - 1);
    } else {
        return size - (size % align);
    }
}

/** Align address upwards (may overflow). */
static_inline void *mem_align_up(void *mem, usize align) {
    return (void *)size_align_up((usize)mem, align);
}

/** Align address downwards. */
static_inline void *mem_align_down(void *mem, usize align) {
    return (void *)size_align_down((usize)mem, align);
}



/*==============================================================================
 * Bits Utils
 *============================================================================*/

/** Returns the number of leading 0-bits in value (input should not be 0). */
static_inline
u32 u64_lz_bits(u64 v) {
#if GCC_HAS_CLZLL
    return (u32)__builtin_clzll(v);
#elif MSC_HAS_BIT_SCAN_64
    unsigned long r;
    _BitScanReverse64(&r, v);
    return (u32)63 - (u32)r;
#elif MSC_HAS_BIT_SCAN
    unsigned long hi, lo;
    bool hi_set = _BitScanReverse(&hi, (u32)(v >> 32)) != 0;
    _BitScanReverse(&lo, (u32)v);
    hi |= 32;
    return (u32)63 - (u32)(hi_set ? hi : lo);
#else
    /* branchless, use de Bruijn sequences */
    const u8 table[64] = {
        63, 16, 62,  7, 15, 36, 61,  3,  6, 14, 22, 26, 35, 47, 60,  2,
         9,  5, 28, 11, 13, 21, 42, 19, 25, 31, 34, 40, 46, 52, 59,  1,
        17,  8, 37,  4, 23, 27, 48, 10, 29, 12, 43, 20, 32, 41, 53, 18,
        38, 24, 49, 30, 44, 33, 54, 39, 50, 45, 55, 51, 56, 57, 58,  0
    };
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return table[(v * U64(0x03F79D71, 0xB4CB0A89)) >> 58];
#endif
}

/** Returns the number of trailing 0-bits in value (input should not be 0). */
static_inline u32 u64_tz_bits(u64 v) {
#if GCC_HAS_CTZLL
    return (u32)__builtin_ctzll(v);
#elif MSC_HAS_BIT_SCAN_64
    unsigned long r;
    _BitScanForward64(&r, v);
    return (u32)r;
#elif MSC_HAS_BIT_SCAN
    unsigned long lo, hi;
    bool lo_set = _BitScanForward(&lo, (u32)(v)) != 0;
    _BitScanForward(&hi, (u32)(v >> 32));
    hi += 32;
    return lo_set ? lo : hi;
#else
    /* branchless, use de Bruijn sequences */
    const u8 table[64] = {
         0,  1,  2, 53,  3,  7, 54, 27,  4, 38, 41,  8, 34, 55, 48, 28,
        62,  5, 39, 46, 44, 42, 22,  9, 24, 35, 59, 56, 49, 18, 29, 11,
        63, 52,  6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
        51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
    };
    return table[((v & (~v + 1)) * U64(0x022FDD63, 0xCC95386D)) >> 58];
#endif
}

/** Returns the number of significant bits in value (should not be 0). */
static_inline u32 u64_sig_bits(u64 v) {
    return (u32)64 - u64_lz_bits(v) - u64_tz_bits(v);
}



/*==============================================================================
 * 128-bit Integer Utils
 *============================================================================*/

/** Multiplies two 64-bit unsigned integers (a * b),
    returns the 128-bit result as 'hi' and 'lo'. */
static_inline void u128_mul(u64 a, u64 b, u64 *hi, u64 *lo) {
#if YYJSON_HAS_INT128
    u128 m = (u128)a * b;
    *hi = (u64)(m >> 64);
    *lo = (u64)(m);
#elif MSC_HAS_UMUL128
    *lo = _umul128(a, b, hi);
#else
    u32 a0 = (u32)(a), a1 = (u32)(a >> 32);
    u32 b0 = (u32)(b), b1 = (u32)(b >> 32);
    u64 p00 = (u64)a0 * b0, p01 = (u64)a0 * b1;
    u64 p10 = (u64)a1 * b0, p11 = (u64)a1 * b1;
    u64 m0 = p01 + (p00 >> 32);
    u32 m00 = (u32)(m0), m01 = (u32)(m0 >> 32);
    u64 m1 = p10 + m00;
    u32 m10 = (u32)(m1), m11 = (u32)(m1 >> 32);
    *hi = p11 + m01 + m11;
    *lo = ((u64)m10 << 32) | (u32)p00;
#endif
}

/** Multiplies two 64-bit unsigned integers and add a value (a * b + c),
    returns the 128-bit result as 'hi' and 'lo'. */
static_inline void u128_mul_add(u64 a, u64 b, u64 c, u64 *hi, u64 *lo) {
#if YYJSON_HAS_INT128
    u128 m = (u128)a * b + c;
    *hi = (u64)(m >> 64);
    *lo = (u64)(m);
#else
    u64 h, l, t;
    u128_mul(a, b, &h, &l);
    t = l + c;
    h += ((t < l) | (t < c));
    *hi = h;
    *lo = t;
#endif
}



/*==============================================================================
 * Default Memory Allocator
 *
 * This is a simple wrapper of libc.
 *============================================================================*/

static void *default_malloc(void *ctx, usize size) {
    return malloc(size);
}

static void *default_realloc(void *ctx, void *ptr, usize size) {
    return realloc(ptr, size);
}

static void default_free(void *ctx, void *ptr) {
    free(ptr);
}

const yyjson_alc YYJSON_DEFAULT_ALC = {
    default_malloc,
    default_realloc,
    default_free,
    NULL
};



/*==============================================================================
 * Pool Memory Allocator
 *
 * This is a simple memory allocator that uses linked list memory chunk.
 *============================================================================*/

/** chunk header */
typedef struct pool_chunk {
    usize size; /* chunk memory size (include chunk header) */
    struct pool_chunk *next;
} pool_chunk;

/** ctx header */
typedef struct pool_ctx {
    usize size; /* total memory size (include ctx header) */
    pool_chunk *free_list;
} pool_ctx;

static void *pool_malloc(void *ctx_ptr, usize size) {
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *next, *prev = NULL, *cur = ctx->free_list;
    
    if (unlikely(size == 0 || size >= ctx->size)) return NULL;
    size = size_align_up(size, sizeof(pool_chunk)) + sizeof(pool_chunk);
    
    while (cur) {
        if (cur->size < size) {
            /* not enough space, try next chunk */
            prev = cur;
            cur = cur->next;
            continue;
        }
        if (cur->size >= size + sizeof(pool_chunk) * 2) {
            /* too much space, split this chunk */
            next = (pool_chunk *)((u8 *)cur + size);
            next->size = cur->size - size;
            next->next = cur->next;
            cur->size = size;
        } else {
            /* just enough space, use whole chunk */
            next = cur->next;
        }
        if (prev) prev->next = next;
        else ctx->free_list = next;
        return (void *)(cur + 1);
    }
    return NULL;
}

static void pool_free(void *ctx_ptr, void *ptr) {
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *cur = ((pool_chunk *)ptr) - 1;
    pool_chunk *prev = NULL, *next = ctx->free_list;
    
    while (next && next < cur) {
        prev = next;
        next = next->next;
    }
    if (prev) prev->next = cur;
    else ctx->free_list = cur;
    cur->next = next;
    
    if (next && ((u8 *)cur + cur->size) == (u8 *)next) {
        /* merge cur to higher chunk */
        cur->size += next->size;
        cur->next = next->next;
    }
    if (prev && ((u8 *)prev + prev->size) == (u8 *)cur) {
        /* merge cur to lower chunk */
        prev->size += cur->size;
        prev->next = cur->next;
    }
}

static void *pool_realloc(void *ctx_ptr, void *ptr, usize size) {
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *cur = ((pool_chunk *)ptr) - 1, *prev, *next, *tmp;
    usize free_size;
    void *new_ptr;
    
    if (unlikely(size == 0 || size >= ctx->size)) return NULL;
    size = size_align_up(size, sizeof(pool_chunk)) + sizeof(pool_chunk);
    
    /* reduce size */
    if (unlikely(size <= cur->size)) {
        free_size = cur->size - size;
        if (free_size >= sizeof(pool_chunk) * 2) {
            tmp = (pool_chunk *)((u8 *)cur + cur->size - free_size);
            tmp->size = free_size;
            pool_free(ctx_ptr, (void *)(tmp + 1));
            cur->size -= free_size;
        }
        return ptr;
    }
    
    /* find next and prev chunk */
    prev = NULL;
    next = ctx->free_list;
    while (next && next < cur) {
        prev = next;
        next = next->next;
    }
    
    /* merge to higher chunk if they are contiguous */
    if ((u8 *)cur + cur->size == (u8 *)next &&
        cur->size + next->size >= size) {
        free_size = cur->size + next->size - size;
        if (free_size > sizeof(pool_chunk) * 2) {
            tmp = (pool_chunk *)((u8 *)cur + size);
            if (prev) prev->next = tmp;
            else ctx->free_list = tmp;
            tmp->next = next->next;
            tmp->size = free_size;
            cur->size = size;
        } else {
            if (prev) prev->next = next->next;
            else ctx->free_list = next->next;
            cur->size += next->size;
        }
        return ptr;
    }
    
    /* fallback to malloc and memcpy */
    new_ptr = pool_malloc(ctx_ptr, size - sizeof(pool_chunk));
    if (new_ptr) {
        memcpy(new_ptr, ptr, cur->size - sizeof(pool_chunk));
        pool_free(ctx_ptr, ptr);
    }
    return new_ptr;
}

bool yyjson_alc_pool_init(yyjson_alc *alc, void *buf, usize size) {
    pool_chunk *chunk;
    pool_ctx *ctx;
    
    if (unlikely(!alc || size < sizeof(pool_ctx) * 4)) return false;
    ctx = (pool_ctx *)mem_align_up(buf, sizeof(pool_ctx));
    if (unlikely(!ctx)) return false;
    size -= ((u8 *)ctx - (u8 *)buf);
    size = size_align_down(size, sizeof(pool_ctx));
    
    chunk = (pool_chunk *)(ctx + 1);
    chunk->size = size - sizeof(pool_ctx);
    chunk->next = NULL;
    ctx->size = size;
    ctx->free_list = chunk;
    
    alc->malloc = pool_malloc;
    alc->realloc = pool_realloc;
    alc->free = pool_free;
    alc->ctx = (void *)ctx;
    return true;
}



/*==============================================================================
 * JSON document and value
 *============================================================================*/

static_inline void unsafe_yyjson_str_pool_release(yyjson_str_pool *pool,
                                                  yyjson_alc *alc) {
    yyjson_str_chunk *chunk = pool->chunks, *next;
    while (chunk) {
        next = chunk->next;
        alc->free(alc->ctx, chunk);
        chunk = next;
    }
}

static_inline void unsafe_yyjson_val_pool_release(yyjson_val_pool *pool,
                                                       yyjson_alc *alc) {
    yyjson_val_chunk *chunk = pool->chunks, *next;
    while (chunk) {
        next = chunk->next;
        alc->free(alc->ctx, chunk);
        chunk = next;
    }
}

bool unsafe_yyjson_str_pool_grow(yyjson_str_pool *pool,
                                 yyjson_alc *alc, usize len) {
    yyjson_str_chunk *chunk;
    usize size = len + sizeof(yyjson_str_chunk);
    size = yyjson_max(pool->chunk_size, size);
    chunk = (yyjson_str_chunk *)alc->malloc(alc->ctx, size);
    if (yyjson_unlikely(!chunk)) return false;
    
    chunk->next = pool->chunks;
    pool->chunks = chunk;
    pool->cur = (char *)chunk + sizeof(yyjson_str_chunk);
    pool->end = (char *)chunk + size;
    
    size = yyjson_min(pool->chunk_size * 2, pool->chunk_size_max);
    pool->chunk_size = size;
    return true;
}

bool unsafe_yyjson_val_pool_grow(yyjson_val_pool *pool,
                                 yyjson_alc *alc, usize count) {
    yyjson_val_chunk *chunk;
    usize size;
    
    if (count >= USIZE_MAX / sizeof(yyjson_mut_val) - 16) return false;
    size = (count + 1) * sizeof(yyjson_mut_val);
    size = yyjson_max(pool->chunk_size, size);
    chunk = (yyjson_val_chunk *)alc->malloc(alc->ctx, size);
    if (yyjson_unlikely(!chunk)) return false;
    
    chunk->next = pool->chunks;
    pool->chunks = chunk;
    pool->cur = (yyjson_mut_val *)((u8 *)chunk + sizeof(yyjson_mut_val));
    pool->end = (yyjson_mut_val *)((u8 *)chunk + size);
    
    size = yyjson_min(pool->chunk_size * 2, pool->chunk_size_max);
    pool->chunk_size = size;
    return true;
}

void yyjson_mut_doc_free(yyjson_mut_doc *doc) {
    if (doc) {
        yyjson_alc alc = doc->alc;
        unsafe_yyjson_str_pool_release(&doc->str_pool, &alc);
        unsafe_yyjson_val_pool_release(&doc->val_pool, &alc);
        alc.free(alc.ctx, doc);
    }
}

yyjson_mut_doc *yyjson_mut_doc_new(yyjson_alc *alc) {
    yyjson_mut_doc *doc;
    if (!alc) alc = (yyjson_alc *)&YYJSON_DEFAULT_ALC;
    doc = (yyjson_mut_doc *)alc->malloc(alc->ctx, sizeof(yyjson_mut_doc));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(yyjson_mut_doc));
    
    doc->alc = *alc;
    doc->str_pool.chunk_size = 0x100;
    doc->str_pool.chunk_size_max = 0x10000000;
    doc->val_pool.chunk_size = 0x10 * sizeof(yyjson_mut_val);
    doc->val_pool.chunk_size_max = 0x1000000 *sizeof(yyjson_mut_val);
    return doc;
}

yyjson_api yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *i_doc,
                                               yyjson_alc *alc) {
    yyjson_mut_doc *m_doc;
    yyjson_mut_val *m_val;
    
    if (!i_doc) return NULL;
    m_doc = yyjson_mut_doc_new(alc);
    if (!m_doc) return NULL;
    m_val = yyjson_val_mut_copy(m_doc, i_doc->root);
    if (!m_val) {
        yyjson_mut_doc_free(m_doc);
        return NULL;
    }
    yyjson_mut_doc_set_root(m_doc, m_val);
    return m_doc;
}

yyjson_api yyjson_mut_val *yyjson_val_mut_copy(yyjson_mut_doc *m_doc,
                                               yyjson_val *i_vals) {
    /*
     The immutable object or array stores all sub-value in a contiguous memory,
     We copy them to another contiguous memory as mutable values,
     then reconnect the mutable values with the original relationship.
     */
    
    usize i_vals_len;
    yyjson_mut_val *m_vals, *m_val;
    yyjson_val *i_val, *i_end;
    
    if (!m_doc || !i_vals) return NULL;
    i_end = unsafe_yyjson_get_next(i_vals);
    i_vals_len = unsafe_yyjson_get_next(i_vals) - i_vals;
    m_vals = unsafe_yyjson_mut_val(m_doc, i_vals_len);
    if (!m_vals) return NULL;
    i_val = i_vals;
    m_val = m_vals;
    
    for (; i_val < i_end; i_val++, m_val++) {
        yyjson_type type = unsafe_yyjson_get_type(i_val);
        m_val->tag = i_val->tag;
        m_val->uni.u64 = i_val->uni.u64;
        if (type == YYJSON_TYPE_STR) {
            const char *str = i_val->uni.str;
            usize str_len = unsafe_yyjson_get_len(i_val);
            m_val->uni.str = unsafe_yyjson_mut_strncpy(m_doc, str, str_len);
            if (!m_val->uni.str) return NULL;
            
        } else if (type == YYJSON_TYPE_ARR) {
            usize len = unsafe_yyjson_get_len(i_val);
            if (len > 0) {
                yyjson_val *ii_val = i_val + 1, *ii_next;
                yyjson_mut_val *mm_val = m_val + 1, *mm_ctn = m_val, *mm_next;
                while (len-- > 1) {
                    ii_next = unsafe_yyjson_get_next(ii_val);
                    mm_next = mm_val + (ii_next - ii_val);
                    mm_val->next = mm_next;
                    ii_val = ii_next;
                    mm_val = mm_next;
                }
                mm_val->next = mm_ctn + 1;
                mm_ctn->uni.ptr = mm_val;
            }
        } else if (type == YYJSON_TYPE_OBJ) {
            usize len = unsafe_yyjson_get_len(i_val);
            if (len > 0) {
                yyjson_val *ii_key = i_val + 1, *ii_nextkey;
                yyjson_mut_val *mm_key = m_val + 1, *mm_ctn = m_val, *mm_nextkey;
                while (len-- > 1) {
                    ii_nextkey = unsafe_yyjson_get_next(ii_key + 1);
                    mm_nextkey = mm_key + (ii_nextkey - ii_key);
                    mm_key->next = mm_key + 1;
                    mm_key->next->next = mm_nextkey;
                    ii_key = ii_nextkey;
                    mm_key = mm_nextkey;
                }
                mm_key->next = mm_key + 1;
                mm_key->next->next = mm_ctn + 1;
                mm_ctn->uni.ptr = mm_key;
            }
        }
    }
    
    return m_vals;
}



#if !YYJSON_DISABLE_READER

/*==============================================================================
 * JSON Character Matcher
 *============================================================================*/

/** Character type */
typedef u8 char_type;

/** Whitespace character: ' ', '\\t', '\\n', '\\r'. */
static const char_type CHAR_TYPE_SPACE      = 1 << 0;

/** Number character: '-', [0-9]. */
static const char_type CHAR_TYPE_NUMBER     = 1 << 1;

/** JSON Escaped character: '"', '\', [0x00-0x1F]. */
static const char_type CHAR_TYPE_ESC_ASCII  = 1 << 2;

/** Non-ASCII character: [0x80-0xFF]. */
static const char_type CHAR_TYPE_NON_ASCII  = 1 << 3;

/** JSON container character: '{', '['. */
static const char_type CHAR_TYPE_CONTAINER  = 1 << 4;

/** Comment character: '/'. */
static const char_type CHAR_TYPE_COMMENT    = 1 << 5;

/** Line end character '\\n', '\\r', '\0'. */
static const char_type CHAR_TYPE_LINE_END   = 1 << 6;

/** Character type table (generate with misc/make_tables.c) */
static const char_type char_table[256] = {
    0x44, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x05, 0x45, 0x04, 0x04, 0x45, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x20,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

/** Match a character with specified type. */
static_inline bool char_is_type(u8 c, char_type type) {
    return (char_table[c] & type) != 0;
}

/** Match a whitespace: ' ', '\\t', '\\n', '\\r'. */
static_inline bool char_is_space(u8 c) {
    return char_is_type(c, CHAR_TYPE_SPACE);
}

/** Match a whitespace or comment: ' ', '\\t', '\\n', '\\r', '/'. */
static_inline bool char_is_space_or_comment(u8 c) {
    return char_is_type(c, CHAR_TYPE_SPACE | CHAR_TYPE_COMMENT);
}

/** Match a JSON number: '-', [0-9]. */
static_inline bool char_is_number(u8 c) {
    return char_is_type(c, CHAR_TYPE_NUMBER);
}

/** Match a JSON container: '{', '['. */
static_inline bool char_is_container(u8 c) {
    return char_is_type(c, CHAR_TYPE_CONTAINER);
}

/** Match a stop character in ASCII string: '"', '\', [0x00-0x1F], [0x80-0xFF]*/
static_inline bool char_is_ascii_stop(u8 c) {
    return char_is_type(c, CHAR_TYPE_ESC_ASCII | CHAR_TYPE_NON_ASCII);
}

/** Match a line end character: '\\n', '\\r', '\0'*/
static_inline bool char_is_line_end(u8 c) {
    return char_is_type(c, CHAR_TYPE_LINE_END);
}



/*==============================================================================
 * Digit Character Matcher
 *============================================================================*/

/** Digit type */
typedef u8 digi_type;

/** Digit: '0'. */
static const digi_type DIGI_TYPE_ZERO       = 1 << 0;

/** Digit: [1-9]. */
static const digi_type DIGI_TYPE_NONZERO    = 1 << 1;

/** Plus sign (positive): '+'. */
static const digi_type DIGI_TYPE_POS        = 1 << 2;

/** Minus sign (negative): '-'. */
static const digi_type DIGI_TYPE_NEG        = 1 << 3;

/** Decimal point: '.' */
static const digi_type DIGI_TYPE_DOT        = 1 << 4;

/** Exponent sign: 'e, 'E'. */
static const digi_type DIGI_TYPE_EXP        = 1 << 5;

/** Digit type table (generate with misc/make_tables.c) */
static const digi_type digi_table[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x10, 0x00,
    0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/** Match a character with specified type. */
static_inline bool digi_is_type(u8 d, digi_type type) {
    return (digi_table[d] & type) != 0;
}

/** Match a sign: '+', '-' */
static_inline bool digi_is_sign(u8 d) {
    return digi_is_type(d, DIGI_TYPE_POS | DIGI_TYPE_NEG);
}

/** Match a none zero digit: [1-9] */
static_inline bool digi_is_nonzero(u8 d) {
    return digi_is_type(d, DIGI_TYPE_NONZERO);
}

/** Match a digit: [0-9] */
static_inline bool digi_is_digit(u8 d) {
    return digi_is_type(d, DIGI_TYPE_ZERO | DIGI_TYPE_NONZERO);
}

/** Match an exponent sign: 'e', 'E'. */
static_inline bool digi_is_exp(u8 d) {
    return digi_is_type(d, DIGI_TYPE_EXP);
}

/** Match a floating point indicator: '.', 'e', 'E'. */
static_inline bool digi_is_fp(u8 d) {
    return digi_is_type(d, DIGI_TYPE_DOT | DIGI_TYPE_EXP);
}

/** Match a digit or floating point indicator: [0-9], '.', 'e', 'E'. */
static_inline bool digi_is_digit_or_fp(u8 d) {
    return digi_is_type(d, DIGI_TYPE_ZERO | DIGI_TYPE_NONZERO |
                           DIGI_TYPE_DOT | DIGI_TYPE_EXP);
}



/*==============================================================================
 * Hex Character Reader
 *============================================================================*/

/**
 This table is used to convert 4 hex character sequence to a number,
 A valid hex character [0-9A-Fa-f] will mapped to it's raw number [0x00, 0x0F],
 an invalid hex character will mapped to [0xF0].
 (generate with misc/make_tables.c)
 */
static const u8 hex_conv_table[256] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0
};

/**
 Scans an escaped character sequence as a UTF-16 code unit (branchless).
 e.g. "\u005C" should pass "005C" as `cur`.
 
 This requires the string has 4-byte zero padding.
 */
static_inline bool read_hex_u16(const u8 *cur, u16 *val) {
    u16 c0, c1, c2, c3, t0, t1;
    c0 = hex_conv_table[cur[0]];
    c1 = hex_conv_table[cur[1]];
    c2 = hex_conv_table[cur[2]];
    c3 = hex_conv_table[cur[3]];
    t0 = (u16)((c0 << 8) | c2);
    t1 = (u16)((c1 << 8) | c3);
    *val = (u16)((t0 << 4) | t1);
    return ((t0 | t1) & (u16)0xF0F0) == 0;
}



/*==============================================================================
 * JSON Reader Utils
 *============================================================================*/

/** Read 'true' literal, '*cur' should be 't'. */
static_inline bool read_true(u8 *cur, u8 **end, yyjson_val *val) {
    if (likely(byte_match_4(cur, "true"))) {
        val->tag = YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_TRUE;
        *end = cur + 4;
        return true;
    }
    return false;
}

/** Read 'false' literal, '*cur' should be 'f'. */
static_inline bool read_false(u8 *cur, u8 **end, yyjson_val *val) {
    if (likely(byte_match_4(cur + 1, "alse"))) {
        val->tag = YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_FALSE;
        *end = cur + 5;
        return true;
    }
    return false;
}

/** Read 'null' literal, '*cur' should be 'n'. */
static_inline bool read_null(u8 *cur, u8 **end, yyjson_val *val) {
    if (likely(byte_match_4(cur, "null"))) {
        val->tag = YYJSON_TYPE_NULL;
        *end = cur + 4;
        return true;
    }
    return false;
}

/** Read 'Inf' or 'Infinity' literal (ignoring case). */
static_inline bool read_inf(bool sign, u8 *cur, u8 **end, yyjson_val *val) {
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    if ((cur[0] == 'I' || cur[0] == 'i') &&
        (cur[1] == 'N' || cur[1] == 'n') &&
        (cur[2] == 'F' || cur[2] == 'f')) {
        if ((cur[3] == 'I' || cur[3] == 'i') &&
            (cur[4] == 'N' || cur[4] == 'n') &&
            (cur[5] == 'I' || cur[5] == 'i') &&
            (cur[6] == 'T' || cur[6] == 't') &&
            (cur[7] == 'Y' || cur[7] == 'y')) {
            *end = cur + 8;
        } else {
            *end = cur + 3;
        }
        val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL;
        val->uni.u64 = f64_raw_get_inf(sign);
        return true;
    }
#endif
    return false;
}

/** Read 'NaN' literal (ignoring case). */
static_inline bool read_nan(bool sign, u8 *cur, u8 **end, yyjson_val *val) {
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    if ((cur[0] == 'N' || cur[0] == 'n') &&
        (cur[1] == 'A' || cur[1] == 'a') &&
        (cur[2] == 'N' || cur[2] == 'n')) {
        *end = cur + 3;
        val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL;
        val->uni.u64 = f64_raw_get_nan(sign);
        return true;
    }
#endif
    return false;
}

/** Read 'Inf', 'Infinity' or 'NaN' literal (ignoring case). */
static_inline bool read_inf_or_nan(bool sign, u8 *cur, u8 **end,
                                   yyjson_val *val) {
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    if (read_inf(sign, cur, end, val)) return true;
    if (read_nan(sign, cur, end, val)) return true;
#endif
    return false;
}

/**
 Skips spaces and comments as many as possible.
 
 It will return false in these cases:
    1. No character is skipped. The 'end' pointer is set as input cursor.
    2. A multiline comment is not closed. The 'end' pointer is set as the head
       of this comment block.
 */
static_noinline bool skip_spaces_and_comments(u8 *cur, u8 **end) {
    u8 *hdr = cur;
    while (true) {
        if (byte_match_2(cur, "/*")) {
            hdr = cur;
            cur += 2;
            while (true) {
                if (byte_match_2(cur, "*/")) {
                    cur += 2;
                    break;
                }
                if (byte_match_1(cur, "\0")) {
                    *end = hdr;
                    return false;
                }
                cur++;
            }
            continue;
        }
        if (byte_match_2(cur, "//")) {
            cur += 2;
            while (!char_is_line_end(*cur)) cur++;
            continue;
        }
        if (char_is_space(*cur)) {
            cur += 1;
            while (char_is_space(*cur)) cur++;
            continue;
        }
        break;
    }
    *end = cur;
    return hdr != cur;
}



#if YYJSON_HAS_IEEE_754 && !YYJSON_DISABLE_FP_READER

/*==============================================================================
 * BigInt For Floating Point Number Reader
 *
 * The bigint algorithm is used by floating-point number parser to get correctly
 * rounded result for numbers with lots of digits. This part of code is rarely
 * used for normal JSON.
 *============================================================================*/

/** Maximum exponent of exact pow10 */
#define U64_POW10_MAX_EXP 19

/** Table: [ 10^0, ..., 10^19 ] (generate with misc/make_tables.c) */
static const u64 u64_pow10_table[U64_POW10_MAX_EXP + 1] = {
    U64(0x00000000, 0x00000001), U64(0x00000000, 0x0000000A),
    U64(0x00000000, 0x00000064), U64(0x00000000, 0x000003E8),
    U64(0x00000000, 0x00002710), U64(0x00000000, 0x000186A0),
    U64(0x00000000, 0x000F4240), U64(0x00000000, 0x00989680),
    U64(0x00000000, 0x05F5E100), U64(0x00000000, 0x3B9ACA00),
    U64(0x00000002, 0x540BE400), U64(0x00000017, 0x4876E800),
    U64(0x000000E8, 0xD4A51000), U64(0x00000918, 0x4E72A000),
    U64(0x00005AF3, 0x107A4000), U64(0x00038D7E, 0xA4C68000),
    U64(0x002386F2, 0x6FC10000), U64(0x01634578, 0x5D8A0000),
    U64(0x0DE0B6B3, 0xA7640000), U64(0x8AC72304, 0x89E80000)
};

/** Maximum numbers of chunks used by a bigint (58 is enough here). */
#define BIGINT_MAX_CHUNKS 64

/** Unsigned arbitrarily large integer */
typedef struct bigint {
    u32 used; /* used chunks count, should not be 0 */
    u64 bits[BIGINT_MAX_CHUNKS]; /* chunks */
} bigint;

/**
 Evaluate 'big += val'.
 @param big A big number (can be 0).
 @param val An unsigned integer (can be 0).
 */
static_inline void bigint_add_u64(bigint *big, u64 val) {
    u32 idx, max;
    u64 num = big->bits[0];
    u64 add = num + val;
    big->bits[0] = add;
    if (likely((add >= num) || (add >= val))) return;
    for ((void)(idx = 1), max = big->used; idx < max; idx++) {
        if (likely(big->bits[idx] != U64_MAX)) {
            big->bits[idx] += 1;
            return;
        }
        big->bits[idx] = 0;
    }
    big->bits[big->used++] = 1;
}

/**
 Evaluate 'big *= val'.
 @param big A big number (can be 0).
 @param val An unsigned integer (cannot be 0).
 */
static_inline void bigint_mul_u64(bigint *big, u64 val) {
    u32 idx = 0, max = big->used;
    u64 hi, lo, carry = 0;
    for (; idx < max; idx++) {
        if (big->bits[idx]) break;
    }
    for (; idx < max; idx++) {
        u128_mul_add(big->bits[idx], val, carry, &hi, &lo);
        big->bits[idx] = lo;
        carry = hi;
    }
    if (carry) big->bits[big->used++] = carry;
}

/**
 Evaluate 'big *= 2^exp'.
 @param big A big number (can be 0).
 @param exp An exponent integer (can be 0).
 */
static_inline void bigint_mul_pow2(bigint *big, u32 exp) {
    u32 shft = exp % 64;
    u32 move = exp / 64;
    u32 idx = big->used;
    if (unlikely(shft == 0)) {
        for (; idx > 0; idx--) {
            big->bits[idx + move - 1] = big->bits[idx - 1];
        }
        big->used += move;
        while (move) big->bits[--move] = 0;
    } else {
        big->bits[idx] = 0;
        for (; idx > 0; idx--) {
            u64 num = big->bits[idx] << shft;
            num |= big->bits[idx - 1] >> (64 - shft);
            big->bits[idx + move] = num;
        }
        big->bits[move] = big->bits[0] << shft;
        big->used += move + (big->bits[big->used + move] > 0);
        while (move) big->bits[--move] = 0;
    }
}

/**
 Evaluate 'big *= 10^exp'.
 @param big A big number (can be 0).
 @param exp An exponent integer (cannot be 0).
 */
static_inline void bigint_mul_pow10(bigint *big, i32 exp) {
    for (; exp >= U64_POW10_MAX_EXP; exp -= U64_POW10_MAX_EXP) {
        bigint_mul_u64(big, u64_pow10_table[U64_POW10_MAX_EXP]);
    }
    if (exp) {
        bigint_mul_u64(big, u64_pow10_table[exp]);
    }
}

/**
 Compare two bigint.
 @return -1 if 'a < b', +1 if 'a > b', 0 if 'a == b'.
 */
static_inline i32 bigint_cmp(bigint *a, bigint *b) {
    u32 idx = a->used;
    if (a->used < b->used) return -1;
    if (a->used > b->used) return +1;
    while (idx --> 0) {
        u64 av = a->bits[idx];
        u64 bv = b->bits[idx];
        if (av < bv) return -1;
        if (av > bv) return +1;
    }
    return 0;
}

/**
 Evaluate 'big = val'.
 @param big A big number (can be 0).
 @param val An unsigned integer (can be 0).
 */
static_inline void bigint_set_u64(bigint *big, u64 val) {
    big->used = 1;
    big->bits[0] = val;
}

/** Set a bigint with floating point number string. */
static_noinline void bigint_set_buf(bigint *big, u64 sig, i32 *exp,
                                    u8 *sig_cut, u8 *sig_end, u8 *dot_pos) {
    
    if (unlikely(!sig_cut)) {
        /* no digit cut, set significant part only */
        bigint_set_u64(big, sig);
        return;
        
    } else {
        /* some digits was cut, read them from 'sig_cut' to 'sig_end' */
        u8 *hdr = sig_cut;
        u8 *cur = hdr;
        u32 len = 0;
        u64 val = 0;
        bool dig_big_cut = false;
        bool has_dot = (hdr < dot_pos) & (dot_pos < sig_end);
        u32 dig_len_total = U64_SAFE_DIG + (u32)(sig_end - hdr) - has_dot;
        
        sig -= (*sig_cut >= '5'); /* sig was rounded before */
        if (dig_len_total > F64_MAX_DEC_DIG) {
            dig_big_cut = true;
            sig_end -= dig_len_total - (F64_MAX_DEC_DIG + 1);
            sig_end -= (dot_pos + 1 == sig_end);
            dig_len_total = (F64_MAX_DEC_DIG + 1);
        }
        *exp -= (i32)dig_len_total - U64_SAFE_DIG;
        
        big->used = 1;
        big->bits[0] = sig;
        while (cur < sig_end) {
            if (likely(cur != dot_pos)) {
                val = val * 10 + (*cur++ - '0');
                len++;
                if (unlikely(cur == sig_end && dig_big_cut)) {
                    /* The last digit must be non-zero,    */
                    /* set it to '1' for correct rounding. */
                    val = val - (val % 10) + 1;
                }
                if (len == U64_SAFE_DIG || cur == sig_end) {
                    bigint_mul_pow10(big, len);
                    bigint_add_u64(big, val);
                    val = 0;
                    len = 0;
                }
            } else {
                cur++;
            }
        }
    }
}



/*==============================================================================
 * Diy Floating Point
 *============================================================================*/

/** Minimum decimal exponent in sig table (F64_MIN_DEC_EXP + 1 - 20). */
#define POW10_SIG_TABLE_MIN_EXP -343

/** Maximum decimal exponent in sig table (F64_MAX_DEC_EXP). */
#define POW10_SIG_TABLE_MAX_EXP 308

/** Normalized significant bits table for pow10 (5.1KB).
    (generate with misc/make_tables.c) */
static const u64 pow10_sig_table[] = {
    U64(0xBF29DCAB, 0xA82FDEAE), U64(0xEEF453D6, 0x923BD65A),
    U64(0x9558B466, 0x1B6565F8), U64(0xBAAEE17F, 0xA23EBF76),
    U64(0xE95A99DF, 0x8ACE6F54), U64(0x91D8A02B, 0xB6C10594),
    U64(0xB64EC836, 0xA47146FA), U64(0xE3E27A44, 0x4D8D98B8),
    U64(0x8E6D8C6A, 0xB0787F73), U64(0xB208EF85, 0x5C969F50),
    U64(0xDE8B2B66, 0xB3BC4724), U64(0x8B16FB20, 0x3055AC76),
    U64(0xADDCB9E8, 0x3C6B1794), U64(0xD953E862, 0x4B85DD79),
    U64(0x87D4713D, 0x6F33AA6C), U64(0xA9C98D8C, 0xCB009506),
    U64(0xD43BF0EF, 0xFDC0BA48), U64(0x84A57695, 0xFE98746D),
    U64(0xA5CED43B, 0x7E3E9188), U64(0xCF42894A, 0x5DCE35EA),
    U64(0x818995CE, 0x7AA0E1B2), U64(0xA1EBFB42, 0x19491A1F),
    U64(0xCA66FA12, 0x9F9B60A7), U64(0xFD00B897, 0x478238D1),
    U64(0x9E20735E, 0x8CB16382), U64(0xC5A89036, 0x2FDDBC63),
    U64(0xF712B443, 0xBBD52B7C), U64(0x9A6BB0AA, 0x55653B2D),
    U64(0xC1069CD4, 0xEABE89F9), U64(0xF148440A, 0x256E2C77),
    U64(0x96CD2A86, 0x5764DBCA), U64(0xBC807527, 0xED3E12BD),
    U64(0xEBA09271, 0xE88D976C), U64(0x93445B87, 0x31587EA3),
    U64(0xB8157268, 0xFDAE9E4C), U64(0xE61ACF03, 0x3D1A45DF),
    U64(0x8FD0C162, 0x06306BAC), U64(0xB3C4F1BA, 0x87BC8697),
    U64(0xE0B62E29, 0x29ABA83C), U64(0x8C71DCD9, 0xBA0B4926),
    U64(0xAF8E5410, 0x288E1B6F), U64(0xDB71E914, 0x32B1A24B),
    U64(0x892731AC, 0x9FAF056F), U64(0xAB70FE17, 0xC79AC6CA),
    U64(0xD64D3D9D, 0xB981787D), U64(0x85F04682, 0x93F0EB4E),
    U64(0xA76C5823, 0x38ED2622), U64(0xD1476E2C, 0x07286FAA),
    U64(0x82CCA4DB, 0x847945CA), U64(0xA37FCE12, 0x6597973D),
    U64(0xCC5FC196, 0xFEFD7D0C), U64(0xFF77B1FC, 0xBEBCDC4F),
    U64(0x9FAACF3D, 0xF73609B1), U64(0xC795830D, 0x75038C1E),
    U64(0xF97AE3D0, 0xD2446F25), U64(0x9BECCE62, 0x836AC577),
    U64(0xC2E801FB, 0x244576D5), U64(0xF3A20279, 0xED56D48A),
    U64(0x9845418C, 0x345644D7), U64(0xBE5691EF, 0x416BD60C),
    U64(0xEDEC366B, 0x11C6CB8F), U64(0x94B3A202, 0xEB1C3F39),
    U64(0xB9E08A83, 0xA5E34F08), U64(0xE858AD24, 0x8F5C22CA),
    U64(0x91376C36, 0xD99995BE), U64(0xB5854744, 0x8FFFFB2E),
    U64(0xE2E69915, 0xB3FFF9F9), U64(0x8DD01FAD, 0x907FFC3C),
    U64(0xB1442798, 0xF49FFB4B), U64(0xDD95317F, 0x31C7FA1D),
    U64(0x8A7D3EEF, 0x7F1CFC52), U64(0xAD1C8EAB, 0x5EE43B67),
    U64(0xD863B256, 0x369D4A41), U64(0x873E4F75, 0xE2224E68),
    U64(0xA90DE353, 0x5AAAE202), U64(0xD3515C28, 0x31559A83),
    U64(0x8412D999, 0x1ED58092), U64(0xA5178FFF, 0x668AE0B6),
    U64(0xCE5D73FF, 0x402D98E4), U64(0x80FA687F, 0x881C7F8E),
    U64(0xA139029F, 0x6A239F72), U64(0xC9874347, 0x44AC874F),
    U64(0xFBE91419, 0x15D7A922), U64(0x9D71AC8F, 0xADA6C9B5),
    U64(0xC4CE17B3, 0x99107C23), U64(0xF6019DA0, 0x7F549B2B),
    U64(0x99C10284, 0x4F94E0FB), U64(0xC0314325, 0x637A193A),
    U64(0xF03D93EE, 0xBC589F88), U64(0x96267C75, 0x35B763B5),
    U64(0xBBB01B92, 0x83253CA3), U64(0xEA9C2277, 0x23EE8BCB),
    U64(0x92A1958A, 0x7675175F), U64(0xB749FAED, 0x14125D37),
    U64(0xE51C79A8, 0x5916F485), U64(0x8F31CC09, 0x37AE58D3),
    U64(0xB2FE3F0B, 0x8599EF08), U64(0xDFBDCECE, 0x67006AC9),
    U64(0x8BD6A141, 0x006042BE), U64(0xAECC4991, 0x4078536D),
    U64(0xDA7F5BF5, 0x90966849), U64(0x888F9979, 0x7A5E012D),
    U64(0xAAB37FD7, 0xD8F58179), U64(0xD5605FCD, 0xCF32E1D7),
    U64(0x855C3BE0, 0xA17FCD26), U64(0xA6B34AD8, 0xC9DFC070),
    U64(0xD0601D8E, 0xFC57B08C), U64(0x823C1279, 0x5DB6CE57),
    U64(0xA2CB1717, 0xB52481ED), U64(0xCB7DDCDD, 0xA26DA269),
    U64(0xFE5D5415, 0x0B090B03), U64(0x9EFA548D, 0x26E5A6E2),
    U64(0xC6B8E9B0, 0x709F109A), U64(0xF867241C, 0x8CC6D4C1),
    U64(0x9B407691, 0xD7FC44F8), U64(0xC2109436, 0x4DFB5637),
    U64(0xF294B943, 0xE17A2BC4), U64(0x979CF3CA, 0x6CEC5B5B),
    U64(0xBD8430BD, 0x08277231), U64(0xECE53CEC, 0x4A314EBE),
    U64(0x940F4613, 0xAE5ED137), U64(0xB9131798, 0x99F68584),
    U64(0xE757DD7E, 0xC07426E5), U64(0x9096EA6F, 0x3848984F),
    U64(0xB4BCA50B, 0x065ABE63), U64(0xE1EBCE4D, 0xC7F16DFC),
    U64(0x8D3360F0, 0x9CF6E4BD), U64(0xB080392C, 0xC4349DED),
    U64(0xDCA04777, 0xF541C568), U64(0x89E42CAA, 0xF9491B61),
    U64(0xAC5D37D5, 0xB79B6239), U64(0xD77485CB, 0x25823AC7),
    U64(0x86A8D39E, 0xF77164BD), U64(0xA8530886, 0xB54DBDEC),
    U64(0xD267CAA8, 0x62A12D67), U64(0x8380DEA9, 0x3DA4BC60),
    U64(0xA4611653, 0x8D0DEB78), U64(0xCD795BE8, 0x70516656),
    U64(0x806BD971, 0x4632DFF6), U64(0xA086CFCD, 0x97BF97F4),
    U64(0xC8A883C0, 0xFDAF7DF0), U64(0xFAD2A4B1, 0x3D1B5D6C),
    U64(0x9CC3A6EE, 0xC6311A64), U64(0xC3F490AA, 0x77BD60FD),
    U64(0xF4F1B4D5, 0x15ACB93C), U64(0x99171105, 0x2D8BF3C5),
    U64(0xBF5CD546, 0x78EEF0B7), U64(0xEF340A98, 0x172AACE5),
    U64(0x9580869F, 0x0E7AAC0F), U64(0xBAE0A846, 0xD2195713),
    U64(0xE998D258, 0x869FACD7), U64(0x91FF8377, 0x5423CC06),
    U64(0xB67F6455, 0x292CBF08), U64(0xE41F3D6A, 0x7377EECA),
    U64(0x8E938662, 0x882AF53E), U64(0xB23867FB, 0x2A35B28E),
    U64(0xDEC681F9, 0xF4C31F31), U64(0x8B3C113C, 0x38F9F37F),
    U64(0xAE0B158B, 0x4738705F), U64(0xD98DDAEE, 0x19068C76),
    U64(0x87F8A8D4, 0xCFA417CA), U64(0xA9F6D30A, 0x038D1DBC),
    U64(0xD47487CC, 0x8470652B), U64(0x84C8D4DF, 0xD2C63F3B),
    U64(0xA5FB0A17, 0xC777CF0A), U64(0xCF79CC9D, 0xB955C2CC),
    U64(0x81AC1FE2, 0x93D599C0), U64(0xA21727DB, 0x38CB0030),
    U64(0xCA9CF1D2, 0x06FDC03C), U64(0xFD442E46, 0x88BD304B),
    U64(0x9E4A9CEC, 0x15763E2F), U64(0xC5DD4427, 0x1AD3CDBA),
    U64(0xF7549530, 0xE188C129), U64(0x9A94DD3E, 0x8CF578BA),
    U64(0xC13A148E, 0x3032D6E8), U64(0xF18899B1, 0xBC3F8CA2),
    U64(0x96F5600F, 0x15A7B7E5), U64(0xBCB2B812, 0xDB11A5DE),
    U64(0xEBDF6617, 0x91D60F56), U64(0x936B9FCE, 0xBB25C996),
    U64(0xB84687C2, 0x69EF3BFB), U64(0xE65829B3, 0x046B0AFA),
    U64(0x8FF71A0F, 0xE2C2E6DC), U64(0xB3F4E093, 0xDB73A093),
    U64(0xE0F218B8, 0xD25088B8), U64(0x8C974F73, 0x83725573),
    U64(0xAFBD2350, 0x644EEAD0), U64(0xDBAC6C24, 0x7D62A584),
    U64(0x894BC396, 0xCE5DA772), U64(0xAB9EB47C, 0x81F5114F),
    U64(0xD686619B, 0xA27255A3), U64(0x8613FD01, 0x45877586),
    U64(0xA798FC41, 0x96E952E7), U64(0xD17F3B51, 0xFCA3A7A1),
    U64(0x82EF8513, 0x3DE648C5), U64(0xA3AB6658, 0x0D5FDAF6),
    U64(0xCC963FEE, 0x10B7D1B3), U64(0xFFBBCFE9, 0x94E5C620),
    U64(0x9FD561F1, 0xFD0F9BD4), U64(0xC7CABA6E, 0x7C5382C9),
    U64(0xF9BD690A, 0x1B68637B), U64(0x9C1661A6, 0x51213E2D),
    U64(0xC31BFA0F, 0xE5698DB8), U64(0xF3E2F893, 0xDEC3F126),
    U64(0x986DDB5C, 0x6B3A76B8), U64(0xBE895233, 0x86091466),
    U64(0xEE2BA6C0, 0x678B597F), U64(0x94DB4838, 0x40B717F0),
    U64(0xBA121A46, 0x50E4DDEC), U64(0xE896A0D7, 0xE51E1566),
    U64(0x915E2486, 0xEF32CD60), U64(0xB5B5ADA8, 0xAAFF80B8),
    U64(0xE3231912, 0xD5BF60E6), U64(0x8DF5EFAB, 0xC5979C90),
    U64(0xB1736B96, 0xB6FD83B4), U64(0xDDD0467C, 0x64BCE4A1),
    U64(0x8AA22C0D, 0xBEF60EE4), U64(0xAD4AB711, 0x2EB3929E),
    U64(0xD89D64D5, 0x7A607745), U64(0x87625F05, 0x6C7C4A8B),
    U64(0xA93AF6C6, 0xC79B5D2E), U64(0xD389B478, 0x79823479),
    U64(0x843610CB, 0x4BF160CC), U64(0xA54394FE, 0x1EEDB8FF),
    U64(0xCE947A3D, 0xA6A9273E), U64(0x811CCC66, 0x8829B887),
    U64(0xA163FF80, 0x2A3426A9), U64(0xC9BCFF60, 0x34C13053),
    U64(0xFC2C3F38, 0x41F17C68), U64(0x9D9BA783, 0x2936EDC1),
    U64(0xC5029163, 0xF384A931), U64(0xF64335BC, 0xF065D37D),
    U64(0x99EA0196, 0x163FA42E), U64(0xC06481FB, 0x9BCF8D3A),
    U64(0xF07DA27A, 0x82C37088), U64(0x964E858C, 0x91BA2655),
    U64(0xBBE226EF, 0xB628AFEB), U64(0xEADAB0AB, 0xA3B2DBE5),
    U64(0x92C8AE6B, 0x464FC96F), U64(0xB77ADA06, 0x17E3BBCB),
    U64(0xE5599087, 0x9DDCAABE), U64(0x8F57FA54, 0xC2A9EAB7),
    U64(0xB32DF8E9, 0xF3546564), U64(0xDFF97724, 0x70297EBD),
    U64(0x8BFBEA76, 0xC619EF36), U64(0xAEFAE514, 0x77A06B04),
    U64(0xDAB99E59, 0x958885C5), U64(0x88B402F7, 0xFD75539B),
    U64(0xAAE103B5, 0xFCD2A882), U64(0xD59944A3, 0x7C0752A2),
    U64(0x857FCAE6, 0x2D8493A5), U64(0xA6DFBD9F, 0xB8E5B88F),
    U64(0xD097AD07, 0xA71F26B2), U64(0x825ECC24, 0xC8737830),
    U64(0xA2F67F2D, 0xFA90563B), U64(0xCBB41EF9, 0x79346BCA),
    U64(0xFEA126B7, 0xD78186BD), U64(0x9F24B832, 0xE6B0F436),
    U64(0xC6EDE63F, 0xA05D3144), U64(0xF8A95FCF, 0x88747D94),
    U64(0x9B69DBE1, 0xB548CE7D), U64(0xC24452DA, 0x229B021C),
    U64(0xF2D56790, 0xAB41C2A3), U64(0x97C560BA, 0x6B0919A6),
    U64(0xBDB6B8E9, 0x05CB600F), U64(0xED246723, 0x473E3813),
    U64(0x9436C076, 0x0C86E30C), U64(0xB9447093, 0x8FA89BCF),
    U64(0xE7958CB8, 0x7392C2C3), U64(0x90BD77F3, 0x483BB9BA),
    U64(0xB4ECD5F0, 0x1A4AA828), U64(0xE2280B6C, 0x20DD5232),
    U64(0x8D590723, 0x948A535F), U64(0xB0AF48EC, 0x79ACE837),
    U64(0xDCDB1B27, 0x98182245), U64(0x8A08F0F8, 0xBF0F156B),
    U64(0xAC8B2D36, 0xEED2DAC6), U64(0xD7ADF884, 0xAA879177),
    U64(0x86CCBB52, 0xEA94BAEB), U64(0xA87FEA27, 0xA539E9A5),
    U64(0xD29FE4B1, 0x8E88640F), U64(0x83A3EEEE, 0xF9153E89),
    U64(0xA48CEAAA, 0xB75A8E2B), U64(0xCDB02555, 0x653131B6),
    U64(0x808E1755, 0x5F3EBF12), U64(0xA0B19D2A, 0xB70E6ED6),
    U64(0xC8DE0475, 0x64D20A8C), U64(0xFB158592, 0xBE068D2F),
    U64(0x9CED737B, 0xB6C4183D), U64(0xC428D05A, 0xA4751E4D),
    U64(0xF5330471, 0x4D9265E0), U64(0x993FE2C6, 0xD07B7FAC),
    U64(0xBF8FDB78, 0x849A5F97), U64(0xEF73D256, 0xA5C0F77D),
    U64(0x95A86376, 0x27989AAE), U64(0xBB127C53, 0xB17EC159),
    U64(0xE9D71B68, 0x9DDE71B0), U64(0x92267121, 0x62AB070E),
    U64(0xB6B00D69, 0xBB55C8D1), U64(0xE45C10C4, 0x2A2B3B06),
    U64(0x8EB98A7A, 0x9A5B04E3), U64(0xB267ED19, 0x40F1C61C),
    U64(0xDF01E85F, 0x912E37A3), U64(0x8B61313B, 0xBABCE2C6),
    U64(0xAE397D8A, 0xA96C1B78), U64(0xD9C7DCED, 0x53C72256),
    U64(0x881CEA14, 0x545C7575), U64(0xAA242499, 0x697392D3),
    U64(0xD4AD2DBF, 0xC3D07788), U64(0x84EC3C97, 0xDA624AB5),
    U64(0xA6274BBD, 0xD0FADD62), U64(0xCFB11EAD, 0x453994BA),
    U64(0x81CEB32C, 0x4B43FCF5), U64(0xA2425FF7, 0x5E14FC32),
    U64(0xCAD2F7F5, 0x359A3B3E), U64(0xFD87B5F2, 0x8300CA0E),
    U64(0x9E74D1B7, 0x91E07E48), U64(0xC6120625, 0x76589DDB),
    U64(0xF79687AE, 0xD3EEC551), U64(0x9ABE14CD, 0x44753B53),
    U64(0xC16D9A00, 0x95928A27), U64(0xF1C90080, 0xBAF72CB1),
    U64(0x971DA050, 0x74DA7BEF), U64(0xBCE50864, 0x92111AEB),
    U64(0xEC1E4A7D, 0xB69561A5), U64(0x9392EE8E, 0x921D5D07),
    U64(0xB877AA32, 0x36A4B449), U64(0xE69594BE, 0xC44DE15B),
    U64(0x901D7CF7, 0x3AB0ACD9), U64(0xB424DC35, 0x095CD80F),
    U64(0xE12E1342, 0x4BB40E13), U64(0x8CBCCC09, 0x6F5088CC),
    U64(0xAFEBFF0B, 0xCB24AAFF), U64(0xDBE6FECE, 0xBDEDD5BF),
    U64(0x89705F41, 0x36B4A597), U64(0xABCC7711, 0x8461CEFD),
    U64(0xD6BF94D5, 0xE57A42BC), U64(0x8637BD05, 0xAF6C69B6),
    U64(0xA7C5AC47, 0x1B478423), U64(0xD1B71758, 0xE219652C),
    U64(0x83126E97, 0x8D4FDF3B), U64(0xA3D70A3D, 0x70A3D70A),
    U64(0xCCCCCCCC, 0xCCCCCCCD), U64(0x80000000, 0x00000000),
    U64(0xA0000000, 0x00000000), U64(0xC8000000, 0x00000000),
    U64(0xFA000000, 0x00000000), U64(0x9C400000, 0x00000000),
    U64(0xC3500000, 0x00000000), U64(0xF4240000, 0x00000000),
    U64(0x98968000, 0x00000000), U64(0xBEBC2000, 0x00000000),
    U64(0xEE6B2800, 0x00000000), U64(0x9502F900, 0x00000000),
    U64(0xBA43B740, 0x00000000), U64(0xE8D4A510, 0x00000000),
    U64(0x9184E72A, 0x00000000), U64(0xB5E620F4, 0x80000000),
    U64(0xE35FA931, 0xA0000000), U64(0x8E1BC9BF, 0x04000000),
    U64(0xB1A2BC2E, 0xC5000000), U64(0xDE0B6B3A, 0x76400000),
    U64(0x8AC72304, 0x89E80000), U64(0xAD78EBC5, 0xAC620000),
    U64(0xD8D726B7, 0x177A8000), U64(0x87867832, 0x6EAC9000),
    U64(0xA968163F, 0x0A57B400), U64(0xD3C21BCE, 0xCCEDA100),
    U64(0x84595161, 0x401484A0), U64(0xA56FA5B9, 0x9019A5C8),
    U64(0xCECB8F27, 0xF4200F3A), U64(0x813F3978, 0xF8940984),
    U64(0xA18F07D7, 0x36B90BE5), U64(0xC9F2C9CD, 0x04674EDF),
    U64(0xFC6F7C40, 0x45812296), U64(0x9DC5ADA8, 0x2B70B59E),
    U64(0xC5371912, 0x364CE305), U64(0xF684DF56, 0xC3E01BC7),
    U64(0x9A130B96, 0x3A6C115C), U64(0xC097CE7B, 0xC90715B3),
    U64(0xF0BDC21A, 0xBB48DB20), U64(0x96769950, 0xB50D88F4),
    U64(0xBC143FA4, 0xE250EB31), U64(0xEB194F8E, 0x1AE525FD),
    U64(0x92EFD1B8, 0xD0CF37BE), U64(0xB7ABC627, 0x050305AE),
    U64(0xE596B7B0, 0xC643C719), U64(0x8F7E32CE, 0x7BEA5C70),
    U64(0xB35DBF82, 0x1AE4F38C), U64(0xE0352F62, 0xA19E306F),
    U64(0x8C213D9D, 0xA502DE45), U64(0xAF298D05, 0x0E4395D7),
    U64(0xDAF3F046, 0x51D47B4C), U64(0x88D8762B, 0xF324CD10),
    U64(0xAB0E93B6, 0xEFEE0054), U64(0xD5D238A4, 0xABE98068),
    U64(0x85A36366, 0xEB71F041), U64(0xA70C3C40, 0xA64E6C52),
    U64(0xD0CF4B50, 0xCFE20766), U64(0x82818F12, 0x81ED44A0),
    U64(0xA321F2D7, 0x226895C8), U64(0xCBEA6F8C, 0xEB02BB3A),
    U64(0xFEE50B70, 0x25C36A08), U64(0x9F4F2726, 0x179A2245),
    U64(0xC722F0EF, 0x9D80AAD6), U64(0xF8EBAD2B, 0x84E0D58C),
    U64(0x9B934C3B, 0x330C8577), U64(0xC2781F49, 0xFFCFA6D5),
    U64(0xF316271C, 0x7FC3908B), U64(0x97EDD871, 0xCFDA3A57),
    U64(0xBDE94E8E, 0x43D0C8EC), U64(0xED63A231, 0xD4C4FB27),
    U64(0x945E455F, 0x24FB1CF9), U64(0xB975D6B6, 0xEE39E437),
    U64(0xE7D34C64, 0xA9C85D44), U64(0x90E40FBE, 0xEA1D3A4B),
    U64(0xB51D13AE, 0xA4A488DD), U64(0xE264589A, 0x4DCDAB15),
    U64(0x8D7EB760, 0x70A08AED), U64(0xB0DE6538, 0x8CC8ADA8),
    U64(0xDD15FE86, 0xAFFAD912), U64(0x8A2DBF14, 0x2DFCC7AB),
    U64(0xACB92ED9, 0x397BF996), U64(0xD7E77A8F, 0x87DAF7FC),
    U64(0x86F0AC99, 0xB4E8DAFD), U64(0xA8ACD7C0, 0x222311BD),
    U64(0xD2D80DB0, 0x2AABD62C), U64(0x83C7088E, 0x1AAB65DB),
    U64(0xA4B8CAB1, 0xA1563F52), U64(0xCDE6FD5E, 0x09ABCF27),
    U64(0x80B05E5A, 0xC60B6178), U64(0xA0DC75F1, 0x778E39D6),
    U64(0xC913936D, 0xD571C84C), U64(0xFB587849, 0x4ACE3A5F),
    U64(0x9D174B2D, 0xCEC0E47B), U64(0xC45D1DF9, 0x42711D9A),
    U64(0xF5746577, 0x930D6501), U64(0x9968BF6A, 0xBBE85F20),
    U64(0xBFC2EF45, 0x6AE276E9), U64(0xEFB3AB16, 0xC59B14A3),
    U64(0x95D04AEE, 0x3B80ECE6), U64(0xBB445DA9, 0xCA61281F),
    U64(0xEA157514, 0x3CF97227), U64(0x924D692C, 0xA61BE758),
    U64(0xB6E0C377, 0xCFA2E12E), U64(0xE498F455, 0xC38B997A),
    U64(0x8EDF98B5, 0x9A373FEC), U64(0xB2977EE3, 0x00C50FE7),
    U64(0xDF3D5E9B, 0xC0F653E1), U64(0x8B865B21, 0x5899F46D),
    U64(0xAE67F1E9, 0xAEC07188), U64(0xDA01EE64, 0x1A708DEA),
    U64(0x884134FE, 0x908658B2), U64(0xAA51823E, 0x34A7EEDF),
    U64(0xD4E5E2CD, 0xC1D1EA96), U64(0x850FADC0, 0x9923329E),
    U64(0xA6539930, 0xBF6BFF46), U64(0xCFE87F7C, 0xEF46FF17),
    U64(0x81F14FAE, 0x158C5F6E), U64(0xA26DA399, 0x9AEF774A),
    U64(0xCB090C80, 0x01AB551C), U64(0xFDCB4FA0, 0x02162A63),
    U64(0x9E9F11C4, 0x014DDA7E), U64(0xC646D635, 0x01A1511E),
    U64(0xF7D88BC2, 0x4209A565), U64(0x9AE75759, 0x6946075F),
    U64(0xC1A12D2F, 0xC3978937), U64(0xF209787B, 0xB47D6B85),
    U64(0x9745EB4D, 0x50CE6333), U64(0xBD176620, 0xA501FC00),
    U64(0xEC5D3FA8, 0xCE427B00), U64(0x93BA47C9, 0x80E98CE0),
    U64(0xB8A8D9BB, 0xE123F018), U64(0xE6D3102A, 0xD96CEC1E),
    U64(0x9043EA1A, 0xC7E41393), U64(0xB454E4A1, 0x79DD1877),
    U64(0xE16A1DC9, 0xD8545E95), U64(0x8CE2529E, 0x2734BB1D),
    U64(0xB01AE745, 0xB101E9E4), U64(0xDC21A117, 0x1D42645D),
    U64(0x899504AE, 0x72497EBA), U64(0xABFA45DA, 0x0EDBDE69),
    U64(0xD6F8D750, 0x9292D603), U64(0x865B8692, 0x5B9BC5C2),
    U64(0xA7F26836, 0xF282B733), U64(0xD1EF0244, 0xAF2364FF),
    U64(0x8335616A, 0xED761F1F), U64(0xA402B9C5, 0xA8D3A6E7),
    U64(0xCD036837, 0x130890A1), U64(0x80222122, 0x6BE55A65),
    U64(0xA02AA96B, 0x06DEB0FE), U64(0xC83553C5, 0xC8965D3D),
    U64(0xFA42A8B7, 0x3ABBF48D), U64(0x9C69A972, 0x84B578D8),
    U64(0xC38413CF, 0x25E2D70E), U64(0xF46518C2, 0xEF5B8CD1),
    U64(0x98BF2F79, 0xD5993803), U64(0xBEEEFB58, 0x4AFF8604),
    U64(0xEEAABA2E, 0x5DBF6785), U64(0x952AB45C, 0xFA97A0B3),
    U64(0xBA756174, 0x393D88E0), U64(0xE912B9D1, 0x478CEB17),
    U64(0x91ABB422, 0xCCB812EF), U64(0xB616A12B, 0x7FE617AA),
    U64(0xE39C4976, 0x5FDF9D95), U64(0x8E41ADE9, 0xFBEBC27D),
    U64(0xB1D21964, 0x7AE6B31C), U64(0xDE469FBD, 0x99A05FE3),
    U64(0x8AEC23D6, 0x80043BEE), U64(0xADA72CCC, 0x20054AEA),
    U64(0xD910F7FF, 0x28069DA4), U64(0x87AA9AFF, 0x79042287),
    U64(0xA99541BF, 0x57452B28), U64(0xD3FA922F, 0x2D1675F2),
    U64(0x847C9B5D, 0x7C2E09B7), U64(0xA59BC234, 0xDB398C25),
    U64(0xCF02B2C2, 0x1207EF2F), U64(0x8161AFB9, 0x4B44F57D),
    U64(0xA1BA1BA7, 0x9E1632DC), U64(0xCA28A291, 0x859BBF93),
    U64(0xFCB2CB35, 0xE702AF78), U64(0x9DEFBF01, 0xB061ADAB),
    U64(0xC56BAEC2, 0x1C7A1916), U64(0xF6C69A72, 0xA3989F5C),
    U64(0x9A3C2087, 0xA63F6399), U64(0xC0CB28A9, 0x8FCF3C80),
    U64(0xF0FDF2D3, 0xF3C30B9F), U64(0x969EB7C4, 0x7859E744),
    U64(0xBC4665B5, 0x96706115), U64(0xEB57FF22, 0xFC0C795A),
    U64(0x9316FF75, 0xDD87CBD8), U64(0xB7DCBF53, 0x54E9BECE),
    U64(0xE5D3EF28, 0x2A242E82), U64(0x8FA47579, 0x1A569D11),
    U64(0xB38D92D7, 0x60EC4455), U64(0xE070F78D, 0x3927556B),
    U64(0x8C469AB8, 0x43B89563), U64(0xAF584166, 0x54A6BABB),
    U64(0xDB2E51BF, 0xE9D0696A), U64(0x88FCF317, 0xF22241E2),
    U64(0xAB3C2FDD, 0xEEAAD25B), U64(0xD60B3BD5, 0x6A5586F2),
    U64(0x85C70565, 0x62757457), U64(0xA738C6BE, 0xBB12D16D),
    U64(0xD106F86E, 0x69D785C8), U64(0x82A45B45, 0x0226B39D),
    U64(0xA34D7216, 0x42B06084), U64(0xCC20CE9B, 0xD35C78A5),
    U64(0xFF290242, 0xC83396CE), U64(0x9F79A169, 0xBD203E41),
    U64(0xC75809C4, 0x2C684DD1), U64(0xF92E0C35, 0x37826146),
    U64(0x9BBCC7A1, 0x42B17CCC), U64(0xC2ABF989, 0x935DDBFE),
    U64(0xF356F7EB, 0xF83552FE), U64(0x98165AF3, 0x7B2153DF),
    U64(0xBE1BF1B0, 0x59E9A8D6), U64(0xEDA2EE1C, 0x7064130C),
    U64(0x9485D4D1, 0xC63E8BE8), U64(0xB9A74A06, 0x37CE2EE1),
    U64(0xE8111C87, 0xC5C1BA9A), U64(0x910AB1D4, 0xDB9914A0),
    U64(0xB54D5E4A, 0x127F59C8), U64(0xE2A0B5DC, 0x971F303A),
    U64(0x8DA471A9, 0xDE737E24), U64(0xB10D8E14, 0x56105DAD),
    U64(0xDD50F199, 0x6B947519), U64(0x8A5296FF, 0xE33CC930),
    U64(0xACE73CBF, 0xDC0BFB7B), U64(0xD8210BEF, 0xD30EFA5A),
    U64(0x8714A775, 0xE3E95C78), U64(0xA8D9D153, 0x5CE3B396),
    U64(0xD31045A8, 0x341CA07C), U64(0x83EA2B89, 0x2091E44E),
    U64(0xA4E4B66B, 0x68B65D61), U64(0xCE1DE406, 0x42E3F4B9),
    U64(0x80D2AE83, 0xE9CE78F4), U64(0xA1075A24, 0xE4421731),
    U64(0xC94930AE, 0x1D529CFD), U64(0xFB9B7CD9, 0xA4A7443C),
    U64(0x9D412E08, 0x06E88AA6), U64(0xC491798A, 0x08A2AD4F),
    U64(0xF5B5D7EC, 0x8ACB58A3), U64(0x9991A6F3, 0xD6BF1766),
    U64(0xBFF610B0, 0xCC6EDD3F), U64(0xEFF394DC, 0xFF8A948F),
    U64(0x95F83D0A, 0x1FB69CD9), U64(0xBB764C4C, 0xA7A44410),
    U64(0xEA53DF5F, 0xD18D5514), U64(0x92746B9B, 0xE2F8552C),
    U64(0xB7118682, 0xDBB66A77), U64(0xE4D5E823, 0x92A40515),
    U64(0x8F05B116, 0x3BA6832D), U64(0xB2C71D5B, 0xCA9023F8),
    U64(0xDF78E4B2, 0xBD342CF7), U64(0x8BAB8EEF, 0xB6409C1A),
    U64(0xAE9672AB, 0xA3D0C321), U64(0xDA3C0F56, 0x8CC4F3E9),
    U64(0x88658996, 0x17FB1871), U64(0xAA7EEBFB, 0x9DF9DE8E),
    U64(0xD51EA6FA, 0x85785631), U64(0x8533285C, 0x936B35DF),
    U64(0xA67FF273, 0xB8460357), U64(0xD01FEF10, 0xA657842C),
    U64(0x8213F56A, 0x67F6B29C), U64(0xA298F2C5, 0x01F45F43),
    U64(0xCB3F2F76, 0x42717713), U64(0xFE0EFB53, 0xD30DD4D8),
    U64(0x9EC95D14, 0x63E8A507), U64(0xC67BB459, 0x7CE2CE49),
    U64(0xF81AA16F, 0xDC1B81DB), U64(0x9B10A4E5, 0xE9913129),
    U64(0xC1D4CE1F, 0x63F57D73), U64(0xF24A01A7, 0x3CF2DCD0),
    U64(0x976E4108, 0x8617CA02), U64(0xBD49D14A, 0xA79DBC82),
    U64(0xEC9C459D, 0x51852BA3), U64(0x93E1AB82, 0x52F33B46),
    U64(0xB8DA1662, 0xE7B00A17), U64(0xE7109BFB, 0xA19C0C9D),
    U64(0x906A617D, 0x450187E2), U64(0xB484F9DC, 0x9641E9DB),
    U64(0xE1A63853, 0xBBD26451), U64(0x8D07E334, 0x55637EB3),
    U64(0xB049DC01, 0x6ABC5E60), U64(0xDC5C5301, 0xC56B75F7),
    U64(0x89B9B3E1, 0x1B6329BB), U64(0xAC2820D9, 0x623BF429),
    U64(0xD732290F, 0xBACAF134), U64(0x867F59A9, 0xD4BED6C0),
    U64(0xA81F3014, 0x49EE8C70), U64(0xD226FC19, 0x5C6A2F8C),
    U64(0x83585D8F, 0xD9C25DB8), U64(0xA42E74F3, 0xD032F526),
    U64(0xCD3A1230, 0xC43FB26F), U64(0x80444B5E, 0x7AA7CF85),
    U64(0xA0555E36, 0x1951C367), U64(0xC86AB5C3, 0x9FA63441),
    U64(0xFA856334, 0x878FC151), U64(0x9C935E00, 0xD4B9D8D2),
    U64(0xC3B83581, 0x09E84F07), U64(0xF4A642E1, 0x4C6262C9),
    U64(0x98E7E9CC, 0xCFBD7DBE), U64(0xBF21E440, 0x03ACDD2D),
    U64(0xEEEA5D50, 0x04981478), U64(0x95527A52, 0x02DF0CCB),
    U64(0xBAA718E6, 0x8396CFFE), U64(0xE950DF20, 0x247C83FD),
    U64(0x91D28B74, 0x16CDD27E), U64(0xB6472E51, 0x1C81471E),
    U64(0xE3D8F9E5, 0x63A198E5), U64(0x8E679C2F, 0x5E44FF8F)
};

/** "Do It Yourself Floating Point" struct. */
typedef struct diy_fp {
    u64 sig; /* significand */
    i32 exp; /* exponent, base 2 */
} diy_fp;

/** Get cached diy_fp with 10^x. The input value must in range
    [POW10_SIG_TABLE_MIN_EXP, POW10_SIG_TABLE_MAX_EXP]. */
static_inline diy_fp diy_fp_get_cached_pow10(i32 pow10) {
    u64 sig = pow10_sig_table[pow10 - POW10_SIG_TABLE_MIN_EXP];
    i32 exp = (pow10 * 217706 - 4128768) >> 16; /* exponent base 2 */
    diy_fp fp;
    fp.sig = sig;
    fp.exp = exp;
    return fp;
}

/** Returns fp * fp2. */
static_inline diy_fp diy_fp_mul(diy_fp fp, diy_fp fp2) {
    u64 hi, lo;
    u128_mul(fp.sig, fp2.sig, &hi, &lo);
    fp.sig = hi + (lo >> 63);
    fp.exp += fp2.exp + 64;
    return fp;
}

/** Convert diy_fp to IEEE-754 raw value. */
static_inline u64 diy_fp_to_ieee_raw(diy_fp fp) {
    u64 sig = fp.sig;
    i32 exp = fp.exp;
    u32 lz_bits;
    if (unlikely(fp.sig == 0)) return 0;
    
    lz_bits = u64_lz_bits(sig);
    sig <<= lz_bits;
    sig >>= F64_BITS - F64_SIG_FULL_BITS;
    exp -= lz_bits;
    exp += F64_BITS - F64_SIG_FULL_BITS;
    exp += F64_SIG_BITS;
    
    if (unlikely(exp >= F64_MAX_BIN_EXP)) {
        /* overflow */
        return F64_RAW_INF;
    } else if (likely(exp >= F64_MIN_BIN_EXP - 1)) {
        /* normal */
        exp += F64_EXP_BIAS;
        return ((u64)exp << F64_SIG_BITS) | (sig & F64_SIG_MASK);
    } else if (likely(exp >= F64_MIN_BIN_EXP - F64_SIG_FULL_BITS)) {
        /* subnormal */
        return sig >> (F64_MIN_BIN_EXP - exp - 1);
    } else {
        /* underflow */
        return 0;
    }
}



/*==============================================================================
 * JSON Number Reader (IEEE-754)
 *============================================================================*/

/** Maximum pow10 exponent for double value. */
#define F64_POW10_EXP_MAX_EXACT 22

/** Maximum pow10 exponent cached (same as F64_MAX_DEC_EXP). */
#define F64_POW10_EXP_MAX 308

/** Cached pow10 table (size: 2.4KB) (generate with misc/make_tables.c). */
static const f64 f64_pow10_table[F64_POW10_EXP_MAX + 1] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29,
    1e30, 1e31, 1e32, 1e33, 1e34, 1e35, 1e36, 1e37, 1e38, 1e39,
    1e40, 1e41, 1e42, 1e43, 1e44, 1e45, 1e46, 1e47, 1e48, 1e49,
    1e50, 1e51, 1e52, 1e53, 1e54, 1e55, 1e56, 1e57, 1e58, 1e59,
    1e60, 1e61, 1e62, 1e63, 1e64, 1e65, 1e66, 1e67, 1e68, 1e69,
    1e70, 1e71, 1e72, 1e73, 1e74, 1e75, 1e76, 1e77, 1e78, 1e79,
    1e80, 1e81, 1e82, 1e83, 1e84, 1e85, 1e86, 1e87, 1e88, 1e89,
    1e90, 1e91, 1e92, 1e93, 1e94, 1e95, 1e96, 1e97, 1e98, 1e99,
    1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109,
    1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119,
    1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129,
    1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139,
    1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149,
    1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159,
    1e160, 1e161, 1e162, 1e163, 1e164, 1e165, 1e166, 1e167, 1e168, 1e169,
    1e170, 1e171, 1e172, 1e173, 1e174, 1e175, 1e176, 1e177, 1e178, 1e179,
    1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186, 1e187, 1e188, 1e189,
    1e190, 1e191, 1e192, 1e193, 1e194, 1e195, 1e196, 1e197, 1e198, 1e199,
    1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207, 1e208, 1e209,
    1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219,
    1e220, 1e221, 1e222, 1e223, 1e224, 1e225, 1e226, 1e227, 1e228, 1e229,
    1e230, 1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239,
    1e240, 1e241, 1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249,
    1e250, 1e251, 1e252, 1e253, 1e254, 1e255, 1e256, 1e257, 1e258, 1e259,
    1e260, 1e261, 1e262, 1e263, 1e264, 1e265, 1e266, 1e267, 1e268, 1e269,
    1e270, 1e271, 1e272, 1e273, 1e274, 1e275, 1e276, 1e277, 1e278, 1e279,
    1e280, 1e281, 1e282, 1e283, 1e284, 1e285, 1e286, 1e287, 1e288, 1e289,
    1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296, 1e297, 1e298, 1e299,
    1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307, 1e308
};

/** Minimum valid bits in f64_bits_to_pow10_exp_table. */
#define F64_BITS_TO_POW10_MIN 3

/** 
 Maximum pow10 exponent value which can fit in the bits.
 For example:

    10^4 = binary [1001110001]0000, significant bit is 10.
    10^5 = binary [110000110101]00000, significant bit is 12.
    table[10] = 4.
    table[12] = 5.
    
 */
static const i32 f64_bit_to_pow10_exp_table[F64_SIG_FULL_BITS + 1] = {
    -1, 0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8,
    9, 9, 9, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 15,
    16, 16, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22
};

/**
 Read a JSON number.
 
 1. This function assume that the floating-point stack is based on IEEE-754.
 2. This function support uint64/int64/double number. If an integer number
    cannot fit in uint64/int64, it will returns as a double number. If a double
    number is infinite, the return value is based on flag.
 3. This function does not support 'nan' or 'inf' literal.
 4. This function (with inline attribute) may generate a lot of instructions.
 */
static_inline bool read_number(u8 *cur,
                               u8 **end,
                               yyjson_read_flag flg,
                               yyjson_val *val,
                               const char **msg) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)
    
#define return_err(_pos, _msg) do { \
    *msg = _msg; \
    *end = _pos; \
    return false; \
} while(false)
    
#define return_i64(_v) do { \
    val->tag = YYJSON_TYPE_NUM | ((u8)sign << 3); \
    val->uni.u64 = (u64)(sign ? (u64)(~(_v) + 1) : (u64)(_v)); \
    *end = cur; return true; \
} while(false)
    
#define return_f64(_v) do { \
    val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL; \
    val->uni.f64 = sign ? -(f64)(_v) : (f64)(_v); \
    *end = cur; return true; \
} while(false)
    
#define return_f64_raw(_v) do { \
    val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL; \
    val->uni.u64 = ((u64)sign << 63) | (u64)(_v); \
    *end = cur; return true; \
} while(false)
    
#define return_inf() do { \
    if (has_flag(ALLOW_INF_AND_NAN)) return_f64_raw(F64_RAW_INF); \
    else return_err(hdr, "number is infinity when parsed as double"); \
} while(false)
    
    u8 *sig_cut = NULL; /* significant part cutting position for long number */
    u8 *sig_end = NULL; /* significant part ending position */
    u8 *dot_pos = NULL; /* decimal point position */
    
    u64 sig = 0; /* significant part of the number */
    i32 exp = 0; /* exponent part of the number */
    
    bool exp_sign = false; /* temporary exponent sign from literal part */
    i64 exp_sig = 0; /* temporary exponent number from significant part */
    i64 exp_lit = 0; /* temporary exponent number from exponent literal part */
    u64 num; /* temporary number for reading */
    u8 *tmp; /* temporary cursor for reading */
    
    u8 *hdr = cur;
    bool sign = (*hdr == '-');
    cur += sign;
    
    if (unlikely(!digi_is_nonzero(*cur))) { /* 0 or non-digit char */
        if (unlikely(*cur != '0')) { /* non-digit char */
            if (has_flag(ALLOW_INF_AND_NAN)) {
                if (read_inf_or_nan(sign, cur, end, val)) return true;
            }
            return_err(cur - 1, "no digit after minus sign");
        }
        /* begin with 0 */
        if (likely(!digi_is_digit_or_fp(*++cur))) return_i64(0);
        if (likely(*cur == '.')) {
            dot_pos = cur++;
            if (unlikely(!digi_is_digit(*cur))) {
                return_err(cur - 1, "no digit after decimal point");
            }
            while (unlikely(*cur == '0')) cur++;
            if (likely(digi_is_digit(*cur))) {
                /* first non-zero digit after decimal point */
                sig = (u64)(*cur - '0'); /* read first digit */
                cur--;
                goto digi_frac_1; /* continue read fraction part */
            }
        }
        if (unlikely(digi_is_digit(*cur))) {
            return_err(cur - 1, "number with leading zero is not allowed");
        }
        if (unlikely(digi_is_exp(*cur))) { /* 0 with any exponent is still 0 */
            cur += (usize)1 + digi_is_sign(cur[1]);
            if (unlikely(!digi_is_digit(*cur))) {
                return_err(cur - 1, "no digit after exponent sign");
            }
            while (digi_is_digit(*++cur));
        }
        return_f64_raw(0);
    }
    
    /* begin with non-zero digit,  */
    sig = (u64)(*cur - '0');
    
    /* read integral part */
#define expr_intg(i) \
    if (likely((num = (u64)(cur[i] - (u8)'0')) <= 9)) sig = num + sig * 10; \
    else goto digi_sepr_##i;
    repeat_in_1_18(expr_intg);
#undef expr_intg
    
    cur += 19; /* skip continuous 19 digits */
    if (!digi_is_digit_or_fp(*cur)) {
        if (sign && (sig > ((u64)1 << 63))) return_f64(sig); /* overflow */
        return_i64(sig);
    }
    goto digi_intg_more; /* read more digits in integral part */
    
    /* process first non-digit character */
#define expr_sepr(i) \
    digi_sepr_##i: \
    if (likely(!digi_is_fp(cur[i]))) { cur += i; return_i64(sig); } \
    dot_pos = cur + i; \
    if (likely(cur[i] == '.')) goto digi_frac_##i; \
    cur += i; sig_end = cur; goto digi_exp_more;
    repeat_in_1_18(expr_sepr)
#undef expr_sepr
    
    /* read fraction part */
#define expr_frac(i) \
    digi_frac_##i: \
    if (likely((num = (u64)(cur[i + 1] - (u8)'0')) <= 9)) \
        sig = num + sig * 10; \
    else goto digi_stop_##i;
    repeat_in_1_18(expr_frac)
#undef expr_frac
    
    cur += 20; /* skip 19 digits and 1 decimal point */
    if (!digi_is_digit(*cur)) goto digi_frac_end; /* fraction part end */
    goto digi_frac_more; /* read more digits in fraction part */
    
    /* significant part end */
#define expr_stop(i) \
    digi_stop_##i: \
    cur += i + 1; \
    goto digi_frac_end;
    repeat_in_1_18(expr_stop)
#undef expr_stop
    
digi_intg_more: /* read more digits in integral part */
    if (digi_is_digit(*cur)) {
        if (!digi_is_digit_or_fp(cur[1])) {
            /* this number is an integer with 20 digits */
            num = (u64)(*cur - '0');
            if ((sig < (U64_MAX / 10)) ||
                (sig == (U64_MAX / 10) && num <= (U64_MAX % 10))) {
                sig = num + sig * 10;
                cur++;
                /* convert to double if overflow */
                if (sign && (sig > ((u64)1 << 63))) return_f64(sig);
                else return_i64(sig);
            }
        }
    }
    
    if (digi_is_exp(*cur)) {
        dot_pos = cur;
        goto digi_exp_more;
    }
    
    if (*cur == '.') {
        dot_pos = cur++;
        if (!digi_is_digit(*cur)) {
            return_err(cur, "no digit after decimal point");
        }
    }
    
digi_frac_more: /* read more digits in fraction part */
    sig_cut = cur; /* too large to fit in u64, excess digits need to be cut */
    sig += (*cur >= '5'); /* round */
    while (digi_is_digit(*++cur));
    if (!dot_pos) {
        dot_pos = cur;
        if (*cur == '.') {
            if (!digi_is_digit(*++cur)) {
                return_err(cur, "no digit after decimal point");
            }
            while (digi_is_digit(*cur)) cur++;
        }
    }
    exp_sig = (i64)(dot_pos - sig_cut);
    exp_sig += (dot_pos < sig_cut);
    
    /* ignore trailing zeros */
    tmp = cur - 1;
    while (*tmp == '0' || *tmp == '.') tmp--;
    if (tmp < sig_cut) {
        sig_cut = NULL;
    } else {
        sig_end = cur;
    }
    
    if (digi_is_exp(*cur)) goto digi_exp_more;
    goto digi_exp_finish;
    
digi_frac_end: /* fraction part end */
    if (unlikely(dot_pos + 1 == cur)) {
        return_err(cur - 1, "no digit after decimal point");
    }
    sig_end = cur;
    exp_sig = -(i64)((u64)(cur - dot_pos) - 1);
    if (likely(!digi_is_exp(*cur))) {
        if (unlikely(exp_sig < F64_MIN_DEC_EXP - 19)) {
            return_f64_raw(0); /* underflow */
        }
        exp = (i32)exp_sig;
        goto digi_finish;
    } else {
        goto digi_exp_more;
    }
    
digi_exp_more: /* read exponent part */
    exp_sign = (*++cur == '-');
    cur += digi_is_sign(*cur);
    if (unlikely(!digi_is_digit(*cur))) {
        return_err(cur - 1, "no digit after exponent sign");
    }
    while (*cur == '0') cur++;
    
    /* read exponent literal */
    tmp = cur;
    while (digi_is_digit(*cur)) {
        exp_lit = (*cur++ - '0') + (u64)exp_lit * 10;
    }
    if (unlikely(cur - tmp >= U64_SAFE_DIG)) {
        if (exp_sign) {
            return_f64_raw(0); /* underflow */
        } else {
            return_inf(); /* overflow */
        }
    }
    exp_sig += exp_sign ? -exp_lit : exp_lit;
    
digi_exp_finish: /* validate exponent value */
    if (unlikely(exp_sig < F64_MIN_DEC_EXP - 19)) {
        return_f64_raw(0); /* underflow */
    }
    if (unlikely(exp_sig > F64_MAX_DEC_EXP)) {
        return_inf(); /* overflow */
    }
    exp = (i32)exp_sig;
    
digi_finish: /* all digit read finished */
    
    /* Fast return (0-2 ulp error) */
    if (has_flag(FASTFP)) {
        f64 ret = (f64)sig;
        if (likely(exp >= -F64_POW10_EXP_MAX)) {
            if (likely(exp < 0)) {
                ret = ret / f64_pow10_table[-exp];
            } else {
                ret = ret * f64_pow10_table[exp];
                if (!has_flag(ALLOW_INF_AND_NAN) && unlikely(f64_isinf(ret))) {
                    return_inf();
                }
            }
        } else {
            ret = ret / f64_pow10_table[F64_MAX_DEC_EXP];
            ret = ret / f64_pow10_table[-(exp + F64_MAX_DEC_EXP)];
        }
        return_f64(ret);
    }
    
    /*
     Fast path (accurate), requirements:
     1. The input floating-point number does not lose precision.
     2. The floating-point number calculation is accurate.
     3. Correct rounding is performed (FE_TONEAREST).
     */
#if YYJSON_DOUBLE_MATH_CORRECT
    if (likely(!sig_cut &&
               exp >= -F64_POW10_EXP_MAX_EXACT &&
               exp <= +F64_POW10_EXP_MAX_EXACT * 2)) {
        u32 bits = u64_sig_bits(sig);
        if (bits <= F64_SIG_FULL_BITS) {
            if (exp < 0) {
                f64 dbl = (f64)sig;
                dbl /= f64_pow10_table[-exp];
                return_f64(dbl);
            }
            if (exp <= F64_POW10_EXP_MAX_EXACT) {
                f64 dbl = (f64)sig;
                dbl *= f64_pow10_table[exp];
                return_f64(dbl);
            }
            if (F64_SIG_FULL_BITS - bits >= F64_BITS_TO_POW10_MIN) {
                i32 exp1 = f64_bit_to_pow10_exp_table[F64_SIG_FULL_BITS - bits];
                i32 exp2 = exp - exp1;
                if (exp2 <= F64_POW10_EXP_MAX_EXACT) {
                    f64 dbl = (f64)sig;
                    dbl *= f64_pow10_table[exp1];
                    dbl *= f64_pow10_table[exp2];
                    return_f64(dbl);
                }
            }
        }
    }
#endif
    
    /*
     Slow path (accurate):
     1. Use cached diy-fp to get an approximation value.
     2. Use bigcomp to check the approximation value if needed.
     
     This algorithm refers to google's double-conversion project:
     https://github.com/google/double-conversion
     */
    {
        const i32 ERR_ULP_LOG = 3;
        const i32 ERR_ULP = 1 << ERR_ULP_LOG;
        const i32 ERR_CACHED_POW = ERR_ULP / 2;
        const i32 ERR_MUL_FIXED = ERR_ULP / 2;
        const i32 DIY_SIG_BITS = 64;
        const i32 EXP_BIAS = F64_EXP_BIAS + F64_SIG_BITS;
        const i32 EXP_SUBNORMAL = -EXP_BIAS + 1;
        
        u64 fp_err;
        u32 bits;
        i32 order_of_magnitude;
        i32 effective_significand_size;
        i32 precision_digits_count;
        u64 precision_bits;
        u64 half_way;
        
        u64 raw;
        diy_fp fp, fp_upper;
        bigint big_full, big_comp;
        i32 cmp;
        
        fp.sig = sig;
        fp.exp = 0;
        fp_err = sig_cut ? (u64)(ERR_ULP / 2) : (u64)0;
        
        /* normalize */
        bits = u64_lz_bits(fp.sig);
        fp.sig <<= bits;
        fp.exp -= bits;
        fp_err <<= bits;
        
        /* multiply and add error */
        fp = diy_fp_mul(fp, diy_fp_get_cached_pow10(exp));
        fp_err += (u64)ERR_CACHED_POW + (fp_err != 0) + (u64)ERR_MUL_FIXED;
        
        /* normalize */
        bits = u64_lz_bits(fp.sig);
        fp.sig <<= bits;
        fp.exp -= bits;
        fp_err <<= bits;
        
        /* effective significand */
        order_of_magnitude = DIY_SIG_BITS + fp.exp;
        if (likely(order_of_magnitude >= EXP_SUBNORMAL + F64_SIG_FULL_BITS)) {
            effective_significand_size = F64_SIG_FULL_BITS;
        } else if (order_of_magnitude <= EXP_SUBNORMAL) {
            effective_significand_size = 0;
        } else {
            effective_significand_size = order_of_magnitude - EXP_SUBNORMAL;
        }
        
        /* precision digits count */
        precision_digits_count = DIY_SIG_BITS - effective_significand_size;
        if (unlikely(precision_digits_count + ERR_ULP_LOG >= DIY_SIG_BITS)) {
            i32 shr = (precision_digits_count + ERR_ULP_LOG) - DIY_SIG_BITS + 1;
            fp.sig >>= shr;
            fp.exp += shr;
            fp_err = (fp_err >> shr) + 1 + ERR_ULP;
            precision_digits_count -= shr;
        }
        
        /* half way */
        precision_bits = fp.sig & (((u64)1 << precision_digits_count) - 1);
        precision_bits *= ERR_ULP;
        half_way = (u64)1 << (precision_digits_count - 1);
        half_way *= ERR_ULP;
        
        /* rounding */
        fp.sig >>= precision_digits_count;
        fp.sig += (precision_bits >= half_way + fp_err);
        fp.exp += precision_digits_count;
        
        /* get IEEE double raw value */
        raw = diy_fp_to_ieee_raw(fp);
        if (unlikely(raw == F64_RAW_INF)) return_inf();
        if (likely(precision_bits <= half_way - fp_err ||
                   precision_bits >= half_way + fp_err)) {
            return_f64_raw(raw); /* number is accurate */
        }
        
        /* upper boundary */
        if (raw & F64_EXP_MASK) {
            fp_upper.sig = (raw & F64_SIG_MASK) + ((u64)1 << F64_SIG_BITS);
            fp_upper.exp = (i32)((raw & F64_EXP_MASK) >> F64_SIG_BITS);
        } else {
            fp_upper.sig = (raw & F64_SIG_MASK);
            fp_upper.exp = 1;
        }
        fp_upper.exp -= F64_EXP_BIAS + F64_SIG_BITS;
        fp_upper.sig <<= 1;
        fp_upper.exp -= 1;
        fp_upper.sig += 1; /* add half ulp */
        
        /* bigint compare */
        bigint_set_buf(&big_full, sig, &exp, sig_cut, sig_end, dot_pos);
        bigint_set_u64(&big_comp, fp_upper.sig);
        if (exp >= 0) {
            bigint_mul_pow10(&big_full, +exp);
        } else {
            bigint_mul_pow10(&big_comp, -exp);
        }
        if (fp_upper.exp > 0) {
            bigint_mul_pow2(&big_comp, (u32)+fp_upper.exp);
        } else {
            bigint_mul_pow2(&big_full, (u32)-fp_upper.exp);
        }
        cmp = bigint_cmp(&big_full, &big_comp);
        if (likely(cmp != 0)) {
            /* round down or round up */
            raw += (cmp > 0);
        } else {
            /* falls midway, round to even */
            raw += (raw & 1);
        }
        
        if (unlikely(raw == F64_RAW_INF)) return_inf();
        return_f64_raw(raw);
    }
    
#undef has_flag
#undef return_err
#undef return_inf
#undef return_u64
#undef return_f64
#undef return_f64_raw
}

#else /* FP_READER */

/**
 Read a JSON number.
 This is a fallback function if the custom number parser is disabled.
 This function use libc's strtod() to read floating-point number.
 */
static_noinline bool read_number(u8 *cur,
                                 u8 **end,
                                 yyjson_read_flag flg,
                                 yyjson_val *val,
                                 const char **msg) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)

#define return_err(_pos, _msg) do { \
    *msg = _msg; \
    *end = _pos; \
    return false; \
} while(false)
    
#define return_i64(_v) do { \
    val->tag = YYJSON_TYPE_NUM | ((u8)sign << 3); \
    val->uni.u64 = (u64)(sign ? (u64)(~(_v) + 1) : (u64)(_v)); \
    *end = cur; return true; \
} while(false)
    
#define return_f64(_v) do { \
    val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL; \
    val->uni.f64 = sign ? -(f64)(_v) : (f64)(_v); \
    *end = cur; return true; \
} while(false)
    
    u64 sig, num;
    u8 *hdr = cur;
    bool sign = (*hdr == '-');
    
    cur += sign;
    sig = *cur - '0';
    
    /* read first digit, check leading zero */
    if (unlikely(!digi_is_digit(*cur))) {
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_inf_or_nan(sign, cur, end, val)) return true;
        }
        return_err(cur - 1, "no digit after minus sign");
    }
    if (*cur == '0') {
        cur++;
        if (unlikely(digi_is_digit(*cur))) {
            return_err(cur - 1, "number with leading zero is not allowed");
        }
        if (!digi_is_fp(*cur)) return_i64(0);
        goto read_double;
    }
    
    /* read continuous digits, up to 19 characters */
#define expr_intg(i) \
    if (likely((num = (u64)(cur[i] - (u8)'0')) <= 9)) sig = num + sig * 10; \
    else { cur += i; goto intg_end; }
    repeat_in_1_18(expr_intg);
#undef expr_intg
    
    /* here are 19 continuous digits, skip them */
    cur += 19;
    if (digi_is_digit(cur[0]) && !digi_is_digit_or_fp(cur[1])) {
        /* this number is an integer consisting of 20 digits */
        num = *cur - '0';
        if ((sig < (U64_MAX / 10)) ||
            (sig == (U64_MAX / 10) && num <= (U64_MAX % 10))) {
            sig = num + sig * 10;
            cur++;
            if (sign && (sig > ((u64)1 << 63))) return_f64(sig);
            else return_i64(sig);
        }
    }
    
intg_end:
    /* continuous digits ended */
    if (!digi_is_digit_or_fp(*cur)) {
        /* this number is an integer consisting of 1 to 19 digits */
        if (sign && (sig > ((u64)1 << 63))) return_f64(sig);
        return_i64(sig);
    }
    
read_double:
    /* this number should be read as double */
    while (digi_is_digit(*cur)) cur++;
    if (*cur == '.') {
        /* skip fraction part */
        cur++;
        if (!digi_is_digit(*cur++)) {
            return_err(cur - 1, "no digit after decimal point");
        }
        while (digi_is_digit(*cur)) cur++;
    }
    if (digi_is_exp(*cur)) {
        /* skip exponent part */
        cur += 1 + digi_is_sign(cur[1]);
        if (!digi_is_digit(*cur++)) {
            return_err(cur - 1, "no digit after exponent sign");
        }
        while (digi_is_digit(*cur)) cur++;
    }
    
    /*
     We use libc's strtod() to read the number as a double value.
     The format of this number has been verified, so we don't need the endptr.
     Note that the decimal point character used by strtod() is locale-dependent,
     and the rounding direction may affected by fesetround().
     */
    val->uni.f64 = strtod((const char *)hdr, NULL);
    if (unlikely(!f64_isfinite(val->uni.f64)) && !has_flag(ALLOW_INF_AND_NAN)) {
        return_err(hdr, "number is infinity when parsed as double");
    }
    val->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL;
    *end = cur;
    return true;
    
#undef has_flag
#undef return_err
#undef return_i64
#undef return_f64
}

#endif /* FP_READER */



/*==============================================================================
 * JSON String Reader
 *============================================================================*/

/**
 Read a JSON string.
 @param cur The head of string before '"' prefix.
 @param end The end of string after '"' suffix.
 @param val The string value to be written.
 @param msg The error message pointer.
 @return Whether succeed.
 */
static_inline bool read_string(u8 *cur,
                               u8 **end,
                               yyjson_val *val,
                               const char **msg) {
    
    /*
     Each unicode code point is encoded as 1 to 4 bytes in UTF-8 encoding,
     we use 4-byte mask and pattern value to validate UTF-8 byte sequence,
     this requires the input data to have 4-byte zero padding.
     ---------------------------------------------------
     1 byte
     unicode range [U+0000, U+007F]
     unicode min   [.......0]
     unicode max   [.1111111]
     bit pattern   [0.......]
     ---------------------------------------------------
     2 byte
     unicode range [U+0080, U+07FF]
     unicode min   [......10 ..000000]
     unicode max   [...11111 ..111111]
     bit require   [...xxxx. ........] (1E 00)
     bit mask      [xxx..... xx......] (E0 C0)
     bit pattern   [110..... 10......] (C0 80)
     ---------------------------------------------------
     3 byte
     unicode range [U+0800, U+FFFF]
     unicode min   [........ ..100000 ..000000]
     unicode max   [....1111 ..111111 ..111111]
     bit require   [....xxxx ..x..... ........] (0F 20 00)
     bit mask      [xxxx.... xx...... xx......] (F0 C0 C0)
     bit pattern   [1110.... 10...... 10......] (E0 80 80)
     ---------------------------------------------------
     3 byte invalid (reserved for surrogate halves)
     unicode range [U+D800, U+DFFF]
     unicode min   [....1101 ..100000 ..000000]
     unicode max   [....1101 ..111111 ..111111]
     bit mask      [....xxxx ..x..... ........] (0F 20 00)
     bit pattern   [....1101 ..1..... ........] (0D 20 00)
     ---------------------------------------------------
     4 byte
     unicode range [U+10000, U+10FFFF]
     unicode min   [........ ...10000 ..000000 ..000000]
     unicode max   [.....100 ..001111 ..111111 ..111111]
     bit require   [.....xxx ..xx.... ........ ........] (07 30 00 00)
     bit mask      [xxxxx... xx...... xx...... xx......] (F8 C0 C0 C0)
     bit pattern   [11110... 10...... 10...... 10......] (F0 80 80 80)
     ---------------------------------------------------
     */
#if YYJSON_ENDIAN == YYJSON_BIG_ENDIAN
    const u32 b1_mask = 0x80000000UL;
    const u32 b1_patt = 0x00000000UL;
    const u32 b2_mask = 0xE0C00000UL;
    const u32 b2_patt = 0xC0800000UL;
    const u32 b2_requ = 0x1E000000UL;
    const u32 b3_mask = 0xF0C0C000UL;
    const u32 b3_patt = 0xE0808000UL;
    const u32 b3_requ = 0x0F200000UL;
    const u32 b3_erro = 0x0D200000UL;
    const u32 b4_mask = 0xF8C0C0C0UL;
    const u32 b4_patt = 0xF0808080UL;
    const u32 b4_requ = 0x07300000UL;
    const u32 b4_err0 = 0x04000000UL;
    const u32 b4_err1 = 0x03300000UL;
#elif YYJSON_ENDIAN == YYJSON_LITTLE_ENDIAN
    const u32 b1_mask = 0x00000080UL;
    const u32 b1_patt = 0x00000000UL;
    const u32 b2_mask = 0x0000C0E0UL;
    const u32 b2_patt = 0x000080C0UL;
    const u32 b2_requ = 0x0000001EUL;
    const u32 b3_mask = 0x00C0C0F0UL;
    const u32 b3_patt = 0x008080E0UL;
    const u32 b3_requ = 0x0000200FUL;
    const u32 b3_erro = 0x0000200DUL;
    const u32 b4_mask = 0xC0C0C0F8UL;
    const u32 b4_patt = 0x808080F0UL;
    const u32 b4_requ = 0x00003007UL;
    const u32 b4_err0 = 0x00000004UL;
    const u32 b4_err1 = 0x00003003UL;
#else
    v32_uni b1_mask_uni = {{ 0x80, 0x00, 0x00, 0x00 }};
    v32_uni b1_patt_uni = {{ 0x00, 0x00, 0x00, 0x00 }};
    v32_uni b2_mask_uni = {{ 0xE0, 0xC0, 0x00, 0x00 }};
    v32_uni b2_patt_uni = {{ 0xC0, 0x80, 0x00, 0x00 }};
    v32_uni b2_requ_uni = {{ 0x1E, 0x00, 0x00, 0x00 }};
    v32_uni b3_mask_uni = {{ 0xF0, 0xC0, 0xC0, 0x00 }};
    v32_uni b3_patt_uni = {{ 0xE0, 0x80, 0x80, 0x00 }};
    v32_uni b3_requ_uni = {{ 0x0F, 0x20, 0x00, 0x00 }};
    v32_uni b3_erro_uni = {{ 0x0D, 0x20, 0x00, 0x00 }};
    v32_uni b4_mask_uni = {{ 0xF8, 0xC0, 0xC0, 0xC0 }};
    v32_uni b4_patt_uni = {{ 0xF0, 0x80, 0x80, 0x80 }};
    v32_uni b4_requ_uni = {{ 0x07, 0x30, 0x00, 0x00 }};
    v32_uni b4_err0_uni = {{ 0x04, 0x00, 0x00, 0x00 }};
    v32_uni b4_err1_uni = {{ 0x03, 0x30, 0x00, 0x00 }};
    u32 b1_mask = b1_mask_uni.u;
    u32 b1_patt = b1_patt_uni.u;
    u32 b2_mask = b2_mask_uni.u;
    u32 b2_patt = b2_patt_uni.u;
    u32 b2_requ = b2_requ_uni.u;
    u32 b3_mask = b3_mask_uni.u;
    u32 b3_patt = b3_patt_uni.u;
    u32 b3_requ = b3_requ_uni.u;
    u32 b3_erro = b3_erro_uni.u;
    u32 b4_mask = b4_mask_uni.u;
    u32 b4_patt = b4_patt_uni.u;
    u32 b4_requ = b4_requ_uni.u;
    u32 b4_err0 = b4_err0_uni.u;
    u32 b4_err1 = b4_err1_uni.u;
#endif
    
#define is_valid_seq_1(uni) \
    ((uni & b1_mask) == b1_patt)

#define is_valid_seq_2(uni) \
    ((uni & b2_mask) == b2_patt) && \
    ((uni & b2_requ))

#define is_valid_seq_3(uni) \
    ((uni & b3_mask) == b3_patt) && \
    ((tmp = (uni & b3_requ))) && \
    ((tmp != b3_erro))

#define is_valid_seq_4(uni) \
    ((uni & b4_mask) == b4_patt) && \
    ((tmp = (uni & b4_requ))) && \
    ((tmp & b4_err0) == 0 || (tmp & b4_err1) == 0)
    
#define return_err(_end, _msg) do { \
    *msg = _msg; \
    *end = _end; \
    return false; \
} while(false);
    
    u8 *src = ++cur, *dst, *pos;
    u16 hi, lo;
    u32 uni, tmp;
    
skip_ascii:
    /* Most strings have no escaped characters, so we can jump them quickly. */
    
skip_ascii_begin:
    /*
     We want to make loop unrolling, as shown in the following code. Some
     compiler may not generate instructions as expected, so we rewrite it with
     explicit goto statements. We hope the compiler can generate instructions
     like this: https://godbolt.org/z/8vjsYq
     
         while (true) repeat16({
            if (likely(!(char_is_ascii_stop(*src)))) src++;
            else break;
         });
     */
#define expr_jump(i) \
    if (likely(!char_is_ascii_stop(src[i]))) {} \
    else goto skip_ascii_stop##i;
    
#define expr_stop(i) \
    skip_ascii_stop##i: \
    src += i; \
    goto skip_ascii_end;
    
    repeat16_incr(expr_jump);
    src += 16;
    goto skip_ascii_begin;
    repeat16_incr(expr_stop);
    
#undef expr_jump
#undef expr_stop
    
skip_ascii_end:

    /*
     GCC may store src[i] in a register at each line of expr_jump(i) above.
     These instructions are useless and will degrade performance.
     This inline asm is a hint for gcc: "the memory has been modified,
     do not cache it".
     */
#if yyjson_is_real_gcc
    __asm volatile("":"=m"(*src)::)
#endif
    if (likely(*src == '"')) {
        val->tag = ((u64)(src - cur) << YYJSON_TAG_BIT) | YYJSON_TYPE_STR;
        val->uni.str = (const char *)cur;
        *src = '\0';
        *end = src + 1;
        return true;
    }
    
skip_utf8:
    if (*src & 0x80) { /* non-ASCII character */
        /*
         Non-ASCII character appears here, which means that the text is likely
         to be written in non-English or emoticons. According to some common
         data set statistics, byte sequences of the same length may appear
         consecutively. We process the byte sequences of the same length in each
         loop, which is more friendly to branch prediction.
         */
        pos = src;
        uni = byte_load_4(src);
        while (is_valid_seq_3(uni)) {
            src += 3;
            uni = byte_load_4(src);
        }
        if (is_valid_seq_1(uni)) goto skip_ascii;
        while (is_valid_seq_2(uni)) {
            src += 2;
            uni = byte_load_4(src);
        }
        while (is_valid_seq_4(uni)) {
            src += 4;
            uni = byte_load_4(src);
        }
        if (unlikely(pos == src)) {
            return_err(src, "invalid UTF-8 encoding in string");
        }
        goto skip_ascii;
    }
    
    /* The escape character appears, we need to copy it. */
    dst = src;
copy_escape:
    if (likely(*src == '\\')) {
        switch (*++src) {
            case '"':  *dst++ = '"';  src++; break;
            case '\\': *dst++ = '\\'; src++; break;
            case '/':  *dst++ = '/';  src++; break;
            case 'b':  *dst++ = '\b'; src++; break;
            case 'f':  *dst++ = '\f'; src++; break;
            case 'n':  *dst++ = '\n'; src++; break;
            case 'r':  *dst++ = '\r'; src++; break;
            case 't':  *dst++ = '\t'; src++; break;
            case 'u':
                if (unlikely(!read_hex_u16(++src, &hi))) {
                    return_err(src - 2, "invalid escaped unicode in string");
                }
                src += 4;
                if (likely((hi & 0xF800) != 0xD800)) {
                    /* a BMP character */
                    if (hi >= 0x800) {
                        *dst++ = (u8)(0xE0 | (hi >> 12));
                        *dst++ = (u8)(0x80 | ((hi >> 6) & 0x3F));
                        *dst++ = (u8)(0x80 | (hi & 0x3F));
                    } else if (hi >= 0x80) {
                        *dst++ = (u8)(0xC0 | (hi >> 6));
                        *dst++ = (u8)(0x80 | (hi & 0x3F));
                    } else {
                        *dst++ = (u8)hi;
                    }
                } else {
                    /* a non-BMP character, represented as a surrogate pair */
                    if (unlikely((hi & 0xFC00) != 0xD800)) {
                        return_err(src - 6, "invalid high surrogate in string");
                    }
                    if (unlikely(!byte_match_2(src, "\\u")) ||
                        unlikely(!read_hex_u16(src + 2, &lo))) {
                        return_err(src, "no matched low surrogate in string");
                    }
                    if (unlikely((lo & 0xFC00) != 0xDC00)) {
                        return_err(src, "invalid low surrogate in string");
                    }
                    uni = ((((u32)hi - 0xD800) << 10) |
                            ((u32)lo - 0xDC00)) + 0x10000;
                    *dst++ = (u8)(0xF0 | (uni >> 18));
                    *dst++ = (u8)(0x80 | ((uni >> 12) & 0x3F));
                    *dst++ = (u8)(0x80 | ((uni >> 6) & 0x3F));
                    *dst++ = (u8)(0x80 | (uni & 0x3F));
                    src += 6;
                }
                break;
            default: return_err(src, "invalid escaped character in string");
        }
    } else if (likely(*src == '"')) {
        val->tag = ((u64)(dst - cur) << YYJSON_TAG_BIT) | YYJSON_TYPE_STR;
        val->uni.str = (const char *)cur;
        *dst = '\0';
        *end = src + 1;
        return true;
    } else {
        return_err(src, "unexpected control character in string");
    }
    
copy_ascii:
    /*
     Copy continuous ASCII, loop unrolling, same as the following code:
     
         while (true) repeat16({
            if (unlikely(char_is_ascii_stop(*src))) break;
            *dst++ = *src++;
         });
     */
#if yyjson_is_real_gcc
#   define expr_jump(i) \
    if (likely(!(char_is_ascii_stop(src[i])))) {} \
    else { __asm volatile("":"=m"(src[i])::); goto copy_ascii_stop_##i; }
#else
#   define expr_jump(i) \
    if (likely(!(char_is_ascii_stop(src[i])))) {} \
    else { goto copy_ascii_stop_##i; }
#endif
    repeat16_incr(expr_jump);
#undef expr_jump
    
    byte_move_16(dst, src);
    src += 16;
    dst += 16;
    goto copy_ascii;
    
copy_ascii_stop_0:
    goto copy_utf8;
copy_ascii_stop_1:
    byte_move_2(dst, src);
    src += 1;
    dst += 1;
    goto copy_utf8;
copy_ascii_stop_2:
    byte_move_2(dst, src);
    src += 2;
    dst += 2;
    goto copy_utf8;
copy_ascii_stop_3:
    byte_move_4(dst, src);
    src += 3;
    dst += 3;
    goto copy_utf8;
copy_ascii_stop_4:
    byte_move_4(dst, src);
    src += 4;
    dst += 4;
    goto copy_utf8;
copy_ascii_stop_5:
    byte_move_4(dst, src);
    byte_move_2(dst + 4, src + 4);
    src += 5;
    dst += 5;
    goto copy_utf8;
copy_ascii_stop_6:
    byte_move_4(dst, src);
    byte_move_2(dst + 4, src + 4);
    src += 6;
    dst += 6;
    goto copy_utf8;
copy_ascii_stop_7:
    byte_move_8(dst, src);
    src += 7;
    dst += 7;
    goto copy_utf8;
copy_ascii_stop_8:
    byte_move_8(dst, src);
    src += 8;
    dst += 8;
    goto copy_utf8;
copy_ascii_stop_9:
    byte_move_8(dst, src);
    byte_move_2(dst + 8, src + 8);
    src += 9;
    dst += 9;
    goto copy_utf8;
copy_ascii_stop_10:
    byte_move_8(dst, src);
    byte_move_2(dst + 8, src + 8);
    src += 10;
    dst += 10;
    goto copy_utf8;
copy_ascii_stop_11:
    byte_move_8(dst, src);
    byte_move_4(dst + 8, src + 8);
    src += 11;
    dst += 11;
    goto copy_utf8;
copy_ascii_stop_12:
    byte_move_8(dst, src);
    byte_move_4(dst + 8, src + 8);
    src += 12;
    dst += 12;
    goto copy_utf8;
copy_ascii_stop_13:
    byte_move_8(dst, src);
    byte_move_4(dst + 8, src + 8);
    byte_move_2(dst + 12, src + 12);
    src += 13;
    dst += 13;
    goto copy_utf8;
copy_ascii_stop_14:
    byte_move_8(dst, src);
    byte_move_4(dst + 8, src + 8);
    byte_move_2(dst + 12, src + 12);
    src += 14;
    dst += 14;
    goto copy_utf8;
copy_ascii_stop_15:
    byte_move_16(dst, src);
    src += 15;
    dst += 15;
    goto copy_utf8;
    
copy_utf8:
    if (*src & 0x80) { /* non-ASCII character */
        pos = src;
        uni = byte_load_4(src);
        while (is_valid_seq_3(uni)) {
            byte_move_4(dst, &uni);
            dst += 3;
            src += 3;
            uni = byte_load_4(src);
        }
        if (is_valid_seq_1(uni)) goto copy_ascii;
        while (is_valid_seq_2(uni)) {
            byte_move_2(dst, &uni);
            dst += 2;
            src += 2;
            uni = byte_load_4(src);
        }
        while (is_valid_seq_4(uni)) {
            byte_move_4(dst, &uni);
            dst += 4;
            src += 4;
            uni = byte_load_4(src);
        }
        if (unlikely(pos == src)) {
            return_err(src, "invalid UTF-8 encoding in string");
        }
        goto copy_ascii;
    }
    goto copy_escape;
    
#undef return_err
#undef is_valid_seq_1
#undef is_valid_seq_2
#undef is_valid_seq_3
#undef is_valid_seq_4
}



/*==============================================================================
 * JSON Reader Implementation
 *
 * We use goto statements to build the finite state machine (FSM).
 * The FSM's state was hold by program counter (PC) and the 'goto' make the
 * state transitions.
 *============================================================================*/

/** Read single value JSON document. */
static_noinline yyjson_doc *read_root_single(u8 *hdr,
                                             u8 *cur,
                                             u8 *end,
                                             yyjson_alc alc,
                                             yyjson_read_flag flg,
                                             yyjson_read_err *err) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)
    
#define return_err(_pos, _code, _msg) do { \
    if (_pos >= end) { \
        err->pos = end - hdr; \
        err->code = YYJSON_READ_ERROR_UNEXPECTED_END; \
        err->msg = "unexpected end of data"; \
    } else { \
        err->pos = _pos - hdr; \
        err->code = YYJSON_READ_ERROR_##_code; \
        err->msg = _msg; \
    } \
    if (val_hdr) alc.free(alc.ctx, (void *)val_hdr); \
    return NULL; \
} while(false)
    
    usize hdr_len; /* value count used by doc */
    usize alc_num; /* value count capacity */
    yyjson_val *val_hdr; /* the head of allocated values */
    yyjson_val *val; /* current value */
    yyjson_doc *doc; /* the JSON document, equals to val_hdr */
    const char *msg; /* error message */
    
    hdr_len = sizeof(yyjson_doc) / sizeof(yyjson_val);
    hdr_len += (sizeof(yyjson_doc) % sizeof(yyjson_val)) > 0;
    alc_num = hdr_len + 1; /* single value */
    
    val_hdr = (yyjson_val *)alc.malloc(alc.ctx, alc_num * sizeof(yyjson_val));
    if (unlikely(!val_hdr)) goto fail_alloc;
    val = val_hdr + hdr_len;
    
    if (char_is_number(*cur)) {
        if (likely(read_number(cur, &cur, flg, val, &msg))) goto doc_end;
        goto fail_number;
    }
    if (*cur == '"') {
        if (likely(read_string(cur, &cur, val, &msg))) goto doc_end;
        goto fail_string;
    }
    if (*cur == 't') {
        if (likely(read_true(cur, &cur, val))) goto doc_end;
        goto fail_literal;
    }
    if (*cur == 'f') {
        if (likely(read_false(cur, &cur, val))) goto doc_end;
        goto fail_literal;
    }
    if (*cur == 'n') {
        if (likely(read_null(cur, &cur, val))) goto doc_end;
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_nan(false, cur, &cur, val)) goto doc_end;
        }
        goto fail_literal;
    }
    if (has_flag(ALLOW_INF_AND_NAN)) {
        if (read_inf_or_nan(false, cur, &cur, val)) goto doc_end;
    }
    goto fail_character;
    
doc_end:
    /* check invalid contents after json document */
    if (unlikely(cur < end) && !has_flag(STOP_WHEN_DONE)) {
#if !YYJSON_DISABLE_COMMENT_READER
        if (has_flag(ALLOW_COMMENTS)) {
            if (!skip_spaces_and_comments(cur, &cur)) {
                if (byte_match_2(cur, "/*")) goto fail_comment;
            }
        } else while(char_is_space(*cur)) cur++;
#else
        while(char_is_space(*cur)) cur++;
#endif
        if (unlikely(cur < end)) goto fail_garbage;
    }
    
    doc = (yyjson_doc *)val_hdr;
    doc->root = val_hdr + hdr_len;
    doc->alc = alc;
    doc->dat_read = cur - hdr;
    doc->val_read = 1;
    doc->str_pool = has_flag(INSITU) ? NULL : (char *)hdr;
    return doc;
    
fail_string:
    return_err(cur, INVALID_STRING, msg);
fail_number:
    return_err(cur, INVALID_NUMBER, msg);
fail_alloc:
    return_err(cur, MEMORY_ALLOCATION, "memory allocation failed");
fail_literal:
    return_err(cur, LITERAL, "invalid literal");
fail_comment:
    return_err(cur, INVALID_COMMENT, "unclosed multiline comment");
fail_character:
    return_err(cur, UNEXPECTED_CHARACTER, "unexpected character");
fail_garbage:
    return_err(cur, UNEXPECTED_CONTENT, "unexpected content after document");
    
#undef has_flag
#undef return_err
}

/** Read JSON document (optimized for minify). */
static_inline yyjson_doc *read_root_minify(u8 *hdr,
                                           u8 *cur,
                                           u8 *end,
                                           yyjson_alc alc,
                                           yyjson_read_flag flg,
                                           yyjson_read_err *err) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)

#define return_err(_pos, _code, _msg) do { \
    if (_pos >= end) { \
        err->pos = end - hdr; \
        err->code = YYJSON_READ_ERROR_UNEXPECTED_END; \
        err->msg = "unexpected end of data"; \
    } else { \
        err->pos = _pos - hdr; \
        err->code = YYJSON_READ_ERROR_##_code; \
        err->msg = _msg; \
    } \
    if (val_hdr) alc.free(alc.ctx, (void *)val_hdr); \
    return NULL; \
} while(false)
    
#define val_incr() do { \
    val++; \
    if (unlikely(val >= val_end)) { \
        alc_len += alc_len / 2; \
        if ((alc_len >= alc_max)) goto fail_alloc; \
        val_tmp = (yyjson_val *)alc.realloc(alc.ctx, (void *)val_hdr, \
            alc_len * sizeof(yyjson_val)); \
        if ((!val_tmp)) goto fail_alloc; \
        val = val_tmp + (size_t)(val - val_hdr); \
        ctn = val_tmp + (size_t)(ctn - val_hdr); \
        val_hdr = val_tmp; \
        val_end = val_tmp + (alc_len - 2); \
    } \
} while(false)
    
    usize dat_len; /* data length in bytes, hint for allocator */
    usize hdr_len; /* value count used by yyjson_doc */
    usize alc_len; /* value count allocated */
    usize alc_max; /* maximum value count for allocator */
    usize ctn_len; /* the number of elements in current container */
    yyjson_val *val_hdr; /* the head of allocated values */
    yyjson_val *val_end; /* the end of allocated values */
    yyjson_val *val_tmp; /* temporary pointer for realloc */
    yyjson_val *val; /* current JSON value */
    yyjson_val *ctn; /* current container */
    yyjson_val *ctn_parent; /* parent of current container */
    yyjson_doc *doc; /* the JSON document, equals to val_hdr */
    const char *msg; /* error message */
    
    dat_len = has_flag(STOP_WHEN_DONE) ? 256 : (end - cur);
    hdr_len = sizeof(yyjson_doc) / sizeof(yyjson_val);
    hdr_len += (sizeof(yyjson_doc) % sizeof(yyjson_val)) > 0;
    alc_max = USIZE_MAX / sizeof(yyjson_val);
    alc_len = hdr_len + (dat_len / YYJSON_READER_ESTIMATED_MINIFY_RATIO) + 4;
    alc_len = yyjson_min(alc_len, alc_max);
    
    val_hdr = (yyjson_val *)alc.malloc(alc.ctx, alc_len * sizeof(yyjson_val));
    if (unlikely(!val_hdr)) goto fail_alloc;
    val_end = val_hdr + (alc_len - 2); /* padding for key-value pair reading */
    val = val_hdr + hdr_len;
    ctn = val;
    ctn_len = 0;

    if (*cur++ == '{') {
        ctn->tag = YYJSON_TYPE_OBJ;
        ctn->uni.ofs = 0;
        goto obj_key_begin;
    } else {
        ctn->tag = YYJSON_TYPE_ARR;
        ctn->uni.ofs = 0;
        goto arr_val_begin;
    }
    
arr_begin:
    /* save current container */
    ctn->tag = (((u64)ctn_len + 1) << YYJSON_TAG_BIT) |
               (ctn->tag & YYJSON_TAG_MASK);
    
    /* create a new array value, save parent container offset */
    val_incr();
    val->tag = YYJSON_TYPE_ARR;
    val->uni.ofs = (u8 *)val - (u8 *)ctn;
    
    /* push the new array value as current container */
    ctn = val;
    ctn_len = 0;
    
arr_val_begin:
    if (*cur == '{') {
        cur++;
        goto obj_begin;
    }
    if (*cur == '[') {
        cur++;
        goto arr_begin;
    }
    if (char_is_number(*cur)) {
        val_incr();
        ctn_len++;
        if (likely(read_number(cur, &cur, flg, val, &msg))) goto arr_val_end;
        goto fail_number;
    }
    if (*cur == '"') {
        val_incr();
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto arr_val_end;
        goto fail_string;
    }
    if (*cur == 't') {
        val_incr();
        ctn_len++;
        if (likely(read_true(cur, &cur, val))) goto arr_val_end;
        goto fail_literal;
    }
    if (*cur == 'f') {
        val_incr();
        ctn_len++;
        if (likely(read_false(cur, &cur, val))) goto arr_val_end;
        goto fail_literal;
    }
    if (*cur == 'n') {
        val_incr();
        ctn_len++;
        if (likely(read_null(cur, &cur, val))) goto arr_val_end;
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_nan(false, cur, &cur, val)) goto arr_val_end;
        }
        goto fail_literal;
    }
    if (*cur == ']') {
        cur++;
        if (likely(ctn_len == 0)) goto arr_end;
        if (has_flag(ALLOW_TRAILING_COMMAS)) goto arr_end;
        goto fail_trailing_comma;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto arr_val_begin;
    }
    if (has_flag(ALLOW_INF_AND_NAN) && *cur == 'N') {
        val_incr();
        ctn_len++;
        if (read_inf_or_nan(false, cur, &cur, val)) goto arr_val_end;
        goto fail_character;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto arr_val_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
arr_val_end:
    if (*cur == ',') {
        cur++;
        goto arr_val_begin;
    }
    if (*cur == ']') {
        cur++;
        goto arr_end;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto arr_val_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto arr_val_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
arr_end:
    /* get parent container */
    ctn_parent = (yyjson_val *)((u8 *)ctn - ctn->uni.ofs);
    
    /* save the next sibling value offset */
    ctn->uni.ofs = (u8 *)val - (u8 *)ctn + sizeof(yyjson_val);
    ctn->tag = ((ctn_len) << YYJSON_TAG_BIT) | YYJSON_TYPE_ARR;
    if (unlikely(ctn == ctn_parent)) goto doc_end;
    
    /* pop parent as current container */
    ctn = ctn_parent;
    ctn_len = (usize)(ctn->tag >> YYJSON_TAG_BIT);
    if ((ctn->tag & YYJSON_TYPE_MASK) == YYJSON_TYPE_OBJ) {
        goto obj_val_end;
    } else {
        goto arr_val_end;
    }
    
obj_begin:
    /* push container */
    ctn->tag = (((u64)ctn_len + 1) << YYJSON_TAG_BIT) |
               (ctn->tag & YYJSON_TAG_MASK);
    val_incr();
    val->tag = YYJSON_TYPE_OBJ;
    /* offset to the parent */
    val->uni.ofs = (u8 *)val - (u8 *)ctn;
    ctn = val;
    ctn_len = 0;
    
obj_key_begin:
    if (likely(*cur == '"')) {
        val_incr();
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto obj_key_end;
        goto fail_string;
    }
    if (likely(*cur == '}')) {
        cur++;
        if (likely(ctn_len == 0)) goto obj_end;
        if (has_flag(ALLOW_TRAILING_COMMAS)) goto obj_end;
        goto fail_trailing_comma;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_key_begin;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_key_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_key_end:
    if (*cur == ':') {
        cur++;
        goto obj_val_begin;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_key_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_key_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_val_begin:
    if (*cur == '"') {
        val++;
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto obj_val_end;
        goto fail_string;
    }
    if (char_is_number(*cur)) {
        val++;
        ctn_len++;
        if (likely(read_number(cur, &cur, flg, val, &msg))) goto obj_val_end;
        goto fail_number;
    }
    if (*cur == '{') {
        cur++;
        goto obj_begin;
    }
    if (*cur == '[') {
        cur++;
        goto arr_begin;
    }
    if (*cur == 't') {
        val++;
        ctn_len++;
        if (likely(read_true(cur, &cur, val))) goto obj_val_end;
        goto fail_literal;
    }
    if (*cur == 'f') {
        val++;
        ctn_len++;
        if (likely(read_false(cur, &cur, val))) goto obj_val_end;
        goto fail_literal;
    }
    if (*cur == 'n') {
        val++;
        ctn_len++;
        if (likely(read_null(cur, &cur, val))) goto obj_val_end;
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_nan(false, cur, &cur, val)) goto obj_val_end;
        }
        goto fail_literal;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_val_begin;
    }
    if (has_flag(ALLOW_INF_AND_NAN) && *cur == 'N') {
        val++;
        ctn_len++;
        if (read_inf_or_nan(false, cur, &cur, val)) goto obj_val_end;
        goto fail_character;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_val_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_val_end:
    if (likely(*cur == ',')) {
        cur++;
        goto obj_key_begin;
    }
    if (likely(*cur == '}')) {
        cur++;
        goto obj_end;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_val_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_val_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_end:
    /* pop container */
    ctn_parent = (yyjson_val *)((u8 *)ctn - ctn->uni.ofs);
    /* point to the next value */
    ctn->uni.ofs = (u8 *)val - (u8 *)ctn + sizeof(yyjson_val);
    ctn->tag = (ctn_len << (YYJSON_TAG_BIT - 1)) | YYJSON_TYPE_OBJ;
    if (unlikely(ctn == ctn_parent)) goto doc_end;
    ctn = ctn_parent;
    ctn_len = (usize)(ctn->tag >> YYJSON_TAG_BIT);
    if ((ctn->tag & YYJSON_TYPE_MASK) == YYJSON_TYPE_OBJ) {
        goto obj_val_end;
    } else {
        goto arr_val_end;
    }
    
doc_end:
    
    /* check invalid contents after json document */
    if (unlikely(cur < end) && !has_flag(STOP_WHEN_DONE)) {
#if !YYJSON_DISABLE_COMMENT_READER
        if (has_flag(ALLOW_COMMENTS)) skip_spaces_and_comments(cur, &cur); else
#endif
        while(char_is_space(*cur)) cur++;
        if (unlikely(cur < end)) goto fail_garbage;
    }
    
    doc = (yyjson_doc *)val_hdr;
    doc->root = val_hdr + hdr_len;
    doc->alc = alc;
    doc->dat_read = cur - hdr;
    doc->val_read = (val - doc->root) + 1;
    doc->str_pool = has_flag(INSITU) ? NULL : (char *)hdr;
    return doc;
    
fail_string:
    return_err(cur, INVALID_STRING, msg);
fail_number:
    return_err(cur, INVALID_NUMBER, msg);
fail_alloc:
    return_err(cur, MEMORY_ALLOCATION, "memory allocation failed");
fail_trailing_comma:
    return_err(cur, JSON_STRUCTURE, "trailing comma is not allowed");
fail_literal:
    return_err(cur, LITERAL, "invalid literal");
fail_comment:
    return_err(cur, INVALID_COMMENT, "unclosed multiline comment");
fail_character:
    return_err(cur, UNEXPECTED_CHARACTER, "unexpected character");
fail_garbage:
    return_err(cur, UNEXPECTED_CONTENT, "unexpected content after document");
    
#undef has_flag
#undef val_incr
#undef return_err
}

/** Read JSON document (optimized for pretty). */
static_inline yyjson_doc *read_root_pretty(u8 *hdr,
                                           u8 *cur,
                                           u8 *end,
                                           yyjson_alc alc,
                                           yyjson_read_flag flg,
                                           yyjson_read_err *err) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)

#define return_err(_pos, _code, _msg) do { \
    if (_pos >= end) { \
        err->pos = end - hdr; \
        err->code = YYJSON_READ_ERROR_UNEXPECTED_END; \
        err->msg = "unexpected end of data"; \
    } else { \
        err->pos = _pos - hdr; \
        err->code = YYJSON_READ_ERROR_##_code; \
        err->msg = _msg; \
    } \
    if (val_hdr) alc.free(alc.ctx, (void *)val_hdr); \
    return NULL; \
} while(false)
    
#define val_incr() do { \
    val++; \
    if (unlikely(val >= val_end)) { \
        alc_len += alc_len / 2; \
        if ((alc_len >= alc_max)) goto fail_alloc; \
        val_tmp = (yyjson_val *)alc.realloc(alc.ctx, (void *)val_hdr, \
            alc_len * sizeof(yyjson_val)); \
        if ((!val_tmp)) goto fail_alloc; \
        val = val_tmp + (size_t)(val - val_hdr); \
        ctn = val_tmp + (size_t)(ctn - val_hdr); \
        val_hdr = val_tmp; \
        val_end = val_tmp + (alc_len - 2); \
    } \
} while(false)
    
    usize dat_len; /* data length in bytes, hint for allocator */
    usize hdr_len; /* value count used by yyjson_doc */
    usize alc_len; /* value count allocated */
    usize alc_max; /* maximum value count for allocator */
    usize ctn_len; /* the number of elements in current container */
    yyjson_val *val_hdr; /* the head of allocated values */
    yyjson_val *val_end; /* the end of allocated values */
    yyjson_val *val_tmp; /* temporary pointer for realloc */
    yyjson_val *val; /* current JSON value */
    yyjson_val *ctn; /* current container */
    yyjson_val *ctn_parent; /* parent of current container */
    yyjson_doc *doc; /* the JSON document, equals to val_hdr */
    const char *msg; /* error message */
    
    dat_len = has_flag(STOP_WHEN_DONE) ? 256 : (end - cur);
    hdr_len = sizeof(yyjson_doc) / sizeof(yyjson_val);
    hdr_len += (sizeof(yyjson_doc) % sizeof(yyjson_val)) > 0;
    alc_max = USIZE_MAX / sizeof(yyjson_val);
    alc_len = hdr_len + (dat_len / YYJSON_READER_ESTIMATED_PRETTY_RATIO) + 4;
    alc_len = yyjson_min(alc_len, alc_max);
    
    val_hdr = (yyjson_val *)alc.malloc(alc.ctx, alc_len * sizeof(yyjson_val));
    if (unlikely(!val_hdr)) goto fail_alloc;
    val_end = val_hdr + (alc_len - 2); /* padding for key-value pair reading */
    val = val_hdr + hdr_len;
    ctn = val;
    ctn_len = 0;
    
    if (*cur++ == '{') {
        ctn->tag = YYJSON_TYPE_OBJ;
        ctn->uni.ofs = 0;
        if (*cur == '\n') cur++;
        goto obj_key_begin;
    } else {
        ctn->tag = YYJSON_TYPE_ARR;
        ctn->uni.ofs = 0;
        if (*cur == '\n') cur++;
        goto arr_val_begin;
    }
    
arr_begin:
    /* save current container */
    ctn->tag = (((u64)ctn_len + 1) << YYJSON_TAG_BIT) |
               (ctn->tag & YYJSON_TAG_MASK);
    
    /* create a new array value, save parent container offset */
    val_incr();
    val->tag = YYJSON_TYPE_ARR;
    val->uni.ofs = (u8 *)val - (u8 *)ctn;
    
    /* push the new array value as current container */
    ctn = val;
    ctn_len = 0;
    if (*cur == '\n') cur++;
    
arr_val_begin:
    while (true) repeat16({
        if (likely(byte_match_2(cur, "  "))) cur += 2;
        else break;
    });
    if (*cur == '{') {
        cur++;
        goto obj_begin;
    }
    if (*cur == '[') {
        cur++;
        goto arr_begin;
    }
    if (char_is_number(*cur)) {
        val_incr();
        ctn_len++;
        if (likely(read_number(cur, &cur, flg, val, &msg))) goto arr_val_end;
        goto fail_number;
    }
    if (*cur == '"') {
        val_incr();
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto arr_val_end;
        goto fail_string;
    }
    if (*cur == 't') {
        val_incr();
        ctn_len++;
        if (likely(read_true(cur, &cur, val))) goto arr_val_end;
        goto fail_literal;
    }
    if (*cur == 'f') {
        val_incr();
        ctn_len++;
        if (likely(read_false(cur, &cur, val))) goto arr_val_end;
        goto fail_literal;
    }
    if (*cur == 'n') {
        val_incr();
        ctn_len++;
        if (likely(read_null(cur, &cur, val))) goto arr_val_end;
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_nan(false, cur, &cur, val)) goto arr_val_end;
        }
        goto fail_literal;
    }
    if (*cur == ']') {
        cur++;
        if (likely(ctn_len == 0)) goto arr_end;
        if (has_flag(ALLOW_TRAILING_COMMAS)) goto arr_end;
        goto fail_trailing_comma;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto arr_val_begin;
    }
    if (has_flag(ALLOW_INF_AND_NAN) && *cur == 'N') {
        val_incr();
        ctn_len++;
        if (read_inf_or_nan(false, cur, &cur, val)) goto arr_val_end;
        goto fail_character;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto arr_val_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
arr_val_end:
    if (byte_match_2(cur, ",\n")) {
        cur += 2;
        goto arr_val_begin;
    }
    if (*cur == ',') {
        cur++;
        goto arr_val_begin;
    }
    if (*cur == ']') {
        cur++;
        goto arr_end;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto arr_val_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto arr_val_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
arr_end:
    /* get parent container */
    ctn_parent = (yyjson_val *)((u8 *)ctn - ctn->uni.ofs);
    
    /* save the next sibling value offset */
    ctn->uni.ofs = (u8 *)val - (u8 *)ctn + sizeof(yyjson_val);
    ctn->tag = ((ctn_len) << YYJSON_TAG_BIT) | YYJSON_TYPE_ARR;
    if (unlikely(ctn == ctn_parent)) goto doc_end;
    
    /* pop parent as current container */
    ctn = ctn_parent;
    ctn_len = (usize)(ctn->tag >> YYJSON_TAG_BIT);
    if (*cur == '\n') cur++;
    if ((ctn->tag & YYJSON_TYPE_MASK) == YYJSON_TYPE_OBJ) {
        goto obj_val_end;
    } else {
        goto arr_val_end;
    }
    
obj_begin:
    /* push container */
    ctn->tag = (((u64)ctn_len + 1) << YYJSON_TAG_BIT) |
               (ctn->tag & YYJSON_TAG_MASK);
    val_incr();
    val->tag = YYJSON_TYPE_OBJ;
    /* offset to the parent */
    val->uni.ofs = (u8 *)val - (u8 *)ctn;
    ctn = val;
    ctn_len = 0;
    if (*cur == '\n') cur++;
    
obj_key_begin:
    while (true) repeat16({
        if (likely(byte_match_2(cur, "  "))) cur += 2;
        else break;
    });
    if (likely(*cur == '"')) {
        val_incr();
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto obj_key_end;
        goto fail_string;
    }
    if (likely(*cur == '}')) {
        cur++;
        if (likely(ctn_len == 0)) goto obj_end;
        if (has_flag(ALLOW_TRAILING_COMMAS)) goto obj_end;
        goto fail_trailing_comma;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_key_begin;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_key_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_key_end:
    if (byte_match_2(cur, ": ")) {
        cur += 2;
        goto obj_val_begin;
    }
    if (*cur == ':') {
        cur++;
        goto obj_val_begin;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_key_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_key_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_val_begin:
    if (*cur == '"') {
        val++;
        ctn_len++;
        if (likely(read_string(cur, &cur, val, &msg))) goto obj_val_end;
        goto fail_string;
    }
    if (char_is_number(*cur)) {
        val++;
        ctn_len++;
        if (likely(read_number(cur, &cur, flg, val, &msg))) goto obj_val_end;
        goto fail_number;
    }
    if (*cur == '{') {
        cur++;
        goto obj_begin;
    }
    if (*cur == '[') {
        cur++;
        goto arr_begin;
    }
    if (*cur == 't') {
        val++;
        ctn_len++;
        if (likely(read_true(cur, &cur, val))) goto obj_val_end;
        goto fail_literal;
    }
    if (*cur == 'f') {
        val++;
        ctn_len++;
        if (likely(read_false(cur, &cur, val))) goto obj_val_end;
        goto fail_literal;
    }
    if (*cur == 'n') {
        val++;
        ctn_len++;
        if (likely(read_null(cur, &cur, val))) goto obj_val_end;
        if (has_flag(ALLOW_INF_AND_NAN)) {
            if (read_nan(false, cur, &cur, val)) goto obj_val_end;
        }
        goto fail_literal;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_val_begin;
    }
    if (has_flag(ALLOW_INF_AND_NAN) && *cur == 'N') {
        val++;
        ctn_len++;
        if (read_inf_or_nan(false, cur, &cur, val)) goto obj_val_end;
        goto fail_character;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_val_begin;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_val_end:
    if (byte_match_2(cur, ",\n")) {
        cur += 2;
        goto obj_key_begin;
    }
    if (likely(*cur == ',')) {
        cur++;
        goto obj_key_begin;
    }
    if (likely(*cur == '}')) {
        cur++;
        goto obj_end;
    }
    if (char_is_space(*cur)) {
        while(char_is_space(*++cur));
        goto obj_val_end;
    }
#if !YYJSON_DISABLE_COMMENT_READER
    if (has_flag(ALLOW_COMMENTS)) {
        if (skip_spaces_and_comments(cur, &cur)) goto obj_val_end;
        if (byte_match_2(cur, "/*")) goto fail_comment;
    }
#endif
    goto fail_character;
    
obj_end:
    /* pop container */
    ctn_parent = (yyjson_val *)((u8 *)ctn - ctn->uni.ofs);
    /* point to the next value */
    ctn->uni.ofs = (u8 *)val - (u8 *)ctn + sizeof(yyjson_val);
    ctn->tag = (ctn_len << (YYJSON_TAG_BIT - 1)) | YYJSON_TYPE_OBJ;
    if (unlikely(ctn == ctn_parent)) goto doc_end;
    ctn = ctn_parent;
    ctn_len = (usize)(ctn->tag >> YYJSON_TAG_BIT);
    if (*cur == '\n') cur++;
    if ((ctn->tag & YYJSON_TYPE_MASK) == YYJSON_TYPE_OBJ) {
        goto obj_val_end;
    } else {
        goto arr_val_end;
    }
    
doc_end:
    
    /* check invalid contents after json document */
    if (unlikely(cur < end) && !has_flag(STOP_WHEN_DONE)) {
#if !YYJSON_DISABLE_COMMENT_READER
        if (has_flag(ALLOW_COMMENTS)) skip_spaces_and_comments(cur, &cur); else
#endif
        while(char_is_space(*cur)) cur++;
        if (unlikely(cur < end)) goto fail_garbage;
    }
    
    doc = (yyjson_doc *)val_hdr;
    doc->root = val_hdr + hdr_len;
    doc->alc = alc;
    doc->dat_read = cur - hdr;
    doc->val_read = (val - val_hdr) - hdr_len + 1;
    doc->str_pool = has_flag(INSITU) ? NULL : (char *)hdr;
    return doc;
    
fail_string:
    return_err(cur, INVALID_STRING, msg);
fail_number:
    return_err(cur, INVALID_NUMBER, msg);
fail_alloc:
    return_err(cur, MEMORY_ALLOCATION, "memory allocation failed");
fail_trailing_comma:
    return_err(cur, JSON_STRUCTURE, "trailing comma is not allowed");
fail_literal:
    return_err(cur, LITERAL, "invalid literal");
fail_comment:
    return_err(cur, INVALID_COMMENT, "unclosed multiline comment");
fail_character:
    return_err(cur, UNEXPECTED_CHARACTER, "unexpected character");
fail_garbage:
    return_err(cur, UNEXPECTED_CONTENT, "unexpected content after document");

#undef has_flag
#undef val_incr
#undef return_err
}



/*==============================================================================
 * JSON Reader Entrance
 *============================================================================*/

yyjson_doc *yyjson_read_opts(char *dat,
                             usize len,
                             yyjson_read_flag flg,
                             yyjson_alc *alc_ptr,
                             yyjson_read_err *err) {
    
#define has_flag(_flag) unlikely((flg & YYJSON_READ_##_flag) != 0)
    
#define return_err(_pos, _code, _msg) do { \
    err->pos = _pos; \
    err->msg = _msg; \
    err->code = YYJSON_READ_ERROR_##_code; \
    if (!has_flag(INSITU) && hdr) alc.free(alc.ctx, (void *)hdr); \
    return NULL; \
} while(false)
    
    yyjson_read_err dummy_err;
    yyjson_alc alc;
    yyjson_doc *doc;
    u8 *hdr = NULL, *end, *cur;
    
    /* validate input parameters */
    if (!err) err = &dummy_err;
    if (likely(!alc_ptr)) {
        alc = YYJSON_DEFAULT_ALC;
    } else {
        alc = *alc_ptr;
    }
    if (unlikely(!dat)) {
        return_err(0, INVALID_PARAMETER, "input data is NULL");
    }
    if (unlikely(!len)) {
        return_err(0, INVALID_PARAMETER, "input length is 0");
    }
    
    /* add 4-byte zero padding for input data */
    if (!has_flag(INSITU)) {
        if (unlikely(len >= USIZE_MAX - PADDING_SIZE)) {
            return_err(0, MEMORY_ALLOCATION, "memory allocation failed");
        }
        hdr = (u8 *)alc.malloc(alc.ctx, len + PADDING_SIZE);
        if (unlikely(!hdr)) {
            return_err(0, MEMORY_ALLOCATION, "memory allocation failed");
        }
        end = hdr + len;
        cur = hdr;
        memcpy(hdr, dat, len);
        memset(end, 0, PADDING_SIZE);
    } else {
        hdr = (u8 *)dat;
        end = (u8 *)dat + len;
        cur = (u8 *)dat;
    }
    
    /* skip empty contents before json document */
    if (unlikely(char_is_space_or_comment(*cur))) {
#if !YYJSON_DISABLE_COMMENT_READER
        if (has_flag(ALLOW_COMMENTS)) {
            if (!skip_spaces_and_comments(cur, &cur)) {
                return_err(cur - hdr, INVALID_COMMENT,
                           "unclosed multiline comment");
            }
        } else {
            if (likely(char_is_space(*cur))) {
                while(char_is_space(*++cur));
            }
        }
#else
        if (likely(char_is_space(*cur))) {
            while(char_is_space(*++cur));
        }
#endif
        if (unlikely(cur >= end)) {
            return_err(0, EMPTY_CONTENT, "input data is empty");
        }
    }
    
    /* read json document */
    if (likely(char_is_container(*cur))) {
        if (char_is_space(cur[1]) && char_is_space(cur[2])) {
            doc = read_root_pretty(hdr, cur, end, alc, flg, err);
        } else {
            doc = read_root_minify(hdr, cur, end, alc, flg, err);
        }
    } else {
        doc = read_root_single(hdr, cur, end, alc, flg, err);
    }
    
    /* check result */
    if (likely(doc)) {
        memset(err, 0, sizeof(yyjson_read_err));
    } else {
        if (err->pos == 0 && err->code != YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
            if ((hdr[0] == 0xEF && hdr[1] == 0xBB && hdr[2] == 0xBF)) {
                err->msg = "byte order mark (BOM) is not supported";
            } else if ((hdr[0] == 0xFE && hdr[1] == 0xFF) ||
                       (hdr[0] == 0xFF && hdr[1] == 0xFE)) {
                err->msg = "UTF-16 encoding is not supported";
            } else if ((hdr[0] == 0x00 && hdr[1] == 0x00 &&
                        hdr[2] == 0xFE && hdr[3] == 0xFF) ||
                       (hdr[0] == 0xFF && hdr[1] == 0xFE &&
                        hdr[2] == 0x00 && hdr[3] == 0x00)) {
                err->msg = "UTF-32 encoding is not supported";
            }
        }
        if (!has_flag(INSITU) && hdr) alc.free(alc.ctx, (void *)hdr);
    }
    return doc;
    
#undef has_flag
#undef return_err
}

yyjson_doc *yyjson_read_file(const char *path,
                             yyjson_read_flag flg,
                             yyjson_alc *alc_ptr,
                             yyjson_read_err *err) {
    
#define return_err(_code, _msg) do { \
    err->pos = 0; \
    err->msg = _msg; \
    err->code = YYJSON_READ_ERROR_##_code; \
    if (file) fclose(file); \
    if (hdr) alc.free(alc.ctx, hdr); \
    return NULL; \
} while(false)
    
    yyjson_read_err dummy_err;
    yyjson_alc alc;
    yyjson_doc *doc;
    
    u8 *hdr = NULL;
    FILE *file = NULL;
    long file_size = 0;
    void *buf = NULL;
    usize buf_size = 0;
    
    /* validate input parameters */
    if (!err) err = &dummy_err;
    if (likely(!alc_ptr)) {
        alc = YYJSON_DEFAULT_ALC;
    } else {
        alc = *alc_ptr;
    }
    if (unlikely(!path)) {
        return_err(INVALID_PARAMETER, "input path is NULL");
    }
    
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, "rb") != 0) {
        return_err(FILE_OPEN, "file opening failed");
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) {
        return_err(FILE_OPEN, "file opening failed");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        return_err(FILE_READ, "file seeking failed");
    }
    file_size = ftell(file);
    if (file_size < 0) {
        return_err(FILE_READ, "file reading failed");
    }
    if (file_size == 0) {
        return_err(EMPTY_CONTENT, "input file is empty");
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        return_err(FILE_READ, "file seeking failed");
    }
    
    buf_size = (size_t)file_size + PADDING_SIZE;
    buf = alc.malloc(alc.ctx, buf_size);
    if (buf == NULL) {
        return_err(MEMORY_ALLOCATION, "fail to alloc memory");
    }
    
#if _MSC_VER >= 1400
    if (fread_s(buf, buf_size, file_size, 1, file) != 1) {
        return_err(FILE_READ, "file reading failed");
    }
#else
    if (fread(buf, file_size, 1, file) != 1) {
        return_err(FILE_READ, "file reading failed");
    }
#endif
    fclose(file);
    
    memset((u8 *)buf + file_size, 0, PADDING_SIZE);
    flg |= YYJSON_READ_INSITU;
    doc = yyjson_read_opts((char *)buf, file_size, flg, &alc, err);
    if (doc) {
        doc->str_pool = (char *)buf;
    } else {
        alc.free(alc.ctx, buf);
    }
    return doc;
    
#undef return_err
}

#endif /* YYJSON_DISABLE_READER */



#if !YYJSON_DISABLE_WRITER

/*==============================================================================
 * Integer Writer
 *
 * The maximum value of uint32_t is 4294967295 (10 digits),
 * these digits are named as 'aabbccddee' here.
 *
 * Although most compilers may convert the "division by constant value" into
 * "multiply and shift", manual conversion can still help some compilers
 * generate fewer and better instructions.
 *============================================================================*/

/** Digit table from 00 to 99. */
yyjson_align(2)
static const char digit_table[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4',
    '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
    '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
    '2', '0', '2', '1', '2', '2', '2', '3', '2', '4',
    '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',
    '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
    '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
    '5', '0', '5', '1', '5', '2', '5', '3', '5', '4',
    '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4',
    '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
    '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
    '8', '0', '8', '1', '8', '2', '8', '3', '8', '4',
    '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4',
    '9', '5', '9', '6', '9', '7', '9', '8', '9', '9'
};

static_inline u8 *write_u32_len_8(u32 val, u8 *buf) {
    /* 8 digits: aabbccdd */
    u32 aa, bb, cc, dd, aabb, ccdd;
    aabb = (u32)(((u64)val * 109951163) >> 40); /* (val / 10000) */
    ccdd = val - aabb * 10000; /* (val % 10000) */
    aa = (aabb * 5243) >> 19; /* (aabb / 100) */
    cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
    bb = aabb - aa * 100; /* (aabb % 100) */
    dd = ccdd - cc * 100; /* (ccdd % 100) */
    ((v16 *)buf)[0] = ((v16 *)digit_table)[aa];
    ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
    ((v16 *)buf)[2] = ((v16 *)digit_table)[cc];
    ((v16 *)buf)[3] = ((v16 *)digit_table)[dd];
    return buf + 8;
}

static_inline u8 *write_u32_len_4(u32 val, u8 *buf) {
    /* 4 digits: aabb */
    u32 aa, bb;
    aa = (val * 5243) >> 19; /* (val / 100) */
    bb = val - aa * 100; /* (val % 100) */
    ((v16 *)buf)[0] = ((v16 *)digit_table)[aa];
    ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
    return buf + 4;
}

static_inline u8 *write_u32_len_1_8(u32 val, u8 *buf) {
    u32 aa, bb, cc, dd, aabb, bbcc, ccdd, lz;
    
    if (val < 100) { /* 1-2 digits: aa */
        lz = val < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[val * 2 + lz]);
        buf -= lz;
        return buf + 2;
        
    } else if (val < 10000) { /* 3-4 digits: aabb */
        aa = (val * 5243) >> 19; /* (val / 100) */
        bb = val - aa * 100; /* (val % 100) */
        lz = aa < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[aa * 2 + lz]);
        buf -= lz;
        ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
        return buf + 4;
        
    } else if (val < 1000000) { /* 5-6 digits: aabbcc */
        aa = (u32)(((u64)val * 429497) >> 32); /* (val / 10000) */
        bbcc = val - aa * 10000; /* (val % 10000) */
        bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
        cc = bbcc - bb * 100; /* (bbcc % 100) */
        lz = aa < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[aa * 2 + lz]);
        buf -= lz;
        ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
        ((v16 *)buf)[2] = ((v16 *)digit_table)[cc];
        return buf + 6;
        
    } else { /* 7-8 digits: aabbccdd */
        aabb = (u32)(((u64)val * 109951163) >> 40); /* (val / 10000) */
        ccdd = val - aabb * 10000; /* (val % 10000) */
        aa = (aabb * 5243) >> 19; /* (aabb / 100) */
        cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
        bb = aabb - aa * 100; /* (aabb % 100) */
        dd = ccdd - cc * 100; /* (ccdd % 100) */
        lz = aa < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[aa * 2 + lz]);
        buf -= lz;
        ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
        ((v16 *)buf)[2] = ((v16 *)digit_table)[cc];
        ((v16 *)buf)[3] = ((v16 *)digit_table)[dd];
        return buf + 8;
    }
}

static_inline u8 *write_u64_len_5_8(u32 val, u8 *buf) {
    u32 aa, bb, cc, dd, aabb, bbcc, ccdd, lz;
    
    if (val < 1000000) { /* 5-6 digits: aabbcc */
        aa = (u32)(((u64)val * 429497) >> 32); /* (val / 10000) */
        bbcc = val - aa * 10000; /* (val % 10000) */
        bb = (bbcc * 5243) >> 19; /* (bbcc / 100) */
        cc = bbcc - bb * 100; /* (bbcc % 100) */
        lz = aa < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[aa * 2 + lz]);
        buf -= lz;
        ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
        ((v16 *)buf)[2] = ((v16 *)digit_table)[cc];
        return buf + 6;
        
    } else { /* 7-8 digits: aabbccdd */
        /* (val / 10000) */
        aabb = (u32)(((u64)val * 109951163) >> 40);
        ccdd = val - aabb * 10000; /* (val % 10000) */
        aa = (aabb * 5243) >> 19; /* (aabb / 100) */
        cc = (ccdd * 5243) >> 19; /* (ccdd / 100) */
        bb = aabb - aa * 100; /* (aabb % 100) */
        dd = ccdd - cc * 100; /* (ccdd % 100) */
        lz = aa < 10;
        ((v16 *)buf)[0] = *(v16 *)&(digit_table[aa * 2 + lz]);
        buf -= lz;
        ((v16 *)buf)[1] = ((v16 *)digit_table)[bb];
        ((v16 *)buf)[2] = ((v16 *)digit_table)[cc];
        ((v16 *)buf)[3] = ((v16 *)digit_table)[dd];
        return buf + 8;
    }
}

static_inline u8 *write_u64(u64 val, u8 *buf) {
    u64 tmp, hgh;
    u32 mid, low;
    
    if (val < 100000000) { /* 1-8 digits */
        buf = write_u32_len_1_8((u32)val, buf);
        return buf;
        
    } else if (val < (u64)100000000 * 100000000) { /* 9-16 digits */
        hgh = val / 100000000;
        low = (u32)(val - hgh * 100000000); /* (val % 100000000) */
        buf = write_u32_len_1_8((u32)hgh, buf);
        buf = write_u32_len_8(low, buf);
        return buf;
        
    } else { /* 17-20 digits */
        tmp = val / 100000000;
        low = (u32)(val - tmp * 100000000); /* (val % 100000000) */
        hgh = (u32)(tmp / 10000);
        mid = (u32)(tmp - hgh * 10000); /* (tmp % 10000) */
        buf = write_u64_len_5_8((u32)hgh, buf);
        buf = write_u32_len_4(mid, buf);
        buf = write_u32_len_8(low, buf);
        return buf;
    }
}



/*==============================================================================
 * Number Writer
 *============================================================================*/

#if !YYJSON_DISABLE_FP_WRITER

/** The significant bits kept in pow5_inv_sig_table. */
#define POW5_INV_SIG_BITS 122

/** The significant bits table of pow5 multiplicative inverse (4.5KB).
    (generate with misc/make_tables.c) */
static const u64 pow5_inv_sig_table[291][2] = {
    { U64(0x04000000, 0x00000000), U64(0x00000000, 0x00000001) },
    { U64(0x03333333, 0x33333333), U64(0x33333333, 0x33333334) },
    { U64(0x028F5C28, 0xF5C28F5C), U64(0x28F5C28F, 0x5C28F5C3) },
    { U64(0x020C49BA, 0x5E353F7C), U64(0xED916872, 0xB020C49C) },
    { U64(0x0346DC5D, 0x63886594), U64(0xAF4F0D84, 0x4D013A93) },
    { U64(0x029F16B1, 0x1C6D1E10), U64(0x8C3F3E03, 0x70CDC876) },
    { U64(0x0218DEF4, 0x16BDB1A6), U64(0xD698FE69, 0x270B06C5) },
    { U64(0x035AFE53, 0x5795E90A), U64(0xF0F4CA41, 0xD811A46E) },
    { U64(0x02AF31DC, 0x4611873B), U64(0xF3F70834, 0xACDAE9F1) },
    { U64(0x0225C17D, 0x04DAD296), U64(0x5CC5A02A, 0x23E254C1) },
    { U64(0x036F9BFB, 0x3AF7B756), U64(0xFAD5CD10, 0x396A2135) },
    { U64(0x02BFAFFC, 0x2F2C92AB), U64(0xFBDE3DA6, 0x9454E75E) },
    { U64(0x0232F330, 0x25BD4223), U64(0x2FE4FE1E, 0xDD10B918) },
    { U64(0x0384B84D, 0x092ED038), U64(0x4CA19697, 0xC81AC1BF) },
    { U64(0x02D09370, 0xD4257360), U64(0x3D4E1213, 0x067BCE33) },
    { U64(0x024075F3, 0xDCEAC2B3), U64(0x643E74DC, 0x052FD829) },
    { U64(0x039A5652, 0xFB113785), U64(0x6D30BAF9, 0xA1E626A7) },
    { U64(0x02E1DEA8, 0xC8DA92D1), U64(0x2426FBFA, 0xE7EB5220) },
    { U64(0x024E4BBA, 0x3A487574), U64(0x1CEBFCC8, 0xB9890E80) },
    { U64(0x03B07929, 0xF6DA5586), U64(0x94ACC7A7, 0x8F41B0CC) },
    { U64(0x02F39421, 0x9248446B), U64(0xAA23D2EC, 0x729AF3D7) },
    { U64(0x025C7681, 0x41D369EF), U64(0xBB4FDBF0, 0x5BAF2979) },
    { U64(0x03C72402, 0x02EBDCB2), U64(0xC54C931A, 0x2C4B758D) },
    { U64(0x0305B668, 0x02564A28), U64(0x9DD6DC14, 0xF03C5E0B) },
    { U64(0x026AF853, 0x3511D4ED), U64(0x4B1249AA, 0x59C9E4D6) },
    { U64(0x03DE5A1E, 0xBB4FBB15), U64(0x44EA0F76, 0xF60FD489) },
    { U64(0x03184818, 0x95D96277), U64(0x6A54D92B, 0xF80CAA07) },
    { U64(0x0279D346, 0xDE4781F9), U64(0x21DD7A89, 0x933D54D2) },
    { U64(0x03F61ED7, 0xCA0C0328), U64(0x362F2A75, 0xB8622150) },
    { U64(0x032B4BDF, 0xD4D668EC), U64(0xF825BB91, 0x604E810D) },
    { U64(0x0289097F, 0xDD7853F0), U64(0xC684960D, 0xE6A5340B) },
    { U64(0x02073ACC, 0xB12D0FF3), U64(0xD203AB3E, 0x521DC33C) },
    { U64(0x033EC47A, 0xB514E652), U64(0xE99F7863, 0xB696052C) },
    { U64(0x02989D2E, 0xF743EB75), U64(0x87B2C6B6, 0x2BAB3757) },
    { U64(0x0213B0F2, 0x5F69892A), U64(0xD2F56BC4, 0xEFBC2C45) },
    { U64(0x0352B4B6, 0xFF0F41DE), U64(0x1E55793B, 0x192D13A2) },
    { U64(0x02A89092, 0x65A5CE4B), U64(0x4B77942F, 0x475742E8) },
    { U64(0x022073A8, 0x515171D5), U64(0xD5F94359, 0x05DF68BA) },
    { U64(0x03671F73, 0xB54F1C89), U64(0x565B9EF4, 0xD6324129) },
    { U64(0x02B8E5F6, 0x2AA5B06D), U64(0xDEAFB25D, 0x78283421) },
    { U64(0x022D84C4, 0xEEEAF38B), U64(0x188C8EB1, 0x2CECF681) },
    { U64(0x037C07A1, 0x7E44B8DE), U64(0x8DADB11B, 0x7B14BD9B) },
    { U64(0x02C99FB4, 0x6503C718), U64(0x7157C0E2, 0xC8DD647C) },
    { U64(0x023AE629, 0xEA696C13), U64(0x8DDFCD82, 0x3A4AB6CA) },
    { U64(0x03917043, 0x10A8ACEC), U64(0x1632E269, 0xF6DDF142) },
    { U64(0x02DAC035, 0xA6ED5723), U64(0x44F581EE, 0x5F17F435) },
    { U64(0x024899C4, 0x858AAC1C), U64(0x372ACE58, 0x4C1329C4) },
    { U64(0x03A75C6D, 0xA27779C6), U64(0xBEAAE3C0, 0x79B842D3) },
    { U64(0x02EC49F1, 0x4EC5FB05), U64(0x65558300, 0x61603576) },
    { U64(0x0256A18D, 0xD89E626A), U64(0xB7779C00, 0x4DE6912B) },
    { U64(0x03BDCF49, 0x5A9703DD), U64(0xF258F99A, 0x163DB512) },
    { U64(0x02FE3F6D, 0xE212697E), U64(0x5B7A6148, 0x11CAF741) },
    { U64(0x0264FF8B, 0x1B41EDFE), U64(0xAF951AA0, 0x0E3BF901) },
    { U64(0x03D4CC11, 0xC5364997), U64(0x7F54F766, 0x7D2CC19B) },
    { U64(0x0310A341, 0x6A91D479), U64(0x32AA5F85, 0x30F09AE3) },
    { U64(0x0273B5CD, 0xEEDB1060), U64(0xF5551937, 0x5A5A1582) },
    { U64(0x03EC5616, 0x4AF81A34), U64(0xBBBB5B8B, 0xC3C3559D) },
    { U64(0x03237811, 0xD593482A), U64(0x2FC91609, 0x6969114A) },
    { U64(0x0282C674, 0xAADC39BB), U64(0x596DAB3A, 0xBABA743C) },
    { U64(0x0202385D, 0x557CFAFC), U64(0x478AEF62, 0x2EFB9030) },
    { U64(0x0336C095, 0x5594C4C6), U64(0xD8DE4BD0, 0x4B2C19E6) },
    { U64(0x029233AA, 0xAADD6A38), U64(0xAD7EA30D, 0x08F014B8) },
    { U64(0x020E8FBB, 0xBBE454FA), U64(0x24654F3D, 0xA0C01093) },
    { U64(0x034A7F92, 0xC63A2190), U64(0x3A3BB1FC, 0x346680EB) },
    { U64(0x02A1FFA8, 0x9E94E7A6), U64(0x94FC8E63, 0x5D1ECD89) },
    { U64(0x021B32ED, 0x4BAA52EB), U64(0xAA63A51C, 0x4A7F0AD4) },
    { U64(0x035EB7E2, 0x12AA1E45), U64(0xDD6C3B60, 0x7731AAED) },
    { U64(0x02B22CB4, 0xDBBB4B6B), U64(0x1789C919, 0xF8F488BD) },
    { U64(0x022823C3, 0xE2FC3C55), U64(0xAC6E3A7B, 0x2D906D64) },
    { U64(0x03736C6C, 0x9E606089), U64(0x13E390C5, 0x15B3E23A) },
    { U64(0x02C2BD23, 0xB1E6B3A0), U64(0xDCB60D6A, 0x77C31B62) },
    { U64(0x0235641C, 0x8E52294D), U64(0x7D5E7121, 0xF968E2B5) },
    { U64(0x0388A02D, 0xB0837548), U64(0xC8971B69, 0x8F0E3787) },
    { U64(0x02D3B357, 0xC0692AA0), U64(0xA078E2BA, 0xD8D82C6C) },
    { U64(0x0242F5DF, 0xCD20EEE6), U64(0xE6C71BC8, 0xAD79BD24) },
    { U64(0x039E5632, 0xE1CE4B0B), U64(0x0AD82C74, 0x48C2C839) },
    { U64(0x02E511C2, 0x4E3EA26F), U64(0x3BE02390, 0x3A356CFA) },
    { U64(0x0250DB01, 0xD8321B8C), U64(0x2FE682D9, 0xC82ABD95) },
    { U64(0x03B4919C, 0x8D1CF8E0), U64(0x4CA4048F, 0xA6AAC8EE) },
    { U64(0x02F6DAE3, 0xA4172D80), U64(0x3D5003A6, 0x1EEF0725) },
    { U64(0x025F1582, 0xE9AC2466), U64(0x9773361E, 0x7F259F51) },
    { U64(0x03CB559E, 0x42AD070A), U64(0x8BEB89CA, 0x6508FEE8) },
    { U64(0x0309114B, 0x688A6C08), U64(0x6FEFA16E, 0xB73A6586) },
    { U64(0x026DA76F, 0x86D52339), U64(0xF3261ABE, 0xF8FB846B) },
    { U64(0x03E2A57F, 0x3E21D1F6), U64(0x51D69131, 0x8E5F3A45) },
    { U64(0x031BB798, 0xFE8174C5), U64(0x0E4540F4, 0x71E5C837) },
    { U64(0x027C92E0, 0xCB9AC3D0), U64(0xD8376729, 0xF4B7D360) },
    { U64(0x03FA849A, 0xDF5E061A), U64(0xF38BD843, 0x21261EFF) },
    { U64(0x032ED07B, 0xE5E4D1AF), U64(0x293CAD02, 0x80EB4BFF) },
    { U64(0x028BD9FC, 0xB7EA4158), U64(0xEDCA2402, 0x00BC3CCC) },
    { U64(0x02097B30, 0x9321CDE0), U64(0xBE3B5001, 0x9A3030A4) },
    { U64(0x03425EB4, 0x1E9C7C9A), U64(0xC9F88002, 0x904D1A9F) },
    { U64(0x029B7EF6, 0x7EE396E2), U64(0x3B2D3335, 0x403DAEE6) },
    { U64(0x0215FF2B, 0x98B6124E), U64(0x95BDC291, 0x003158B8) },
    { U64(0x03566512, 0x8DF01D4A), U64(0x892F9DB4, 0xCD1BC126) },
    { U64(0x02AB840E, 0xD7F34AA2), U64(0x07594AF7, 0x0A7C9A85) },
    { U64(0x0222D00B, 0xDFF5D54E), U64(0x6C476F2C, 0x0863AED1) },
    { U64(0x036AE679, 0x66562217), U64(0x13A57EAC, 0xDA3917B4) },
    { U64(0x02BBEB94, 0x51DE81AC), U64(0x0FB7988A, 0x482DAC90) },
    { U64(0x022FEFA9, 0xDB1867BC), U64(0xD95FAD3B, 0x6CF156DA) },
    { U64(0x037FE5DC, 0x91C0A5FA), U64(0xF565E1F8, 0xAE4EF15C) },
    { U64(0x02CCB7E3, 0xA7CD5195), U64(0x911E4E60, 0x8B725AB0) },
    { U64(0x023D5FE9, 0x530AA7AA), U64(0xDA7EA51A, 0x0928488D) },
    { U64(0x03956642, 0x1E7772AA), U64(0xF7310829, 0xA8407415) },
    { U64(0x02DDEB68, 0x185F8EEF), U64(0x2C2739BA, 0xED005CDE) },
    { U64(0x024B22B9, 0xAD193F25), U64(0xBCEC2E2F, 0x24004A4B) },
    { U64(0x03AB6AC2, 0xAE8ECB6F), U64(0x94AD16B1, 0xD333AA11) },
    { U64(0x02EF889B, 0xBED8A2BF), U64(0xAA241227, 0xDC2954DB) },
    { U64(0x02593A16, 0x3246E899), U64(0x54E9A81F, 0xE35443E2) },
    { U64(0x03C1F689, 0xEA0B0DC2), U64(0x2175D9CC, 0x9EED396A) },
    { U64(0x03019207, 0xEE6F3E34), U64(0xE7917B0A, 0x18BDC788) },
    { U64(0x0267A806, 0x5858FE90), U64(0xB9412F3B, 0x46FE393A) },
    { U64(0x03D90CD6, 0xF3C1974D), U64(0xF535185E, 0xD7FD285C) },
    { U64(0x03140A45, 0x8FCE12A4), U64(0xC42A79E5, 0x7997537D) },
    { U64(0x02766E9E, 0x0CA4DBB7), U64(0x03552E51, 0x2E12A931) },
    { U64(0x03F0B0FC, 0xE107C5F1), U64(0x9EEEB081, 0xE3510EB4) },
    { U64(0x0326F3FD, 0x80D304C1), U64(0x4BF226CE, 0x4F740BC3) },
    { U64(0x02858FFE, 0x00A8D09A), U64(0xA3281F0B, 0x72C33C9C) },
    { U64(0x02047331, 0x9A20A6E2), U64(0x1C2018D5, 0xF568FD4A) },
    { U64(0x033A51E8, 0xF69AA49C), U64(0xF9CCF489, 0x88A7FBA9) },
    { U64(0x02950E53, 0xF87BB6E3), U64(0xFB0A5D3A, 0xD3B99621) },
    { U64(0x0210D843, 0x2D2FC583), U64(0x2F3B7DC8, 0xA96144E7) },
    { U64(0x034E26D1, 0xE1E608D1), U64(0xE52BFC74, 0x42353B0C) },
    { U64(0x02A4EBDB, 0x1B1E6D74), U64(0xB7566390, 0x34F76270) },
    { U64(0x021D897C, 0x15B1F12A), U64(0x2C451C73, 0x5D92B526) },
    { U64(0x03627593, 0x55E981DD), U64(0x13A1C71E, 0xFC1DEEA3) },
    { U64(0x02B52ADC, 0x44BACE4A), U64(0x761B05B2, 0x634B2550) },
    { U64(0x022A88B0, 0x36FBD83B), U64(0x91AF37C1, 0xE908EAA6) },
    { U64(0x03774119, 0xF192F392), U64(0x82B1F2CF, 0xDB417770) },
    { U64(0x02C5CDAE, 0x5ADBF60E), U64(0xCEF4C23F, 0xE29AC5F3) },
    { U64(0x0237D7BE, 0xAF165E72), U64(0x3F2A34FF, 0xE87BD190) },
    { U64(0x038C8C64, 0x4B56FD83), U64(0x984387FF, 0xDA5FB5B2) },
    { U64(0x02D6D6B6, 0xA2ABFE02), U64(0xE0360666, 0x484C915B) },
    { U64(0x02457892, 0x1BBCCB35), U64(0x802B3851, 0xD3707449) },
    { U64(0x03A25A83, 0x5F947855), U64(0x99DEC082, 0xEBE72075) },
    { U64(0x02E84869, 0x19439377), U64(0xAE4BCD35, 0x8985B391) },
    { U64(0x02536D20, 0xE102DC5F), U64(0xBEA30A91, 0x3AD15C74) },
    { U64(0x03B8AE9B, 0x019E2D65), U64(0xFDD1AA81, 0xF7B560B9) },
    { U64(0x02FA2548, 0xCE182451), U64(0x97DAEECE, 0x5FC44D61) },
    { U64(0x0261B76D, 0x71ACE9DA), U64(0xDFE258A5, 0x1969D781) },
    { U64(0x03CF8BE2, 0x4F7B0FC4), U64(0x996A276E, 0x8F0FBF34) },
    { U64(0x030C6FE8, 0x3F95A636), U64(0xE121B925, 0x3F3FCC2A) },
    { U64(0x02705986, 0x994484F8), U64(0xB41AFA84, 0x32997022) },
    { U64(0x03E6F5A4, 0x286DA18D), U64(0xECF7F739, 0xEA8F19CF) },
    { U64(0x031F2AE9, 0xB9F14E0B), U64(0x23F99294, 0xBBA5AE40) },
    { U64(0x027F5587, 0xC7F43E6F), U64(0x4FFADBAA, 0x2FB7BE99) },
    { U64(0x03FEEF3F, 0xA6539718), U64(0x7FF7C5DD, 0x1925FDC2) },
    { U64(0x033258FF, 0xB842DF46), U64(0xCCC637E4, 0x141E649B) },
    { U64(0x028EAD99, 0x60357F6B), U64(0xD704F983, 0x434B83AF) },
    { U64(0x020BBE14, 0x4CF79923), U64(0x126A6135, 0xCF6F9C8C) },
    { U64(0x0345FCED, 0x47F28E9E), U64(0x83DD6856, 0x18B29414) },
    { U64(0x029E63F1, 0x065BA54B), U64(0x9CB12044, 0xE08EDCDD) },
    { U64(0x02184FF4, 0x05161DD6), U64(0x16F419D0, 0xB3A57D7D) },
    { U64(0x035A1986, 0x6E89C956), U64(0x8B20294D, 0xEC3BFBFB) },
    { U64(0x02AE7AD1, 0xF207D445), U64(0x3C19BAA4, 0xBCFCC996) },
    { U64(0x02252F0E, 0x5B39769D), U64(0xC9AE2EEA, 0x30CA3ADF) },
    { U64(0x036EB1B0, 0x91F58A96), U64(0x0F7D17DD, 0x1ADD2AFD) },
    { U64(0x02BEF48D, 0x41913BAB), U64(0x3F97464A, 0x7BE42264) },
    { U64(0x02325D3D, 0xCE0DC955), U64(0xCC790508, 0x631CE850) },
    { U64(0x0383C862, 0xE3494222), U64(0xE0C1A1A7, 0x04FB0D4D) },
    { U64(0x02CFD382, 0x4F6DCE82), U64(0x4D67B485, 0x9D95A43E) },
    { U64(0x023FDC68, 0x3F8B0B9B), U64(0x711FC39E, 0x17AAE9CB) },
    { U64(0x039960A6, 0xCC11AC2B), U64(0xE832D296, 0x8C44A945) },
    { U64(0x02E11A1F, 0x09A7BCEF), U64(0xECF57545, 0x3D03BA9E) },
    { U64(0x024DAE7F, 0x3AEC9726), U64(0x572AC437, 0x6402FBB1) },
    { U64(0x03AF7D98, 0x5E47583D), U64(0x58446D25, 0x6CD192B5) },
    { U64(0x02F2CAE0, 0x4B6C4697), U64(0x79D05751, 0x23DADBC4) },
    { U64(0x025BD580, 0x3C569EDF), U64(0x94A6AC40, 0xE97BE303) },
    { U64(0x03C62266, 0xC6F0FE32), U64(0x8771139B, 0x0F2C9E6C) },
    { U64(0x0304E852, 0x38C0CB5B), U64(0x9F8DA948, 0xD8F07EBD) },
    { U64(0x026A5374, 0xFA33D5E2), U64(0xE60AEDD3, 0xE0C06564) },
    { U64(0x03DD5254, 0xC3862304), U64(0xA344AFB9, 0x679A3BD2) },
    { U64(0x03177510, 0x9C6B4F36), U64(0xE903BFC7, 0x8614FCA8) },
    { U64(0x02792A73, 0xB055D8F8), U64(0xBA696639, 0x3810CA20) },
    { U64(0x03F510B9, 0x1A22F4C1), U64(0x2A423D28, 0x59B4769A) },
    { U64(0x032A73C7, 0x481BF700), U64(0xEE9B6420, 0x47C39215) },
    { U64(0x02885C9F, 0x6CE32C00), U64(0xBEE2B680, 0x396941AA) },
    { U64(0x0206B07F, 0x8A4F5666), U64(0xFF1BC533, 0x61210155) },
    { U64(0x033DE732, 0x76E5570B), U64(0x31C60852, 0x35019BBB) },
    { U64(0x0297EC28, 0x5F1DDF3C), U64(0x27D1A041, 0xC4014963) },
    { U64(0x02132353, 0x7F4B18FC), U64(0xECA7B367, 0xD0010782) },
    { U64(0x0351D21F, 0x3211C194), U64(0xADD91F0C, 0x8001A59D) },
    { U64(0x02A7DB4C, 0x280E3476), U64(0xF17A7F3D, 0x3334847E) },
    { U64(0x021FE2A3, 0x533E905F), U64(0x27953297, 0x5C2A0398) },
    { U64(0x0366376B, 0xB8641A31), U64(0xD8EEB758, 0x93766C26) },
    { U64(0x02B82C56, 0x2D1CE1C1), U64(0x7A5892AD, 0x42C52352) },
    { U64(0x022CF044, 0xF0E3E7CD), U64(0xFB7A0EF1, 0x02374F75) },
    { U64(0x037B1A07, 0xE7D30C7C), U64(0xC59017E8, 0x038BB254) },
    { U64(0x02C8E19F, 0xECA8D6CA), U64(0x37A67986, 0x693C8EAA) },
    { U64(0x023A4E19, 0x8A20ABD4), U64(0xF951FAD1, 0xEDCA0BBB) },
    { U64(0x03907CF5, 0xA9CDDFBB), U64(0x28832AE9, 0x7C76792B) },
    { U64(0x02D9FD91, 0x54A4B2FC), U64(0x2068EF21, 0x305EC756) },
    { U64(0x0247FE0D, 0xDD508F30), U64(0x19ED8C1A, 0x8D189F78) },
    { U64(0x03A66349, 0x621A7EB3), U64(0x5CAF4690, 0xE1C0FF26) },
    { U64(0x02EB82A1, 0x1B48655C), U64(0x4A25D20D, 0x81673285) },
    { U64(0x0256021A, 0x7C39EAB0), U64(0x3B5174D7, 0x9AB8F537) },
    { U64(0x03BCD02A, 0x605CAAB3), U64(0x921BEE25, 0xC45B21F1) },
    { U64(0x02FD7355, 0x19E3BBC2), U64(0xDB498B51, 0x69E2818E) },
    { U64(0x02645C44, 0x14B62FCF), U64(0x15D46F74, 0x54B53472) },
    { U64(0x03D3C6D3, 0x5456B2E4), U64(0xEFBA4BED, 0x545520B6) },
    { U64(0x030FD242, 0xA9DEF583), U64(0xF2FB6FF1, 0x10441A2B) },
    { U64(0x02730E9B, 0xBB18C469), U64(0x8F2F8CC0, 0xD9D014EF) },
    { U64(0x03EB4A92, 0xC4F46D75), U64(0xB1E5AE01, 0x5C80217F) },
    { U64(0x0322A20F, 0x03F6BDF7), U64(0xC1848B34, 0x4A001ACC) },
    { U64(0x02821B3F, 0x365EFE5F), U64(0xCE03A290, 0x3B3348A3) },
    { U64(0x0201AF65, 0xC518CB7F), U64(0xD802E873, 0x628F6D4F) },
    { U64(0x0335E56F, 0xA1C14599), U64(0x599E40B8, 0x9DB2487F) },
    { U64(0x02918459, 0x4E3437AD), U64(0xE14B66FA, 0x17C1D399) },
    { U64(0x020E037A, 0xA4F692F1), U64(0x81091F2E, 0x7967DC7A) },
    { U64(0x03499F2A, 0xA18A84B5), U64(0x9B41CB7D, 0x8F0C93F6) },
    { U64(0x02A14C22, 0x1AD536F7), U64(0xAF67D5FE, 0x0C0A0FF8) },
    { U64(0x021AA34E, 0x7BDDC592), U64(0xF2B977FE, 0x70080CC7) },
    { U64(0x035DD217, 0x2C9608EB), U64(0x1DF58CCA, 0x4CD9AE0B) },
    { U64(0x02B174DF, 0x56DE6D88), U64(0xE4C470A1, 0xD7148B3C) },
    { U64(0x022790B2, 0xABE5246D), U64(0x83D05A1B, 0x1276D5CA) },
    { U64(0x0372811D, 0xDFD50715), U64(0x9FB3C35E, 0x83F1560F) },
    { U64(0x02C200E4, 0xB310D277), U64(0xB2F635E5, 0x365AAB3F) },
    { U64(0x0234CD83, 0xC273DB92), U64(0xF591C4B7, 0x5EAEEF66) },
    { U64(0x0387AF39, 0x371FC5B7), U64(0xEF4FA125, 0x644B18A3) },
    { U64(0x02D2F294, 0x2C196AF9), U64(0x8C3FB41D, 0xE9D5AD4F) },
    { U64(0x02425BA9, 0xBCE12261), U64(0x3CFFC34B, 0x2177BDD9) },
    { U64(0x039D5F75, 0xFB01D09B), U64(0x94CC6BAB, 0x68BF9628) },
    { U64(0x02E44C5E, 0x6267DA16), U64(0x10A38955, 0xED6611B9) },
    { U64(0x02503D18, 0x4EB97B44), U64(0xDA1C6DDE, 0x5784DAFB) },
    { U64(0x03B394F3, 0xB128C53A), U64(0xF693E2FD, 0x58D49191) },
    { U64(0x02F610C2, 0xF4209DC8), U64(0xC5431BFD, 0xE0AA0E0E) },
    { U64(0x025E73CF, 0x29B3B16D), U64(0x6A9C1664, 0xB3BB3E72) },
    { U64(0x03CA52E5, 0x0F85E8AF), U64(0x10F9BD6D, 0xEC5ECA4F) },
    { U64(0x03084250, 0xD937ED58), U64(0xDA616457, 0xF04BD50C) },
    { U64(0x026D01DA, 0x475FF113), U64(0xE1E78379, 0x8D09773D) },
    { U64(0x03E19C90, 0x72331B53), U64(0x030C058F, 0x480F252E) },
    { U64(0x031AE3A6, 0xC1C27C42), U64(0x68D66AD9, 0x06728425) },
    { U64(0x027BE952, 0x349B969B), U64(0x8711EF14, 0x052869B7) },
    { U64(0x03F97550, 0x542C242C), U64(0x0B4FE4EC, 0xD50D75F2) },
    { U64(0x032DF773, 0x7689B689), U64(0xA2A650BD, 0x773DF7F5) },
    { U64(0x028B2C5C, 0x5ED49207), U64(0xB551DA31, 0x2C31932A) },
    { U64(0x0208F049, 0xE576DB39), U64(0x5DDB14F4, 0x235ADC22) },
    { U64(0x03418076, 0x3BF15EC2), U64(0x2FC4EE53, 0x6BC49369) },
    { U64(0x029ACD2B, 0x63277F01), U64(0xBFD0BEA9, 0x2303A921) },
    { U64(0x021570EF, 0x8285FF34), U64(0x9973CBBA, 0x8269541A) },
    { U64(0x0355817F, 0x373CCB87), U64(0x5BEC792A, 0x6A42202A) },
    { U64(0x02AACDFF, 0x5F63D605), U64(0xE3239421, 0xEE9B4CEF) },
    { U64(0x02223E65, 0xE5E97804), U64(0xB5B6101B, 0x25490A59) },
    { U64(0x0369FD6F, 0xD64259A1), U64(0x22BCE691, 0xD541AA27) },
    { U64(0x02BB3126, 0x4501E14D), U64(0xB563EBA7, 0xDDCE21B9) },
    { U64(0x022F5A85, 0x0401810A), U64(0xF78322EC, 0xB171B494) },
    { U64(0x037EF73B, 0x399C01AB), U64(0x259E9E47, 0x824F8753) },
    { U64(0x02CBF8FC, 0x2E1667BC), U64(0x1E187E9F, 0x9B72D2A9) },
    { U64(0x023CC730, 0x24DEB963), U64(0x4B46CBB2, 0xE2C24221) },
    { U64(0x039471E6, 0xA1645BD2), U64(0x120ADF84, 0x9E039D01) },
    { U64(0x02DD27EB, 0xB4504974), U64(0xDB3BE603, 0xB19C7D9A) },
    { U64(0x024A8656, 0x29D9D45D), U64(0x7C2FEB36, 0x27B0647C) },
    { U64(0x03AA7089, 0xDC8FBA2F), U64(0x2D197856, 0xA5E7072C) },
    { U64(0x02EEC06E, 0x4A0C94F2), U64(0x8A7AC6AB, 0xB7EC05BD) },
    { U64(0x025899F1, 0xD4D6DD8E), U64(0xD52F0556, 0x2CBCD164) },
    { U64(0x03C0F64F, 0xBAF1627E), U64(0x21E4D556, 0xADFAE8A0) },
    { U64(0x0300C50C, 0x958DE864), U64(0xE7EA4445, 0x57FBED4D) },
    { U64(0x0267040A, 0x113E5383), U64(0xECBB69D1, 0x132FF10A) },
    { U64(0x03D80676, 0x81FD526C), U64(0xADF8A94E, 0x851981AA) },
    { U64(0x0313385E, 0xCE6441F0), U64(0x8B2D543E, 0xD0E13488) },
    { U64(0x0275C6B2, 0x3EB69B26), U64(0xD5BDDCFF, 0x0D80F6D3) },
    { U64(0x03EFA450, 0x64575EA4), U64(0x892FC7FE, 0x7C018AEB) },
    { U64(0x03261D0D, 0x1D12B21D), U64(0x3A8C9FFE, 0xC99AD589) },
    { U64(0x0284E40A, 0x7DA88E7D), U64(0xC8707FFF, 0x07AF113B) },
    { U64(0x0203E9A1, 0xFE2071FE), U64(0x39F39998, 0xD2F2742F) },
    { U64(0x033975CF, 0xFD00B663), U64(0x8FEC28F4, 0x84B7204B) },
    { U64(0x02945E3F, 0xFD9A2B82), U64(0xD989BA5D, 0x36F8E6A2) },
    { U64(0x02104B66, 0x647B5602), U64(0x47A161E4, 0x2BFA521C) },
    { U64(0x034D4570, 0xA0C5566A), U64(0x0C35696D, 0x132A1CF9) },
    { U64(0x02A4378D, 0x4D6AAB88), U64(0x09C45457, 0x4288172D) },
    { U64(0x021CF93D, 0xD7888939), U64(0xA169DD12, 0x9BA0128B) },
    { U64(0x03618EC9, 0x58DA7529), U64(0x0242FB50, 0xF9001DAB) },
    { U64(0x02B4723A, 0xAD7B90ED), U64(0x9B68C90D, 0x940017BC) },
    { U64(0x0229F4FB, 0xBDFC73F1), U64(0x4920A0D7, 0xA999AC96) },
    { U64(0x037654C5, 0xFCC71FE8), U64(0x75010159, 0x0F5C4757) },
    { U64(0x02C5109E, 0x63D27FED), U64(0x2A673447, 0x3F7D05DF) },
    { U64(0x0237407E, 0xB641FFF0), U64(0xEEB8F69F, 0x65FD9E4C) },
    { U64(0x038B9A64, 0x56CFFFE7), U64(0xE45B2432, 0x3CC8FD46) },
    { U64(0x02D6151D, 0x123FFFEC), U64(0xB6AF5028, 0x30A0CA9F) },
    { U64(0x0244DDB0, 0xDB666656), U64(0xF88C4020, 0x26E7087F) },
    { U64(0x03A162B4, 0x923D708B), U64(0x2746CD00, 0x3E3E73FE) },
    { U64(0x02E7822A, 0x0E978D3C), U64(0x1F6BD733, 0x64FEC332) },
    { U64(0x0252CE88, 0x0BAC70FC), U64(0xE5EFDF5C, 0x50CBCF5B) },
    { U64(0x03B7B0D9, 0xAC471B2E), U64(0x3CB2FEFA, 0x1ADFB22B) },
    { U64(0x02F95A47, 0xBD05AF58), U64(0x308F3261, 0xAF195B56) },
    { U64(0x02611506, 0x30D15913), U64(0x5A0C284E, 0x25ADE2AB) },
    { U64(0x03CE8809, 0xE7B55B52), U64(0x29AD0D49, 0xD5E30445) },
    { U64(0x030BA007, 0xEC9115DB), U64(0x548A7107, 0xDE4F369D) },
    { U64(0x026FB339, 0x8A0DAB15), U64(0xDD3B8D9F, 0xE50C2BB1) },
    { U64(0x03E5EB8F, 0x434911BC), U64(0x952C15CC, 0xA1AD12B5) },
    { U64(0x031E560C, 0x35D40E30), U64(0x775677D6, 0xE7BDA891) }
};

/** The significant bits kept in pow5_sig_table. */
#define POW5_SIG_BITS 121

/** The significant bits table of pow5 (5.7KB).
    (generate with misc/make_tables.c) */
static const u64 pow5_sig_table[326][2] = {
    { U64(0x01000000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01400000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01900000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01F40000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01388000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x0186A000, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01E84800, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01312D00, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x017D7840, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x01DCD650, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x012A05F2, 0x00000000), U64(0x00000000, 0x00000000) },
    { U64(0x0174876E, 0x80000000), U64(0x00000000, 0x00000000) },
    { U64(0x01D1A94A, 0x20000000), U64(0x00000000, 0x00000000) },
    { U64(0x012309CE, 0x54000000), U64(0x00000000, 0x00000000) },
    { U64(0x016BCC41, 0xE9000000), U64(0x00000000, 0x00000000) },
    { U64(0x01C6BF52, 0x63400000), U64(0x00000000, 0x00000000) },
    { U64(0x011C3793, 0x7E080000), U64(0x00000000, 0x00000000) },
    { U64(0x01634578, 0x5D8A0000), U64(0x00000000, 0x00000000) },
    { U64(0x01BC16D6, 0x74EC8000), U64(0x00000000, 0x00000000) },
    { U64(0x01158E46, 0x0913D000), U64(0x00000000, 0x00000000) },
    { U64(0x015AF1D7, 0x8B58C400), U64(0x00000000, 0x00000000) },
    { U64(0x01B1AE4D, 0x6E2EF500), U64(0x00000000, 0x00000000) },
    { U64(0x010F0CF0, 0x64DD5920), U64(0x00000000, 0x00000000) },
    { U64(0x0152D02C, 0x7E14AF68), U64(0x00000000, 0x00000000) },
    { U64(0x01A78437, 0x9D99DB42), U64(0x00000000, 0x00000000) },
    { U64(0x0108B2A2, 0xC2802909), U64(0x40000000, 0x00000000) },
    { U64(0x014ADF4B, 0x7320334B), U64(0x90000000, 0x00000000) },
    { U64(0x019D971E, 0x4FE8401E), U64(0x74000000, 0x00000000) },
    { U64(0x01027E72, 0xF1F12813), U64(0x08800000, 0x00000000) },
    { U64(0x01431E0F, 0xAE6D7217), U64(0xCAA00000, 0x00000000) },
    { U64(0x0193E593, 0x9A08CE9D), U64(0xBD480000, 0x00000000) },
    { U64(0x01F8DEF8, 0x808B0245), U64(0x2C9A0000, 0x00000000) },
    { U64(0x013B8B5B, 0x5056E16B), U64(0x3BE04000, 0x00000000) },
    { U64(0x018A6E32, 0x246C99C6), U64(0x0AD85000, 0x00000000) },
    { U64(0x01ED09BE, 0xAD87C037), U64(0x8D8E6400, 0x00000000) },
    { U64(0x01342617, 0x2C74D822), U64(0xB878FE80, 0x00000000) },
    { U64(0x01812F9C, 0xF7920E2B), U64(0x66973E20, 0x00000000) },
    { U64(0x01E17B84, 0x357691B6), U64(0x403D0DA8, 0x00000000) },
    { U64(0x012CED32, 0xA16A1B11), U64(0xE8262889, 0x00000000) },
    { U64(0x0178287F, 0x49C4A1D6), U64(0x622FB2AB, 0x40000000) },
    { U64(0x01D6329F, 0x1C35CA4B), U64(0xFABB9F56, 0x10000000) },
    { U64(0x0125DFA3, 0x71A19E6F), U64(0x7CB54395, 0xCA000000) },
    { U64(0x016F578C, 0x4E0A060B), U64(0x5BE2947B, 0x3C800000) },
    { U64(0x01CB2D6F, 0x618C878E), U64(0x32DB399A, 0x0BA00000) },
    { U64(0x011EFC65, 0x9CF7D4B8), U64(0xDFC90400, 0x47440000) },
    { U64(0x0166BB7F, 0x0435C9E7), U64(0x17BB4500, 0x59150000) },
    { U64(0x01C06A5E, 0xC5433C60), U64(0xDDAA1640, 0x6F5A4000) },
    { U64(0x0118427B, 0x3B4A05BC), U64(0x8A8A4DE8, 0x45986800) },
    { U64(0x015E531A, 0x0A1C872B), U64(0xAD2CE162, 0x56FE8200) },
    { U64(0x01B5E7E0, 0x8CA3A8F6), U64(0x987819BA, 0xECBE2280) },
    { U64(0x0111B0EC, 0x57E6499A), U64(0x1F4B1014, 0xD3F6D590) },
    { U64(0x01561D27, 0x6DDFDC00), U64(0xA71DD41A, 0x08F48AF4) },
    { U64(0x01ABA471, 0x4957D300), U64(0xD0E54920, 0x8B31ADB1) },
    { U64(0x010B46C6, 0xCDD6E3E0), U64(0x828F4DB4, 0x56FF0C8E) },
    { U64(0x014E1878, 0x814C9CD8), U64(0xA3332121, 0x6CBECFB2) },
    { U64(0x01A19E96, 0xA19FC40E), U64(0xCBFFE969, 0xC7EE839E) },
    { U64(0x0105031E, 0x2503DA89), U64(0x3F7FF1E2, 0x1CF51243) },
    { U64(0x014643E5, 0xAE44D12B), U64(0x8F5FEE5A, 0xA43256D4) },
    { U64(0x0197D4DF, 0x19D60576), U64(0x7337E9F1, 0x4D3EEC89) },
    { U64(0x01FDCA16, 0xE04B86D4), U64(0x1005E46D, 0xA08EA7AB) },
    { U64(0x013E9E4E, 0x4C2F3444), U64(0x8A03AEC4, 0x845928CB) },
    { U64(0x018E45E1, 0xDF3B0155), U64(0xAC849A75, 0xA56F72FD) },
    { U64(0x01F1D75A, 0x5709C1AB), U64(0x17A5C113, 0x0ECB4FBD) },
    { U64(0x01372698, 0x7666190A), U64(0xEEC798AB, 0xE93F11D6) },
    { U64(0x0184F03E, 0x93FF9F4D), U64(0xAA797ED6, 0xE38ED64B) },
    { U64(0x01E62C4E, 0x38FF8721), U64(0x1517DE8C, 0x9C728BDE) },
    { U64(0x012FDBB0, 0xE39FB474), U64(0xAD2EEB17, 0xE1C7976B) },
    { U64(0x017BD29D, 0x1C87A191), U64(0xD87AA5DD, 0xDA397D46) },
    { U64(0x01DAC744, 0x63A989F6), U64(0x4E994F55, 0x50C7DC97) },
    { U64(0x0128BC8A, 0xBE49F639), U64(0xF11FD195, 0x527CE9DE) },
    { U64(0x0172EBAD, 0x6DDC73C8), U64(0x6D67C5FA, 0xA71C2456) },
    { U64(0x01CFA698, 0xC95390BA), U64(0x88C1B779, 0x50E32D6C) },
    { U64(0x0121C81F, 0x7DD43A74), U64(0x957912AB, 0xD28DFC63) },
    { U64(0x016A3A27, 0x5D494911), U64(0xBAD75756, 0xC7317B7C) },
    { U64(0x01C4C8B1, 0x349B9B56), U64(0x298D2D2C, 0x78FDDA5B) },
    { U64(0x011AFD6E, 0xC0E14115), U64(0xD9F83C3B, 0xCB9EA879) },
    { U64(0x0161BCCA, 0x7119915B), U64(0x50764B4A, 0xBE865297) },
    { U64(0x01BA2BFD, 0x0D5FF5B2), U64(0x2493DE1D, 0x6E27E73D) },
    { U64(0x01145B7E, 0x285BF98F), U64(0x56DC6AD2, 0x64D8F086) },
    { U64(0x0159725D, 0xB272F7F3), U64(0x2C938586, 0xFE0F2CA8) },
    { U64(0x01AFCEF5, 0x1F0FB5EF), U64(0xF7B866E8, 0xBD92F7D2) },
    { U64(0x010DE159, 0x3369D1B5), U64(0xFAD34051, 0x767BDAE3) },
    { U64(0x015159AF, 0x80444623), U64(0x79881065, 0xD41AD19C) },
    { U64(0x01A5B01B, 0x605557AC), U64(0x57EA147F, 0x49218603) },
    { U64(0x01078E11, 0x1C3556CB), U64(0xB6F24CCF, 0x8DB4F3C1) },
    { U64(0x01497195, 0x6342AC7E), U64(0xA4AEE003, 0x712230B2) },
    { U64(0x019BCDFA, 0xBC13579E), U64(0x4DDA9804, 0x4D6ABCDF) },
    { U64(0x010160BC, 0xB58C16C2), U64(0xF0A89F02, 0xB062B60B) },
    { U64(0x0141B8EB, 0xE2EF1C73), U64(0xACD2C6C3, 0x5C7B638E) },
    { U64(0x01922726, 0xDBAAE390), U64(0x98077874, 0x339A3C71) },
    { U64(0x01F6B0F0, 0x92959C74), U64(0xBE095691, 0x4080CB8E) },
    { U64(0x013A2E96, 0x5B9D81C8), U64(0xF6C5D61A, 0xC8507F38) },
    { U64(0x0188BA3B, 0xF284E23B), U64(0x34774BA1, 0x7A649F07) },
    { U64(0x01EAE8CA, 0xEF261ACA), U64(0x01951E89, 0xD8FDC6C8) },
    { U64(0x0132D17E, 0xD577D0BE), U64(0x40FD3316, 0x279E9C3D) },
    { U64(0x017F85DE, 0x8AD5C4ED), U64(0xD13C7FDB, 0xB186434C) },
    { U64(0x01DF6756, 0x2D8B3629), U64(0x458B9FD2, 0x9DE7D420) },
    { U64(0x012BA095, 0xDC7701D9), U64(0xCB7743E3, 0xA2B0E494) },
    { U64(0x017688BB, 0x5394C250), U64(0x3E5514DC, 0x8B5D1DB9) },
    { U64(0x01D42AEA, 0x2879F2E4), U64(0x4DEA5A13, 0xAE346527) },
    { U64(0x01249AD2, 0x594C37CE), U64(0xB0B2784C, 0x4CE0BF38) },
    { U64(0x016DC186, 0xEF9F45C2), U64(0x5CDF165F, 0x6018EF06) },
    { U64(0x01C931E8, 0xAB871732), U64(0xF416DBF7, 0x381F2AC8) },
    { U64(0x011DBF31, 0x6B346E7F), U64(0xD88E497A, 0x83137ABD) },
    { U64(0x01652EFD, 0xC6018A1F), U64(0xCEB1DBD9, 0x23D8596C) },
    { U64(0x01BE7ABD, 0x3781ECA7), U64(0xC25E52CF, 0x6CCE6FC7) },
    { U64(0x01170CB6, 0x42B133E8), U64(0xD97AF3C1, 0xA40105DC) },
    { U64(0x015CCFE3, 0xD35D80E3), U64(0x0FD9B0B2, 0x0D014754) },
    { U64(0x01B403DC, 0xC834E11B), U64(0xD3D01CDE, 0x90419929) },
    { U64(0x01108269, 0xFD210CB1), U64(0x6462120B, 0x1A28FFB9) },
    { U64(0x0154A304, 0x7C694FDD), U64(0xBD7A968D, 0xE0B33FA8) },
    { U64(0x01A9CBC5, 0x9B83A3D5), U64(0x2CD93C31, 0x58E00F92) },
    { U64(0x010A1F5B, 0x81324665), U64(0x3C07C59E, 0xD78C09BB) },
    { U64(0x014CA732, 0x617ED7FE), U64(0x8B09B706, 0x8D6F0C2A) },
    { U64(0x019FD0FE, 0xF9DE8DFE), U64(0x2DCC24C8, 0x30CACF34) },
    { U64(0x0103E29F, 0x5C2B18BE), U64(0xDC9F96FD, 0x1E7EC180) },
    { U64(0x0144DB47, 0x3335DEEE), U64(0x93C77CBC, 0x661E71E1) },
    { U64(0x01961219, 0x000356AA), U64(0x38B95BEB, 0x7FA60E59) },
    { U64(0x01FB969F, 0x40042C54), U64(0xC6E7B2E6, 0x5F8F91EF) },
    { U64(0x013D3E23, 0x88029BB4), U64(0xFC50CFCF, 0xFBB9BB35) },
    { U64(0x018C8DAC, 0x6A0342A2), U64(0x3B6503C3, 0xFAA82A03) },
    { U64(0x01EFB117, 0x8484134A), U64(0xCA3E44B4, 0xF9523484) },
    { U64(0x0135CEAE, 0xB2D28C0E), U64(0xBE66EAF1, 0x1BD360D2) },
    { U64(0x0183425A, 0x5F872F12), U64(0x6E00A5AD, 0x62C83907) },
    { U64(0x01E412F0, 0xF768FAD7), U64(0x0980CF18, 0xBB7A4749) },
    { U64(0x012E8BD6, 0x9AA19CC6), U64(0x65F0816F, 0x752C6C8D) },
    { U64(0x017A2ECC, 0x414A03F7), U64(0xFF6CA1CB, 0x527787B1) },
    { U64(0x01D8BA7F, 0x519C84F5), U64(0xFF47CA3E, 0x2715699D) },
    { U64(0x0127748F, 0x9301D319), U64(0xBF8CDE66, 0xD86D6202) },
    { U64(0x017151B3, 0x77C247E0), U64(0x2F701600, 0x8E88BA83) },
    { U64(0x01CDA620, 0x55B2D9D8), U64(0x3B4C1B80, 0xB22AE923) },
    { U64(0x012087D4, 0x358FC827), U64(0x250F9130, 0x6F5AD1B6) },
    { U64(0x0168A9C9, 0x42F3BA30), U64(0xEE53757C, 0x8B318623) },
    { U64(0x01C2D43B, 0x93B0A8BD), U64(0x29E852DB, 0xADFDE7AC) },
    { U64(0x0119C4A5, 0x3C4E6976), U64(0x3A3133C9, 0x4CBEB0CC) },
    { U64(0x016035CE, 0x8B6203D3), U64(0xC8BD80BB, 0x9FEE5CFF) },
    { U64(0x01B84342, 0x2E3A84C8), U64(0xBAECE0EA, 0x87E9F43E) },
    { U64(0x01132A09, 0x5CE492FD), U64(0x74D40C92, 0x94F238A7) },
    { U64(0x0157F48B, 0xB41DB7BC), U64(0xD2090FB7, 0x3A2EC6D1) },
    { U64(0x01ADF1AE, 0xA12525AC), U64(0x068B53A5, 0x08BA7885) },
    { U64(0x010CB70D, 0x24B7378B), U64(0x84171447, 0x25748B53) },
    { U64(0x014FE4D0, 0x6DE5056E), U64(0x651CD958, 0xEED1AE28) },
    { U64(0x01A3DE04, 0x895E46C9), U64(0xFE640FAF, 0x2A8619B2) },
    { U64(0x01066AC2, 0xD5DAEC3E), U64(0x3EFE89CD, 0x7A93D00F) },
    { U64(0x01480573, 0x8B51A74D), U64(0xCEBE2C40, 0xD938C413) },
    { U64(0x019A06D0, 0x6E261121), U64(0x426DB751, 0x0F86F518) },
    { U64(0x01004442, 0x44D7CAB4), U64(0xC9849292, 0xA9B4592F) },
    { U64(0x01405552, 0xD60DBD61), U64(0xFBE5B737, 0x54216F7A) },
    { U64(0x01906AA7, 0x8B912CBA), U64(0x7ADF2505, 0x2929CB59) },
    { U64(0x01F48551, 0x6E7577E9), U64(0x1996EE46, 0x73743E2F) },
    { U64(0x0138D352, 0xE5096AF1), U64(0xAFFE54EC, 0x0828A6DD) },
    { U64(0x01870827, 0x9E4BC5AE), U64(0x1BFDEA27, 0x0A32D095) },
    { U64(0x01E8CA31, 0x85DEB719), U64(0xA2FD64B0, 0xCCBF84BA) },
    { U64(0x01317E5E, 0xF3AB3270), U64(0x05DE5EEE, 0x7FF7B2F4) },
    { U64(0x017DDDF6, 0xB095FF0C), U64(0x0755F6AA, 0x1FF59FB1) },
    { U64(0x01DD5574, 0x5CBB7ECF), U64(0x092B7454, 0xA7F3079E) },
    { U64(0x012A5568, 0xB9F52F41), U64(0x65BB28B4, 0xE8F7E4C3) },
    { U64(0x0174EAC2, 0xE8727B11), U64(0xBF29F2E2, 0x2335DDF3) },
    { U64(0x01D22573, 0xA28F19D6), U64(0x2EF46F9A, 0xAC035570) },
    { U64(0x01235768, 0x45997025), U64(0xDD58C5C0, 0xAB821566) },
    { U64(0x016C2D42, 0x56FFCC2F), U64(0x54AEF730, 0xD6629AC0) },
    { U64(0x01C73892, 0xECBFBF3B), U64(0x29DAB4FD, 0x0BFB4170) },
    { U64(0x011C835B, 0xD3F7D784), U64(0xFA28B11E, 0x277D08E6) },
    { U64(0x0163A432, 0xC8F5CD66), U64(0x38B2DD65, 0xB15C4B1F) },
    { U64(0x01BC8D3F, 0x7B3340BF), U64(0xC6DF94BF, 0x1DB35DE7) },
    { U64(0x0115D847, 0xAD000877), U64(0xDC4BBCF7, 0x72901AB0) },
    { U64(0x015B4E59, 0x98400A95), U64(0xD35EAC35, 0x4F34215C) },
    { U64(0x01B221EF, 0xFE500D3B), U64(0x48365742, 0xA30129B4) },
    { U64(0x010F5535, 0xFEF20845), U64(0x0D21F689, 0xA5E0BA10) },
    { U64(0x01532A83, 0x7EAE8A56), U64(0x506A742C, 0x0F58E894) },
    { U64(0x01A7F524, 0x5E5A2CEB), U64(0xE4851137, 0x132F22B9) },
    { U64(0x0108F936, 0xBAF85C13), U64(0x6ED32AC2, 0x6BFD75B4) },
    { U64(0x014B3784, 0x69B67318), U64(0x4A87F573, 0x06FCD321) },
    { U64(0x019E0565, 0x84240FDE), U64(0x5D29F2CF, 0xC8BC07E9) },
    { U64(0x0102C35F, 0x729689EA), U64(0xFA3A37C1, 0xDD7584F1) },
    { U64(0x01437437, 0x4F3C2C65), U64(0xB8C8C5B2, 0x54D2E62E) },
    { U64(0x01945145, 0x230B377F), U64(0x26FAF71E, 0xEA079FB9) },
    { U64(0x01F96596, 0x6BCE055E), U64(0xF0B9B4E6, 0xA48987A8) },
    { U64(0x013BDF7E, 0x0360C35B), U64(0x56741110, 0x26D5F4C9) },
    { U64(0x018AD75D, 0x8438F432), U64(0x2C111554, 0x308B71FB) },
    { U64(0x01ED8D34, 0xE547313E), U64(0xB7155AA9, 0x3CAE4E7A) },
    { U64(0x01347841, 0x0F4C7EC7), U64(0x326D58A9, 0xC5ECF10C) },
    { U64(0x01819651, 0x531F9E78), U64(0xFF08AED4, 0x37682D4F) },
    { U64(0x01E1FBE5, 0xA7E78617), U64(0x3ECADA89, 0x454238A3) },
    { U64(0x012D3D6F, 0x88F0B3CE), U64(0x873EC895, 0xCB496366) },
    { U64(0x01788CCB, 0x6B2CE0C2), U64(0x290E7ABB, 0x3E1BBC3F) },
    { U64(0x01D6AFFE, 0x45F818F2), U64(0xB352196A, 0x0DA2AB4F) },
    { U64(0x01262DFE, 0xEBBB0F97), U64(0xB0134FE2, 0x4885AB11) },
    { U64(0x016FB97E, 0xA6A9D37D), U64(0x9C1823DA, 0xDAA715D6) },
    { U64(0x01CBA7DE, 0x5054485D), U64(0x031E2CD1, 0x9150DB4B) },
    { U64(0x011F48EA, 0xF234AD3A), U64(0x21F2DC02, 0xFAD2890F) },
    { U64(0x01671B25, 0xAEC1D888), U64(0xAA6F9303, 0xB9872B53) },
    { U64(0x01C0E1EF, 0x1A724EAA), U64(0xD50B77C4, 0xA7E8F628) },
    { U64(0x01188D35, 0x7087712A), U64(0xC5272ADA, 0xE8F199D9) },
    { U64(0x015EB082, 0xCCA94D75), U64(0x7670F591, 0xA32E004F) },
    { U64(0x01B65CA3, 0x7FD3A0D2), U64(0xD40D32F6, 0x0BF98063) },
    { U64(0x0111F9E6, 0x2FE44483), U64(0xC4883FD9, 0xC77BF03E) },
    { U64(0x0156785F, 0xBBDD55A4), U64(0xB5AA4FD0, 0x395AEC4D) },
    { U64(0x01AC1677, 0xAAD4AB0D), U64(0xE314E3C4, 0x47B1A760) },
    { U64(0x010B8E0A, 0xCAC4EAE8), U64(0xADED0E5A, 0xACCF089C) },
    { U64(0x014E718D, 0x7D7625A2), U64(0xD96851F1, 0x5802CAC3) },
    { U64(0x01A20DF0, 0xDCD3AF0B), U64(0x8FC2666D, 0xAE037D74) },
    { U64(0x010548B6, 0x8A044D67), U64(0x39D98004, 0x8CC22E68) },
    { U64(0x01469AE4, 0x2C8560C1), U64(0x084FE005, 0xAFF2BA03) },
    { U64(0x0198419D, 0x37A6B8F1), U64(0x4A63D807, 0x1BEF6883) },
    { U64(0x01FE5204, 0x8590672D), U64(0x9CFCCE08, 0xE2EB42A4) },
    { U64(0x013EF342, 0xD37A407C), U64(0x821E00C5, 0x8DD309A7) },
    { U64(0x018EB013, 0x8858D09B), U64(0xA2A580F6, 0xF147CC10) },
    { U64(0x01F25C18, 0x6A6F04C2), U64(0x8B4EE134, 0xAD99BF15) },
    { U64(0x0137798F, 0x428562F9), U64(0x97114CC0, 0xEC80176D) },
    { U64(0x018557F3, 0x1326BBB7), U64(0xFCD59FF1, 0x27A01D48) },
    { U64(0x01E6ADEF, 0xD7F06AA5), U64(0xFC0B07ED, 0x7188249A) },
    { U64(0x01302CB5, 0xE6F642A7), U64(0xBD86E4F4, 0x66F516E0) },
    { U64(0x017C37E3, 0x60B3D351), U64(0xACE89E31, 0x80B25C98) },
    { U64(0x01DB45DC, 0x38E0C826), U64(0x1822C5BD, 0xE0DEF3BE) },
    { U64(0x01290BA9, 0xA38C7D17), U64(0xCF15BB96, 0xAC8B5857) },
    { U64(0x01734E94, 0x0C6F9C5D), U64(0xC2DB2A7C, 0x57AE2E6D) },
    { U64(0x01D02239, 0x0F8B8375), U64(0x3391F51B, 0x6D99BA08) },
    { U64(0x01221563, 0xA9B73229), U64(0x403B3931, 0x24801445) },
    { U64(0x016A9ABC, 0x9424FEB3), U64(0x904A077D, 0x6DA01956) },
    { U64(0x01C5416B, 0xB92E3E60), U64(0x745C895C, 0xC9081FAC) },
    { U64(0x011B48E3, 0x53BCE6FC), U64(0x48B9D5D9, 0xFDA513CB) },
    { U64(0x01621B1C, 0x28AC20BB), U64(0x5AE84B50, 0x7D0E58BE) },
    { U64(0x01BAA1E3, 0x32D728EA), U64(0x31A25E24, 0x9C51EEEE) },
    { U64(0x0114A52D, 0xFFC67992), U64(0x5F057AD6, 0xE1B33554) },
    { U64(0x0159CE79, 0x7FB817F6), U64(0xF6C6D98C, 0x9A2002AA) },
    { U64(0x01B04217, 0xDFA61DF4), U64(0xB4788FEF, 0xC0A80354) },
    { U64(0x010E294E, 0xEBC7D2B8), U64(0xF0CB59F5, 0xD8690214) },
    { U64(0x0151B3A2, 0xA6B9C767), U64(0x2CFE3073, 0x4E83429A) },
    { U64(0x01A6208B, 0x50683940), U64(0xF83DBC90, 0x22241340) },
    { U64(0x0107D457, 0x124123C8), U64(0x9B2695DA, 0x15568C08) },
    { U64(0x0149C96C, 0xD6D16CBA), U64(0xC1F03B50, 0x9AAC2F0A) },
    { U64(0x019C3BC8, 0x0C85C7E9), U64(0x726C4A24, 0xC1573ACD) },
    { U64(0x0101A55D, 0x07D39CF1), U64(0xE783AE56, 0xF8D684C0) },
    { U64(0x01420EB4, 0x49C8842E), U64(0x616499EC, 0xB70C25F0) },
    { U64(0x01929261, 0x5C3AA539), U64(0xF9BDC067, 0xE4CF2F6C) },
    { U64(0x01F736F9, 0xB3494E88), U64(0x782D3081, 0xDE02FB47) },
    { U64(0x013A825C, 0x100DD115), U64(0x4B1C3E51, 0x2AC1DD0C) },
    { U64(0x018922F3, 0x1411455A), U64(0x9DE34DE5, 0x7572544F) },
    { U64(0x01EB6BAF, 0xD91596B1), U64(0x455C215E, 0xD2CEE963) },
    { U64(0x0133234D, 0xE7AD7E2E), U64(0xCB5994DB, 0x43C151DE) },
    { U64(0x017FEC21, 0x6198DDBA), U64(0x7E2FFA12, 0x14B1A655) },
    { U64(0x01DFE729, 0xB9FF1529), U64(0x1DBBF896, 0x99DE0FEB) },
    { U64(0x012BF07A, 0x143F6D39), U64(0xB2957B5E, 0x202AC9F3) },
    { U64(0x0176EC98, 0x994F4888), U64(0x1F3ADA35, 0xA8357C6F) },
    { U64(0x01D4A7BE, 0xBFA31AAA), U64(0x270990C3, 0x1242DB8B) },
    { U64(0x0124E8D7, 0x37C5F0AA), U64(0x5865FA79, 0xEB69C937) },
    { U64(0x016E230D, 0x05B76CD4), U64(0xEE7F7918, 0x66443B85) },
    { U64(0x01C9ABD0, 0x4725480A), U64(0x2A1F575E, 0x7FD54A66) },
    { U64(0x011E0B62, 0x2C774D06), U64(0x5A53969B, 0x0FE54E80) },
    { U64(0x01658E3A, 0xB7952047), U64(0xF0E87C41, 0xD3DEA220) },
    { U64(0x01BEF1C9, 0x657A6859), U64(0xED229B52, 0x48D64AA8) },
    { U64(0x0117571D, 0xDF6C8138), U64(0x3435A113, 0x6D85EEA9) },
    { U64(0x015D2CE5, 0x5747A186), U64(0x41430958, 0x48E76A53) },
    { U64(0x01B4781E, 0xAD1989E7), U64(0xD193CBAE, 0x5B2144E8) },
    { U64(0x0110CB13, 0x2C2FF630), U64(0xE2FC5F4C, 0xF8F4CB11) },
    { U64(0x0154FDD7, 0xF73BF3BD), U64(0x1BBB7720, 0x3731FDD5) },
    { U64(0x01AA3D4D, 0xF50AF0AC), U64(0x62AA54E8, 0x44FE7D4A) },
    { U64(0x010A6650, 0xB926D66B), U64(0xBDAA7511, 0x2B1F0E4E) },
    { U64(0x014CFFE4, 0xE7708C06), U64(0xAD151255, 0x75E6D1E2) },
    { U64(0x01A03FDE, 0x214CAF08), U64(0x585A56EA, 0xD360865B) },
    { U64(0x010427EA, 0xD4CFED65), U64(0x37387652, 0xC41C53F8) },
    { U64(0x014531E5, 0x8A03E8BE), U64(0x850693E7, 0x752368F7) },
    { U64(0x01967E5E, 0xEC84E2EE), U64(0x264838E1, 0x526C4334) },
    { U64(0x01FC1DF6, 0xA7A61BA9), U64(0xAFDA4719, 0xA7075402) },
    { U64(0x013D92BA, 0x28C7D14A), U64(0x0DE86C70, 0x08649481) },
    { U64(0x018CF768, 0xB2F9C59C), U64(0x9162878C, 0x0A7DB9A1) },
    { U64(0x01F03542, 0xDFB83703), U64(0xB5BB296F, 0x0D1D280A) },
    { U64(0x01362149, 0xCBD32262), U64(0x5194F9E5, 0x68323906) },
    { U64(0x0183A99C, 0x3EC7EAFA), U64(0xE5FA385E, 0xC23EC747) },
    { U64(0x01E49403, 0x4E79E5B9), U64(0x9F78C676, 0x72CE7919) },
    { U64(0x012EDC82, 0x110C2F94), U64(0x03AB7C0A, 0x07C10BB0) },
    { U64(0x017A93A2, 0x954F3B79), U64(0x04965B0C, 0x89B14E9C) },
    { U64(0x01D9388B, 0x3AA30A57), U64(0x45BBF1CF, 0xAC1DA243) },
    { U64(0x0127C357, 0x04A5E676), U64(0x8B957721, 0xCB92856A) },
    { U64(0x0171B42C, 0xC5CF6014), U64(0x2E7AD4EA, 0x3E7726C4) },
    { U64(0x01CE2137, 0xF7433819), U64(0x3A198A24, 0xCE14F075) },
    { U64(0x0120D4C2, 0xFA8A030F), U64(0xC44FF657, 0x00CD1649) },
    { U64(0x016909F3, 0xB92C83D3), U64(0xB563F3EC, 0xC1005BDB) },
    { U64(0x01C34C70, 0xA777A4C8), U64(0xA2BCF0E7, 0xF14072D2) },
    { U64(0x011A0FC6, 0x68AAC6FD), U64(0x65B61690, 0xF6C847C3) },
    { U64(0x016093B8, 0x02D578BC), U64(0xBF239C35, 0x347A59B4) },
    { U64(0x01B8B8A6, 0x038AD6EB), U64(0xEEEC8342, 0x8198F021) },
    { U64(0x01137367, 0xC236C653), U64(0x7553D209, 0x90FF9615) },
    { U64(0x01585041, 0xB2C477E8), U64(0x52A8C68B, 0xF53F7B9A) },
    { U64(0x01AE6452, 0x1F7595E2), U64(0x6752F82E, 0xF28F5A81) },
    { U64(0x010CFEB3, 0x53A97DAD), U64(0x8093DB1D, 0x57999890) },
    { U64(0x01503E60, 0x2893DD18), U64(0xE0B8D1E4, 0xAD7FFEB4) },
    { U64(0x01A44DF8, 0x32B8D45F), U64(0x18E7065D, 0xD8DFFE62) },
    { U64(0x0106B0BB, 0x1FB384BB), U64(0x6F9063FA, 0xA78BFEFD) },
    { U64(0x01485CE9, 0xE7A065EA), U64(0x4B747CF9, 0x516EFEBC) },
    { U64(0x019A7424, 0x61887F64), U64(0xDE519C37, 0xA5CABE6B) },
    { U64(0x01008896, 0xBCF54F9F), U64(0x0AF301A2, 0xC79EB703) },
    { U64(0x0140AABC, 0x6C32A386), U64(0xCDAFC20B, 0x798664C4) },
    { U64(0x0190D56B, 0x873F4C68), U64(0x811BB28E, 0x57E7FDF5) },
    { U64(0x01F50AC6, 0x690F1F82), U64(0xA1629F31, 0xEDE1FD72) },
    { U64(0x013926BC, 0x01A973B1), U64(0xA4DDA37F, 0x34AD3E67) },
    { U64(0x0187706B, 0x0213D09E), U64(0x0E150C5F, 0x01D88E01) },
    { U64(0x01E94C85, 0xC298C4C5), U64(0x919A4F76, 0xC24EB181) },
    { U64(0x0131CFD3, 0x999F7AFB), U64(0x7B0071AA, 0x39712EF1) },
    { U64(0x017E43C8, 0x800759BA), U64(0x59C08E14, 0xC7CD7AAD) },
    { U64(0x01DDD4BA, 0xA0093028), U64(0xF030B199, 0xF9C0D958) },
    { U64(0x012AA4F4, 0xA405BE19), U64(0x961E6F00, 0x3C1887D7) },
    { U64(0x01754E31, 0xCD072D9F), U64(0xFBA60AC0, 0x4B1EA9CD) },
    { U64(0x01D2A1BE, 0x4048F907), U64(0xFA8F8D70, 0x5DE65440) },
    { U64(0x0123A516, 0xE82D9BA4), U64(0xFC99B866, 0x3AAFF4A8) },
    { U64(0x016C8E5C, 0xA239028E), U64(0x3BC0267F, 0xC95BF1D2) },
    { U64(0x01C7B1F3, 0xCAC74331), U64(0xCAB0301F, 0xBBB2EE47) },
    { U64(0x011CCF38, 0x5EBC89FF), U64(0x1EAE1E13, 0xD54FD4EC) },
    { U64(0x01640306, 0x766BAC7E), U64(0xE659A598, 0xCAA3CA27) },
    { U64(0x01BD03C8, 0x1406979E), U64(0x9FF00EFE, 0xFD4CBCB1) },
    { U64(0x0116225D, 0x0C841EC3), U64(0x23F6095F, 0x5E4FF5EF) },
    { U64(0x015BAAF4, 0x4FA52673), U64(0xECF38BB7, 0x35E3F36A) },
    { U64(0x01B295B1, 0x638E7010), U64(0xE8306EA5, 0x035CF045) },
    { U64(0x010F9D8E, 0xDE39060A), U64(0x911E4527, 0x221A162B) },
    { U64(0x015384F2, 0x95C7478D), U64(0x3565D670, 0xEAA09BB6) },
    { U64(0x01A8662F, 0x3B391970), U64(0x82BF4C0D, 0x2548C2A3) },
    { U64(0x01093FDD, 0x8503AFE6), U64(0x51B78F88, 0x374D79A6) },
    { U64(0x014B8FD4, 0xE6449BDF), U64(0xE625736A, 0x4520D810) },
    { U64(0x019E73CA, 0x1FD5C2D7), U64(0xDFAED044, 0xD6690E14) },
    { U64(0x0103085E, 0x53E599C6), U64(0xEBCD422B, 0x0601A8CC) },
    { U64(0x0143CA75, 0xE8DF0038), U64(0xA6C092B5, 0xC78212FF) },
    { U64(0x0194BD13, 0x6316C046), U64(0xD070B763, 0x396297BF) },
    { U64(0x01F9EC58, 0x3BDC7058), U64(0x848CE53C, 0x07BB3DAF) },
    { U64(0x013C33B7, 0x2569C637), U64(0x52D80F45, 0x84D5068D) },
    { U64(0x018B40A4, 0xEEC437C5), U64(0x278E1316, 0xE60A4831) }
};

/**
 Convert exponent from base 2 to base 10.
    => floor(log10(pow(2, exp)))
    => exp * log10(2)
    => exp * 78913 / 262144; (exp <= 1650)
 */
static_inline u32 exp_base2to10(u32 exp) {
    return (exp * (u32)78913) >> 18;
}

/**
 Convert exponent from base 5 to base 10.
    => floor(log10(pow(5, exp)))
    => exp * log10(5)
    => exp * 183231 / 262144; (exp <= 1650)
 */
static_inline u32 exp_base5to10(u32 exp) {
    return (exp * (u32)183231) >> 18;
}

/**
 Returns bits count of 5^exp.
    => (exp == 0) ? 1 : ceil(log2(pow(5, exp)))
    => (exp == 0) ? 1 : ceil(e * log2(5))
    => (exp * 152170 / 65536) + 1; (exp <= 642)
 */
static_inline u32 exp_base5bits(u32 exp) {
    return ((exp * (u32)152170) >> 16) + 1;
}

/** Returns whether val is divisible by 5^exp (val should not be 0). */
static_inline bool u64_is_divisible_by_pow5(u64 val, u32 exp) {
    while ((i32)exp-- > 0) {
        u64 div = val / 5;
        u64 mod = val - (div * 5);
        if (mod) return (i32)exp < 0;
        val = div;
    }
    return true;
}

/** Returns whether val is divisible by 2^exp (val should not be 0). */
static_inline bool u64_is_divisible_by_pow2(u64 val, u32 exp) {
    return u64_tz_bits(val) >= exp;
}

/** Returns (a * b) >> shr, shr should in range [64, 128]  */
static_inline u64 u128_mul_shr(u64 a, u64 b_hi, u64 b_lo, u64 shr) {
#if YYJSON_HAS_INT128
    u128 r_lo = (u128)a * b_lo;
    u128 r_hi = (u128)a * b_hi;
    return (u64)(((r_lo >> 64) + r_hi) >> (shr - 64));
#else
    u64 r0_hi, r0_lo, r1_hi, r1_lo;
    u128_mul(a, b_lo, &r0_hi, &r0_lo);
    u128_mul_add(a, b_hi, r0_hi, &r1_hi, &r1_lo);
    return (r1_hi << (128 - shr)) | (r1_lo >> (shr - 64));
#endif
}

/** Returns the number of digits in decimal.
    It was used to print floating-point number, the max length should be 17. */
static_inline u32 u64_dec_len(u64 u) {
#if GCC_HAS_CLZLL | MSC_HAS_BIT_SCAN_64 | MSC_HAS_BIT_SCAN
    const u64 powers_of_10[] = {
        (u64)0UL,
        (u64)10UL,
        (u64)100UL,
        (u64)1000UL,
        (u64)10000UL,
        (u64)100000UL,
        (u64)1000000UL,
        (u64)10000000UL,
        (u64)100000000UL,
        (u64)1000000000UL,
        (u64)1000000000UL * 10UL,
        (u64)1000000000UL * 100UL,
        (u64)1000000000UL * 1000UL,
        (u64)1000000000UL * 10000UL,
        (u64)1000000000UL * 100000UL,
        (u64)1000000000UL * 1000000UL,
        (u64)1000000000UL * 10000000UL,
        (u64)1000000000UL * 100000000UL,
        (u64)1000000000UL * 1000000000UL
    };
    u32 t = (64 - u64_lz_bits(u | 1)) * 1233 >> 12;
    return t - (u < powers_of_10[t]) + 1;
#else
    if (u >= (u64)1000000000UL * 10000000UL) return 17;
    if (u >= (u64)1000000000UL * 1000000UL) return 16;
    if (u >= (u64)1000000000UL * 100000UL) return 15;
    if (u >= (u64)1000000000UL * 10000UL) return 14;
    if (u >= (u64)1000000000UL * 1000UL) return 13;
    if (u >= (u64)1000000000UL * 100UL) return 12;
    if (u >= (u64)1000000000UL * 10UL) return 11;
    if (u >= (u64)1000000000UL) return 10;
    if (u >= (u64)100000000UL) return 9;
    if (u >= (u64)10000000UL) return 8;
    if (u >= (u64)1000000UL) return 7;
    if (u >= (u64)100000UL) return 6;
    if (u >= (u64)10000UL) return 5;
    if (u >= (u64)1000UL) return 4;
    if (u >= (u64)100UL) return 3;
    if (u >= (u64)10UL) return 2;
    return 1;
#endif
}

/** Write an unsigned integer with a length of 1 to 17. */
static_inline u8 *write_u64_len_1_17(u64 val, u8 *buf) {
    u64 hgh;
    u32 mid, low, one;
    
    if (val < 100000000) { /* 1-8 digits */
        buf = write_u32_len_1_8((u32)val, buf);
        return buf;
        
    } else if (val < (u64)100000000 * 100000000) { /* 9-16 digits */
        hgh = val / 100000000;
        low = (u32)(val - hgh * 100000000); /* (val % 100000000) */
        buf = write_u32_len_1_8((u32)hgh, buf);
        buf = write_u32_len_8(low, buf);
        return buf;
        
    } else { /* 17 digits */
        hgh = val / 100000000;
        low = (u32)(val - hgh * 100000000); /* (val % 100000000) */
        one = (u32)(hgh / 100000000);
        mid = (u32)(hgh - (u64)one * 100000000); /* (hgh % 100000000) */
        *buf++ = (u8)one + (u8)'0';
        buf = write_u32_len_8(mid, buf);
        buf = write_u32_len_8(low, buf);
        return buf;
    }
}

/**  Write multiple '0', count should in range 0 to 20. */
static_inline u8 *write_zeros(u8 *cur, u32 count) {
    u8 *end = cur + count;
    *(v32 *)&cur[0] = v32_make('0','0','0','0');
    if (count <= 4) return end;
    *(v32 *)&cur[4] = v32_make('0','0','0','0');
    if (count <= 8) return end;
    *(v32 *)&cur[8] = v32_make('0','0','0','0');
    if (count <= 12) return end;
    *(v32 *)&cur[12] = v32_make('0','0','0','0');
    if (count <= 16) return end;
    *(v32 *)&cur[16] = v32_make('0','0','0','0');
    return end;
}

/** Write a signed integer in the range -324 to 308. */
static_inline u8 *write_f64_exp(i32 exp, u8 *buf) {
    buf[0] = '-';
    buf += exp < 0;
    exp = exp < 0 ? -exp : exp;
    if (exp < 100) {
        u32 lz = exp < 10;
        *(v16 *)&buf[0] = *(v16 *)&digit_table[exp * 2 + lz];
        return buf - lz + 2;
    } else {
        u32 hi = ((u32)exp * 656) >> 16; /* exp / 100 */
        u32 lo = exp - hi * 100; /* exp % 100 */
        buf[0] = (u8)hi + (u8)'0';
        *(v16 *)&buf[1] = *(v16 *)&digit_table[lo * 2];
        return buf + 3;
    }
}

/**
 Convert double number from binary to a shortest decimal representation.
 
 This algorithm refers to Ulf Adams's Ryu algorithm.
 Code is rewritten for better performance.
 Paper: https://dl.acm.org/citation.cfm?id=3192369
 Project: https://github.com/ulfjack/ryu
 
 @param sig_raw IEEE-754 significand part in binary.
 @param exp_raw IEEE-754 exponent part in binary.
 @param sig_dec Output shortest significand in decimal.
 @param exp_dec Output exponent in decimal.
 */
static_inline void f64_to_dec(u64 sig_raw, u32 exp_raw,
                              u64 *sig_dec, i32 *exp_dec) {
    i32 exp_bin; /* exponent base 2 */
    u64 sig_bin; /* significand base 2 */
    i32 exp; /* exponent base 10 */
    u64 sig; /* significand base 10 */
    u64 sig_up; /* upper halfway between sig and next ulp */
    u64 sig_lo; /* lower halfway between sig and prev ulp */
    
    u8 last_trim_num = 0;
    u64 sig_up_div;
    u64 sig_lo_div;
    u64 sig_lo_mod;
    u64 sig_div;
    u64 sig_mod;
    
    bool sig_lo_end_zero = false;
    bool sig_end_zero = false;
    bool sig_not_zero = (sig_raw != 0);
    bool accept_bounds = ((sig_raw & 1) == 0);
    
    if (exp_raw != 0) { /* normal */
        exp_bin = (i32)exp_raw - F64_EXP_BIAS - F64_SIG_BITS;
        sig_bin = sig_raw | ((u64)1 << F64_SIG_BITS);
        
        /* fast path for integer number which does not have decimal part */
        if (-F64_SIG_BITS <= exp_bin && exp_bin <= 0) {
            if (u64_tz_bits(sig_bin) >= (u32)-exp_bin) {
                sig = sig_bin >> -exp_bin;
                exp = 0;
                while (true) {
                    u64 div = sig / 10;
                    if (sig > 10 * div) break;
                    sig = div;
                    exp++;
                }
                *sig_dec = sig;
                *exp_dec = exp;
                return;
            }
        }
        
        exp_bin -= 2;
        sig_bin <<= 2;
    } else { /* subnormal */
        exp_bin = 1 - F64_EXP_BIAS - F64_SIG_BITS - 2;
        sig_bin = sig_raw << 2;
    }
    
    if (exp_bin < 0) {
        i32 e10 = exp_base5to10((u32)(-exp_bin) - (exp_bin < -1));
        i32 neg = -(e10 + exp_bin);
        i32 shr = e10 - exp_base5bits((u32)neg) + POW5_SIG_BITS;
        u64 mul_hi = pow5_sig_table[neg][0];
        u64 mul_lo = pow5_sig_table[neg][1];
        u32 sig_up_dist = 2;
        u32 sig_lo_dist = 1 + sig_not_zero;
        sig_up = u128_mul_shr(sig_bin + sig_up_dist, mul_hi, mul_lo, (u64)shr);
        sig_lo = u128_mul_shr(sig_bin - sig_lo_dist, mul_hi, mul_lo, (u64)shr);
        sig    = u128_mul_shr(sig_bin,               mul_hi, mul_lo, (u64)shr);
        exp = -neg;
        if (e10 <= 1) {
            sig_end_zero = true;
            if (accept_bounds) {
                sig_lo_end_zero = sig_not_zero;
            } else {
                --sig_up;
            }
        } else if (e10 < 63) {
            sig_end_zero = u64_is_divisible_by_pow2(sig_bin, e10 - 1);
        }
    } else {
        u32 e10 = exp_base2to10(exp_bin) - (exp_bin > 3);
        i32 shr = -exp_bin + e10 + POW5_INV_SIG_BITS + exp_base5bits(e10) - 1;
        u64 mul_hi = pow5_inv_sig_table[e10][0];
        u64 mul_lo = pow5_inv_sig_table[e10][1];
        u32 sig_up_dist = 2;
        u32 sig_lo_dist = 1 + sig_not_zero;
        sig_up = u128_mul_shr(sig_bin + sig_up_dist, mul_hi, mul_lo, (u64)shr);
        sig_lo = u128_mul_shr(sig_bin - sig_lo_dist, mul_hi, mul_lo, (u64)shr);
        sig    = u128_mul_shr(sig_bin,               mul_hi, mul_lo, (u64)shr);
        exp = e10;
        if (e10 <= 21) {
            if ((sig_bin % 5) == 0) {
                sig_end_zero = u64_is_divisible_by_pow5(sig_bin, e10);
            } else if (accept_bounds) {
                sig_lo_end_zero = u64_is_divisible_by_pow5(sig_bin - 1 -
                                                           sig_not_zero, e10);
            } else {
                sig_up -= u64_is_divisible_by_pow5(sig_bin + 2, e10);
            }
        }
    }
    
    if (!(sig_lo_end_zero | sig_end_zero)) {
        bool round_up = false;
        
#define if_can_trim(div, len) \
        sig_up_div = sig_up / div; \
        sig_lo_div = sig_lo / div; \
        if (sig_up_div > sig_lo_div)
        
#define do_trim(div, len) \
        sig_div = sig / div; \
        sig_mod = sig - div * sig_div; \
        round_up = (sig_mod >= div / 2); \
        sig = sig_div; \
        sig_up = sig_up_div; \
        sig_lo = sig_lo_div; \
        exp += len;
        
        /* optimize for numbers with few digits */
        if_can_trim(((u64)1000000 * 1000000), 12) {
            do_trim(((u64)1000000 * 1000000), 12);
            if_can_trim(10000, 4) {
                do_trim(10000, 4);
            }
        } else {
            if_can_trim(1000000, 6) {
                do_trim(1000000, 6);
            }
        }
        if_can_trim(100, 2) {
            do_trim(100, 2);
        }
        while (true) {
            if_can_trim(10, 1) {
                do_trim(10, 1);
            } else break;
            if_can_trim(10, 1) {
                do_trim(10, 1);
            } else break;
            if_can_trim(10, 1) {
                do_trim(10, 1);
            } else break;
            break;
        }
        sig += (sig == sig_lo) | round_up;
        *sig_dec = sig;
        *exp_dec = exp;
        
    } else {
        while (true) {
            sig_up_div = sig_up / 10;
            sig_lo_div = sig_lo / 10;
            if (sig_up_div <= sig_lo_div) break;
            
            sig_div = sig / 10;
            sig_mod = sig - sig_div * 10;
            sig_end_zero &= (last_trim_num == 0);
            last_trim_num = (u8)sig_mod;
            sig_lo_mod = sig_lo - sig_lo_div * 10;
            sig_lo_end_zero &= (sig_lo_mod == 0);
            sig = sig_div;
            sig_up = sig_up_div;
            sig_lo = sig_lo_div;
            exp++;
        }
        
        if (sig_lo_end_zero) {
            while (true) {
                sig_lo_div = sig_lo / 10;
                sig_lo_mod = sig_lo - sig_lo_div * 10;
                if (sig_lo_mod != 0) break;
                
                sig_up_div = sig_up / 10;
                sig_div = sig / 10;
                sig_mod = sig - sig_div * 10;
                sig_end_zero &= (last_trim_num == 0);
                last_trim_num = (u8)sig_mod;
                sig = sig_div;
                sig_up = sig_up_div;
                sig_lo = sig_lo_div;
                exp++;
            }
        }
        if (sig_end_zero && last_trim_num == 5 && sig % 2 == 0) {
            last_trim_num = 4;
        }
        if (sig == sig_lo && (!accept_bounds || !sig_lo_end_zero)) {
            sig++;
        } else if (last_trim_num >= 5) {
            sig++;
        }
        
        *sig_dec = sig;
        *exp_dec = exp;
    }
}

/** 
 Write a double number (require 32 bytes). 
 
 We follows the ECMAScript specification to print floating point numbers, 
 but with the following changes:
 1. Keep the negative sign of 0.0 to preserve input information.
 2. Keep decimal point to indicate the number is floating point.
 3. Remove positive sign of exponent part.
*/
static_noinline u8 *write_f64_raw(u8 *buf, u64 raw, bool allow_nan_and_inf) {
    
    u64 sig; /* significand in decimal */
    i32 exp; /* exponent in decimal */
    i32 sig_len; /* significand digit count */
    i32 dot_pos; /* decimal point position in significand digit sequence */
    
    /* decode from raw bytes with IEEE-754 double format. */
    bool sign = (bool)(raw >> (F64_BITS - 1));
    u64 sig_raw = raw & F64_SIG_MASK;
    u32 exp_raw = (u32)((raw & F64_EXP_MASK) >> F64_SIG_BITS);
    
    buf[0] = '-';
    buf += sign;
    
    if (unlikely(exp_raw == ((u32)1 << F64_EXP_BITS) - 1)) {
        /* nan or inf */
        if (allow_nan_and_inf) {
            if (sig_raw == 0) {
                *(v32 *)&buf[0] = v32_make('I', 'n', 'f', 'i');
                *(v32 *)&buf[4] = v32_make('n', 'i', 't', 'y');
                return buf + 8;
            } else {
                buf -= sign;
                *(v32 *)&buf[0] = v32_make('N', 'a', 'N', '\0');
                return buf + 3;
            }
        } else {
            return NULL;
        }
    }
    
    if ((raw << 1) == 0) {
        *(v32 *)&buf[0] = v32_make('0', '.', '0', '\0');
        return buf + 3;
    }
    
    /* get shortest decimal representation */
    f64_to_dec(sig_raw, exp_raw, &sig, &exp);
    sig_len = u64_dec_len(sig);
    dot_pos = sig_len + exp;
    
    if (0 < dot_pos && dot_pos <= 21) {
        /* dot is after the first digit, digit count <= 21 */
        if (sig_len <= dot_pos) {
            /* dot is after last digit, e.g. 123e2 -> 12300 */
            buf = write_u64_len_1_17(sig, buf);
            buf = write_zeros(buf, exp);
            *(v16 *)buf = v16_make('.', '0');
            buf += 2;
            return buf;
        } else {
            /* dot is inside the digits, e.g. 123e-2 -> 1.23 */
            u8 *end = write_u64_len_1_17(sig, buf + 1);
            while(dot_pos-- > 0) {
                buf[0] = buf[1];
                buf++;
            }
            *buf = '.';
            return end;
        }
    } else if (-6 < dot_pos && dot_pos <= 0) {
        /* dot is before first digit, and the padding zero count < 6,
           e.g. 123e-8 -> 0.00000123 */
        *(v16 *)buf = v16_make('0', '.');
        buf = write_zeros(buf + 2, -dot_pos);
        buf = write_u64_len_1_17(sig, buf);
        return buf;
    } else {
        if (sig_len == 1) {
            /* single digit, no need dot, e.g. 1e45 */
            buf[0] = (u8)sig + (u8)'0';
            buf[1] = 'e';
            buf = write_f64_exp(dot_pos - 1, buf + 2);
            return buf;
        } else {
            /* format exponential, e.g. 1.23e45 */
            u8 *hdr = buf;
            buf = write_u64_len_1_17(sig, buf + 1);
            hdr[0] = hdr[1];
            hdr[1] = '.';
            buf[0] = 'e';
            buf = write_f64_exp(dot_pos - 1, buf + 1);
            return buf;
        }
    }
}

#else /* FP_WRITER */

/** Write a double number (require 32 bytes). */
static_noinline u8 *write_f64_raw(u8 *buf, u64 raw, bool allow_nan_and_inf) {
    f64 val = f64_from_raw(raw);
    if (f64_isfinite(val)) {
#if _MSC_VER >= 1400
        int len = sprintf_s((char *)buf, 32, "%.17g", val);
#else
        int len = sprintf((char *)buf, "%.17g", val);
#endif
        return buf + len;
    } else {
        /* nan or inf */
        if (allow_nan_and_inf) {
            if (f64_isinf(val)) {
                buf[0] = '-';
                buf += val < 0;
                *(v32 *)&buf[0] = v32_make('I', 'n', 'f', 'i');
                *(v32 *)&buf[4] = v32_make('n', 'i', 't', 'y');
                buf += 8;
                return buf;
            } else {
                *(v32 *)&buf[0] = v32_make('N', 'a', 'N', '\0');
                return buf + 3;
            }
        } else {
            return NULL;
        }
    }
}

#endif /* FP_WRITER */

/** Write a JSON number (require 32 bytes). */
static_inline u8 *write_number(u8 *cur, yyjson_val *val,
                               bool allow_nan_and_inf) {
    if (val->tag & YYJSON_SUBTYPE_REAL) {
        u64 raw = val->uni.u64;
        return write_f64_raw(cur, raw, allow_nan_and_inf);
    } else {
        u64 pos = val->uni.u64;
        u64 neg = ~pos + 1;
        usize sgn = ((val->tag & YYJSON_SUBTYPE_SINT) > 0) & ((i64)pos < 0);
        *cur = '-';
        return write_u64(sgn ? neg : pos, cur + sgn);
    }
}



/*==============================================================================
 * String Writer
 *============================================================================*/

/** Character escape type. */
typedef u8 char_esc_type;
#define CHAR_ESC_NONE   0 /* Character do not need to be escaped. */
#define CHAR_ESC_ASCII  1 /* ASCII character, escaped as '\x'. */
#define CHAR_ESC_UTF8_1 2 /* 1-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_2 3 /* 2-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_3 4 /* 3-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_4 5 /* 4-byte UTF-8 character, escaped as two '\uXXXX'. */

/** Character escape type table: don't escape unicode, don't escape '/'.
    (generate with misc/make_tables.c) */
static const char_esc_type esc_table_default[256] = {
    2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Character escape type table: don't escape unicode, escape '/'.
    (generate with misc/make_tables.c) */
static const char_esc_type esc_table_default_with_slashes[256] = {
    2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Character escape type table: escape unicode, don't escape '/'.
    (generate with misc/make_tables.c) */
static const char_esc_type esc_table_unicode[256] = {
    2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Character escape type table: escape unicode, escape '/'.
    (generate with misc/make_tables.c) */
static const char_esc_type esc_table_unicode_with_slashes[256] = {
    2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Escaped hex character table: ["00" "01" "02" ... "FD" "FE" "FF"].
    (generate with misc/make_tables.c) */
yyjson_align(2)
static const u8 esc_hex_char_table[512] = {
    '0', '0', '0', '1', '0', '2', '0', '3',
    '0', '4', '0', '5', '0', '6', '0', '7',
    '0', '8', '0', '9', '0', 'A', '0', 'B',
    '0', 'C', '0', 'D', '0', 'E', '0', 'F',
    '1', '0', '1', '1', '1', '2', '1', '3',
    '1', '4', '1', '5', '1', '6', '1', '7',
    '1', '8', '1', '9', '1', 'A', '1', 'B',
    '1', 'C', '1', 'D', '1', 'E', '1', 'F',
    '2', '0', '2', '1', '2', '2', '2', '3',
    '2', '4', '2', '5', '2', '6', '2', '7',
    '2', '8', '2', '9', '2', 'A', '2', 'B',
    '2', 'C', '2', 'D', '2', 'E', '2', 'F',
    '3', '0', '3', '1', '3', '2', '3', '3',
    '3', '4', '3', '5', '3', '6', '3', '7',
    '3', '8', '3', '9', '3', 'A', '3', 'B',
    '3', 'C', '3', 'D', '3', 'E', '3', 'F',
    '4', '0', '4', '1', '4', '2', '4', '3',
    '4', '4', '4', '5', '4', '6', '4', '7',
    '4', '8', '4', '9', '4', 'A', '4', 'B',
    '4', 'C', '4', 'D', '4', 'E', '4', 'F',
    '5', '0', '5', '1', '5', '2', '5', '3',
    '5', '4', '5', '5', '5', '6', '5', '7',
    '5', '8', '5', '9', '5', 'A', '5', 'B',
    '5', 'C', '5', 'D', '5', 'E', '5', 'F',
    '6', '0', '6', '1', '6', '2', '6', '3',
    '6', '4', '6', '5', '6', '6', '6', '7',
    '6', '8', '6', '9', '6', 'A', '6', 'B',
    '6', 'C', '6', 'D', '6', 'E', '6', 'F',
    '7', '0', '7', '1', '7', '2', '7', '3',
    '7', '4', '7', '5', '7', '6', '7', '7',
    '7', '8', '7', '9', '7', 'A', '7', 'B',
    '7', 'C', '7', 'D', '7', 'E', '7', 'F',
    '8', '0', '8', '1', '8', '2', '8', '3',
    '8', '4', '8', '5', '8', '6', '8', '7',
    '8', '8', '8', '9', '8', 'A', '8', 'B',
    '8', 'C', '8', 'D', '8', 'E', '8', 'F',
    '9', '0', '9', '1', '9', '2', '9', '3',
    '9', '4', '9', '5', '9', '6', '9', '7',
    '9', '8', '9', '9', '9', 'A', '9', 'B',
    '9', 'C', '9', 'D', '9', 'E', '9', 'F',
    'A', '0', 'A', '1', 'A', '2', 'A', '3',
    'A', '4', 'A', '5', 'A', '6', 'A', '7',
    'A', '8', 'A', '9', 'A', 'A', 'A', 'B',
    'A', 'C', 'A', 'D', 'A', 'E', 'A', 'F',
    'B', '0', 'B', '1', 'B', '2', 'B', '3',
    'B', '4', 'B', '5', 'B', '6', 'B', '7',
    'B', '8', 'B', '9', 'B', 'A', 'B', 'B',
    'B', 'C', 'B', 'D', 'B', 'E', 'B', 'F',
    'C', '0', 'C', '1', 'C', '2', 'C', '3',
    'C', '4', 'C', '5', 'C', '6', 'C', '7',
    'C', '8', 'C', '9', 'C', 'A', 'C', 'B',
    'C', 'C', 'C', 'D', 'C', 'E', 'C', 'F',
    'D', '0', 'D', '1', 'D', '2', 'D', '3',
    'D', '4', 'D', '5', 'D', '6', 'D', '7',
    'D', '8', 'D', '9', 'D', 'A', 'D', 'B',
    'D', 'C', 'D', 'D', 'D', 'E', 'D', 'F',
    'E', '0', 'E', '1', 'E', '2', 'E', '3',
    'E', '4', 'E', '5', 'E', '6', 'E', '7',
    'E', '8', 'E', '9', 'E', 'A', 'E', 'B',
    'E', 'C', 'E', 'D', 'E', 'E', 'E', 'F',
    'F', '0', 'F', '1', 'F', '2', 'F', '3',
    'F', '4', 'F', '5', 'F', '6', 'F', '7',
    'F', '8', 'F', '9', 'F', 'A', 'F', 'B',
    'F', 'C', 'F', 'D', 'F', 'E', 'F', 'F'
};

/** Escaped single character table. (generate with misc/make_tables.c) */
yyjson_align(2)
static const u8 esc_single_char_table[512] = {
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '\\', 'b', '\\', 't', '\\', 'n', ' ', ' ',
    '\\', 'f', '\\', 'r', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', '\\', '"', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', '\\', '/',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '\\', '\\', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
};

/** Returns escape table with options. */
static_inline const char_esc_type *get_esc_table_with_flag(
    yyjson_read_flag flg) {
    if (unlikely(flg & YYJSON_WRITE_ESCAPE_UNICODE)) {
        if (unlikely(flg & YYJSON_WRITE_ESCAPE_SLASHES)) {
            return esc_table_unicode_with_slashes;
        } else {
            return esc_table_unicode;
        }
    } else {
        if (unlikely(flg & YYJSON_WRITE_ESCAPE_SLASHES)) {
            return esc_table_default_with_slashes;
        } else {
            return esc_table_default;
        }
    }
}

/**
 Write UTF-8 string (require len * 6 + 2 bytes).
 If the input string is not valid UTF-8 encoding, undefined behavior may occur.
 @param cur Buffer cursor.
 @param str A valid UTF-8 string with null-terminator.
 @param str_len Length of string in bytes.
 @param esc_table Escape type table for character escaping.
 @return The buffer cursor after string.
 */
static_inline u8 *write_string(u8 *cur,
                               const u8 *str,
                               usize str_len,
                               const char_esc_type *esc_table) {
    const u8 *end = str + str_len;
    *cur++ = '"';
    
copy_char:
    /*
     Copy continuous unescaped char, loop unrolling, same as the following code:
     
         while (true) repeat16({
            if (unlikely(esc_table[*str] != CHAR_ESC_NONE)) break;
            *cur++ = *str++;
         });
     */
#define expr_jump(i) \
    if (unlikely(esc_table[str[i]] != CHAR_ESC_NONE)) goto stop_char_##i;

#define expr_stop(i) \
    stop_char_##i: \
    memcpy(cur, str, i); \
    cur += i; str += i; goto copy_next;

    repeat16_incr(expr_jump);
    memcpy(cur, str, 16);
    cur += 16; str += 16;
    goto copy_char;
    repeat16_incr(expr_stop);

#undef expr_jump
#undef expr_stop
    
copy_next:
    while (str < end) {
        switch (esc_table[*str]) {
            case CHAR_ESC_NONE: {
                *cur++ = *str++;
                goto copy_char;
            }
            case CHAR_ESC_ASCII: {
                *(v16 *)cur = ((v16 *)esc_single_char_table)[*str];
                cur += 2;
                str += 1;
                continue;
            }
            case CHAR_ESC_UTF8_1: {
                ((v32 *)cur)[0] = v32_make('\\', 'u', '0', '0');
                ((v16 *)cur)[2] = ((v16 *)esc_hex_char_table)[*str];
                cur += 6;
                str += 1;
                continue;
            }
            case CHAR_ESC_UTF8_2: {
                u16 u = (u16)(((u16)(str[0] & 0x1F) << 6) |
                              ((u16)(str[1] & 0x3F) << 0));
                ((v16 *)cur)[0] = v16_make('\\', 'u');
                ((v16 *)cur)[1] = ((v16 *)esc_hex_char_table)[u >> 8];
                ((v16 *)cur)[2] = ((v16 *)esc_hex_char_table)[u & 0xFF];
                cur += 6;
                str += 2;
                continue;
            }
            case CHAR_ESC_UTF8_3: {
                u16 u = (u16)(((u16)(str[0] & 0x0F) << 12) |
                              ((u16)(str[1] & 0x3F) << 6) |
                              ((u16)(str[2] & 0x3F) << 0));
                ((v16 *)cur)[0] = v16_make('\\', 'u');
                ((v16 *)cur)[1] = ((v16 *)esc_hex_char_table)[u >> 8];
                ((v16 *)cur)[2] = ((v16 *)esc_hex_char_table)[u & 0xFF];
                cur += 6;
                str += 3;
                continue;
            }
            case CHAR_ESC_UTF8_4: {
                u32 hi, lo;
                u32 u = ((u32)(str[0] & 0x07) << 18) |
                        ((u32)(str[1] & 0x3F) << 12) |
                        ((u32)(str[2] & 0x3F) << 6) |
                        ((u32)(str[3] & 0x3F) << 0);
                u -= 0x10000;
                hi = (u >> 10) + 0xD800;
                lo = (u & 0x3FF) + 0xDC00;
                ((v16 *)cur)[0] = v16_make('\\', 'u');
                ((v16 *)cur)[1] = ((v16 *)esc_hex_char_table)[hi >> 8];
                ((v16 *)cur)[2] = ((v16 *)esc_hex_char_table)[hi & 0xFF];
                ((v16 *)cur)[3] = v16_make('\\', 'u');
                ((v16 *)cur)[4] = ((v16 *)esc_hex_char_table)[lo >> 8];
                ((v16 *)cur)[5] = ((v16 *)esc_hex_char_table)[lo & 0xFF];
                cur += 12;
                str += 4;
                continue;
            }
            default:
                break;
        }
    }
    
copy_end:
    *cur++ = '"';
    return cur;
}

/*==============================================================================
 * Writer Utilities
 *============================================================================*/

/** Write null (require 8 bytes). */
static_inline u8 *write_null(u8 *cur) {
    *(v64 *)cur = v64_make('n', 'u', 'l', 'l', ',', '\n', 0, 0);
    return cur + 4;
}

/** Write bool (require 8 bytes). */
static_inline u8 *write_bool(u8 *cur, bool val) {
    *(v64 *)cur = val ? v64_make('t', 'r', 'u', 'e', ',', '\n', 0, 0) :
                        v64_make('f', 'a', 'l', 's', 'e', ',', '\n', 0);
    return cur + 5 - val;
}

/** Write indent (require level * 4 bytes). */
static_inline u8 *write_indent(u8 *cur, usize level) {
    while (level --> 0) {
        *(v32 *)cur = v32_make(' ', ' ', ' ', ' ');
        cur += 4;
    }
    return cur;
}

/** Write data to file. */
static bool write_dat_to_file(const char *path, u8 *dat, usize len,
                              yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    err->msg = _msg; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    if (file) fclose(file); \
    return false; \
} while(false)
    
    FILE *file = NULL;
    
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, "wb") != 0) {
        return_err(FILE_OPEN, "file opening failed");
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL) {
        return_err(FILE_OPEN, "file opening failed");
    }
    if (fwrite(dat, len, 1, file) != 1) {
        return_err(FILE_WRITE, "file writing failed");
    }
    if (fclose(file) != 0) {
        file = NULL;
        return_err(FILE_WRITE, "file closing failed");
    }
    
    return true;
#undef return_err
}



/*==============================================================================
 * JSON Writer Implementation
 *============================================================================*/

typedef struct yyjson_write_ctx {
    usize tag;
} yyjson_write_ctx;

static_inline void yyjson_write_ctx_set(yyjson_write_ctx *ctx,
                                        usize size, bool is_obj) {
    ctx->tag = (size << 1) | (usize)is_obj;
}

static_inline void yyjson_write_ctx_get(yyjson_write_ctx *ctx,
                                        usize *size, bool *is_obj) {
    usize tag = ctx->tag;
    *size = tag >> 1;
    *is_obj = (bool)(tag & 1);
}

/** Write single JSON value. */
static_inline u8 *yyjson_write_single(yyjson_val *val,
                                      yyjson_write_flag flg,
                                      yyjson_alc alc,
                                      usize *dat_len,
                                      yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    if (hdr) alc.free(alc.ctx, (void *)hdr); \
    *dat_len = 0; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    err->msg = _msg; \
    return NULL; \
} while(false)
    
#define incr_len(_len) do { \
    hdr = (u8 *)alc.malloc(alc.ctx, _len); \
    if (!hdr) goto fail_alloc; \
    cur = hdr; \
} while(false)
    
#define check_str_len(_len) do { \
    if ((USIZE_MAX < U64_MAX) && (_len >= (USIZE_MAX - 16) / 6)) \
        goto fail_alloc; \
} while(false)
    
    u8 *hdr = NULL, *cur;
    usize str_len;
    const u8 *str_ptr;
    bool allow_nan_and_inf = (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) > 0;
    const char_esc_type *esc_table = get_esc_table_with_flag(flg);
    
    switch (unsafe_yyjson_get_type(val)) {
        case YYJSON_TYPE_STR:
            str_len = unsafe_yyjson_get_len(val);
            str_ptr = (const u8 *)unsafe_yyjson_get_str(val);
            check_str_len(str_len);
            incr_len(str_len * 6 + 4);
            cur = write_string(cur, str_ptr, str_len, esc_table);
            break;
            
        case YYJSON_TYPE_NUM:
            incr_len(32);
            cur = write_number(cur, val, allow_nan_and_inf);
            if (unlikely(!cur)) goto fail_num;
            break;
            
        case YYJSON_TYPE_BOOL:
            incr_len(8);
            cur = write_bool(cur, unsafe_yyjson_get_bool(val));
            break;
            
        case YYJSON_TYPE_NULL:
            incr_len(8);
            cur = write_null(cur);
            break;
            
        case YYJSON_TYPE_ARR:
            incr_len(4);
            *(v16 *)cur = v16_make('[', ']');
            cur += 2;
            break;
            
        case YYJSON_TYPE_OBJ:
            incr_len(4);
            *(v16 *)cur = v16_make('{', '}');
            cur += 2;
            break;
            
        default:
            goto fail_type;
    }
    
    *cur = '\0';
    *dat_len = cur - hdr;
    memset(err, 0, sizeof(yyjson_write_err));
    return hdr;
    
fail_alloc:
    return_err(MEMORY_ALLOCATION, "memory allocation failed");
fail_type:
    return_err(INVALID_VALUE_TYPE, "invalid JSON value type");
fail_num:
    return_err(NAN_OR_INF, "nan or inf number is not allowed");
    
#undef return_err
#undef check_str_len
#undef incr_len
}

/** Write JSON document minify.
    The root of this document should be a non-empty container. */
static_inline u8 *yyjson_write_minify(yyjson_doc *doc,
                                      yyjson_write_flag flg,
                                      yyjson_alc alc,
                                      usize *dat_len,
                                      yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    *dat_len = 0; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    err->msg = _msg; \
    if (hdr) alc.free(alc.ctx, hdr); \
    return NULL; \
} while(false)
    
#define incr_len(_len) do { \
    ext_len = _len; \
    if (unlikely((u8 *)(cur + ext_len) >= (u8 *)ctx)) { \
        alc_inc = yyjson_max(alc_len / 2, ext_len); \
        alc_inc = size_align_up(alc_inc, sizeof(yyjson_write_ctx)); \
        if (size_add_is_overflow(alc_len, alc_inc)) goto fail_alloc; \
        alc_len += alc_inc; \
        tmp = (u8 *)alc.realloc(alc.ctx, hdr, alc_len); \
        if (unlikely(!tmp)) goto fail_alloc; \
        ctx_len = end - (u8 *)ctx; \
        ctx_tmp = (yyjson_write_ctx *)(tmp + (alc_len - ctx_len)); \
        memmove((void *)ctx_tmp, (void *)(tmp + ((u8 *)ctx - hdr)), ctx_len); \
        ctx = ctx_tmp; \
        cur = tmp + (cur - hdr); \
        end = tmp + alc_len; \
        hdr = tmp; \
    } \
} while(false)
    
#define check_str_len(_len) do { \
    if ((USIZE_MAX < U64_MAX) && (_len >= (USIZE_MAX - 16) / 6)) \
        goto fail_alloc; \
} while(false)
    
    yyjson_val *val;
    yyjson_type val_type;
    usize ctn_len, ctn_len_tmp;
    bool ctn_obj, ctn_obj_tmp, is_key;
    u8 *hdr, *cur, *end, *tmp;
    yyjson_write_ctx *ctx, *ctx_tmp;
    usize alc_len, alc_inc, ctx_len, ext_len, str_len;
    const u8 *str_ptr;
    bool allow_nan_and_inf = (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) > 0;
    const char_esc_type *esc_table = get_esc_table_with_flag(flg);
    
    alc_len = doc->val_read * YYJSON_WRITER_ESTIMATED_MINIFY_RATIO + 64;
    alc_len = size_align_up(alc_len, sizeof(yyjson_write_ctx));
    hdr = (u8 *)alc.malloc(alc.ctx, alc_len);
    if (!hdr) goto fail_alloc;
    cur = hdr;
    end = hdr + alc_len;
    ctx = (yyjson_write_ctx *)end;
    
doc_begin:
    val = doc->root;
    val_type = unsafe_yyjson_get_type(val);
    ctn_obj = (val_type == YYJSON_TYPE_OBJ);
    ctn_len = unsafe_yyjson_get_len(val) << (u8)ctn_obj;
    *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
    val++;
    
val_begin:
    val_type = unsafe_yyjson_get_type(val);
    switch (val_type) {
        case YYJSON_TYPE_STR:
            is_key = ((u8)ctn_obj & (u8)~ctn_len);
            str_len = unsafe_yyjson_get_len(val);
            str_ptr = (const u8 *)unsafe_yyjson_get_str(val);
            check_str_len(str_len);
            incr_len(str_len * 6 + 16);
            cur = write_string(cur, str_ptr, str_len, esc_table);
            *cur++ = is_key ? ':' : ',';
            break;
            
        case YYJSON_TYPE_NUM:
            incr_len(32);
            cur = write_number(cur, val, allow_nan_and_inf);
            if (unlikely(!cur)) goto fail_num;
            *cur++ = ',';
            break;
            
        case YYJSON_TYPE_BOOL:
            incr_len(16);
            cur = write_bool(cur, unsafe_yyjson_get_bool(val));
            cur++;
            break;
            
        case YYJSON_TYPE_NULL:
            incr_len(16);
            cur = write_null(cur);
            cur++;
            break;
            
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ:
            ctn_len_tmp = unsafe_yyjson_get_len(val);
            ctn_obj_tmp = (val_type == YYJSON_TYPE_OBJ);
            incr_len(16);
            if (unlikely(ctn_len_tmp == 0)) {
                /* write empty container */
                *cur++ = (u8)('[' | ((u8)ctn_obj_tmp << 5));
                *cur++ = (u8)(']' | ((u8)ctn_obj_tmp << 5));
                *cur++ = ',';
                break;
            } else {
                /* push context, setup new container */
                yyjson_write_ctx_set(--ctx, ctn_len, ctn_obj);
                ctn_len = ctn_len_tmp << (u8)ctn_obj_tmp;
                ctn_obj = ctn_obj_tmp;
                *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
                val++;
                goto val_begin;
            }
            
        default:
            goto fail_type;
    }
    
    val++;
    ctn_len--;
    if (unlikely(ctn_len == 0)) goto ctn_end;
    goto val_begin;
    
ctn_end:
    cur--;
    *cur++ = (u8)(']' | ((u8)ctn_obj << 5));
    *cur++ = ',';
    if (unlikely((u8 *)ctx >= end)) goto doc_end;
    yyjson_write_ctx_get(ctx++, &ctn_len, &ctn_obj);
    ctn_len--;
    if (likely(ctn_len > 0)) {
        goto val_begin;
    } else {
        goto ctn_end;
    }
    
doc_end:
    *--cur = '\0';
    *dat_len = cur - hdr;
    memset(err, 0, sizeof(yyjson_write_err));
    return hdr;
    
fail_alloc:
    return_err(MEMORY_ALLOCATION, "memory allocation failed");
fail_type:
    return_err(INVALID_VALUE_TYPE, "invalid JSON value type");
fail_num:
    return_err(NAN_OR_INF, "nan or inf number is not allowed");
    
#undef return_err
#undef incr_len
#undef check_str_len
}

/** Write JSON document pretty.
    The root of this document should be a non-empty container. */
static_inline u8 *yyjson_write_pretty(yyjson_doc *doc,
                                      yyjson_write_flag flg,
                                      yyjson_alc alc,
                                      usize *dat_len,
                                      yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    *dat_len = 0; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    err->msg = _msg; \
    if (hdr) alc.free(alc.ctx, hdr); \
    return NULL; \
} while(false)
    
#define incr_len(_len) do { \
    ext_len = _len; \
    if (unlikely((u8 *)(cur + ext_len) >= (u8 *)ctx)) { \
        alc_inc = yyjson_max(alc_len / 2, ext_len); \
        alc_inc = size_align_up(alc_inc, sizeof(yyjson_write_ctx)); \
        if (size_add_is_overflow(alc_len, alc_inc)) goto fail_alloc; \
        alc_len += alc_inc; \
        tmp = (u8 *)alc.realloc(alc.ctx, hdr, alc_len); \
        if (unlikely(!tmp)) goto fail_alloc; \
        ctx_len = end - (u8 *)ctx; \
        ctx_tmp = (yyjson_write_ctx *)(tmp + (alc_len - ctx_len)); \
        memmove((void *)ctx_tmp, (void *)(tmp + ((u8 *)ctx - hdr)), ctx_len); \
        ctx = ctx_tmp; \
        cur = tmp + (cur - hdr); \
        end = tmp + alc_len; \
        hdr = tmp; \
    } \
} while(false)
    
#define check_str_len(_len) do { \
    if ((USIZE_MAX < U64_MAX) && (_len >= (USIZE_MAX - 16) / 6)) \
        goto fail_alloc; \
} while(false)
    
    yyjson_val *val;
    yyjson_type val_type;
    usize ctn_len, ctn_len_tmp;
    bool ctn_obj, ctn_obj_tmp, is_key, no_indent;
    u8 *hdr, *cur, *end, *tmp;
    yyjson_write_ctx *ctx, *ctx_tmp;
    usize alc_len, alc_inc, ctx_len, ext_len, str_len, level;
    const u8 *str_ptr;
    bool allow_nan_and_inf = (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) > 0;
    const char_esc_type *esc_table = get_esc_table_with_flag(flg);
    
    alc_len = doc->val_read * YYJSON_WRITER_ESTIMATED_PRETTY_RATIO + 64;
    alc_len = size_align_up(alc_len, sizeof(yyjson_write_ctx));
    hdr = (u8 *)alc.malloc(alc.ctx, alc_len);
    if (!hdr) goto fail_alloc;
    cur = hdr;
    end = hdr + alc_len;
    ctx = (yyjson_write_ctx *)end;
    
doc_begin:
    val = doc->root;
    val_type = unsafe_yyjson_get_type(val);
    ctn_obj = (val_type == YYJSON_TYPE_OBJ);
    ctn_len = unsafe_yyjson_get_len(val) << (u8)ctn_obj;
    *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
    *cur++ = '\n';
    val++;
    level = 1;
    
val_begin:
    val_type = unsafe_yyjson_get_type(val);
    switch (val_type) {
        case YYJSON_TYPE_STR:
            is_key = ((u8)ctn_obj & (u8)~ctn_len);
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            str_len = unsafe_yyjson_get_len(val);
            str_ptr = (const u8 *)unsafe_yyjson_get_str(val);
            check_str_len(str_len);
            incr_len(str_len * 6 + 16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_string(cur, str_ptr, str_len, esc_table);
            *cur++ = is_key ? ':' : ',';
            *cur++ = is_key ? ' ' : '\n';
            break;
            
        case YYJSON_TYPE_NUM:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(32 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_number(cur, val, allow_nan_and_inf);
            if (unlikely(!cur)) goto fail_num;
            *cur++ = ',';
            *cur++ = '\n';
            break;
            
        case YYJSON_TYPE_BOOL:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_bool(cur, unsafe_yyjson_get_bool(val));
            cur += 2;
            break;
            
        case YYJSON_TYPE_NULL:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_null(cur);
            cur += 2;
            break;
            
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            ctn_len_tmp = unsafe_yyjson_get_len(val);
            ctn_obj_tmp = (val_type == YYJSON_TYPE_OBJ);
            if (unlikely(ctn_len_tmp == 0)) {
                /* write empty container */
                incr_len(16 + (no_indent ? 0 : level * 4));
                cur = write_indent(cur, no_indent ? 0 : level);
                *cur++ = (u8)('[' | ((u8)ctn_obj_tmp << 5));
                *cur++ = (u8)(']' | ((u8)ctn_obj_tmp << 5));
                *cur++ = ',';
                *cur++ = '\n';
                break;
            } else {
                /* push context, setup new container */
                incr_len(32 + (no_indent ? 0 : level * 4));
                yyjson_write_ctx_set(--ctx, ctn_len, ctn_obj);
                ctn_len = ctn_len_tmp << (u8)ctn_obj_tmp;
                ctn_obj = ctn_obj_tmp;
                cur = write_indent(cur, no_indent ? 0 : level);
                level++;
                *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
                *cur++ = '\n';
                val++;
                goto val_begin;
            }
            
        default:
            goto fail_type;
    }
    
    val++;
    ctn_len--;
    if (unlikely(ctn_len == 0)) goto ctn_end;
    goto val_begin;
    
ctn_end:
    cur -= 2;
    *cur++ = '\n';
    incr_len(level * 4);
    cur = write_indent(cur, --level);
    *cur++ = (u8)(']' | ((u8)ctn_obj << 5));
    if (unlikely((u8 *)ctx >= end)) goto doc_end;
    yyjson_write_ctx_get(ctx++, &ctn_len, &ctn_obj);
    ctn_len--;
    *cur++ = ',';
    *cur++ = '\n';
    if (likely(ctn_len > 0)) {
        goto val_begin;
    } else {
        goto ctn_end;
    }
    
doc_end:
    *cur = '\0';
    *dat_len = cur - hdr;
    memset(err, 0, sizeof(yyjson_write_err));
    return hdr;
    
fail_alloc:
    return_err(MEMORY_ALLOCATION, "memory allocation failed");
fail_type:
    return_err(INVALID_VALUE_TYPE, "invalid JSON value type");
fail_num:
    return_err(NAN_OR_INF, "nan or inf number is not allowed");
    
#undef return_err
#undef incr_len
#undef check_str_len
}

char *yyjson_write_opts(yyjson_doc *doc,
                        yyjson_write_flag flg,
                        yyjson_alc *alc_ptr,
                        usize *dat_len,
                        yyjson_write_err *err) {
    
    yyjson_write_err dummy_err;
    usize dummy_dat_len;
    yyjson_alc alc;
    
    err = err ? err : &dummy_err;
    dat_len = dat_len ? dat_len : &dummy_dat_len;
    alc = alc_ptr ? *alc_ptr : YYJSON_DEFAULT_ALC;
    
    if (unlikely(!doc)) {
        *dat_len = 0;
        err->msg = "input JSON document is NULL";
        err->code = YYJSON_READ_ERROR_INVALID_PARAMETER;
        return NULL;
    }
    
    if (doc->val_read == 1) {
        return (char *)yyjson_write_single(doc->root, flg, alc, dat_len, err);
    }
    if (flg & YYJSON_WRITE_PRETTY) {
        return (char *)yyjson_write_pretty(doc, flg, alc, dat_len, err);
    } else {
        return (char *)yyjson_write_minify(doc, flg, alc, dat_len, err);
    }
}

bool yyjson_write_file(const char *path,
                       yyjson_doc *doc,
                       yyjson_write_flag flg,
                       yyjson_alc *alc_ptr,
                       yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    err->msg = _msg; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    return false; \
} while(false)
    
    yyjson_write_err dummy_err;
    u8 *dat;
    usize dat_len = 0;
    bool suc;
    
    /* validate input parameters */
    if (!err) err = &dummy_err;
    if (unlikely(!path)) {
        return_err(INVALID_PARAMETER, "input path is NULL");
    }
    if (unlikely(*path == 0)) {
        return_err(INVALID_PARAMETER, "input path is empty");
    }
    
    dat = (u8 *)yyjson_write_opts(doc, flg, alc_ptr, &dat_len, err);
    if (unlikely(!dat)) return false;
    suc = write_dat_to_file(path, dat, dat_len, err);
    if (alc_ptr) alc_ptr->free(alc_ptr->ctx, dat);
    else free(dat);
    return suc;
    
#undef return_err
}



/*==============================================================================
 * Mutable JSON Writer Implementation
 *============================================================================*/

typedef struct yyjson_mut_write_ctx {
    usize tag;
    yyjson_mut_val *ctn;
} yyjson_mut_write_ctx;

static_inline void yyjson_mut_write_ctx_set(yyjson_mut_write_ctx *ctx,
                                            yyjson_mut_val *ctn,
                                            usize size, bool is_obj) {
    ctx->tag = (size << 1) | (usize)is_obj;
    ctx->ctn = ctn;
}

static_inline void yyjson_mut_write_ctx_get(yyjson_mut_write_ctx *ctx,
                                            yyjson_mut_val **ctn,
                                            usize *size, bool *is_obj) {
    usize tag = ctx->tag;
    *size = tag >> 1;
    *is_obj = (bool)(tag & 1);
    *ctn = ctx->ctn;
}

/** Write single JSON value. */
static_inline u8 *yyjson_mut_write_single(yyjson_mut_val *val,
                                          yyjson_write_flag flg,
                                          yyjson_alc alc,
                                          usize *dat_len,
                                          yyjson_write_err *err) {
    return yyjson_write_single((yyjson_val *)val, flg, alc, dat_len, err);
}

/** Write JSON document minify.
    The root of this document should be a non-empty container. */
static_inline u8 *yyjson_mut_write_minify(yyjson_mut_doc *doc,
                                          yyjson_write_flag flg,
                                          yyjson_alc alc,
                                          usize *dat_len,
                                          yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    *dat_len = 0; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    err->msg = _msg; \
    if (hdr) alc.free(alc.ctx, hdr); \
    return NULL; \
} while(false)
    
#define incr_len(_len) do { \
    ext_len = _len; \
    if (unlikely((u8 *)(cur + ext_len) >= (u8 *)ctx)) { \
        alc_inc = yyjson_max(alc_len / 2, ext_len); \
        alc_inc = size_align_up(alc_inc, sizeof(yyjson_mut_write_ctx)); \
        if (size_add_is_overflow(alc_len, alc_inc)) goto fail_alloc; \
        alc_len += alc_inc; \
        tmp = (u8 *)alc.realloc(alc.ctx, hdr, alc_len); \
        if (unlikely(!tmp)) goto fail_alloc; \
        ctx_len = end - (u8 *)ctx; \
        ctx_tmp = (yyjson_mut_write_ctx *)(tmp + (alc_len - ctx_len)); \
        memmove((void *)ctx_tmp, (void *)(tmp + ((u8 *)ctx - hdr)), ctx_len); \
        ctx = ctx_tmp; \
        cur = tmp + (cur - hdr); \
        end = tmp + alc_len; \
        hdr = tmp; \
    } \
} while(false)
    
#define check_str_len(_len) do { \
    if ((USIZE_MAX < U64_MAX) && (_len >= (USIZE_MAX - 16) / 6)) \
        goto fail_alloc; \
} while(false)
    
    yyjson_mut_val *val, *ctn;
    yyjson_type val_type;
    usize ctn_len, ctn_len_tmp;
    bool ctn_obj, ctn_obj_tmp, is_key;
    u8 *hdr, *cur, *end, *tmp;
    yyjson_mut_write_ctx *ctx, *ctx_tmp;
    usize alc_len, alc_inc, ctx_len, ext_len, str_len;
    const u8 *str_ptr;
    bool allow_nan_and_inf = (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) > 0;
    const char_esc_type *esc_table = get_esc_table_with_flag(flg);
    
    alc_len = 0 * YYJSON_WRITER_ESTIMATED_MINIFY_RATIO + 64;
    alc_len = size_align_up(alc_len, sizeof(yyjson_mut_write_ctx));
    hdr = (u8 *)alc.malloc(alc.ctx, alc_len);
    if (!hdr) goto fail_alloc;
    cur = hdr;
    end = hdr + alc_len;
    ctx = (yyjson_mut_write_ctx *)end;
    
doc_begin:
    val = doc->root;
    val_type = unsafe_yyjson_get_type(val);
    ctn_obj = (val_type == YYJSON_TYPE_OBJ);
    ctn_len = unsafe_yyjson_get_len(val) << (u8)ctn_obj;
    *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
    ctn = val;
    val = (yyjson_mut_val *)val->uni.ptr; /* tail */
    val = ctn_obj ? val->next->next : val->next;
    
val_begin:
    val_type = unsafe_yyjson_get_type(val);
    switch (val_type) {
        case YYJSON_TYPE_STR:
            is_key = ((u8)ctn_obj & (u8)~ctn_len);
            str_len = unsafe_yyjson_get_len(val);
            str_ptr = (const u8 *)unsafe_yyjson_get_str(val);
            check_str_len(str_len);
            incr_len(str_len * 6 + 16);
            cur = write_string(cur, str_ptr, str_len, esc_table);
            *cur++ = is_key ? ':' : ',';
            break;
            
        case YYJSON_TYPE_NUM:
            incr_len(32);
            cur = write_number(cur, (yyjson_val *)val, allow_nan_and_inf);
            if (unlikely(!cur)) goto fail_num;
            *cur++ = ',';
            break;
            
        case YYJSON_TYPE_BOOL:
            incr_len(16);
            cur = write_bool(cur, unsafe_yyjson_get_bool(val));
            cur++;
            break;
            
        case YYJSON_TYPE_NULL:
            incr_len(16);
            cur = write_null(cur);
            cur++;
            break;
            
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ:
            ctn_len_tmp = unsafe_yyjson_get_len(val);
            ctn_obj_tmp = (val_type == YYJSON_TYPE_OBJ);
            incr_len(16);
            if (unlikely(ctn_len_tmp == 0)) {
                /* write empty container */
                *cur++ = (u8)('[' | ((u8)ctn_obj_tmp << 5));
                *cur++ = (u8)(']' | ((u8)ctn_obj_tmp << 5));
                *cur++ = ',';
                break;
            } else {
                /* push context, setup new container */
                yyjson_mut_write_ctx_set(--ctx, ctn, ctn_len, ctn_obj);
                ctn_len = ctn_len_tmp << (u8)ctn_obj_tmp;
                ctn_obj = ctn_obj_tmp;
                *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
                ctn = val;
                val = (yyjson_mut_val *)ctn->uni.ptr; /* tail */
                val = ctn_obj ? val->next->next : val->next;
                goto val_begin;
            }
            
        default:
            goto fail_type;
    }
    
    ctn_len--;
    if (unlikely(ctn_len == 0)) goto ctn_end;
    val = val->next;
    goto val_begin;
    
ctn_end:
    cur--;
    *cur++ = (u8)(']' | ((u8)ctn_obj << 5));
    *cur++ = ',';
    if (unlikely((u8 *)ctx >= end)) goto doc_end;
    val = ctn->next;
    yyjson_mut_write_ctx_get(ctx++, &ctn, &ctn_len, &ctn_obj);
    ctn_len--;
    if (likely(ctn_len > 0)) {
        goto val_begin;
    } else {
        goto ctn_end;
    }
    
doc_end:
    *--cur = '\0';
    *dat_len = cur - hdr;
    err->code = YYJSON_WRITE_SUCCESS;
    err->msg = "success";
    return hdr;
    
fail_alloc:
    return_err(MEMORY_ALLOCATION, "memory allocation failed");
fail_type:
    return_err(INVALID_VALUE_TYPE, "invalid JSON value type");
fail_num:
    return_err(NAN_OR_INF, "nan or inf number is not allowed");
    
#undef return_err
#undef incr_len
#undef check_str_len
}

/** Write JSON document pretty.
    The root of this document should be a non-empty container. */
static_inline u8 *yyjson_mut_write_pretty(yyjson_mut_doc *doc,
                                          yyjson_write_flag flg,
                                          yyjson_alc alc,
                                          usize *dat_len,
                                          yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    *dat_len = 0; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    err->msg = _msg; \
    if (hdr) alc.free(alc.ctx, hdr); \
    return NULL; \
} while(false)
    
#define incr_len(_len) do { \
    ext_len = _len; \
    if (unlikely((u8 *)(cur + ext_len) >= (u8 *)ctx)) { \
        alc_inc = yyjson_max(alc_len / 2, ext_len); \
        alc_inc = size_align_up(alc_inc, sizeof(yyjson_mut_write_ctx)); \
        if (size_add_is_overflow(alc_len, alc_inc)) goto fail_alloc; \
        alc_len += alc_inc; \
        tmp = (u8 *)alc.realloc(alc.ctx, hdr, alc_len); \
        if (unlikely(!tmp)) goto fail_alloc; \
        ctx_len = end - (u8 *)ctx; \
        ctx_tmp = (yyjson_mut_write_ctx *)(tmp + (alc_len - ctx_len)); \
        memmove((void *)ctx_tmp, (void *)(tmp + ((u8 *)ctx - hdr)), ctx_len); \
        ctx = ctx_tmp; \
        cur = tmp + (cur - hdr); \
        end = tmp + alc_len; \
        hdr = tmp; \
    } \
} while(false)
    
#define check_str_len(_len) do { \
    if ((USIZE_MAX < U64_MAX) && (_len >= (USIZE_MAX - 16) / 6)) \
        goto fail_alloc; \
} while(false)
    
    yyjson_mut_val *val, *ctn;
    yyjson_type val_type;
    usize ctn_len, ctn_len_tmp;
    bool ctn_obj, ctn_obj_tmp, is_key, no_indent;
    u8 *hdr, *cur, *end, *tmp;
    yyjson_mut_write_ctx *ctx, *ctx_tmp;
    usize alc_len, alc_inc, ctx_len, ext_len, str_len, level;
    const u8 *str_ptr;
    bool allow_nan_and_inf = (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) > 0;
    const char_esc_type *esc_table = get_esc_table_with_flag(flg);
    
    alc_len = 0 * YYJSON_WRITER_ESTIMATED_PRETTY_RATIO + 64;
    alc_len = size_align_up(alc_len, sizeof(yyjson_mut_write_ctx));
    hdr = (u8 *)alc.malloc(alc.ctx, alc_len);
    if (!hdr) goto fail_alloc;
    cur = hdr;
    end = hdr + alc_len;
    ctx = (yyjson_mut_write_ctx *)end;
    
doc_begin:
    val = doc->root;
    val_type = unsafe_yyjson_get_type(val);
    ctn_obj = (val_type == YYJSON_TYPE_OBJ);
    ctn_len = unsafe_yyjson_get_len(val) << (u8)ctn_obj;
    *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
    *cur++ = '\n';
    ctn = val;
    val = (yyjson_mut_val *)val->uni.ptr; /* tail */
    val = ctn_obj ? val->next->next : val->next;
    level = 1;
    
val_begin:
    val_type = unsafe_yyjson_get_type(val);
    switch (val_type) {
        case YYJSON_TYPE_STR:
            is_key = ((u8)ctn_obj & (u8)~ctn_len);
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            str_len = unsafe_yyjson_get_len(val);
            str_ptr = (const u8 *)unsafe_yyjson_get_str(val);
            check_str_len(str_len);
            incr_len(str_len * 6 + 16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_string(cur, str_ptr, str_len, esc_table);
            *cur++ = is_key ? ':' : ',';
            *cur++ = is_key ? ' ' : '\n';
            break;
            
        case YYJSON_TYPE_NUM:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(32 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_number(cur, (yyjson_val *)val, allow_nan_and_inf);
            if (unlikely(!cur)) goto fail_num;
            *cur++ = ',';
            *cur++ = '\n';
            break;
            
        case YYJSON_TYPE_BOOL:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_bool(cur, unsafe_yyjson_get_bool(val));
            cur += 2;
            break;
            
        case YYJSON_TYPE_NULL:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            incr_len(16 + (no_indent ? 0 : level * 4));
            cur = write_indent(cur, no_indent ? 0 : level);
            cur = write_null(cur);
            cur += 2;
            break;
            
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ:
            no_indent = ((u8)ctn_obj & (u8)ctn_len);
            ctn_len_tmp = unsafe_yyjson_get_len(val);
            ctn_obj_tmp = (val_type == YYJSON_TYPE_OBJ);
            if (unlikely(ctn_len_tmp == 0)) {
                /* write empty container */
                incr_len(16 + (no_indent ? 0 : level * 4));
                cur = write_indent(cur, no_indent ? 0 : level);
                *cur++ = (u8)('[' | ((u8)ctn_obj_tmp << 5));
                *cur++ = (u8)(']' | ((u8)ctn_obj_tmp << 5));
                *cur++ = ',';
                *cur++ = '\n';
                break;
            } else {
                /* push context, setup new container */
                incr_len(32 + (no_indent ? 0 : level * 4));
                yyjson_mut_write_ctx_set(--ctx, ctn, ctn_len, ctn_obj);
                ctn_len = ctn_len_tmp << (u8)ctn_obj_tmp;
                ctn_obj = ctn_obj_tmp;
                cur = write_indent(cur, no_indent ? 0 : level);
                level++;
                *cur++ = (u8)('[' | ((u8)ctn_obj << 5));
                *cur++ = '\n';
                ctn = val;
                val = (yyjson_mut_val *)ctn->uni.ptr; /* tail */
                val = ctn_obj ? val->next->next : val->next;
                goto val_begin;
            }
            
        default:
            goto fail_type;
    }
    
    ctn_len--;
    if (unlikely(ctn_len == 0)) goto ctn_end;
    val = val->next;
    goto val_begin;
    
ctn_end:
    cur -= 2;
    *cur++ = '\n';
    incr_len(level * 4);
    cur = write_indent(cur, --level);
    *cur++ = (u8)(']' | ((u8)ctn_obj << 5));
    if (unlikely((u8 *)ctx >= end)) goto doc_end;
    val = ctn->next;
    yyjson_mut_write_ctx_get(ctx++, &ctn, &ctn_len, &ctn_obj);
    ctn_len--;
    *cur++ = ',';
    *cur++ = '\n';
    if (likely(ctn_len > 0)) {
        goto val_begin;
    } else {
        goto ctn_end;
    }
    
doc_end:
    *cur = '\0';
    *dat_len = cur - hdr;
    err->code = YYJSON_WRITE_SUCCESS;
    err->msg = "success";
    return hdr;
    
fail_alloc:
    return_err(MEMORY_ALLOCATION, "memory allocation failed");
fail_type:
    return_err(INVALID_VALUE_TYPE, "invalid JSON value type");
fail_num:
    return_err(NAN_OR_INF, "nan or inf number is not allowed");
    
#undef return_err
#undef incr_len
#undef check_str_len
}

char *yyjson_mut_write_opts(yyjson_mut_doc *doc,
                            yyjson_write_flag flg,
                            yyjson_alc *alc_ptr,
                            usize *dat_len,
                            yyjson_write_err *err) {
    
    yyjson_write_err dummy_err;
    usize dummy_dat_len;
    yyjson_alc alc;
    yyjson_mut_val *root;
    
    err = err ? err : &dummy_err;
    dat_len = dat_len ? dat_len : &dummy_dat_len;
    alc = alc_ptr ? *alc_ptr : YYJSON_DEFAULT_ALC;
    
    if (unlikely(!doc)) {
        *dat_len = 0;
        err->msg = "input JSON document is NULL";
        err->code = YYJSON_READ_ERROR_INVALID_PARAMETER;
        return NULL;
    }
    root = doc->root;
    if (!root) {
        *dat_len = 0;
        err->msg = "input JSON document has no root value";
        err->code = YYJSON_READ_ERROR_INVALID_PARAMETER;
        return NULL;
    }
    
    if (!unsafe_yyjson_is_ctn(root) || unsafe_yyjson_get_len(root) == 0) {
        return (char *)yyjson_mut_write_single(root, flg, alc, dat_len, err);
    }
    if (flg & YYJSON_WRITE_PRETTY) {
        return (char *)yyjson_mut_write_pretty(doc, flg, alc, dat_len, err);
    } else {
        return (char *)yyjson_mut_write_minify(doc, flg, alc, dat_len, err);
    }
}

bool yyjson_mut_write_file(const char *path,
                           yyjson_mut_doc *doc,
                           yyjson_write_flag flg,
                           yyjson_alc *alc_ptr,
                           yyjson_write_err *err) {
    
#define return_err(_code, _msg) do { \
    err->msg = _msg; \
    err->code = YYJSON_WRITE_ERROR_##_code; \
    return false; \
} while(false)
    
    yyjson_write_err dummy_err;
    u8 *dat;
    usize dat_len = 0;
    bool suc;
    
    /* validate input parameters */
    if (!err) err = &dummy_err;
    if (unlikely(!path)) {
        return_err(INVALID_PARAMETER, "input path is NULL");
    }
    if (unlikely(*path == 0)) {
        return_err(INVALID_PARAMETER, "input path is empty");
    }
    
    dat = (u8 *)yyjson_mut_write_opts(doc, flg, alc_ptr, &dat_len, err);
    if (unlikely(!dat)) return false;
    suc = write_dat_to_file(path, dat, dat_len, err);
    if (alc_ptr) alc_ptr->free(alc_ptr->ctx, dat);
    else free(dat);
    return suc;
    
#undef return_err
}

#endif /* YYJSON_DISABLE_WRITER */



/*==============================================================================
 * Compiler Hint End
 *============================================================================*/

#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#   pragma warning(pop)
#endif /* warning suppress end */
