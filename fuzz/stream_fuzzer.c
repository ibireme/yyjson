#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

/* Compare the streaming reader and writer with the DOM reader.
   Read from memory, in-situ, callbacks, and a small pool. Check the values,
   read errors, skipped content, and whether the input was changed. */

/* Limit input and output sizes to keep memory use small in OSS-Fuzz. */
#define FUZZ_MAX_INPUT ((size_t)60 << 10)
#define FUZZ_TIGHT_DEPTH ((size_t)8)
#define FUZZ_TIGHT_POOL ((size_t)256)

/* Writing can make a number longer (`1e20` is `100000000000000000000.0`). */
#define FUZZ_MAX_OUTPUT (FUZZ_MAX_INPUT * 8 + 1024)

typedef enum {
    MODE_CB_1,
    MODE_CB_7,
    MODE_CB_64,
    MODE_MEM,
    MODE_INSITU,
    MODE_CB_TIGHT,
    MODE_MEM_TIGHT,
    MODE_COUNT
} fuzz_mode;

typedef struct {
    const uint8_t *dat;
    size_t len, pos, chunk;
} fuzz_feed;

static size_t fuzz_read(void *ctx, void *dst, size_t cap) {
    fuzz_feed *feed = (fuzz_feed *)ctx;
    size_t len = feed->len - feed->pos;
    if (len > feed->chunk) len = feed->chunk;
    if (len > cap) len = cap;
    memcpy(dst, feed->dat + feed->pos, len);
    feed->pos += len;
    return len;
}

typedef struct {
    uint8_t *dat;
    size_t len;
} fuzz_sink;

static uint8_t g_pool[FUZZ_TIGHT_POOL];
static uint8_t g_input[FUZZ_MAX_INPUT + YYJSON_PADDING_SIZE];
static uint8_t g_output[FUZZ_MAX_OUTPUT];
static uint8_t g_stack[FUZZ_MAX_INPUT];

static bool fuzz_write(void *ctx, const void *src, size_t len) {
    fuzz_sink *sink = (fuzz_sink *)ctx;
    if (len > sizeof(g_output) - sink->len) return false;
    memcpy(sink->dat + sink->len, src, len);
    sink->len += len;
    return true;
}

/*----------------------------------------------------------------------------*/

/* Opens a reader on a private copy of the input. */
static bool fuzz_open(yyjson_sr *sr, fuzz_mode mode, fuzz_feed *feed,
                      const uint8_t *dat, size_t len) {
    yyjson_alc alc;
    bool tight = mode == MODE_CB_TIGHT || mode == MODE_MEM_TIGHT;
    yyjson_sr_opts opts = {0};
    opts.flg = mode == MODE_INSITU ? YYJSON_READ_INSITU : 0;
    if (tight) {
        opts.max_depth = FUZZ_TIGHT_DEPTH;
#if YYJSON_READER_DEPTH_LIMIT
        if (opts.max_depth > YYJSON_READER_DEPTH_LIMIT) {
            opts.max_depth = YYJSON_READER_DEPTH_LIMIT;
        }
#endif
    }
    memcpy(g_input, dat, len);
    if (tight && !yyjson_alc_pool_init(&alc, g_pool, sizeof(g_pool))) {
        return false;
    }
    if (mode == MODE_MEM || mode == MODE_MEM_TIGHT || mode == MODE_INSITU) {
        return yyjson_sr_init_mem(sr, (char *)g_input, len, &opts,
                                   tight ? &alc : NULL);
    }
    feed->dat = g_input;
    feed->len = len;
    feed->pos = 0;
    feed->chunk = mode == MODE_CB_1 ? 1 : mode == MODE_CB_7 ? 7 :
                  mode == MODE_CB_TIGHT ? 3 : 64;
    return yyjson_sr_init_cb(sr, fuzz_read, NULL, feed, &opts,
                              tight ? &alc : NULL);
}

/* Copy the document without recursion. Check writer errors after the read. */
static bool fuzz_copy(yyjson_sr *sr, yyjson_sw *sw) {
    size_t depth = 0;
    bool need = true;
    yyjson_sv sv;
    yyjson_val val;
    while (true) {
        if (need) {
            yyjson_type type = yyjson_sr_peek_type(sr);
            if (type == YYJSON_TYPE_ARR || type == YYJSON_TYPE_OBJ) {
                if (type == YYJSON_TYPE_ARR) {
                    if (!yyjson_sr_arr_begin(sr)) return false;
                    (void)yyjson_sw_arr_begin(sw);
                } else {
                    if (!yyjson_sr_obj_begin(sr)) return false;
                    (void)yyjson_sw_obj_begin(sw);
                }
                g_stack[depth++] = (uint8_t)type;
            } else {
                if (type == YYJSON_TYPE_STR) {
                    sv = yyjson_sr_read_str(sr);
                    if (!sv.ptr) return false;
                    (void)yyjson_sw_write_strn(sw, sv.ptr, sv.len);
                } else {
                    val = yyjson_sr_read_scalar(sr);
                    if (yyjson_sr_has_err(sr)) return false;
                    (void)yyjson_sw_write_scalar(sw, &val);
                }
                if (!depth) return true;
            }
            need = false;
        }
        if (g_stack[depth - 1] == YYJSON_TYPE_OBJ) {
            if (yyjson_sr_obj_next(sr, &sv)) {
                (void)yyjson_sw_write_keyn(sw, sv.ptr, sv.len);
                need = true;
                continue;
            }
            if (!yyjson_sr_obj_end(sr)) return false;
            (void)yyjson_sw_obj_end(sw);
        } else {
            if (yyjson_sr_arr_next(sr)) {
                need = true;
                continue;
            }
            if (!yyjson_sr_arr_end(sr)) return false;
            (void)yyjson_sw_arr_end(sw);
        }
        if (!--depth) return true;
    }
}

/* Only the small pool may reject a document accepted by the DOM reader. */
static bool fuzz_is_read_limit(yyjson_sr *sr, fuzz_mode mode) {
    yyjson_read_code code = yyjson_sr_get_err(sr, NULL);
    if (mode != MODE_CB_TIGHT && mode != MODE_MEM_TIGHT) return false;
    return code == YYJSON_READ_ERROR_DEPTH ||
           code == YYJSON_READ_ERROR_BUFFER_LIMIT;
}

/* The writer may have a lower depth limit than the reader. */
static bool fuzz_is_write_limit(yyjson_sw *sw) {
    return YYJSON_WRITER_DEPTH_LIMIT &&
           yyjson_sw_depth(sw) == YYJSON_WRITER_DEPTH_LIMIT &&
           yyjson_sw_get_err(sw, NULL) == YYJSON_WRITE_ERROR_DEPTH;
}

static void fuzz_intact(fuzz_mode mode, const uint8_t *dat, size_t len) {
    if (mode != MODE_INSITU && memcmp(g_input, dat, len)) abort();
}

/*----------------------------------------------------------------------------*/

/* Skipping must accept any document that was read successfully. */
static void test_skip(fuzz_mode mode, const uint8_t *dat, size_t len,
                      bool accepted) {
    fuzz_feed feed;
    yyjson_sr sr = {0};
    bool ok = fuzz_open(&sr, mode, &feed, dat, len);
    ok = ok && yyjson_sr_skip_val(&sr) && yyjson_sr_finish(&sr);
    if (!ok && accepted) abort();
    fuzz_intact(mode, dat, len);
    yyjson_sr_free(&sr);
}

/* Read the document, write it again, and compare the values. */
static void test_mode(fuzz_mode mode, const uint8_t *dat, size_t len,
                      yyjson_val *dom_root) {
    fuzz_feed feed;
    fuzz_sink sink = { g_output, 0 };
    yyjson_sr sr = {0};
    yyjson_sw sw = {0};
    bool dom_ok = dom_root != NULL, sr_ok, sw_ok;

    /* odd modes write to a callback, the others to memory */
    if (mode & 1) {
        sw_ok = yyjson_sw_init_cb(&sw, fuzz_write, &sink, NULL, NULL);
    } else {
        sw_ok = yyjson_sw_init_mem(&sw, g_output, sizeof(g_output), NULL);
    }
    sr_ok = fuzz_open(&sr, mode, &feed, dat, len);
    sr_ok = sr_ok && fuzz_copy(&sr, &sw) && yyjson_sr_finish(&sr);
    sw_ok = sw_ok && yyjson_sw_finish(&sw);
    if (!(mode & 1)) sink.len = (size_t)yyjson_sw_len(&sw);
    fuzz_intact(mode, dat, len);

    if (sr_ok != dom_ok && !(dom_ok && fuzz_is_read_limit(&sr, mode))) abort();

    if (sr_ok && dom_ok) {
        if (!sw_ok) {
            if (!fuzz_is_write_limit(&sw)) abort();
        } else {
            yyjson_doc *doc = yyjson_read((char *)g_output, sink.len, 0);
            if (!doc) abort();
            if (!yyjson_equals(dom_root, yyjson_doc_get_root(doc))) abort();
            yyjson_doc_free(doc);
        }
    }

    yyjson_sr_free(&sr);
    yyjson_sw_free(&sw);
    test_skip(mode, dat, len, sr_ok || dom_ok);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    yyjson_doc *doc;
    int mode;
    if (!size || size > FUZZ_MAX_INPUT) return 0;
    memcpy(g_input, data, size);
    doc = yyjson_read_opts((char *)g_input, size, 0, NULL, NULL);
    for (mode = 0; mode < MODE_COUNT; mode++) {
        test_mode((fuzz_mode)mode, data, size, yyjson_doc_get_root(doc));
    }
    yyjson_doc_free(doc);
    return 0;
}
