#include "num8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define ASSERT_OK(expr) do { num8_status_t _st = (expr); if (_st != NUM8_STATUS_OK) { \
    fprintf(stderr, "ASSERT_OK failed at %s:%d status=%d\n", __FILE__, __LINE__, (int)_st); return 1; } } while (0)

#define ASSERT_STATUS(expr, expected) do { num8_status_t _st = (expr); if (_st != (expected)) { \
    fprintf(stderr, "ASSERT_STATUS failed at %s:%d expected=%d got=%d\n", __FILE__, __LINE__, (int)(expected), (int)_st); return 1; } } while (0)

#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    fprintf(stderr, "ASSERT_TRUE failed at %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

static uint32_t test_crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int b;

    for (i = 0; i < len; ++i)
    {
        crc ^= (uint32_t)data[i];
        for (b = 0; b < 8; ++b)
        {
            uint32_t mask = (uint32_t)(-(int)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static void test_store_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void test_store_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void test_store_le64(uint8_t* p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; ++i)
    {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

static int test_read_file(const char* path, uint8_t** out_data, size_t* out_len)
{
    FILE* f = fopen(path, "rb");
    long len;
    uint8_t* data;

    if (f == NULL)
    {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }
    len = ftell(f);
    if (len < 0)
    {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }
    data = (uint8_t*)malloc((size_t)len);
    if (data == NULL)
    {
        fclose(f);
        return 0;
    }
    if (fread(data, 1, (size_t)len, f) != (size_t)len)
    {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);
    *out_data = data;
    *out_len = (size_t)len;
    return 1;
}

static int test_write_file(const char* path, const uint8_t* data, size_t len)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL)
    {
        return 0;
    }
    if (len > 0 && fwrite(data, 1, len, f) != len)
    {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static int test_expect_open_status(const char* path, num8_status_t expected)
{
    num8_engine_t e = {0};
    num8_status_t st = num8_open(path, NUM8_OPEN_READ_ONLY, &e);
    if (st != expected)
    {
        fprintf(stderr, "open status mismatch for %s expected=%d got=%d\n", path, (int)expected, (int)st);
        return 0;
    }
    if (st == NUM8_STATUS_OK)
    {
        if (num8_close(&e) != NUM8_STATUS_OK)
        {
            fprintf(stderr, "close failed for %s\n", path);
            return 0;
        }
    }
    return 1;
}

static int test_join_path(char* out, size_t out_cap, const char* dir, const char* name)
{
    int n;
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    if (out == NULL || dir == NULL || name == NULL)
    {
        return 0;
    }
    n = snprintf(out, out_cap, "%s%c%s", dir, sep, name);
    return n >= 0 && (size_t)n < out_cap;
}

static int test_format_name(char* out, size_t out_cap, const char* prefix, unsigned long long suffix, const char* ext)
{
    int n = snprintf(out, out_cap, "%s%llu%s", prefix, suffix, ext);
    return n >= 0 && (size_t)n < out_cap;
}

static int test_current_dir(char* out, size_t out_cap)
{
#if defined(_WIN32)
    char module[MAX_PATH];
    char* slash;
    DWORD n = GetModuleFileNameA(NULL, module, (DWORD)sizeof(module));
    if (n == 0 || n >= sizeof(module))
    {
        return 0;
    }
    slash = strrchr(module, '\\');
    if (slash == NULL)
    {
        return 0;
    }
    *slash = '\0';
    if (strlen(module) + 1u > out_cap)
    {
        return 0;
    }
    strcpy(out, module);
    return 1;
#else
    if (getcwd(out, out_cap) == NULL)
    {
        return 0;
    }
    return 1;
#endif
}

static int test_file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
    {
        return 0;
    }
    fclose(f);
    return 1;
}

static int test_run_builder(const char* builder_path, const char* input_path, const char* output_path)
{
#if defined(_WIN32)
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1u;
    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", builder_path, input_path, output_path);
    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        return 0;
    }
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0)
    {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (GetExitCodeProcess(pi.hProcess, &exit_code) == 0)
    {
        exit_code = 1u;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exit_code == 0u;
#else
    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\"", builder_path, input_path, output_path);
    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        return 0;
    }
    return system(cmd) == 0;
#endif
}

static uint32_t test_random32(uint64_t* state)
{
    *state = *state * 6364136223846793005ull + 1ull;
    return (uint32_t)(*state >> 32);
}

static num8_open_mode_t test_invalid_open_mode(void)
{
    int invalid_mode = 99;
    num8_open_mode_t mode;
    memcpy(&mode, &invalid_mode, sizeof(mode));
    return mode;
}

static int test_builder_cases(const char* dir)
{
    char builder_path[512];
    char input_path[512];
    char output_path[512];
    char temp_path[512];
    char fresh_output_path[512];
    char fresh_temp_path[512];
    char input_name[128];
    char output_name[128];
    char temp_name[128];
    char fresh_output_name[128];
    char fresh_temp_name[128];
    uint8_t* before = NULL;
    uint8_t* after = NULL;
    size_t before_bytes = 0;
    size_t after_bytes = 0;
    uint64_t count = 0;
    num8_engine_t e = {0};
    unsigned long long case_suffix;

#if defined(_WIN32)
    case_suffix = (unsigned long long)GetCurrentProcessId();
#else
    case_suffix = (unsigned long long)getpid();
#endif

#if defined(_WIN32)
    ASSERT_TRUE(test_join_path(builder_path, sizeof(builder_path), dir, "num8_builder.exe"));
#else
    ASSERT_TRUE(test_join_path(builder_path, sizeof(builder_path), dir, "num8_builder"));
#endif
    ASSERT_TRUE(test_format_name(input_name, sizeof(input_name), "num8_builder_input_", case_suffix, ".txt"));
    ASSERT_TRUE(test_format_name(output_name, sizeof(output_name), "num8_builder_output_", case_suffix, ".num8"));
    ASSERT_TRUE(test_format_name(temp_name, sizeof(temp_name), "num8_builder_output_", case_suffix, ".num8.tmp"));
    ASSERT_TRUE(test_format_name(fresh_output_name, sizeof(fresh_output_name), "num8_builder_fresh_output_", case_suffix, ".num8"));
    ASSERT_TRUE(test_format_name(fresh_temp_name, sizeof(fresh_temp_name), "num8_builder_fresh_output_", case_suffix, ".num8.tmp"));
    ASSERT_TRUE(test_join_path(input_path, sizeof(input_path), dir, input_name));
    ASSERT_TRUE(test_join_path(output_path, sizeof(output_path), dir, output_name));
    ASSERT_TRUE(test_join_path(temp_path, sizeof(temp_path), dir, temp_name));
    ASSERT_TRUE(test_join_path(fresh_output_path, sizeof(fresh_output_path), dir, fresh_output_name));
    ASSERT_TRUE(test_join_path(fresh_temp_path, sizeof(fresh_temp_path), dir, fresh_temp_name));

    remove(input_path);
    remove(output_path);
    remove(temp_path);
    remove(fresh_output_path);
    remove(fresh_temp_path);

    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000001\n00000001\n99999999\n", 36u));
    ASSERT_TRUE(test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));
    ASSERT_OK(num8_open(output_path, NUM8_OPEN_READ_ONLY, &e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 3u);
    ASSERT_OK(num8_close(&e));

    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000002\n", 18u));
    ASSERT_TRUE(test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));
    ASSERT_OK(num8_open(output_path, NUM8_OPEN_READ_ONLY, &e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 2u);
    ASSERT_OK(num8_close(&e));

    ASSERT_TRUE(test_read_file(output_path, &before, &before_bytes));

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n1234567\n00000002\n", 26u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));
    ASSERT_TRUE(test_read_file(output_path, &after, &after_bytes));
    ASSERT_TRUE(before_bytes == after_bytes);
    ASSERT_TRUE(memcmp(before, after, before_bytes) == 0);
    free(after);
    after = NULL;

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n\n00000002\n", 19u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"0000000\n00000002\n", 17u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"000000000\n00000002\n", 19u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"0000000A\n00000002\n", 18u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));

    remove(input_path);
    {
        char oversized[320];
        size_t i;
        for (i = 0; i + 2u < sizeof(oversized); ++i)
        {
            oversized[i] = '1';
        }
        oversized[sizeof(oversized) - 2u] = '\n';
        oversized[sizeof(oversized) - 1u] = '\0';
        ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)oversized, sizeof(oversized) - 1u));
        ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
        ASSERT_TRUE(!test_file_exists(temp_path));
    }

    remove(input_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000002", 17u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
    ASSERT_TRUE(!test_file_exists(temp_path));

    ASSERT_TRUE(test_read_file(output_path, &after, &after_bytes));
    ASSERT_TRUE(before_bytes == after_bytes);
    ASSERT_TRUE(memcmp(before, after, before_bytes) == 0);
    free(after);
    after = NULL;

    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000001\n00000002\n", 27u));
    ASSERT_TRUE(test_run_builder(builder_path, input_path, fresh_output_path));
    ASSERT_TRUE(!test_file_exists(fresh_temp_path));
    ASSERT_OK(num8_open(fresh_output_path, NUM8_OPEN_READ_ONLY, &e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 3u);
    ASSERT_OK(num8_close(&e));

    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000001\n00000002\n00000003\n", 36u));
    ASSERT_TRUE(test_run_builder(builder_path, input_path, fresh_output_path));
    ASSERT_TRUE(!test_file_exists(fresh_temp_path));
    ASSERT_OK(num8_open(fresh_output_path, NUM8_OPEN_READ_ONLY, &e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 4u);
    ASSERT_OK(num8_close(&e));

#if defined(_WIN32)
    {
        HANDLE held = CreateFileA(output_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        ASSERT_TRUE(held != INVALID_HANDLE_VALUE);
        if (held != INVALID_HANDLE_VALUE)
        {
            remove(input_path);
            ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n00000003\n", 18u));
            ASSERT_TRUE(!test_run_builder(builder_path, input_path, output_path));
            ASSERT_TRUE(!test_file_exists(temp_path));
            CloseHandle(held);
        }
    }
#endif

    remove(input_path);
    remove(fresh_output_path);
    remove(fresh_temp_path);
    ASSERT_TRUE(test_write_file(input_path, (const uint8_t*)"00000000\n\n00000001\n", 19u));
    ASSERT_TRUE(!test_run_builder(builder_path, input_path, fresh_output_path));
    ASSERT_TRUE(!test_file_exists(fresh_output_path));
    ASSERT_TRUE(!test_file_exists(fresh_temp_path));

    remove(input_path);
    remove(output_path);
    remove(temp_path);
    remove(fresh_output_path);
    remove(fresh_temp_path);
    free(before);
    free(after);
    return 0;
}

static int test_model_randomized(const char* dir)
{
    char path[512];
    char path_name[128];
    num8_engine_t e = {0};
    uint8_t* model = NULL;
    uint64_t model_count = 0;
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    unsigned step;
    unsigned long long case_suffix;

#if defined(_WIN32)
    case_suffix = (unsigned long long)GetCurrentProcessId();
#else
    case_suffix = (unsigned long long)getpid();
#endif

    ASSERT_TRUE(test_format_name(path_name, sizeof(path_name), "num8_model_random_", case_suffix, ".num8"));
    ASSERT_TRUE(test_join_path(path, sizeof(path), dir, path_name));
    remove(path);

    model = (uint8_t*)calloc(NUM8_PAYLOAD_SIZE, 1);
    ASSERT_TRUE(model != NULL);
    ASSERT_OK(num8_create(path, &e));

    for (step = 0; step < 300u; ++step)
    {
        uint32_t op = test_random32(&seed) % 6u;
        uint32_t value = test_random32(&seed) % NUM8_DOMAIN_SIZE;
        int exists = -1;
        uint64_t count = 0;
        num8_status_t st;

        switch (op)
        {
            case 0:
                st = num8_add_u32(&e, value);
                if (model[value >> 3] & (uint8_t)(1u << (value & 7u)))
                {
                    if (st != NUM8_STATUS_ALREADY_EXISTS)
                    {
                        fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=add value=%u status=%d\n",
                                (unsigned long long)seed, step, value, (int)st);
                        free(model);
                        return 1;
                    }
                }
                else
                {
                    if (st != NUM8_STATUS_ADDED)
                    {
                        fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=add value=%u status=%d\n",
                                (unsigned long long)seed, step, value, (int)st);
                        free(model);
                        return 1;
                    }
                    model[value >> 3] |= (uint8_t)(1u << (value & 7u));
                    model_count++;
                }
                break;
            case 1:
                st = num8_remove_u32(&e, value);
                if (model[value >> 3] & (uint8_t)(1u << (value & 7u)))
                {
                    if (st != NUM8_STATUS_REMOVED)
                    {
                        fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=remove value=%u status=%d\n",
                                (unsigned long long)seed, step, value, (int)st);
                        free(model);
                        return 1;
                    }
                    model[value >> 3] &= (uint8_t)~(uint8_t)(1u << (value & 7u));
                    model_count--;
                }
                else
                {
                    if (st != NUM8_STATUS_NOT_FOUND)
                    {
                        fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=remove value=%u status=%d\n",
                                (unsigned long long)seed, step, value, (int)st);
                        free(model);
                        return 1;
                    }
                }
                break;
            case 2:
                st = num8_exists_u32(&e, value, &exists);
                if (st != NUM8_STATUS_OK)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=exists value=%u status=%d\n",
                            (unsigned long long)seed, step, value, (int)st);
                    free(model);
                    return 1;
                }
                if (((model[value >> 3] >> (value & 7u)) & 1u) != (unsigned)exists)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=exists value=%u mismatch\n",
                            (unsigned long long)seed, step, value);
                    free(model);
                    return 1;
                }
                break;
            case 3:
                st = num8_flush(&e);
                if (st != NUM8_STATUS_OK)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=flush status=%d\n",
                            (unsigned long long)seed, step, (int)st);
                    free(model);
                    return 1;
                }
                break;
            case 4:
                st = num8_validate(&e);
                if (st != NUM8_STATUS_OK)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=validate status=%d\n",
                            (unsigned long long)seed, step, (int)st);
                    free(model);
                    return 1;
                }
                break;
            case 5:
                st = num8_close(&e);
                if (st != NUM8_STATUS_OK)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=close status=%d\n",
                            (unsigned long long)seed, step, (int)st);
                    free(model);
                    return 1;
                }
                st = num8_open(path, NUM8_OPEN_READ_WRITE, &e);
                if (st != NUM8_STATUS_OK)
                {
                    fprintf(stderr, "MODEL FAIL seed=%llu step=%u op=reopen status=%d\n",
                            (unsigned long long)seed, step, (int)st);
                    free(model);
                    return 1;
                }
                break;
        }

        if (num8_count(&e, &count) != NUM8_STATUS_OK || count != model_count)
        {
            fprintf(stderr, "MODEL FAIL seed=%llu step=%u count mismatch expected=%llu got=%llu\n",
                    (unsigned long long)seed, step,
                    (unsigned long long)model_count,
                    (unsigned long long)count);
            free(model);
            return 1;
        }

        if (num8_exists_u32(&e, value, &exists) != NUM8_STATUS_OK
            || (((model[value >> 3] >> (value & 7u)) & 1u) != (unsigned)exists))
        {
            fprintf(stderr, "MODEL FAIL seed=%llu step=%u probe mismatch value=%u\n",
                    (unsigned long long)seed, step, value);
            free(model);
            return 1;
        }
        if (num8_validate(&e) != NUM8_STATUS_OK)
        {
            fprintf(stderr, "MODEL FAIL seed=%llu step=%u validate mismatch\n",
                    (unsigned long long)seed, step);
            free(model);
            return 1;
        }
    }

    ASSERT_OK(num8_close(&e));
    remove(path);
    free(model);
    return 0;
}

static int test_core(void)
{
    char path_name[128];
    char missing_name[128];
    char path[256];
    char missing_path[256];
    num8_engine_t e = {0};
    int exists = -1;
    uint64_t count = 0;
    unsigned long long case_suffix;

#if defined(_WIN32)
    case_suffix = (unsigned long long)GetCurrentProcessId();
#else
    case_suffix = (unsigned long long)getpid();
#endif

    ASSERT_TRUE(test_format_name(path_name, sizeof(path_name), "num8_selftest_tmp_", case_suffix, ".num8"));
    ASSERT_TRUE(test_format_name(missing_name, sizeof(missing_name), "num8_selftest_missing_", case_suffix, ".num8"));
    ASSERT_TRUE(test_join_path(path, sizeof(path), ".", path_name));
    ASSERT_TRUE(test_join_path(missing_path, sizeof(missing_path), ".", missing_name));

    remove(path);
    remove(missing_path);
    ASSERT_OK(num8_create(path, &e));
    {
        num8_engine_t other = {0};
        ASSERT_STATUS(num8_open(path, NUM8_OPEN_READ_WRITE, &other), NUM8_STATUS_LOCK_FAILED);
    }
    ASSERT_STATUS(num8_create(path, &e), NUM8_STATUS_ALREADY_OPEN);
    ASSERT_STATUS(num8_open(path, NUM8_OPEN_READ_ONLY, &e), NUM8_STATUS_ALREADY_OPEN);
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 0);

    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ADDED);
    ASSERT_OK(num8_validate_memory(&e));
    ASSERT_OK(num8_validate_disk(&e));
    ASSERT_STATUS(num8_add_u32(&e, 99999999u), NUM8_STATUS_ADDED);
    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ALREADY_EXISTS);
    ASSERT_STATUS(num8_add_u32(&e, 100000000u), NUM8_STATUS_INVALID_NUMBER);
    ASSERT_STATUS(num8_exists_u32(&e, 100000000u, &exists), NUM8_STATUS_INVALID_NUMBER);
    ASSERT_STATUS(num8_remove_u32(&e, 100000000u), NUM8_STATUS_INVALID_NUMBER);

    ASSERT_OK(num8_exists_u32(&e, 0u, &exists));
    ASSERT_TRUE(exists == 1);
    ASSERT_OK(num8_exists_str(&e, "99999999", &exists));
    ASSERT_TRUE(exists == 1);
    ASSERT_STATUS(num8_exists_str(&e, "9999999", &exists), NUM8_STATUS_INVALID_FORMAT);
    ASSERT_STATUS(num8_exists_str(&e, "12A45678", &exists), NUM8_STATUS_INVALID_FORMAT);

    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 2);

    ASSERT_STATUS(num8_remove_u32(&e, 1u), NUM8_STATUS_NOT_FOUND);
    ASSERT_STATUS(num8_remove_u32(&e, 0u), NUM8_STATUS_REMOVED);
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 1);

    ASSERT_OK(num8_flush(&e));
    ASSERT_OK(num8_validate(&e));
    ASSERT_OK(num8_close(&e));
    ASSERT_STATUS(num8_close(&e), NUM8_STATUS_NOT_OPEN);
    ASSERT_STATUS(num8_open(path, test_invalid_open_mode(), &e), NUM8_STATUS_INVALID_MODE);
    ASSERT_STATUS(num8_open(missing_path, NUM8_OPEN_READ_ONLY, &e), NUM8_STATUS_OPEN_FAILED);
    ASSERT_STATUS(num8_create(path, &e), NUM8_STATUS_ALREADY_EXISTS);

    ASSERT_OK(num8_open(path, NUM8_OPEN_READ_ONLY, &e));
    ASSERT_OK(num8_exists_u32(&e, 99999999u, &exists));
    ASSERT_TRUE(exists == 1);
    ASSERT_STATUS(num8_add_u32(&e, 5u), NUM8_STATUS_READ_ONLY);
    ASSERT_OK(num8_validate(&e));
    ASSERT_OK(num8_close(&e));

    ASSERT_OK(num8_open(path, NUM8_OPEN_READ_WRITE, &e));
    ASSERT_OK(num8_clear_all(&e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 0);
    ASSERT_OK(num8_flush(&e));
    ASSERT_OK(num8_close(&e));

    remove(path);
    return 0;
}

static int test_format_validation(void)
{
    const char* base_path = "num8_selftest_format_base.num8";
    const char* variant_path = "num8_selftest_format_variant.num8";
    num8_engine_t e = {0};
    uint8_t* base = NULL;
    size_t len = 0;
    uint8_t* variant = NULL;

    remove(base_path);
    remove(variant_path);
    ASSERT_OK(num8_create(base_path, &e));
    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ADDED);
    ASSERT_OK(num8_flush(&e));
    ASSERT_OK(num8_close(&e));
    ASSERT_TRUE(test_expect_open_status(base_path, NUM8_STATUS_OK));

    ASSERT_TRUE(test_read_file(base_path, &base, &len));
    ASSERT_TRUE(len == (size_t)NUM8_HEADER_SIZE + (size_t)NUM8_PAYLOAD_SIZE);
    variant = (uint8_t*)malloc(len + 1u);
    ASSERT_TRUE(variant != NULL);

    memcpy(variant, base, len);
    variant[0] = 'X';
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_CORRUPTED));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le16(&variant[4], (uint16_t)2u);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_UNSUPPORTED_VERSION));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le32(&variant[8], 63u);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_SIZE_MISMATCH));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le64(&variant[12], 99999999ull);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_CORRUPTED));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le32(&variant[32], 1u);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_SIZE_MISMATCH));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le32(&variant[28], 1u);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_CORRUPTED));
    remove(variant_path);

    memcpy(variant, base, len);
    variant[56] = 1u;
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_CORRUPTED));
    remove(variant_path);

    memcpy(variant, base, len);
    variant[36] ^= 0xFFu;
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_HEADER_CRC_MISMATCH));
    remove(variant_path);

    memcpy(variant, base, len);
    variant[NUM8_HEADER_SIZE] ^= 0x01u;
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_PAYLOAD_CRC_MISMATCH));
    remove(variant_path);

    memcpy(variant, base, len);
    test_store_le64(&variant[20], 2ull);
    test_store_le32(&variant[36], 0u);
    test_store_le32(&variant[36], test_crc32(variant, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_write_file(variant_path, variant, len));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_CORRUPTED));
    remove(variant_path);

    memcpy(variant, base, len);
    ASSERT_TRUE(test_write_file(variant_path, variant, len + 1u));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_SIZE_MISMATCH));
    remove(variant_path);

    ASSERT_TRUE(test_write_file(variant_path, base, NUM8_HEADER_SIZE));
    ASSERT_TRUE(test_expect_open_status(variant_path, NUM8_STATUS_IO_ERROR));
    remove(variant_path);

    {
        const char* bad_path = "num8_missing_dir/num8_cleanup.num8";
        char temp_candidate[256];
        FILE* probe;
        num8_engine_t tmp = {0};

        ASSERT_STATUS(num8_create(bad_path, &tmp), NUM8_STATUS_IO_ERROR);
        snprintf(temp_candidate, sizeof(temp_candidate), "%s.tmp", bad_path);
        probe = fopen(temp_candidate, "rb");
        ASSERT_TRUE(probe == NULL);
        if (probe != NULL)
        {
            fclose(probe);
        }
    }

    free(variant);
    free(base);
    remove(base_path);
    return 0;
}

int main(void)
{
    char dir[512];
    int rc = test_core();
    if (rc != 0)
    {
        return rc;
    }
    rc = test_format_validation();
    if (rc != 0)
    {
        return rc;
    }
    if (!test_current_dir(dir, sizeof(dir)))
    {
        fprintf(stderr, "failed to locate test directory\n");
        return 1;
    }
    rc = test_builder_cases(dir);
    if (rc != 0)
    {
        return rc;
    }
    rc = test_model_randomized(dir);
    if (rc != 0)
    {
        return rc;
    }
    printf("num8_selftest OK\n");
    return 0;
}
