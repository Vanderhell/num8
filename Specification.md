# NUM8 C Engine Specification v1.0

## 1. Purpose

NUM8 is a specialized binary file engine written in C, intended exclusively for tracking the presence of unique 8-digit numbers in a fixed domain:

- `00000000`
- through `99999999`

The engine does not store any additional data.
Each number has only one state:

- exists
- does not exist

This is not a general database engine.
This is a dedicated **membership engine**.

---

## 2. Design Goals

The engine must satisfy these goals:

- extremely fast lookup
- extremely fast add
- extremely fast remove
- simple binary format
- suitable for plain C
- no text parsing required during lookup
- no trees, no hash tables, no general index structures
- deterministic payload content
- fixed and predictable data size
- suitable for both RAM mode and memory-mapped mode

---

## 3. Scope

This specification defines:

- data domain
- binary file format
- engine operations
- validation rules
- error codes
- consistency requirements
- recommended C API contract
- recommended implementation rules

This specification does not define:

- network protocol
- SQL layer
- GUI
- compression
- distributed mode
- multi-writer synchronization

---

## 4. Data Domain

### 4.1 Allowed Values

The engine works only with 8-digit numbers.

Allowed examples:

- `00000000`
- `00001234`
- `12345678`
- `99999999`

Invalid examples:

- `1234567`
- `123456789`
- `-12345678`
- `12A45678`
- ` 12345678`
- `12345678 `

### 4.2 Internal Representation

Each number is mapped to an integer index:

- `00000000` -> `0`
- `00000001` -> `1`
- ...
- `99999999` -> `99999999`

Valid internal range:

- minimum: `0`
- maximum: `99,999,999`

Total number of possible values:

- `100,000,000`

---

## 5. Data Model

### 5.1 Basic Principle

Each possible number is assigned exactly one bit.

- bit `1` = number exists
- bit `0` = number does not exist

### 5.2 Bitset Size

Number of bits:

- `100,000,000`

Number of bytes:

- `100,000,000 / 8 = 12,500,000`

Exact payload size:

- `12,500,000 bytes`

This is a fixed size for all NUM8 v1 files.

---

## 6. Why Bitset Was Chosen

This design is correct because the problem is not "store a list of numbers", but:

- verify existence extremely fast
- add extremely fast
- remove extremely fast

For this goal, bitset is better than:

- text list
- array of strings
- binary search in an array
- B-tree
- file-backed hash table
- SQLite

### 6.1 Advantages

- `exists` is O(1)
- `add` is O(1)
- `remove` is O(1)
- natural uniqueness
- no fragmentation
- fixed payload
- simple C implementation
- very good fit for memory-mapped I/O

### 6.2 Trade-off

Even with a very small number of stored values, payload is always 12.5 MB.
This is intentional. The engine optimizes for speed, not minimal sparse storage.

---

## 7. File Format

A NUM8 file consists of:

1. header
2. payload

### 7.1 File Layout

| Part | Size |
|------|-----:|
| Header | 64 bytes |
| Payload | 12,500,000 bytes |

Total file size:

- `12,500,064 bytes`

---

## 8. Header Format v1

Header has a fixed size of `64 bytes`.

### 8.1 Endianness

All numeric fields must be encoded as:

- **little-endian**

### 8.2 Header Layout

| Offset | Size | Type | Name | Description |
|-------:|-----:|------|------|-------------|
| 0 | 4 | char[4] | magic | must be `NUM8` |
| 4 | 2 | uint16_t | version_major | `1` |
| 6 | 2 | uint16_t | version_minor | `0` |
| 8 | 4 | uint32_t | header_size | must be `64` |
| 12 | 8 | uint64_t | domain_size | must be `100000000` |
| 20 | 8 | uint64_t | set_count | number of active values |
| 28 | 4 | uint32_t | flags | reserved, currently `0` |
| 32 | 4 | uint32_t | payload_size | must be `12500000` |
| 36 | 4 | uint32_t | header_crc32 | CRC32 of header with this field zeroed |
| 40 | 4 | uint32_t | payload_crc32 | optional, `0` if unused |
| 44 | 8 | uint64_t | generation | write generation counter, optional |
| 52 | 12 | uint8_t[] | reserved | reserved, all bytes `0` |

### 8.3 Header Rules

For a valid file, all of the following must hold:

- `magic == "NUM8"`
- `version_major == 1`
- `version_minor == 0`
- `header_size == 64`
- `domain_size == 100000000`
- `payload_size == 12500000`
- `flags == 0`
- all bytes in `reserved[]` are `0`
- `set_count <= 100000000`

If `payload_crc32 == 0`, payload checksum is considered disabled.

---

## 9. Payload Format

Payload is a raw bitset.

### 9.1 Mapping Number to Bit

For number `n` in range `0..99999999`:

- `byte_index = n >> 3`
- `bit_index  = n & 7`

Mask:

- `mask = (uint8_t)(1u << bit_index)`

### 9.2 Bit Order

This bit order is used:

- bit 0 represents lower index
- bit 7 represents higher index within one byte

Example for the first payload byte:

- bit 0 -> number `00000000`
- bit 1 -> number `00000001`
- bit 2 -> number `00000002`
- ...
- bit 7 -> number `00000007`

### 9.3 Interpretation

- `(payload[byte_index] & mask) != 0` -> number exists
- `(payload[byte_index] & mask) == 0` -> number does not exist

---

## 10. Engine Operations

The engine must provide these logical operations.

### 10.1 Exists

Checks whether the number exists.

#### Input

- 8-digit number as string, or
- internal value `uint32_t`

#### Output

- exists / does not exist

#### Complexity

- time: O(1)
- memory: O(1)

---

### 10.2 Add

Adds a number.

#### Behavior

- if number does not exist, set bit to `1`
- if number already exists, do nothing

#### Effects

- on transition `0 -> 1`, `set_count` must increase
- on `1 -> 1`, `set_count` must not change

#### Complexity

- time: O(1)

---

### 10.3 Remove

Removes a number.

#### Behavior

- if number exists, set bit to `0`
- if number does not exist, do nothing

#### Effects

- on transition `1 -> 0`, `set_count` must decrease
- on `0 -> 0`, `set_count` must not change

#### Complexity

- time: O(1)

---

### 10.4 Count

Returns the number of currently stored numbers.

#### Note

Count must not be recomputed over the full payload each call.
It must be maintained incrementally in header field `set_count`.

---

### 10.5 ClearAll

Removes all entries.

#### Behavior

- set entire payload to `0`
- `set_count = 0`

#### Complexity

- time: O(payload_size)

---

### 10.6 BulkAdd

Adds many numbers in one operation.

#### Recommended behavior

- process input sequentially
- on invalid value, report error according to API mode
- update `set_count` only for real state changes

---

### 10.7 BulkRemove

Removes many numbers in one operation.

---

### 10.8 Flush

Writes changes to file.

Exact behavior depends on implementation mode:

- RAM buffer mode
- memory-mapped mode

But logical contract is the same:
after successful flush, data must be consistently written.

---

### 10.9 Validate

Checks integrity of the opened engine.

Must be able to verify:

- header magic
- version
- sizes
- header CRC
- optional payload CRC
- optional `set_count` consistency against real bitset

---

## 11. Input Validation

### 11.1 String Input

String is valid only if:

- length is exactly 8
- every character is `0..9`

Examples:

- `"00000000"` -> valid
- `"12345678"` -> valid
- `"99999999"` -> valid
- `"1234567"` -> invalid
- `"123456789"` -> invalid
- `"12x45678"` -> invalid

### 11.2 Integer Input

Integer is valid only if:

- `value <= 99999999`

Recommended type:

- `uint32_t`

---

## 12. Recommended Internal Modes

### 12.1 ReadOnly

Supported operations:

- open
- exists
- count
- validate
- close

Not supported:

- add
- remove
- clear
- flush write

Use cases:

- production lookup
- maximum read performance

---

### 12.2 ReadWrite

Supports all normal operations.

Use cases:

- dataset maintenance
- administration
- live add/remove

---

### 12.3 Buffered

Engine keeps payload in RAM and writes on flush.

Advantages:

- fast batch updates
- fewer I/O operations

Disadvantages:

- unflushed changes are lost on crash

---

## 13. Recommended C API Contract

This is not the only possible design, but it is reasonable and direct.

### 13.1 Types

```c
typedef enum num8_status_e
{
    NUM8_STATUS_OK = 0,
    NUM8_STATUS_ADDED,
    NUM8_STATUS_REMOVED,
    NUM8_STATUS_ALREADY_EXISTS,
    NUM8_STATUS_NOT_FOUND,

    NUM8_STATUS_INVALID_ARGUMENT,
    NUM8_STATUS_INVALID_NUMBER,
    NUM8_STATUS_INVALID_FORMAT,

    NUM8_STATUS_IO_ERROR,
    NUM8_STATUS_OPEN_FAILED,
    NUM8_STATUS_CREATE_FAILED,
    NUM8_STATUS_FLUSH_FAILED,

    NUM8_STATUS_CORRUPTED,
    NUM8_STATUS_HEADER_CRC_MISMATCH,
    NUM8_STATUS_PAYLOAD_CRC_MISMATCH,
    NUM8_STATUS_SIZE_MISMATCH,
    NUM8_STATUS_UNSUPPORTED_VERSION,

    NUM8_STATUS_READ_ONLY,
    NUM8_STATUS_NOT_OPEN,
    NUM8_STATUS_OUT_OF_MEMORY
} num8_status_t;
```

### 13.2 Open Modes

```c
typedef enum num8_open_mode_e
{
    NUM8_OPEN_READ_ONLY = 0,
    NUM8_OPEN_READ_WRITE = 1
} num8_open_mode_t;
```

### 13.3 Engine Handle

```c
typedef struct num8_engine_s
{
    void*     payload;
    uint64_t  set_count;
    uint64_t  generation;
    uint32_t  flags;

    int       is_open;
    int       is_dirty;
    int       is_read_only;

    /* implementation-specific fields */
    void*     file_handle;
    void*     map_handle;
} num8_engine_t;
```

### 13.4 Core API

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

## 14. API Functional Rules

### 14.1 num8_exists_*

- must not modify engine
- if input is invalid, return an error
- `out_exists` must be set only on success

### 14.2 num8_add_*

- for invalid number, return an error
- for read-only engine, return `NUM8_STATUS_READ_ONLY`
- if number already exists, return `NUM8_STATUS_ALREADY_EXISTS`
- if successfully added, return `NUM8_STATUS_ADDED`

### 14.3 num8_remove_*

- for invalid number, return an error
- if number does not exist, return `NUM8_STATUS_NOT_FOUND`
- if successfully removed, return `NUM8_STATUS_REMOVED`

### 14.4 num8_flush

- if engine is not dirty, it may return `NUM8_STATUS_OK`
- after successful flush, `is_dirty` must be `0`

## 15. Recommended Internal Helper Functions

```c
static int num8_is_valid_u32(uint32_t value);
static int num8_parse_strict_8digit(const char* s, uint32_t* out_value);

static uint32_t num8_byte_index(uint32_t value);
static uint8_t  num8_bit_mask(uint32_t value);

static int num8_test_bit(const uint8_t* payload, uint32_t value);
static void num8_set_bit(uint8_t* payload, uint32_t value);
static void num8_clear_bit(uint8_t* payload, uint32_t value);
```

## 16. File Consistency

Precision is required here.
During writes, process or system crash can happen. A minimum consistency model must be defined.

### 16.1 Minimum Acceptable Model

During change:

- modify payload
- update `set_count`
- increase `generation`
- recompute header CRC
- write header
- flush

This is simple, but not fully crash-safe.

### 16.2 Recommended Model

Recommended:

- 2-phase header commit, or
- shadow header, or
- atomic replace via temporary file

### 16.3 Practical Compromise

For a simple C engine, a reasonable compromise is:

- write payload directly
- keep header as the last commit point
- increase `generation` on every successful write
- on open, accept only valid header CRC

If higher durability is needed, use two headers.
If simplicity is preferred, one header is enough, but it is not fully transactional.

## 17. CRC Rules

### 17.1 Header CRC

- computed across full 64-byte header
- `header_crc32` field is treated as 0 during calculation

### 17.2 Payload CRC

- optional
- when enabled, computed across entire payload
- when disabled, field value is 0

### 17.3 Recommendation

- header CRC: definitely yes
- payload CRC: optional, better for offline validation than for every flush

## 18. Determinism

Engine is deterministic if:

- no timestamps are used
- no random fields are used
- reserved bytes are zero
- same set of numbers produces same payload
- `set_count` matches payload

Note:
If `generation` is used, file after update may not be byte-identical to an earlier version even if membership state is restored.
That is normal. If strict exported-content determinism is required, builder should support mode with `generation = 0`.

## 19. Performance Expectations

### 19.1 Exists

Practically minimal work:

- validation
- index computation
- one byte read
- one mask test

### 19.2 Add / Remove

Also minimal work:

- validation
- index
- bit test
- optional OR/AND on one byte
- count update

### 19.3 Main Bottlenecks

CPU should not be the main issue. Typical dominant costs:

- flush
- fsync
- synchronization between threads
- possible platform memory-mapping overhead

## 20. Concurrent Access

### 20.1 Recommended Model

- single-writer
- multi-reader

That means:

- multiple readers are OK
- one writer
- multiple concurrent writers are not a v1 goal

### 20.2 Thread Safety

If engine is shared across threads, synchronization must be handled by:

- internal engine lock, or
- external synchronization around API

For plain C, it is often better to keep thread synchronization on caller side, but this must be explicit in documentation.

Recommendation for v1:

- engine is not implicitly thread-safe
- caller provides synchronization

This is honest and simple.

## 21. Builder Tool

A separate builder utility is recommended.

### 21.1 Input

Text file where each line contains one 8-digit number.

Example:

```text
00001234
12345678
12345688
99999999
```

### 21.2 Builder Rules

- validate each line strictly
- either reject empty lines or define exact behavior
- detect duplicates
- set corresponding bits
- after completion, compute `set_count` correctly
- write header
- write payload

### 21.3 Recommendation

Prefer disallowing empty lines and whitespace.
A strict format is better.

## 22. Export Utility

An export utility is recommended:

- load `.num8`
- iterate payload
- output all present numbers as 8-digit strings

Note:

- export is O(domain_size)
- it must scan all 100,000,000 positions

This is not a bug. It is a property of the format.

## 23. Validation and Self-Test

Engine should include tests for at least these cases:

### 23.1 Header Tests

- valid magic
- invalid magic
- wrong version
- wrong header size
- wrong payload size
- bad header CRC

### 23.2 Value Tests

- `0`
- `1`
- `99999999`
- `100000000` -> invalid
- string with non-digit character -> invalid
- string shorter than 8 -> invalid
- string longer than 8 -> invalid

### 23.3 Functional Tests

- add new number
- add existing number
- exists after add
- remove existing number
- remove missing number
- count after add/remove series
- `clear_all`
- reopen and persistence check

### 23.4 Deterministic Tests

- same input -> same payload
- builder from same text -> same payload

## 24. Error States

Engine must distinguish at minimum these error classes:

- invalid argument
- invalid number
- invalid string format
- I/O error
- file cannot be opened
- file cannot be created
- corrupted header
- unsupported version
- size mismatch
- write attempted in read-only mode
- engine not open

Errors must never be masked as success.

## 25. Recommended Constants

```c
#define NUM8_MAGIC               "NUM8"
#define NUM8_VERSION_MAJOR       1u
#define NUM8_VERSION_MINOR       0u
#define NUM8_HEADER_SIZE         64u
#define NUM8_DOMAIN_SIZE         100000000ull
#define NUM8_PAYLOAD_SIZE        12500000u
#define NUM8_MAX_VALUE           99999999u
```

## 26. Recommended Extensions and Names

Recommended file extension:

- `.num8`

Examples:

- `allowlist.num8`
- `ids.num8`
- `customers.num8`

## 27. What v1 Intentionally Does Not Solve

To be explicit, version 1 intentionally does not solve:

- variable-length records
- additional per-number metadata
- full transactional journaling
- multi-writer mode
- compression
- sparse format
- network sharing
- replication
- built-in lock files

If this is required, a different product is required.

## 28. Final Technical Conclusion

NUM8 is a fixed, simple, and very fast C engine for one task:

- keep a set of unique 8-digit numbers
- answer very quickly whether a number exists
- add or remove a number quickly

Most important properties:

- 1 number = 1 bit
- fixed payload `12,500,000 bytes`
- O(1) exists
- O(1) add
- O(1) remove
- plain C
- deterministic payload
- ideal for memory-mapped or RAM-buffer implementations

This is the correct design for this specific problem.
Not universal, and that is exactly why it is effective.

## 29. Minimal Reference Logic

### Exists

```c
byte_index = value >> 3;
bit_index  = value & 7;
mask       = (uint8_t)(1u << bit_index);

exists = (payload[byte_index] & mask) != 0;
```

### Add

```c
if ((payload[byte_index] & mask) == 0)
{
    payload[byte_index] |= mask;
    set_count++;
}
```

### Remove

```c
if ((payload[byte_index] & mask) != 0)
{
    payload[byte_index] &= (uint8_t)(~mask);
    set_count--;
}
```

## 30. Recommendation for C Implementation

For v1, the most reasonable approach is:

- 1 `.h`
- 1 `.c`
- separate builder tool
- separate self-test tool

Follow these principles:

- no unnecessary abstraction
- no callbacks without reason
- no polymorphic backends in v1
- no text-storage fallback
- no compression experiments
- maximum directness

That is all.