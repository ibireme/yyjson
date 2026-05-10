// This file is used to test yyjson_write_string_to_buf.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_WRITER

static void check_ok(const char *label, const char *src, size_t src_len,
                     yyjson_write_flag flg, const char *want) {
    char buf[256];
    char *end = yyjson_write_string_to_buf(buf, src, src_len, flg);
    yy_assertf(end != NULL, "%s: returned NULL", label);
    *end = '\0';
    yy_assertf(strcmp(buf, want) == 0,
               "%s: got=%s want=%s", label, buf, want);
}

static void check_fail(const char *label, const char *src, size_t src_len,
                       yyjson_write_flag flg) {
    char buf[256];
    char *end = yyjson_write_string_to_buf(buf, src, src_len, flg);
    yy_assertf(end == NULL, "%s: expected NULL, got non-NULL", label);
}

yy_test_case(test_string_writer) {
    // Plain ASCII with embedded quote: gets surrounded by quotes,
    // inner quote escaped.
    check_ok("ascii embedded quote",
             "hello \"world\"", 13, 0,
             "\"hello \\\"world\\\"\"");

    // Slash is not escaped by default.
    check_ok("slash default", "a/b", 3, 0, "\"a/b\"");

    // Slash escaped under YYJSON_WRITE_ESCAPE_SLASHES.
    check_ok("slash escaped", "a/b", 3,
             YYJSON_WRITE_ESCAPE_SLASHES, "\"a\\/b\"");

    // Multibyte UTF-8 passes through verbatim by default.
    check_ok("utf8 raw", "\xE2\x98\x83", 3, 0, "\"\xE2\x98\x83\"");

    // Same character emitted as \uXXXX with ESCAPE_UNICODE (uppercase hex).
    check_ok("utf8 escape unicode upper",
             "\xE2\x98\x83", 3, YYJSON_WRITE_ESCAPE_UNICODE,
             "\"\\u2603\"");

    // LOWERCASE_HEX changes the hex case in \uXXXX escapes.
    check_ok("u00FF upper hex",
             "\xC3\xBF", 2, YYJSON_WRITE_ESCAPE_UNICODE,
             "\"\\u00FF\"");
    check_ok("u00FF lower hex",
             "\xC3\xBF", 2,
             YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_LOWERCASE_HEX,
             "\"\\u00ff\"");

    // Control characters get short-form escapes.
    check_ok("control newline",
             "line\nbreak", 10, 0, "\"line\\nbreak\"");

    // Invalid UTF-8 with ALLOW_INVALID_UNICODE -> replacement character.
    check_ok("invalid utf8 allowed",
             "\xFF", 1,
             YYJSON_WRITE_ALLOW_INVALID_UNICODE |
             YYJSON_WRITE_ESCAPE_UNICODE,
             "\"\\uFFFD\"");

    // Invalid UTF-8 without ALLOW_INVALID_UNICODE -> NULL.
    check_fail("invalid utf8 rejected", "\xFF", 1, 0);

    // Empty string -> "".
    check_ok("empty", "", 0, 0, "\"\"");

    // Round-trip: write_string_to_buf output equals the string part
    // of yyjson_mut_write for the same input.
    {
        const char *src = "round/trip \"check\"\n";
        size_t src_len = strlen(src);
        char direct[128];
        char *end = yyjson_write_string_to_buf(direct, src, src_len, 0);
        yy_assert(end != NULL);
        *end = '\0';

        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *v = yyjson_mut_strn(doc, src, src_len);
        yyjson_mut_doc_set_root(doc, v);
        size_t two_stage_len = 0;
        char *two_stage = yyjson_mut_write(doc, 0, &two_stage_len);
        yy_assert(two_stage != NULL);
        yy_assertf(strcmp(direct, two_stage) == 0,
                   "round-trip mismatch: direct=%s two_stage=%s",
                   direct, two_stage);
        free(two_stage);
        yyjson_mut_doc_free(doc);
    }
}

#else
yy_test_case(test_string_writer) {}
#endif
