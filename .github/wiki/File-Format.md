# File Format

## Layout

| Part | Size |
|------|-----:|
| Header | 64 bytes |
| Payload | 12,500,000 bytes |

Total size: `12,500,064 bytes`.

## Header Layout (v1)

| Offset | Size | Type | Name | Notes |
|-------:|-----:|------|------|-------|
| 0 | 4 | char[4] | magic | `"NUM8"` |
| 4 | 2 | uint16_t | version_major | `1` |
| 6 | 2 | uint16_t | version_minor | `0` |
| 8 | 4 | uint32_t | header_size | `64` |
| 12 | 8 | uint64_t | domain_size | `100000000` |
| 20 | 8 | uint64_t | set_count | active numbers |
| 28 | 4 | uint32_t | flags | `0` in v1 |
| 32 | 4 | uint32_t | payload_size | `12500000` |
| 36 | 4 | uint32_t | header_crc32 | CRC32 with this field zeroed |
| 40 | 4 | uint32_t | payload_crc32 | optional (`0` if unused) |
| 44 | 8 | uint64_t | generation | write generation |
| 52 | 12 | uint8_t[12] | reserved | all zero |

All numeric fields are little-endian.

## Payload Bit Mapping

For number `n` (`0..99999999`):

- `byte_index = n >> 3`
- `bit_index  = n & 7`
- `mask = (uint8_t)(1u << bit_index)`

Membership test:

- present: `(payload[byte_index] & mask) != 0`
- absent: `(payload[byte_index] & mask) == 0`

## Example Mapping for 12345678

- `byte_index = 12345678 >> 3 = 1543209`
- `bit_index  = 12345678 & 7 = 6`
- `mask       = 1 << 6 = 0x40`

So membership for `12345678` is stored in payload byte `1543209`, bit `6`.