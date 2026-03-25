#include "num8.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_OK(expr) do { num8_status_t _st = (expr); if (_st != NUM8_STATUS_OK) { \
    fprintf(stderr, "ASSERT_OK failed at %s:%d status=%d\n", __FILE__, __LINE__, (int)_st); return 1; } } while (0)

#define ASSERT_STATUS(expr, expected) do { num8_status_t _st = (expr); if (_st != (expected)) { \
    fprintf(stderr, "ASSERT_STATUS failed at %s:%d expected=%d got=%d\n", __FILE__, __LINE__, (int)(expected), (int)_st); return 1; } } while (0)

#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    fprintf(stderr, "ASSERT_TRUE failed at %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

static int test_core(void)
{
    const char* path = "num8_selftest_tmp.num8";
    num8_engine_t e;
    int exists = -1;
    uint64_t count = 0;

    ASSERT_OK(num8_create(path, &e));
    ASSERT_OK(num8_count(&e, &count));
    ASSERT_TRUE(count == 0);

    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ADDED);
    ASSERT_STATUS(num8_add_u32(&e, 99999999u), NUM8_STATUS_ADDED);
    ASSERT_STATUS(num8_add_u32(&e, 0u), NUM8_STATUS_ALREADY_EXISTS);
    ASSERT_STATUS(num8_add_u32(&e, 100000000u), NUM8_STATUS_INVALID_NUMBER);

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

int main(void)
{
    int rc = test_core();
    if (rc != 0)
    {
        return rc;
    }
    printf("num8_selftest OK\n");
    return 0;
}
