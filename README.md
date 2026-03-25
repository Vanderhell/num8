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

Measured on Release build (MSVC, Windows, `num8_bench.exe`, 2026-03-18):

| API call | Average time | Throughput |
|---|---:|---:|
| `num8_exists_u32` | `8.03 ns/op` | `124,511,293 ops/s` |
| `num8_add_u32` | `4.47 ns/op` | `223,848,859 ops/s` |
| `num8_remove_u32` | `3.64 ns/op` | `274,974,566 ops/s` |
| `num8_add_str` | `21.35 ns/op` | `46,843,893 ops/s` |
| `num8_exists_str` | `22.02 ns/op` | `45,409,136 ops/s` |
| `num8_remove_str` | `22.78 ns/op` | `43,895,266 ops/s` |
| `num8_flush` | `14.170 ms/op` | `70.57 ops/s` |

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

## File Format (Technical Specification)

NUM8 file layout is fixed:

- header: `64 bytes`
- payload: `12,500,000 bytes`
- total: `12,500,064 bytes`

### Header layout (v1)

All numeric fields are little-endian.

| Offset | Size | Type | Name | Required value / meaning |
|-------:|-----:|------|------|--------------------------|
| 0 | 4 | char[4] | `magic` | `"NUM8"` |
| 4 | 2 | uint16_t | `version_major` | `1` |
| 6 | 2 | uint16_t | `version_minor` | `0` |
| 8 | 4 | uint32_t | `header_size` | `64` |
| 12 | 8 | uint64_t | `domain_size` | `100000000` |
| 20 | 8 | uint64_t | `set_count` | number of active values |
| 28 | 4 | uint32_t | `flags` | `0` in v1 |
| 32 | 4 | uint32_t | `payload_size` | `12500000` |
| 36 | 4 | uint32_t | `header_crc32` | CRC32 of full header with this field zeroed |
| 40 | 4 | uint32_t | `payload_crc32` | optional (`0` if unused) |
| 44 | 8 | uint64_t | `generation` | write generation counter |
| 52 | 12 | uint8_t[12] | `reserved` | all bytes `0` |

Validation rules:

- `set_count <= 100000000`
- `payload_crc32 == 0` means payload CRC is disabled

### Payload bit mapping

For numeric value `n` (`0..99999999`):

- `byte_index = n >> 3`
- `bit_index = n & 7`
- `mask = (uint8_t)(1u << bit_index)`

Presence:

- exists if `(payload[byte_index] & mask) != 0`
- missing if `(payload[byte_index] & mask) == 0`

Example for `12345678`:

- `byte_index = 1543209`
- `bit_index = 6`
- `mask = 0x40`

### Domain and validation

- Domain is fixed to exactly `00000000`..`99999999`
- Numeric API accepts only `value <= 99999999`
- String API accepts only exact 8-digit strings (`"00000000".."99999999"`)

## API

### Lifecycle

```c
num8_status_t num8_create(const char* path, num8_engine_t* engine);
num8_status_t num8_open(const char* path, num8_open_mode_t mode, num8_engine_t* engine);
num8_status_t num8_close(num8_engine_t* engine);
```

### u32 operations (fastest)

```c
num8_status_t num8_exists_u32(const num8_engine_t* engine, uint32_t value, int* out_exists);
num8_status_t num8_add_u32(num8_engine_t* engine, uint32_t value);
num8_status_t num8_remove_u32(num8_engine_t* engine, uint32_t value);
```

### String operations (`"00000000".."99999999"`)

```c
num8_status_t num8_exists_str(const num8_engine_t* engine, const char* number8, int* out_exists);
num8_status_t num8_add_str(num8_engine_t* engine, const char* number8);
num8_status_t num8_remove_str(num8_engine_t* engine, const char* number8);
```

### Utility

```c
num8_status_t num8_count(const num8_engine_t* engine, uint64_t* out_count);
num8_status_t num8_clear_all(num8_engine_t* engine);
num8_status_t num8_flush(num8_engine_t* engine);
num8_status_t num8_validate(const num8_engine_t* engine);
```

### Status codes

| Code | Meaning |
|------|---------|
| `NUM8_STATUS_OK` | Success |
| `NUM8_STATUS_ADDED` | Value added |
| `NUM8_STATUS_REMOVED` | Value removed |
| `NUM8_STATUS_ALREADY_EXISTS` | Add on existing value |
| `NUM8_STATUS_NOT_FOUND` | Remove on missing value |
| `NUM8_STATUS_INVALID_ARGUMENT` | Invalid/null argument |
| `NUM8_STATUS_INVALID_NUMBER` | Value out of `0..99999999` |
| `NUM8_STATUS_INVALID_FORMAT` | String is not exactly 8 digits |
| `NUM8_STATUS_IO_ERROR` | Generic I/O error |
| `NUM8_STATUS_OPEN_FAILED` | Open failed |
| `NUM8_STATUS_CREATE_FAILED` | Create failed |
| `NUM8_STATUS_FLUSH_FAILED` | Flush/sync failed |
| `NUM8_STATUS_CORRUPTED` | Corrupted state/file |
| `NUM8_STATUS_HEADER_CRC_MISMATCH` | Header CRC mismatch |
| `NUM8_STATUS_PAYLOAD_CRC_MISMATCH` | Payload CRC mismatch |
| `NUM8_STATUS_SIZE_MISMATCH` | Unexpected size |
| `NUM8_STATUS_UNSUPPORTED_VERSION` | Unsupported file version |
| `NUM8_STATUS_READ_ONLY` | Write on read-only engine |
| `NUM8_STATUS_NOT_OPEN` | Engine not open |
| `NUM8_STATUS_OUT_OF_MEMORY` | Allocation failed |

## DLL / Shared Library Usage

Shared build exports the same public API from `num8.h`:

- Windows: `num8.dll` + import library `num8.lib`
- Linux: `libnum8.so`
- macOS: `libnum8.dylib`

Build outputs (default CMake layout):

- library/runtime artifacts in `build/Release/`
- static library target: `num8_static`
- shared library target: `num8_shared`

Typical CMake consumer integration (installed layout):

- headers: `include/num8.h`
- libraries: `lib/` (or `bin/` for runtime)

## Consistency Model

- Engine uses one payload + one header commit point.
- `num8_flush` writes payload, updates header (`set_count`, `generation`, CRC), then syncs.
- Header CRC protects against partial/corrupted header writes.
- Recommended usage model: single-writer, multi-reader.
- Engine is not implicitly thread-safe; caller must synchronize.

## Tools

- `num8_builder` - create `.num8` from text file (one 8-digit number per line)
- `num8_selftest` - full self-test suite
- `num8_minitest` - quick smoke test
- `num8_bench` - performance benchmarks

## Use Cases

- Employee / customer ID access control
- Large-scale membership checks (100M domain)
- Allowlist / blocklist management
- High-frequency ID deduplication

## Limitations

- Domain is fixed: `00000000`-`99999999` only
- File size is always 12.5 MB (by design)
- Not thread-safe (caller must synchronize)
- Single-writer model
- No compression, no variable records (by design)
- Stores presence/absence only (one bit per number)

## Constants

```c
#define NUM8_MAGIC               "NUM8"
#define NUM8_VERSION_MAJOR       1u
#define NUM8_VERSION_MINOR       0u
#define NUM8_HEADER_SIZE         64u
#define NUM8_DOMAIN_SIZE         100000000ull
#define NUM8_PAYLOAD_SIZE        12500000u
#define NUM8_MAX_VALUE           99999999u
```

## License

MIT