// This file is used to test the streaming JSON reader.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_STREAMING && !YYJSON_DISABLE_READER



/*==============================================================================
 * MARK: - Helper
 *============================================================================*/

// Growable byte buffer.
typedef struct {
    char *dat;
    usize len;
    usize cap;
} sink_t;

static void sink_put(sink_t *s, const void *src, usize len) {
    if (s->len + len + 1 > s->cap) {
        usize cap = s->cap ? s->cap : 64;
        while (cap < s->len + len + 1) cap *= 2;
        s->dat = (char *)realloc(s->dat, cap);
        yy_assert(s->dat != NULL);
        s->cap = cap;
    }
    if (len) memcpy(s->dat + s->len, src, len);
    s->len += len;
    s->dat[s->len] = '\0';
}

static void sink_puts(sink_t *s, const char *str) {
    sink_put(s, str, strlen(str));
}

static void sink_free(sink_t *s) {
    free(s->dat);
    memset(s, 0, sizeof(*s));
}

// Allocator that counts calls, and fails after `left` calls or when a
// request is larger than `budget`. It may wrap another allocator.
typedef struct {
    yyjson_alc *inner; // NULL for libc
    usize left;
    usize budget;
    usize calls;
} test_alc;

static void *test_alc_malloc(void *ctx, usize size) {
    test_alc *t = (test_alc *)ctx;
    t->calls++;
    if (!t->left || size > t->budget) return NULL;
    t->left--;
    return t->inner ? t->inner->malloc(t->inner->ctx, size) : malloc(size);
}

static void *test_alc_realloc(void *ctx, void *ptr, usize old, usize size) {
    test_alc *t = (test_alc *)ctx;
    t->calls++;
    if (!t->left || size > t->budget) return NULL;
    t->left--;
    return t->inner ? t->inner->realloc(t->inner->ctx, ptr, old, size) :
                      realloc(ptr, size);
}

static void test_alc_free(void *ctx, void *ptr) {
    test_alc *t = (test_alc *)ctx;
    if (t->inner) t->inner->free(t->inner->ctx, ptr);
    else free(ptr);
}

static void test_alc_init(yyjson_alc *alc, test_alc *ctx, yyjson_alc *inner,
                          usize left, usize budget) {
    ctx->inner = inner;
    ctx->left = left;
    ctx->budget = budget;
    ctx->calls = 0;
    alc->malloc = test_alc_malloc;
    alc->realloc = test_alc_realloc;
    alc->free = test_alc_free;
    alc->ctx = ctx;
}

// Returns a malloc-ed copy of `dat` followed by `pad` zero bytes.
static char *dup_pad(const void *dat, usize len, usize pad) {
    char *buf = (char *)malloc(len + pad ? len + pad : 1);
    yy_assert(buf != NULL);
    if (len) memcpy(buf, dat, len);
    memset(buf + len, 0, pad);
    return buf;
}

// A pool for an allocator, `off` bytes after an aligned address.
typedef struct {
    char *mem;
    yyjson_alc alc;
} pool_t;

static void pool_init(pool_t *p, usize size, usize off) {
    p->mem = (char *)malloc(size + 16);
    yy_assert(p->mem != NULL);
    yy_assert(yyjson_alc_pool_init(&p->alc, p->mem + off, size));
}

static void pool_free(pool_t *p) {
    free(p->mem);
}

#if YYJSON_READER_DEPTH_LIMIT
#   define SR_DEPTH_OK(n) ((usize)(n) <= (usize)YYJSON_READER_DEPTH_LIMIT)
#else
#   define SR_DEPTH_OK(n) 1
#endif

// How the input reaches the reader.
typedef enum {
    SRC_INSITU, // memory, in-situ
    SRC_MEM,    // memory, copied
    SRC_CB,     // callback
} src_kind;

typedef struct {
    src_kind kind;
    usize chunk; // callback step, 0 for as much as asked
    bool seek;   // callback has a seek function
} src_mode;

// Every way to feed the same input, the results must be the same.
static const src_mode src_modes[] = {
    { SRC_INSITU, 0, false },
    { SRC_MEM, 0, false },
    { SRC_CB, 0, false },
    { SRC_CB, 1, false },
    { SRC_CB, 2, false },
    { SRC_CB, 3, false },
    { SRC_CB, 7, false },
    { SRC_CB, 64, false },
};

#define each_mode(m) \
    for (const src_mode *m = src_modes; \
         m < src_modes + yy_nelems(src_modes); m++)

// A reader with its input. The input is exactly `len` bytes long (in-situ
// adds the padding), so a read past the end is caught by a sanitizer.
typedef struct {
    yyjson_sr sr;
    char *dat;
    usize len;
    usize pos; // callback read position
    usize chunk;
} rd_t;

static usize rd_read(void *ctx, void *dst, usize cap) {
    rd_t *r = (rd_t *)ctx;
    usize n = r->len - r->pos;
    if (r->chunk && n > r->chunk) n = r->chunk;
    if (n > cap) n = cap;
    if (n) memcpy(dst, r->dat + r->pos, n);
    r->pos += n;
    return n;
}

static bool rd_seek(void *ctx, u64 pos) {
    rd_t *r = (rd_t *)ctx;
    if (pos > r->len) return false;
    r->pos = (usize)pos;
    return true;
}

static bool rd_open(rd_t *r, src_mode m, const void *json, usize len,
                    const yyjson_sr_opts *opts, const yyjson_alc *alc) {
    yyjson_sr_opts opt = {0};
    usize pad = m.kind == SRC_INSITU ? YYJSON_PADDING_SIZE : 0;
    memset(r, 0, sizeof(*r));
    r->dat = dup_pad(json, len, pad);
    r->len = len;
    r->chunk = m.chunk;
    if (opts) opt = *opts;
    if (m.kind == SRC_CB) {
        return yyjson_sr_init_cb(&r->sr, rd_read, m.seek ? rd_seek : NULL, r,
                                 &opt, alc);
    }
    if (m.kind == SRC_INSITU) opt.flg |= YYJSON_READ_INSITU;
    return yyjson_sr_init_mem(&r->sr, r->dat, len, &opt, alc);
}

static void rd_close(rd_t *r) {
    yyjson_sr_free(&r->sr);
    free(r->dat);
    r->dat = NULL;
}

// Opens a null-terminated `json` and expects init to succeed.
static void rd_start_opts(rd_t *r, src_mode m, const char *json,
                          const yyjson_sr_opts *opts) {
    yy_assert(rd_open(r, m, json, strlen(json), opts, NULL));
}

static void rd_start(rd_t *r, src_mode m, const char *json) {
    yy_assert(rd_open(r, m, json, strlen(json), NULL, NULL));
}

static void expect_err(yyjson_sr *sr, yyjson_read_code code) {
    yyjson_read_err err;
    yy_assert(yyjson_sr_has_err(sr));
    yy_assertf(yyjson_sr_get_err(sr, &err) == code, "error %d (%s), expect %d",
               (int)err.code, err.msg ? err.msg : "?", (int)code);
    yy_assert(err.code == code && err.msg != NULL);
}

static void expect_ok(yyjson_sr *sr) {
    yyjson_read_err err;
    yy_assert(!yyjson_sr_has_err(sr));
    yy_assert(yyjson_sr_get_err(sr, &err) == YYJSON_READ_SUCCESS);
    yy_assert(err.code == YYJSON_READ_SUCCESS && err.msg == NULL);
}

// Reads a string, expects its bytes, and the null terminator.
static void expect_strn(yyjson_sr *sr, const char *str, usize len) {
    yyjson_sv sv = yyjson_sr_read_str(sr);
    yy_assert(sv.ptr != NULL && sv.len == len);
    yy_assert(!len || !memcmp(sv.ptr, str, len));
    yy_assert(sv.ptr[len] == '\0');
}

static void expect_str(yyjson_sr *sr, const char *str) {
    expect_strn(sr, str, strlen(str));
}

// Expects the next object member to have this key.
static void expect_key(yyjson_sr *sr, const char *key) {
    yyjson_sv sv = yyjson_sv_make(NULL, 0);
    yy_assert(yyjson_sr_obj_next(sr, &sv));
    yy_assert(yyjson_sv_equals_str(sv, key));
    yy_assert(sv.ptr[sv.len] == '\0');
}

// Renders the current value in a compact text form, for comparison:
//   uint/sint "12" "-3", real "0.5" "2.0", raw "<12>", string "\"..\""
//   with `\` as `\\` and bytes outside 0x20~0x7E as `\xHH`.
static void dump_str(sink_t *out, yyjson_sv sv) {
    static const char hex[] = "0123456789ABCDEF";
    usize i;
    yy_assert(sv.ptr != NULL);
    sink_puts(out, "\"");
    for (i = 0; i < sv.len; i++) {
        u8 c = (u8)sv.ptr[i];
        if (c == '\\') {
            sink_puts(out, "\\\\");
        } else if (c >= 0x20 && c < 0x7F) {
            sink_put(out, &c, 1);
        } else {
            char esc[4] = { '\\', 'x', hex[c >> 4], hex[c & 15] };
            sink_put(out, esc, 4);
        }
    }
    sink_puts(out, "\"");
}

static bool dump_val(yyjson_sr *sr, sink_t *out) {
    char tmp[64];
    yyjson_sv sv;
    yyjson_val val;
    bool first = true;

    switch (yyjson_sr_peek_type(sr)) {
    case YYJSON_TYPE_ARR:
        if (!yyjson_sr_arr_begin(sr)) return false;
        sink_puts(out, "[");
        while (yyjson_sr_arr_next(sr)) {
            if (!first) sink_puts(out, ",");
            first = false;
            if (!dump_val(sr, out)) return false;
        }
        if (!yyjson_sr_arr_end(sr)) return false;
        sink_puts(out, "]");
        return true;

    case YYJSON_TYPE_OBJ:
        if (!yyjson_sr_obj_begin(sr)) return false;
        sink_puts(out, "{");
        while (yyjson_sr_obj_next(sr, &sv)) {
            if (!first) sink_puts(out, ",");
            first = false;
            yy_assert(sv.ptr && sv.ptr[sv.len] == '\0');
            dump_str(out, sv);
            sink_puts(out, ":");
            if (!dump_val(sr, out)) return false;
        }
        if (!yyjson_sr_obj_end(sr)) return false;
        sink_puts(out, "}");
        return true;

    case YYJSON_TYPE_STR:
        sv = yyjson_sr_read_str(sr);
        if (!sv.ptr) return false;
        yy_assert(sv.ptr[sv.len] == '\0');
        dump_str(out, sv);
        return true;

    default:
        val = yyjson_sr_read_scalar(sr);
        if (yyjson_is_null(&val)) {
            sink_puts(out, "null");
        } else if (yyjson_is_bool(&val)) {
            sink_puts(out, yyjson_get_bool(&val) ? "true" : "false");
        } else if (yyjson_is_uint(&val)) {
            snprintf(tmp, sizeof(tmp), "%llu",
                     (unsigned long long)yyjson_get_uint(&val));
            sink_puts(out, tmp);
        } else if (yyjson_is_sint(&val)) {
            snprintf(tmp, sizeof(tmp), "%lld",
                     (long long)yyjson_get_sint(&val));
            sink_puts(out, tmp);
        } else if (yyjson_is_real(&val)) {
            f64 n = yyjson_get_real(&val);
            if (n == 0.5) sink_puts(out, "0.5");
            else if (n == 2.0) sink_puts(out, "2.0");
            else if (n == -0.5) sink_puts(out, "-0.5");
            else if (n == 0.0) sink_puts(out, "0.0");
            else return false;
        } else if (yyjson_is_raw(&val)) {
            sink_puts(out, "<");
            sink_put(out, yyjson_get_raw(&val), yyjson_get_len(&val));
            sink_puts(out, ">");
        } else {
            return false;
        }
        return !yyjson_sr_has_err(sr);
    }
}

// Dumps a whole document. Returns false if it cannot be read.
static bool dump_doc(yyjson_sr *sr, sink_t *out) {
    return dump_val(sr, out) && yyjson_sr_finish(sr);
}

// Expects that `json` dumps to `dump` in every mode.
static void check_dump_flg(const char *json, yyjson_read_flag flg,
                           const char *dump) {
    each_mode(m) {
        rd_t r;
        sink_t out = {0};
        yy_assert(rd_open(&r, *m, json, strlen(json),
                  &(yyjson_sr_opts){ flg, 0, 0, 0 }, NULL));
        yy_assertf(dump_doc(&r.sr, &out), "should pass: %s", json);
        yy_assertf(!strcmp(out.dat, dump), "got %s, expect %s", out.dat, dump);
        sink_free(&out);
        rd_close(&r);
    }
}

static void check_dump(const char *json, const char *dump) {
    check_dump_flg(json, 0, dump);
}

// Expects that reading all of `json` fails, in every mode, with the same
// error code and position. A `code` of 0 accepts any error code.
static void check_bad_n(const char *json, usize len, yyjson_read_flag flg,
                        yyjson_read_code code) {
    usize pos = 0;
    yyjson_read_code first = 0;
    each_mode(m) {
        rd_t r;
        sink_t out = {0};
        yyjson_read_err err;
        yy_assert(rd_open(&r, *m, json, len, &(yyjson_sr_opts){ flg, 0, 0, 0 }, NULL));
        yy_assertf(!dump_doc(&r.sr, &out), "should fail: %s", json);
        yy_assertf(yyjson_sr_get_err(&r.sr, &err) != YYJSON_READ_SUCCESS,
                   "no error code: %s", json);
        if (m == src_modes) {
            first = err.code;
            pos = err.pos;
        }
        yy_assertf(!code || err.code == code, "%s: code %d, expect %d", json,
                   (int)err.code, (int)code);
        yy_assertf(err.code == first && err.pos == pos,
                   "%s: mode %d differs: code %d pos %zu, first %d %zu", json,
                   (int)(m - src_modes), (int)err.code, (usize)err.pos,
                   (int)first, pos);
        yy_assert(yyjson_sr_pos(&r.sr) == pos);
        sink_free(&out);
        rd_close(&r);
    }
}

static void check_bad_flg(const char *json, yyjson_read_flag flg,
                          yyjson_read_code code) {
    check_bad_n(json, strlen(json), flg, code);
}

static void check_bad(const char *json, yyjson_read_code code) {
    check_bad_n(json, strlen(json), 0, code);
}



/*==============================================================================
 * MARK: - Init
 *============================================================================*/

// Calls every API on a reader that cannot work: NULL, a failed init, or one
// with a sticky error. All of them fail and return the default value.
static void expect_dead(yyjson_sr *sr) {
    yyjson_sv sv = yyjson_sv_make("x", 1);
    yyjson_val val;
    u64 start = UINT64_MAX;
    yy_assert(!yyjson_sr_obj_begin_rewind(sr, &start));
    yy_assert(start == UINT64_MAX);
    yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NONE);
    yy_assert(!yyjson_sr_arr_begin(sr));
    yy_assert(!yyjson_sr_arr_next(sr));
    yy_assert(!yyjson_sr_arr_end(sr));
    yy_assert(!yyjson_sr_obj_begin(sr));
    yy_assert(!yyjson_sr_obj_next(sr, &sv));
    yy_assert(!yyjson_sr_obj_find(sr, "a"));
    yy_assert(!yyjson_sr_obj_findn(sr, "a", 1));
    yy_assert(!yyjson_sr_obj_find_rewind(sr, "a", 0));
    yy_assert(!yyjson_sr_obj_find_rewindn(sr, "a", 1, 0));
    yy_assert(!yyjson_sr_obj_end(sr));
    sv = yyjson_sr_read_str(sr);
    yy_assert(!sv.ptr && !sv.len);
    sv = yyjson_sr_read_raw(sr);
    yy_assert(!sv.ptr && !sv.len);
    sv = yyjson_sr_obj_read_str(sr, "a");
    yy_assert(!sv.ptr && !sv.len);
    yy_assert(!yyjson_sr_read_uint(sr));
    yy_assert(!yyjson_sr_read_sint(sr));
    yy_assert(yyjson_sr_read_real(sr) == 0.0);
    yy_assert(!yyjson_sr_read_bool(sr));
    yy_assert(!yyjson_sr_read_null(sr));
    val = yyjson_sr_read_num(sr);
    yy_assert(yyjson_get_type(&val) == YYJSON_TYPE_NONE);
    val = yyjson_sr_read_scalar(sr);
    yy_assert(yyjson_get_type(&val) == YYJSON_TYPE_NONE);
    yy_assert(!yyjson_sr_skip_val(sr));
    yy_assert(!yyjson_sr_obj_read_uint(sr, "a"));
    yy_assert(!yyjson_sr_obj_read_sint(sr, "a"));
    yy_assert(yyjson_sr_obj_read_real(sr, "a") == 0.0);
    yy_assert(!yyjson_sr_obj_read_bool(sr, "a"));
    yy_assert(!yyjson_sr_obj_read_null(sr, "a"));
    yy_assert(!yyjson_sr_finish(sr));
    yy_assert(!yyjson_sr_doc_next(sr));
}

static void test_sr_init(void) {
    yyjson_sr sr = {0};
    char buf[16] = "0";

    // NULL handle
    yy_assert(!yyjson_sr_init_mem(NULL, buf, 1, NULL, NULL));
    yy_assert(!yyjson_sr_init_cb(NULL, rd_read, NULL, NULL, NULL, NULL));
    expect_dead(NULL);
    yy_assert(yyjson_sr_has_err(NULL));
    yy_assert(yyjson_sr_get_err(NULL, NULL) ==
              YYJSON_READ_ERROR_INVALID_PARAMETER);
    {
        yyjson_read_err err = {0};
        yy_assert(yyjson_sr_get_err(NULL, &err) ==
                  YYJSON_READ_ERROR_INVALID_PARAMETER);
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
        yy_assert(err.msg != NULL && err.pos == 0);
    }
    yy_assert(yyjson_sr_pos(NULL) == 0);
    yy_assert(yyjson_sr_depth(NULL) == 0);
    yy_assert(!yyjson_sr_can_rewind(NULL));
    yyjson_sr_free(NULL);

    // bad arguments leave a handle that fails, and can be freed
    yy_assert(!yyjson_sr_init_mem(&sr, NULL, 0, NULL, NULL));
    expect_err(&sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    expect_dead(&sr);
    yyjson_sr_free(&sr);
    yy_assert(!yyjson_sr_init_mem(&sr, buf, SIZE_MAX, NULL, NULL));
    expect_err(&sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    expect_dead(&sr);
    yyjson_sr_free(&sr);
    yy_assert(!yyjson_sr_init_cb(&sr, NULL, NULL, NULL, NULL, NULL));
    expect_err(&sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    expect_dead(&sr);
    yyjson_sr_free(&sr);
    yyjson_sr_free(&sr);

    // an empty input is valid, and has no document
    yy_assert(yyjson_sr_init_mem(&sr, buf, 0, NULL, NULL));
    expect_ok(&sr);
    yy_assert(!yyjson_sr_doc_next(&sr));
    expect_ok(&sr);
    yy_assert(!yyjson_sr_finish(&sr));
    expect_err(&sr, YYJSON_READ_ERROR_EMPTY_CONTENT);
    yyjson_sr_free(&sr);

    // a callback may have no context, and returns EOF at once
    yy_assert(yyjson_sr_init_cb(&sr, rd_read, NULL, NULL, NULL, NULL));
    yyjson_sr_free(&sr);
}

// A handle is reused after free, with any source.
static void test_sr_reuse(void) {
    static const char json[] = "{\"k\":[1,2]}";
    yyjson_sr sr = {0};
    src_mode modes[3] = { src_modes[0], src_modes[1], src_modes[3] };
    rd_t r;
    sink_t out = {0};
    usize i, j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            yy_assert(rd_open(&r, modes[j], json, sizeof(json) - 1, NULL, NULL));
            yy_assert(dump_doc(&r.sr, &out));
            yy_assert(!strcmp(out.dat, "{\"k\":[1,2]}"));
            sink_free(&out);
            rd_close(&r);
        }
    }
    yy_assert(yyjson_sr_init_mem(&sr, (char *)json, sizeof(json) - 1, NULL, NULL));
    yy_assert(yyjson_sr_obj_begin(&sr));
    yyjson_sr_free(&sr);

    // after an error, a new init starts clean
    yy_assert(!yyjson_sr_init_cb(&sr, NULL, NULL, NULL, NULL, NULL));
    yyjson_sr_free(&sr);
    yy_assert(yyjson_sr_init_mem(&sr, (char *)json, sizeof(json) - 1, NULL, NULL));
    expect_ok(&sr);
    yy_assert(yyjson_sr_obj_begin(&sr));
    yyjson_sr_free(&sr);
}



/*==============================================================================
 * MARK: - Value
 *============================================================================*/

static void test_sr_values(void) {
    static const struct { const char *json, *dump; } cases[] = {
        // literals
        { "null", "null" },
        { "true", "true" },
        { "false", "false" },
        { " \t\r\ntrue \n", "true" },
        // integers, and numbers that are not
        { "0", "0" },
        { "42", "42" },
        { "-42", "-42" },
        { "18446744073709551615", "18446744073709551615" },
        { "-9223372036854775808", "-9223372036854775808" },
        { "0.5", "0.5" },
        { "2.0", "2.0" },
        { "-0.5", "-0.5" },
        { "0.0", "0.0" },
        // strings
        { "\"\"", "\"\"" },
        { "\"abc\"", "\"abc\"" },
        { "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"",
          "\"\"\\\\/\\x08\\x0C\\x0A\\x0D\\x09\"" },
        { "\"\\u0041\\u00e9\\u20ac\\uD83D\\ude00\"",
          "\"A\\xC3\\xA9\\xE2\\x82\\xAC\\xF0\\x9F\\x98\\x80\"" },
        { "\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"",
          "\"\\xC3\\xA9\\xE2\\x82\\xAC\\xF0\\x9F\\x98\\x80\"" },
        { "\"\\u0000\\u001f\\u007f\"", "\"\\x00\\x1F\\x7F\"" },
        // containers
        { "[]", "[]" },
        { "{}", "{}" },
        { "[[]]", "[[]]" },
        { "[[],{},[[]]]", "[[],{},[[]]]" },
        { " [ 1 , 2 ] ", "[1,2]" },
        { "[1,[2,[3,[4,[5]]]]]", "[1,[2,[3,[4,[5]]]]]" },
        { "{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":{}}}}" },
        { "{\"a\":1,\"b\":[true,null],\"c\":{\"d\":\"e\"}}",
          "{\"a\":1,\"b\":[true,null],\"c\":{\"d\":\"e\"}}" },
        { "{\"a\":1,\"a\":2}", "{\"a\":1,\"a\":2}" },
        { "{\"\":0}", "{\"\":0}" },
        { "{\"\\u0041\\n\":\"\\ud83d\\ude00\"}",
          "{\"A\\x0A\":\"\\xF0\\x9F\\x98\\x80\"}" },
        { " {\t\"k\"\n:\r[ ]\n}\n", "{\"k\":[]}" },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        check_dump(cases[i].json, cases[i].dump);
    }
}

// A number is read as uint, sint or real, as the type allows.
typedef struct {
    const char *text;
    bool u_ok, s_ok, r_ok;
    u64 u;
    i64 s;
    f64 r;
} num_case;

static const num_case num_cases[] = {
    { "0",                    1, 1, 1, 0, 0, 0.0 },
    { "7",                    1, 1, 1, 7, 7, 7.0 },
    { "-7",                   0, 1, 1, 0, -7, -7.0 },
    { "18446744073709551615", 1, 0, 0, UINT64_MAX, 0, 0.0 },
    { "9223372036854775807",  1, 1, 0, INT64_MAX, INT64_MAX, 0.0 },
    { "9223372036854775808",  1, 0, 0, (u64)INT64_MAX + 1, 0, 0.0 },
    { "-9223372036854775808", 0, 1, 0, 0, INT64_MIN, 0.0 },
    { "0.5",                  0, 0, 1, 0, 0, 0.5 },
    { "2.0",                  0, 0, 1, 0, 0, 2.0 },
    { "-0.5",                 0, 0, 1, 0, 0, -0.5 },
    { "0.0",                  0, 0, 1, 0, 0, 0.0 },
    { "1e999",                0, 0, 0, 0, 0, 0.0 },
    { "-1e999",               0, 0, 0, 0, 0, 0.0 },
};

static void test_sr_numbers(void) {
    usize i;
    for (i = 0; i < yy_nelems(num_cases); i++) {
        const num_case *c = &num_cases[i];
        // alone, ended by EOF, then inside an array, ended by ","
        usize form, type;
        for (form = 0; form < 2; form++) {
            char json[64];
            snprintf(json, sizeof(json), form ? "[%s,0]" : "%s", c->text);
            for (type = 0; type < 3; type++) {
                bool ok = type == 0 ? c->u_ok : type == 1 ? c->s_ok : c->r_ok;
                /* big integers are checked as integers, not as doubles */
                if (type == 2 && !c->r_ok && (c->u_ok || c->s_ok)) continue;
                each_mode(m) {
                    rd_t r;
                    yyjson_sr *sr = &r.sr;
                    rd_start(&r, *m, json);
                    if (form) {
                        yy_assert(yyjson_sr_arr_begin(sr));
                        yy_assert(yyjson_sr_arr_next(sr));
                    }
                    if (type == 0) {
                        yy_assert(yyjson_sr_read_uint(sr) == (ok ? c->u : 0));
                    } else if (type == 1) {
                        yy_assert(yyjson_sr_read_sint(sr) == (ok ? c->s : 0));
                    } else {
                        yy_assert(yyjson_sr_read_real(sr) == (ok ? c->r : 0.0));
                    }
                    if (ok) {
                        expect_ok(sr);
                        if (form) {
                            yy_assert(yyjson_sr_arr_next(sr));
                            yy_assert(yyjson_sr_read_uint(sr) == 0);
                            yy_assert(!yyjson_sr_arr_next(sr));
                            yy_assert(yyjson_sr_arr_end(sr));
                        }
                        yy_assert(yyjson_sr_finish(sr));
                    } else {
                        expect_err(sr, YYJSON_READ_ERROR_INVALID_NUMBER);
                    }
                    rd_close(&r);
                }
            }
        }
    }
}

// Each number kind reads as a standalone value.
static void test_sr_num_val(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_val v;
        rd_start(&r, *m, "[1,-1,0.5,2.0,\"s\",true,null]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_num(sr);
        yy_assert(yyjson_is_uint(&v) && yyjson_get_uint(&v) == 1);
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_num(sr);
        yy_assert(yyjson_is_sint(&v) && yyjson_get_sint(&v) == -1);
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_num(sr);
        yy_assert(yyjson_is_real(&v) && yyjson_get_real(&v) == 0.5);
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_num(sr);
        yy_assert(yyjson_is_real(&v) && yyjson_get_real(&v) == 2.0);
        // read_num takes numbers only, read_scalar takes all but containers
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_scalar(sr);
        yy_assert(yyjson_is_str(&v) && yyjson_equals_str(&v, "s"));
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_scalar(sr);
        yy_assert(yyjson_is_true(&v));
        yy_assert(yyjson_sr_arr_next(sr));
        v = yyjson_sr_read_num(sr);
        yy_assert(yyjson_get_type(&v) == YYJSON_TYPE_NONE);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);
    }
}

static void test_sr_literals(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        rd_start(&r, *m, "[true,false,null,true]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_bool(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(!yyjson_sr_read_bool(sr));
        expect_ok(sr);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_null(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_bool(sr));
        yy_assert(!yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // the wrong literal is an error, not just false
        rd_start(&r, *m, "[true,null]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(!yyjson_sr_read_null(sr));
        expect_err(sr, YYJSON_READ_ERROR_LITERAL);
        rd_close(&r);

        // a literal ended by EOF
        rd_start(&r, *m, "false");
        yy_assert(!yyjson_sr_read_bool(sr));
        expect_ok(sr);
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
}



/*==============================================================================
 * MARK: - String
 *============================================================================*/

static void test_sr_strings(void) {
    // a long string cut at every place by the small chunks, with an escape
    // after every 7 bytes (5 bytes in, 1 byte out, and a 4-byte emoji)
    enum { count = 600 };
    static const char unit[] = "abcdef\\n\\ud83d\\ude00ghijk\\u00e9lmnop\\\\";
    static const char want[] = "abcdef\n\xf0\x9f\x98\x80ghijk\xc3\xa9lmnop\\";
    usize ulen = sizeof(unit) - 1, wlen = sizeof(want) - 1, i;
    char *json = (char *)malloc(count * ulen + 8);
    char *dec = (char *)malloc(count * wlen + 1);
    usize len = 0;
    yy_assert(json && dec);
    json[len++] = '[';
    json[len++] = '"';
    for (i = 0; i < count; i++) {
        memcpy(json + len, unit, ulen);
        len += ulen;
        memcpy(dec + i * wlen, want, wlen);
    }
    json[len++] = '"';
    json[len++] = ']';
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yy_assert(rd_open(&r, *m, json, len, NULL, NULL));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        expect_strn(sr, dec, count * wlen);
        yy_assert(!yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
    // the same as an object key
    len = 0;
    json[len++] = '{';
    json[len++] = '"';
    for (i = 0; i < count; i++) {
        memcpy(json + len, unit, ulen);
        len += ulen;
    }
    memcpy(json + len, "\":0}", 4);
    len += 4;
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv key;
        yy_assert(rd_open(&r, *m, json, len, NULL, NULL));
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(key.len == count * wlen && !memcmp(key.ptr, dec, key.len));
        yy_assert(key.ptr[key.len] == '\0');
        yy_assert(yyjson_sr_read_uint(sr) == 0);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
    free(json);
    free(dec);
}

static void test_sr_str_edge(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv sv, key;

        // an empty string and key are not NULL
        rd_start(&r, *m, "{\"\":\"\"}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(key.ptr && key.len == 0 && key.ptr[0] == '\0');
        sv = yyjson_sr_read_str(sr);
        yy_assert(sv.ptr && sv.len == 0 && sv.ptr[0] == '\0');
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // an embedded NUL counts in the length
        rd_start(&r, *m, "\"a\\u0000b\"");
        expect_strn(sr, "a\0b", 3);
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // a key is valid until its value is read
        rd_start(&r, *m, "{\"k1\":\"v1\",\"k2\\n\":2}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sv_equals_str(key, "k1"));
        yy_assert(yyjson_sv_equals_str(yyjson_sr_read_str(sr), "v1"));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sv_equals_str(key, "k2\n"));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(!yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
}

// Bad strings, as a value and as a key.
static void test_sr_str_bad(void) {
    static const char *strs[] = {
        "\\q", "\\x41", "\\u12", "\\u12G4", "\\u00zz", "\\ud800", "\\udc00",
        "\\ud800x", "\\ud800\\u0041", "\\ud800\\ud800", "\\udc00\\ud800",
        "a\nb", "a\tb", "\x01", "\x1f",
    };
    usize i;
    for (i = 0; i < yy_nelems(strs); i++) {
        char json[64];
        snprintf(json, sizeof(json), "[\"%s\"]", strs[i]);
        check_bad(json, YYJSON_READ_ERROR_INVALID_STRING);
        snprintf(json, sizeof(json), "{\"%s\":0}", strs[i]);
        check_bad(json, YYJSON_READ_ERROR_INVALID_STRING);
    }
    // cut by EOF
    check_bad("[\"abc", YYJSON_READ_ERROR_UNEXPECTED_END);
    check_bad("{\"abc", YYJSON_READ_ERROR_UNEXPECTED_END);
    check_bad("\"\\", YYJSON_READ_ERROR_UNEXPECTED_END);
    check_bad("\"\\u12", 0);
    check_bad("\"\\ud83d\\ude", 0);
    // a stray quote ends the string early
    check_bad("[\"a\"b\"]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
}

#if !YYJSON_DISABLE_UTF8_VALIDATION
// Invalid UTF-8 is rejected in a string or key, even if a short chunk cuts
// a sequence in half.
static void test_sr_utf8_bad(void) {
    static const char *seqs[] = {
        "\xe0\x80\x80",     // overlong
        "\xc0\x80",         // overlong
        "\xed\xa0\x80",     // surrogate
        "\xf4\x90\x80\x80", // above U+10FFFF
        "\xe6\x97",         // truncated
        "\x80",             // continuation
        "\xff",             // never valid
    };
    usize i;
    for (i = 0; i < yy_nelems(seqs); i++) {
        char json[32];
        snprintf(json, sizeof(json), "[\"a%sb\"]", seqs[i]);
        check_bad(json, YYJSON_READ_ERROR_INVALID_STRING);
        snprintf(json, sizeof(json), "{\"a%sb\":1}", seqs[i]);
        check_bad(json, YYJSON_READ_ERROR_INVALID_STRING);
        snprintf(json, sizeof(json), "[\"\\u0041%s\"]", seqs[i]);
        check_bad(json, YYJSON_READ_ERROR_INVALID_STRING);
    }
}
#endif



/*==============================================================================
 * MARK: - Flag
 *============================================================================*/

// These flags are ignored by the streaming reader.
#define SR_IGNORED_FLAGS ( \
    YYJSON_READ_STOP_WHEN_DONE | YYJSON_READ_ALLOW_TRAILING_COMMAS | \
    YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_INF_AND_NAN | \
    YYJSON_READ_ALLOW_BOM | YYJSON_READ_ALLOW_EXT_NUMBER | \
    YYJSON_READ_ALLOW_EXT_ESCAPE | YYJSON_READ_ALLOW_EXT_WHITESPACE | \
    YYJSON_READ_ALLOW_SINGLE_QUOTED_STR | YYJSON_READ_ALLOW_UNQUOTED_KEY)

static void test_sr_flags(void) {
    static const char big[] = "[0,-1,0.5,2.0,18446744073709551615,1e999]";
    // raw numbers keep their source text
    check_dump_flg(big, YYJSON_READ_NUMBER_AS_RAW,
                   "[<0>,<-1>,<0.5>,<2.0>,<18446744073709551615>,<1e999>]");
    // only a number that is not a finite double stays raw
    check_dump_flg(big, YYJSON_READ_BIGNUM_AS_RAW,
                   "[0,-1,0.5,2.0,18446744073709551615,<1e999>]");
    // the number flag overrides the big number flag
    check_dump_flg(big, YYJSON_READ_NUMBER_AS_RAW | YYJSON_READ_BIGNUM_AS_RAW,
                   "[<0>,<-1>,<0.5>,<2.0>,<18446744073709551615>,<1e999>]");
    // only read_num and read_scalar see the flags, typed reads do not
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yy_assert(rd_open(&r, *m, "[7,0.5,\"s\"]", 11,
                  &(yyjson_sr_opts){ YYJSON_READ_NUMBER_AS_RAW, 0, 0, 0 }, NULL));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 7);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_real(sr) == 0.5);
        yy_assert(yyjson_sr_arr_next(sr));
        expect_str(sr, "s");
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
}

// The flags of other syntax do nothing: the input is still rejected.
static void test_sr_flags_ignored(void) {
    static const char *bad[] = {
        "[1,]", "{\"a\":1,}", "[1/*c*/]", "[1//c\n]", "[NaN]", "[Infinity]",
        "[-Infinity]", "[.5]", "[+1]", "[0x1F]", "[1.]", "['a']", "{a:1}",
        "[\"\\x41\"]", "[\"\\'\"]", "[\v1]", "\xef\xbb\xbf[]",
        "[1] 2", "[1]]", "{'a':1}",
    };
    usize i;
    for (i = 0; i < yy_nelems(bad); i++) {
        check_bad_flg(bad[i], SR_IGNORED_FLAGS, 0);
        check_bad(bad[i], 0);
    }
    check_bad_flg("\xef\xbb\xbf[]", SR_IGNORED_FLAGS,
                  YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
    check_dump_flg("[1,\"a\"]", SR_IGNORED_FLAGS, "[1,\"a\"]");
}

#if !YYJSON_DISABLE_NON_STANDARD
// With the flag, invalid UTF-8 is kept as it is, in a string and a key, on
// the copy-free path and after an escape forced a copy.
static void test_sr_utf8_allow(void) {
    static const char *seqs[] = {
        "\xe0\x80\x80", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        "\xe6\x97", "\x80", "\xff",
    };
    usize i;
    for (i = 0; i < yy_nelems(seqs); i++) {
        char json[64], json2[64], json3[64];
        snprintf(json, sizeof(json), "[\"a%sb\"]", seqs[i]);
        snprintf(json2, sizeof(json2), "{\"\\u0041%s\":\"\\u0042%s\"}",
                 seqs[i], seqs[i]);
        snprintf(json3, sizeof(json3), "{\"%s\":0,\"a\":1}", seqs[i]);
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            yyjson_sv sv, key;
            yy_assert(rd_open(&r, *m, json, strlen(json),
                      &(yyjson_sr_opts){ YYJSON_READ_ALLOW_INVALID_UNICODE, 0, 0, 0 },
                      NULL));
            yy_assert(yyjson_sr_arr_begin(sr));
            yy_assert(yyjson_sr_arr_next(sr));
            sv = yyjson_sr_read_str(sr);
            yy_assert(sv.len == strlen(seqs[i]) + 2);
            yy_assert(!memcmp(sv.ptr + 1, seqs[i], strlen(seqs[i])));
            yy_assert(yyjson_sr_arr_end(sr));
            yy_assert(yyjson_sr_finish(sr));
            rd_close(&r);

            // a key, and a string with an escape
            yy_assert(rd_open(&r, *m, json2, strlen(json2),
                      &(yyjson_sr_opts){ YYJSON_READ_ALLOW_INVALID_UNICODE, 0, 0, 0 },
                      NULL));
            yy_assert(yyjson_sr_obj_begin(sr));
            yy_assert(yyjson_sr_obj_next(sr, &key));
            yy_assert(key.len == strlen(seqs[i]) + 1 && key.ptr[0] == 'A');
            yy_assert(!memcmp(key.ptr + 1, seqs[i], strlen(seqs[i])));
            sv = yyjson_sr_read_str(sr);
            yy_assert(sv.len == strlen(seqs[i]) + 1 && sv.ptr[0] == 'B');
            yy_assert(!memcmp(sv.ptr + 1, seqs[i], strlen(seqs[i])));
            yy_assert(yyjson_sr_obj_end(sr));
            yy_assert(yyjson_sr_finish(sr));
            rd_close(&r);

            // searches also allow invalid UTF-8 in keys they pass
            yy_assert(rd_open(&r, *m, json3, strlen(json3),
                      &(yyjson_sr_opts){ YYJSON_READ_ALLOW_INVALID_UNICODE,
                                         0, 0, 0 }, NULL));
            yy_assert(yyjson_sr_obj_begin(sr));
            yy_assert(yyjson_sr_obj_read_uint(sr, "a") == 1);
            yy_assert(yyjson_sr_obj_end(sr) && yyjson_sr_finish(sr));
            rd_close(&r);
        }
    }
    // valid input is not changed by the flag
    check_dump_flg("[\"\xe4\xb8\xad\"]", YYJSON_READ_ALLOW_INVALID_UNICODE,
                   "[\"\\xE4\\xB8\\xAD\"]");
}
#endif

// A raw value is the source text, not validated.
static void test_sr_raw(void) {
    static const struct { const char *json, *raw; } cases[] = {
        { "123", "123" },
        { "-0.5", "-0.5" },
        { "\"a\\n\"", "\"a\\n\"" },
        { "true", "true" },
        { "null", "null" },
        { "[]", "[]" },
        { "{}", "{}" },
        { "[1, [2, \"]\"], {\"a\": \"}\\\"\"}]", "[1, [2, \"]\"], {\"a\": \"}\\\"\"}]" },
        { "  {\"k\" : [ ]}  ", "{\"k\" : [ ]}" },
        // not validated
        { "[1 2 x]", "[1 2 x]" },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            yyjson_sv sv;
            rd_start(&r, *m, cases[i].json);
            sv = yyjson_sr_read_raw(sr);
            yy_assert(sv.ptr && sv.len == strlen(cases[i].raw));
            yy_assert(!memcmp(sv.ptr, cases[i].raw, sv.len));
            yy_assert(yyjson_sr_finish(sr));
            rd_close(&r);
        }
    }
    // inside containers, in order with the other reads
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv sv;
        rd_start(&r, *m, "{\"a\":[1,{}],\"b\":2}");
        yy_assert(yyjson_sr_obj_begin(sr));
        expect_key(sr, "a");
        sv = yyjson_sr_read_raw(sr);
        yy_assert(sv.len == 6 && !memcmp(sv.ptr, "[1,{}]", 6));
        expect_key(sr, "b");
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
    check_bad("[1,", YYJSON_READ_ERROR_UNEXPECTED_END);
}



/*==============================================================================
 * MARK: - Navigation
 *============================================================================*/

static void test_sr_peek(void) {
    static const struct { const char *json; yyjson_type type; } cases[] = {
        { "{", YYJSON_TYPE_OBJ },
        { "[", YYJSON_TYPE_ARR },
        { "\"", YYJSON_TYPE_STR },
        { "0", YYJSON_TYPE_NUM },
        { "9", YYJSON_TYPE_NUM },
        { "-", YYJSON_TYPE_NUM },
        { "true", YYJSON_TYPE_BOOL },
        { "false", YYJSON_TYPE_BOOL },
        { "null", YYJSON_TYPE_NULL },
        // the value is not validated
        { "-x", YYJSON_TYPE_NUM },
        { "tx", YYJSON_TYPE_BOOL },
        { "fx", YYJSON_TYPE_BOOL },
        { "nul", YYJSON_TYPE_NULL },
        // cannot start a value
        { ",", YYJSON_TYPE_NONE },
        { "]", YYJSON_TYPE_NONE },
        { "}", YYJSON_TYPE_NONE },
        { ":", YYJSON_TYPE_NONE },
        { "x", YYJSON_TYPE_NONE },
        { "'", YYJSON_TYPE_NONE },
        { "+1", YYJSON_TYPE_NONE },
        { ".5", YYJSON_TYPE_NONE },
        { "T", YYJSON_TYPE_NONE },
        { "", YYJSON_TYPE_NONE },
        { "   ", YYJSON_TYPE_NONE },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        char json[64];
        // after whitespace, which a small chunk cuts
        snprintf(json, sizeof(json), " \n\t  %s", cases[i].json);
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            rd_start(&r, *m, json);
            yy_assert(yyjson_sr_peek_type(sr) == cases[i].type);
            yy_assert(yyjson_sr_peek_type(sr) == cases[i].type);
            expect_ok(sr);
            rd_close(&r);
        }
    }
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        // peek does not consume, and needs no current value
        rd_start(&r, *m, "[ 1 , \"s\" ]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NUM);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NUM);
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NONE);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_STR);
        expect_str(sr, "s");
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NONE);
        expect_ok(sr);
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // after an error
        rd_start(&r, *m, "[1]");
        yy_assert(!yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NONE);
        rd_close(&r);
    }
}

#define SAMPLE "{\"a\":1,\"b\":\"two\",\"c\":[3,{\"c\":4}],\"d\":{\"e\":5}," \
               "\"f\":-6,\"g\":0.5,\"h\":true,\"i\":null,\"j\":\"x\"}"

static void test_sr_find(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv sv;

        // forward, with members skipped on the way
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_obj_find(sr, "c"));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "c"));
        yy_assert(yyjson_sr_read_uint(sr) == 4);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_obj_find(sr, "d"));
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "e"));
        yy_assert(yyjson_sr_read_uint(sr) == 5);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_obj_find(sr, "g"));
        yy_assert(yyjson_sr_read_real(sr) == 0.5);
        yy_assert(yyjson_sr_obj_find(sr, "j"));
        expect_str(sr, "x");
        yy_assert(!yyjson_sr_obj_next(sr, &sv));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        expect_ok(sr);
        rd_close(&r);

        // a member behind the cursor is a miss, not an error, and later
        // finds miss too, as the cursor is at the end of the object
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "b"));
        expect_str(sr, "two");
        yy_assert(!yyjson_sr_obj_find(sr, "a"));
        expect_ok(sr);
        yy_assert(!yyjson_sr_obj_find(sr, "j"));
        yy_assert(!yyjson_sr_obj_next(sr, &sv));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        expect_ok(sr);
        rd_close(&r);

        // the key is matched by length: a prefix or a NUL does not match
        rd_start(&r, *m, "{\"ab\":1,\"a\\u0000b\":2,\"a\":3,\"\":4}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_findn(sr, "abxx", 2));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_obj_findn(sr, "a\0b", 3));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(!yyjson_sr_obj_findn(sr, NULL, 0));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "{\"a\\u0000b\":2,\"a\":3,\"\":4}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(yyjson_sr_obj_findn(sr, "", 0));
        yy_assert(yyjson_sr_read_uint(sr) == 4);
        rd_close(&r);

        // keys are compared after unescape
        rd_start(&r, *m, "{\"\\u0061\\u0062\":1,\"c\\n\\ud83d\\ude00\":2,\"d\":3}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "ab"));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_obj_find(sr, "c\n\xf0\x9f\x98\x80"));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_read_uint(sr, "d") == 3);
        rd_close(&r);

        // a duplicated key: the first one after the cursor
        rd_start(&r, *m, "{\"a\":1,\"a\":2,\"a\":3}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_read_uint(sr, "a") == 3);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
}

// The fast paths of find and obj_read: keys of a word or more, keys that need a
// closer look, and every layout around the colon, with the data in pieces of
// any size.
static void test_sr_find_fast(void) {
    static const char *members[] = {
        "\"abcdefg\": 1",
        "\"abcdefgh\" : 2",
        "\"abcdefghi\"  :  3",
        "\"abcdefghijklmnop\"\n:\t4",
        "\"abcdefghijklmnopq\":5",
        "\"ab\\\"cdefghijk\":6",
        "\"abcdefgh\\\\ijk\":7",
        "\"abcdefg\\nhij\":8",
        "\"ab\\u00e9cdefghijk\":9",
        "\"ab\303\251cdefghijk\":10",
        "\"whitespace_nl\": \n11",
        "\"whitespace_tab\": \t12",
        "\"whitespace_cr\": \r13",
        "\"whitespace_mix\": \n\t 14",
        "\"tail\":  15",
    };
    static const char *keys[] = {
        "abcdefg", "abcdefgh", "abcdefghi", "abcdefghijklmnop",
        "abcdefghijklmnopq", "ab\"cdefghijk", "abcdefgh\\ijk",
        "abcdefg\nhij", "ab\303\251cdefghijk", "ab\303\251cdefghijk",
        "whitespace_nl", "whitespace_tab", "whitespace_cr",
        "whitespace_mix", "tail",
    };
    sink_t json = {0};
    usize i, chunk;
    sink_puts(&json, "{");
    for (i = 0; i < yy_nelems(members); i++) {
        if (i) sink_puts(&json, i % 2 ? "," : " , ");
        sink_puts(&json, members[i]);
    }
    sink_puts(&json, "}");

    for (chunk = 0; chunk <= json.len + 1; chunk++) {
        src_mode mode = { chunk ? SRC_CB : SRC_MEM, chunk, false };
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yy_assert(rd_open(&r, mode, json.dat, json.len, NULL, NULL));
        yy_assert(yyjson_sr_obj_begin(sr));
        if (chunk % 2) {
            yy_assert(yyjson_sr_obj_read_uint(sr, "tail") == yy_nelems(keys));
        } else {
            yy_assert(yyjson_sr_obj_find(sr, "tail"));
            yy_assert(yyjson_sr_read_uint(sr) == yy_nelems(keys));
        }
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        expect_ok(sr);
        rd_close(&r);

        yy_assert(rd_open(&r, mode, json.dat, json.len, NULL, NULL));
        yy_assert(yyjson_sr_obj_begin(sr));
        for (i = 0; i < yy_nelems(keys); i++) {
            if (i % 2) {
                yy_assert(yyjson_sr_obj_read_uint(sr, keys[i]) == i + 1);
            } else {
                yy_assert(yyjson_sr_obj_find(sr, keys[i]));
                yy_assert(yyjson_sr_read_uint(sr) == i + 1);
            }
        }
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        expect_ok(sr);
        rd_close(&r);
    }
    sink_free(&json);
}

// The required-member helpers find and read in one call.
static void test_sr_obj_read(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv sv;

        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_read_uint(sr, "a") == 1);
        sv = yyjson_sr_obj_read_str(sr, "b");
        yy_assert(yyjson_sv_equals_str(sv, "two"));
        yy_assert(yyjson_sr_obj_read_sint(sr, "f") == -6);
        yy_assert(yyjson_sr_obj_read_real(sr, "g") == 0.5);
        yy_assert(yyjson_sr_obj_read_bool(sr, "h"));
        yy_assert(yyjson_sr_obj_read_null(sr, "i"));
        expect_ok(sr);
        sv = yyjson_sr_obj_read_str(sr, "j");
        yy_assert(yyjson_sv_equals_str(sv, "x"));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // a missing member, or one already passed, is an error
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_read_sint(sr, "f") == -6);
        yy_assert(yyjson_sr_obj_read_uint(sr, "a") == 0);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
        expect_dead(sr);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
        rd_close(&r);

        // a member of the wrong type is an error
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_read_uint(sr, "b") == 0);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_read_bool(sr, "a"));
        expect_err(sr, YYJSON_READ_ERROR_LITERAL);
        rd_close(&r);
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_read_null(sr, "a"));
        expect_err(sr, YYJSON_READ_ERROR_LITERAL);
        rd_close(&r);
        rd_start(&r, *m, SAMPLE);
        yy_assert(yyjson_sr_obj_begin(sr));
        sv = yyjson_sr_obj_read_str(sr, "a");
        yy_assert(!sv.ptr && !sv.len);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);

        // NULL key, and not inside an object
        rd_start(&r, *m, "{}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_find(sr, NULL));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "{}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_read_uint(sr, NULL));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "{}");
        yy_assert(yyjson_sr_obj_begin(sr));
        sv = yyjson_sr_obj_read_str(sr, NULL);
        yy_assert(!sv.ptr);
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "[1]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(!yyjson_sr_obj_read_uint(sr, "a"));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "{}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_next(sr, NULL));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
    }
}

// Every key passed by a search is checked, like the keys that are read.
static void test_sr_find_bad(void) {
    const struct { const char *json, *key; yyjson_read_code code; } cases[] = {
        { "{\"bad\nkey\":1}", "bad\nkey", YYJSON_READ_ERROR_INVALID_STRING },
        { "{\"bad\nkey\":0,\"a\":7}", "a", YYJSON_READ_ERROR_INVALID_STRING },
        { "{\"\\q\":1,\"a\":2}", "a", YYJSON_READ_ERROR_INVALID_STRING },
        { "{\"a\"b\":1}", "a\"b", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":1,b:2}", "c", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":1 \"b\":2}", "c", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":,\"b\":2}", "c", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":}", "c", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
#if !YYJSON_DISABLE_UTF8_VALIDATION
        { "{\"\xc0\":1}", "\xc0", YYJSON_READ_ERROR_INVALID_STRING },
        { "{\"\xc0\":0,\"a\":7}", "a", YYJSON_READ_ERROR_INVALID_STRING },
#endif
    };
    usize i, fused;
    for (i = 0; i < yy_nelems(cases); i++) {
        for (fused = 0; fused < 2; fused++) {
            each_mode(m) {
                rd_t r;
                yyjson_sr *sr = &r.sr;
                rd_start(&r, *m, cases[i].json);
                yy_assert(yyjson_sr_obj_begin(sr));
                if (fused) (void)yyjson_sr_obj_read_uint(sr, cases[i].key);
                else (void)yyjson_sr_obj_find(sr, cases[i].key);
                expect_err(sr, cases[i].code);
                rd_close(&r);
            }
        }
    }
}

// A member that is not wanted is skipped, whatever is in it.
static void test_sr_find_skip(void) {
    static const char *junk[] = {
        "[\"\\q\\z\",01,+5,tru,-,\"a\nb\"]",
        "{\"\\ud800\":lol,\"x\":9x9}",
        "[{]]", "{\"a\" 1 \"b\":2,}", "[[}}", "[1 2]", "[1,]", "{\"a\":1,}",
        "\"\\\\\"", "\"\\\"]\"", "[\"]\",\"[\",\"}\"]",
    };
    usize i;
    for (i = 0; i < yy_nelems(junk); i++) {
        char json[256];
        snprintf(json, sizeof(json), "{\"skip\":%s,\"good\":7}", junk[i]);
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            rd_start(&r, *m, json);
            yy_assert(yyjson_sr_obj_begin(sr));
            yy_assert(yyjson_sr_obj_find(sr, "good"));
            yy_assert(yyjson_sr_read_uint(sr) == 7);
            yy_assert(yyjson_sr_obj_end(sr));
            yy_assert(yyjson_sr_finish(sr));
            expect_ok(sr);
            rd_close(&r);
        }
    }
}

// A NUL byte in the input is an ordinary byte for skipping: not the end of
// input, a closer or an escape.
static void test_sr_skip_nul(void) {
    static const char c1[] = "[[1,\0,2],5]";
    static const char c2[] = "[{\"a\":[\0]},5]";
    static const char c3[] = "[[[\"x\"]\0],5]";
    static const char c4[] = "[\"\0\",5]";
    static const char c5[] = "[\"x\0y\",5]";
    static const char c6[] = "[[\"\0\"],5]";
    static const struct { const char *dat; usize len; } cases[] = {
        { c1, sizeof(c1) - 1 }, { c2, sizeof(c2) - 1 }, { c3, sizeof(c3) - 1 },
        { c4, sizeof(c4) - 1 }, { c5, sizeof(c5) - 1 }, { c6, sizeof(c6) - 1 },
    };
    usize i, explicit_skip;
    for (i = 0; i < yy_nelems(cases); i++) {
        for (explicit_skip = 0; explicit_skip < 2; explicit_skip++) {
            each_mode(m) {
                rd_t r;
                yyjson_sr *sr = &r.sr;
                yy_assert(rd_open(&r, *m, cases[i].dat, cases[i].len, NULL, NULL));
                yy_assert(yyjson_sr_arr_begin(sr));
                yy_assert(yyjson_sr_arr_next(sr));
                if (explicit_skip) yy_assert(yyjson_sr_skip_val(sr));
                yy_assert(yyjson_sr_arr_next(sr));
                yy_assert(yyjson_sr_read_uint(sr) == 5);
                yy_assert(yyjson_sr_arr_end(sr));
                yy_assert(yyjson_sr_finish(sr));
                expect_ok(sr);
                rd_close(&r);
            }
        }
    }
    // but a key is read, not skipped, and a NUL is invalid in it
    check_bad_n("{\"a\0:1,\"c\":2}", 13, 0, YYJSON_READ_ERROR_INVALID_STRING);
    check_bad_n("[1,\0]", 5, 0, 0);
    check_bad_n("1\0", 2, 0, 0);
    check_bad_n("\"a\0b\"", 5, 0, YYJSON_READ_ERROR_INVALID_STRING);
}

#undef SAMPLE



/*==============================================================================
 * MARK: - State
 *============================================================================*/

static void expect_no_val(yyjson_sr *sr) {
    yyjson_read_err err;
    expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    yyjson_sr_get_err(sr, &err);
    yy_assert(!strcmp(err.msg, "no current value, call next or find first"));
}

// Calls one of the functions that consume the current value.
static void consume(yyjson_sr *sr, usize which) {
    switch (which) {
    case 0: yyjson_sr_read_uint(sr); break;
    case 1: yyjson_sr_read_sint(sr); break;
    case 2: yyjson_sr_read_real(sr); break;
    case 3: yyjson_sr_read_str(sr); break;
    case 4: yyjson_sr_read_bool(sr); break;
    case 5: yyjson_sr_read_null(sr); break;
    case 6: yyjson_sr_read_raw(sr); break;
    case 7: yyjson_sr_read_num(sr); break;
    case 8: yyjson_sr_read_scalar(sr); break;
    case 9: yyjson_sr_skip_val(sr); break;
    case 10: yyjson_sr_arr_begin(sr); break;
    default: yyjson_sr_obj_begin(sr); break;
    }
}

// Exactly one call consumes the current value. After that, or where there is
// none yet, the consuming calls are errors.
static void test_sr_current(void) {
    usize which;
    for (which = 0; which < 12; which++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;

            // a value was read
            rd_start(&r, *m, "[1,2]");
            yy_assert(yyjson_sr_arr_begin(sr));
            consume(sr, which);
            expect_no_val(sr);
            rd_close(&r);

            // the root was consumed
            rd_start(&r, *m, "[1,2]");
            yy_assert(yyjson_sr_skip_val(sr));
            consume(sr, which);
            expect_no_val(sr);
            rd_close(&r);

            // a member value was consumed
            rd_start(&r, *m, "{\"a\":1}");
            yy_assert(yyjson_sr_obj_begin(sr));
            yy_assert(yyjson_sr_obj_find(sr, "a"));
            yy_assert(yyjson_sr_read_uint(sr) == 1);
            consume(sr, which);
            expect_no_val(sr);
            rd_close(&r);
        }
    }
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        // consumed twice
        rd_start(&r, *m, "1");
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_read_uint(sr) == 0);
        expect_no_val(sr);
        rd_close(&r);
        // a closed container leaves no current value
        rd_start(&r, *m, "[[1],2]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(!yyjson_sr_skip_val(sr));
        expect_no_val(sr);
        rd_close(&r);
    }
}

// Nothing to read: the end of the input is an error, and so is a closer or a
// separator, they are not a value.
static void test_sr_empty(void) {
    static const char *ends[] = { "", "  \n " };
    static const char *delims[] = { "]", ",", "}" };
    usize i, which;
    for (which = 0; which < 12; which++) {
        each_mode(m) {
            rd_t r;
            for (i = 0; i < yy_nelems(ends); i++) {
                rd_start(&r, *m, ends[i]);
                consume(&r.sr, which);
                expect_err(&r.sr, YYJSON_READ_ERROR_UNEXPECTED_END);
                rd_close(&r);
            }
            for (i = 0; i < yy_nelems(delims); i++) {
                rd_start(&r, *m, delims[i]);
                consume(&r.sr, which);
                // a literal is told by its first letter
                expect_err(&r.sr, which == 4 || which == 5 ?
                           YYJSON_READ_ERROR_LITERAL :
                           YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
                rd_close(&r);
            }
        }
    }
}

// A call that does not fit where the reader is, is an error.
static void test_sr_misuse(void) {
    usize which;
    for (which = 0; which < 6; which++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            yyjson_sv key;
            // the first four are outside of a container, the others are in
            // the other kind of container
            rd_start(&r, *m, which < 4 ? "1" : which == 4 ? "{\"a\":1}" : "[1]");
            if (which == 4) yy_assert(yyjson_sr_obj_begin(sr));
            if (which == 5) yy_assert(yyjson_sr_arr_begin(sr));
            switch (which) {
            case 0: case 4: yy_assert(!yyjson_sr_arr_next(sr)); break;
            case 1: case 5: yy_assert(!yyjson_sr_obj_next(sr, &key)); break;
            case 2: yy_assert(!yyjson_sr_arr_end(sr)); break;
            default: yy_assert(!yyjson_sr_obj_end(sr)); break;
            }
            expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
            rd_close(&r);
        }
    }
}

// An unconsumed value is skipped when the reader moves on.
static void test_sr_move_on(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv key;

        // arr_next
        rd_start(&r, *m, "[11,[22,{\"x\":[]}],\"s\",33]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 33);
        yy_assert(!yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // obj_next, obj_find, and the required-member helpers
        rd_start(&r, *m, "{\"a\":\"x\",\"b\":{\"c\":1},\"d\":2,\"e\":3,\"f\":[4]}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sv_equals_str(key, "b"));
        yy_assert(yyjson_sr_obj_find(sr, "d"));
        yy_assert(yyjson_sr_obj_read_uint(sr, "e") == 3);
        yy_assert(yyjson_sr_obj_find(sr, "f"));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // arr_end and obj_end skip the rest, with the current value
        rd_start(&r, *m, "[1,[2,{\"a\":[3]}],4]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
        rd_start(&r, *m, "{\"a\":[1,2],\"b\":{\"c\":[]}}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
        rd_start(&r, *m, "[[1,[2]],[3]]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // finish and doc_next skip an unconsumed root
        rd_start(&r, *m, "[1,[2]]");
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
        rd_start(&r, *m, "[1] {\"a\":2} 3");
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(!yyjson_sr_doc_next(sr));
        expect_ok(sr);
        rd_close(&r);
    }
}

// The offset is where the next byte to read is: at the start of a value
// after next(), and after a value once it is read.
static void test_sr_pos(void) {
    enum { count = 40, stride = 16 };
    char json[count * stride + 8];
    usize len = 0, i;

    json[len++] = '[';
    for (i = 0; i < count; i++) {
        memcpy(json + len, "\"cccccccccccc\"", 14);
        len += 14;
        if (i + 1 < count) {
            memcpy(json + len, ", ", 2);
            len += 2;
        }
    }
    json[len++] = ']';
    json[len] = '\0';

    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yy_assert(rd_open(&r, *m, json, len, NULL, NULL));
        yy_assert(yyjson_sr_pos(sr) == 0);
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_pos(sr) == 1);
        for (i = 0; i < count; i++) {
            yy_assert(yyjson_sr_arr_next(sr));
            yy_assert(yyjson_sr_pos(sr) == 1 + i * stride);
            yy_assert(yyjson_sr_read_str(sr).len == 12);
            yy_assert(yyjson_sr_pos(sr) == 1 + i * stride + 14);
        }
        yy_assert(!yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        yy_assert(yyjson_sr_pos(sr) == len);
        rd_close(&r);
    }

    // an error is at the offset of the bad byte, for all kinds of input
    i = (count - 1) * stride + 1;
    json[i] = '#';
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_read_err err;
        yy_assert(rd_open(&r, *m, json, len, NULL, NULL));
        yy_assert(yyjson_sr_arr_begin(sr));
        while (yyjson_sr_arr_next(sr)) {
            if (!yyjson_sr_read_str(sr).ptr) break;
        }
        yy_assert(yyjson_sr_get_err(sr, &err) != YYJSON_READ_SUCCESS);
        yy_assert(err.pos == i);
        yy_assert(yyjson_sr_pos(sr) == i);
        rd_close(&r);
    }
}

static void test_sr_depth(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv key;

        // the depth is the number of open containers, a current value that is
        // not entered yet does not count
        rd_start(&r, *m, "{\"a\":[1,[2]],\"b\":{}}");
        yy_assert(yyjson_sr_depth(sr) == 0);
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_depth(sr) == 1);
        yy_assert(yyjson_sr_obj_find(sr, "a"));
        yy_assert(yyjson_sr_depth(sr) == 1);
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_depth(sr) == 2);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_depth(sr) == 2);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_depth(sr) == 3);
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_depth(sr) == 2);
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_depth(sr) == 1);
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_depth(sr) == 2);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_depth(sr) == 1);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_depth(sr) == 0);
        yy_assert(yyjson_sr_finish(sr));
        yy_assert(yyjson_sr_depth(sr) == 0);
        rd_close(&r);

        // container reads that fail do not change it
        rd_start(&r, *m, "[[1]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(!yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_depth(sr) == 1);
        rd_close(&r);
    }
}

// Several documents in one input: NDJSON, or concatenated.
static void test_sr_docs(void) {
    static const struct { const char *json, *dumps; } cases[] = {
        { "1\n2\n3\n", "1|2|3" },
        { "{\"a\":1}\n{\"a\":2}\n", "{\"a\":1}|{\"a\":2}" },
        { "[1][2]\"s\"true null\t7", "[1]|[2]|\"s\"|true|null|7" },
        { "  [ ]  {}  ", "[]|{}" },
        { "42", "42" },
        { "", "" },
        { " \r\n\t ", "" },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            sink_t out = {0};
            yy_assert(rd_open(&r, *m, cases[i].json, strlen(cases[i].json), NULL, NULL));
            while (yyjson_sr_doc_next(sr)) {
                if (out.len) sink_puts(&out, "|");
                yy_assert(dump_val(sr, &out));
            }
            expect_ok(sr);
            yy_assertf(!strcmp(out.dat ? out.dat : "", cases[i].dumps),
                       "got %s, expect %s", out.dat, cases[i].dumps);
            yy_assert(!yyjson_sr_doc_next(sr));
            rd_close(&r);
            sink_free(&out);
        }
    }
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;

        // doc_next is optional for the first root, skips the unread, and
        // repeats at the end
        rd_start(&r, *m, "[1,2] [3] 4");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 4);
        yy_assert(!yyjson_sr_doc_next(sr));
        yy_assert(!yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // with one document, finish says what is wrong with the rest
        rd_start(&r, *m, "[1] 2");
        yy_assert(!yyjson_sr_finish(sr));
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
        rd_close(&r);
        rd_start(&r, *m, "  ");
        yy_assert(!yyjson_sr_finish(sr));
        expect_err(sr, YYJSON_READ_ERROR_EMPTY_CONTENT);
        rd_close(&r);

        // inside a container, there is no next document
        rd_start(&r, *m, "[1] [2]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(!yyjson_sr_doc_next(sr));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);

        // the next document may be broken, it shows when it is read
        rd_start(&r, *m, "1 x 3");
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_doc_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 0);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);
    }
}

// A lone surrogate is reported like the DOM reader does, also when the
// string or the input ends where a low surrogate could follow.
static void test_sr_surrogate(void) {
    static const struct { const char *json, *msg; } cases[] = {
        { "\"\\ud800\"", "no low surrogate in string" },
        { "\"\\ud800x\"", "no low surrogate in string" },
        { "\"\\udc00\"", "invalid high surrogate in string" },
        { "\"\\ud800\\u0041\"", "invalid low surrogate in string" },
        { "\"a\\q\"", "invalid escape sequence in string" },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            yyjson_read_err err;
            rd_start(&r, *m, cases[i].json);
            yy_assert(!yyjson_sr_read_str(sr).ptr);
            yy_assert(yyjson_sr_get_err(sr, &err) ==
                      YYJSON_READ_ERROR_INVALID_STRING);
            yy_assertf(!strcmp(err.msg, cases[i].msg), "%s", err.msg);
            rd_close(&r);
        }
    }
}

// What skip accepts is not validated. Only the strings and the nesting count.
static void test_sr_skip(void) {
    static const char *relaxed[] = {
        "[[}}", "[1 2]", "[1,]", "{\"a\" 1}", "{\"a\":1 \"b\":2}", "{\"a\":1,}",
        "{\"a\":[1,2}}", "[01,+5,tru,-]", "[\"a\nb\"]",
        "[1,\"s\",true,null,{},[],{\"a\":[1,{\"b\":\"}]\\\"\"}]},[[[[1],2],3],4]]",
    };
    usize i;
    for (i = 0; i < yy_nelems(relaxed); i++) {
        each_mode(m) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            rd_start(&r, *m, relaxed[i]);
            yy_assert(yyjson_sr_skip_val(sr));
            yy_assert(yyjson_sr_finish(sr));
            expect_ok(sr);
            rd_close(&r);
        }
    }
    // the strings end only at an unescaped quote
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        rd_start(&r, *m, "[\"\\\\\",\"\\\"]\",\"]\",\"[\",\"}\",\"{\",\"\\u005d\",1]");
        yy_assert(yyjson_sr_arr_begin(sr));
        for (i = 0; i < 7; i++) {
            yy_assert(yyjson_sr_arr_next(sr));
            yy_assert(yyjson_sr_skip_val(sr));
        }
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_arr_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
    // but the end of the input, and the extra closer, are caught
    check_bad("[[]", YYJSON_READ_ERROR_UNEXPECTED_END);
    check_bad("[]]", YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    check_bad("{\"a\":[1,2", YYJSON_READ_ERROR_UNEXPECTED_END);
    check_bad("[\"abc", YYJSON_READ_ERROR_UNEXPECTED_END);
    // a wrong closer is an error where the closer is read
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        rd_start(&r, *m, "[}");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(!yyjson_sr_obj_end(sr));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        // and where the rest of the container is skipped
        rd_start(&r, *m, "[1,2}");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(!yyjson_sr_arr_end(sr));
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);
        rd_start(&r, *m, "{\"a\":[1],\"b\":2]");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_end(sr));
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);
        rd_start(&r, *m, "[1,}");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_peek_type(sr) == YYJSON_TYPE_NONE);
        yy_assert(!yyjson_sr_skip_val(sr));
        yy_assert(!yyjson_sr_finish(sr));
        rd_close(&r);
    }
}

// Skipping does not use the depth limit or memory: a very deep value is
// skipped on a small pool.
static void test_sr_skip_deep(void) {
    enum { depth = 100000 };
    char *json = (char *)malloc(depth * 2 + 8);
    usize len = depth * 2;
    yy_assert(json != NULL);
    memset(json, '[', depth);
    memset(json + depth, ']', depth);
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yy_assert(rd_open(&r, *m, json, len, &(yyjson_sr_opts){ 0, 1, 0, 0 }, NULL));
        yy_assert(yyjson_sr_skip_val(sr));
        yy_assert(yyjson_sr_finish(sr));
        yy_assert(yyjson_sr_pos(sr) == len);
        rd_close(&r);
    }
    free(json);
}



/*==============================================================================
 * MARK: - Error
 *============================================================================*/

static void test_sr_errors(void) {
    const struct { const char *json; yyjson_read_code code; } cases[] = {
        // the input ends too early
        { "", YYJSON_READ_ERROR_UNEXPECTED_END }, { "   ", YYJSON_READ_ERROR_UNEXPECTED_END }, { "[", YYJSON_READ_ERROR_UNEXPECTED_END }, { "{", YYJSON_READ_ERROR_UNEXPECTED_END },
        { "[1", YYJSON_READ_ERROR_UNEXPECTED_END }, { "[1,", YYJSON_READ_ERROR_UNEXPECTED_END }, { "[1,2", YYJSON_READ_ERROR_UNEXPECTED_END }, { "[[1]", YYJSON_READ_ERROR_UNEXPECTED_END },
        { "{\"a\"", YYJSON_READ_ERROR_UNEXPECTED_END }, { "{\"a\":", YYJSON_READ_ERROR_UNEXPECTED_END }, { "{\"a\":1", YYJSON_READ_ERROR_UNEXPECTED_END },
        { "{\"a\":1,", YYJSON_READ_ERROR_UNEXPECTED_END }, { "{\"a\":[1,{\"b\":2}", YYJSON_READ_ERROR_UNEXPECTED_END },
        // not a value, a separator, or a closer
        { "x", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { ",", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { ":", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "'a'", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[,1]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[1,]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[1,,2]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "[1 2]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[1}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{\"a\":1]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{,}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{1:2}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{a:1}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{'a':1}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\"}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{\"a\" 1}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{\"a\":}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":1,}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{\"a\":1,,}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "{\"a\":1 \"b\":2}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "{\"a\":[}", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "[+1]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[.5]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[NaN]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "[Infinity]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        { "[/*c*/1]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER }, { "\xef\xbb\xbf[]", YYJSON_READ_ERROR_UNEXPECTED_CHARACTER },
        // numbers
        { "[01]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[-01]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[00]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[1.]", YYJSON_READ_ERROR_INVALID_NUMBER },
        { "[-]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[--1]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[1e]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[1e+]", YYJSON_READ_ERROR_INVALID_NUMBER },
        { "[0x10]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[1//c\n]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[1.5.5]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[123abc]", YYJSON_READ_ERROR_INVALID_NUMBER },
        { "[1e999]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[-1e999]", YYJSON_READ_ERROR_INVALID_NUMBER }, { "[-Infinity]", YYJSON_READ_ERROR_INVALID_NUMBER },
        { "-", YYJSON_READ_ERROR_INVALID_NUMBER }, { "1.", YYJSON_READ_ERROR_INVALID_NUMBER }, { "123abc", YYJSON_READ_ERROR_INVALID_NUMBER },
        // literals
        { "[tru]", YYJSON_READ_ERROR_LITERAL }, { "[fals]", YYJSON_READ_ERROR_LITERAL }, { "[nul]", YYJSON_READ_ERROR_LITERAL }, { "[nan]", YYJSON_READ_ERROR_LITERAL },
        { "[truex]", YYJSON_READ_ERROR_LITERAL }, { "[falsee]", YYJSON_READ_ERROR_LITERAL }, { "[nulll]", YYJSON_READ_ERROR_LITERAL },
        { "[tRue]", YYJSON_READ_ERROR_LITERAL }, { "tru", YYJSON_READ_ERROR_LITERAL }, { "nul", YYJSON_READ_ERROR_LITERAL },
        // after the document
        { "1 2", YYJSON_READ_ERROR_UNEXPECTED_CONTENT }, { "\"a\" 1", YYJSON_READ_ERROR_UNEXPECTED_CONTENT }, { "[] x", YYJSON_READ_ERROR_UNEXPECTED_CONTENT }, { "{}{", YYJSON_READ_ERROR_UNEXPECTED_CONTENT },
        { "[1]]", YYJSON_READ_ERROR_UNEXPECTED_CONTENT }, { "[1],", YYJSON_READ_ERROR_UNEXPECTED_CONTENT }, { "null null", YYJSON_READ_ERROR_UNEXPECTED_CONTENT },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        check_bad(cases[i].json, cases[i].code);
        check_bad_flg(cases[i].json, SR_IGNORED_FLAGS, cases[i].code);
    }
}

// After the first error, every call fails, and the error does not change.
static void test_sr_sticky(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_read_err err1, err2;
        yyjson_sv key;
        u64 pos;

        rd_start(&r, *m, "{\"k\":[1,x]}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 0);
        yy_assert(yyjson_sr_get_err(sr, &err1) ==
                  YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        pos = yyjson_sr_pos(sr);
        yy_assert(pos == err1.pos && pos == 8);
        expect_dead(sr);
        yy_assert(yyjson_sr_get_err(sr, &err2) == err1.code);
        yy_assert(err2.msg == err1.msg && err2.pos == err1.pos);
        yy_assert(yyjson_sr_pos(sr) == pos);
        yy_assert(yyjson_sr_has_err(sr));
        rd_close(&r);

        // status queries do not invalidate a view
        rd_start(&r, *m, "{\"key\":\"value\"}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_next(sr, &key));
        (void)yyjson_sr_has_err(sr);
        (void)yyjson_sr_get_err(sr, &err1);
        (void)yyjson_sr_pos(sr);
        (void)yyjson_sr_depth(sr);
        (void)yyjson_sr_can_rewind(sr);
        yy_assert(yyjson_sv_equals_str(key, "key"));
        rd_close(&r);

        // an unclosed container cannot be finished, or go to a next document
        rd_start(&r, *m, "[1]");
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(!yyjson_sr_finish(sr));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
    }
}

// The input source for the I/O errors: gives `cut` bytes in steps of 5, then
// EOF, or a read error.
typedef struct {
    const char *dat;
    usize cut, pos;
    bool fail;
} io_t;

static usize io_read(void *ctx, void *dst, usize cap) {
    io_t *io = (io_t *)ctx;
    usize n = io->cut - io->pos;
    if (!n) return io->fail ? (usize)-1 : 0;
    if (n > 5) n = 5;
    if (n > cap) n = cap;
    memcpy(dst, io->dat + io->pos, n);
    io->pos += n;
    return n;
}

static void test_sr_io(void) {
    static const char json[] = "{\"a\":[1,\"two\",{\"b\":null}],\"c\":true}";
    static const char dump[] = "{\"a\":[1,\"two\",{\"b\":null}],\"c\":true}";
    usize len = sizeof(json) - 1, cut, fail;
    for (cut = 0; cut <= len; cut++) {
        for (fail = 0; fail < 2; fail++) {
            yyjson_sr sr = {0};
            sink_t out = {0};
            yyjson_read_code code;
            io_t io = { json, cut, 0, fail != 0 };
            yy_assert(yyjson_sr_init_cb(&sr, io_read, NULL, &io, NULL, NULL));
            if (dump_doc(&sr, &out)) {
                yy_assert(cut == len && !strcmp(out.dat, dump));
                expect_ok(&sr);
            } else {
                code = yyjson_sr_get_err(&sr, NULL);
                if (fail) {
                    yy_assert(code == YYJSON_READ_ERROR_STREAMING_IO);
                } else {
                    // the end is in a string, or a literal that is cut short
                    yy_assert(code == YYJSON_READ_ERROR_UNEXPECTED_END ||
                              code == YYJSON_READ_ERROR_LITERAL);
                    yy_assert(cut < len);
                }
                expect_dead(&sr);
            }
            yyjson_sr_free(&sr);
            sink_free(&out);
        }
    }
}



/*==============================================================================
 * MARK: - Limit
 *============================================================================*/

// Enters `depth` arrays, one in the other, then leaves them.
static bool nest_arr(yyjson_sr *sr, usize depth) {
    usize i;
    for (i = 0; i < depth; i++) {
        if (i && !yyjson_sr_arr_next(sr)) return false;
        if (!yyjson_sr_arr_begin(sr)) return false;
    }
    for (i = 0; i < depth; i++) {
        if (!yyjson_sr_arr_end(sr)) return false;
    }
    return true;
}

// The same with objects, `{"a":{"a":...}}`.
static bool nest_obj(yyjson_sr *sr, usize depth) {
    yyjson_sv key;
    usize i;
    for (i = 0; i < depth; i++) {
        if (i && !yyjson_sr_obj_next(sr, &key)) return false;
        if (!yyjson_sr_obj_begin(sr)) return false;
    }
    for (i = 0; i < depth; i++) {
        if (!yyjson_sr_obj_end(sr)) return false;
    }
    return true;
}

// Makes `depth` nested arrays, or objects `{"a":{"a":..{}}}` with an empty
// innermost one.
static usize nest_make(char *buf, bool obj, usize depth) {
    usize len = 0, i;
    for (i = 0; i < depth; i++) {
        if (obj && i + 1 < depth) {
            memcpy(buf + len, "{\"a\":", 5);
            len += 5;
        } else {
            buf[len++] = obj ? '{' : '[';
        }
    }
    for (i = 0; i < depth; i++) buf[len++] = obj ? '}' : ']';
    return len;
}

static void test_sr_max_depth(void) {
    usize obj, lim, depth;
    for (obj = 0; obj < 2; obj++) {
        for (lim = 1; lim <= 4; lim++) {
            for (depth = 1; depth <= 6; depth++) {
                char json[64];
                usize len = nest_make(json, obj != 0, depth);
                each_mode(m) {
                    rd_t r;
                    yyjson_sr *sr = &r.sr;
                    bool ok;
                    yy_assert(rd_open(&r, *m, json, len,
                              &(yyjson_sr_opts){ 0, lim, 0, 0 }, NULL));
                    ok = obj ? nest_obj(sr, depth) : nest_arr(sr, depth);
                    if (depth <= lim) {
                        yy_assert(ok && yyjson_sr_finish(sr));
                        expect_ok(sr);
                    } else {
                        yy_assert(!ok);
                        expect_err(sr, YYJSON_READ_ERROR_DEPTH);
                        yy_assert(yyjson_sr_depth(sr) == lim);
                    }
                    rd_close(&r);
                }
            }
        }
    }
}

static void test_sr_options(void) {
    yyjson_sr sr;
    yyjson_sr_opts opts = {0};
    char json[] = "[0]";
    yy_assert(yyjson_sr_init_mem(&sr, json, sizeof(json) - 1, &opts, NULL));
    yy_assert(sr.max_val_len == SIZE_MAX && sr.win_size == 16384);
    yy_assert(sr.max_depth == (YYJSON_READER_DEPTH_LIMIT ?
                               YYJSON_READER_DEPTH_LIMIT : SIZE_MAX));
    yy_assert(yyjson_sr_arr_begin(&sr) && yyjson_sr_arr_end(&sr));
    yy_assert(yyjson_sr_finish(&sr));
    yyjson_sr_free(&sr);

    opts.max_depth = SIZE_MAX;
    opts.max_val_len = SIZE_MAX;
    yy_assert(yyjson_sr_init_mem(&sr, json, sizeof(json) - 1, &opts, NULL));
    yy_assert(sr.max_depth == (YYJSON_READER_DEPTH_LIMIT ?
                               YYJSON_READER_DEPTH_LIMIT : SIZE_MAX));
    yyjson_sr_free(&sr);
#if YYJSON_READER_DEPTH_LIMIT
    opts.max_depth = (usize)YYJSON_READER_DEPTH_LIMIT + 1;
    yy_assert(yyjson_sr_init_mem(&sr, json, sizeof(json) - 1, &opts, NULL));
    yy_assert(sr.max_depth == (usize)YYJSON_READER_DEPTH_LIMIT);
    yyjson_sr_free(&sr);
#endif
    opts.max_depth = 0;
    opts.win_size = SIZE_MAX;
    yy_assert(!yyjson_sr_init_mem(&sr, json, sizeof(json) - 1, &opts, NULL));
    expect_err(&sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    yyjson_sr_free(&sr);

#if !YYJSON_READER_DEPTH_LIMIT || YYJSON_READER_DEPTH_LIMIT >= 5000
    // The default and SIZE_MAX allow deep input, as far as memory goes.
    {
        enum { depth = 5000 };
        char *deep = (char *)malloc(depth * 2);
        usize i;
        yy_assert(deep != NULL);
        yy_assert(nest_make(deep, false, depth) == depth * 2);
        for (i = 0; i < 2; i++) {
            each_mode(m) {
                rd_t r;
                opts = (yyjson_sr_opts){ 0, i ? SIZE_MAX : 0, 0, 0 };
                yy_assert(rd_open(&r, *m, deep, depth * 2, &opts, NULL));
                yy_assert(nest_arr(&r.sr, depth));
                yy_assert(yyjson_sr_finish(&r.sr));
                rd_close(&r);
            }
        }
        free(deep);
    }
#endif
}

// A value read, or looked at by a search, must not be longer than the limit.
typedef struct {
    const char *json;
    usize len; // limit that just fits
    int kind;
} len_case;

static bool len_read(yyjson_sr *sr, int kind) {
    switch (kind) {
    case 0: return yyjson_sr_read_str(sr).ptr != NULL;
    case 1: yyjson_sr_read_uint(sr); break;
    case 2: yyjson_sr_read_sint(sr); break;
    case 3: yyjson_sr_read_real(sr); break;
    case 4: yyjson_sr_read_num(sr); break;
    case 5: yyjson_sr_read_bool(sr); break;
    case 6: yyjson_sr_read_null(sr); break;
    case 7: return yyjson_sr_read_raw(sr).ptr != NULL;
    default: yyjson_sr_read_scalar(sr); break;
    }
    return !yyjson_sr_has_err(sr);
}

static void test_sr_max_val_len(void) {
    static const len_case cases[] = {
        { "\"abcde\"", 5, 0 },
        { "\"a\\u0000\\uD83D\\uDE00\"", 6, 0 },     // decoded size
        { "\"\\u0061b\"", 2, 0 },
        { "12345", 5, 1 },
        { "-12345", 6, 2 },
        { "0.50", 4, 3 },
        { "12345", 5, 4 },
        { "true", 4, 5 },
        { "false", 5, 5 },
        { "null", 4, 6 },
        { "\"\\u0061\"", 8, 7 },                    // source size
        { "[1, [2]]", 8, 7 },
        { "\"abcde\"", 5, 8 },
        { "12345", 5, 8 },
    };
    usize i, delta, lead;
    for (i = 0; i < yy_nelems(cases); i++) {
        for (delta = 0; delta < 3; delta++) {
            for (lead = 0; lead < 2; lead++) {
                char json[64];
                snprintf(json, sizeof(json), "%s%s ", lead ? "  \n " : "",
                         cases[i].json);
                each_mode(m) {
                    rd_t r;
                    yyjson_sr *sr = &r.sr;
                    // one less than it needs, just enough, and enough
                    usize lim = cases[i].len + delta - 1;
                    rd_start_opts(&r, *m, json, &(yyjson_sr_opts){ 0, 0, lim, 0 });
                    if (delta == 0) {
                        yy_assert(!len_read(sr, cases[i].kind));
                        expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
                    } else {
                        yy_assert(len_read(sr, cases[i].kind));
                        yy_assert(yyjson_sr_finish(sr));
                    }
                    rd_close(&r);
                }
            }
        }
    }
}

// Keys count too, the ones a search passes included. A value it skips does not.
static void test_sr_max_val_len_obj(void) {
    each_mode(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv key;
        usize lim;

        // numbers and keys that a required-member read passes
        for (lim = 3; lim <= 5; lim++) {
            rd_start_opts(&r, *m, "{\"abcd\":1,\"a\":12345}",
                      &(yyjson_sr_opts){ 0, 0, lim, 0 });
            yy_assert(yyjson_sr_obj_begin(sr));
            if (lim == 5) {
                yy_assert(yyjson_sr_obj_read_uint(sr, "a") == 12345);
                yy_assert(yyjson_sr_obj_end(sr));
            } else {
                yy_assert(!yyjson_sr_obj_read_uint(sr, "a"));
                expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
            }
            rd_close(&r);
        }

        // a long key, passed by a search, or read
        rd_start_opts(&r, *m, "{\"longkey\":0,\"b\":1}",
                  &(yyjson_sr_opts){ 0, 0, 3, 0 });
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_find(sr, "b"));
        expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
        rd_close(&r);
        for (lim = 6; lim <= 7; lim++) {
            rd_start_opts(&r, *m, "{\"longkey\":0,\"b\":1}",
                      &(yyjson_sr_opts){ 0, 0, lim, 0 });
            yy_assert(yyjson_sr_obj_begin(sr));
            if (lim == 7) {
                expect_key(sr, "longkey");
                yy_assert(yyjson_sr_read_uint(sr) == 0);
                expect_key(sr, "b");
            } else {
                yy_assert(!yyjson_sr_obj_next(sr, &key));
                expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
            }
            rd_close(&r);
        }

        // a decoded key, and a skipped value of any size or depth
        for (lim = 4; lim <= 5; lim++) {
            rd_start_opts(&r, *m, "{\"a\":[[[\"oversized\"]]],\"\\u0062\":12345}",
                      &(yyjson_sr_opts){ 0, 1, lim, 0 });
            yy_assert(yyjson_sr_obj_begin(sr));
            if (lim == 5) {
                yy_assert(yyjson_sr_obj_read_uint(sr, "b") == 12345);
                yy_assert(yyjson_sr_obj_end(sr) && yyjson_sr_finish(sr));
            } else {
                yy_assert(!yyjson_sr_obj_read_uint(sr, "b"));
                expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
            }
            rd_close(&r);
        }
    }
}



// A value is cut at the limit also when it is much longer than the window.
static void test_sr_max_val_len_long(void) {
    enum { n = 4096 };
    char json[n * 6 + 16];
    usize kind, i;
    for (kind = 0; kind < 3; kind++) {
        usize len = 0, vlen = n, bound;
        if (kind != 2) {
            json[len++] = '"';
            for (i = 0; i < n; i++) {
                memcpy(json + len, "\\u0061", 6);
                len += 6;
            }
            json[len++] = '"';
            if (kind == 1) vlen = len; // raw is counted in source bytes
        } else {
            json[len++] = '0';
            json[len++] = '.';
            memset(json + len, '1', n - 2);
            len = n;
        }
        json[len++] = ' ';
        for (bound = 0; bound < 2; bound++) {
            each_mode(m) {
                rd_t r;
                yyjson_sr *sr = &r.sr;
                yy_assert(rd_open(&r, *m, json, len,
                          &(yyjson_sr_opts){ 0, 0, vlen - 1 + bound, 0 }, NULL));
                if (kind == 0) yyjson_sr_read_str(sr);
                else if (kind == 1) yyjson_sr_read_raw(sr);
                else yyjson_sr_read_num(sr);
                if (bound) {
                    yy_assert(yyjson_sr_finish(sr));
                } else {
                    expect_err(sr, YYJSON_READ_ERROR_VALUE_LENGTH);
                }
                rd_close(&r);
            }
        }
    }
}

// The input source that records the largest read it was asked for.
typedef struct {
    const char *dat;
    usize len, pos, max_cap;
} win_t;

static usize win_read(void *ctx, void *dst, usize cap) {
    win_t *w = (win_t *)ctx;
    usize n = w->len - w->pos;
    if (cap > w->max_cap) w->max_cap = cap;
    if (n > cap) n = cap;
    memcpy(dst, w->dat + w->pos, n);
    w->pos += n;
    return n;
}

// Reads an array of count numbers 0..count-1, and checks their sum.
static void win_check(yyjson_sr *sr, usize count) {
    u64 sum = 0, i;
    yy_assert(yyjson_sr_arr_begin(sr));
    while (yyjson_sr_arr_next(sr)) sum += yyjson_sr_read_uint(sr);
    yy_assert(yyjson_sr_arr_end(sr));
    yy_assert(yyjson_sr_finish(sr));
    for (i = 0; i < count; i++) sum -= i;
    yy_assert(sum == 0);
}

static void test_sr_win_size(void) {
    static const usize wins[] = { 1, 7, 100, 4096, 70000, 300000 };
    enum { count = 30000 };
    char *json = (char *)malloc(count * 8 + YYJSON_PADDING_SIZE);
    char *copy, pool[65536];
    usize len = 0, i, w;
    yyjson_alc alc;
    yyjson_sr sr = {0};
    win_t feed;

    yy_assert(json != NULL);
    json[len++] = '[';
    for (i = 0; i < count; i++) {
        len += (usize)snprintf(json + len, 16, i ? ",%u" : "%u", (unsigned)i);
    }
    json[len++] = ']';
    memset(json + len, 0, YYJSON_PADDING_SIZE);

    for (w = 0; w < yy_nelems(wins); w++) {
        // a callback is never asked for more, and is asked for the whole
        // window when the buffer can grow
        feed = (win_t){ json, len, 0, 0 };
        yy_assert(yyjson_sr_init_cb(&sr, win_read, NULL, &feed,
                  &(yyjson_sr_opts){ 0, 0, 0, wins[w] }, NULL));
        win_check(&sr, count);
        yy_assert(feed.max_cap == wins[w]);
        yyjson_sr_free(&sr);

        // memory input is copied in the same steps
        yy_assert(yyjson_sr_init_mem(&sr, json, len,
                  &(yyjson_sr_opts){ 0, 0, 0, wins[w] }, NULL));
        win_check(&sr, count);
        yyjson_sr_free(&sr);

        // a pool does not grow, so the window is only an upper bound
        feed = (win_t){ json, len, 0, 0 };
        yy_assert(yyjson_alc_pool_init(&alc, pool, sizeof(pool)));
        yy_assert(yyjson_sr_init_cb(&sr, win_read, NULL, &feed,
                  &(yyjson_sr_opts){ 0, 0, 0, wins[w] }, &alc));
        win_check(&sr, count);
        yy_assert(feed.max_cap <= wins[w] && feed.max_cap <= sizeof(pool));
        yyjson_sr_free(&sr);
    }

    // a value longer than the window is read in several steps
    feed = (win_t){ "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"", 42, 0, 0 };
    yy_assert(yyjson_sr_init_cb(&sr, win_read, NULL, &feed,
              &(yyjson_sr_opts){ 0, 0, 0, 7 }, NULL));
    expect_strn(&sr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 40);
    yy_assert(feed.max_cap == 7);
    yy_assert(yyjson_sr_finish(&sr));
    yyjson_sr_free(&sr);

    // in-situ input has no reads, the window is accepted and ignored
    copy = dup_pad(json, len, YYJSON_PADDING_SIZE);
    yy_assert(yyjson_sr_init_mem(&sr, copy, len,
              &(yyjson_sr_opts){ YYJSON_READ_INSITU, 0, 0, 5 }, NULL));
    win_check(&sr, count);
    yyjson_sr_free(&sr);
    free(copy);

    // The requested window is allocated once at init.
    {
        test_alc ctx;
        yyjson_sr_opts opts = {0};
        opts.win_size = (usize)1 << 20;
        test_alc_init(&alc, &ctx, NULL, 1, SIZE_MAX);
        feed = (win_t){ json, len, 0, 0 };
        yy_assert(yyjson_sr_init_cb(&sr, win_read, NULL, &feed, &opts, &alc));
        yy_assert(ctx.calls == 1);
        win_check(&sr, count);
        yyjson_sr_free(&sr);
        test_alc_init(&alc, &ctx, NULL, 1, 1024);
        yy_assert(!yyjson_sr_init_cb(&sr, win_read, NULL, &feed, &opts, &alc));
        expect_err(&sr, YYJSON_READ_ERROR_MEMORY_ALLOCATION);
        yyjson_sr_free(&sr);
    }
    free(json);
}



/*==============================================================================
 * MARK: - Pool
 *============================================================================*/

// What a walk over a document sees, to compare one read with another.
typedef struct {
    u64 hash;       // of every key, string, and scalar
    usize count;    // values and keys
    usize max_len;  // longest string, key, or raw scalar
    usize max_depth;
} walk_t;

static void walk_mix(walk_t *w, const void *src, usize len) {
    const u8 *p = (const u8 *)src;
    while (len--) w->hash = (w->hash ^ *p++) * UINT64_C(1099511628211);
    w->hash = (w->hash ^ 0xFF) * UINT64_C(1099511628211);
}

static void walk_sv(walk_t *w, yyjson_sv sv) {
    walk_mix(w, sv.ptr, sv.len);
    if (sv.len > w->max_len) w->max_len = sv.len;
    w->count++;
}

// Reads a whole document with no recursion, `raw` reads the scalars as raw.
// Returns false on a read error, `w` is still valid for what was read.
static bool walk_doc(yyjson_sr *sr, walk_t *w, bool raw) {
    u8 *stack = NULL;
    usize depth = 0, cap = 0;
    bool ok = false;
    yyjson_sv sv;

    while (true) {
        // a value
        switch (yyjson_sr_peek_type(sr)) {
        case YYJSON_TYPE_ARR:
        case YYJSON_TYPE_OBJ: {
            bool obj = yyjson_sr_peek_type(sr) == YYJSON_TYPE_OBJ;
            if (!(obj ? yyjson_sr_obj_begin(sr) : yyjson_sr_arr_begin(sr))) {
                goto done;
            }
            if (depth == cap) {
                cap = cap ? cap * 2 : 64;
                stack = (u8 *)realloc(stack, cap);
                yy_assert(stack != NULL);
            }
            stack[depth++] = obj;
            if (depth > w->max_depth) w->max_depth = depth;
            w->count++;
            break;
        }
        case YYJSON_TYPE_STR:
            sv = yyjson_sr_read_str(sr);
            if (!sv.ptr) goto done;
            walk_sv(w, sv);
            break;
        default:
            if (raw) {
                sv = yyjson_sr_read_raw(sr);
                if (!sv.ptr) goto done;
                walk_sv(w, sv);
            } else {
                yyjson_val val = yyjson_sr_read_scalar(sr);
                u64 bits = 0;
                if (yyjson_sr_has_err(sr)) goto done;
                if (yyjson_is_real(&val)) {
                    f64 num = yyjson_get_real(&val);
                    memcpy(&bits, &num, sizeof(bits));
                } else if (yyjson_is_bool(&val)) {
                    bits = yyjson_get_bool(&val) ? 1 : 0;
                } else if (!yyjson_is_null(&val)) {
                    bits = yyjson_get_uint(&val);
                }
                walk_mix(w, &bits, sizeof(bits));
                w->count++;
            }
            break;
        }

        // the next value, closing the containers that are done
        while (true) {
            if (!depth) {
                ok = yyjson_sr_finish(sr);
                goto done;
            }
            if (stack[depth - 1]) {
                if (yyjson_sr_obj_next(sr, &sv)) {
                    walk_sv(w, sv);
                    break;
                }
                if (!yyjson_sr_obj_end(sr)) goto done;
            } else {
                if (yyjson_sr_arr_next(sr)) break;
                if (!yyjson_sr_arr_end(sr)) goto done;
            }
            depth--;
        }
    }
done:
    free(stack);
    return ok;
}

// Reads `json` with the largest value and depth it has, in a pool that has
// exactly the guaranteed size. The limits are set to the same values or not.
static void pool_check(const char *json, usize len, src_mode m, usize off,
                       bool limits, const walk_t *ref) {
    rd_t r;
    pool_t pool;
    walk_t w = {0};
    usize val_len = ref->max_len ? ref->max_len : 1;
    yyjson_read_err err = {0};
    yyjson_sr_opts opts = {0};

    pool_init(&pool, YYJSON_SR_POOL_SIZE(val_len, ref->max_depth), off);
    if (limits) {
        opts.max_depth = ref->max_depth;
        opts.max_val_len = val_len;
    }
    yy_assert(rd_open(&r, m, json, len, &opts, &pool.alc));
    if (!walk_doc(&r.sr, &w, false)) {
        yyjson_sr_get_err(&r.sr, &err);
        yy_assertf(0, "pool read failed: %s, pos %zu, kind %d chunk %zu "
                   "off %zu limits %d", err.msg, (usize)err.pos, (int)m.kind,
                   m.chunk, off, (int)limits);
    }
    yy_assert(w.hash == ref->hash && w.count == ref->count);
    yy_assert(yyjson_sr_pos(&r.sr) == len);
    rd_close(&r);
    pool_free(&pool);
}

// The input sources used with the pools: the window is the largest read.
static const src_mode pool_modes[] = {
    { SRC_INSITU, 0, false },
    { SRC_MEM, 0, false },
    { SRC_CB, 0, false },
    { SRC_CB, 1, false },
    { SRC_CB, 7, false },
    { SRC_CB, 17, false },
    { SRC_CB, 4096, false },
};

// Measures `json` with an ordinary read, then reads it in the exact pools.
// A raw read measures the length of numbers and literals too.
static void pool_sweep(const char *json, usize len, usize offs) {
    walk_t ref = {0}, raw = {0};
    rd_t r;
    usize off, i;

    yy_assert(rd_open(&r, src_modes[1], json, len, NULL, NULL));
    yy_assert(walk_doc(&r.sr, &ref, false));
    rd_close(&r);
    yy_assert(rd_open(&r, src_modes[1], json, len, NULL, NULL));
    yy_assert(walk_doc(&r.sr, &raw, true));
    rd_close(&r);
    yy_assert(ref.count == raw.count && ref.max_depth == raw.max_depth);
    ref.max_len = raw.max_len;
    if (!SR_DEPTH_OK(ref.max_depth)) return;

    for (i = 0; i < yy_nelems(pool_modes); i++) {
        for (off = 0; off < offs; off++) {
            pool_check(json, len, pool_modes[i], off, false, &ref);
            pool_check(json, len, pool_modes[i], off, true, &ref);
        }
    }
}

// Spaces everywhere between the tokens, one more each time. The refill is
// cut at every position of the document.
static void test_sr_pool_space(void) {
    static const char tpl[] =
        "{~\"k1\"~:~[~1~,~-0.5~,~\"s\\n\"~,~true~,~null~]~,~\"k2\"~:~{~"
        "\"x\"~:~\"\\u00e9\\ud83d\\ude00abcdefghij\"~}~}";
    char *json = (char *)malloc(sizeof(tpl) + 40 * 300);
    usize k, len, i;
    yy_assert(json != NULL);
    for (k = 0; k <= 300; k++) {
        for (len = 0, i = 0; tpl[i]; i++) {
            if (tpl[i] == '~') {
                memset(json + len, k & 1 ? '\n' : ' ', k);
                len += k;
            } else {
                json[len++] = tpl[i];
            }
        }
        pool_sweep(json, len, k % 7 ? 1 : 3);
    }
    free(json);
}

// One long token between spaces: the token and the spaces take the window
// in all the ways.
static void test_sr_pool_token(void) {
    static const usize sizes[] = { 1, 40, 150 };
    usize kind, i, pre;
    for (kind = 0; kind < 6; kind++) {
        for (i = 0; i < yy_nelems(sizes); i++) {
            for (pre = 0; pre <= 320; pre += (pre < 40 ? 1 : 7)) {
                char *json = (char *)malloc(sizes[i] * 6 + pre * 2 + 40);
                usize len = 0, n;
                yy_assert(json != NULL);
                json[len++] = kind < 4 ? '[' : '{';
                memset(json + len, ' ', pre);
                len += pre;
                if (kind != 2 && kind != 3) json[len++] = '"';
                for (n = 0; n < sizes[i]; n++) {
                    switch (kind) {
                    case 0: json[len++] = 'a'; break;
                    case 1: memcpy(json + len, "\\u00e9", 6); len += 6; break;
                    case 2: json[len++] = n ? (char)('0' + n % 10) : '1'; break;
                    case 3: json[len++] = n ? (n == 1 ? '.' : '5') : '0'; break;
                    case 4: json[len++] = 'k'; break;
                    default: memcpy(json + len, "\\n", 2); len += 2; break;
                    }
                }
                if (kind != 2 && kind != 3) json[len++] = '"';
                if (kind >= 4) {
                    json[len++] = ':';
                    memset(json + len, ' ', pre / 3);
                    len += pre / 3;
                    json[len++] = '1';
                    json[len++] = '}';
                } else {
                    memset(json + len, ' ', pre / 3);
                    len += pre / 3;
                    json[len++] = ']';
                }
                pool_sweep(json, len, 1 + pre % 4);
                free(json);
            }
        }
    }
}

// Deep nesting with a long value in the middle, behind the spaces that move
// the opening brackets and the value over the end of the window.
static void test_sr_pool_deep(void) {
    static const usize depths[] = { 1, 255, 257, 1030 };
    static const usize vals[] = { 1, 100, 300 };
    usize d, v, pre;
    for (d = 0; d < yy_nelems(depths); d++) {
        if (!SR_DEPTH_OK(depths[d])) continue;
        for (v = 0; v < yy_nelems(vals); v++) {
            for (pre = 0; pre < 40; pre++) {
                usize depth = depths[d], vlen = vals[v];
                usize size = YYJSON_SR_POOL_SIZE(vlen, depth);
                usize gap = size - 20 + pre, len = 0;
                char *json = (char *)malloc(depth * 2 + vlen + gap * 2 + 8);
                yy_assert(json != NULL);
                memset(json, ' ', gap);
                len += gap;
                memset(json + len, '[', depth);
                len += depth;
                json[len++] = '"';
                memset(json + len, 'a', vlen);
                len += vlen;
                json[len++] = '"';
                memset(json + len, ' ', gap);
                len += gap;
                memset(json + len, ']', depth);
                len += depth;
                pool_sweep(json, len, 1);
                free(json);
            }
        }
    }
}

// A pool that is too small, or just the minimum: the read either works, or
// stops at the pool limit. Past the compile-time depth limit it stops for
// depth instead. No other error, and no write out of the pool.
static void test_sr_pool_limit(void) {
    static const char *toks[] = { "true", "false", "null", "12345", "\"abc\"" };
    usize t, depth, ok = 0, bad = 0;
    for (t = 0; t < yy_nelems(toks); t++) {
        for (depth = 0; depth < 400; depth += 3) {
            const char *tok = toks[t];
            usize tlen = strlen(tok), len = 0;
            char *json = (char *)malloc(depth * 2 + tlen + 4);
            yy_assert(json != NULL);
            memset(json, '[', depth);
            len = depth;
            memcpy(json + len, tok, tlen);
            len += tlen;
            memset(json + len, ']', depth);
            len += depth;
            each_mode(m) {
                rd_t r;
                pool_t pool;
                walk_t w = {0};
                pool_init(&pool, YYJSON_STREAMING_POOL_SIZE_MIN, ok % 16);
                yy_assert(rd_open(&r, *m, json, len, NULL, &pool.alc));
                if (walk_doc(&r.sr, &w, false)) {
                    ok++;
                    yy_assert(w.max_depth == depth);
                } else {
                    bad++;
                    expect_err(&r.sr, SR_DEPTH_OK(depth) ?
                               YYJSON_READ_ERROR_BUFFER_LIMIT :
                               YYJSON_READ_ERROR_DEPTH);
                }
                rd_close(&r);
                pool_free(&pool);
            }
            free(json);
        }
    }
    yy_assert(ok > 0 && bad > 0);
}

// A value too long for the pool is a pool limit error, unless it is in-situ.
static void test_sr_pool_small(void) {
    char *json = (char *)malloc(20000);
    usize len = 0;
    yy_assert(json != NULL);
    json[len++] = '"';
    memset(json + len, 'a', 15000);
    len += 15000;
    json[len++] = '"';
    each_mode(m) {
        rd_t r;
        pool_t pool;
        pool_init(&pool, 4096, 0);
        yy_assert(rd_open(&r, *m, json, len, NULL, &pool.alc));
        if (m->kind == SRC_INSITU) { // the string stays in the input
            expect_strn(&r.sr, json + 1, 15000);
            yy_assert(yyjson_sr_finish(&r.sr));
        } else {
            yy_assert(yyjson_sr_read_str(&r.sr).ptr == NULL);
            expect_err(&r.sr, YYJSON_READ_ERROR_BUFFER_LIMIT);
        }
        rd_close(&r);
        pool_free(&pool);
    }
    free(json);
}



/*==============================================================================
 * MARK: - Allocator
 *============================================================================*/

// A deep or a long value makes one buffer that is not much larger than it
// needs: a byte per level, and the copied string. The buffer doubles.
static void test_sr_alloc_budget(void) {
    enum { depth = 20000, str = 40000 };
    usize i;

    for (i = 0; i < 3; i++) {
        // 0: only the depth, 1: and a string, 2: only the string
        usize n = i == 2 ? 0 : depth, s = i ? str : 0, len = n;
        char *json;
        if (!SR_DEPTH_OK(n)) continue;
        json = (char *)malloc(n * 2 + s + 8);
        yy_assert(json != NULL);
        memset(json, '[', n);
        json[len++] = '"';
        memset(json + len, 'x', s);
        len += s;
        json[len++] = '"';
        memset(json + len, ']', n);
        len += n;

        each_mode(m) {
            rd_t r;
            test_alc ctx;
            yyjson_alc alc;
            walk_t w = {0};
            usize budget = i ? 131072 : 65536;
            if (m->kind == SRC_CB) budget *= 2;
            test_alc_init(&alc, &ctx, NULL, SIZE_MAX, budget);
            yy_assert(rd_open(&r, *m, json, len, &(yyjson_sr_opts){ 0, n, 0, 0 }, &alc));
            yy_assertf(walk_doc(&r.sr, &w, false), "mode %d, case %d",
                       (int)(m - src_modes), (int)i);
            yy_assert(w.max_depth == n && w.max_len == s);
            rd_close(&r);
        }
        free(json);
    }
}

// A failed allocation anywhere: an error, or no failure at all.
static void test_sr_alloc_fail(void) {
    enum { depth = 3000, str = 40000 };
    char *json;
    usize len = 0, left, fails = 0;
    walk_t ref = {0};
    if (!SR_DEPTH_OK(depth)) return;
    json = (char *)malloc(depth * 2 + str + 8);
    yy_assert(json != NULL);
    memset(json, '[', depth);
    len = depth;
    json[len++] = '"';
    memset(json + len, 'b', str);
    len += str;
    json[len++] = '"';
    memset(json + len, ']', depth);
    len += depth;
    {
        rd_t r;
        yy_assert(rd_open(&r, src_modes[1], json, len, NULL, NULL));
        yy_assert(walk_doc(&r.sr, &ref, false));
        rd_close(&r);
    }

    each_mode(m) {
        bool done = false;
        for (left = 0; !done; left++) {
            rd_t r;
            test_alc ctx;
            yyjson_alc alc;
            walk_t w = {0};
            test_alc_init(&alc, &ctx, NULL, left, SIZE_MAX);
            if (!rd_open(&r, *m, json, len, NULL, &alc)) {
                yy_assert(yyjson_sr_get_err(&r.sr, NULL) ==
                          YYJSON_READ_ERROR_MEMORY_ALLOCATION);
                fails++;
            } else if (!walk_doc(&r.sr, &w, false)) {
                yy_assert(yyjson_sr_get_err(&r.sr, NULL) ==
                          YYJSON_READ_ERROR_MEMORY_ALLOCATION);
                yy_assert(ctx.left == 0);
                fails++;
            } else {
                yy_assert(w.hash == ref.hash && w.count == ref.count);
                done = true;
            }
            rd_close(&r);
            yy_assert(left < 100);
        }
    }
    yy_assert(fails > 0);
    free(json);
}

// A pool that is wrapped by an allocator is an ordinary allocator, it grows.
static void test_sr_alloc_wrapped(void) {
    enum { depth = 20000 };
    yyjson_alc pool, alc;
    test_alc ctx;
    char *mem, *json;
    rd_t r;
    walk_t w = {0};
    if (!SR_DEPTH_OK(depth)) return;
    mem = (char *)malloc(1 << 20);
    json = (char *)malloc(depth * 2);
    yy_assert(mem && json);
    yy_assert(yyjson_alc_pool_init(&pool, mem, 1 << 20));
    test_alc_init(&alc, &ctx, &pool, SIZE_MAX, SIZE_MAX);
    yy_assert(nest_make(json, false, depth) == depth * 2);
    yy_assert(rd_open(&r, src_modes[1], json, depth * 2,
              &(yyjson_sr_opts){ 0, depth, 0, 0 }, &alc));
    yy_assert(walk_doc(&r.sr, &w, false) && w.max_depth == depth);
    yy_assert(ctx.calls > 1);
    rd_close(&r);
    free(json);
    free(mem);
}



/*==============================================================================
 * MARK: - Rewind
 *============================================================================*/

// The input sources that can rewind, and those that cannot.
static const src_mode rewind_yes[] = {
    { SRC_MEM, 0, false },
    { SRC_CB, 0, true },
    { SRC_CB, 1, true },
    { SRC_CB, 3, true },
    { SRC_CB, 64, true },
};

static const src_mode rewind_no[] = {
    { SRC_INSITU, 0, false },
    { SRC_CB, 0, false },
    { SRC_CB, 3, false },
};

#define each_rewind(m) \
    for (const src_mode *m = rewind_yes; \
         m < rewind_yes + yy_nelems(rewind_yes); m++)

static void test_sr_rewind_can(void) {
    usize i;
    yy_assert(!yyjson_sr_can_rewind(NULL));
    for (i = 0; i < yy_nelems(rewind_yes); i++) {
        rd_t r;
        u64 start = UINT64_MAX;
        rd_start(&r, rewind_yes[i], "{}");
        yy_assert(yyjson_sr_can_rewind(&r.sr));
        yy_assert(yyjson_sr_obj_begin_rewind(&r.sr, &start));
        yy_assert(start == 1);
        yy_assert(yyjson_sr_obj_end(&r.sr));
        yy_assert(yyjson_sr_finish(&r.sr));
        rd_close(&r);
    }
    for (i = 0; i < yy_nelems(rewind_no); i++) {
        rd_t r;
        u64 start = UINT64_MAX;
        rd_start(&r, rewind_no[i], "{}");
        yy_assert(!yyjson_sr_can_rewind(&r.sr));
        yy_assert(!yyjson_sr_obj_begin_rewind(&r.sr, &start));
        yy_assert(start == UINT64_MAX && yyjson_sr_pos(&r.sr) == 0);
        expect_err(&r.sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
    }
}

// Searches the members that are behind the cursor, and those ahead of it.
static void test_sr_rewind_find(void) {
    static const char json[] =
        "{\"a\":1,\"b\":[2,{\"x\":3}],\"\\u0063\":\"s\",\"d\":null}";
    each_rewind(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        yyjson_sv key;
        u64 start, start2;

        rd_start(&r, *m, json);
        yy_assert(yyjson_sr_obj_begin_rewind(sr, &start));
        yy_assert(start == 1);

        // ahead: the same as a find
        yy_assert(yyjson_sr_obj_find_rewind(sr, "c", start));
        expect_str(sr, "s");
        // behind
        yy_assert(yyjson_sr_obj_find_rewind(sr, "a", start));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        // ahead of the cursor, which is now after "a"
        yy_assert(yyjson_sr_obj_find_rewind(sr, "d", start));
        yy_assert(yyjson_sr_read_null(sr));
        // behind again, and the value is a container with its own search
        yy_assert(yyjson_sr_obj_find_rewind(sr, "b", start));
        yy_assert(yyjson_sr_arr_begin(sr));
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_arr_next(sr));
        yy_assert(yyjson_sr_obj_begin_rewind(sr, &start2));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "y", start2));
        expect_ok(sr);
        yy_assert(yyjson_sr_obj_find_rewind(sr, "x", start2));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_arr_end(sr));

        // a miss goes back to where it was: "c" and "d" are still to read
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "none", start));
        expect_ok(sr);
        expect_key(sr, "c");
        expect_str(sr, "s");
        expect_key(sr, "d");
        yy_assert(yyjson_sr_read_null(sr));
        yy_assert(!yyjson_sr_obj_next(sr, &key));
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);
    }
}

// The position after a miss is the one before it, with or without members
// behind it, and with a value that was not read.
static void test_sr_rewind_miss(void) {
    each_rewind(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;
        u64 start, pos;

        // nothing behind, the whole object is ahead
        rd_start(&r, *m, "{\"a\":1,\"b\":2,\"c\":3}");
        yy_assert(yyjson_sr_obj_begin_rewind(sr, &start));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "z", start));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "z", start));
        expect_ok(sr);
        yy_assert(yyjson_sr_pos(sr) == start);
        expect_key(sr, "a");
        yy_assert(yyjson_sr_read_uint(sr) == 1);

        // members behind and ahead
        pos = yyjson_sr_pos(sr);
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "z", start));
        expect_ok(sr);
        yy_assert(yyjson_sr_pos(sr) == pos);
        yy_assert(yyjson_sr_obj_find_rewind(sr, "c", start));
        yy_assert(yyjson_sr_read_uint(sr) == 3);
        yy_assert(yyjson_sr_obj_find_rewind(sr, "b", start));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_end(sr));
        yy_assert(yyjson_sr_finish(sr));
        rd_close(&r);

        // the current value is skipped first, it is not a candidate
        rd_start(&r, *m, "{\"a\":1,\"b\":2,\"c\":3}");
        yy_assert(yyjson_sr_obj_begin_rewind(sr, &start));
        expect_key(sr, "a");
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "z", start));
        expect_key(sr, "b");
        yy_assert(yyjson_sr_obj_find_rewind(sr, "a", start));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        yy_assert(yyjson_sr_obj_end(sr));
        rd_close(&r);

        // the key is the one at the cursor
        rd_start(&r, *m, "{\"a\":1,\"a\":2}");
        yy_assert(yyjson_sr_obj_begin_rewind(sr, &start));
        expect_key(sr, "a");
        yy_assert(yyjson_sr_obj_find_rewind(sr, "a", start));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        yy_assert(yyjson_sr_obj_find_rewind(sr, "a", start));
        yy_assert(yyjson_sr_read_uint(sr) == 1);
        rd_close(&r);
    }
}

static void test_sr_rewind_err(void) {
    usize i;
    u64 start = UINT64_MAX;
    yy_assert(!yyjson_sr_obj_begin_rewind(NULL, &start));
    yy_assert(start == UINT64_MAX);
    yy_assert(!yyjson_sr_obj_find_rewind(NULL, "a", 0));
    yy_assert(!yyjson_sr_obj_find_rewindn(NULL, "a", 1, 0));

    // sources that cannot rewind fail, even for a member that is ahead
    for (i = 0; i < yy_nelems(rewind_no); i++) {
        rd_t r;
        rd_start(&r, rewind_no[i], "{\"a\":1}");
        yy_assert(yyjson_sr_obj_begin(&r.sr));
        yy_assert(!yyjson_sr_obj_find_rewind(&r.sr, "a", 1));
        expect_err(&r.sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
    }

    each_rewind(m) {
        rd_t r;
        yyjson_sr *sr = &r.sr;

        // no place to save the start
        rd_start(&r, *m, "{}");
        yy_assert(!yyjson_sr_obj_begin_rewind(sr, NULL));
        yy_assert(yyjson_sr_pos(sr) == 0);
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);

        // the current value is not an object
        rd_start(&r, *m, "[]");
        yy_assert(!yyjson_sr_obj_begin_rewind(sr, &start));
        yy_assert(start == UINT64_MAX);
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
        rd_close(&r);

        // not in an object
        rd_start(&r, *m, "[{\"a\":1}]");
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "a", 0));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);
        rd_start(&r, *m, "[{\"a\":1}]");
        yy_assert(yyjson_sr_arr_begin(sr) && yyjson_sr_arr_next(sr));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "a", 0));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);

        // no key
        rd_start(&r, *m, "{\"a\":1}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, NULL, 1));
        yy_assert(!yyjson_sr_obj_find_rewindn(sr, NULL, 0, 1));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);

        // a start after the cursor is used only when there is a miss
        rd_start(&r, *m, "{\"a\":1,\"b\":2}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(yyjson_sr_obj_find_rewind(sr, "b", 9999));
        yy_assert(yyjson_sr_read_uint(sr) == 2);
        rd_close(&r);
        rd_start(&r, *m, "{\"a\":1,\"b\":2}");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "z", yyjson_sr_pos(sr) + 1));
        expect_err(sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
        rd_close(&r);

        // a member that cannot be passed
        rd_start(&r, *m, "{\"a\":1,\"b\":\"abc");
        yy_assert(yyjson_sr_obj_begin(sr));
        yy_assert(!yyjson_sr_obj_find_rewind(sr, "c", 1));
        expect_err(sr, YYJSON_READ_ERROR_UNEXPECTED_END);
        rd_close(&r);
    }
}

// Any start offset is memory safe, a valid one gives the member.
static void test_sr_rewind_start(void) {
    static const char json[] = "{\"a\":1,\"bb\":[1,{\"c\":2}],\"d\":\"s\",\"e\":4}";
    usize len = sizeof(json) - 1, start;
    each_rewind(m) {
        for (start = 0; start <= len + 2; start++) {
            rd_t r;
            yyjson_sr *sr = &r.sr;
            bool ok;
            rd_start(&r, *m, json);
            yy_assert(yyjson_sr_obj_begin(sr));
            yy_assert(yyjson_sr_obj_find(sr, "e"));
            ok = yyjson_sr_obj_find_rewind(sr, "a", start);
            if (start == 1) {
                yy_assert(ok && yyjson_sr_read_uint(sr) == 1);
            } else if (start > yyjson_sr_pos(sr) && !ok) {
                yy_assert(yyjson_sr_has_err(sr));
            }
            // the key `d` is behind `e`, the reader can go on or has failed
            (void)yyjson_sr_obj_find_rewind(sr, "d", start);
            rd_close(&r);
        }
    }
}

// A long object, read from the end back to the start, in all the ways.
static void test_sr_rewind_long(void) {
    enum { count = 3000 };
    sink_t json = {0};
    usize i, len;
    char tmp[64];
    sink_puts(&json, "{");
    for (i = 0; i < count; i++) {
        snprintf(tmp, sizeof(tmp), "%s\"k%u\":[%u,\"v%u\"]", i ? "," : "",
                 (unsigned)i, (unsigned)i, (unsigned)i);
        sink_puts(&json, tmp);
    }
    sink_puts(&json, "}");
    len = json.len;

    each_rewind(m) {
        // the default allocator, and a pool of the least size
        usize pass;
        for (pass = 0; pass < 2; pass++) {
            static const usize order[] = { 2999, 0, 1500, 10, 2000, 5, 2999, 0 };
            rd_t r;
            pool_t pool = {0};
            yyjson_sr *sr = &r.sr;
            u64 start;
            usize k;
            if (pass) pool_init(&pool, YYJSON_SR_POOL_SIZE(16, 2), 0);
            yy_assert(rd_open(&r, *m, json.dat, len, NULL, pass ? &pool.alc : NULL));
            yy_assert(yyjson_sr_obj_begin_rewind(sr, &start));
            for (k = 0; k < yy_nelems(order); k++) {
                snprintf(tmp, sizeof(tmp), "k%u", (unsigned)order[k]);
                yy_assert(yyjson_sr_obj_find_rewind(sr, tmp, start));
                yy_assert(yyjson_sr_arr_begin(sr));
                yy_assert(yyjson_sr_arr_next(sr));
                yy_assert(yyjson_sr_read_uint(sr) == order[k]);
                yy_assert(yyjson_sr_arr_next(sr));
                snprintf(tmp, sizeof(tmp), "v%u", (unsigned)order[k]);
                expect_str(sr, tmp);
                yy_assert(yyjson_sr_arr_end(sr));
            }
            yy_assert(!yyjson_sr_obj_find_rewind(sr, "none", start));
            expect_ok(sr);
            yy_assert(yyjson_sr_obj_end(sr));
            yy_assert(yyjson_sr_finish(sr));
            yy_assert(yyjson_sr_pos(sr) == len);
            rd_close(&r);
            if (pass) pool_free(&pool);
        }
    }
    sink_free(&json);
}

// The input source that counts its seeks.
typedef struct {
    const char *dat;
    usize len, pos;
    usize seeks;
    u64 last;
    bool fail;
} seek_t;

static usize seek_read(void *ctx, void *dst, usize cap) {
    seek_t *s = (seek_t *)ctx;
    usize n = s->len - s->pos;
    if (n > cap) n = cap;
    memcpy(dst, s->dat + s->pos, n);
    s->pos += n;
    return n;
}

static bool seek_seek(void *ctx, u64 pos) {
    seek_t *s = (seek_t *)ctx;
    s->seeks++;
    s->last = pos;
    if (s->fail || pos > s->len) return false;
    s->pos = (usize)pos;
    return true;
}

static void test_sr_rewind_seek(void) {
    static const char json[] = "{\"a\":1,\"b\":2,\"c\":3}";
    yyjson_sr sr = {0};
    seek_t s = { json, sizeof(json) - 1, 0, 0, 0, false };
    u64 start, origin;

    yy_assert(yyjson_sr_init_cb(&sr, seek_read, seek_seek, &s, NULL, NULL));
    yy_assert(yyjson_sr_can_rewind(&sr));
    yy_assert(yyjson_sr_obj_begin_rewind(&sr, &start));

    // an ahead member does not seek
    yy_assert(yyjson_sr_obj_find_rewind(&sr, "b", start));
    yy_assert(yyjson_sr_read_uint(&sr) == 2);
    yy_assert(s.seeks == 0);

    // a member behind: seeks to the start, reads as far as the old position
    yy_assert(yyjson_sr_obj_find_rewind(&sr, "a", start));
    yy_assert(s.seeks == 1 && s.last == start);
    yy_assert(yyjson_sr_read_uint(&sr) == 1);

    // a miss goes back to the position it had
    origin = yyjson_sr_pos(&sr);
    s.seeks = 0;
    yy_assert(!yyjson_sr_obj_find_rewind(&sr, "z", start));
    yy_assert(s.seeks == 2 && s.last == origin);
    yy_assert(yyjson_sr_pos(&sr) == origin);
    expect_ok(&sr);

    // a seek that fails is an I/O error
    s.fail = true;
    yy_assert(!yyjson_sr_obj_find_rewind(&sr, "z", start));
    expect_err(&sr, YYJSON_READ_ERROR_STREAMING_IO);
    yyjson_sr_free(&sr);

    // no seek function, no rewind
    s = (seek_t){ json, sizeof(json) - 1, 0, 0, 0, false };
    yy_assert(yyjson_sr_init_cb(&sr, seek_read, NULL, &s, NULL, NULL));
    yy_assert(!yyjson_sr_can_rewind(&sr));
    yyjson_sr_free(&sr);
}

#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE



/*==============================================================================
 * MARK: - File
 *============================================================================*/

// Returns a FILE with `prefix` and the text in it, at the start of the text.
static FILE *fp_make(const char *prefix, const void *dat, usize len) {
    FILE *fp = tmpfile();
    yy_assert(fp != NULL);
    yy_assert(fwrite(prefix, 1, strlen(prefix), fp) == strlen(prefix));
    yy_assert(fwrite(dat, 1, len, fp) == len);
    yy_assert(fseek(fp, (long)strlen(prefix), SEEK_SET) == 0);
    return fp;
}

static void test_sr_fp(void) {
    static const char json[] =
        "{\"a\":[1,\"two\",{\"b\":null}],\"c\":true,\"d\":0.5}";
    static const usize wins[] = { 0, 1, 7, 100 };
    yyjson_sr sr = {0};
    usize i;

    // no FILE
    yy_assert(!yyjson_sr_init_fp(&sr, NULL, NULL, NULL));
    expect_err(&sr, YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(!yyjson_sr_init_fp(NULL, stdin, NULL, NULL));

    // the same text as the memory, with a window of any size, and the
    // reader's offsets start from where the file was
    for (i = 0; i < yy_nelems(wins); i++) {
        usize pre;
        for (pre = 0; pre < 2; pre++) {
            FILE *fp = fp_make(pre ? "xxx\n" : "", json, sizeof(json) - 1);
            sink_t out = {0};
            u64 start;
            yy_assert(yyjson_sr_init_fp(&sr, fp, &(yyjson_sr_opts){ 0, 0, 0, wins[i] },
                      NULL));
            yy_assert(yyjson_sr_can_rewind(&sr));
            yy_assert(yyjson_sr_pos(&sr) == 0);
            yy_assert(yyjson_sr_obj_begin_rewind(&sr, &start));
            yy_assert(start == 1);
            yy_assert(yyjson_sr_obj_find_rewind(&sr, "d", start));
            yy_assert(yyjson_sr_read_real(&sr) == 0.5);
            yy_assert(yyjson_sr_obj_find_rewind(&sr, "c", start));
            yy_assert(yyjson_sr_read_bool(&sr));
            yy_assert(!yyjson_sr_obj_find_rewind(&sr, "z", start));
            expect_ok(&sr);
            yy_assert(yyjson_sr_obj_find_rewind(&sr, "a", start));
            yy_assert(yyjson_sr_arr_begin(&sr));
            yy_assert(yyjson_sr_arr_end(&sr));
            yy_assert(yyjson_sr_obj_end(&sr));
            yy_assert(yyjson_sr_finish(&sr));
            yy_assert(yyjson_sr_pos(&sr) == sizeof(json) - 1);
            yyjson_sr_free(&sr);

            // the file is left after what was read: the whole file here
            yy_assert(fseek(fp, pre ? 4 : 0, SEEK_SET) == 0);
            yy_assert(yyjson_sr_init_fp(&sr, fp, NULL, NULL));
            yy_assert(dump_doc(&sr, &out));
            yy_assert(strstr(out.dat, "\"two\"") != NULL);
            sink_free(&out);
            yyjson_sr_free(&sr);
            fclose(fp);
        }
    }

    // a seek that cannot be done: a position the file cannot take
    {
        FILE *fp = fp_make("", "{\"a\":1}", 7);
        yy_assert(yyjson_sr_init_fp(&sr, fp, NULL, NULL));
        yy_assert(yyjson_sr_obj_begin(&sr));
        yy_assert(yyjson_sr_obj_find(&sr, "a"));
        yy_assert(!yyjson_sr_obj_find_rewind(&sr, "z", (u64)LONG_MAX));
        yy_assert(yyjson_sr_has_err(&sr));
        yyjson_sr_free(&sr);
        fclose(fp);
    }

    // a file that cannot be read
    {
#ifdef _WIN32
        FILE *fp = fopen("NUL", "wb");
#else
        FILE *fp = fopen("/dev/null", "wb");
#endif
        if (fp) {
            yy_assert(yyjson_sr_init_fp(&sr, fp, NULL, NULL));
            yy_assert(yyjson_sr_peek_type(&sr) == YYJSON_TYPE_NONE);
            yy_assert(!yyjson_sr_finish(&sr));
            expect_err(&sr, YYJSON_READ_ERROR_STREAMING_IO);
            yyjson_sr_free(&sr);
            fclose(fp);
        }
    }
}

#endif /* !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE */



yy_test_case(test_json_sr) {
    test_sr_init();
    test_sr_reuse();
    test_sr_values();
    test_sr_numbers();
    test_sr_num_val();
    test_sr_literals();
    test_sr_strings();
    test_sr_str_edge();
    test_sr_str_bad();
#if !YYJSON_DISABLE_UTF8_VALIDATION
    test_sr_utf8_bad();
#endif
    test_sr_flags();
    test_sr_flags_ignored();
#if !YYJSON_DISABLE_NON_STANDARD
    test_sr_utf8_allow();
#endif
    test_sr_raw();
    test_sr_peek();
    test_sr_find();
    test_sr_find_fast();
    test_sr_obj_read();
    test_sr_find_bad();
    test_sr_find_skip();
    test_sr_skip_nul();
    test_sr_current();
    test_sr_empty();
    test_sr_misuse();
    test_sr_move_on();
    test_sr_pos();
    test_sr_depth();
    test_sr_docs();
    test_sr_surrogate();
    test_sr_skip();
    test_sr_skip_deep();
    test_sr_errors();
    test_sr_sticky();
    test_sr_io();
    test_sr_max_depth();
    test_sr_options();
    test_sr_max_val_len();
    test_sr_max_val_len_obj();
    test_sr_max_val_len_long();
    test_sr_win_size();
    test_sr_pool_space();
    test_sr_pool_token();
    test_sr_pool_deep();
    test_sr_pool_limit();
    test_sr_pool_small();
    test_sr_alloc_budget();
    test_sr_alloc_fail();
    test_sr_alloc_wrapped();
    test_sr_rewind_can();
    test_sr_rewind_find();
    test_sr_rewind_miss();
    test_sr_rewind_err();
    test_sr_rewind_start();
    test_sr_rewind_long();
    test_sr_rewind_seek();
#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE
    test_sr_fp();
#endif
}

#else
yy_test_case(test_json_sr) {}
#endif
