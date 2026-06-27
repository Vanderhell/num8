#ifndef NUM8_H
#define NUM8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM8_MAGIC "NUM8"
#define NUM8_VERSION_MAJOR 1u
#define NUM8_VERSION_MINOR 0u
#define NUM8_HEADER_SIZE 64u
#define NUM8_DOMAIN_SIZE 100000000ull
#define NUM8_PAYLOAD_SIZE 12500000u
#define NUM8_MAX_VALUE 99999999u
#define NUM8_ENGINE_STORAGE_SIZE 128u

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
    NUM8_STATUS_ALREADY_OPEN,
    NUM8_STATUS_NOT_OPEN,
    NUM8_STATUS_INVALID_MODE,
    NUM8_STATUS_LOCK_FAILED,
    NUM8_STATUS_RECOVERY_REQUIRED,
    NUM8_STATUS_GENERATION_EXHAUSTED,
    NUM8_STATUS_ATOMIC_REPLACE_FAILED,
    NUM8_STATUS_OUT_OF_MEMORY
} num8_status_t;

typedef enum num8_open_mode_e
{
    NUM8_OPEN_READ_ONLY = 0,
    NUM8_OPEN_READ_WRITE = 1
} num8_open_mode_t;

typedef struct num8_engine_s
{
    void* _alignment;
    unsigned char opaque[NUM8_ENGINE_STORAGE_SIZE];
} num8_engine_t;

/* Creates a new .num8 file, initializes empty payload, and opens engine in read-write mode. */
num8_status_t num8_create(const char* path, num8_engine_t* engine);
/* Opens existing .num8 file in read-only or read-write mode and validates header integrity. */
num8_status_t num8_open(const char* path, num8_open_mode_t mode, num8_engine_t* engine);
/* Closes engine handle; flushes pending changes for read-write engines. */
num8_status_t num8_close(num8_engine_t* engine);

/* Checks existence of value 0..99999999; sets out_exists to 0/1 on success. */
num8_status_t num8_exists_u32(const num8_engine_t* engine, uint32_t value, int* out_exists);
/* Adds value if absent, returns ADDED or ALREADY_EXISTS. */
num8_status_t num8_add_u32(num8_engine_t* engine, uint32_t value);
/* Removes value if present, returns REMOVED or NOT_FOUND. */
num8_status_t num8_remove_u32(num8_engine_t* engine, uint32_t value);

/* Strict 8-digit string variants ("00000000".."99999999"). */
num8_status_t num8_exists_str(const num8_engine_t* engine, const char* number8, int* out_exists);
num8_status_t num8_add_str(num8_engine_t* engine, const char* number8);
num8_status_t num8_remove_str(num8_engine_t* engine, const char* number8);

/* Returns incremental count from header model (not recomputed each call). */
num8_status_t num8_count(const num8_engine_t* engine, uint64_t* out_count);
/* Clears entire bitset and sets count to zero. */
num8_status_t num8_clear_all(num8_engine_t* engine);
/* Commits payload + header CRC to file; clears dirty flag on success. */
num8_status_t num8_flush(num8_engine_t* engine);
/* Validates the current in-memory snapshot. Clean engines may also be verified against disk. */
num8_status_t num8_validate_memory(const num8_engine_t* engine);
/* Validates the on-disk snapshot associated with the engine path. */
num8_status_t num8_validate_disk(const num8_engine_t* engine);
/* Validates current snapshot; dirty engines are checked in memory only. */
num8_status_t num8_validate(const num8_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif
