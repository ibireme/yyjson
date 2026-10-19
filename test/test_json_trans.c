// This file is used to test the streaming JSON reader and writer together:
// a document is read by the reader and written by the writer, and the result
// must be the same as the DOM reader and writer give.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_STREAMING && !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER



/*==============================================================================
 * MARK: - Helper
 *============================================================================*/

// Growable byte buffer, also the context of the write callback.
typedef struct {
    char *dat;
    usize len;
    usize cap;
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
    if (len > s->max_chunk) s->max_chunk = len;
    sink_put(s, src, len);
    return true;
}

// How the input reaches the reader: memory in-situ, memory copied, or a
// callback that gives `chunk` bytes at most (0 for as much as asked).
typedef enum { SRC_INSITU, SRC_MEM, SRC_CB } src_kind;

typedef struct {
    src_kind kind;
    usize chunk;
} src_mode;

static const src_mode src_modes[] = {
    { SRC_INSITU, 0 },
    { SRC_MEM, 0 },
    { SRC_CB, 0 },
    { SRC_CB, 1 },
    { SRC_CB, 7 },
};

// A reader with its input. The input is exactly `len` bytes long (in-situ adds
// the padding), so a read past the end is caught by a sanitizer.
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

static void rd_open(rd_t *r, src_mode m, const char *json, usize len,
                    yyjson_read_flag flg) {
    yyjson_sr_opts opts = { flg, 0, 0, 0 };
    usize pad = m.kind == SRC_INSITU ? YYJSON_PADDING_SIZE : 0;
    memset(r, 0, sizeof(*r));
    r->dat = (char *)calloc(len + pad + 1, 1);
    yy_assert(r->dat != NULL);
    if (len) memcpy(r->dat, json, len);
    r->len = len;
    r->chunk = m.chunk;
    if (m.kind == SRC_CB) {
        yy_assert(yyjson_sr_init_cb(&r->sr, rd_read, NULL, r, &opts, NULL));
    } else {
        if (m.kind == SRC_INSITU) opts.flg |= YYJSON_READ_INSITU;
        yy_assert(yyjson_sr_init_mem(&r->sr, r->dat, len, &opts, NULL));
    }
}

static void rd_close(rd_t *r) {
    yyjson_sr_free(&r->sr);
    free(r->dat);
}

// Where the output goes: memory, or a callback with a window of `win` bytes
// (0 for the default).
typedef enum { DST_MEM, DST_CB } dst_kind;

typedef struct {
    dst_kind kind;
    usize win;
} dst_mode;

static const dst_mode dst_modes[] = {
    { DST_MEM, 0 },
    { DST_CB, 0 },
    { DST_CB, 5 },
    { DST_CB, 16 },
};

// A writer with its output.
typedef struct {
    yyjson_sw sw;
    sink_t out;
    char *mem;
    dst_mode mode;
} wr_t;

static void wr_open(wr_t *w, dst_mode m, yyjson_write_flag flg, usize cap) {
    yyjson_sw_opts opts = { flg, 0, m.win };
    memset(w, 0, sizeof(*w));
    w->mode = m;
    if (m.kind == DST_MEM) {
        w->mem = (char *)malloc(cap);
        yy_assert(w->mem != NULL);
        yy_assert(yyjson_sw_init_mem(&w->sw, w->mem, cap, &opts));
    } else {
        yy_assert(yyjson_sw_init_cb(&w->sw, sink_write, &w->out, &opts, NULL));
    }
}

static void wr_close(wr_t *w) {
    yyjson_sw_free(&w->sw);
    sink_free(&w->out);
    free(w->mem);
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

// Copies one value from the reader to the writer, without recursion. Returns
// false if anything fails, on either side.
static bool copy_val(yyjson_sr *sr, yyjson_sw *sw) {
    u8 *ctn = NULL;
    usize len = 0, cap = 0;
    bool ok = false, need = true;
    yyjson_sv sv;
    yyjson_val val;

    while (true) {
        if (need) {
            yyjson_type type = yyjson_sr_peek_type(sr);
            if (type == YYJSON_TYPE_ARR || type == YYJSON_TYPE_OBJ) {
                if (type == YYJSON_TYPE_ARR) {
                    if (!yyjson_sr_arr_begin(sr) || !yyjson_sw_arr_begin(sw)) {
                        goto done;
                    }
                } else {
                    if (!yyjson_sr_obj_begin(sr) || !yyjson_sw_obj_begin(sw)) {
                        goto done;
                    }
                }
                if (len == cap) {
                    cap = cap ? cap * 2 : 16;
                    ctn = (u8 *)realloc(ctn, cap);
                    yy_assert(ctn != NULL);
                }
                ctn[len++] = (u8)type;
            } else {
                if (type == YYJSON_TYPE_STR) {
                    sv = yyjson_sr_read_str(sr);
                    if (!sv.ptr || !yyjson_sw_write_strn(sw, sv.ptr, sv.len)) {
                        goto done;
                    }
                } else if (type == YYJSON_TYPE_NUM) {
                    val = yyjson_sr_read_num(sr);
                    if (yyjson_sr_has_err(sr)) goto done;
                    if (!yyjson_sw_write_num(sw, &val)) goto done;
                } else {
                    val = yyjson_sr_read_scalar(sr);
                    if (yyjson_sr_has_err(sr)) goto done;
                    if (!yyjson_sw_write_scalar(sw, &val)) goto done;
                }
                if (len == 0) {
                    ok = true;
                    goto done;
                }
            }
            need = false;
        }

        if (ctn[len - 1] == YYJSON_TYPE_OBJ) {
            if (yyjson_sr_obj_next(sr, &sv)) {
                if (!yyjson_sw_write_keyn(sw, sv.ptr, sv.len)) goto done;
                need = true;
                continue;
            }
            if (!yyjson_sr_obj_end(sr) || !yyjson_sw_obj_end(sw)) goto done;
        } else {
            if (yyjson_sr_arr_next(sr)) {
                need = true;
                continue;
            }
            if (!yyjson_sr_arr_end(sr) || !yyjson_sw_arr_end(sw)) goto done;
        }
        if (--len == 0) {
            ok = true;
            goto done;
        }
    }
done:
    free(ctn);
    return ok;
}

// Copies every document of NDJSON or concatenated JSON.
static bool copy_docs(yyjson_sr *sr, yyjson_sw *sw) {
    usize count = 0;
    while (yyjson_sr_doc_next(sr)) {
        if (count++ && !yyjson_sw_doc_next(sw)) return false;
        if (!copy_val(sr, sw)) return false;
    }
    return !yyjson_sr_has_err(sr) && yyjson_sw_finish(sw);
}

// Expects that a reader and a writer together do what the DOM does: accept
// the same documents, and write the same text. With `full`, every kind of
// input goes with every kind of output, otherwise they are paired by `pair`.
static void check_trans(const char *json, usize len, yyjson_read_flag rflg,
                        yyjson_write_flag wflg, bool full, usize pair) {
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)json, len, rflg,
                                       NULL, NULL);
    char *ref = doc ? yyjson_write(doc, wflg, NULL) : NULL;
    usize ref_len = ref ? strlen(ref) : 0;
    usize cap = (ref ? ref_len : len) * 2 + 64;
    usize si, di;

    for (si = 0; si < yy_nelems(src_modes); si++) {
        for (di = 0; di < yy_nelems(dst_modes); di++) {
            rd_t r;
            wr_t w;
            bool ok;
            if (!full && di != (si + pair) % yy_nelems(dst_modes)) continue;
            rd_open(&r, src_modes[si], json, len, rflg);
            wr_open(&w, dst_modes[di], wflg, cap);
            ok = copy_val(&r.sr, &w.sw) && yyjson_sr_finish(&r.sr) &&
                 yyjson_sw_finish(&w.sw);
            yy_assertf(ok == (ref != NULL),
                       "src %zu, dst %zu: %s, DOM %s: %.80s", si, di,
                       ok ? "accepted" : "rejected", ref ? "accepts" : "rejects",
                       json);
            if (ok) {
                usize got_len;
                const char *got = wr_text(&w, &got_len);
                yy_assertf(got_len == ref_len && !memcmp(got, ref, ref_len),
                           "src %zu, dst %zu: got [%.80s], expect [%.80s]",
                           si, di, got, ref);
                // a chunk is never longer than the window
                if (w.mode.kind == DST_CB) {
                    yy_assert(w.out.max_chunk <=
                              (w.mode.win ? w.mode.win : 16384));
                }
            }
            wr_close(&w);
            rd_close(&r);
        }
    }
    free(ref);
    yyjson_doc_free(doc);
}

static void check_trans_str(const char *json, yyjson_read_flag rflg,
                            yyjson_write_flag wflg) {
    check_trans(json, strlen(json), rflg, wflg, true, 0);
}



/*==============================================================================
 * MARK: - Flag
 *============================================================================*/

// The read flags of the reader, and the write flags that go with them.
static void test_trans_flags(void) {
    const struct {
        const char *json;
        yyjson_read_flag rflg;
        yyjson_write_flag wflg;
    } cases[] = {
        // numbers as raw text, or as numbers
        { "[1,0.50,2.0,-0]", YYJSON_READ_NUMBER_AS_RAW, 0 },
        { "[1,0.50,2.0,-0]", 0, 0 },
        { "[1,0.50,1e999]", YYJSON_READ_BIGNUM_AS_RAW, 0 },
        { "{\"a\":[2.0,2]}", YYJSON_READ_NUMBER_AS_RAW, YYJSON_WRITE_PRETTY },
#if !YYJSON_DISABLE_UTF8_VALIDATION
        // invalid UTF-8, rejected, or kept
        { "\"a\xff\xfe\"", 0, YYJSON_WRITE_ALLOW_INVALID_UNICODE },
        { "\"a\xff\xfe\"", YYJSON_READ_ALLOW_INVALID_UNICODE,
          YYJSON_WRITE_ALLOW_INVALID_UNICODE },
        { "{\"\xff\":\"\xc0\"}", YYJSON_READ_ALLOW_INVALID_UNICODE,
          YYJSON_WRITE_ALLOW_INVALID_UNICODE | YYJSON_WRITE_ESCAPE_UNICODE },
        { "[\"\xe4\xb8\"]", YYJSON_READ_ALLOW_INVALID_UNICODE,
          YYJSON_WRITE_ALLOW_INVALID_UNICODE },
#endif
    };
    usize i;
    for (i = 0; i < yy_nelems(cases); i++) {
        check_trans_str(cases[i].json, cases[i].rflg, cases[i].wflg);
    }
}



/*==============================================================================
 * MARK: - Docs
 *============================================================================*/

// Makes `count` documents, on lines or apart by a space, and what they are
// after the DOM minifies them, one per line.
static void docs_make(sink_t *src, sink_t *expect, usize count) {
    static const char *items[] = {
        "null", "[]", "{}", "2.0", "\"text\"", "[1,2,3]", "[[],{}]",
        "{\"a\":{\"b\":[true,false]}}", "{\"k\":\"\\u00e9\\ud83d\\ude00\"}",
    };
    usize i;
    for (i = 0; i < count; i++) {
        const char *item = items[i % yy_nelems(items)];
        yyjson_doc *doc = yyjson_read(item, strlen(item), 0);
        char *min = doc ? yyjson_write(doc, 0, NULL) : NULL;
        yy_assert(min != NULL);
        if (i) {
            sink_puts(src, i % 3 ? "\n" : " ");
            sink_puts(expect, "\n");
        }
        sink_puts(src, item);
        sink_puts(expect, min);
        free(min);
        yyjson_doc_free(doc);
    }
}

// Many documents in one input: NDJSON and concatenated JSON.
static void test_trans_docs(void) {
    sink_t src = {0}, expect = {0};
    usize si, di;
    docs_make(&src, &expect, 60);
    for (si = 0; si < yy_nelems(src_modes); si++) {
        for (di = 0; di < yy_nelems(dst_modes); di++) {
            rd_t r;
            wr_t w;
            usize len;
            const char *got;
            rd_open(&r, src_modes[si], src.dat, src.len, 0);
            wr_open(&w, dst_modes[di], 0, expect.len * 2);
            yy_assert(copy_docs(&r.sr, &w.sw));
            got = wr_text(&w, &len);
            yy_assert(len == expect.len && !memcmp(got, expect.dat, len));
            wr_close(&w);
            rd_close(&r);
        }
    }
    sink_free(&src);
    sink_free(&expect);
}

#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE

// From a file to a file.
static void test_trans_fp(void) {
    sink_t src = {0}, expect = {0};
    yyjson_sr sr = {0};
    yyjson_sw sw = {0};
    FILE *in = tmpfile(), *out = tmpfile();
    char *buf;
    usize len;

    yy_assert(in != NULL && out != NULL);
    docs_make(&src, &expect, 400);
    yy_assert(fwrite(src.dat, 1, src.len, in) == src.len);
    yy_assert(fflush(in) == 0 && fseek(in, 0, SEEK_SET) == 0);

    yy_assert(yyjson_sr_init_fp(&sr, in, NULL, NULL));
    yy_assert(yyjson_sw_init_fp(&sw, out, NULL, NULL));
    yy_assert(copy_docs(&sr, &sw));
    yy_assert(yyjson_sr_finish(&sr));
    yy_assert(yyjson_sw_len(&sw) == expect.len);
    yyjson_sr_free(&sr);
    yyjson_sw_free(&sw);

    buf = (char *)malloc(expect.len + 1);
    yy_assert(buf != NULL);
    yy_assert(fflush(out) == 0 && fseek(out, 0, SEEK_SET) == 0);
    len = fread(buf, 1, expect.len + 1, out);
    yy_assert(len == expect.len && !memcmp(buf, expect.dat, len));
    free(buf);
    fclose(in);
    fclose(out);
    sink_free(&src);
    sink_free(&expect);
}

#endif /* !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE */



/*==============================================================================
 * MARK: - Corpus
 *============================================================================*/

// Returns whether the file must be accepted, by the name of it.
static bool corpus_must_pass(usize dir, const char *name) {
    switch (dir) {
    case 0: return yy_str_has_prefix(name, "pass");
    case 1: return !strcmp(name, "utf8.json");
    case 2: return yy_str_has_prefix(name, "y_");
    case 3: return true;
    case 4: return !yy_str_contains(name, "invalid");
    default: return !strchr(name, '(');
    }
}

// A file that must pass. The depth limit may reject the read or the write.
static yyjson_doc *read_must_pass(const char *path, char *dat, usize len) {
    yyjson_read_err rerr;
    yyjson_write_err werr;
    yyjson_doc *doc = yyjson_read_opts(dat, len, 0, NULL, &rerr);
    char *out;
    if (!doc) {
#if YYJSON_READER_DEPTH_LIMIT
        yy_assertf(rerr.code == YYJSON_READ_ERROR_DEPTH,
                   "valid file is rejected: %s", path);
#else
        yy_assertf(doc != NULL, "valid file is rejected: %s", path);
#endif
        return NULL;
    }
    out = yyjson_write_opts(doc, 0, NULL, NULL, &werr);
    if (!out) {
#if YYJSON_WRITER_DEPTH_LIMIT
        yy_assertf(werr.code == YYJSON_WRITE_ERROR_DEPTH,
                   "valid file is rejected: %s", path);
#else
        yy_assertf(out != NULL, "valid file is rejected: %s", path);
#endif
    }
    free(out);
    return doc;
}

// Every file in the test data goes through the reader and the writer, with
// the flags of the writer, and the text is the same as the DOM gives.
static void test_trans_corpus(void) {
    static const char *dirs[] = {
        "test_checker", "test_encoding", "test_parsing", "test_roundtrip",
        "test_transform", "test_yyjson",
    };
    const yyjson_write_flag wflags[] = {
        0,
        YYJSON_WRITE_PRETTY,
        YYJSON_WRITE_PRETTY_TWO_SPACES,
        YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_LOWERCASE_HEX,
        YYJSON_WRITE_ESCAPE_SLASHES | YYJSON_WRITE_NEWLINE_AT_END,
    };
    char dir[YY_MAX_PATH], path[YY_MAX_PATH];
    usize d, tested = 0, valid = 0, other = 0;

    for (d = 0; d < yy_nelems(dirs); d++) {
        int i, count;
        char **names;
        yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", dirs[d],
                        NULL);
        names = yy_dir_read(dir, &count);
        yy_assertf(names && count, "failed to read corpus: %s", dir);
        for (i = 0; i < count; i++) {
            const char *name = names[i];
            yyjson_doc *doc;
            u8 *dat = NULL;
            usize len = 0, f;
            bool must;
            if (*name == '.') continue;
            must = corpus_must_pass(d, name);
            yy_path_combine(path, dir, name, NULL);
            yy_assertf(yy_file_read(path, &dat, &len), "failed to read: %s",
                       path);
            if (must) {
                doc = read_must_pass(path, (char *)(void *)dat, len);
            } else {
                doc = yyjson_read((const char *)dat, len, 0);
            }
            // without the validation, the DOM takes some bad files and writes
            // them as they are, the writer here does not
            if (must || !YYJSON_DISABLE_UTF8_VALIDATION) {
                for (f = 0; f < (doc ? yy_nelems(wflags) : 1); f++) {
                    check_trans((const char *)dat, len, 0, wflags[f], false,
                                tested + f);
                }
            }
            if (doc) valid++;
            else other++;
            yyjson_doc_free(doc);
            free(dat);
            tested++;
        }
        yy_dir_free(names);
    }
    yy_assert(tested != 0 && valid != 0 && other != 0);
}



/*==============================================================================
 * MARK: - Entry
 *============================================================================*/

yy_test_case(test_json_trans) {
    test_trans_flags();
    test_trans_docs();
#if !YYJSON_FREESTANDING && !YYJSON_DISABLE_FILE
    test_trans_fp();
#endif
    test_trans_corpus();
}

#else
yy_test_case(test_json_trans) {}
#endif
