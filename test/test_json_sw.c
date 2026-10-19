// This file is used to test the streaming JSON writer.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_STREAMING && !YYJSON_DISABLE_WRITER



/*==============================================================================
 * MARK: - Helper
 *============================================================================*/

// Growable byte sink, also the context of the write callback.
typedef struct {
    char *dat;
    usize len;
    usize cap;
    usize calls;     // write callbacks seen
    usize max_chunk; // longest chunk seen
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

static bool sink_write(void *ctx, const void *src, size_t len) {
    sink_t *s = (sink_t *)ctx;
    s->calls++;
    if (len > s->max_chunk) s->max_chunk = len;
    sink_put(s, src, len);
    return true;
}

// Write callback that fails after `left` successful calls, or always without
// a context.
typedef struct {
    usize left;
} fail_t;

static bool fail_write(void *ctx, const void *src, size_t len) {
    fail_t *f = (fail_t *)ctx;
    (void)src;
    (void)len;
    if (!f || !f->left) return false;
    f->left--;
    return true;
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

#if YYJSON_WRITER_DEPTH_LIMIT
#   define SW_DEPTH_OK(n) ((usize)(n) <= (usize)YYJSON_WRITER_DEPTH_LIMIT)
#else
#   define SW_DEPTH_OK(n) 1
#endif

// Where the output goes.
typedef enum {
    DST_MEM,  // memory
    DST_CB,   // callback
    DST_POOL, // callback, with a pool
} dst_kind;

typedef struct {
    dst_kind kind;
    usize win; // callback window, 0 for the default
} dst_mode;

// Every way to take the same output, the results must be the same.
static const dst_mode dst_modes[] = {
    { DST_MEM, 0 },
    { DST_CB, 0 },
    { DST_CB, 5 },
    { DST_CB, 6 },
    { DST_CB, 16 },
    { DST_CB, 128 },
    { DST_POOL, 0 },
    { DST_POOL, 7 },
};

#define each_dst(m) \
    for (const dst_mode *m = dst_modes; \
         m < dst_modes + yy_nelems(dst_modes); m++)

// A writer with its output.
typedef struct {
    yyjson_sw sw;
    sink_t out;
    char *mem;
    pool_t pool;
    usize pool_size;
    dst_mode mode;
} wr_t;

// Opens a writer. `cap` is the size of the memory buffer, and `depth` is the
// depth that a pool must have. The memory is exactly `cap` bytes long.
static void wr_open(wr_t *w, dst_mode m, const yyjson_sw_opts *opts, usize cap,
                    usize depth) {
    yyjson_sw_opts opt = {0};
    if (opts) opt = *opts;
    if (m.win) opt.win_size = m.win;
    memset(w, 0, sizeof(*w));
    w->mode = m;
    if (m.kind == DST_MEM) {
        w->mem = (char *)malloc(cap);
        yy_assert(w->mem != NULL);
        yy_assert(yyjson_sw_init_mem(&w->sw, w->mem, cap, &opt));
        return;
    }
    if (m.kind == DST_POOL) {
        usize win = m.win ? m.win : 128;
        w->pool_size = YYJSON_SW_POOL_SIZE(win, depth);
        pool_init(&w->pool, w->pool_size, (win + depth) % 16);
    }
    yy_assert(yyjson_sw_init_cb(&w->sw, sink_write, &w->out, &opt,
                                m.kind == DST_POOL ? &w->pool.alc : NULL));
}

static void wr_close(wr_t *w) {
    yyjson_sw_free(&w->sw);
    sink_free(&w->out);
    free(w->mem);
    if (w->pool.mem) pool_free(&w->pool);
}

// Returns the output, it is complete after a finish.
static const char *wr_text(wr_t *w, usize *len) {
    if (w->mode.kind == DST_MEM) {
        *len = (usize)yyjson_sw_len(&w->sw);
        return w->mem;
    }
    yy_assert(w->out.len == yyjson_sw_len(&w->sw));
    *len = w->out.len;
    return w->out.dat ? w->out.dat : "";
}

// Expects the finished output, and that no chunk was longer than the window.
static void wr_expect(wr_t *w, const char *expect) {
    usize len = strlen(expect), got_len;
    const char *got = wr_text(w, &got_len);
    yy_assertf(got_len == len && !memcmp(got, expect, len),
               "mode %d/%zu: got [%.*s], expect [%s]", (int)w->mode.kind,
               w->mode.win, (int)got_len, got, expect);
    if (w->mode.kind == DST_CB) {
        yy_assert(w->out.max_chunk <= (w->mode.win ? w->mode.win : 16384));
    } else if (w->mode.kind == DST_POOL) {
        yy_assert(w->out.max_chunk <=
                  (w->mode.win ? w->mode.win : w->pool_size));
    }
}

// A document, written by calls on the writer. Returns false on an error.
typedef bool (*sw_script)(yyjson_sw *sw);

// Writes the script in every kind of output, and expects the same text.
static void wr_check(sw_script fn, yyjson_write_flag flg, usize depth,
                     const char *expect) {
    each_dst(m) {
        wr_t w;
        wr_open(&w, *m, &(yyjson_sw_opts){ flg, 0, 0 }, strlen(expect) + depth + 8,
                  depth);
        yy_assert(fn(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        yy_assert(!yyjson_sw_has_err(&w.sw));
        wr_expect(&w, expect);
        wr_close(&w);
    }
}

static void expect_sw_err(yyjson_sw *sw, yyjson_write_code code) {
    yyjson_write_err err;
    yy_assert(yyjson_sw_has_err(sw));
    yy_assertf(yyjson_sw_get_err(sw, &err) == code, "error %d (%s), expect %d",
               (int)err.code, err.msg ? err.msg : "?", (int)code);
    yy_assert(err.code == code && err.msg != NULL);
}

static void expect_sw_ok(yyjson_sw *sw) {
    yyjson_write_err err;
    yy_assert(!yyjson_sw_has_err(sw));
    yy_assert(yyjson_sw_get_err(sw, &err) == YYJSON_WRITE_SUCCESS);
    yy_assert(err.code == YYJSON_WRITE_SUCCESS && err.msg == NULL);
}

// The scripts.

static bool sw_basic(yyjson_sw *sw) {
    return yyjson_sw_obj_begin(sw) &&
           yyjson_sw_write_key(sw, "name") &&
           yyjson_sw_write_str(sw, "Mash") &&
           yyjson_sw_write_key(sw, "values") &&
           yyjson_sw_arr_begin(sw) &&
           yyjson_sw_write_uint(sw, UINT64_MAX) &&
           yyjson_sw_write_sint(sw, INT64_MIN) &&
           yyjson_sw_write_real(sw, 3.25) &&
           yyjson_sw_write_bool(sw, true) &&
           yyjson_sw_write_null(sw) &&
           yyjson_sw_write_raw(sw, "123") &&
           yyjson_sw_arr_end(sw) &&
           yyjson_sw_write_key(sw, "escaped") &&
           yyjson_sw_write_str(sw, "a\n\"b\\c") &&
           yyjson_sw_write_key(sw, "empty") &&
           yyjson_sw_obj_begin(sw) && yyjson_sw_obj_end(sw) &&
           yyjson_sw_write_key(sw, "list") &&
           yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw) &&
           yyjson_sw_obj_end(sw);
}

static bool sw_nums(yyjson_sw *sw) {
    // the last ones need the shortest text, the slow conversion has more
    static const f64 reals[] = {
        0.0, -0.0, 1.5, 1e100, 1e21, 123456789.0,
#if !YYJSON_DISABLE_FAST_FP_CONV
        5e-324, 0.1, -2.5e-8, 1.7976931348623157e308, 3.141592653589793,
#endif
    };
    usize i;
    if (!yyjson_sw_arr_begin(sw) ||
        !yyjson_sw_write_uint(sw, 0) || !yyjson_sw_write_uint(sw, 1) ||
        !yyjson_sw_write_uint(sw, UINT64_MAX) ||
        !yyjson_sw_write_sint(sw, -1) || !yyjson_sw_write_sint(sw, INT64_MIN) ||
        !yyjson_sw_write_sint(sw, INT64_MAX)) return false;
    for (i = 0; i < yy_nelems(reals); i++) {
        if (!yyjson_sw_write_real(sw, reals[i])) return false;
    }
    return yyjson_sw_arr_end(sw);
}

static bool sw_strs(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) &&
           yyjson_sw_write_str(sw, "") &&
           yyjson_sw_write_str(sw, "a\"b\\c\n\r\t\b\f/") &&
           yyjson_sw_write_strn(sw, "\x01\x1f\x7f", 3) &&
           yyjson_sw_write_str(sw, "\xe4\xb8\xad\xf0\x9f\x98\x80\xc2\xa2") &&
           yyjson_sw_write_strn(sw, "a\0b", 3) &&
           yyjson_sw_arr_end(sw);
}

static bool sw_keys(yyjson_sw *sw) {
    return yyjson_sw_obj_begin(sw) &&
           yyjson_sw_write_key(sw, "") && yyjson_sw_write_null(sw) &&
           yyjson_sw_write_key(sw, "a\"\\\n/") && yyjson_sw_write_null(sw) &&
           yyjson_sw_write_keyn(sw, "k\0l", 3) && yyjson_sw_write_null(sw) &&
           yyjson_sw_write_key(sw, "\xe4\xb8\xad") && yyjson_sw_write_null(sw) &&
           yyjson_sw_obj_end(sw);
}

static bool sw_nan(yyjson_sw *sw) {
    yyjson_val num;
    unsafe_yyjson_set_real(&num, INFINITY);
    return yyjson_sw_arr_begin(sw) &&
           yyjson_sw_write_real(sw, -INFINITY) &&
           yyjson_sw_write_real(sw, NAN) &&
           yyjson_sw_write_num(sw, &num) &&
           yyjson_sw_write_real(sw, 1.5) &&
           yyjson_sw_arr_end(sw);
}

static bool sw_nest(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) &&
           yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw) &&
           yyjson_sw_obj_begin(sw) && yyjson_sw_obj_end(sw) &&
           yyjson_sw_arr_begin(sw) && yyjson_sw_arr_begin(sw) &&
           yyjson_sw_arr_end(sw) && yyjson_sw_arr_end(sw) &&
           yyjson_sw_obj_begin(sw) &&
           yyjson_sw_write_key(sw, "a") && yyjson_sw_obj_begin(sw) &&
           yyjson_sw_obj_end(sw) &&
           yyjson_sw_obj_end(sw) &&
           yyjson_sw_arr_end(sw);
}

static const char BASIC[] =
    "{\"name\":\"Mash\",\"values\":[18446744073709551615,"
    "-9223372036854775808,3.25,true,null,123],"
    "\"escaped\":\"a\\n\\\"b\\\\c\",\"empty\":{},\"list\":[]}";
static const char BASIC_PRETTY4[] =
    "{\n"
    "    \"name\": \"Mash\",\n"
    "    \"values\": [\n"
    "        18446744073709551615,\n"
    "        -9223372036854775808,\n"
    "        3.25,\n"
    "        true,\n"
    "        null,\n"
    "        123\n"
    "    ],\n"
    "    \"escaped\": \"a\\n\\\"b\\\\c\",\n"
    "    \"empty\": {},\n"
    "    \"list\": []\n"
    "}";
static const char BASIC_PRETTY2[] =
    "{\n"
    "  \"name\": \"Mash\",\n"
    "  \"values\": [\n"
    "    18446744073709551615,\n"
    "    -9223372036854775808,\n"
    "    3.25,\n"
    "    true,\n"
    "    null,\n"
    "    123\n"
    "  ],\n"
    "  \"escaped\": \"a\\n\\\"b\\\\c\",\n"
    "  \"empty\": {},\n"
    "  \"list\": []\n"
    "}";
static const char BASIC_PRETTY_TAB[] =
    "{\n"
    "\t\"name\": \"Mash\",\n"
    "\t\"values\": [\n"
    "\t\t18446744073709551615,\n"
    "\t\t-9223372036854775808,\n"
    "\t\t3.25,\n"
    "\t\ttrue,\n"
    "\t\tnull,\n"
    "\t\t123\n"
    "\t],\n"
    "\t\"escaped\": \"a\\n\\\"b\\\\c\",\n"
    "\t\"empty\": {},\n"
    "\t\"list\": []\n"
    "}";
static const char NUMS[] =
    "[0,1,18446744073709551615,-1,-9223372036854775808,9223372036854775807,"
    "0.0,-0.0,1.5,1e100,1e21,123456789.0"
#if !YYJSON_DISABLE_FAST_FP_CONV
    ",5e-324,0.1,-2.5e-8,1.7976931348623157e308,3.141592653589793"
#endif
    "]";



/*==============================================================================
 * MARK: - Basic
 *============================================================================*/

static void test_sw_init(void) {
    char buf[64];
    yyjson_sw sw = {0};
    yyjson_val val = {0};

    // no writer
    yy_assert(!yyjson_sw_init_mem(NULL, buf, sizeof(buf), NULL));
    yy_assert(!yyjson_sw_init_cb(NULL, sink_write, NULL, NULL, NULL));
    yy_assert(yyjson_sw_has_err(NULL));
    yy_assert(yyjson_sw_get_err(NULL, NULL) ==
              YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    {
        yyjson_write_err err = {0};
        yy_assert(yyjson_sw_get_err(NULL, &err) ==
                  YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        yy_assert(err.msg != NULL);
    }
    yy_assert(yyjson_sw_len(NULL) == 0 && yyjson_sw_depth(NULL) == 0);
    yy_assert(!yyjson_sw_arr_begin(NULL) && !yyjson_sw_arr_end(NULL));
    yy_assert(!yyjson_sw_obj_begin(NULL) && !yyjson_sw_obj_end(NULL));
    yy_assert(!yyjson_sw_write_key(NULL, "a"));
    yy_assert(!yyjson_sw_write_keyn(NULL, "a", 1));
    yy_assert(!yyjson_sw_write_str(NULL, "a"));
    yy_assert(!yyjson_sw_write_strn(NULL, "a", 1));
    yy_assert(!yyjson_sw_write_uint(NULL, 0));
    yy_assert(!yyjson_sw_write_sint(NULL, 0));
    yy_assert(!yyjson_sw_write_real(NULL, 0.0));
    yy_assert(!yyjson_sw_write_bool(NULL, false));
    yy_assert(!yyjson_sw_write_null(NULL));
    yy_assert(!yyjson_sw_write_num(NULL, &val));
    yy_assert(!yyjson_sw_write_scalar(NULL, &val));
    yy_assert(!yyjson_sw_write_raw(NULL, "1"));
    yy_assert(!yyjson_sw_write_rawn(NULL, "1", 1));
    yy_assert(!yyjson_sw_flush(NULL) && !yyjson_sw_finish(NULL));
    yy_assert(!yyjson_sw_doc_next(NULL));
    yyjson_sw_free(NULL);

    // no output
    yy_assert(!yyjson_sw_init_mem(&sw, NULL, 8, NULL));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    yy_assert(!yyjson_sw_init_mem(&sw, buf, 0, NULL));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    yy_assert(!yyjson_sw_init_cb(&sw, NULL, NULL, NULL, NULL));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    yyjson_sw_free(&sw);

    // a context is optional
    yy_assert(yyjson_sw_init_cb(&sw, fail_write, NULL, NULL, NULL));
    expect_sw_ok(&sw);
    yy_assert(yyjson_sw_write_null(&sw));
    yy_assert(!yyjson_sw_finish(&sw));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_STREAMING_IO);
    yyjson_sw_free(&sw);
}

// A writer is reset when it is initialized again, with whatever it was doing
// and wherever it was writing.
static void test_sw_reuse(void) {
    static const char expect[] = "{\"v\":[1]}\nnull";
    usize from, to, state, round;
    for (from = 0; from < 3; from++) {
        for (to = 0; to < 3; to++) {
            for (state = 0; state < 3; state++) {
                yyjson_sw sw = {0};
                char buf[128];
                pool_t pool[2] = {{0}, {0}};
                sink_t out[2] = {{0}, {0}};
                for (round = 0; round < 2; round++) {
                    usize mode = round ? to : from;
                    yyjson_sw_opts opts = { round ? 0 : YYJSON_WRITE_PRETTY,
                                            round ? 0 : 1, 128 };
                    if (mode == 0) {
                        yy_assert(yyjson_sw_init_mem(&sw, buf, sizeof(buf), &opts));
                    } else {
                        if (mode == 2) {
                            pool_init(&pool[round],
                                      YYJSON_SW_POOL_SIZE(128, 8), 0);
                        }
                        yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out[round],
                                  &opts, mode == 2 ? &pool[round].alc : NULL));
                    }
                    if (!round) {
                        yy_assert(yyjson_sw_arr_begin(&sw));
                        yy_assert(yyjson_sw_write_null(&sw));
                        if (state == 1) {
                            yy_assert(yyjson_sw_arr_end(&sw));
                            yy_assert(yyjson_sw_finish(&sw));
                        } else if (state == 2) {
                            yy_assert(!yyjson_sw_write_key(&sw, "bad"));
                        }
                    } else {
                        usize len;
                        yy_assert(yyjson_sw_obj_begin(&sw));
                        yy_assert(yyjson_sw_write_key(&sw, "v"));
                        yy_assert(yyjson_sw_arr_begin(&sw));
                        yy_assert(yyjson_sw_write_uint(&sw, 1));
                        yy_assert(yyjson_sw_arr_end(&sw));
                        yy_assert(yyjson_sw_obj_end(&sw));
                        yy_assert(yyjson_sw_doc_next(&sw));
                        yy_assert(yyjson_sw_write_null(&sw));
                        yy_assert(yyjson_sw_finish(&sw));
                        len = (usize)yyjson_sw_len(&sw);
                        yy_assert(len == sizeof(expect) - 1);
                        yy_assert(!memcmp(mode ? out[round].dat : buf, expect, len));
                        yy_assert(!yyjson_sw_write_null(&sw));
                        expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
                    }
                    yyjson_sw_free(&sw);
                }
                sink_free(&out[0]);
                sink_free(&out[1]);
                if (pool[0].mem) pool_free(&pool[0]);
                if (pool[1].mem) pool_free(&pool[1]);
            }
        }
    }
}

static void test_sw_values(void) {
    wr_check(sw_basic, 0, 2, BASIC);
    wr_check(sw_nums, 0, 1, NUMS);
    wr_check(sw_strs, 0, 1,
             "[\"\",\"a\\\"b\\\\c\\n\\r\\t\\b\\f/\",\"\\u0001\\u001F\x7f\","
             "\"\xe4\xb8\xad\xf0\x9f\x98\x80\xc2\xa2\",\"a\\u0000b\"]");
    wr_check(sw_keys, 0, 1,
             "{\"\":null,\"a\\\"\\\\\\n/\":null,\"k\\u0000l\":null,"
             "\"\xe4\xb8\xad\":null}");
    wr_check(sw_nest, 0, 3, "[[],{},[[]],{\"a\":{}}]");
}

// A document that is one value.
static bool sw_root(yyjson_sw *sw, usize kind) {
    switch (kind) {
    case 0: return yyjson_sw_write_null(sw);
    case 1: return yyjson_sw_write_bool(sw, true);
    case 2: return yyjson_sw_write_bool(sw, false);
    case 3: return yyjson_sw_write_uint(sw, 42);
    case 4: return yyjson_sw_write_sint(sw, -42);
    case 5: return yyjson_sw_write_real(sw, 0.5);
    case 6: return yyjson_sw_write_str(sw, "s");
    case 7: return yyjson_sw_write_raw(sw, "[1, 2]");
    case 8: return yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw);
    default: return yyjson_sw_obj_begin(sw) && yyjson_sw_obj_end(sw);
    }
}

static void test_sw_root(void) {
    static const char *expects[] = {
        "null", "true", "false", "42", "-42", "0.5", "\"s\"", "[1, 2]", "[]",
        "{}",
    };
    usize i;
    each_dst(m) {
        for (i = 0; i < yy_nelems(expects); i++) {
            wr_t w;
            wr_open(&w, *m, NULL, 32, 1);
            yy_assert(sw_root(&w.sw, i));
            yy_assert(yyjson_sw_finish(&w.sw));
            wr_expect(&w, expects[i]);
            wr_close(&w);
        }
    }
}

// Raw values: unchanged, any bytes, with a length.
static void test_sw_raw(void) {
    static const char raw[] = "{\"a\" : [1,\n2] ,\"\xff\":\t0}";
    each_dst(m) {
        wr_t w;
        yyjson_sw *sw;
        wr_open(&w, *m, NULL, 128, 2);
        sw = &w.sw;
        yy_assert(yyjson_sw_arr_begin(sw));
        yy_assert(yyjson_sw_write_rawn(sw, raw, sizeof(raw) - 1));
        yy_assert(yyjson_sw_write_rawn(sw, "12345", 3));
        yy_assert(yyjson_sw_write_rawn(sw, "n", 1));
        yy_assert(yyjson_sw_arr_end(sw));
        yy_assert(yyjson_sw_finish(sw));
        {
            char expect[64];
            snprintf(expect, sizeof(expect), "[%s,123,n]", raw);
            wr_expect(&w, expect);
        }
        wr_close(&w);
    }

    // short raw values share the window, they are not sent one by one
    {
        sink_t out = {0};
        yyjson_sw sw = {0};
        char dat[200];
        usize i;
        memset(dat, '1', sizeof(dat));
        yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, NULL, NULL));
        yy_assert(yyjson_sw_arr_begin(&sw));
        for (i = 0; i < 100; i++) {
            yy_assert(yyjson_sw_write_rawn(&sw, dat, sizeof(dat)));
        }
        yy_assert(yyjson_sw_arr_end(&sw) && yyjson_sw_finish(&sw));
        yy_assert(out.calls < 5);
        yyjson_sw_free(&sw);
        sink_free(&out);
    }
}

static void test_sw_flags(void) {
    typedef struct {
        sw_script fn;
        yyjson_write_flag flg;
        usize depth;
        const char *expect;
    } flag_case;
    static const yyjson_write_flag P = YYJSON_WRITE_PRETTY;
    static const yyjson_write_flag T = YYJSON_WRITE_PRETTY_TWO_SPACES;
    static const yyjson_write_flag B = YYJSON_WRITE_PRETTY_TABS;
    static const yyjson_write_flag N = YYJSON_WRITE_NEWLINE_AT_END;
    static const flag_case cases[] = {
        { sw_basic, 0, 2, BASIC },
        { sw_basic, P, 2, BASIC_PRETTY4 },
        { sw_basic, T, 2, BASIC_PRETTY2 },
        { sw_basic, P | T, 2, BASIC_PRETTY2 },
        { sw_basic, B, 2, BASIC_PRETTY_TAB },
        { sw_basic, P | B, 2, BASIC_PRETTY_TAB },
        { sw_basic, T | B, 2, BASIC_PRETTY_TAB },
        { sw_basic, P | T | B, 2, BASIC_PRETTY_TAB },
        { sw_nest, P, 3,
          "[\n    [],\n    {},\n    [\n        []\n    ],\n    {\n"
          "        \"a\": {}\n    }\n]" },
        { sw_nest, T, 3,
          "[\n  [],\n  {},\n  [\n    []\n  ],\n  {\n    \"a\": {}\n  }\n]" },
        { sw_nest, B, 3,
          "[\n\t[],\n\t{},\n\t[\n\t\t[]\n\t],\n\t{\n\t\t\"a\": {}\n\t}\n]" },
        { sw_nums, YYJSON_WRITE_FP_TO_FIXED(2), 1,
          "[0,1,18446744073709551615,-1,-9223372036854775808,"
          "9223372036854775807,0.0,-0.0,1.5,1e100,1e21,123456789.0"
#if !YYJSON_DISABLE_FAST_FP_CONV
          ",0.0,0.1,-0.0,1.7976931348623157e308,3.14"
#endif
          "]" },
        { sw_strs, YYJSON_WRITE_ESCAPE_UNICODE, 1,
          "[\"\",\"a\\\"b\\\\c\\n\\r\\t\\b\\f/\",\"\\u0001\\u001F\x7f\","
          "\"\\u4E2D\\uD83D\\uDE00\\u00A2\",\"a\\u0000b\"]" },
        { sw_strs, YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_LOWERCASE_HEX, 1,
          "[\"\",\"a\\\"b\\\\c\\n\\r\\t\\b\\f/\",\"\\u0001\\u001f\x7f\","
          "\"\\u4e2d\\ud83d\\ude00\\u00a2\",\"a\\u0000b\"]" },
        { sw_strs, YYJSON_WRITE_ESCAPE_SLASHES, 1,
          "[\"\",\"a\\\"b\\\\c\\n\\r\\t\\b\\f\\/\",\"\\u0001\\u001F\x7f\","
          "\"\xe4\xb8\xad\xf0\x9f\x98\x80\xc2\xa2\",\"a\\u0000b\"]" },
        { sw_keys, YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_ESCAPE_SLASHES, 1,
          "{\"\":null,\"a\\\"\\\\\\n\\/\":null,\"k\\u0000l\":null,"
          "\"\\u4E2D\":null}" },
        { sw_nan, YYJSON_WRITE_INF_AND_NAN_AS_NULL, 1, "[null,null,null,1.5]" },
        { sw_nan, YYJSON_WRITE_INF_AND_NAN_AS_NULL | YYJSON_WRITE_ALLOW_INF_AND_NAN,
          1, "[null,null,null,1.5]" },
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        wr_check(cases[i].fn, cases[i].flg, cases[i].depth, cases[i].expect);
    }

    // a newline after the document
    {
        char pretty[sizeof(BASIC_PRETTY4) + 1], compact[sizeof(BASIC) + 1];
        snprintf(pretty, sizeof(pretty), "%s\n", BASIC_PRETTY4);
        snprintf(compact, sizeof(compact), "%s\n", BASIC);
        wr_check(sw_basic, N, 2, compact);
        wr_check(sw_basic, N | P, 2, pretty);
    }

    // a double that is too big for a float is an error
    each_dst(m) {
        wr_t w;
        wr_open(&w, *m, &(yyjson_sw_opts){ YYJSON_WRITE_FP_TO_FLOAT, 0, 0 }, 32, 1);
        yy_assert(yyjson_sw_arr_begin(&w.sw));
        yy_assert(yyjson_sw_write_real(&w.sw, 1.5));
        yy_assert(yyjson_sw_write_real(&w.sw, 0.1));
        yy_assert(!yyjson_sw_write_real(&w.sw, 1e40));
        expect_sw_err(&w.sw, YYJSON_WRITE_ERROR_NAN_OR_INF);
        wr_close(&w);
    }
}

// Invalid UTF-8 is an error, unless it is allowed.
static void test_sw_utf8(void) {
#if !YYJSON_DISABLE_NON_STANDARD && !YYJSON_DISABLE_UTF8_VALIDATION
    static const char bad[] = "a\xc0\xaf" "b";
    static const char copied[] = "{\"a\xc0\xaf" "b\":\"a\xc0\xaf" "b\"}";
    static const char escaped[] = "\"a\\uFFFD\\uFFFDb\"";
    each_dst(m) {
        wr_t w;
        yyjson_write_flag allow = YYJSON_WRITE_ALLOW_INVALID_UNICODE;

        wr_open(&w, *m, NULL, 64, 1);
        yy_assert(!yyjson_sw_write_str(&w.sw, bad));
        expect_sw_err(&w.sw, YYJSON_WRITE_ERROR_INVALID_STRING);
        wr_close(&w);

        wr_open(&w, *m, NULL, 64, 1);
        yy_assert(yyjson_sw_obj_begin(&w.sw));
        yy_assert(!yyjson_sw_write_key(&w.sw, bad));
        expect_sw_err(&w.sw, YYJSON_WRITE_ERROR_INVALID_STRING);
        wr_close(&w);

        // the bytes are copied
        wr_open(&w, *m, &(yyjson_sw_opts){ allow, 0, 0 }, 64, 1);
        yy_assert(yyjson_sw_obj_begin(&w.sw));
        yy_assert(yyjson_sw_write_key(&w.sw, bad));
        yy_assert(yyjson_sw_write_str(&w.sw, bad));
        yy_assert(yyjson_sw_obj_end(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        wr_expect(&w, copied);
        wr_close(&w);

        // when they are escaped, each is a replacement character
        wr_open(&w, *m, &(yyjson_sw_opts){ allow | YYJSON_WRITE_ESCAPE_UNICODE, 0, 0 },
                  64, 1);
        yy_assert(yyjson_sw_write_str(&w.sw, bad));
        yy_assert(yyjson_sw_finish(&w.sw));
        wr_expect(&w, escaped);
        wr_close(&w);
    }
#endif
}

// A document after another, or many of them.
static void test_sw_docs(void) {
    each_dst(m) {
        wr_t w;
        yyjson_sw *sw;

        wr_open(&w, *m, NULL, 64, 1);
        sw = &w.sw;
        yy_assert(yyjson_sw_write_uint(sw, 1));
        yy_assert(yyjson_sw_doc_next(sw));
        yy_assert(yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw));
        yy_assert(yyjson_sw_doc_next(sw));
        yy_assert(yyjson_sw_write_uint(sw, 2));
        yy_assert(yyjson_sw_finish(sw));
        wr_expect(&w, "1\n[]\n2");
        wr_close(&w);

        wr_open(&w, *m, &(yyjson_sw_opts){ YYJSON_WRITE_NEWLINE_AT_END, 0, 0 }, 64, 1);
        sw = &w.sw;
        yy_assert(yyjson_sw_write_null(sw));
        yy_assert(yyjson_sw_doc_next(sw));
        yy_assert(yyjson_sw_write_bool(sw, true));
        yy_assert(yyjson_sw_finish(sw));
        wr_expect(&w, "null\ntrue\n");
        wr_close(&w);

        // every document is pretty
        wr_open(&w, *m, &(yyjson_sw_opts){ YYJSON_WRITE_PRETTY_TWO_SPACES, 0, 0 }, 64,
                  2);
        sw = &w.sw;
        yy_assert(yyjson_sw_arr_begin(sw) && yyjson_sw_write_uint(sw, 1));
        yy_assert(yyjson_sw_arr_end(sw));
        yy_assert(yyjson_sw_doc_next(sw));
        yy_assert(yyjson_sw_obj_begin(sw) && yyjson_sw_write_key(sw, "a"));
        yy_assert(yyjson_sw_write_null(sw) && yyjson_sw_obj_end(sw));
        yy_assert(yyjson_sw_finish(sw));
        wr_expect(&w, "[\n  1\n]\n{\n  \"a\": null\n}");
        wr_close(&w);
    }
}

// A string or a key of 4000 bytes is the same as the DOM writer gives, and the
// invalid ones are rejected by both.
static void test_sw_str_bound(void) {
    static const yyjson_write_flag flags[] = {
        0, YYJSON_WRITE_ESCAPE_UNICODE, YYJSON_WRITE_ESCAPE_SLASHES,
        YYJSON_WRITE_ALLOW_INVALID_UNICODE,
        YYJSON_WRITE_ALLOW_INVALID_UNICODE | YYJSON_WRITE_ESCAPE_UNICODE |
        YYJSON_WRITE_LOWERCASE_HEX,
    };
    static const unsigned char pattern[] = {
        'a', '\n', '/', '\t', 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80,
        0xE0, 'a', 0xC2, 0xA2, 0x80, 0x80, 0x80, 0x80,
    };
    char src[4000];
    usize kind, f, i, key;
    for (kind = 0; kind < 4; kind++) {
        for (i = 0; i < sizeof(src); i++) {
            src[i] = kind == 0 ? (char)0x80 :
                     kind == 1 ? (char)pattern[i % sizeof(pattern)] :
                     kind == 2 ? (char)pattern[i % 11] : (char)i;
        }
        for (f = 0; f < yy_nelems(flags); f++) {
            yyjson_val val;
            usize len = 0;
            char *ref;
            unsafe_yyjson_set_strn(&val, src, sizeof(src));
            ref = yyjson_val_write(&val, flags[f], &len);
            for (key = 0; key < 2; key++) {
                sink_t expect = {0};
                usize cap = ref ? len + (key ? 8 : 0) : sizeof(src) * 6 + 16;
                if (ref) {
                    if (key) sink_puts(&expect, "{");
                    sink_put(&expect, ref, len);
                    if (key) sink_puts(&expect, ":null}");
                }
                each_dst(m) {
                    wr_t w;
                    bool ok;
                    wr_open(&w, *m, &(yyjson_sw_opts){ flags[f], 0, 0 }, cap, key);
                    if (key) yy_assert(yyjson_sw_obj_begin(&w.sw));
                    ok = key ? yyjson_sw_write_keyn(&w.sw, src, sizeof(src)) :
                               yyjson_sw_write_strn(&w.sw, src, sizeof(src));
                    yy_assert(ok == (ref != NULL));
                    if (ok) {
                        if (key) {
                            yy_assert(yyjson_sw_write_null(&w.sw));
                            yy_assert(yyjson_sw_obj_end(&w.sw));
                        }
                        yy_assert(yyjson_sw_finish(&w.sw));
                        wr_expect(&w, expect.dat);
                    } else {
                        expect_sw_err(&w.sw, YYJSON_WRITE_ERROR_INVALID_STRING);
                    }
                    wr_close(&w);
                }
                sink_free(&expect);
            }
            free(ref);
        }
    }
}



/*==============================================================================
 * MARK: - Error
 *============================================================================*/

// Expects the writer to have failed with `code`, then closes it.
static void wr_bad(wr_t *w, yyjson_write_code code) {
    expect_sw_err(&w->sw, code);
    wr_close(w);
}

// Expects that `call` fails on a new writer, in every kind of output, after
// `pre` has worked. A `code` of 0 accepts any error. `sw` is the writer.
#define expect_fail(code, pre, call) do { \
    each_dst(m_) { \
        wr_t w_; \
        yyjson_sw *sw = &w_.sw; \
        wr_open(&w_, *m_, NULL, 64, 2); \
        yy_assert(pre); \
        yy_assert(!(call)); \
        if (code) expect_sw_err(sw, code); \
        else yy_assert(yyjson_sw_has_err(sw)); \
        wr_close(&w_); \
    } \
} while (0)

static void test_sw_args(void) {
    static const yyjson_write_code PAR = YYJSON_WRITE_ERROR_INVALID_PARAMETER;
    static const yyjson_write_code TYPE = YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE;
    expect_fail(PAR, yyjson_sw_obj_begin(sw), yyjson_sw_write_key(sw, NULL));
    expect_fail(PAR, yyjson_sw_obj_begin(sw), yyjson_sw_write_keyn(sw, NULL, 0));
    expect_fail(0, yyjson_sw_obj_begin(sw), yyjson_sw_write_keyn(sw, "x", SIZE_MAX));
    expect_fail(PAR, true, yyjson_sw_write_str(sw, NULL));
    expect_fail(PAR, true, yyjson_sw_write_strn(sw, NULL, 0));
    expect_fail(0, true, yyjson_sw_write_strn(sw, "x", SIZE_MAX));
    expect_fail(PAR, true, yyjson_sw_write_raw(sw, NULL));
    expect_fail(PAR, true, yyjson_sw_write_rawn(sw, "", 0));
    expect_fail(PAR, true, yyjson_sw_write_scalar(sw, NULL));
    expect_fail(TYPE, true, yyjson_sw_write_num(sw, NULL));
}

// Numbers may be typed or raw. Other scalars cannot be written as numbers,
// and containers cannot be written as scalars.
static void test_sw_val_type(void) {
    static const yyjson_write_code TYPE = YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE;
    char raw_num[] = { '1', '2', '3' };
    yyjson_val raw = {0}, str = {0}, arr = {0}, obj = {0}, num = {0}, odd = {0};
    yy_assert(yyjson_set_raw(&raw, raw_num, sizeof(raw_num)));
    yy_assert(yyjson_set_str(&str, "s"));
    unsafe_yyjson_set_arr(&arr, 0);
    unsafe_yyjson_set_obj(&obj, 0);
    yy_assert(yyjson_set_uint(&num, 7));
    odd.tag = (u64)YYJSON_TYPE_NUM | (3 << 3); // not uint, sint or real

    expect_fail(TYPE, true, yyjson_sw_write_num(sw, &odd));
    expect_fail(TYPE, true, yyjson_sw_write_num(sw, &str));
    expect_fail(TYPE, true, yyjson_sw_write_num(sw, &arr));
    expect_fail(TYPE, true, yyjson_sw_write_scalar(sw, &arr));
    expect_fail(TYPE, true, yyjson_sw_write_scalar(sw, &obj));

    // the scalars, and a raw one that is not changed
    each_dst(m) {
        wr_t w;
        wr_open(&w, *m, NULL, 64, 2);
        yy_assert(yyjson_sw_arr_begin(&w.sw));
        yy_assert(yyjson_sw_write_scalar(&w.sw, &raw));
        yy_assert(yyjson_sw_write_scalar(&w.sw, &str));
        yy_assert(yyjson_sw_write_scalar(&w.sw, &num));
        yy_assert(yyjson_sw_write_num(&w.sw, &num));
        yy_assert(yyjson_sw_write_num(&w.sw, &raw));
        yy_assert(yyjson_sw_arr_end(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        wr_expect(&w, "[123,\"s\",7,7,123]");
        wr_close(&w);
    }
}

// The calls that are not allowed at the place they are made.
static void test_sw_state(void) {
    static const yyjson_write_code BAD = YYJSON_WRITE_ERROR_INVALID_PARAMETER;

    // a key outside of an object
    expect_fail(BAD, true, yyjson_sw_write_key(sw, "a"));
    expect_fail(BAD, yyjson_sw_arr_begin(sw), yyjson_sw_write_key(sw, "a"));

    // a value in an object without a key
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_write_null(sw));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_write_str(sw, "a"));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_write_uint(sw, 1));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_write_raw(sw, "1"));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_arr_begin(sw));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_obj_begin(sw));

    // two keys in a row, or a key at the end of an object
    expect_fail(BAD, yyjson_sw_obj_begin(sw) && yyjson_sw_write_key(sw, "a"),
                yyjson_sw_write_key(sw, "b"));
    expect_fail(BAD, yyjson_sw_obj_begin(sw) && yyjson_sw_write_key(sw, "a"),
                yyjson_sw_obj_end(sw));

    // a container closed by the wrong call, or not open
    expect_fail(BAD, yyjson_sw_arr_begin(sw), yyjson_sw_obj_end(sw));
    expect_fail(BAD, yyjson_sw_obj_begin(sw), yyjson_sw_arr_end(sw));
    expect_fail(BAD, true, yyjson_sw_arr_end(sw));
    expect_fail(BAD, true, yyjson_sw_obj_end(sw));

    // a second root value
    expect_fail(BAD, yyjson_sw_write_null(sw), yyjson_sw_write_null(sw));
    expect_fail(BAD, yyjson_sw_write_null(sw), yyjson_sw_arr_begin(sw));
    expect_fail(BAD, yyjson_sw_write_null(sw), yyjson_sw_write_key(sw, "a"));
    expect_fail(BAD, yyjson_sw_write_null(sw), yyjson_sw_write_str(sw, "a"));

    // a next document in a wrong place
    expect_fail(BAD, true, yyjson_sw_doc_next(sw));
    expect_fail(BAD, yyjson_sw_arr_begin(sw), yyjson_sw_doc_next(sw));
    expect_fail(BAD, yyjson_sw_write_null(sw) && yyjson_sw_finish(sw),
                yyjson_sw_doc_next(sw));
    expect_fail(BAD, yyjson_sw_write_null(sw) && yyjson_sw_doc_next(sw),
                yyjson_sw_doc_next(sw));

    // a finish that is too early
    expect_fail(BAD, true, yyjson_sw_finish(sw));
    expect_fail(BAD, yyjson_sw_arr_begin(sw), yyjson_sw_finish(sw));
    expect_fail(BAD, yyjson_sw_obj_begin(sw) && yyjson_sw_write_key(sw, "a"),
                yyjson_sw_finish(sw));
    expect_fail(BAD, yyjson_sw_write_null(sw) && yyjson_sw_doc_next(sw),
                yyjson_sw_finish(sw));

    // a finish is final: no more writes, and a finish again is the same
#define after_finish(call) \
    expect_fail(BAD, yyjson_sw_write_null(sw) && yyjson_sw_finish(sw), call)
    after_finish(yyjson_sw_write_str(sw, "a"));
    after_finish(yyjson_sw_write_strn(sw, "a", 1));
    after_finish(yyjson_sw_write_uint(sw, 1));
    after_finish(yyjson_sw_write_real(sw, 1.5));
    after_finish(yyjson_sw_write_raw(sw, "1"));
    after_finish(yyjson_sw_write_key(sw, "a"));
    after_finish(yyjson_sw_arr_begin(sw));
    after_finish(yyjson_sw_obj_begin(sw));
#undef after_finish
    each_dst(m) {
        wr_t w;
        wr_open(&w, *m, &(yyjson_sw_opts){ YYJSON_WRITE_NEWLINE_AT_END, 0, 0 }, 64, 2);
        yy_assert(yyjson_sw_write_null(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        yy_assert(yyjson_sw_len(&w.sw) == 5);
        yy_assert(!yyjson_sw_write_null(&w.sw));
        wr_bad(&w, BAD);
    }
}

static void test_sw_nan(void) {
    static const yyjson_write_code NAN_INF = YYJSON_WRITE_ERROR_NAN_OR_INF;
    static const f64 bad[] = { INFINITY, -INFINITY, NAN, -NAN, HUGE_VAL };
    usize i;
    each_dst(m) {
        for (i = 0; i < yy_nelems(bad); i++) {
            wr_t w;
            yyjson_val num;
            yyjson_write_flag flg[] = {
                0, YYJSON_WRITE_ALLOW_INF_AND_NAN, YYJSON_WRITE_ESCAPE_UNICODE,
            };
            usize f;
            for (f = 0; f < yy_nelems(flg); f++) {
                // never a literal, with or without the flag of the DOM
                wr_open(&w, *m, &(yyjson_sw_opts){ flg[f], 0, 0 }, 64, 1);
                yy_assert(yyjson_sw_arr_begin(&w.sw));
                yy_assert(!yyjson_sw_write_real(&w.sw, bad[i]));
                wr_bad(&w, NAN_INF);

                unsafe_yyjson_set_real(&num, bad[i]);
                wr_open(&w, *m, &(yyjson_sw_opts){ flg[f], 0, 0 }, 64, 1);
                yy_assert(!yyjson_sw_write_num(&w.sw, &num));
                wr_bad(&w, NAN_INF);
                wr_open(&w, *m, &(yyjson_sw_opts){ flg[f], 0, 0 }, 64, 1);
                yy_assert(!yyjson_sw_write_scalar(&w.sw, &num));
                wr_bad(&w, NAN_INF);
            }
        }
    }
}

// The first error stays, the calls that follow do nothing.
static void test_sw_sticky(void) {
    each_dst(m) {
        wr_t w;
        yyjson_sw *sw;
        yyjson_write_err err1, err2;
        yyjson_val num = {0};
        u64 len;

        wr_open(&w, *m, NULL, 64, 4);
        sw = &w.sw;
        yy_assert(yyjson_sw_obj_begin(sw));
        yy_assert(yyjson_sw_write_key(sw, "k"));
        yy_assert(yyjson_sw_write_uint(sw, 1));
        yy_assert(!yyjson_sw_write_key(sw, NULL));
        expect_sw_err(sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        yy_assert(yyjson_sw_get_err(sw, &err1) ==
                  YYJSON_WRITE_ERROR_INVALID_PARAMETER);
        len = yyjson_sw_len(sw);

        // a valid call fails, and an invalid one does not replace the error
        yy_assert(yyjson_set_uint(&num, 2));
        yy_assert(!yyjson_sw_write_key(sw, "a"));
        yy_assert(!yyjson_sw_write_uint(sw, 1));
        yy_assert(!yyjson_sw_write_sint(sw, -1));
        yy_assert(!yyjson_sw_write_real(sw, NAN));
        yy_assert(!yyjson_sw_write_num(sw, &num));
        yy_assert(!yyjson_sw_write_scalar(sw, &num));
        yy_assert(!yyjson_sw_write_str(sw, "x"));
        yy_assert(!yyjson_sw_write_raw(sw, "1"));
        yy_assert(!yyjson_sw_write_bool(sw, true));
        yy_assert(!yyjson_sw_write_null(sw));
        yy_assert(!yyjson_sw_arr_begin(sw));
        yy_assert(!yyjson_sw_obj_begin(sw));
        yy_assert(!yyjson_sw_arr_end(sw));
        yy_assert(!yyjson_sw_obj_end(sw));
        yy_assert(!yyjson_sw_doc_next(sw));
        yy_assert(!yyjson_sw_flush(sw));
        yy_assert(!yyjson_sw_finish(sw));
        yy_assert(yyjson_sw_get_err(sw, &err2) == err1.code);
        yy_assert(err2.code == err1.code && err2.msg == err1.msg);
        yy_assert(yyjson_sw_len(sw) == len && yyjson_sw_depth(sw) == 1);
        wr_close(&w);
    }
}



/*==============================================================================
 * MARK: - Depth
 *============================================================================*/

static void test_sw_depth(void) {
    each_dst(m) {
        wr_t w;
        yyjson_sw *sw;
        wr_open(&w, *m, NULL, 64, 3);
        sw = &w.sw;
        yy_assert(yyjson_sw_depth(sw) == 0);
        yy_assert(yyjson_sw_obj_begin(sw) && yyjson_sw_depth(sw) == 1);
        yy_assert(yyjson_sw_write_key(sw, "a") && yyjson_sw_depth(sw) == 1);
        yy_assert(yyjson_sw_arr_begin(sw) && yyjson_sw_depth(sw) == 2);
        yy_assert(yyjson_sw_write_null(sw) && yyjson_sw_depth(sw) == 2);
        yy_assert(yyjson_sw_arr_begin(sw) && yyjson_sw_depth(sw) == 3);
        yy_assert(yyjson_sw_arr_end(sw) && yyjson_sw_depth(sw) == 2);
        yy_assert(yyjson_sw_arr_end(sw) && yyjson_sw_depth(sw) == 1);
        yy_assert(yyjson_sw_write_key(sw, "b"));
        yy_assert(yyjson_sw_obj_begin(sw) && yyjson_sw_obj_end(sw));
        yy_assert(yyjson_sw_depth(sw) == 1);
        yy_assert(yyjson_sw_obj_end(sw) && yyjson_sw_depth(sw) == 0);
        yy_assert(yyjson_sw_finish(sw) && yyjson_sw_depth(sw) == 0);
        wr_close(&w);
    }
}

static void test_sw_max_depth(void) {
    usize obj, lim, depth;
    for (obj = 0; obj < 2; obj++) {
        for (lim = 1; lim <= 4; lim++) {
            for (depth = 1; depth <= 6; depth++) {
                each_dst(m) {
                    wr_t w;
                    yyjson_sw *sw;
                    usize i;
                    bool ok = true;
                    wr_open(&w, *m, &(yyjson_sw_opts){ 0, lim, 0 }, 128, 6);
                    sw = &w.sw;
                    for (i = 0; i < depth && ok; i++) {
                        if (i && obj) ok = yyjson_sw_write_key(sw, "a");
                        ok = ok && (obj ? yyjson_sw_obj_begin(sw) :
                                          yyjson_sw_arr_begin(sw));
                    }
                    if (depth <= lim) {
                        yy_assert(ok);
                        expect_sw_ok(sw);
                        wr_close(&w);
                    } else {
                        yy_assert(!ok);
                        yy_assert(yyjson_sw_depth(sw) == lim);
                        wr_bad(&w, YYJSON_WRITE_ERROR_DEPTH);
                    }
                }
            }
        }
    }
}

// A document that is deeper than a buffer, or a window.
static void test_sw_deep(void) {
    enum { depth = 20000 };
    char *expect;
    if (!SW_DEPTH_OK(depth)) return;
    expect = (char *)malloc(depth * 2 + 8);
    yy_assert(expect != NULL);
    memset(expect, '[', depth);
    memcpy(expect + depth, "null", 4);
    memset(expect + depth + 4, ']', depth);
    expect[depth * 2 + 4] = '\0';

    each_dst(m) {
        wr_t w;
        usize i;
        wr_open(&w, *m, &(yyjson_sw_opts){ 0, depth, 0 }, depth * 2 + 8, depth);
        for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_begin(&w.sw));
        yy_assert(yyjson_sw_depth(&w.sw) == depth);
        yy_assert(yyjson_sw_write_null(&w.sw));
        for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_end(&w.sw));
        yy_assert(yyjson_sw_finish(&w.sw));
        wr_expect(&w, expect);
        wr_close(&w);
    }
    free(expect);
}

// The nesting that needs the indent of many spaces, longer than the pieces
// that the writer copies. The DOM writer, which builds the expected text,
// rejects a document at the compile-time depth limit, so the shape stays
// one level inside that limit.
static void test_sw_deep_pretty(void) {
    usize steps = 69; /* root array, then an object and an array each step */
    usize depth, f;
    static const yyjson_write_flag flags[] = {
        YYJSON_WRITE_PRETTY, YYJSON_WRITE_PRETTY_TWO_SPACES,
        YYJSON_WRITE_PRETTY_TABS,
    };
#if YYJSON_WRITER_DEPTH_LIMIT
    if ((usize)YYJSON_WRITER_DEPTH_LIMIT < 2) return;
    if (1 + 2 * steps >= (usize)YYJSON_WRITER_DEPTH_LIMIT) {
        steps = ((usize)YYJSON_WRITER_DEPTH_LIMIT - 2) / 2;
    }
#endif
    depth = 1 + 2 * steps;
    for (f = 0; f < yy_nelems(flags); f++) {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *root = yyjson_mut_arr(doc), *cur = root;
        char *expect;
        usize i;
        yyjson_mut_doc_set_root(doc, root);
        for (i = 0; i < steps; i++) {
            yyjson_mut_val *obj = yyjson_mut_obj(doc);
            yyjson_mut_val *arr = yyjson_mut_arr(doc);
            yy_assert(yyjson_mut_arr_add_str(doc, cur, "s"));
            yy_assert(yyjson_mut_arr_append(cur, obj));
            yy_assert(yyjson_mut_obj_add_val(doc, obj, "k", arr));
            cur = arr;
        }
        expect = yyjson_mut_write(doc, flags[f], NULL);
        yy_assert(expect != NULL);

        each_dst(m) {
            wr_t w;
            wr_open(&w, *m, &(yyjson_sw_opts){ flags[f], 0, 0 },
                      strlen(expect) + 2 * depth, depth);
            for (i = 0; i < steps; i++) {
                yy_assert(yyjson_sw_arr_begin(&w.sw));
                yy_assert(yyjson_sw_write_str(&w.sw, "s"));
                yy_assert(yyjson_sw_obj_begin(&w.sw));
                yy_assert(yyjson_sw_write_key(&w.sw, "k"));
            }
            yy_assert(yyjson_sw_arr_begin(&w.sw));
            for (i = 0; i < steps; i++) {
                yy_assert(yyjson_sw_arr_end(&w.sw));
                yy_assert(yyjson_sw_obj_end(&w.sw));
            }
            yy_assert(yyjson_sw_arr_end(&w.sw));
            yy_assert(yyjson_sw_finish(&w.sw));
            wr_expect(&w, expect);
            wr_close(&w);
        }
        free(expect);
        yyjson_mut_doc_free(doc);
    }
}



/*==============================================================================
 * MARK: - Memory
 *============================================================================*/

// A document, and the smallest buffer that it fits. A buffer holds the
// output and the stack of the open containers, an open or a close needs the
// stack byte of its container.
typedef struct {
    sw_script fn;
    yyjson_write_flag flg;
    usize min_cap;
    const char *expect;
} fit_case;

static bool sw_null(yyjson_sw *sw) {
    return yyjson_sw_write_null(sw);
}

static bool sw_empty(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw);
}

static bool sw_one(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) && yyjson_sw_write_null(sw) &&
           yyjson_sw_arr_end(sw);
}

static bool sw_comma(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) && yyjson_sw_write_uint(sw, 1) &&
           yyjson_sw_arr_begin(sw) && yyjson_sw_arr_end(sw) &&
           yyjson_sw_arr_end(sw);
}

static bool sw_member(yyjson_sw *sw) {
    return yyjson_sw_obj_begin(sw) && yyjson_sw_write_key(sw, "a") &&
           yyjson_sw_write_uint(sw, 1) && yyjson_sw_obj_end(sw);
}

static bool sw_triple(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) && yyjson_sw_arr_begin(sw) &&
           yyjson_sw_arr_begin(sw) && yyjson_sw_write_null(sw) &&
           yyjson_sw_arr_end(sw) && yyjson_sw_arr_end(sw) &&
           yyjson_sw_arr_end(sw);
}

static bool sw_two_strs(yyjson_sw *sw) {
    return yyjson_sw_arr_begin(sw) && yyjson_sw_write_str(sw, "1234567890") &&
           yyjson_sw_write_str(sw, "a\n") && yyjson_sw_arr_end(sw);
}

static bool sw_long_str(yyjson_sw *sw) {
    static const char str[] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaa";
    return yyjson_sw_write_str(sw, str);
}

static bool sw_escapes(yyjson_sw *sw) {
    return yyjson_sw_write_str(sw, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
}

static void test_sw_fit(void) {
    static const fit_case cases[] = {
        { sw_null, 0, 4, "null" },
        { sw_empty, 0, 3, "[]" },
        { sw_one, 0, 7, "[null]" },
        { sw_comma, 0, 7, "[1,[]]" },
        { sw_member, 0, 8, "{\"a\":1}" },
        { sw_triple, 0, 11, "[[[null]]]" },
        { sw_two_strs, 0, 21, "[\"1234567890\",\"a\\n\"]" },
        { sw_long_str, 0, 82,
          "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
          "aaaaaaaaaaaaaaa\"" },
        { sw_escapes, 0, 42,
          "\"\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\\n\"" },
        { sw_one, YYJSON_WRITE_PRETTY, 13, "[\n    null\n]" },
        { sw_one, YYJSON_WRITE_PRETTY_TWO_SPACES, 11, "[\n  null\n]" },
        { sw_one, YYJSON_WRITE_PRETTY_TABS, 10, "[\n\tnull\n]" },
        { sw_one, YYJSON_WRITE_NEWLINE_AT_END, 7, "[null]\n" },
        { sw_null, YYJSON_WRITE_NEWLINE_AT_END, 5, "null\n" },
    };
    usize i, cap;
    for (i = 0; i < yy_nelems(cases); i++) {
        for (cap = cases[i].min_cap - 1; cap <= cases[i].min_cap + 2; cap++) {
            wr_t w;
            bool ok;
            wr_open(&w, dst_modes[0], &(yyjson_sw_opts){ cases[i].flg, 0, 0 }, cap, 0);
            ok = cases[i].fn(&w.sw) && yyjson_sw_finish(&w.sw);
            if (cap < cases[i].min_cap) {
                yy_assert(!ok);
                wr_bad(&w, YYJSON_WRITE_ERROR_BUFFER_LIMIT);
            } else {
                yy_assert(ok);
                wr_expect(&w, cases[i].expect);
                wr_close(&w);
            }
        }
    }
}



/*==============================================================================
 * MARK: - Callback
 *============================================================================*/

// Flush pushes the buffered bytes, it does not change the length.
static void test_sw_flush(void) {
    sink_t out = {0};
    yyjson_sw sw = {0};
    char buf[64];
    usize calls;

    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, NULL, NULL));
    yy_assert(yyjson_sw_obj_begin(&sw));
    yy_assert(yyjson_sw_write_key(&sw, "a"));
    yy_assert(yyjson_sw_write_uint(&sw, 1));
    // `{"a":1` is in the window, nothing is pushed yet
    yy_assert(out.calls == 0 && yyjson_sw_len(&sw) == 6);
    yy_assert(yyjson_sw_flush(&sw));
    yy_assert(out.calls == 1 && out.len == 6 && yyjson_sw_len(&sw) == 6);

    // nothing to push, no call with zero bytes
    calls = out.calls;
    yy_assert(yyjson_sw_flush(&sw));
    yy_assert(out.calls == calls && yyjson_sw_len(&sw) == 6);

    // the document goes on
    yy_assert(yyjson_sw_write_key(&sw, "b"));
    yy_assert(yyjson_sw_arr_begin(&sw));
    yy_assert(yyjson_sw_write_str(&sw, "x"));
    yy_assert(yyjson_sw_flush(&sw));
    yy_assert(yyjson_sw_write_str(&sw, "y"));
    yy_assert(yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_obj_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(out.len == yyjson_sw_len(&sw));
    yy_assert(!strcmp(out.dat, "{\"a\":1,\"b\":[\"x\",\"y\"]}"));
    yyjson_sw_free(&sw);
    sink_free(&out);

    // a small document is pushed once, by the finish
    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, NULL, NULL));
    yy_assert(yyjson_sw_arr_begin(&sw) && yyjson_sw_arr_end(&sw));
    yy_assert(out.calls == 0);
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(out.calls == 1 && out.len == 2);
    yyjson_sw_free(&sw);
    sink_free(&out);

    // memory has nowhere to push to
    yy_assert(yyjson_sw_init_mem(&sw, buf, sizeof(buf), NULL));
    yy_assert(yyjson_sw_arr_begin(&sw));
    yy_assert(yyjson_sw_flush(&sw));
    yy_assert(yyjson_sw_len(&sw) == 1);
    yy_assert(yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(yyjson_sw_len(&sw) == 2 && !memcmp(buf, "[]", 2));
    yyjson_sw_free(&sw);
}

// Sets `n` bytes of a string that has escapes, and the escaped text of it.
static void str_make(sink_t *src, sink_t *ref, usize kind, usize n, bool esc) {
    static const char *units[] = {
        "x", "a\n\t\"\\", "A\xe4\xb8\xad\xf0\x9f\x98\x80",
    };
    static const char *refs[2][3] = {
        { "x", "a\\n\\t\\\"\\\\", "A\xe4\xb8\xad\xf0\x9f\x98\x80" },
        { "x", "a\\n\\t\\\"\\\\", "A\\u4E2D\\uD83D\\uDE00" },
    };
    usize i;
    for (i = 0; i < n; i++) {
        sink_puts(src, units[kind]);
        sink_puts(ref, refs[esc][kind]);
    }
}

// A string, a key, or a raw value that is much longer than the window is
// written in pieces, and does not change.
static void test_sw_long(void) {
    static const usize counts[] = { 0, 1, 2, 3, 10, 31, 64, 129, 500, 2000 };
    usize kind, i, f;
    for (kind = 0; kind < 3; kind++) {
        for (i = 0; i < yy_nelems(counts); i++) {
            for (f = 0; f < 2; f++) {
                sink_t src = {0}, ref = {0}, expect = {0};
                yyjson_write_flag flg = f ? YYJSON_WRITE_ESCAPE_UNICODE : 0;
                str_make(&src, &ref, kind, counts[i], f != 0);
                if (!src.dat) sink_puts(&src, "");
                if (!ref.dat) sink_puts(&ref, "");

                // a string
                sink_puts(&expect, "\"");
                sink_puts(&expect, ref.dat);
                sink_puts(&expect, "\"");
                each_dst(m) {
                    wr_t w;
                    wr_open(&w, *m, &(yyjson_sw_opts){ flg, 0, 0 }, expect.len, 0);
                    yy_assert(yyjson_sw_write_strn(&w.sw, src.dat, src.len));
                    yy_assert(yyjson_sw_finish(&w.sw));
                    wr_expect(&w, expect.dat);
                    wr_close(&w);
                }

                // a key
                expect.len = 0;
                sink_puts(&expect, "{\"");
                sink_puts(&expect, ref.dat);
                sink_puts(&expect, "\":[]}");
                each_dst(m) {
                    wr_t w;
                    wr_open(&w, *m, &(yyjson_sw_opts){ flg, 0, 0 }, expect.len + 1, 2);
                    yy_assert(yyjson_sw_obj_begin(&w.sw));
                    yy_assert(yyjson_sw_write_keyn(&w.sw, src.dat, src.len));
                    yy_assert(yyjson_sw_arr_begin(&w.sw));
                    yy_assert(yyjson_sw_arr_end(&w.sw));
                    yy_assert(yyjson_sw_obj_end(&w.sw));
                    yy_assert(yyjson_sw_finish(&w.sw));
                    wr_expect(&w, expect.dat);
                    wr_close(&w);
                }

                // a raw value is not escaped
                if (f == 0 && src.len) {
                    expect.len = 0;
                    sink_puts(&expect, "[");
                    sink_puts(&expect, src.dat);
                    sink_puts(&expect, "]");
                    each_dst(m) {
                        wr_t w;
                        wr_open(&w, *m, NULL, expect.len + 1, 1);
                        yy_assert(yyjson_sw_arr_begin(&w.sw));
                        yy_assert(yyjson_sw_write_rawn(&w.sw, src.dat, src.len));
                        yy_assert(yyjson_sw_arr_end(&w.sw));
                        yy_assert(yyjson_sw_finish(&w.sw));
                        wr_expect(&w, expect.dat);
                        wr_close(&w);
                    }
                }
                sink_free(&src);
                sink_free(&ref);
                sink_free(&expect);
            }
        }
    }
}

// The window: no chunk is larger, and a larger window has less calls.
static void test_sw_win_size(void) {
    static const usize wins[] = { 5, 6, 7, 100, 128, 4096, 16384, 70000 };
    sink_t src = {0}, ref = {0}, expect = {0};
    usize i, prev = (usize)-1;
    str_make(&src, &ref, 1, 1000, false);
    sink_puts(&expect, "[\"");
    sink_puts(&expect, ref.dat);
    sink_puts(&expect, "\",1,2,3,4,5,6,7,8,9]");

    for (i = 0; i < yy_nelems(wins); i++) {
        sink_t out = {0};
        yyjson_sw sw = {0};
        usize k;
        yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
                  &(yyjson_sw_opts){ 0, 0, wins[i] }, NULL));
        yy_assert(yyjson_sw_arr_begin(&sw));
        yy_assert(yyjson_sw_write_strn(&sw, src.dat, src.len));
        for (k = 1; k < 10; k++) yy_assert(yyjson_sw_write_uint(&sw, k));
        yy_assert(yyjson_sw_arr_end(&sw));
        yy_assert(yyjson_sw_finish(&sw));
        yy_assert(out.len == expect.len && !memcmp(out.dat, expect.dat, out.len));
        yy_assert(out.max_chunk <= wins[i]);
        yy_assert(out.calls <= prev);
        prev = out.calls;
        yyjson_sw_free(&sw);
        sink_free(&out);
    }
    yy_assert(prev == 1);

    sink_free(&src);
    sink_free(&ref);
    sink_free(&expect);
}

// Options are applied before allocating the output buffer.
static void test_sw_options(void) {
    sink_t out = {0};
    yyjson_sw sw;
    yyjson_sw_opts opts = {0};
    pool_t pool;
    test_alc ctx;
    yyjson_alc alc;
    char buf[64];

    yy_assert(yyjson_sw_init_mem(&sw, buf, sizeof(buf), &opts));
    yy_assert(sw.max_depth == (YYJSON_WRITER_DEPTH_LIMIT ?
                               YYJSON_WRITER_DEPTH_LIMIT : SIZE_MAX));
    yy_assert(yyjson_sw_write_null(&sw) && yyjson_sw_finish(&sw));
    yyjson_sw_free(&sw);
    opts.max_depth = SIZE_MAX;
    yy_assert(yyjson_sw_init_mem(&sw, buf, sizeof(buf), &opts));
    yy_assert(sw.max_depth == (YYJSON_WRITER_DEPTH_LIMIT ?
                               YYJSON_WRITER_DEPTH_LIMIT : SIZE_MAX));
    yyjson_sw_free(&sw);
#if YYJSON_WRITER_DEPTH_LIMIT
    opts.max_depth = (usize)YYJSON_WRITER_DEPTH_LIMIT + 1;
    yy_assert(yyjson_sw_init_mem(&sw, buf, sizeof(buf), &opts));
    yy_assert(sw.max_depth == (usize)YYJSON_WRITER_DEPTH_LIMIT);
    yyjson_sw_free(&sw);
#endif
    opts.max_depth = 0;
    opts.win_size = 4;
    yy_assert(!yyjson_sw_init_cb(&sw, sink_write, &out, &opts, NULL));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    yyjson_sw_free(&sw);

    opts.win_size = (usize)1 << 20;
    test_alc_init(&alc, &ctx, NULL, 1, SIZE_MAX);
    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, &opts, &alc));
    yy_assert(ctx.calls == 1);
    yy_assert(yyjson_sw_arr_begin(&sw) && yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yyjson_sw_free(&sw);
    sink_free(&out);
    test_alc_init(&alc, &ctx, NULL, 1, 1024);
    yy_assert(!yyjson_sw_init_cb(&sw, sink_write, &out, &opts, &alc));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
    yyjson_sw_free(&sw);

    // A pool makes the window an upper bound.
    pool_init(&pool, 1024, 0);
    opts.win_size = 4096;
    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, &opts, &pool.alc));
    yy_assert(yyjson_sw_arr_begin(&sw) && yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(out.len == 2);
    yyjson_sw_free(&sw);
    pool_free(&pool);
    sink_free(&out);
}



/*==============================================================================
 * MARK: - I/O
 *============================================================================*/

// A document with pieces that go through the window, and straight to the
// callback.
static bool sw_big(yyjson_sw *sw) {
    static const char raw[] =
        "{\"r\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,"
        "25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47]}";
    char str[300];
    usize i;
    memset(str, 'x', sizeof(str));
    str[10] = '\n';
    if (!yyjson_sw_arr_begin(sw)) return false;
    for (i = 0; i < 20; i++) {
        if (!yyjson_sw_write_strn(sw, str, sizeof(str)) ||
            !yyjson_sw_write_rawn(sw, raw, sizeof(raw) - 1) ||
            !yyjson_sw_write_uint(sw, i) ||
            !yyjson_sw_arr_begin(sw) || !yyjson_sw_arr_end(sw)) return false;
    }
    return yyjson_sw_arr_end(sw);
}

// A callback that fails at any call: an error, and nothing after it.
static void test_sw_io(void) {
    static const usize wins[] = { 0, 5, 64 };
    usize w, done_calls = 0;
    for (w = 0; w < yy_nelems(wins); w++) {
        bool done = false;
        usize left;
        for (left = 0; !done; left++) {
            yyjson_sw sw = {0};
            fail_t fail = { left };
            yy_assert(yyjson_sw_init_cb(&sw, fail_write, &fail,
                      &(yyjson_sw_opts){ 0, 0, wins[w] }, NULL));
            if (sw_big(&sw) && yyjson_sw_finish(&sw)) {
                expect_sw_ok(&sw);
                done = true;
                done_calls = left;
            } else {
                expect_sw_err(&sw, YYJSON_WRITE_ERROR_STREAMING_IO);
                yy_assert(!yyjson_sw_flush(&sw));
                yy_assert(!yyjson_sw_finish(&sw));
                yy_assert(fail.left == 0);
            }
            yyjson_sw_free(&sw);
            yy_assert(left < 10000);
        }
    }
    yy_assert(done_calls > 0);

    // the first push works, the second does not
    {
        yyjson_sw sw = {0};
        fail_t fail = { 1 };
        yy_assert(yyjson_sw_init_cb(&sw, fail_write, &fail, NULL, NULL));
        yy_assert(yyjson_sw_write_null(&sw));
        yy_assert(yyjson_sw_flush(&sw));
        yy_assert(yyjson_sw_doc_next(&sw));
        yy_assert(yyjson_sw_write_null(&sw));
        yy_assert(!yyjson_sw_finish(&sw));
        expect_sw_err(&sw, YYJSON_WRITE_ERROR_STREAMING_IO);
        yyjson_sw_free(&sw);
    }
}



/*==============================================================================
 * MARK: - Pool
 *============================================================================*/

// A pool that is much smaller than the guaranteed size, writes a small
// document through a small window.
static void test_sw_pool_min(void) {
    static const usize wins[] = { 0, 5, 31 };
    static const char *expects[] = {
        "[false,null,-9223372036854775808,1.25,\"a\\n\",[]]",
        "[\n    false,\n    null,\n    -9223372036854775808,\n"
        "    1.25,\n    \"a\\n\",\n    []\n]",
    };
    static const yyjson_write_flag flags[] = { 0, YYJSON_WRITE_PRETTY };
    usize off, w, f;
    yy_assert(YYJSON_SW_POOL_SIZE(0, 0) == YYJSON_STREAMING_POOL_SIZE_MIN);
    for (off = 0; off < 16; off++) {
        for (w = 0; w < yy_nelems(wins); w++) {
            for (f = 0; f < yy_nelems(flags); f++) {
                pool_t pool;
                sink_t out = {0};
                yyjson_sw sw = {0};
                pool_init(&pool, YYJSON_STREAMING_POOL_SIZE_MIN, off);
                yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
                          &(yyjson_sw_opts){ flags[f], 0, wins[w] }, &pool.alc));
                yy_assert(yyjson_sw_arr_begin(&sw));
                yy_assert(yyjson_sw_write_bool(&sw, false));
                yy_assert(yyjson_sw_write_null(&sw));
                yy_assert(yyjson_sw_write_sint(&sw, INT64_MIN));
                yy_assert(yyjson_sw_write_real(&sw, 1.25));
                yy_assert(yyjson_sw_write_str(&sw, "a\n"));
                yy_assert(yyjson_sw_arr_begin(&sw));
                yy_assert(yyjson_sw_arr_end(&sw));
                yy_assert(yyjson_sw_arr_end(&sw));
                yy_assert(yyjson_sw_finish(&sw));
                yy_assert(!strcmp(out.dat, expects[f]));
                if (wins[w]) yy_assert(out.max_chunk <= wins[w]);
                yyjson_sw_free(&sw);
                sink_free(&out);
                pool_free(&pool);
            }
        }
    }
}

// The guaranteed size takes any depth and window, with a long string.
static void test_sw_pool_guarantee(void) {
    static const usize depths[] = { 0, 1, 257, 1025 };
    static const usize wins[] = { 0, 5, 7, 127, 128, 1024, 16384 };
    char src[4096];
    usize d, w, i;
    memset(src, '\n', sizeof(src));
    for (d = 0; d < yy_nelems(depths); d++) {
        usize depth = depths[d];
        if (!SW_DEPTH_OK(depth)) continue;
        for (w = 0; w < yy_nelems(wins); w++) {
            usize size = YYJSON_SW_POOL_SIZE(wins[w], depth);
            usize len = depth * 2 + sizeof(src) * 2 + 2;
            pool_t pool;
            sink_t out = {0};
            yyjson_sw sw = {0};
            pool_init(&pool, size, w * 5 % 16);
            yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
                      &(yyjson_sw_opts){ 0, depth, wins[w] }, &pool.alc));
            for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_begin(&sw));
            yy_assert(yyjson_sw_write_strn(&sw, src, sizeof(src)));
            for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_end(&sw));
            yy_assert(yyjson_sw_finish(&sw));
            yy_assert(out.max_chunk <= (wins[w] ? wins[w] : size));
            yy_assert(out.len == len);
            for (i = 0; i < depth; i++) {
                yy_assert(out.dat[i] == '[' && out.dat[len - 1 - i] == ']');
            }
            yy_assert(out.dat[depth] == '"' && out.dat[len - depth - 1] == '"');
            for (i = 0; i < sizeof(src); i++) {
                yy_assert(!memcmp(out.dat + depth + 1 + i * 2, "\\n", 2));
            }
            yyjson_sw_free(&sw);
            sink_free(&out);
            pool_free(&pool);
        }
    }
}

// A pool that is too small for the depth is a limit error, no crash.
static void test_sw_pool_limit(void) {
    static const usize depths[] = { 300, 1000 };
    usize d, i;
    for (d = 0; d < yy_nelems(depths); d++) {
        usize depth = depths[d];
        pool_t pool;
        sink_t out = {0};
        yyjson_sw sw = {0};
        bool ok = true;
        if (!SW_DEPTH_OK(depth)) continue;
        pool_init(&pool, YYJSON_SW_POOL_SIZE(0, depth / 4), d);
        yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
                  &(yyjson_sw_opts){ 0, depth, 0 }, &pool.alc));
        for (i = 0; i < depth && ok; i++) ok = yyjson_sw_arr_begin(&sw);
        yy_assert(!ok);
        expect_sw_err(&sw, YYJSON_WRITE_ERROR_BUFFER_LIMIT);
        yy_assert(!yyjson_sw_write_null(&sw) && !yyjson_sw_finish(&sw));
        expect_sw_err(&sw, YYJSON_WRITE_ERROR_BUFFER_LIMIT);
        yyjson_sw_free(&sw);
        sink_free(&out);
        pool_free(&pool);
    }
}

// A pool that is wrapped by an allocator is an ordinary allocator, it grows.
static void test_sw_pool_wrapped(void) {
    enum { depth = 20000 };
    yyjson_alc pool, alc;
    test_alc ctx;
    sink_t out = {0};
    yyjson_sw sw = {0};
    char *mem;
    usize i;
    if (!SW_DEPTH_OK(depth)) return;
    mem = (char *)malloc(1 << 20);
    yy_assert(mem != NULL);
    yy_assert(yyjson_alc_pool_init(&pool, mem, 1 << 20));
    test_alc_init(&alc, &ctx, &pool, SIZE_MAX, SIZE_MAX);
    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
              &(yyjson_sw_opts){ 0, depth, 0 }, &alc));
    for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_begin(&sw));
    for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(out.len == depth * 2 && ctx.calls > 1);
    yyjson_sw_free(&sw);
    sink_free(&out);
    free(mem);
}

// The window is at most the default, also with a pool that is much larger.
static void test_sw_pool_win(void) {
    usize raw;
    for (raw = 0; raw < 2; raw++) {
        pool_t pool;
        sink_t out = {0};
        yyjson_sw sw = {0};
        char *src = (char *)malloc(40000);
        yy_assert(src != NULL);
        memset(src, 'a', 40000);
        pool_init(&pool, 65536, 0);
        yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out, NULL, &pool.alc));
        yy_assert(raw ? yyjson_sw_write_rawn(&sw, src, 40000) :
                        yyjson_sw_write_strn(&sw, src, 40000));
        yy_assert(yyjson_sw_finish(&sw));
        yy_assert(out.max_chunk <= 16384);
        yy_assert(out.len == 40000 + (raw ? 0 : 2));
        yy_assert(!memcmp(out.dat + (raw ? 0 : 1), src, 40000));
        yyjson_sw_free(&sw);
        sink_free(&out);
        pool_free(&pool);
        free(src);
    }
}



/*==============================================================================
 * MARK: - Allocator
 *============================================================================*/

// A failed allocation anywhere: an error, or no failure at all.
static void test_sw_alloc_fail(void) {
    enum { depth = 20000 };
    usize left, fails = 0, i;
    bool done = false;
    if (!SW_DEPTH_OK(depth)) return;
    for (left = 0; !done; left++) {
        sink_t out = {0};
        yyjson_sw sw = {0};
        test_alc ctx;
        yyjson_alc alc;
        bool ok;
        test_alc_init(&alc, &ctx, NULL, left, SIZE_MAX);
        ok = yyjson_sw_init_cb(&sw, sink_write, &out, &(yyjson_sw_opts){ 0, depth, 0 },
                  &alc);
        if (ok) {
            for (i = 0; i < depth && ok; i++) ok = yyjson_sw_arr_begin(&sw);
            for (i = 0; i < depth && ok; i++) ok = yyjson_sw_arr_end(&sw);
            ok = ok && yyjson_sw_finish(&sw);
        }
        if (!ok) {
            expect_sw_err(&sw, YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
            fails++;
        } else {
            yy_assert(out.len == depth * 2);
            done = true;
        }
        yyjson_sw_free(&sw);
        sink_free(&out);
        yy_assert(left < 100);
    }
    yy_assert(fails > 0);
}

// The buffer grows to what it needs, and not much more.
static void test_sw_alloc_budget(void) {
    enum { depth = 20000 };
    sink_t out = {0};
    yyjson_sw sw = {0};
    test_alc ctx;
    yyjson_alc alc;
    usize i;
    if (!SW_DEPTH_OK(depth)) return;
    test_alc_init(&alc, &ctx, NULL, SIZE_MAX, 65536 + 16384);
    yy_assert(yyjson_sw_init_cb(&sw, sink_write, &out,
              &(yyjson_sw_opts){ 0, depth, 0 }, &alc));
    for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_begin(&sw));
    yy_assert(yyjson_sw_write_null(&sw));
    for (i = 0; i < depth; i++) yy_assert(yyjson_sw_arr_end(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(out.len == depth * 2 + 4);
    yyjson_sw_free(&sw);
    sink_free(&out);
}



#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE



/*==============================================================================
 * MARK: - File
 *============================================================================*/

static void test_sw_fp(void) {
    yyjson_sw sw = {0};
    FILE *fp;
    char buf[256];
    usize len;

    // no FILE
    yy_assert(!yyjson_sw_init_fp(&sw, NULL, NULL, NULL));
    expect_sw_err(&sw, YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    yyjson_sw_free(&sw);
    yy_assert(!yyjson_sw_init_fp(NULL, stdout, NULL, NULL));

    // the text goes to the current position
    fp = tmpfile();
    yy_assert(fp != NULL);
    yy_assert(fwrite("// ", 1, 3, fp) == 3);
    yy_assert(yyjson_sw_init_fp(&sw, fp,
              &(yyjson_sw_opts){ YYJSON_WRITE_NEWLINE_AT_END, 0, 0 }, NULL));
    yy_assert(sw_basic(&sw));
    yy_assert(yyjson_sw_finish(&sw));
    yy_assert(yyjson_sw_len(&sw) == strlen(BASIC) + 1);
    yyjson_sw_free(&sw);
    yy_assert(fflush(fp) == 0 && fseek(fp, 0, SEEK_SET) == 0);
    len = fread(buf, 1, sizeof(buf), fp);
    yy_assert(len == 3 + strlen(BASIC) + 1);
    yy_assert(!memcmp(buf, "// ", 3) && !memcmp(buf + 3, BASIC, len - 4));
    yy_assert(buf[len - 1] == '\n');
    fclose(fp);

    // a file that cannot be written
    {
#ifdef _WIN32
        fp = fopen("NUL", "rb");
#else
        fp = fopen("/dev/null", "rb");
#endif
        if (fp) {
            yy_assert(yyjson_sw_init_fp(&sw, fp, NULL, NULL));
            yy_assert(yyjson_sw_write_null(&sw));
            yy_assert(!yyjson_sw_finish(&sw));
            expect_sw_err(&sw, YYJSON_WRITE_ERROR_STREAMING_IO);
            yyjson_sw_free(&sw);
            fclose(fp);
        }
    }
}

#endif /* !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE */



yy_test_case(test_json_sw) {
    test_sw_init();
    test_sw_reuse();
    test_sw_values();
    test_sw_root();
    test_sw_raw();
    test_sw_flags();
    test_sw_utf8();
    test_sw_docs();
    test_sw_str_bound();
    test_sw_args();
    test_sw_val_type();
    test_sw_state();
    test_sw_nan();
    test_sw_sticky();
    test_sw_depth();
    test_sw_max_depth();
    test_sw_deep();
    test_sw_deep_pretty();
    test_sw_fit();
    test_sw_flush();
    test_sw_long();
    test_sw_win_size();
    test_sw_options();
    test_sw_io();
    test_sw_pool_min();
    test_sw_pool_guarantee();
    test_sw_pool_limit();
    test_sw_pool_wrapped();
    test_sw_pool_win();
    test_sw_alloc_fail();
    test_sw_alloc_budget();
#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE
    test_sw_fp();
#endif
}

#else
yy_test_case(test_json_sw) {}
#endif
