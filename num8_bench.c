#include "num8.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static double now_sec(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f;
    LARGE_INTEGER c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

static void fmt8(uint32_t v, char out[9])
{
    out[8] = '\0';
    for (int i = 7; i >= 0; --i)
    {
        out[i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
}

static int check(num8_status_t s)
{
    return s == NUM8_STATUS_OK || s == NUM8_STATUS_ADDED || s == NUM8_STATUS_REMOVED || s == NUM8_STATUS_ALREADY_EXISTS || s == NUM8_STATUS_NOT_FOUND;
}

int main(void)
{
    const char* path = "num8_bench_tmp.num8";
    const uint32_t n_exists = 5000000u;
    const uint32_t n_mut = 1000000u;
    const uint32_t n_str = 200000u;
    const uint32_t n_flush = 8u;

    num8_engine_t e;
    num8_status_t st;
    int ex = 0;
    double t0, t1;

    uint64_t seed = 88172645463393265ull;
    uint64_t ok = 0;

    st = num8_create(path, &e);
    if (st != NUM8_STATUS_OK)
    {
        fprintf(stderr, "create failed: %d\n", (int)st);
        return 1;
    }

    for (uint32_t i = 0; i < 100000u; ++i)
    {
        (void)num8_add_u32(&e, i * 7u % 100000000u);
    }

    t0 = now_sec();
    for (uint32_t i = 0; i < n_exists; ++i)
    {
        seed = seed * 2862933555777941757ull + 3037000493ull;
        uint32_t v = (uint32_t)(seed % 100000000ull);
        st = num8_exists_u32(&e, v, &ex);
        if (!check(st))
        {
            fprintf(stderr, "exists_u32 failed: %d\n", (int)st);
            return 1;
        }
        ok += (uint64_t)ex;
    }
    t1 = now_sec();
    printf("exists_u32 avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_exists, (double)n_exists / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_mut; ++i)
    {
        st = num8_add_u32(&e, i);
        if (st != NUM8_STATUS_ADDED && st != NUM8_STATUS_ALREADY_EXISTS)
        {
            fprintf(stderr, "add_u32 failed: %d\n", (int)st);
            return 1;
        }
    }
    t1 = now_sec();
    printf("add_u32 avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_mut, (double)n_mut / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_mut; ++i)
    {
        st = num8_remove_u32(&e, i);
        if (st != NUM8_STATUS_REMOVED && st != NUM8_STATUS_NOT_FOUND)
        {
            fprintf(stderr, "remove_u32 failed: %d\n", (int)st);
            return 1;
        }
    }
    t1 = now_sec();
    printf("remove_u32 avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_mut, (double)n_mut / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_str; ++i)
    {
        char s[9];
        uint32_t v = 50000000u + i;
        fmt8(v, s);
        st = num8_add_str(&e, s);
        if (st != NUM8_STATUS_ADDED && st != NUM8_STATUS_ALREADY_EXISTS)
        {
            fprintf(stderr, "add_str failed: %d\n", (int)st);
            return 1;
        }
    }
    t1 = now_sec();
    printf("add_str avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_str, (double)n_str / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_str; ++i)
    {
        char s[9];
        uint32_t v = 50000000u + i;
        fmt8(v, s);
        st = num8_exists_str(&e, s, &ex);
        if (st != NUM8_STATUS_OK)
        {
            fprintf(stderr, "exists_str failed: %d\n", (int)st);
            return 1;
        }
        ok += (uint64_t)ex;
    }
    t1 = now_sec();
    printf("exists_str avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_str, (double)n_str / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_str; ++i)
    {
        char s[9];
        uint32_t v = 50000000u + i;
        fmt8(v, s);
        st = num8_remove_str(&e, s);
        if (st != NUM8_STATUS_REMOVED && st != NUM8_STATUS_NOT_FOUND)
        {
            fprintf(stderr, "remove_str failed: %d\n", (int)st);
            return 1;
        }
    }
    t1 = now_sec();
    printf("remove_str avg_ns=%.2f ops_per_sec=%.2f\n", (t1 - t0) * 1e9 / (double)n_str, (double)n_str / (t1 - t0));

    t0 = now_sec();
    for (uint32_t i = 0; i < n_flush; ++i)
    {
        uint32_t v = 90000000u + i;
        st = num8_add_u32(&e, v);
        if (st != NUM8_STATUS_ADDED && st != NUM8_STATUS_ALREADY_EXISTS)
        {
            fprintf(stderr, "add before flush failed: %d\n", (int)st);
            return 1;
        }
        st = num8_flush(&e);
        if (st != NUM8_STATUS_OK)
        {
            fprintf(stderr, "flush failed: %d\n", (int)st);
            return 1;
        }
    }
    t1 = now_sec();
    printf("flush avg_ms=%.3f ops_per_sec=%.2f\n", (t1 - t0) * 1e3 / (double)n_flush, (double)n_flush / (t1 - t0));

    st = num8_validate(&e);
    if (st != NUM8_STATUS_OK)
    {
        fprintf(stderr, "validate failed: %d\n", (int)st);
        return 1;
    }

    st = num8_close(&e);
    if (st != NUM8_STATUS_OK)
    {
        fprintf(stderr, "close failed: %d\n", (int)st);
        return 1;
    }

    remove(path);
    printf("bench_checksum=%" PRIu64 "\n", ok);
    return 0;
}
