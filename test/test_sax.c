/*==============================================================================
 * Tests for the bounded-memory streaming (SAX) reader.
 *
 * The core of this file is a *differential* test: for a given JSON input the
 * event stream produced by `yyjson_sax_read()` must be identical to a recursive
 * walk of the DOM produced by `yyjson_read()` — at every window size and every
 * source chunk size. Driving the source in tiny, awkward chunks forces the
 * window to compact and rebase at every possible byte boundary, which is the
 * riskiest part of the implementation.
 *
 * Build (standalone):
 *   cc -std=c99 -O2 -Isrc test/test_sax.c src/yyjson.c -o /tmp/test_sax
 *============================================================================*/

#include "yy_test_utils.h"
#include "yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_SAX_READER

/*------------------------------------------------------------------------------
 * A growable, binary-safe event buffer. Both the DOM walk and the SAX handler
 * append length-prefixed records here; the two buffers are then compared byte
 * for byte.
 *----------------------------------------------------------------------------*/

typedef struct {
    char *dat;
    size_t len;
    size_t cap;
} evbuf;

static void ev_init(evbuf *b) { b->dat = NULL; b->len = 0; b->cap = 0; }
static void ev_free(evbuf *b) { free(b->dat); ev_init(b); }

static void ev_raw(evbuf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (b->len + n > nc) nc *= 2;
        b->dat = (char *)realloc(b->dat, nc);
        if (!b->dat) { fprintf(stderr, "OOM\n"); exit(2); }
        b->cap = nc;
    }
    memcpy(b->dat + b->len, p, n);
    b->len += n;
}

static void ev_byte(evbuf *b, char c) { ev_raw(b, &c, 1); }
static void ev_u64(evbuf *b, uint64_t v) { ev_raw(b, &v, sizeof(v)); }

/* tag + length + bytes (binary-safe: strings may hold NUL / arbitrary UTF-8) */
static void ev_blob(evbuf *b, char tag, const char *s, size_t n) {
    ev_byte(b, tag);
    ev_u64(b, (uint64_t)n);
    ev_raw(b, s, n);
}

/* Serialize a single scalar number value identically for both producers.
   RAW must be checked by *type* first: YYJSON_SUBTYPE_UINT == 0 collides with a
   RAW value's (absent) subtype bits. */
static void ev_num(evbuf *b, const yyjson_val *v) {
    if (yyjson_get_type(v) == YYJSON_TYPE_RAW) {
        ev_blob(b, 'w', yyjson_get_raw(v), yyjson_get_len(v));
        return;
    }
    switch (yyjson_get_subtype(v)) {
    case YYJSON_SUBTYPE_UINT:
        ev_byte(b, 'u'); ev_u64(b, yyjson_get_uint(v)); break;
    case YYJSON_SUBTYPE_SINT:
        ev_byte(b, 'i'); ev_u64(b, (uint64_t)yyjson_get_sint(v)); break;
    case YYJSON_SUBTYPE_REAL: {
        /* compare exact bit patterns; both paths use the same read_num */
        double d = yyjson_get_real(v);
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        ev_byte(b, 'r'); ev_u64(b, bits);
        break;
    }
    default: /* RAW */
        ev_blob(b, 'w', yyjson_get_raw(v), yyjson_get_len(v)); break;
    }
}

/*------------------------------------------------------------------------------
 * Producer 1: recursive DOM walk.
 *----------------------------------------------------------------------------*/

static void dom_walk(evbuf *b, yyjson_val *val) {
    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL: ev_byte(b, 'Z'); break;
    case YYJSON_TYPE_BOOL:
        ev_byte(b, yyjson_get_bool(val) ? 'T' : 'F'); break;
    case YYJSON_TYPE_NUM: ev_num(b, val); break;
    case YYJSON_TYPE_RAW: ev_blob(b, 'w', yyjson_get_raw(val),
                                  yyjson_get_len(val)); break;
    case YYJSON_TYPE_STR:
        ev_blob(b, 'S', yyjson_get_str(val), yyjson_get_len(val)); break;
    case YYJSON_TYPE_ARR: {
        yyjson_val *e;
        yyjson_arr_iter it;
        ev_byte(b, '[');
        yyjson_arr_iter_init(val, &it);
        while ((e = yyjson_arr_iter_next(&it))) dom_walk(b, e);
        ev_byte(b, ']');
        ev_u64(b, (uint64_t)yyjson_arr_size(val));
        break;
    }
    case YYJSON_TYPE_OBJ: {
        yyjson_val *k;
        yyjson_obj_iter it;
        ev_byte(b, '{');
        yyjson_obj_iter_init(val, &it);
        while ((k = yyjson_obj_iter_next(&it))) {
            ev_blob(b, 'K', yyjson_get_str(k), yyjson_get_len(k));
            dom_walk(b, yyjson_obj_iter_get_val(k));
        }
        ev_byte(b, '}');
        ev_u64(b, (uint64_t)yyjson_obj_size(val));
        break;
    }
    default: ev_byte(b, '?'); break;
    }
}

/*------------------------------------------------------------------------------
 * Producer 2: SAX handler.
 *----------------------------------------------------------------------------*/

static bool h_obj_begin(void *c) { ev_byte((evbuf *)c, '{'); return true; }
static bool h_obj_end(void *c, size_t n) {
    ev_byte((evbuf *)c, '}'); ev_u64((evbuf *)c, (uint64_t)n); return true;
}
static bool h_arr_begin(void *c) { ev_byte((evbuf *)c, '['); return true; }
static bool h_arr_end(void *c, size_t n) {
    ev_byte((evbuf *)c, ']'); ev_u64((evbuf *)c, (uint64_t)n); return true;
}
static bool h_key(void *c, const char *s, size_t n) {
    ev_blob((evbuf *)c, 'K', s, n); return true;
}
static bool h_str(void *c, const char *s, size_t n) {
    ev_blob((evbuf *)c, 'S', s, n); return true;
}
static bool h_num(void *c, const yyjson_val *v) { ev_num((evbuf *)c, v); return true; }
static bool h_bool(void *c, bool v) { ev_byte((evbuf *)c, v ? 'T' : 'F'); return true; }
static bool h_null(void *c) { ev_byte((evbuf *)c, 'Z'); return true; }

static const yyjson_sax_handler diff_handler = {
    h_obj_begin, h_obj_end, h_arr_begin, h_arr_end,
    h_key, h_str, h_num, h_bool, h_null
};

/*------------------------------------------------------------------------------
 * A chunked in-memory source: hands out at most `chunk` bytes per call.
 *----------------------------------------------------------------------------*/

typedef struct {
    const char *dat;
    size_t len;
    size_t pos;
    size_t chunk;
} mem_src;

static size_t mem_source(void *ctx, void *buf, size_t len) {
    mem_src *s = (mem_src *)ctx;
    size_t avail = s->len - s->pos;
    size_t give = len;
    if (give > avail) give = avail;
    if (give > s->chunk) give = s->chunk;
    memcpy(buf, s->dat + s->pos, give);
    s->pos += give;
    return give;
}

/*------------------------------------------------------------------------------
 * Differential check for a single input.
 *----------------------------------------------------------------------------*/

static int g_pass = 0, g_fail = 0;

static void fail(const char *what, const char *json, size_t jlen) {
    size_t i;
    g_fail++;
    fprintf(stderr, "FAIL: %s\n  input: ", what);
    for (i = 0; i < jlen && i < 120; i++) fputc(json[i], stderr);
    fputc('\n', stderr);
}

/* Parse `json` as a valid document and require SAX == DOM at many
   window/chunk combinations. */
static void check_valid_flags(const char *json, size_t jlen,
                              yyjson_read_flag flg) {
    evbuf dom;
    yyjson_doc *doc;
    size_t wi, ci;
    static const size_t windows[] = {8192, 8192 + 1, 8192 + 7, 12345, 65536};
    static const size_t chunks[]  = {1, 2, 3, 5, 8, 13, 64, 1024, 1 << 20};

    ev_init(&dom);
    doc = yyjson_read(json, jlen, flg);
    if (!doc) { fail("DOM parse failed on supposedly-valid input", json, jlen);
                ev_free(&dom); return; }
    dom_walk(&dom, yyjson_doc_get_root(doc));
    yyjson_doc_free(doc);

    for (wi = 0; wi < sizeof(windows) / sizeof(windows[0]); wi++) {
        for (ci = 0; ci < sizeof(chunks) / sizeof(chunks[0]); ci++) {
            evbuf sx;
            mem_src src;
            yyjson_sax_opts opts;
            yyjson_read_err err;
            bool ok;
            ev_init(&sx);
            src.dat = json; src.len = jlen; src.pos = 0; src.chunk = chunks[ci];
            memset(&opts, 0, sizeof(opts));
            opts.window = windows[wi];
            ok = yyjson_sax_read(mem_source, &src, &diff_handler, &sx,
                                 flg, &opts, NULL, &err);
            if (!ok) {
                fprintf(stderr, "  (window=%zu chunk=%zu) sax err=%u @%zu: %s\n",
                        windows[wi], chunks[ci], err.code, err.pos, err.msg);
                fail("SAX failed on valid input", json, jlen);
                ev_free(&sx);
                goto done;
            }
            if (sx.len != dom.len || memcmp(sx.dat, dom.dat, dom.len) != 0) {
                fprintf(stderr, "  (window=%zu chunk=%zu) event mismatch "
                        "(dom=%zu sax=%zu bytes)\n",
                        windows[wi], chunks[ci], dom.len, sx.len);
                fail("SAX/DOM event mismatch", json, jlen);
                ev_free(&sx);
                goto done;
            }
            ev_free(&sx);
        }
    }
    g_pass++;
done:
    ev_free(&dom);
}

static void check_valid(const char *json, size_t jlen) {
    check_valid_flags(json, jlen, 0);
}

/*------------------------------------------------------------------------------
 * Random JSON generator (produces only valid JSON).
 *----------------------------------------------------------------------------*/

static unsigned g_rng = 2463534242u;
static unsigned rnd(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

static void gen_string(evbuf *out) {
    int n = (int)(rnd() % 12);
    int i;
    ev_byte(out, '"');
    for (i = 0; i < n; i++) {
        unsigned r = rnd() % 12;
        switch (r) {
        case 0: ev_raw(out, "\\n", 2); break;
        case 1: ev_raw(out, "\\t", 2); break;
        case 2: ev_raw(out, "\\\"", 2); break;
        case 3: ev_raw(out, "\\\\", 2); break;
        case 4: ev_raw(out, "\\u00e9", 6); break;  /* é */
        case 5: ev_raw(out, "\xc3\xa9", 2); break;  /* literal é (UTF-8) */
        case 6: ev_raw(out, "\xe2\x82\xac", 3); break; /* € */
        default: ev_byte(out, (char)('a' + (r % 26))); break;
        }
    }
    ev_byte(out, '"');
}

static void gen_number(evbuf *out) {
    char tmp[64];
    int kind = (int)(rnd() % 5);
    switch (kind) {
    case 0: sprintf(tmp, "%d", (int)(rnd() % 1000)); break;
    case 1: sprintf(tmp, "-%d", (int)(rnd() % 1000)); break;
    case 2: sprintf(tmp, "%u", rnd()); break;
    case 3: sprintf(tmp, "%d.%03u", (int)(rnd() % 100), rnd() % 1000); break;
    default: sprintf(tmp, "%de%d", (int)(rnd() % 100), (int)(rnd() % 10)); break;
    }
    ev_raw(out, tmp, strlen(tmp));
}

static void gen_value(evbuf *out, int depth);

static void gen_array(evbuf *out, int depth) {
    int n = (int)(rnd() % 6);
    int i;
    ev_byte(out, '[');
    for (i = 0; i < n; i++) {
        if (i) ev_byte(out, ',');
        gen_value(out, depth + 1);
    }
    ev_byte(out, ']');
}

static void gen_object(evbuf *out, int depth) {
    int n = (int)(rnd() % 6);
    int i;
    ev_byte(out, '{');
    for (i = 0; i < n; i++) {
        if (i) ev_byte(out, ',');
        gen_string(out);
        ev_byte(out, ':');
        gen_value(out, depth + 1);
    }
    ev_byte(out, '}');
}

static void gen_value(evbuf *out, int depth) {
    unsigned r = rnd() % 10;
    if (depth > 6) r = rnd() % 5; /* stop nesting */
    switch (r) {
    case 0: ev_raw(out, "null", 4); break;
    case 1: ev_raw(out, "true", 4); break;
    case 2: ev_raw(out, "false", 5); break;
    case 3: gen_number(out); break;
    case 4: gen_string(out); break;
    case 5: case 6: gen_array(out, depth); break;
    default: gen_object(out, depth); break;
    }
}

/*------------------------------------------------------------------------------
 * Targeted unit tests.
 *----------------------------------------------------------------------------*/

#define CHECK(cond, name) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL: %s\n", name); } \
} while (0)

/* Run SAX over `json` with a given window/chunk; return err code (0 = ok). */
static yyjson_read_code run_sax(const char *json, size_t window, size_t chunk,
                                yyjson_read_flag flg, evbuf *out) {
    mem_src src;
    yyjson_sax_opts opts;
    yyjson_read_err err;
    src.dat = json; src.len = strlen(json); src.pos = 0; src.chunk = chunk;
    memset(&opts, 0, sizeof(opts));
    opts.window = window;
    memset(&err, 0, sizeof(err));
    yyjson_sax_read(mem_source, &src, &diff_handler, out, flg, &opts, NULL, &err);
    return err.code;
}

/* Handler that aborts after N events. */
typedef struct { int budget; int seen; } abort_ctx;
static bool ab_any(void *c) {
    abort_ctx *a = (abort_ctx *)c;
    a->seen++;
    return a->seen < a->budget;
}
static bool ab_end(void *c, size_t n) { (void)n; return ab_any(c); }
static bool ab_blob(void *c, const char *s, size_t n) { (void)s; (void)n; return ab_any(c); }
static bool ab_num(void *c, const yyjson_val *v) { (void)v; return ab_any(c); }
static bool ab_bool(void *c, bool v) { (void)v; return ab_any(c); }

static void unit_tests(void) {
    evbuf b;
    yyjson_read_code code;

    /* error cases */
    ev_init(&b); code = run_sax("", 8192, 4, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_EMPTY_CONTENT, "empty input"); ev_free(&b);

    ev_init(&b); code = run_sax("   \t\n ", 8192, 4, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_EMPTY_CONTENT, "whitespace only"); ev_free(&b);

    ev_init(&b); code = run_sax("[1,2", 8192, 1, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_UNEXPECTED_END, "truncated array"); ev_free(&b);

    ev_init(&b); code = run_sax("\"abc", 8192, 1, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_UNEXPECTED_END ||
          code == YYJSON_READ_ERROR_INVALID_STRING, "unterminated string"); ev_free(&b);

    ev_init(&b); code = run_sax("tru", 8192, 1, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_LITERAL ||
          code == YYJSON_READ_ERROR_UNEXPECTED_END, "bad literal"); ev_free(&b);

    ev_init(&b); code = run_sax("{\"a\"}", 8192, 1, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_UNEXPECTED_CHARACTER, "missing colon"); ev_free(&b);

    ev_init(&b); code = run_sax("[1] garbage", 8192, 4, 0, &b);
    CHECK(code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT, "trailing garbage"); ev_free(&b);

    ev_init(&b); code = run_sax("[1] garbage", 8192, 4,
                                YYJSON_READ_STOP_WHEN_DONE, &b);
    CHECK(code == YYJSON_READ_SUCCESS, "trailing garbage ok with STOP_WHEN_DONE");
    ev_free(&b);

    /* TOKEN_TOO_LARGE: a string bigger than a minimal window */
    {
        size_t huge = 20000, i;
        char *j = (char *)malloc(huge + 4);
        j[0] = '"';
        for (i = 1; i < huge - 1; i++) j[i] = 'x';
        j[huge - 1] = '"'; j[huge] = 0;
        ev_init(&b);
        code = run_sax(j, 8192, 512, 0, &b);
        CHECK(code == YYJSON_READ_ERROR_TOKEN_TOO_LARGE, "string exceeds window");
        ev_free(&b);
        /* but it fits in a big enough window */
        ev_init(&b);
        code = run_sax(j, 65536, 512, 0, &b);
        CHECK(code == YYJSON_READ_SUCCESS, "big string fits in big window");
        ev_free(&b);
        free(j);
    }

    /* depth limit */
    {
        char deep[400];
        int i;
        yyjson_sax_opts opts;
        yyjson_read_err err;
        mem_src src;
        for (i = 0; i < 200; i++) deep[i] = '[';
        for (i = 200; i < 400; i++) deep[i] = ']';
        src.dat = deep; src.len = 400; src.pos = 0; src.chunk = 7;
        memset(&opts, 0, sizeof(opts));
        opts.window = 8192; opts.max_depth = 50;
        ev_init(&b);
        yyjson_sax_read(mem_source, &src, &diff_handler, &b, 0, &opts, NULL, &err);
        CHECK(err.code == YYJSON_READ_ERROR_DEPTH, "depth limit enforced");
        ev_free(&b);
    }

    /* handler abort */
    {
        abort_ctx a; a.budget = 3; a.seen = 0;
        yyjson_sax_handler h;
        yyjson_read_err err;
        mem_src src;
        const char *j = "[1,2,3,4,5,6,7,8]";
        memset(&h, 0, sizeof(h));
        h.arr_begin = ab_any; h.arr_end = ab_end;
        h.num = ab_num; h.bool_val = ab_bool;
        h.key = ab_blob; h.str = ab_blob;
        src.dat = j; src.len = strlen(j); src.pos = 0; src.chunk = 4;
        yyjson_sax_read(mem_source, &src, &h, &a, 0, NULL, NULL, &err);
        CHECK(err.code == YYJSON_READ_ERROR_ABORTED, "handler abort");
    }

    /* big numbers parsed as real still match DOM */
    check_valid("[1,2.5,-3,1e10,123456789012345678901234567890]",
                strlen("[1,2.5,-3,1e10,123456789012345678901234567890]"));

    /* NUMBER_AS_RAW / BIGNUM_AS_RAW (Decimal-style) still matches DOM */
    {
        const char *j = "[1,2.5,-3,1e10,123456789012345678901234567890,0.1]";
        check_valid_flags(j, strlen(j), YYJSON_READ_NUMBER_AS_RAW);
        check_valid_flags(j, strlen(j), YYJSON_READ_BIGNUM_AS_RAW);
    }
}

/*------------------------------------------------------------------------------
 * Main.
 *----------------------------------------------------------------------------*/

yy_test_case(test_sax) {
    size_t iter;
    static const char *fixed[] = {
        "null", "true", "false", "0", "-0", "42", "-17", "3.14", "1e10",
        "-2.5e-3", "\"\"", "\"hello\"", "\"a\\\"b\\\\c\\n\"", "\"\\u00e9\"",
        "[]", "{}", "[1]", "[1,2,3]", "{\"a\":1}", "{\"a\":1,\"b\":2}",
        "[[]]", "[[[]]]", "{\"a\":{}}", "{\"a\":{\"b\":{\"c\":[]}}}",
        "[null,true,false,\"s\",1,2.5]",
        "{\"k\":[1,{\"n\":null},[]],\"e\":\"\\u20ac\"}",
        "  [ 1 , 2 , 3 ]  ", "\n\t{\n\"a\"\t:\n1\n}\n",
        "18446744073709551615", "-9223372036854775808",
        "12345678901234567890123456789012345",
        0
    };

    /* fixed inputs */
    for (iter = 0; fixed[iter]; iter++)
        check_valid(fixed[iter], strlen(fixed[iter]));

    /* a big array that easily exceeds the window, to force compaction */
    {
        evbuf big;
        size_t i;
        ev_init(&big);
        ev_byte(&big, '[');
        for (i = 0; i < 20000; i++) {
            if (i) ev_byte(&big, ',');
            gen_value(&big, 5);
        }
        ev_byte(&big, ']');
        ev_byte(&big, '\0');
        check_valid(big.dat, big.len - 1);
        ev_free(&big);
    }

    /* random fuzzing */
    for (iter = 0; iter < 1000; iter++) {
        evbuf j;
        ev_init(&j);
        gen_value(&j, 0);
        ev_byte(&j, '\0');
        check_valid(j.dat, j.len - 1);
        ev_free(&j);
    }

    unit_tests();

    printf("\nSAX tests: %d passed, %d failed\n", g_pass, g_fail);
    yy_assert(g_fail == 0);
}

#else
yy_test_case(test_sax) {}
#endif
