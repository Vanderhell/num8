# Performance

## Measured Benchmark (Release, MSVC, Windows)

| Operation | Time | Throughput |
|-----------|------|------------|
| `num8_exists_u32` | 8.03 ns/op | 124,511,293 ops/s |
| `num8_add_u32` | 4.47 ns/op | 223,848,859 ops/s |
| `num8_remove_u32` | 3.64 ns/op | 274,974,566 ops/s |
| `num8_exists_str` | 22.02 ns/op | 45,409,136 ops/s |
| `num8_add_str` | 21.35 ns/op | 46,843,893 ops/s |
| `num8_remove_str` | 22.78 ns/op | 43,895,266 ops/s |
| `num8_flush` | 14.170 ms/op | 70.57 ops/s |

## Notes: u32 vs str Variants

- `*_u32` variants are fastest because parsing is skipped.
- `*_str` variants include strict 8-digit validation and conversion overhead.
- Use string variants at boundaries, convert to `uint32_t` for hot paths.

## Flush Behavior

- `num8_flush` writes payload and header, updates generation, recomputes header CRC, and syncs to disk.
- `num8_flush` returns `NUM8_STATUS_OK` immediately when engine is not dirty.
- Flush is I/O-bound and significantly slower than in-memory bit operations.