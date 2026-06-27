#include "num8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int test_core(void)
{
    const char* path = "num8_selftest_tmp.num8";
    const char* missing_path = "num8_selftest_missing.num8";
    num8_engine_t e = {0};
    int exists = -1;
    uint64_t count = 0;

    remove(path);
    remove(missing_path);
    ASSERT_OK(num8_create(path, &e));
    ASSERT_STATUS(num8_create(path, &e), NUM8_STATUS_ALREADY_OPEN);
    ASSERT_STATUS(num8_open(path, NUM8_OPEN_READ_ONLY, &e), NUM8_STATUS_ALREADY_OPEN);
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 0);

    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ADDED);
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
    ASSERT_STATUS(num8_open(path, (num8_open_mode_t)99, &e), NUM8_STATUS_INVALID_MODE);
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
    size_t i;

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
    printf("num8_selftest OK\n");
    return 0;
}
