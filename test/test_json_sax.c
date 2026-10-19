// This file is used to test a SAX adapter built on the streaming reader and
// writer.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_STREAMING && !YYJSON_DISABLE_READER

// Bounds both the reader and the C recursion of the walker. The reader stops
// at this depth, so the walker never goes deeper.
#if YYJSON_READER_DEPTH_LIMIT && YYJSON_READER_DEPTH_LIMIT < 256
#   define SAX_MAX_DEPTH ((size_t)YYJSON_READER_DEPTH_LIMIT)
#else
#   define SAX_MAX_DEPTH ((size_t)256)
#endif



/*==============================================================================
 * MARK: - SAX Adapter
 *============================================================================*/

/**
 SAX event handler.

 Each callback is invoked as the corresponding JSON token is parsed and returns
 `true` to continue or `false` to stop. Stopping is not a reader error. Any
 callback may be `NULL`, in which case the event is ignored.

 The `str`/`len` passed to `str` and `key` point into the input or reader buffer
 and are only valid for the duration of the callback. They are NUL-terminated,
 but `len` is needed for embedded NUL characters. Copy data that must live
 longer.

 The `num` callback receives a temporary `yyjson_val` valid only for the
 duration of the call. Use the `yyjson_get_*` accessors on it. With
 `YYJSON_READ_NUMBER_AS_RAW` / `YYJSON_READ_BIGNUM_AS_RAW` it may be a RAW
 value; use `yyjson_get_raw()` + `yyjson_get_len()` (it is not
 null-terminated).
 */
typedef struct yyjson_sax_handler {
    /** Called at the start of an object (`{`). */
    bool (*obj_begin)(void *ctx);
    /** Called at the end of an object (`}`). `count` is the member count. */
    bool (*obj_end)(void *ctx, size_t count);
    /** Called at the start of an array (`[`). */
    bool (*arr_begin)(void *ctx);
    /** Called at the end of an array (`]`). `count` is the element count. */
    bool (*arr_end)(void *ctx, size_t count);
    /** Called for an object member key (a string). */
    bool (*key)(void *ctx, const char *str, size_t len);
    /** Called for a string value. */
    bool (*str)(void *ctx, const char *str, size_t len);
    /** Called for a number value (see note above about RAW numbers). */
    bool (*num)(void *ctx, const yyjson_val *num);
    /** Called for a boolean value. */
    bool (*bool_val)(void *ctx, bool value);
    /** Called for a `null` value. */
    bool (*null_val)(void *ctx);
} yyjson_sax_handler;

// Reads a value, and sends its events to the handler. Returns false if the
// handler stops, or the input is not valid.
static bool sax_read_value(yyjson_sr *sr, const yyjson_sax_handler *h,
                           void *ctx) {
    switch (yyjson_sr_peek_type(sr)) {
    case YYJSON_TYPE_OBJ: {
        yyjson_sv key;
        size_t count = 0;
        if (!yyjson_sr_obj_begin(sr)) return false;
        if (h->obj_begin && !h->obj_begin(ctx)) return false;
        while (yyjson_sr_obj_next(sr, &key)) {
            if (h->key && !h->key(ctx, key.ptr, key.len)) return false;
            if (!sax_read_value(sr, h, ctx)) return false;
            count++;
        }
        if (yyjson_sr_has_err(sr) || !yyjson_sr_obj_end(sr)) return false;
        return !h->obj_end || h->obj_end(ctx, count);
    }

    case YYJSON_TYPE_ARR: {
        size_t count = 0;
        if (!yyjson_sr_arr_begin(sr)) return false;
        if (h->arr_begin && !h->arr_begin(ctx)) return false;
        while (yyjson_sr_arr_next(sr)) {
            if (!sax_read_value(sr, h, ctx)) return false;
            count++;
        }
        if (yyjson_sr_has_err(sr) || !yyjson_sr_arr_end(sr)) return false;
        return !h->arr_end || h->arr_end(ctx, count);
    }

    case YYJSON_TYPE_STR: {
        yyjson_sv sv = yyjson_sr_read_str(sr);
        if (yyjson_sr_has_err(sr)) return false;
        return !h->str || h->str(ctx, sv.ptr, sv.len);
    }

    case YYJSON_TYPE_NUM: {
        yyjson_val val = yyjson_sr_read_num(sr);
        if (yyjson_sr_has_err(sr)) return false;
        return !h->num || h->num(ctx, &val);
    }

    case YYJSON_TYPE_BOOL: {
        bool val = yyjson_sr_read_bool(sr);
        if (yyjson_sr_has_err(sr)) return false;
        return !h->bool_val || h->bool_val(ctx, val);
    }

    case YYJSON_TYPE_NULL:
        yyjson_sr_read_null(sr);
        if (yyjson_sr_has_err(sr)) return false;
        return !h->null_val || h->null_val(ctx);

    default:
        // let the reader set the error
        (void)yyjson_sr_read_scalar(sr);
        return false;
    }
}

// Reads a whole document, and sends its events to the handler.
static bool sax_read(yyjson_sr *sr, const yyjson_sax_handler *h, void *ctx) {
    if (!sr || !h) return false;
    return sax_read_value(sr, h, ctx) && yyjson_sr_finish(sr);
}



/*==============================================================================
 * MARK: - Helper
 *============================================================================*/

// How the input reaches the reader.
typedef enum {
    SAX_INSITU, // memory, in-situ
    SAX_MEM,    // memory, copied
    SAX_CB_1,   // callback, 1-byte at a time
    SAX_CB_7,   // callback, 7-byte at a time (YYJSON_PADDING_SIZE - 1)
    SAX_CB,     // callback, as much as asked
} sax_mode;

#define foreach_mode(m) for (sax_mode m = SAX_INSITU; m <= SAX_CB; m++)

typedef struct {
    const char *dat;
    size_t len, pos, step;
} sax_feed;

static size_t sax_feed_read(void *ctx, void *dst, size_t cap) {
    sax_feed *feed = (sax_feed *)ctx;
    size_t len = feed->len - feed->pos;
    if (len > feed->step) len = feed->step;
    if (len > cap) len = cap;
    memcpy(dst, feed->dat + feed->pos, len);
    feed->pos += len;
    return len;
}

// Runs the handler on `json` (NUL-terminated). `reader_ok` tells whether the
// reader was fine, so that a stop of the handler can be told from a bad input.
static bool sax_run(const char *json, sax_mode mode, yyjson_read_flag flg,
                    const yyjson_sax_handler *h, void *ctx, bool *reader_ok) {
    size_t len = strlen(json);
    char *input = (char *)malloc(len + YYJSON_PADDING_SIZE);
    size_t step = mode == SAX_CB_1 ? 1 : mode == SAX_CB_7 ? 7 : len + 1;
    sax_feed feed = { input, len, 0, step };
    yyjson_sr_opts opts = { flg, SAX_MAX_DEPTH, 0, 0 };
    yyjson_sr sr = {0};
    bool ok;

    yy_assert(input != NULL);
    memcpy(input, json, len);
    memset(input + len, 0, YYJSON_PADDING_SIZE);
    if (mode == SAX_INSITU) {
        opts.flg |= YYJSON_READ_INSITU;
        ok = yyjson_sr_init_mem(&sr, input, len, &opts, NULL);
    } else if (mode == SAX_MEM) {
        ok = yyjson_sr_init_mem(&sr, input, len, &opts, NULL);
    } else {
        ok = yyjson_sr_init_cb(&sr, sax_feed_read, NULL, &feed, &opts, NULL);
    }
    if (ok) ok = sax_read(&sr, h, ctx);
    if (reader_ok) *reader_ok = !yyjson_sr_has_err(&sr);
    yyjson_sr_free(&sr);
    free(input);
    return ok;
}



/*==============================================================================
 * MARK: - SAX Reader
 *============================================================================*/

// What the handler has seen.
typedef struct {
    size_t obj_begin, obj_end, arr_begin, arr_end;
    size_t keys, strings, uints, sints, reals, bools, trues, nulls;
    size_t obj_counts[2];
    size_t arr_count;
    uint64_t uint_sum;
    int64_t sint_sum;
    double real_sum;
} sax_stat;

static bool stat_obj_begin(void *ctx) {
    ((sax_stat *)ctx)->obj_begin++;
    return true;
}

static bool stat_obj_end(void *ctx, size_t count) {
    sax_stat *stat = (sax_stat *)ctx;
    yy_assert(stat->obj_end < yy_nelems(stat->obj_counts));
    stat->obj_counts[stat->obj_end++] = count;
    return true;
}

static bool stat_arr_begin(void *ctx) {
    ((sax_stat *)ctx)->arr_begin++;
    return true;
}

static bool stat_arr_end(void *ctx, size_t count) {
    sax_stat *stat = (sax_stat *)ctx;
    stat->arr_end++;
    stat->arr_count = count;
    return true;
}

static bool stat_key(void *ctx, const char *str, size_t len) {
    static const char *keys[] = { "n", "a", "s", "r", "neg" };
    sax_stat *stat = (sax_stat *)ctx;
    const char *key;
    yy_assert(stat->keys < yy_nelems(keys));
    key = keys[stat->keys++];
    yy_assertf(strlen(key) == len && !memcmp(str, key, len),
               "expected key '%s', got '%.*s'", key, (int)len, str);
    return true;
}

static bool stat_str(void *ctx, const char *str, size_t len) {
    yy_assert(len == 2 && !memcmp(str, "x\n", 2) && str[len] == '\0');
    ((sax_stat *)ctx)->strings++;
    return true;
}

static bool stat_num(void *ctx, const yyjson_val *num) {
    sax_stat *stat = (sax_stat *)ctx;
    if (yyjson_is_uint(num)) {
        stat->uints++;
        stat->uint_sum += yyjson_get_uint(num);
    } else if (yyjson_is_sint(num)) {
        stat->sints++;
        stat->sint_sum += yyjson_get_sint(num);
    } else {
        yy_assert(yyjson_is_real(num));
        stat->reals++;
        stat->real_sum += yyjson_get_real(num);
    }
    return true;
}

static bool stat_bool(void *ctx, bool value) {
    sax_stat *stat = (sax_stat *)ctx;
    stat->bools++;
    stat->trues += value ? 1 : 0;
    return true;
}

static bool stat_null(void *ctx) {
    ((sax_stat *)ctx)->nulls++;
    return true;
}

static const yyjson_sax_handler stat_handler = {
    stat_obj_begin, stat_obj_end, stat_arr_begin, stat_arr_end,
    stat_key, stat_str, stat_num, stat_bool, stat_null,
};

// Every event arrives, in every kind of input.
static void test_sax_events(void) {
    static const char json[] =
        "{\"n\":1,\"a\":[true,false,null,{\"s\":\"x\\n\",\"r\":1.5}],\"neg\":-2}";
    foreach_mode(mode) {
        sax_stat stat = {0};
        bool ok = false;
        yy_assert(sax_run(json, mode, 0, &stat_handler, &stat, &ok));
        yy_assert(ok);
        yy_assert(stat.obj_begin == 2 && stat.obj_end == 2);
        yy_assert(stat.obj_counts[0] == 2 && stat.obj_counts[1] == 3);
        yy_assert(stat.arr_begin == 1 && stat.arr_end == 1);
        yy_assert(stat.arr_count == 4);
        yy_assert(stat.keys == 5 && stat.strings == 1);
        yy_assert(stat.uints == 1 && stat.uint_sum == 1);
        yy_assert(stat.sints == 1 && stat.sint_sum == -2);
        yy_assert(stat.reals == 1 && stat.real_sum == 1.5);
        yy_assert(stat.bools == 2 && stat.trues == 1);
        yy_assert(stat.nulls == 1);
    }
}

// A callback that is NULL is skipped.
static void test_sax_empty(void) {
    static const yyjson_sax_handler empty = {0};
    foreach_mode(mode) {
        bool ok = false;
        yy_assert(sax_run("{\"a\":[1,true,null,\"x\",{}]}", mode, 0, &empty,
                          NULL, &ok));
        yy_assert(ok);
    }
}

// A callback that stops: the read stops, and it is not an error.
typedef struct {
    size_t stop; // the event that returns false, counted from 0
    size_t seen;
} sax_stop;

static bool stop_event(void *ctx) {
    sax_stop *s = (sax_stop *)ctx;
    return s->seen++ != s->stop;
}

static bool stop_count(void *ctx, size_t count) {
    (void)count;
    return stop_event(ctx);
}

static bool stop_str(void *ctx, const char *str, size_t len) {
    (void)str;
    (void)len;
    return stop_event(ctx);
}

static bool stop_num(void *ctx, const yyjson_val *num) {
    (void)num;
    return stop_event(ctx);
}

static bool stop_bool(void *ctx, bool value) {
    (void)value;
    return stop_event(ctx);
}

static void test_sax_stop(void) {
    static const char json[] = "{\"a\":[1,\"x\",true,null],\"b\":{}}";
    static const yyjson_sax_handler h = {
        stop_event, stop_count, stop_event, stop_count,
        stop_str, stop_str, stop_num, stop_bool, stop_event,
    };
    // 12 events: {, "a", [, 1, "x", true, null, ], "b", {, }, }
    size_t stop;
    foreach_mode(mode) {
        for (stop = 0; stop < 12; stop++) {
            sax_stop s = { stop, 0 };
            bool ok = true;
            yy_assert(!sax_run(json, mode, 0, &h, &s, &ok));
            yy_assert(ok && s.seen == stop + 1);
        }
        {
            sax_stop s = { 100, 0 };
            bool ok = false;
            yy_assert(sax_run(json, mode, 0, &h, &s, &ok));
            yy_assert(ok && s.seen == 12);
        }
    }
}

// Raw numbers reach the handler as raw values.
static bool raw_num(void *ctx, const yyjson_val *num) {
    sax_stat *stat = (sax_stat *)ctx;
    yy_assert(yyjson_is_raw(num));
    yy_assert(yyjson_get_len(num) == 4 && !memcmp(yyjson_get_raw(num), "1.50", 4));
    stat->reals++;
    return true;
}

static void test_sax_raw(void) {
    static const yyjson_sax_handler h = { .num = raw_num };
    foreach_mode(mode) {
        sax_stat stat = {0};
        bool ok = false;
        yy_assert(sax_run("[1.50]", mode, YYJSON_READ_NUMBER_AS_RAW, &h, &stat,
                          &ok));
        yy_assert(ok && stat.reals == 1);
    }
}

// The reader stops at the depth, and the walker does not go deeper.
static void test_sax_depth(void) {
    static const yyjson_sax_handler empty = {0};
    char over[SAX_MAX_DEPTH + 2], deep[SAX_MAX_DEPTH * 2 + 1];
    yyjson_sr sr = {0};

    memset(over, '[', SAX_MAX_DEPTH + 1);
    over[SAX_MAX_DEPTH + 1] = '\0';
    memset(deep, '[', SAX_MAX_DEPTH);
    memset(deep + SAX_MAX_DEPTH, ']', SAX_MAX_DEPTH);
    deep[SAX_MAX_DEPTH * 2] = '\0';

    foreach_mode(mode) {
        bool ok = true;
        yy_assert(!sax_run(over, mode, 0, &empty, NULL, &ok));
        yy_assert(!ok);
        ok = false;
        yy_assert(sax_run(deep, mode, 0, &empty, NULL, &ok));
        yy_assert(ok);
    }

    yy_assert(yyjson_sr_init_mem(&sr, over, SAX_MAX_DEPTH + 1,
                                    &(yyjson_sr_opts){ 0, SAX_MAX_DEPTH, 0, 0 }, NULL));
    yy_assert(!sax_read(&sr, &empty, NULL));
    yy_assert(yyjson_sr_get_err(&sr, NULL) == YYJSON_READ_ERROR_DEPTH);
    yyjson_sr_free(&sr);
}

// A bad input is an error of the reader, and the read fails.
static void test_sax_invalid(void) {
    static const yyjson_sax_handler empty = {0};
    static const char *invalid[] = {
        "[[}}", "[1 2]", "[1,]", "{\"a\" 1}", "{\"a\":1 \"b\":2}",
        "{\"a\":[1,2}}", "[[]", "[]]", "", "x", "[tru]", "[01]", "\"a",
    };
    size_t i;
    for (i = 0; i < yy_nelems(invalid); i++) {
        foreach_mode(mode) {
            bool ok = true;
            yy_assertf(!sax_run(invalid[i], mode, 0, &empty, NULL, &ok),
                       "malformed SAX input passed: %s", invalid[i]);
            yy_assertf(!ok, "malformed SAX input had no error: %s", invalid[i]);
        }
    }
}

#if !YYJSON_DISABLE_WRITER



/*==============================================================================
 * MARK: - SAX Writer
 *============================================================================*/

// A handler that writes every event with the streaming writer. `ctx` is the
// writer. The same events as input make a JSON text, so this one copies or
// reformats a document.
static bool wr_obj_begin(void *ctx) {
    return yyjson_sw_obj_begin((yyjson_sw *)ctx);
}

static bool wr_obj_end(void *ctx, size_t count) {
    (void)count;
    return yyjson_sw_obj_end((yyjson_sw *)ctx);
}

static bool wr_arr_begin(void *ctx) {
    return yyjson_sw_arr_begin((yyjson_sw *)ctx);
}

static bool wr_arr_end(void *ctx, size_t count) {
    (void)count;
    return yyjson_sw_arr_end((yyjson_sw *)ctx);
}

static bool wr_key(void *ctx, const char *str, size_t len) {
    return yyjson_sw_write_keyn((yyjson_sw *)ctx, str, len);
}

static bool wr_str(void *ctx, const char *str, size_t len) {
    return yyjson_sw_write_strn((yyjson_sw *)ctx, str, len);
}

static bool wr_num(void *ctx, const yyjson_val *num) {
    // a raw number is written as it is
    return yyjson_sw_write_scalar((yyjson_sw *)ctx, num);
}

static bool wr_bool(void *ctx, bool value) {
    return yyjson_sw_write_bool((yyjson_sw *)ctx, value);
}

static bool wr_null(void *ctx) {
    return yyjson_sw_write_null((yyjson_sw *)ctx);
}

static const yyjson_sax_handler wr_handler = {
    wr_obj_begin, wr_obj_end, wr_arr_begin, wr_arr_end,
    wr_key, wr_str, wr_num, wr_bool, wr_null,
};

typedef struct {
    char *dat;
    size_t len, cap;
} sax_out;

static bool sax_out_write(void *ctx, const void *src, size_t len) {
    sax_out *out = (sax_out *)ctx;
    if (out->len + len + 1 > out->cap) {
        out->cap = (out->len + len + 1) * 2;
        out->dat = (char *)realloc(out->dat, out->cap);
        yy_assert(out->dat != NULL);
    }
    memcpy(out->dat + out->len, src, len);
    out->len += len;
    out->dat[out->len] = '\0';
    return true;
}

// The events of a document make the text of it, minified or pretty, to memory
// or to a callback.
static void test_sax_copy(void) {
    static const struct {
        const char *json;
        yyjson_read_flag rflg;
        yyjson_write_flag wflg;
        const char *expect;
    } cases[] = {
        { " { \"n\" : 1 ,\"a\":[ true,false , null,{ \"s\":\"x\\n\", \"r\":1.50} ],"
          "\"neg\" :-2 }\n", 0, 0,
          "{\"n\":1,\"a\":[true,false,null,{\"s\":\"x\\n\",\"r\":1.5}],"
          "\"neg\":-2}" },
        { "{\"n\":1,\"a\":[true,[],{}],\"b\":\"\"}", 0, YYJSON_WRITE_PRETTY,
          "{\n    \"n\": 1,\n    \"a\": [\n        true,\n        [],\n"
          "        {}\n    ],\n    \"b\": \"\"\n}" },
        { "[\"\\u4e2d\\ud83d\\ude00\",\"/\"]", 0,
          YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_ESCAPE_SLASHES,
          "[\"\\u4E2D\\uD83D\\uDE00\",\"\\/\"]" },
        { "[1.50,1e2,123456789012345678901234567890]",
          YYJSON_READ_NUMBER_AS_RAW, 0, "[1.50,1e2,123456789012345678901234567890]" },
        { "\"a\\u0000b\"", 0, 0, "\"a\\u0000b\"" },
        { "3", 0, YYJSON_WRITE_NEWLINE_AT_END, "3\n" },
    };
    size_t i;
    foreach_mode(mode) {
        for (i = 0; i < yy_nelems(cases); i++) {
            size_t len = strlen(cases[i].expect);
            yyjson_sw sw = {0};
            sax_out out = {0};
            char *mem = (char *)malloc(len + 64);
            bool ok = false;
            yy_assert(mem != NULL);

            // to a callback
            yy_assert(yyjson_sw_init_cb(&sw, sax_out_write, &out,
                      &(yyjson_sw_opts){ cases[i].wflg, 0, 0 }, NULL));
            yy_assert(sax_run(cases[i].json, mode, cases[i].rflg, &wr_handler,
                              &sw, &ok) && ok);
            yy_assert(yyjson_sw_finish(&sw));
            yy_assertf(out.len == len && !memcmp(out.dat, cases[i].expect, len),
                       "got [%s], expect [%s]", out.dat, cases[i].expect);
            yyjson_sw_free(&sw);
            free(out.dat);

            // to memory
            yy_assert(yyjson_sw_init_mem(&sw, mem, len + 64,
                      &(yyjson_sw_opts){ cases[i].wflg, 0, 0 }));
            yy_assert(sax_run(cases[i].json, mode, cases[i].rflg, &wr_handler,
                              &sw, &ok) && ok);
            yy_assert(yyjson_sw_finish(&sw));
            yy_assert(yyjson_sw_len(&sw) == len && !memcmp(mem, cases[i].expect, len));
            yyjson_sw_free(&sw);
            free(mem);
        }
    }
}

// A writer that fails stops the read: the reader is fine, the writer has the
// error.
static void test_sax_copy_fail(void) {
    foreach_mode(mode) {
        yyjson_sw sw = {0};
        char mem[8];
        bool ok = true;
        yy_assert(yyjson_sw_init_mem(&sw, mem, sizeof(mem), NULL));
        yy_assert(!sax_run("{\"a\":[1,2,3,4,5,6]}", mode, 0, &wr_handler, &sw,
                           &ok));
        yy_assert(ok);
        yy_assert(yyjson_sw_get_err(&sw, NULL) == YYJSON_WRITE_ERROR_BUFFER_LIMIT);
        yyjson_sw_free(&sw);
    }
}

#endif /* !YYJSON_DISABLE_WRITER */



/*==============================================================================
 * MARK: - Entry
 *============================================================================*/

yy_test_case(test_json_sax) {
    test_sax_events();
    test_sax_empty();
    test_sax_stop();
    test_sax_raw();
    test_sax_depth();
    test_sax_invalid();
#if !YYJSON_DISABLE_WRITER
    test_sax_copy();
    test_sax_copy_fail();
#endif
}

#else
yy_test_case(test_json_sax) {}
#endif
