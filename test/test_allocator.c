// This file is used to test the built-in pool memory allocator. 

#include "yyjson.h"
#include "yy_test_utils.h"


static void test_alc_init(void) {
    yyjson_alc alc;
    usize size;
    void *buf;
    
    yy_assert(!yyjson_alc_pool_init(NULL, NULL, 0));
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 0));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 1024));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    char small_buf[10];
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, small_buf, sizeof(small_buf)));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    size = 8 * sizeof(void *) - 1;
    buf = malloc(size);
    yy_assert(!yyjson_alc_pool_init(&alc, buf, size));
    free(buf);
}

static void test_alc_new(void) {
    yyjson_alc *alc;
    void *ptr;
    
    alc = yyjson_alc_pool_new(0);
    yy_assert(!alc);
    
    alc = yyjson_alc_pool_new(1024);
    yy_assert(alc);
    ptr = alc->malloc(alc->ctx, 512);
    yy_assert(ptr);
    alc->free(alc->ctx, ptr);
    yyjson_alc_pool_free(alc);
    
    yyjson_alc_pool_free(NULL);
}

static void test_alc_use(void) {
    yyjson_alc alc;
    usize size;
    void *buf, *mem[16];
    
    size = 1024;
    buf = malloc(size);
    yy_assert(yyjson_alc_pool_init(&alc, buf, size));
    
    {   // suc and fail
        mem[0] = alc.malloc(alc.ctx, 0);
        yy_assert(!mem[0]);
        mem[0] = alc.malloc(alc.ctx, 512);
        yy_assert(mem[0]);
        memset(mem[0], 0, 512);
        mem[1] = alc.malloc(alc.ctx, 512);
        yy_assert(!mem[1]);
        alc.free(alc.ctx, mem[0]);
    }
    
    {   // alc large, free, alc again
        for (int i = 0; i < 16; i++) {
            mem[i] = alc.malloc(alc.ctx, 32);
            yy_assert(mem[i]);
            memset(mem[i], 0, 32);
        }
        for (int i = 0; i < 16; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 0; i < 16; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 15; i >= 0; i--) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // alc large, free, alc small
        for (int i = 0; i < 16; i++) {
            mem[i] = alc.malloc(alc.ctx, 32);
            yy_assert(mem[i]);
            memset(mem[i], 0, 32);
        }
        for (int i = 0; i < 16; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 0; i < 16; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 1);
            yy_assert(mem[i]);
            memset(mem[i], 0, 1);
        }
        for (int i = 15; i >= 0; i--) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // alc small, free, alc large
        for (int i = 0; i < 16; i++) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 16; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 0; i < 16; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 32);
            yy_assert(mem[i]);
            memset(mem[i], 0, 32);
        }
        for (int i = 0; i < 16; i++) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // alc, realloc same
        for (int i = 0; i < 16; i++) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 16; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 1; i < 16; i += 2) {
            yy_assert(!alc.realloc(alc.ctx, mem[i], 0, 0));
            yy_assert(!alc.realloc(alc.ctx, mem[i], 0, 1024));
            mem[i] = alc.realloc(alc.ctx, mem[i], 0, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 16; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 16; i++) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // alc large, realloc small
        for (int i = 0; i < 8; i++) {
            mem[i] = alc.malloc(alc.ctx, 64);
            yy_assert(mem[i]);
            memset(mem[i], 0, 64);
        }
        for (int i = 0; i < 8; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 1; i < 8; i += 2) {
            mem[i] = alc.realloc(alc.ctx, mem[i], 0, 1);
            yy_assert(mem[i]);
            memset(mem[i], 0, 1);
        }
        for (int i = 0; i < 8; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 8; i++) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // alc small, realloc large
        for (int i = 0; i < 8; i++) {
            mem[i] = alc.malloc(alc.ctx, 8);
            yy_assert(mem[i]);
            memset(mem[i], 0, 8);
        }
        for (int i = 0; i < 8; i += 2) {
            alc.free(alc.ctx, mem[i]);
        }
        for (int i = 1; i < 8; i += 2) {
            mem[i] = alc.realloc(alc.ctx, mem[i], 0, 32);
            yy_assert(mem[i]);
            memset(mem[i], 0, 32);
        }
        for (int i = 0; i < 8; i += 2) {
            mem[i] = alc.malloc(alc.ctx, 16);
            yy_assert(mem[i]);
            memset(mem[i], 0, 16);
        }
        for (int i = 0; i < 8; i++) {
            alc.free(alc.ctx, mem[i]);
        }
    }
    {   // same space realloc
        mem[0] = alc.malloc(alc.ctx, 64);
        mem[0] = alc.realloc(alc.ctx, mem[0], 0, 128);
        yy_assert(mem[0]);
        alc.free(alc.ctx, mem[0]);
    }
    {   // random
        memset(mem, 0, sizeof(mem));
        yy_rand_reset(0);
        for (int p = 0; p < 50000; p++) {
            int i = yy_rand_u32_uniform(16);
            usize rsize = yy_rand_u32_uniform(1024 + 16);
            void *tmp = mem[i];
            if (tmp) {
                if (yy_rand_u32_uniform(4) == 0) {
                    tmp = alc.realloc(alc.ctx, tmp, 0, rsize);
                    if (tmp) mem[i] = tmp;
                } else {
                    alc.free(alc.ctx, tmp);
                    mem[i] = NULL;
                }
            } else {
                tmp = alc.malloc(alc.ctx, rsize);
                if (tmp) memset(tmp, 0xFF, rsize);
                mem[i] = tmp;
            }
        }
        for (int i = 0; i < 16; i++) {
            if (mem[i]) alc.free(alc.ctx, mem[i]);
        }
    }
    
    free(buf);
}

// test allocator for different length json
static void test_alc_read(void) {
#if !YYJSON_DISABLE_READER
    for (size_t n = 1; n <= 1000; n++) {
        // e.g. n = 3: [1,1,1]
        size_t len = 1 + n * 2;
        char *str = malloc(len);
        str[0] = '[';
        for (size_t i = 0; i < n; i++) {
            str[i * 2 + 1] = '1';
            str[i * 2 + 2] = ',';
        }
        str[len - 1] = ']';
        
        {
            yyjson_read_flag flg = 0;
            yyjson_alc *alc = yyjson_alc_pool_new(yyjson_read_max_memory_usage(len, flg));
            yyjson_doc *doc = yyjson_read_opts(str, len, flg, alc, NULL);
            yy_assert(doc);
            yy_assert(doc->val_read == n + 1);
            yyjson_doc_free(doc);
            yyjson_alc_pool_free(alc);
        }
        {
            str = realloc(str, len + YYJSON_PADDING_SIZE);
            memset(str + len, 0, YYJSON_PADDING_SIZE);
            yyjson_read_flag flg = YYJSON_READ_INSITU;
            yyjson_alc *alc = yyjson_alc_pool_new(yyjson_read_max_memory_usage(len, flg));
            yyjson_doc *doc = yyjson_read_opts(str, len, flg, alc, NULL);
            yy_assert(doc);
            yy_assert(doc->val_read == n + 1);
            yyjson_doc_free(doc);
            yyjson_alc_pool_free(alc);
        }
        
        free(str);
    }
#endif
}

yy_test_case(test_allocator) {
    test_alc_init();
    test_alc_new();
    test_alc_use();
    test_alc_read();
}
