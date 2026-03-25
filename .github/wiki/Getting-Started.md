# Getting Started

## Prerequisites

- CMake 3.16+
- C99 compiler (MSVC, GCC, or Clang)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## First Program

```c
#include "num8.h"

int main(void)
{
    num8_engine_t engine;
    int exists = 0;

    if (num8_create("sample.num8", &engine) != NUM8_STATUS_OK)
        return 1;

    num8_add_u32(&engine, 12345678);
    num8_exists_u32(&engine, 12345678, &exists);

    num8_close(&engine);
    return exists ? 0 : 2;
}
```

## Run Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```