/*==============================================================================
 * Utilities for single thread test and benchmark.
 *
 * Copyright (C) 2018 YaoYuan <ibireme@gmail.com>.
 * Released under the MIT license (MIT).
 *============================================================================*/

#include "yy_test_utils.h"

#define REPEAT_2(x)   x x
#define REPEAT_4(x)   REPEAT_2(REPEAT_2(x))
#define REPEAT_8(x)   REPEAT_2(REPEAT_4(x))
#define REPEAT_16(x)  REPEAT_2(REPEAT_8(x))
#define REPEAT_32(x)  REPEAT_2(REPEAT_16(x))
#define REPEAT_64(x)  REPEAT_2(REPEAT_32(x))
#define REPEAT_128(x) REPEAT_2(REPEAT_64(x))
#define REPEAT_256(x) REPEAT_2(REPEAT_128(x))
#define REPEAT_512(x) REPEAT_2(REPEAT_256(x))



/*==============================================================================
 * CPU
 *============================================================================*/

bool yy_cpu_setup_priority(void) {
#if defined(_WIN32)
    BOOL ret1 = SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    BOOL ret2 = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    return ret1 && ret2;
#else
    int policy;
    struct sched_param param;
    pthread_t thread = pthread_self();
    pthread_getschedparam(thread, &policy, &param);
    param.sched_priority = sched_get_priority_max(policy);
    if (param.sched_priority != -1) {
        return pthread_setschedparam(pthread_self(), policy, &param) == 0;
    }
    return false;
#endif
}

void yy_cpu_spin(f64 second) {
    f64 end = yy_time_get_seconds() + second;
    while(yy_time_get_seconds() < end) {
        volatile int x = 0;
        while (x < 1000) x++;
    }
}

#if (yy_has_attribute(naked)) && YY_ARCH_ARM64
#define YY_CPU_RUN_INST_COUNT_A (8192 * (128 + 256))
#define YY_CPU_RUN_INST_COUNT_B (8192 * (512))

__attribute__((naked, noinline))
void yy_cpu_run_seq_a(void) {
    __asm volatile
    (
     "mov x0, #8192\n"
     "Loc_seq_loop_begin_a:\n"
     REPEAT_128("add x1, x1, x1\n")
     REPEAT_256("add x1, x1, x1\n")
     "subs x0, x0, #1\n"
     "bne Loc_seq_loop_begin_a\n"
     "ret\n"
     );
}

__attribute__((naked, noinline))
void yy_cpu_run_seq_b(void) {
    __asm volatile
    (
     "mov x0, #8192\n"
     "Loc_seq_loop_begin_b:\n"
     REPEAT_512("add x1, x1, x1\n")
     "subs x0, x0, #1\n"
     "bne Loc_seq_loop_begin_b\n"
     "ret\n"
     );
}

#elif (yy_has_attribute(naked)) && YY_ARCH_ARM32
#define YY_CPU_RUN_INST_COUNT_A (8192 * (128 + 256))
#define YY_CPU_RUN_INST_COUNT_B (8192 * (512))

__attribute__((naked, noinline))
void yy_cpu_run_seq_a(void) {
    __asm volatile
    (
     "mov.w r0, #8192\n"
     "Loc_seq_loop_begin_a:\n"
     REPEAT_128("add r1, r1, r1\n")
     REPEAT_256("add r1, r1, r1\n")
     "subs r0, r0, #1\n"
     "bne.w Loc_seq_loop_begin_a\n"
     "bx lr\n"
     );
}

__attribute__((naked, noinline))
void yy_cpu_run_seq_b(void) {
    __asm volatile
    (
     "mov.w r0, #8192\n"
     "Loc_seq_loop_begin_b:\n"
     REPEAT_512("add r1, r1, r1\n")
     "subs r0, r0, #1\n"
     "bne.w Loc_seq_loop_begin_b\n"
     "bx lr\n"
     );
}

#elif (yy_has_attribute(naked)) && (YY_ARCH_X64 || YY_ARCH_X86)
#define YY_CPU_RUN_INST_COUNT_A (8192 * (128 + 256))
#define YY_CPU_RUN_INST_COUNT_B (8192 * (512))

__attribute__((naked, noinline))
void yy_cpu_run_seq_a(void) {
    __asm volatile
    (
        "movl $8192, %eax\n"
        "seq_loop_begin_a:\n"
        REPEAT_128("addl %edx, %edx\n")
        REPEAT_256("addl %edx, %edx\n")
        "subl $1, %eax\n"
        "jne seq_loop_begin_a\n"
        "ret\n"
    );
}

__attribute__((naked, noinline))
void yy_cpu_run_seq_b(void) {
    __asm volatile
    (
        "movl $8192, %eax\n"
        "seq_loop_begin_b:\n"
        REPEAT_512("addl %edx, %edx\n")
        "subl $1, %eax\n"
        "jne seq_loop_begin_b\n"
        "ret\n"
    );
}

#else
#define YY_CPU_RUN_INST_COUNT_A (8192 * 4 * (32 + 64))
#define YY_CPU_RUN_INST_COUNT_B (8192 * 4 * (128))

/* These functions contains some `add` instructions with data dependence.
   This file should be compiled with optimization flag on.
   We hope that each line of the code in the inner loop may compiled as
   an `add` instruction, each `add` instruction takes 1 cycle, and inner kernel
   can fit in the L1i cache. Try: https://godbolt.org/z/d3GP1b */

u32 yy_cpu_run_seq_vals[8];

void yy_cpu_run_seq_a(void) {
    u32 loop = 8192;
    u32 v1 = yy_cpu_run_seq_vals[1];
    u32 v2 = yy_cpu_run_seq_vals[2];
    u32 v3 = yy_cpu_run_seq_vals[3];
    u32 v4 = yy_cpu_run_seq_vals[4];
    do {
        REPEAT_32( v1 += v4; v2 += v1; v3 += v2; v4 += v3; )
        REPEAT_64( v1 += v4; v2 += v1; v3 += v2; v4 += v3; )
    } while(--loop);
    yy_cpu_run_seq_vals[0] = v1;
}

void yy_cpu_run_seq_b(void) {
    u32 loop = 8192;
    u32 v1 = yy_cpu_run_seq_vals[1];
    u32 v2 = yy_cpu_run_seq_vals[2];
    u32 v3 = yy_cpu_run_seq_vals[3];
    u32 v4 = yy_cpu_run_seq_vals[4];
    do {
        REPEAT_128( v1 += v4; v2 += v1; v3 += v2; v4 += v3; )
    } while(--loop);
    yy_cpu_run_seq_vals[0] = v1;
}

#endif

static u64 yy_cycle_per_sec = 0;
static u64 yy_tick_per_sec = 0;

void yy_cpu_measure_freq(void) {
#define warmup_count 8
#define measure_count 128
    yy_time p1, p2;
    u64 ticks_a[measure_count];
    u64 ticks_b[measure_count];
    
    /* warm up CPU caches and stabilize the frequency */
    for (int i = 0; i < warmup_count; i++) {
        yy_cpu_run_seq_a();
        yy_cpu_run_seq_b();
        yy_time_get_current(&p1);
        yy_time_get_ticks();
    }
    
    /* run sequence a and b repeatedly, record ticks and times */
    yy_time_get_current(&p1);
    u64 t1 = yy_time_get_ticks();
    for (int i = 0; i < measure_count; i++) {
        u64 s1 = yy_time_get_ticks();
        yy_cpu_run_seq_a();
        u64 s2 = yy_time_get_ticks();
        yy_cpu_run_seq_b();
        u64 s3 = yy_time_get_ticks();
        ticks_a[i] = s2 - s1;
        ticks_b[i] = s3 - s2;
    }
    u64 t2 = yy_time_get_ticks();
    yy_time_get_current(&p2);
    
    /* calculate tick count per second, this value is high precision */
    f64 total_seconds = yy_time_to_seconds(&p2) - yy_time_to_seconds(&p1);
    u64 total_ticks = t2 - t1;
    yy_tick_per_sec = (u64)((f64)total_ticks / total_seconds);
    
    /* find the minimum ticks of each sequence to avoid inaccurate values
       caused by context switching, etc. */
    for (int i = 1; i < measure_count; i++) {
        if (ticks_a[i] < ticks_a[0]) ticks_a[0] = ticks_a[i];
        if (ticks_b[i] < ticks_b[0]) ticks_b[0] = ticks_b[i];
    }
    
    /* use the difference between two sequences to eliminate the overhead of
       loops and function calls */
    u64 one_ticks = ticks_b[0] - ticks_a[0];
    u64 one_insts = YY_CPU_RUN_INST_COUNT_B - YY_CPU_RUN_INST_COUNT_A;
    yy_cycle_per_sec = (u64)((f64)one_insts / (f64)one_ticks * (f64)yy_tick_per_sec);
#undef warmup_count
#undef measure_count
}

u64 yy_cpu_get_freq(void) {
    return yy_cycle_per_sec;
}

u64 yy_cpu_get_tick_per_sec(void) {
    return yy_tick_per_sec;
}

f64 yy_cpu_get_cycle_per_tick(void) {
    return (f64)yy_cycle_per_sec / (f64)yy_tick_per_sec;
}


/*==============================================================================
 * Environment
 *============================================================================*/

const char *yy_env_get_os_desc(void) {
#if defined(__MINGW64__)
    return "Windows (MinGW-w64)";
#elif defined(__MINGW32__)
    return "Windows (MinGW)";
#elif defined(__CYGWIN__) && defined(ARCH_64_DEFINED)
    return "Windows (Cygwin x64)";
#elif defined(__CYGWIN__)
    return "Windows (Cygwin x86)";
#elif defined(_WIN64)
    return "Windows 64-bit";
#elif defined(_WIN32)
    return "Windows 32-bit";
    
#elif defined(__APPLE__)
#   if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#       if defined(YY_ARCH_64)
    return "iOS 64-bit";
#       else
    return "iOS 32-bit";
#       endif
#   elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
#       if defined(YY_ARCH_64)
    return "macOS 64-bit";
#       else
    return "macOS 32-bit";
#       endif
#   else
#       if defined(YY_ARCH_64)
    return "Apple OS 64-bit";
#       else
    return "Apple OS 32-bit";
#       endif
#   endif
    
#elif defined(__ANDROID__)
#   if defined(YY_ARCH_64)
    return "Android 64-bit";
#   else
    return "Android 32-bit";
#   endif
    
#elif defined(__linux__) || defined(__linux) || defined(__gnu_linux__)
#   if defined(YY_ARCH_64)
    return "Linux 64-bit";
#   else
    return "Linux 32-bit";
#   endif
    
#elif defined(__BSD__) || defined(__FreeBSD__)
#   if defined(YY_ARCH_64)
    return "BSD 64-bit";
#   else
    return "BSD 32-bit";
#   endif
    
#else
#   if defined(YY_ARCH_64)
    return "Unknown OS 64-bit";
#   else
    return "Unknown OS 32-bit";
#   endif
#endif
}

const char *yy_env_get_cpu_desc(void) {
#if defined(__APPLE__)
    static char brand[256] = {0};
    size_t size = sizeof(brand);
    static bool finished = false;
    struct utsname sysinfo;
    
    if (finished) return brand;
    /* works for macOS */
    if (sysctlbyname("machdep.cpu.brand_string", (void *)brand, &size, NULL, 0) == 0) {
        if (strlen(brand) > 0) finished = true;
    }
    /* works for iOS, returns device model such as "iPhone9,1 ARM64_T8010" */
    if (!finished) {
        uname(&sysinfo);
        const char *model = sysinfo.machine;
        const char *cpu = sysinfo.version;
        if (cpu) {
            cpu = strstr(cpu, "RELEASE_");
            if (cpu) cpu += 8;
        }
        if (model || cpu) {
            snprintf(brand, sizeof(brand), "%s %s", model ? model : "", cpu ? cpu : "");
            finished = true;
        }
    }
    if (!finished) {
        snprintf(brand, sizeof(brand), "Unknown CPU");
        finished = true;
    }
    return brand;
    
#elif defined(_WIN32)
#if defined(__x86_64__) || defined(__amd64__) || \
    defined(_M_IX86) || defined(_M_AMD64)
    static char brand[0x40] = { 0 };
    static bool finished = false;
    int cpui[4] = { 0 };
    int nexids, i;
    
    if (finished) return brand;
    __cpuid(cpui, 0x80000000);
    nexids = cpui[0];
    if (nexids >= 0x80000004) {
        for (i = 2; i <= 4; i++) {
            memset(cpui, 0, sizeof(cpui));
            __cpuidex(cpui, i + 0x80000000, 0);
            memcpy(brand + (i - 2) * sizeof(cpui), cpui, sizeof(cpui));
        }
        finished = true;
    }
    if (!finished || strlen(brand) == 0) {
        snprintf(brand, sizeof(brand), "Unknown CPU");
        finished = true;
    }
    return brand;
#   else
    return "Unknown CPU";
#   endif
    
#else
#   define BUF_LENGTH 1024
    static char buf[BUF_LENGTH], *res = NULL;
    static bool finished = false;
    const char *prefixes[] = {
        "model name", /* x86 */
        "CPU part",   /* arm */
        "cpu model\t",/* mips */
        "cpu\t"       /* powerpc */
    };
    int i, len;
    FILE *fp;
    
    if (res) return res;
    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        while (!res) {
            memset(buf, 0, BUF_LENGTH);
            if (fgets(buf, BUF_LENGTH - 1, fp) == NULL) break;
            for (i = 0; i < (int)(sizeof(prefixes) / sizeof(char *)) && !res; i++) {
                if (strncmp(prefixes[i], buf, strlen(prefixes[i])) == 0) {
                    res = buf + strlen(prefixes[i]);
                }
            }
        }
        fclose(fp);
    }
    if (res) {
        while (*res == ' ' || *res == '\t' || *res == ':') res++;
        for (i = 0, len = (int)strlen(res); i < len; i++) {
            if (res[i] == '\t') res[i] = ' ';
            if (res[i] == '\r' || res[i] == '\n') res[i] = '\0';
        }
    } else {
        res = "Unknown CPU";
    }
    finished = true;
    return res;
#endif
}

const char *yy_env_get_compiler_desc(void) {
    static char buf[512] = {0};
    static bool finished = false;
    if (finished) return buf;

#if defined(__ICL) || defined(__ICC) || defined(__INTEL_COMPILER)
    int v, r; /* version, revision */
#   if defined(__INTEL_COMPILER)
    v = __INTEL_COMPILER;
#   elif defined(__ICC)
    v = __ICC;
#   else
    v = __ICL;
#   endif
    r = (v - (v / 100) * 100) / 10;
    v = v / 100;
    snprintf(buf, sizeof(buf), "Intel C++ Compiler %d.%d", v, r);
    
#elif defined(__ARMCC_VERSION)
    int v, r;
    v = __ARMCC_VERSION; /* PVVbbbb or Mmmuuxx */
    r = (v - (v / 1000000) * 1000000) / 1000;
    v = v / 1000000;
    snprintf(buf, sizeof(buf), "ARM Compiler %d.%d", v, r);
    
#elif defined(_MSC_VER)
    /* https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros */
    const char *vc;
#   if _MSC_VER >= 1930
    vc = "";
#   elif _MSC_VER >= 1920
    vc = " 2019";
#   elif _MSC_VER >= 1910
    vc = " 2017";
#   elif _MSC_VER >= 1900
    vc = " 2015";
#   elif _MSC_VER >= 1800
    vc = " 2013";
#   elif _MSC_VER >= 1700
    vc = " 2012";
#   elif _MSC_VER >= 1600
    vc = " 2010";
#   elif _MSC_VER >= 1500
    vc = " 2008";
#   elif _MSC_VER >= 1400
    vc = " 2005";
#   elif _MSC_VER >= 1310
    vc = " 7.1";
#   elif _MSC_VER >= 1300
    vc = " 7.0";
#   elif _MSC_VER >= 1200
    vc = " 6.0";
#   else
    vc = "";
#   endif
    snprintf(buf, sizeof(buf), "Microsoft Visual C++%s (%d)", vc, _MSC_VER);
    
#elif defined(__clang__)
#   if defined(__apple_build_version__)
    /* Apple versions: https://en.wikipedia.org/wiki/Xcode#Latest_versions */
    snprintf(buf, sizeof(buf), "Clang %d.%d.%d (Apple version)",
             __clang_major__, __clang_minor__, __clang_patchlevel__);
#   else
    snprintf(buf, sizeof(buf), "Clang %d.%d.%d",
             __clang_major__, __clang_minor__, __clang_patchlevel__);
#   endif
    
#elif defined(__GNUC__)
    const char *ext;
#   if defined(__CYGWIN__)
    ext = " (Cygwin)";
#   elif defined(__MINGW64__)
    ext = " (MinGW-w64)";
#   elif defined(__MINGW32__)
    ext = " (MinGW)";
#   else
    ext = "";
#   endif
#   if defined(__GNUC_PATCHLEVEL__)
    snprintf(buf, sizeof(buf), "GCC %d.%d.%d%s", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, ext);
#   else
    snprintf(buf, sizeof(buf), "GCC %d.%d%s", __GNUC__, __GNUC_MINOR__, ext);
#   endif
    
#else
    snprintf(buf, sizeof(buf), "Unknown Compiler");
#endif
    
    finished = true;
    return buf;
}



/*==============================================================================
 * Random Number Generator
 * PCG random: http://www.pcg-random.org
 * A fixed seed should be used to ensure repeatability of the test or benchmark.
 *============================================================================*/

#define YY_RANDOM_STATE_INIT (((u64)0x853C49E6U << 32) + 0x748FEA9BU)
#define YY_RANDOM_INC_INIT (((u64)0xDA3E39CBU << 32) + 0x94B95BDBU)
#define YY_RANDOM_MUL (((u64)0x5851F42DU << 32) + 0x4C957F2DU)

static u64 yy_random_state = YY_RANDOM_STATE_INIT;
static u64 yy_random_inc = YY_RANDOM_INC_INIT;

void yy_random_reset(void) {
    yy_random_state = YY_RANDOM_STATE_INIT;
    yy_random_inc = YY_RANDOM_INC_INIT;
}

u32 yy_random32(void) {
    u32 xorshifted, rot;
    u64 oldstate = yy_random_state;
    yy_random_state = oldstate * YY_RANDOM_MUL + yy_random_inc;
    xorshifted = (u32)(((oldstate >> 18) ^ oldstate) >> 27);
    rot = (u32)(oldstate >> 59);
    return (xorshifted >> rot) | (xorshifted << (((u32)-(i32)rot) & 31));
}

u32 yy_random32_uniform(u32 bound) {
    u32 r, threshold = (u32)(-(i32)bound) % bound;
    if (bound < 2) return 0;
    while (true) {
        r = yy_random32();
        if (r >= threshold) return r % bound;
    }
}

u32 yy_random32_range(u32 min, u32 max) {
    return yy_random32_uniform(max - min + 1) + min;
}

u64 yy_random64(void) {
    return (u64)yy_random32() << 32 | yy_random32();
}

u64 yy_random64_uniform(u64 bound) {
    u64 r, threshold = ((u64)-(i64)bound) % bound;
    if (bound < 2) return 0;
    while (true) {
        r = yy_random64();
        if (r >= threshold) return r % bound;
    }
}

u64 yy_random64_range(u64 min, u64 max) {
    return yy_random64_uniform(max - min + 1) + min;
}



/*==============================================================================
 * File Utils
 *============================================================================*/

bool yy_path_combine(char *buf, const char *path, ...) {
    if (!buf) return false;
    *buf = '\0';
    if (!path) return false;
    
    usize len = strlen(path);
    memmove(buf, path, len);
    const char *hdr = buf;
    buf += len;
    
    va_list args;
    va_start(args, path);
    while (true) {
        const char *item = va_arg(args, const char *);
        if (!item) break;
        if (buf > hdr && *(buf - 1) != YY_DIR_SEPARATOR) {
            *buf++ = YY_DIR_SEPARATOR;
        }
        len = strlen(item);
        if (len && *item == YY_DIR_SEPARATOR) {
            len--;
            item++;
        }
        memmove(buf, item, len);
        buf += len;
    }
    va_end(args);
    
    *buf = '\0';
    return true;
}

bool yy_path_remove_last(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    *buf = '\0';
    if (len == 0) return false;
    
    const char *cur = path + len - 1;
    if (*cur == YY_DIR_SEPARATOR) cur--;
    for (; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
    }
    len = cur + 1 - path;
    memmove(buf, path, len);
    buf[len] = '\0';
    return len > 0;
}

bool yy_path_get_last(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    const char *end, *cur;
    if (!buf) return false;
    *buf = '\0';
    if (len == 0) return false;
    
    end = path + len - 1;
    if (*end == YY_DIR_SEPARATOR) end--;
    for (cur = end; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
    }
    len = end - cur;
    memmove(buf, cur + 1, len);
    buf[len] = '\0';
    return len > 0;
}

bool yy_path_append_ext(char *buf, const char *path, const char *ext) {
    usize len = path ? strlen(path) : 0;
    char tmp[YY_MAX_PATH];
    char *cur = tmp;
    if (!buf) return false;
    
    memcpy(cur, path, len);
    cur += len;
    *cur++ = '.';
    
    len = ext ? strlen(ext) : 0;
    memcpy(cur, ext, len);
    cur += len;
    *cur++ = '\0';
    
    memcpy(buf, tmp, cur - tmp);
    return true;
}

bool yy_path_remove_ext(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    memmove(buf, path, len + 1);
    for (char *cur = buf + len; cur >= buf; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
        if (*cur == '.') {
            *cur = '\0';
            return true;
        }
    }
    return false;
}

bool yy_path_get_ext(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    for (const char *cur = path + len; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
        if (*cur == '.') {
            memmove(buf, cur + 1, len - (cur - path));
            return true;
        }
    }
    *buf = '\0';
    return false;
}



bool yy_path_exist(const char *path) {
    if (!path || !strlen(path)) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    struct stat attr;
    if (stat(path, &attr) != 0) return false;
    return true;
#endif
}

bool yy_path_is_dir(const char *path) {
    if (!path || !strlen(path)) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat attr;
    if (stat(path, &attr) != 0) return false;
    return S_ISDIR(attr.st_mode);
#endif
}



static int strcmp_func(void const *a, void const *b) {
    char const *astr = *(char const **)a;
    char const *bstr = *(char const **)b;
    return strcmp(astr, bstr);
}

char **yy_dir_read_opts(const char *path, int *count, bool full) {
#ifdef _WIN32
    struct _finddata_t entry;
    intptr_t handle;
    int idx = 0, alc = 0;
    char **names = NULL, **names_tmp, *search;
    usize path_len = path ? strlen(path) : 0;
    
    if (count) *count = 0;
    if (path_len == 0) return NULL;
    search = malloc(path_len + 3);
    if (!search) return NULL;
    memcpy(search, path, path_len);
    if (search[path_len - 1] == '\\') path_len--;
    memcpy(search + path_len, "\\*\0", 3);
    
    handle = _findfirst(search, &entry);
    if (handle == -1) goto fail;
    
    alc = 4;
    names = malloc(alc * sizeof(char*));
    if (!names) goto fail;
    
    do {
        char *name = (char *)entry.name;
        if (!name || !strlen(name)) continue;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        name = _strdup(name);
        if (!name) goto fail;
        if (idx + 1 >= alc) {
            alc *= 2;
            names_tmp = realloc(names, alc * sizeof(char*));
            if (!names_tmp) goto fail;
            names = names_tmp;
        }
        if (full) {
            char *fullpath = malloc(strlen(path) + strlen(name) + 4);
            if (!fullpath) goto fail;
            yy_path_combine(fullpath, path, name, NULL);
            free(name);
            if (fullpath) name = fullpath;
            else break;
        }
        names[idx] = name;
        idx++;
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
    
    if (idx > 1) qsort(names, idx, sizeof(char *), strcmp_func);
    names[idx] = NULL;
    if (count) *count = idx;
    return names;
    
fail:
    if (handle != -1)_findclose(handle);
    if (search) free(search);
    if (names) free(names);
    return NULL;
    
#else
    DIR *dir = NULL;
    struct dirent *entry;
    int idx = 0, alc = 0;
    char **names = NULL, **names_tmp;
    
    if (count) *count = 0;
    if (!path || !strlen(path) || !(dir = opendir(path))) {
        goto fail;
    }
    
    alc = 4;
    names = calloc(1, alc * sizeof(char *));
    if (!names) goto fail;
    
    while ((entry = readdir(dir))) {
        char *name = (char *)entry->d_name;
        if (!name || !strlen(name)) continue;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (idx + 1 >= alc) {
            alc *= 2;
            names_tmp = realloc(names, alc * sizeof(char *));
            if (!names_tmp)
                goto fail;
            names = names_tmp;
        }
        name = strdup(name);
        if (!name) goto fail;
        if (full) {
            char *fullpath = malloc(strlen(path) + strlen(name) + 4);
            if (!fullpath) goto fail;
            yy_path_combine(fullpath, path, name, NULL);
            free(name);
            if (fullpath) name = fullpath;
            else break;
        }
        names[idx] = name;
        idx++;
    }
    closedir(dir);
    if (idx > 1) qsort(names, idx, sizeof(char *), strcmp_func);
    names[idx] = NULL;
    if (count) *count = idx;
    return names;
    
fail:
    if (dir) closedir(dir);
    yy_dir_free(names);
    return NULL;
#endif
}

char **yy_dir_read(const char *path, int *count) {
    return yy_dir_read_opts(path, count, false);
}

char **yy_dir_read_full(const char *path, int *count) {
    return yy_dir_read_opts(path, count, true);
}

void yy_dir_free(char **names) {
    if (names) {
        for (int i = 0; ; i++) {
            if (names[i]) free(names[i]);
            else break;
        }
        free(names);
    }
}



bool yy_file_read(const char *path, u8 **dat, usize *len) {
    return yy_file_read_with_padding(path, dat, len, 1); // for string
}

bool yy_file_read_with_padding(const char *path, u8 **dat, usize *len, usize padding) {
    if (!path || !strlen(path)) return false;
    if (!dat || !len) return false;
    
    FILE *file = NULL;
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, "rb") != 0) return false;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    void *buf = malloc((usize)file_size + padding);
    if (buf == NULL) {
        fclose(file);
        return false;
    }
    
    if (file_size > 0) {
#if _MSC_VER >= 1400
        if (fread_s(buf, file_size, file_size, 1, file) != 1) {
            free(buf);
            fclose(file);
            return false;
        }
#else
        if (fread(buf, file_size, 1, file) != 1) {
            free(buf);
            fclose(file);
            return false;
        }
#endif
    }
    fclose(file);
    
    memset((char *)buf + file_size, 0, padding);
    *dat = (u8 *)buf;
    *len = (usize)file_size;
    return true;
}

bool yy_file_write(const char *path, u8 *dat, usize len) {
    if (!path || !strlen(path)) return false;
    if (len && !dat) return false;
    
    FILE *file = NULL;
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, "wb") != 0) return false;
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL) return false;
    if (fwrite(dat, len, 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        file = NULL;
        return false;
    }
    return true;
}

bool yy_file_delete(const char *path) {
    if (!path || !*path) return false;
    return remove(path) == 0;
}



/*==============================================================================
 * String Utils
 *============================================================================*/

char *yy_str_copy(const char *str) {
    if (!str) return NULL;
    usize len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

bool yy_str_contains(const char *str, const char *search) {
    if (!str || !search) return false;
    return strstr(str, search) != NULL;
}

bool yy_str_has_prefix(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    usize len1 = strlen(str);
    usize len2 = strlen(prefix);
    if (len2 > len1) return false;
    return memcmp(str, prefix, len2) == 0;
}

bool yy_str_has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    usize len1 = strlen(str);
    usize len2 = strlen(suffix);
    if (len2 > len1) return false;
    return memcmp(str + (len1 - len2), suffix, len2) == 0;
}



/*==============================================================================
 * Memory Buffer
 *============================================================================*/

bool yy_buf_init(yy_buf *buf, usize len) {
    if (!buf) return false;
    if (len < 16) len = 16;
    memset(buf, 0, sizeof(yy_buf));
    buf->hdr = malloc(len);
    if (!buf->hdr) return false;
    buf->cur = buf->hdr;
    buf->end = buf->hdr + len;
    buf->need_free = true;
    return true;
}

void yy_buf_release(yy_buf *buf) {
    if (!buf || !buf->hdr) return;
    if (buf->need_free) free(buf->hdr);
    memset(buf, 0, sizeof(yy_buf));
}

usize yy_buf_len(yy_buf *buf) {
    if (!buf) return 0;
    return buf->cur - buf->hdr;
}

bool yy_buf_grow(yy_buf *buf, usize len) {
    if (!buf) return false;
    if ((usize)(buf->end - buf->cur) >= len) return true;
    if (!buf->hdr) return yy_buf_init(buf, len);
    
    usize use = buf->cur - buf->hdr;
    usize alc = buf->end - buf->hdr;
    do {
        if (alc * 2 < alc) return false; /* overflow */
        alc *= 2;
    } while(alc - use < len);
    u8 *tmp = (u8 *)realloc(buf->hdr, alc);
    if (!tmp) return false;
    
    buf->cur = tmp + (buf->cur - buf->hdr);
    buf->hdr = tmp;
    buf->end = tmp + alc;
    return true;
}

bool yy_buf_append(yy_buf *buf, u8 *dat, usize len) {
    if (!buf) return false;
    if (len == 0) return true;
    if (!dat) return false;
    if (!yy_buf_grow(buf, len)) return false;
    memcpy(buf->cur, dat, len);
    buf->cur += len;
    return true;
}



/*==============================================================================
 * String Builder
 *============================================================================*/

bool yy_sb_init(yy_sb *sb, usize len) {
    return yy_buf_init(sb, len);
}

void yy_sb_release(yy_sb *sb) {
    yy_buf_release(sb);
}

usize yy_sb_get_len(yy_sb *sb) {
    if (!sb) return 0;
    return sb->cur - sb->hdr;
}

char *yy_sb_get_str(yy_sb *sb) {
    if (!sb || !sb->hdr) return NULL;
    if (sb->cur >= sb->end) {
        if (!yy_buf_grow(sb, 1)) return NULL;
    }
    *sb->cur = '\0';
    return (char *)sb->hdr;
}

char *yy_sb_copy_str(yy_sb *sb, usize *len) {
    if (!sb || !sb->hdr) return NULL;
    usize sb_len = sb->cur - sb->hdr;
    char *str = (char *)malloc(sb_len + 1);
    if (!str) return NULL;
    memcpy(str, sb->hdr, sb_len);
    str[sb_len] = '\0';
    if (len) *len = sb_len;
    return str;
}

bool yy_sb_append(yy_sb *sb, const char *str) {
    if (!str) return false;
    usize len = strlen(str);
    if (!yy_buf_grow(sb, len + 1)) return false;
    memcpy(sb->cur, str, len);
    sb->cur += len;
    return true;
}

bool yy_sb_append_html(yy_sb *sb, const char *str) {
    if (!sb || !str) return false;
    const char *cur = str;
    usize hdr_pos = sb->cur - sb->hdr;
    while (true) {
        usize esc_len;
        const char *esc;
        if (*cur == '\0') break;
        switch (*cur) {
            case '"': esc = "&quot;"; esc_len = 6; break;
            case '&': esc = "&amp;"; esc_len = 5; break;
            case '\'': esc = "&#39;"; esc_len = 5; break;
            case '<': esc = "&lt;"; esc_len = 4; break;
            case '>': esc = "&gt;"; esc_len = 4; break;
            default: esc = NULL; esc_len = 0; break;
        }
        if (esc_len) {
            usize len = cur - str;
            if (!yy_buf_grow(sb, len + esc_len + 1)) {
                sb->cur = sb->hdr + hdr_pos;
                return false;
            }
            memcpy(sb->cur, str, len);
            sb->cur += len;
            memcpy(sb->cur, esc, esc_len);
            sb->cur += esc_len;
            str = cur + 1;
        }
        cur++;
    }
    if (cur != str) {
        if (!yy_sb_append(sb, str)) {
            sb->cur = sb->hdr + hdr_pos;
            return false;
        }
    }
    return true;
}

bool yy_sb_append_esc(yy_sb *sb, char esc, const char *str) {
    if (!sb || !str) return false;
    const char *cur = str;
    usize hdr_pos = sb->cur - sb->hdr;
    while (true) {
        char c = *cur;
        if (c == '\0') break;
        if (c == '\\') {
            if (*(cur++) == '\0') break;
            else continue;
        }
        if (c == esc) {
            usize len = cur - str + 2;
            if (!yy_buf_grow(sb, len + 1)) {
                sb->cur = sb->hdr + hdr_pos;
                return false;
            }
            memcpy(sb->cur, str, len - 2);
            sb->cur[len - 2] = '\\';
            sb->cur[len - 1] = esc;
            sb->cur += len;
            str = cur + 1;
        }
        cur++;
    }
    if (cur != str) {
        if (!yy_sb_append(sb, str)) {
            sb->cur = sb->hdr + hdr_pos;
            return false;
        }
    }
    return true;
}

bool yy_sb_printf(yy_sb *sb, const char *fmt, ...) {
    if (!sb || !fmt) return false;
    usize incr_size = 64;
    int len = 0;
    do {
        if (!yy_buf_grow(sb, incr_size + 1)) return false;
        va_list args;
        va_start(args, fmt);
        len = vsnprintf((char *)sb->cur, incr_size, fmt, args);
        va_end(args);
        if (len < 0) return false; /* error */
        if ((usize)len < incr_size) break; /* success */
        if (incr_size * 2 < incr_size) return false; /* overflow */
        incr_size *= 2;
    } while (true);
    sb->cur += len;
    return true;
}



/*==============================================================================
 * Data Reader
 *============================================================================*/

bool yy_dat_init_with_file(yy_dat *dat, const char *path) {
    u8 *mem;
    usize len;
    if (!dat) return false;
    memset(dat, 0, sizeof(yy_dat));
    if (!yy_file_read(path, &mem, &len)) return false;
    dat->hdr = mem;
    dat->cur = mem;
    dat->end = mem + len;
    dat->need_free = true;
    return true;
}

bool yy_dat_init_with_mem(yy_dat *dat, u8 *mem, usize len) {
    if (!dat) return false;
    if (len && !mem) return false;
    dat->hdr = mem;
    dat->cur = mem;
    dat->end = mem + len;
    dat->need_free = false;
    return true;
}

void yy_dat_release(yy_dat *dat) {
    yy_buf_release(dat);
}

void yy_dat_reset(yy_dat *dat) {
    if (dat) dat->cur = dat->hdr;
}
char *yy_dat_read_line(yy_dat *dat, usize *len) {
    if (len) *len = 0;
    if (!dat || dat->cur >= dat->end) return NULL;
    
    u8 *str = dat->cur;
    u8 *cur = dat->cur;
    u8 *end = dat->end;
    while (cur < end && *cur != '\r' && *cur != '\n' && *cur != '\0') cur++;
    if (len) *len = cur - str;
    if (cur < end) {
        if (cur + 1 < end && *cur == '\r' && cur[1] == '\n') cur += 2;
        else if (*cur == '\r' || *cur == '\n' || *cur == '\0') cur++;
    }
    dat->cur = cur;
    return (char *)str;
}

char *yy_dat_copy_line(yy_dat *dat, usize *len) {
    if (len) *len = 0;
    usize _len;
    char *_str = yy_dat_read_line(dat, &_len);
    if (!_str) return NULL;
    char *str = malloc(_len + 1);
    if (!str) return NULL;
    memcpy(str, _str, _len);
    str[_len] = '\0';
    if (len) *len = _len;
    return str;
}



/*==============================================================================
 * Chart Report
 *============================================================================*/

#define ARR_TYPE(type) yy_buf
#define ARR_INIT(arr) yy_buf_init(&(arr), 0)
#define ARR_RELEASE(arr) yy_buf_release(&(arr))
#define ARR_HEAD(arr, type) (((type *)(&(arr))->hdr))
#define ARR_GET(arr, type, idx) (((type *)(&(arr))->hdr) + idx)
#define ARR_COUNT(arr, type) (yy_buf_len(&(arr)) / sizeof(type))
#define ARR_ADD(arr, value, type) (yy_buf_append(&(arr), (void *)&(value), sizeof(type)))

typedef struct {
    f64 v;
    bool is_integer;
    bool is_null;
} yy_chart_value;

typedef struct {
    const char *name;
    ARR_TYPE(yy_chart_value) values;
    f64 avg_value;
} yy_chart_item;

struct yy_chart {
    yy_chart_options options;
    ARR_TYPE(yy_chart_item) items;
    bool item_opened;
    bool options_need_release;
    int ref_count;
};

void yy_chart_options_init(yy_chart_options *op) {
    if (!op) return;
    memset(op, 0, sizeof(yy_chart_options));
    op->type = YY_CHART_LINE;
    op->width = 800;
    op->height = 500;
    
    op->h_axis.min = op->h_axis.max = op->h_axis.tick_interval = NAN;
    op->h_axis.allow_decimals = true;
    op->v_axis.min = op->v_axis.max = op->v_axis.tick_interval = NAN;
    op->v_axis.allow_decimals = true;

    op->tooltip.value_decimals = -1;
    
    op->legend.enabled = true;
    op->legend.layout = YY_CHART_VERTICAL;
    op->legend.h_align = YY_CHART_RIGHT;
    op->legend.v_align = YY_CHART_MIDDLE;
    
    op->plot.point_interval = 1;
    op->plot.group_padding = 0.2f;
    op->plot.point_padding = 0.1f;
    op->plot.border_width = 1.0f;
    op->plot.value_labels_decimals = -1;
}

static int yy_chart_axis_category_count(yy_chart_axis_options *op) {
    int i;
    if (!op || !op->categories) return 0;
    for (i = 0; op->categories[i]; i++) {}
    return i;
}

static void yy_chart_axis_options_release(yy_chart_axis_options *op) {
    if (!op) return;
    if (op->title) free((void *)op->title);
    if (op->label_prefix) free((void *)op->label_prefix);
    if (op->label_suffix) free((void *)op->label_suffix);
    if (op->categories) {
        int i;
        for (i = 0; op->categories[i]; i++) {
            free((void *)op->categories[i]);
        }
        free((void *)op->categories);
    }
}

static bool yy_chart_axis_options_copy(yy_chart_axis_options *dst,
                                       yy_chart_axis_options *src) {
#define RETURN_FAIL() do { yy_chart_axis_options_release(dst); return false;} while (0)
#define STR_COPY(name) do { \
    if (src->name) { \
        dst->name = yy_str_copy(src->name); \
        if (!(dst->name)) RETURN_FAIL(); \
    }  } while(0)
    if (!src || !dst) return false;
    STR_COPY(title);
    STR_COPY(label_prefix);
    STR_COPY(label_suffix);
    if (src->categories) {
        int i, count;
        count = yy_chart_axis_category_count(src);
        if (count) {
            dst->categories = calloc(count, sizeof(char *));
            if (!dst->categories) RETURN_FAIL();
            for (i = 0 ; i < count; i++) STR_COPY(categories[i]);
        }
    }
    return true;
#undef STR_COPY
#undef RETURN_FAIL
}

static void yy_chart_options_release(yy_chart_options *op) {
#define STR_FREE(name) if (op->name) free((void *)op->name)
    if (!op) return;
    STR_FREE(title);
    STR_FREE(subtitle);
    STR_FREE(tooltip.value_prefix);
    STR_FREE(tooltip.value_suffix);
    yy_chart_axis_options_release(&op->h_axis);
    yy_chart_axis_options_release(&op->v_axis);
#undef STR_FREE
}

static bool yy_chart_options_copy(yy_chart_options *dst,
                                  yy_chart_options *src) {
#define RETURN_FAIL() do { yy_chart_options_release(dst); return false;} while (0)
#define STR_COPY(name) do { \
    if (src->name) { \
        dst->name = yy_str_copy(src->name); \
        if (!(dst->name)) RETURN_FAIL(); \
    } } while(0)
    if (!src || !dst) return false;
    *dst = *src;
    STR_COPY(title);
    STR_COPY(subtitle);
    STR_COPY(tooltip.value_prefix);
    STR_COPY(tooltip.value_suffix);
    if (!yy_chart_axis_options_copy(&dst->h_axis, &src->h_axis)) RETURN_FAIL();
    if (!yy_chart_axis_options_copy(&dst->v_axis, &src->v_axis)) RETURN_FAIL();
    return true;
#undef STR_COPY
#undef RETURN_FAIL
}

static bool yy_chart_item_release(yy_chart_item *item) {
    if (!item) return false;
    if (item->name) free((void *)item->name);
    ARR_RELEASE(item->values);
    return true;
}

static bool yy_chart_item_init(yy_chart_item *item, const char *name) {
    if (!item || !name) return false;
    memset(item, 0, sizeof(yy_chart_item));
    if (!ARR_INIT(item->values)) return false;
    item->name = yy_str_copy(name);
    if (!item->name) {
        ARR_RELEASE(item->values);
        return false;
    }
    return true;
}

yy_chart *yy_chart_new(void) {
    yy_chart *chart = calloc(1, sizeof(struct yy_chart));
    if (!chart) return NULL;
    yy_chart_options_init(&chart->options);
    chart->ref_count = 1;
    return chart;
}

void yy_chart_free(yy_chart *chart) {
    if (!chart) return;
    chart->ref_count--;
    if (chart->ref_count > 0) return;
    if (chart->options_need_release) yy_chart_options_release(&chart->options);
    free((void *)chart);
}

bool yy_chart_set_options(yy_chart *chart, yy_chart_options *op) {
    if (!chart || !op) return false;
    if (chart->options_need_release) {
        yy_chart_options_release(&chart->options);
    }
    if (yy_chart_options_copy(&chart->options, op)) {
        chart->options_need_release = true;
        return true;
    } else {
        yy_chart_options_init(&chart->options);
        chart->options_need_release = false;
        return false;
    }
}

bool yy_chart_item_begin(yy_chart *chart, const char *name) {
    yy_chart_item item;
    
    if (!chart || !name) return false;
    if (chart->item_opened) return false;
    
    if (!yy_chart_item_init(&item, name)) return false;
    if (!ARR_ADD(chart->items, item, yy_chart_item)) {
        yy_chart_item_release(&item);
        return false;
    }
    chart->item_opened = true;
    return true;
}

bool yy_chart_item_add_int(yy_chart *chart, int value) {
    size_t count;
    yy_chart_item *item;
    yy_chart_value cvalue;
    
    if (!chart || !chart->item_opened) return false;
    cvalue.v = value;
    cvalue.is_integer = true;
    cvalue.is_null = false;
    count = ARR_COUNT(chart->items, yy_chart_item);
    item = ARR_GET(chart->items, yy_chart_item, count - 1);
    return ARR_ADD(item->values, cvalue, yy_chart_value);
}

bool yy_chart_item_add_float(yy_chart *chart, float value) {
    size_t count;
    yy_chart_item *item;
    yy_chart_value cvalue;
    
    if (!chart || !chart->item_opened) return false;
    cvalue.v = value;
    cvalue.is_integer = false;
    cvalue.is_null = !isfinite(cvalue.v);
    count = ARR_COUNT(chart->items, yy_chart_item);
    item = ARR_GET(chart->items, yy_chart_item, count - 1);
    return ARR_ADD(item->values, cvalue, yy_chart_value);
}

bool yy_chart_item_end(yy_chart *chart) {
    if (!chart) return false;
    if (!chart->item_opened) return false;
    chart->item_opened = false;
    return true;
}

bool yy_chart_item_with_int(yy_chart *chart, const char *name, int value) {
    if (!yy_chart_item_begin(chart, name) ||
        !yy_chart_item_add_int(chart, value) ||
        !yy_chart_item_end(chart)) return false;
    return true;
}

bool yy_chart_item_with_float(yy_chart *chart, const char *name, float value) {
    if (!yy_chart_item_begin(chart, name) ||
        !yy_chart_item_add_float(chart, value) ||
        !yy_chart_item_end(chart)) return false;
    return true;
}

static int yy_chart_item_cmp_value_asc(const void *p1, const void *p2) {
    f64 v1, v2;
    v1 = ((yy_chart_item *)p1)->avg_value;
    v2 = ((yy_chart_item *)p2)->avg_value;
    if (v1 == v2) return 0;
    return v1 < v2 ? -1 : 1;
}

static int yy_chart_item_cmp_value_desc(const void *p1, const void *p2) {
    f64 v1, v2;
    v1 = ((yy_chart_item *)p1)->avg_value;
    v2 = ((yy_chart_item *)p2)->avg_value;
    if (v1 == v2) return 0;
    return v1 > v2 ? -1 : 1;
}

bool yy_chart_sort_items_with_value(yy_chart *chart, bool ascent) {
    size_t i, imax, j, jmax, count;
    f64 sum;
    
    for ((void)(i = 0), imax = ARR_COUNT(chart->items, yy_chart_item); i < imax; i++) {
        yy_chart_item *item = ARR_GET(chart->items, yy_chart_item, i);
        count = 0;
        sum = 0;
        for ((void)(j = 0), jmax = ARR_COUNT(item->values, yy_chart_value); j < jmax; j++) {
            yy_chart_value *value = ARR_GET(item->values, yy_chart_value, j);
            if (!value->is_null) {
                count++;
                sum += value->v;
            }
        }
        item->avg_value = count ? sum / count : 0;
    }
    
    if (imax <= 1) return true;
    qsort(chart->items.hdr, imax, sizeof(yy_chart_item),
          ascent ? yy_chart_item_cmp_value_asc : yy_chart_item_cmp_value_desc);
    return true;
}

static int yy_chart_item_cmp_name_asc(const void *p1, const void *p2) {
    return strcmp(((yy_chart_item *)p1)->name, ((yy_chart_item *)p2)->name);
}

static int yy_chart_item_cmp_name_desc(const void *p1, const void *p2) {
    return -strcmp(((yy_chart_item *)p1)->name, ((yy_chart_item *)p2)->name);
}

bool yy_chart_sort_items_with_name(yy_chart *chart, bool ascent) {
    size_t count;
    
    if (!chart) return false;
    count = ARR_COUNT(chart->items, yy_chart_item);
    if (count <= 1) return true;
    qsort(chart->items.hdr, count, sizeof(yy_chart_item),
          ascent ? yy_chart_item_cmp_name_asc : yy_chart_item_cmp_name_desc);
    return true;
}



struct yy_report {
    ARR_TYPE(yy_chart *) charts;
    ARR_TYPE(char *) infos;
};

yy_report *yy_report_new(void) {
    yy_report *report = calloc(1, sizeof(yy_report));
    return report;
}

void yy_report_free(yy_report *report) {
    if (!report) return;
    usize i, count;
    
    count = ARR_COUNT(report->charts, yy_chart);
    for (i = 0; i < count; i++) {
        yy_chart_free(*ARR_GET(report->charts, yy_chart *, i));
    }
    
    count = ARR_COUNT(report->infos, char *);
    for (i = 0; i < count; i++) {
        free(*ARR_GET(report->infos, char *, i));
    }
}

bool yy_report_add_chart(yy_report *report, yy_chart *chart) {
    if (!report || !chart) return false;
    if (ARR_ADD(report->charts, chart, yy_chart *)) {
        chart->ref_count++;
        return true;
    }
    return false;
}

bool yy_report_add_info(yy_report *report, const char *info) {
    if (!report || !info) return false;
    char *str = yy_str_copy(info);
    if (!str) return false;
    if (ARR_ADD(report->infos, str, char *)) {
        return true;
    } else {
        free(str);
        return false;
    }
}

bool yy_report_add_env_info(yy_report *report) {
    char info[1024];
    snprintf(info, sizeof(info), "Compiler: %s", yy_env_get_compiler_desc());
    if (!yy_report_add_info(report, info)) return false;
    snprintf(info, sizeof(info), "OS: %s", yy_env_get_os_desc());
    if (!yy_report_add_info(report, info)) return false;
    snprintf(info, sizeof(info), "CPU: %s", yy_env_get_cpu_desc());
    if (!yy_report_add_info(report, info)) return false;
    snprintf(info, sizeof(info), "CPU Frequency: %.2f MHz", yy_cpu_get_freq() / 1000.0 / 1000.0);
    if (!yy_report_add_info(report, info)) return false;
    return true;
}

bool yy_report_write_html_string(yy_report *report, char **html, usize *len) {
    /* append line string */
#define LS(str) do { \
    if (!yy_sb_append(sb, str)) goto fail; \
    if (!yy_sb_append(sb, "\n")) goto fail; } while(0)
    /* append line format */
#define LF(str, arg) do { \
    if (!yy_sb_printf(sb, str, arg)) goto fail; \
    if (!yy_sb_append(sb, "\n")) goto fail; } while(0)
    /* append string */
#define AS(str) do { if (!yy_sb_append(sb, str)) goto fail; } while(0)
    /* append string format */
#define AF(str, arg) do { if (!yy_sb_printf(sb, str, arg)) goto fail; } while(0)
    /* append string escaped (single quote) */
#define AE(str) do { if (!yy_sb_append_esc(sb, '\'', str)) goto fail; } while(0)
    /* append string escaped (html) */
#define AH(str) do { if (!yy_sb_append_html(sb, str)) goto fail; } while(0)
    /* string with default value */
#define STRDEF(str, def) ((str) ? (str) : (def))
    /* string with bool */
#define STRBOOL(flag) ((flag) ? "true" : "false")
    
    yy_chart_options *op;
    yy_chart_axis_options *x_axis;
    yy_chart_axis_options *y_axis;
    yy_chart_item *item;
    yy_chart_value *val;
    yy_sb _sb;
    yy_sb *sb;
    const char *str;
    const char **str_arr;
    usize i, c, v, val_count, cate_count, max_count;
    
    usize chart_count = ARR_COUNT(report->charts, yy_chart *);
    usize info_count = ARR_COUNT(report->infos, char *);
    
    if (len) *len = 0;
    if (!report || !html) return false;
    if (!yy_sb_init(&_sb, 0)) return false;
    sb = &_sb;
    
    LS("<!DOCTYPE html>");
    LS("<html>");
    LS("<head>");
    LS("<meta charset='utf-8'>");
    LS("<title>Report</title>");
    LS("<script src='https://cdnjs.cloudflare.com/ajax/libs/highcharts/8.2.0/highcharts.min.js'></script>");
    LS("<script src='https://cdnjs.cloudflare.com/ajax/libs/highcharts/8.2.0/modules/series-label.min.js'></script>");
    LS("<script src='https://cdnjs.cloudflare.com/ajax/libs/highcharts/8.2.0/modules/exporting.min.js'></script>");
    LS("<script src='https://cdnjs.cloudflare.com/ajax/libs/highcharts/8.2.0/modules/export-data.min.js'></script>");
    LS("<script src='https://cdnjs.cloudflare.com/ajax/libs/highcharts/8.2.0/modules/offline-exporting.min.js'></script>");
    LS("<link rel='stylesheet' type='text/css' href='https://cdnjs.cloudflare.com/ajax/libs/bulma/0.9.0/css/bulma.min.css'>");
    LS("<style type='text/css'> hr { height: 1px; margin: 5px 0; background-color: #999999; } </style>");
    LS("</head>");
    LS("<body>");
    LS("<nav class='navbar is-light is-fixed-top' role='navigation' aria-label='main navigation'>");
    LS("    <div class='navbar-brand'>");
    LS("        <a class='navbar-item' href='#'>Report</a>");
    LS("        <a role='button' class='navbar-burger burger' aria-label='menu' data-target='main-menu'");
    LS("            onclick='document.querySelector(\".navbar-menu\").classList.toggle(\"is-active\");'>");
    LS("            <span aria-hidden='true'></span>");
    LS("            <span aria-hidden='true'></span>");
    LS("            <span aria-hidden='true'></span>");
    LS("        </a>");
    LS("    </div>");
    LS("    <div id='main-menu' class='navbar-menu'>");
    LS("        <div class='navbar-start'>");
    if (info_count) {
        LS("            <div class='navbar-item has-dropdown is-hoverable'>");
        LS("                <a class='navbar-link'>Info</a>");
        LS("                <div class='navbar-dropdown'>");
        for (c = 0; c < info_count; c++) {
            char *info = *ARR_GET(report->infos, char *, c);
            AS("                    <a class='navbar-item'>"); AH(info); LS("</a>");
        }
        LS("                </div>");
        LS("            </div>");
    }
    LS("            <div class='navbar-item has-dropdown is-hoverable'>");
    LS("                <a class='navbar-link'>Charts</a>");
    LS("                <div class='navbar-dropdown'>");
    for (c = 0; c < chart_count; c++) {
        yy_chart *chart = *ARR_GET(report->charts, yy_chart *, c);
        AF("                    <a class='navbar-item' href='#chart_%d'>", c);
        AH(STRDEF(chart->options.title, "Unnamed Chart"));
        LS("</a>");
    }
    LS("                </div>");
    LS("            </div>");
    LS("        </div>");
    LS("    </div>");
    LS("</nav>");
    
    for (c = 0; c < chart_count; c++) {
        yy_chart *chart = *ARR_GET(report->charts, yy_chart *, c);
        usize item_count = ARR_COUNT(chart->items, yy_chart_item);
        
        op = &chart->options;
        if (op->type == YY_CHART_BAR) {
            x_axis = &op->v_axis;
            y_axis = &op->h_axis;
        } else {
            x_axis = &op->h_axis;
            y_axis = &op->v_axis;
        }
        
        LS("");
        LF("<a name='chart_%d'></a>", c);
        LS("<div style='width: 60px; height: 60px; margin: 0 auto'></div>");
        AF("<div id='chart_id_%d' style='", c);
        AF("width: %dpx; ", op->width > 0 ? op->width : 800);
        AF("height: %dpx; ", op->height > 0 ? op->height : 500);
        LS("margin: 0 auto'></div>");
        LS("<script type='text/javascript'>");
        LF("Highcharts.chart('chart_id_%d', {", c);
        
        switch (op->type) {
            case YY_CHART_LINE: str = "line"; break;
            case YY_CHART_BAR: str = "bar"; break;
            case YY_CHART_COLUMN: str = "column"; break;
            case YY_CHART_PIE: str = "pie"; break;
            default: str = "line"; break;
        }
        LF("    chart: { type: '%s' },", str);
        AS("    title: { text: '"); AE(STRDEF(op->title, "Unnamed Chart")); LS("' },");
        if (op->subtitle) {
            AS("    subtitle: { text: '"); AE(op->subtitle); LS("' },");
        }
        LS("    credits: { enabled: false },");
        
        /* colors */
        if (op->colors && op->colors[0]) {
            AS("    colors: [");
            for(str_arr = op->colors; str_arr[0]; str_arr++) {
                AS("'"); AE(STRDEF(str_arr[0], "null")); AS("'");
                if (str_arr[1]) AS(", ");
            }
            LS("],");
        }
        
        /* x axis */
        AS("    xAxis: { ");
        if (x_axis->title) {
            AS("title: { text: '"); AE(x_axis->title); AS("' }, ");
        }
        if (x_axis->label_prefix || x_axis->label_suffix) {
            AS("labels: { format: '");
            AE(STRDEF(x_axis->label_prefix, ""));
            AS("{value}");
            AE(STRDEF(x_axis->label_suffix, ""));
            AS("' }, ");
        }
        if (isfinite(x_axis->min)) AF("min: %f, ", x_axis->min);
        if (isfinite(x_axis->max)) AF("max: %f, ", x_axis->max);
        if (isfinite(x_axis->tick_interval)) AF("tickInterval: %f, ", x_axis->tick_interval);
        AF("allowDecimals: %s, ", STRBOOL(x_axis->allow_decimals));
        if (op->type == YY_CHART_LINE && x_axis->categories && x_axis->categories[0]) {
            AS("categories: [");
            for(str_arr = x_axis->categories; str_arr[0]; str_arr++) {
                AS("'"); AE(STRDEF(str_arr[0], "null")); AS("'");
                if (str_arr[1]) AS(", ");
            }
            AS("], ");
        }
        if ((op->type == YY_CHART_BAR || op->type == YY_CHART_COLUMN) && item_count) {
            AS("categories: [");
            for(i = 0; i < item_count; i++) {
                item = ARR_GET(chart->items, yy_chart_item, i);
                AS("'"); AE(STRDEF(item->name, "null")); AS("'");
                if (i + 1 < item_count) AS(", ");
            }
            AS("], ");
        }
        AF("type: '%s' ", x_axis->logarithmic ? "logarithmic" : "linear");
        LS("},");
        
        /* y axis */
        AS("    yAxis: { ");
        if (y_axis->title) {
            AS("title: { text: '"); AE(y_axis->title); AS("' }, ");
        }
        if (y_axis->label_prefix || y_axis->label_suffix) {
            AS("labels: { format: '");
            AE(STRDEF(y_axis->label_prefix, ""));
            AS("{value}");
            AE(STRDEF(y_axis->label_suffix, ""));
            AS("' }, ");
        }
        if (isfinite(y_axis->min)) AF("min: %f, ", y_axis->min);
        if (isfinite(y_axis->max)) AF("max: %f, ", y_axis->max);
        if (isfinite(y_axis->tick_interval)) AF("tickInterval: %f, ", y_axis->tick_interval);
        AF("allowDecimals: %s, ", STRBOOL(y_axis->allow_decimals));
        AF("type: '%s' ", y_axis->logarithmic ? "logarithmic" : "linear");
        LS("},");
                    
        /* tooltip */
        AS("    tooltip: {");
        if (op->tooltip.value_decimals >= 0) {
            AF("valueDecimals: %d, ", op->tooltip.value_decimals);
        }
        if (op->tooltip.value_prefix) {
            AS("valuePrefix: '"); AE(op->tooltip.value_prefix); AS("', ");
        }
        if (op->tooltip.value_suffix) {
            AS("valueSuffix: '"); AE(op->tooltip.value_suffix); AS("', ");
        }
        AF("shared: %s, ", STRBOOL(op->tooltip.shared));
        AF("crosshairs: %s, ", STRBOOL(op->tooltip.crosshairs));
        AS("shadow: false ");
        LS("},");
        
        /* legend */
        AS("    legend: { ");
        switch (op->legend.layout) {
            case YY_CHART_HORIZONTAL: str = "horizontal"; break;
            case YY_CHART_VERTICAL: str = "vertical"; break;
            case YY_CHART_PROXIMATE: str = "proximate"; break;
            default: str = "horizontal"; break;
        }
        AF("layout: '%s', ", str);
        switch (op->legend.h_align) {
            case YY_CHART_LEFT: str = "left"; break;
            case YY_CHART_CENTER: str = "center"; break;
            case YY_CHART_RIGHT: str = "right"; break;
            default: str = "center"; break;
        }
        AF("align: '%s', ", str);
        switch (op->legend.v_align) {
            case YY_CHART_TOP: str = "top"; break;
            case YY_CHART_MIDDLE: str = "middle"; break;
            case YY_CHART_BOTTOM: str = "bottom"; break;
            default: str = "bottom"; break;
        }
        AF("verticalAlign: '%s', ", str);
        AF("enabled: %s ", STRBOOL(op->legend.enabled));
        LS("},");
        
        /* plot */
        LS("    plotOptions: {");
        if (op->type == YY_CHART_LINE) {
            AF("        line: { pointStart: %f, ", op->plot.point_start);
            LF("pointInterval: %f },", op->plot.point_interval);
        }
        if (op->type == YY_CHART_BAR ||
            op->type == YY_CHART_COLUMN) {
            AF("        %s: { ", op->type == YY_CHART_BAR ? "bar" : "column");
            if (isfinite(op->plot.group_padding)) {
                AF("groupPadding: %f, ", op->plot.group_padding);
            }
            if (isfinite(op->plot.point_padding)) {
                AF("pointPadding: %f, ", op->plot.point_padding);
            }
            if (isfinite(op->plot.border_width)) {
                AF("borderWidth: %f, ", op->plot.border_width);
            }
            if (op->plot.group_stacked) {
                AS("stacking: 'normal', ");
            }
            AF("colorByPoint: %s ", STRBOOL(op->plot.color_by_point));
            LS("},");
        }
        AF("        series: { label: { enabled: %s}, ", STRBOOL(op->plot.name_label_enabled));
        AF("dataLabels: { enabled: %s ", STRBOOL(op->plot.value_labels_enabled));
        AS(", allowOverlap: true");
        if (op->plot.value_labels_decimals >= 0) {
            AF(", format: '{point.y:.%df}' ", op->plot.value_labels_decimals);
        }
        LS("} }");
        LS("    },");
        
        /* data */
        LS("    series: [");
        if (op->type == YY_CHART_BAR || op->type == YY_CHART_COLUMN) {
            cate_count = (size_t)yy_chart_axis_category_count(x_axis);
            max_count = 0;
            for (i = 0; i < item_count; i++) {
                item = ARR_GET(chart->items, yy_chart_item, i);
                val_count = ARR_COUNT(item->values, yy_chart_value);
                if (val_count > max_count) max_count = val_count;
            }
            for (v = 0; v < max_count; v++) {
                AS("        { ");
                if (v < cate_count) {
                    AS("name: '"); AE(x_axis->categories[v]); AS("', ");
                }
                AS("data: [");
                for (i = 0; i < item_count; i++) {
                    item = ARR_GET(chart->items, yy_chart_item, i);
                    val_count = ARR_COUNT(item->values, yy_chart_value);
                    val = ARR_GET(item->values, yy_chart_value, v);
                    if (v >= val_count || val->is_null) AS("null");
                    else if (val->is_integer) AF("%d", (int)val->v);
                    else AF("%f", (float)val->v);
                    if (i + 1 < item_count) AS(", ");
                }
                AS("] }"); if (v + 1 < max_count) AS(","); LS("");
            }
        } else {
            for (i = 0; i < item_count; i++) {
                item = ARR_GET(chart->items, yy_chart_item, i);
                AS("        { name: '"); AE(item->name); AS("', data: [");
                for ((void)(v = 0), val_count = ARR_COUNT(item->values, yy_chart_value); v < val_count; v++) {
                    val = ARR_GET(item->values, yy_chart_value, v);
                    if (val->is_null) AS("null");
                    else if (val->is_integer) AF("%d", (int)val->v);
                    else AF("%f", (float)val->v);
                    if (v + 1 < val_count) AS(", ");
                }
                AS("] }"); if (i + 1 < item_count) AS(","); LS("");
            }
        }
        LS("    ]");
        
        LS("});");
        LS("</script>");
    }
    
    LS("");
    LS("</body>");
    AS("</html>");
    
    *html = yy_sb_get_str(sb);
    if (!*html) goto fail;
    if (len) *len = yy_sb_get_len(sb);
    return true;
    
fail:
    yy_sb_release(sb);
    return false;
}

bool yy_report_write_html_file(yy_report *report, const char *path) {
    char *html;
    usize len;
    if (!yy_report_write_html_string(report, &html, &len)) return false;
    bool suc = yy_file_write(path, (u8 *)html, len);
    free(html);
    return suc;
}
