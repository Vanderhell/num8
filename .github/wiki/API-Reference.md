# API Reference

## Public Functions

```c
num8_status_t num8_create(const char* path, num8_engine_t* engine);
num8_status_t num8_open(const char* path, num8_open_mode_t mode, num8_engine_t* engine);
num8_status_t num8_close(num8_engine_t* engine);

num8_status_t num8_exists_u32(const num8_engine_t* engine, uint32_t value, int* out_exists);
num8_status_t num8_add_u32(num8_engine_t* engine, uint32_t value);
num8_status_t num8_remove_u32(num8_engine_t* engine, uint32_t value);

num8_status_t num8_exists_str(const num8_engine_t* engine, const char* number8, int* out_exists);
num8_status_t num8_add_str(num8_engine_t* engine, const char* number8);
num8_status_t num8_remove_str(num8_engine_t* engine, const char* number8);

num8_status_t num8_count(const num8_engine_t* engine, uint64_t* out_count);
num8_status_t num8_clear_all(num8_engine_t* engine);
num8_status_t num8_flush(num8_engine_t* engine);
num8_status_t num8_validate(const num8_engine_t* engine);
```

## Parameters and Return Values

- `path`: filesystem path to `.num8` file.
- `engine`: engine handle (input/output depending on function).
- `mode`: `NUM8_OPEN_READ_ONLY` or `NUM8_OPEN_READ_WRITE`.
- `value`: numeric ID in range `0..99999999`.
- `number8`: strict 8-digit string (`"00000000".."99999999"`).
- `out_exists`: output 0/1 for existence check.
- `out_count`: output number of active entries.
- return value: `num8_status_t` status code.

## Status Codes

| Code | Meaning |
|------|---------|
| `NUM8_STATUS_OK` | Success |
| `NUM8_STATUS_ADDED` | Value added |
| `NUM8_STATUS_REMOVED` | Value removed |
| `NUM8_STATUS_ALREADY_EXISTS` | Add requested for existing value |
| `NUM8_STATUS_NOT_FOUND` | Remove requested for missing value |
| `NUM8_STATUS_INVALID_ARGUMENT` | Null or invalid pointer argument |
| `NUM8_STATUS_INVALID_NUMBER` | Numeric value out of range |
| `NUM8_STATUS_INVALID_FORMAT` | String is not exactly 8 digits |
| `NUM8_STATUS_IO_ERROR` | Generic I/O failure |
| `NUM8_STATUS_OPEN_FAILED` | Failed to open file |
| `NUM8_STATUS_CREATE_FAILED` | Failed to create file |
| `NUM8_STATUS_FLUSH_FAILED` | Failed to flush/sync |
| `NUM8_STATUS_CORRUPTED` | Corrupted engine/file state |
| `NUM8_STATUS_HEADER_CRC_MISMATCH` | Header CRC check failed |
| `NUM8_STATUS_PAYLOAD_CRC_MISMATCH` | Payload CRC check failed |
| `NUM8_STATUS_SIZE_MISMATCH` | Unexpected file/header size |
| `NUM8_STATUS_UNSUPPORTED_VERSION` | Unsupported file version |
| `NUM8_STATUS_READ_ONLY` | Write operation on read-only engine |
| `NUM8_STATUS_NOT_OPEN` | Operation requires open engine |
| `NUM8_STATUS_OUT_OF_MEMORY` | Memory allocation failed |