#include "yyjson.h"
#include "yy_test_utils.h"


// JSON reader will validate the input string, ignore invalid UTF-8 encoding.
static void validate_str_read(const char *str, usize len,
                              const char *esc_str, usize esc_len) {
#if !YYJSON_DISABLE_READER
    char *json = malloc(esc_len + 3);
    json[0] = '"';
    memcpy(json + 1, esc_str, esc_len);
    json[esc_len + 1] = '"';
    json[esc_len + 2] = '\0';
    
    yyjson_doc *doc = yyjson_read(json, esc_len + 2, 0);
    if (str) {
        yy_assert(doc);
        yyjson_val *val = yyjson_doc_get_root(doc);
        yy_assert(yyjson_is_str(val));
        yy_assertf(yyjson_equals_strn(val, str, len),
                   "read fail,\ninput: %s\nexpect:%s\noutput:%s\n",
                   json, str, yyjson_get_str(val));
    } else {
        yy_assertf(!doc, "input string should be reject: %s\n", esc_str);
    }
    free(json);
    yyjson_doc_free(doc);
#endif
}


// JSON write assume the string is valid UTF-8 encoded, do not validate again.
static void validate_str_write(yyjson_write_flag flag,
                               const char *str, usize len,
                               const char *expect, usize expect_len) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc;
    yyjson_mut_val *val;
    char *ret;
    usize ret_len;
    
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_strn(doc, str, len);
    yyjson_mut_doc_set_root(doc, val);
    ret = yyjson_mut_write_opts(doc, flag, NULL, &ret_len, NULL);
    yy_assertf(ret_len == expect_len + 2,
               "write fail, expect length:%d, return:%d\n",
               (int)expect_len, (int)ret_len - 2);
    yy_assertf(memcmp(ret + 1, expect, expect_len) == 0,
               "write not match\nexpect: \"%s\"\nreturn :%s\n", expect, ret);

    if (ret) free((void *)ret);
    yyjson_mut_doc_free(doc);
#endif
}


static void validate_str(const char *str, usize len,
                         const char *esc_non, usize non_len,
                         const char *esc_uni, usize uni_len,
                         const char *esc_sla, usize sla_len,
                         const char *esc_uni_sla, usize uni_sla_len) {
    yyjson_write_flag flag;
    flag = YYJSON_WRITE_NOFLAG;
    validate_str_write(flag, str, len, esc_non, non_len);
    flag = YYJSON_WRITE_ESCAPE_UNICODE;
    validate_str_write(flag, str, len, esc_uni, uni_len);
    flag = YYJSON_WRITE_ESCAPE_SLASHES;
    validate_str_write(flag, str, len, esc_sla, sla_len);
    flag = YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_ESCAPE_SLASHES;
    validate_str_write(flag, str, len, esc_uni_sla, uni_sla_len);
    
    validate_str_read(str, len, esc_non, non_len);
    validate_str_read(str, len, esc_uni, uni_len);
    validate_str_read(str, len, esc_sla, sla_len);
    validate_str_read(str, len, esc_uni_sla, uni_sla_len);
}


// This file must be compiled with UTF-8 encoding.
yy_test_case(test_string) {
    
    validate_str("", 0,
                 "", 0,
                 "", 0,
                 "", 0,
                 "", 0);
    
    validate_str("a", 1,
                 "a", 1,
                 "a", 1,
                 "a", 1,
                 "a", 1);
    
    validate_str("abc", 3,
                 "abc", 3,
                 "abc", 3,
                 "abc", 3,
                 "abc", 3);
        
    validate_str("\0", 1,
                 "\\u0000", 6,
                 "\\u0000", 6,
                 "\\u0000", 6,
                 "\\u0000", 6);
    
    validate_str("abc\0", 4,
                 "abc\\u0000", 9,
                 "abc\\u0000", 9,
                 "abc\\u0000", 9,
                 "abc\\u0000", 9);
    
    validate_str("\0abc", 4,
                 "\\u0000abc", 9,
                 "\\u0000abc", 9,
                 "\\u0000abc", 9,
                 "\\u0000abc", 9);
    
    validate_str("abc\0def", 7,
                 "abc\\u0000def", 12,
                 "abc\\u0000def", 12,
                 "abc\\u0000def", 12,
                 "abc\\u0000def", 12);
    
    validate_str("abc", 3,
                 "abc", 3,
                 "abc", 3,
                 "abc", 3,
                 "abc", 3);
    
    validate_str("a\\b", 3,
                 "a\\\\b", 4,
                 "a\\\\b", 4,
                 "a\\\\b", 4,
                 "a\\\\b", 4);
    
    validate_str("a/b", 3,
                 "a/b", 3,
                 "a/b", 3,
                 "a\\/b", 4,
                 "a\\/b", 4);
    
    validate_str("\"\\/\b\f\n\r\t", 8,
                 "\\\"\\\\/\\b\\f\\n\\r\\t", 15,
                 "\\\"\\\\/\\b\\f\\n\\r\\t", 15,
                 "\\\"\\\\\\/\\b\\f\\n\\r\\t", 16,
                 "\\\"\\\\\\/\\b\\f\\n\\r\\t", 16);
    
    validate_str("Alizée", 7,
                 "Alizée", 7,
                 "Aliz\\u00E9e", 11,
                 "Alizée", 7,
                 "Aliz\\u00E9e", 11);
    
    validate_str("Hello世界", 11,
                 "Hello世界", 11,
                 "Hello\\u4E16\\u754C", 17,
                 "Hello世界", 11,
                 "Hello\\u4E16\\u754C", 17);
    
    validate_str("Emoji😊", 9,
                 "Emoji😊", 9,
                 "Emoji\\uD83D\\uDE0A", 17,
                 "Emoji😊", 9,
                 "Emoji\\uD83D\\uDE0A", 17);
    
    validate_str("🐱\t🐶", 9,
                 "🐱\\t🐶", 10,
                 "\\uD83D\\uDC31\\t\\uD83D\\uDC36", 26,
                 "🐱\\t🐶", 10,
                 "\\uD83D\\uDC31\\t\\uD83D\\uDC36", 26);
    
    validate_str("Check✅©\t2020®яблоко////แอปเปิ้ล\\\\リンゴ|تفاحة|蘋果|사과|", 97,
                 "Check✅©\\t2020®яблоко////แอปเปิ้ล\\\\\\\\リンゴ|تفاحة|蘋果|사과|", 100,
                 "Check\\u2705\\u00A9\\t2020\\u00AE\\u044F\\u0431\\u043B\\u043E\\u043A\\u043E////\\u0E41\\u0E2D\\u0E1B\\u0E40\\u0E1B\\u0E34\\u0E49\\u0E25\\\\\\\\\\u30EA\\u30F3\\u30B4|\\u062A\\u0641\\u0627\\u062D\\u0629|\\u860B\\u679C|\\uC0AC\\uACFC|\\uF8FF", 203,
                 "Check✅©\\t2020®яблоко\\/\\/\\/\\/แอปเปิ้ล\\\\\\\\リンゴ|تفاحة|蘋果|사과|", 104,
                 "Check\\u2705\\u00A9\\t2020\\u00AE\\u044F\\u0431\\u043B\\u043E\\u043A\\u043E\\/\\/\\/\\/\\u0E41\\u0E2D\\u0E1B\\u0E40\\u0E1B\\u0E34\\u0E49\\u0E25\\\\\\\\\\u30EA\\u30F3\\u30B4|\\u062A\\u0641\\u0627\\u062D\\u0629|\\u860B\\u679C|\\uC0AC\\uACFC|\\uF8FF", 207);
    
    for (int i = 1; i < 48; i++) {
        char buf[64];
        memset(buf, 'a', sizeof(buf));
        buf[i] = '\0';
        validate_str(buf, i, buf, i, buf, i, buf, i, buf, i);
    }
    for (int i = 1; i < 48; i++) {
        char buf1[64];
        char buf2[64];
        memset(buf1, 'a', sizeof(buf1));
        memset(buf2, 'a', sizeof(buf2));
        buf1[0] = '\t';
        buf1[i] = '\0';
        buf2[0] = '\\';
        buf2[1] = 't';
        buf2[i + 1] = '\t';
        validate_str(buf1, i, buf2, i + 1, buf2, i + 1, buf2, i + 1, buf2, i + 1);
    }
    
    char buf[64], *cur;
    cur = buf;
    
    *cur++ = '\\';
    *cur++ = 'u';
    *cur++ = '0';
    *cur++ = '0';
    *cur++ = '0';
    validate_str_read(NULL, 0, buf, 2);
    validate_str_read(NULL, 0, buf, 3);
    validate_str_read(NULL, 0, buf, 4);
    validate_str_read(NULL, 0, buf, 5);
    
    cur = buf;
    *cur++ = 0xFF;
    validate_str_read(NULL, 0, buf, 1);
    
    cur = buf;
    *cur++ = 0x01;
    validate_str_read(NULL, 0, buf, 1);
    
    cur = buf;
    *cur++ = '\\';
    *cur++ = 't';
    *cur++ = 0xFF;
    validate_str_read(NULL, 0, buf, 3);
    
    cur = buf;
    *cur++ = '\\';
    *cur++ = 't';
    *cur++ = 0x01;
    validate_str_read(NULL, 0, buf, 3);
    
    validate_str_read(NULL, 0, "\\uD83D", 6);
    validate_str_read(NULL, 0, "\\uF83D\\uDE0A", 12);
    validate_str_read(NULL, 0, "\\uD83D\\uFE0A", 12);
    validate_str_read(NULL, 0, "\\t\\uD83D", 6);
    validate_str_read(NULL, 0, "\\t\\uF83D\\uDE0A", 12);
    validate_str_read(NULL, 0, "\\t\\uD83D\\uFE0A", 12);
    
}
