#include "num8.h"

#include <stdio.h>

#define T_OK(x) do { num8_status_t s = (x); if (s != NUM8_STATUS_OK) { fprintf(stderr, "FAIL:%d status=%d\n", __LINE__, (int)s); return 1; } } while (0)
#define T_ST(x,e) do { num8_status_t s = (x); if (s != (e)) { fprintf(stderr, "FAIL:%d expected=%d got=%d\n", __LINE__, (int)(e), (int)s); return 1; } } while (0)
#define T_TRUE(x) do { if (!(x)) { fprintf(stderr, "FAIL:%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    const char* path = "num8_minitest_tmp.num8";
    num8_engine_t e;
    int ex = 0;
    uint64_t cnt = 0;

    T_OK(num8_create(path, &e));

    T_ST(num8_add_str(&e, "1234567"), NUM8_STATUS_INVALID_FORMAT);
    T_ST(num8_add_str(&e, "123456789"), NUM8_STATUS_INVALID_FORMAT);
    T_ST(num8_add_str(&e, "1234A678"), NUM8_STATUS_INVALID_FORMAT);

    T_ST(num8_add_str(&e, "00000000"), NUM8_STATUS_ADDED);
    T_ST(num8_add_str(&e, "00000001"), NUM8_STATUS_ADDED);
    T_ST(num8_add_str(&e, "99999999"), NUM8_STATUS_ADDED);

    T_OK(num8_count(&e, &cnt));
    T_TRUE(cnt == 3);

    T_OK(num8_exists_str(&e, "00000000", &ex));
    T_TRUE(ex == 1);
    T_OK(num8_exists_u32(&e, 2u, &ex));
    T_TRUE(ex == 0);

    T_ST(num8_remove_str(&e, "00000001"), NUM8_STATUS_REMOVED);
    T_ST(num8_remove_str(&e, "00000001"), NUM8_STATUS_NOT_FOUND);

    T_OK(num8_flush(&e));
    T_OK(num8_close(&e));

    T_OK(num8_open(path, NUM8_OPEN_READ_ONLY, &e));
    T_OK(num8_count(&e, &cnt));
    T_TRUE(cnt == 2);
    T_ST(num8_add_u32(&e, 3u), NUM8_STATUS_READ_ONLY);
    T_OK(num8_close(&e));

    remove(path);
    printf("num8_minitest OK\n");
    return 0;
}