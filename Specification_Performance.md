# NUM8 API Performance Specification (Measured)

Measurement: `Release` build, `num8_bench.exe`, date `2026-03-18`.

## API Times and Throughput

| API call | Average time | Throughput |
|---|---:|---:|
| `num8_exists_u32` | `8.03 ns/op` | `124,511,293 ops/s` |
| `num8_add_u32` | `4.47 ns/op` | `223,848,859 ops/s` |
| `num8_remove_u32` | `3.64 ns/op` | `274,974,566 ops/s` |
| `num8_add_str` | `21.35 ns/op` | `46,843,893 ops/s` |
| `num8_exists_str` | `22.02 ns/op` | `45,409,136 ops/s` |
| `num8_remove_str` | `22.78 ns/op` | `43,895,266 ops/s` |
| `num8_flush` | `14.170 ms/op` | `70.57 ops/s` |