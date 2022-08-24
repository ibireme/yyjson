#include "yyjson.h"
#include "yy_test_utils.h"

yy_test_case(test_allocator) {
    yyjson_alc alc;
    usize size;
    void *buf, *mem[16];
    
    yy_assert(!yyjson_alc_pool_init(NULL, NULL, 0));
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 0));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 1));
    alc.free(NULL, NULL);
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 1024));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 1));
    alc.free(NULL, NULL);
    
    char small_buf[10];
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, small_buf, sizeof(small_buf)));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 1));
    alc.free(NULL, NULL);
    
    size = 8 * sizeof(void *) - 1;
    buf = malloc(size);
    yy_assert(!yyjson_alc_pool_init(&alc, buf, size));
    free(buf);
    
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
            yy_assert(!alc.realloc(alc.ctx, mem[i], 0));
            yy_assert(!alc.realloc(alc.ctx, mem[i], 1024));
            mem[i] = alc.realloc(alc.ctx, mem[i], 16);
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
            mem[i] = alc.realloc(alc.ctx, mem[i], 1);
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
            mem[i] = alc.realloc(alc.ctx, mem[i], 32);
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
        mem[0] = alc.realloc(alc.ctx, mem[0], 128);
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
                    tmp = alc.realloc(alc.ctx, tmp, rsize);
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
