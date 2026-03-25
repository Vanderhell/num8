# num8

> O(1) membership engine for 8-digit numbers. C99, 12.5 MB, no dependencies.

![Language](https://img.shields.io/badge/Language-C99-blue)
![License](https://img.shields.io/badge/License-MIT-green)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)

## What is num8?

num8 tracks whether 8-digit numbers (`00000000`-`99999999`) exist in a set. It stores only membership state, nothing else, and each number occupies exactly one bit in a fixed 12.5 MB payload.

## Why a bitset?

| Approach | exists | add | remove | File size |
|----------|--------|-----|--------|-----------|
| Text list | O(n) | O(n) | O(n) | variable |
| SQLite | O(log n) | O(log n) | O(log n) | variable |
| Hash table | O(1) avg | O(1) avg | O(1) avg | variable |
| **num8** | **O(1)** | **O(1)** | **O(1)** | **12.5 MB fixed** |

## Performance

Measured on Release build (MSVC, Windows):

| Operation | Time | Throughput |
|-----------|------|------------|
| exists (u32) | 8 ns | 124M ops/s |
| add (u32) | 4 ns | 223M ops/s |
| remove (u32) | 3 ns | 274M ops/s |
| exists (str) | 22 ns | 45M ops/s |
| add (str) | 21 ns | 46M ops/s |
| remove (str) | 22 ns | 43M ops/s |
| flush | 14 ms | 70 ops/s |

## Quick Start

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Create and use

```c
#include "num8.h"

num8_engine_t engine;
num8_create("employees.num8", &engine);

num8_add_u32(&engine, 12345678);

int exists = 0;
num8_exists_u32(&engine, 12345678, &exists);
/* exists == 1 */

num8_remove_u32(&engine, 12345678);
num8_flush(&engine);
num8_close(&engine);
```

## Use Cases

- Employee / customer ID access control
- Large-scale membership checks (100M domain)
- Allowlist / blocklist management
- High-frequency ID deduplication

## File Format

Fixed structure, always 12,500,064 bytes:

`[64 bytes header][12,500,000 bytes payload]`

Header includes: magic `"NUM8"`, version, CRC32, `set_count`, `generation`. Little-endian.

## API

### Lifecycle

`num8_create(path, engine)` - create new `.num8` file
`num8_open(path, mode, engine)` - open existing
`num8_close(engine)` - close and flush

### u32 operations (fastest)

`num8_exists_u32(engine, value, out_exists)`
`num8_add_u32(engine, value)`
`num8_remove_u32(engine, value)`

### String operations (`"00000000".."99999999"`)

`num8_exists_str(engine, number8, out_exists)`
`num8_add_str(engine, number8)`
`num8_remove_str(engine, number8)`

### Utility

`num8_count(engine, out_count)`
`num8_clear_all(engine)`
`num8_flush(engine)`
`num8_validate(engine)`

## Tools

`num8_builder` - create `.num8` from text file (one 8-digit number per line)
`num8_selftest` - full self-test suite
`num8_minitest` - quick smoke test
`num8_bench` - performance benchmarks

## Limitations

- Domain is fixed: `00000000`-`99999999` only
- File size is always 12.5 MB (by design)
- Not thread-safe (caller must synchronize)
- Single-writer model
- No compression, no variable records (by design)

## License

MIT