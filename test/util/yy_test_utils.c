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
