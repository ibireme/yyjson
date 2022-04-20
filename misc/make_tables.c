/*==============================================================================
 * Make look-up tables for yyjson.
 * Copyright (C) 2020 Yaoyuan <ibireme@gmail.com>.
 *
 * Released under the MIT License:
 * https://github.com/ibireme/yyjson/blob/master/LICENSE
 *============================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gmp.h>
#include <mpfr.h>

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

/*----------------------------------------------------------------------------*/

void make_pow10_sig_table(void) {
    static const int DEF_PREC = 5000;
    static const int BUF_LEN = 2000;
    char buf[BUF_LEN];
    
    mpfr_t sigMax, sigMin, half;
    mpfr_t pow10, pow2, div, sub;
    mpfr_inits2(DEF_PREC, sigMax, sigMin, half, NULL);
    mpfr_inits2(DEF_PREC, pow10, pow2, div, sub, NULL);
    
    mpfr_set_ui(sigMax, 0xFFFFFFFFFFFFFFFFULL, MPFR_RNDN);
    mpfr_set_ui(sigMin, 0x8000000000000000ULL, MPFR_RNDN);
    mpfr_set_d(half, 0.5, MPFR_RNDN);
    
    int e10min = -343, e10max = 324, e10step = 1;
    
    printf("#define POW10_SIG_TABLE_MIN_EXP %d\n", e10min);
    printf("#define POW10_SIG_TABLE_MAX_EXP %d\n", e10max);
    printf("#define POW10_SIG_TABLE_MIN_EXACT_EXP %d\n", 0);
    printf("#define POW10_SIG_TABLE_MAX_EXACT_EXP %d\n", 55);
    printf("static const u64 pow10_sig_table[] = {\n");
    
    for (int e10 = e10min; e10 <= e10max; e10 += e10step) {
        mpfr_set_d(pow10, 10, MPFR_RNDN);
        mpfr_pow_si(pow10, pow10, e10, MPFR_RNDN); // pow10 = 10^e10
        
        // 10^e10 = 2^e2
        // e2 = floor(log2(pow(10, e10)))
        // e2 = floor(log2(10) * e10)
        int e2 = (int)floor(log2(10) * e10) - 64 + 1;
        mpfr_set_d(pow2, 2, MPFR_RNDN);
        mpfr_pow_si(pow2, pow2, e2, MPFR_RNDN); // pow2 = 2^e2
        mpfr_div(div, pow10, pow2, MPFR_RNDN); // div = pow10 / pow2;
        if (mpfr_cmp(div, sigMin) < 0 || mpfr_cmp(div, sigMax) > 0) {
            printf("err!\n"); // make sure the highest bit is 1 (normalized)
        }
        
        mpfr_set_d(pow2, 2, MPFR_RNDN);
        mpfr_pow_si(pow2, pow2, e2, MPFR_RNDN); // pow2 = 2^e2
        mpfr_div(div, pow10, pow2, MPFR_RNDN); // div = pow10 / pow2;
        
        mpfr_snprintf(buf, BUF_LEN, "%.1000Rg", div);
        u64 val = strtoull(buf, NULL, 0);
        mpfr_sub_ui(sub, div, val, MPFR_RNDN); // sub = div - (uint64_t)div
        int cmp = mpfr_cmp(sub, half);
        if (cmp == 0) printf("err!\n"); // avoid round to even
        if (cmp > 0 && val == UINT64_MAX) printf("err!\n"); // avoid round up overflow
        
        printf("    ");
        printf("U64(0x%.8X, 0x%.8X),", (u32)(val >> 32), (u32)val);
        
        mpfr_set_d(pow2, 2, MPFR_RNDN);
        mpfr_pow_si(pow2, pow2, 64, MPFR_RNDN); // pow2 = 2^64
        mpfr_mul(sub, sub, pow2, MPFR_RNDN); // sub *= 2^64
        
        mpfr_snprintf(buf, BUF_LEN, "%.1000Rg", sub);
        u64 val2 = strtoull(buf, NULL, 0);
        mpfr_sub_ui(sub, sub, val2, MPFR_RNDN); // sub -= (uint64_t)sub
        int cmp2 = mpfr_cmp(sub, half);
        if (cmp2 == 0) printf("err!\n"); // avoid round to even
        if ((cmp > 0) && (val2 < ((u64)1) << 63)) printf("err!\n"); // avoid round up overflow
        bool is_exact = mpfr_cmp_ui(sub, 0) == 0;
        
        printf(" ");
        printf("U64(0x%.8X, 0x%.8X)", (u32)(val2 >> 32), (u32)val2);
        printf("%c", e10 < e10max ? ',' : ' ');
        printf(" /* %s 10^%d */", is_exact ? "==" : "~=", e10);
        printf("\n");
    }
    
    printf("};\n");
    printf("\n");
    
    mpfr_clears(sigMax, sigMin, half, NULL);
    mpfr_clears(pow10, pow2, div, sub, NULL);
}

/*----------------------------------------------------------------------------*/

static void make_dec_trailing_zero_table(void) {
    int table_len = 100;
    int line_len = 10;
    
    printf("static const u8 dec_trailing_zero_table[] = {\n");
    for (int i = 0; i < table_len; i++) {        
        int tz = 0;
        if (i == 0) tz = 2;
        else {
            if ((i % 10) == 0) tz++;
        }
        
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        
        printf("%1d", tz);
        
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

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

static void make_char_table(void) {
    u8 table[256] = {0};
    
    table[' '] |= CHAR_TYPE_SPACE;
    table['\t'] |= CHAR_TYPE_SPACE;
    table['\n'] |= CHAR_TYPE_SPACE;
    table['\r'] |= CHAR_TYPE_SPACE;

    table['-'] |= CHAR_TYPE_NUMBER;
    for (int i = 0; i <= 9; i++) {
        table[i + '0'] |= CHAR_TYPE_NUMBER;
    }
    
    table['"'] |= CHAR_TYPE_ESC_ASCII;
    table['\\'] |= CHAR_TYPE_ESC_ASCII;
    for (int i = 0x00; i <= 0x1F; i++) {
        table[i] |= CHAR_TYPE_ESC_ASCII;
    }
    
    for (int i = 0x80; i <= 0xFF; i++) {
        table[i] |= CHAR_TYPE_NON_ASCII;
    }
    
    table['{'] |= CHAR_TYPE_CONTAINER;
    table['['] |= CHAR_TYPE_CONTAINER;

    table['/'] |= CHAR_TYPE_COMMENT;
    
    table['\n'] |= CHAR_TYPE_LINE_END;
    table['\r'] |= CHAR_TYPE_LINE_END;
    table['\0'] |= CHAR_TYPE_LINE_END;
    
    int table_len = 256;
    int line_len = 8;
    printf("static const char_type char_table[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("0x%.2X", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

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

static void make_digit_table(void) {
    u8 table[256] = {0};
    
    table['0'] |= DIGI_TYPE_ZERO;
    for (int i = 1; i <= 9; i++) {
        table[i + '0'] |= DIGI_TYPE_NONZERO;
    }
    table['+'] |= DIGI_TYPE_POS;
    table['-'] |= DIGI_TYPE_NEG;
    table['.'] |= DIGI_TYPE_DOT;
    table['e'] |= DIGI_TYPE_EXP;
    table['E'] |= DIGI_TYPE_EXP;
    
    int table_len = 128; /* ASCII only */
    int line_len = 8;
    printf("static const digi_type digi_table[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("0x%.2X", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

static void make_hex_conv_table(void) {
    u8 table[256] = {0};
    
    for (int i = 0; i < 256; i++) {
        if ('0' <= i && i <= '9') {
            table[i] = (u8)(i - '0');
        } else if ('a' <= i && i <= 'f') {
            table[i] = (u8)(0xA + i - 'a');
        } else if ('A' <= i && i <= 'F') {
            table[i] = (u8)(0xA + i - 'A');
        } else {
            table[i] = 0xF0;
        }
    }
    
    int table_len = 256;
    int line_len = 8;
    printf("static const u8 hex_conv_table[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("0x%.2X", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

static void make_u64_pow10_table(void) {
    int table_len = 20;
    int line_len = 2;
    
    printf("static const u64 u64_pow10_table[U64_POW10_MAX_EXP + 1] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        u64 num = 1;
        for (int e = 0; e < i; e++) num *= 10;
        
        if (is_head) printf("    ");
        printf("U64(0x%.8X, 0x%.8X)", (u32)(num >> 32), (u32)(num));
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

#define CHAR_ESC_NONE   0 /* Character do not need to be escaped. */
#define CHAR_ESC_ASCII  1 /* ASCII character, escaped as '\x'. */
#define CHAR_ESC_UTF8_1 2 /* 1-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_2 3 /* 2-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_3 4 /* 3-byte UTF-8 character, escaped as '\uXXXX'. */
#define CHAR_ESC_UTF8_4 5 /* 4-byte UTF-8 character, escaped as two '\uXXXX'. */
#define CHAR_ESC_INV8_1 6 /* CHAR_ESC_UTF8_1 with unicode validation. */
#define CHAR_ESC_INV8_2 7 /* CHAR_ESC_UTF8_2 with unicode validation. */
#define CHAR_ESC_INV8_3 8 /* CHAR_ESC_UTF8_3 with unicode validation. */
#define CHAR_ESC_INV8_4 9 /* CHAR_ESC_UTF8_4 with unicode validation. */

static void make_esc_table(void) {
    u8 table[256];
    int table_len = 256;
    int line_len = 16;
    
    
    // esc_table_default
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_UTF8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    
    printf("static const char_esc_type esc_table_default[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
    
    
    // esc_table_default_with_slashes
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_UTF8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['/'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    
    printf("static const char_esc_type esc_table_default_with_slashes[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
    
    
    /*
     UTF-8 encoding:
     0xxxxxxx ........ ........ ........ 1 Byte [U+0000, U+007F]
     110xxxxx 10xxxxxx ........ ........ 2 Byte [U+0080, U+07FF]
     1110xxxx 10xxxxxx 10xxxxxx ........ 3 Byte [U+0800, U+FFFF]
     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx 4 Byte [U+10000, U+10FFFF]
     */
    
    // esc_table_unicode
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_UTF8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    for (int i = 0; i <= 0xFF; i++) {
        if ((i & 0xE0) == 0xC0) table[i] = CHAR_ESC_UTF8_2;
        if ((i & 0xF0) == 0xE0) table[i] = CHAR_ESC_UTF8_3;
        if ((i & 0xF8) == 0xF0) table[i] = CHAR_ESC_UTF8_4;
    }
    
    printf("static const char_esc_type esc_table_unicode[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
    
    // esc_table_unicode_inv
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_INV8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    for (int i = 0; i <= 0xFF; i++) {
        if ((i & 0xE0) == 0xC0) table[i] = CHAR_ESC_INV8_2;
        if ((i & 0xF0) == 0xE0) table[i] = CHAR_ESC_INV8_3;
        if ((i & 0xF8) == 0xF0) table[i] = CHAR_ESC_INV8_4;
    }
    
    printf("static const char_esc_type esc_table_unicode_inv[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
    
    // esc_table_unicode_with_slashes
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_UTF8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['/'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    for (int i = 0; i <= 0xFF; i++) {
        if ((i & 0xE0) == 0xC0) table[i] = CHAR_ESC_UTF8_2;
        if ((i & 0xF0) == 0xE0) table[i] = CHAR_ESC_UTF8_3;
        if ((i & 0xF8) == 0xF0) table[i] = CHAR_ESC_UTF8_4;
    }
    
    printf("static const char_esc_type esc_table_unicode_with_slashes[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
    
    // esc_table_unicode_with_slashes_inv
    memset(table, CHAR_ESC_NONE, 256);
    for (int i = 0; i <= 0x1F; i++) {
        table[i] = CHAR_ESC_INV8_1;
    }
    table['\b'] = CHAR_ESC_ASCII;
    table['\t'] = CHAR_ESC_ASCII;
    table['\n'] = CHAR_ESC_ASCII;
    table['\f'] = CHAR_ESC_ASCII;
    table['\r'] = CHAR_ESC_ASCII;
    table['\\'] = CHAR_ESC_ASCII;
    table['/'] = CHAR_ESC_ASCII;
    table['"'] = CHAR_ESC_ASCII;
    for (int i = 0; i <= 0xFF; i++) {
        if ((i & 0xE0) == 0xC0) table[i] = CHAR_ESC_INV8_2;
        if ((i & 0xF0) == 0xE0) table[i] = CHAR_ESC_INV8_3;
        if ((i & 0xF8) == 0xF0) table[i] = CHAR_ESC_INV8_4;
    }
    
    printf("static const char_esc_type esc_table_unicode_with_slashes_inv[256] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        printf("%d", table[i]);
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

static void make_esc_hex_char_table(void) {
    int table_len = 512;
    int line_len = 8;
    
    printf("static const u8 esc_hex_char_table[512] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        
        char buf[16];
        sprintf(buf, "%.2X", i / 2);
        printf("'%c'", buf[i % 2]);
        
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

/*----------------------------------------------------------------------------*/

static void make_esc_single_char_table(void) {
    u8 table[512];
    int table_len = 512;
    int line_len = 8;
    
    memset(table, ' ', 512);
    table['\b' * 2 + 0] = '\\';
    table['\b' * 2 + 1] = 'b';
    
    table['\t' * 2 + 0] = '\\';
    table['\t' * 2 + 1] = 't';
    
    table['\n' * 2 + 0] = '\\';
    table['\n' * 2 + 1] = 'n';
    
    table['\f' * 2 + 0] = '\\';
    table['\f' * 2 + 1] = 'f';
    
    table['\r' * 2 + 0] = '\\';
    table['\r' * 2 + 1] = 'r';
    
    table['\\' * 2 + 0] = '\\';
    table['\\' * 2 + 1] = '\\';
    
    table['/' * 2 + 0] = '\\';
    table['/' * 2 + 1] = '/';
    
    table['"' * 2 + 0] = '\\';
    table['"' * 2 + 1] = '"';
    
    printf("static const u8 esc_single_char_table[512] = {\n");
    for (int i = 0; i < table_len; i++) {
        bool is_head = ((i % line_len) == 0);
        bool is_tail = ((i % line_len) == line_len - 1);
        bool is_last = i + 1 == table_len;
        
        if (is_head) printf("    ");
        
        if (table[i] == '\\') printf("'\\\\'");
        else printf("'%c'", table[i]);
        
        if (i + 1 < table_len) printf(",");
        if (!is_tail && !is_last) printf(" "); else printf("\n");
    }
    printf("};\n");
    printf("\n");
}

int main(void) {
    make_pow10_sig_table();
    make_dec_trailing_zero_table();
    make_char_table();
    make_digit_table();
    make_hex_conv_table();
    make_u64_pow10_table();
    make_esc_table();
    make_esc_hex_char_table();
    make_esc_single_char_table();
    return 0;
}
