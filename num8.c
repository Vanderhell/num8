#include "num8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

typedef struct num8_internal_s
{
    uint32_t payload_crc32;
    int payload_crc_enabled;
} num8_internal_t;

typedef struct num8_header_fields_s
{
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t header_size;
    uint64_t domain_size;
    uint64_t set_count;
    uint32_t flags;
    uint32_t payload_size;
    uint32_t header_crc32;
    uint32_t payload_crc32;
    uint64_t generation;
    uint8_t reserved[12];
} num8_header_fields_t;

static int num8_is_valid_u32(uint32_t value);
static int num8_parse_strict_8digit(const char* s, uint32_t* out_value);
static uint32_t num8_byte_index(uint32_t value);
static uint8_t num8_bit_mask(uint32_t value);
static int num8_test_bit(const uint8_t* payload, uint32_t value);
static void num8_set_bit(uint8_t* payload, uint32_t value);
static void num8_clear_bit(uint8_t* payload, uint32_t value);
static void num8_engine_reset(num8_engine_t* engine);
static int num8_engine_ready(const num8_engine_t* engine);
static uint32_t num8_crc32(const uint8_t* data, size_t len);
static void num8_store_le16(uint8_t* p, uint16_t v);
static void num8_store_le32(uint8_t* p, uint32_t v);
static void num8_store_le64(uint8_t* p, uint64_t v);
static uint16_t num8_load_le16(const uint8_t* p);
static uint32_t num8_load_le32(const uint8_t* p);
static uint64_t num8_load_le64(const uint8_t* p);
static void num8_fill_header_bytes(
    uint8_t* out_header,
    uint64_t set_count,
    uint64_t generation,
    uint32_t flags,
    uint32_t payload_crc32);
static num8_status_t num8_parse_and_validate_header(const uint8_t* header, num8_header_fields_t* out_fields);
static uint64_t num8_payload_popcount(const uint8_t* payload);
static num8_status_t num8_sync_file(FILE* f);
static num8_status_t num8_validate_file_snapshot(const num8_engine_t* engine);

static int num8_is_valid_u32(uint32_t value)
{
    return value <= NUM8_MAX_VALUE;
}

static int num8_parse_strict_8digit(const char* s, uint32_t* out_value)
{
    uint32_t v = 0;
    int i;

    if (s == NULL || out_value == NULL)
    {
        return 0;
    }

    for (i = 0; i < 8; ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if (c < '0' || c > '9')
        {
            return 0;
        }
        v = (uint32_t)(v * 10u + (uint32_t)(c - (unsigned char)'0'));
    }

    if (s[8] != '\0')
    {
        return 0;
    }

    *out_value = v;
    return 1;
}

static uint32_t num8_byte_index(uint32_t value)
{
    return value >> 3;
}

static uint8_t num8_bit_mask(uint32_t value)
{
    return (uint8_t)(1u << (value & 7u));
}

static int num8_test_bit(const uint8_t* payload, uint32_t value)
{
    return (payload[num8_byte_index(value)] & num8_bit_mask(value)) != 0u;
}

static void num8_set_bit(uint8_t* payload, uint32_t value)
{
    payload[num8_byte_index(value)] = (uint8_t)(payload[num8_byte_index(value)] | num8_bit_mask(value));
}

static void num8_clear_bit(uint8_t* payload, uint32_t value)
{
    payload[num8_byte_index(value)] = (uint8_t)(payload[num8_byte_index(value)] & (uint8_t)(~num8_bit_mask(value)));
}

static void num8_engine_reset(num8_engine_t* engine)
{
    if (engine != NULL)
    {
        memset(engine, 0, sizeof(*engine));
    }
}

static int num8_engine_ready(const num8_engine_t* engine)
{
    return engine != NULL && engine->is_open != 0 && engine->payload != NULL;
}

static uint32_t num8_crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int b;

    for (i = 0; i < len; ++i)
    {
        crc ^= (uint32_t)data[i];
        for (b = 0; b < 8; ++b)
        {
            uint32_t mask = (uint32_t)(-(int)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static void num8_store_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void num8_store_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void num8_store_le64(uint8_t* p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; ++i)
    {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

static uint16_t num8_load_le16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (uint16_t)(p[1] << 8));
}

static uint32_t num8_load_le32(const uint8_t* p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint64_t num8_load_le64(const uint8_t* p)
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; ++i)
    {
        v |= ((uint64_t)p[i] << (8 * i));
    }
    return v;
}

static void num8_fill_header_bytes(
    uint8_t* out_header,
    uint64_t set_count,
    uint64_t generation,
    uint32_t flags,
    uint32_t payload_crc32)
{
    uint32_t crc;
    memset(out_header, 0, NUM8_HEADER_SIZE);

    out_header[0] = 'N';
    out_header[1] = 'U';
    out_header[2] = 'M';
    out_header[3] = '8';
    num8_store_le16(&out_header[4], (uint16_t)NUM8_VERSION_MAJOR);
    num8_store_le16(&out_header[6], (uint16_t)NUM8_VERSION_MINOR);
    num8_store_le32(&out_header[8], (uint32_t)NUM8_HEADER_SIZE);
    num8_store_le64(&out_header[12], (uint64_t)NUM8_DOMAIN_SIZE);
    num8_store_le64(&out_header[20], set_count);
    num8_store_le32(&out_header[28], flags);
    num8_store_le32(&out_header[32], (uint32_t)NUM8_PAYLOAD_SIZE);
    num8_store_le32(&out_header[36], 0u);
    num8_store_le32(&out_header[40], payload_crc32);
    num8_store_le64(&out_header[44], generation);

    crc = num8_crc32(out_header, NUM8_HEADER_SIZE);
    num8_store_le32(&out_header[36], crc);
}

static num8_status_t num8_parse_and_validate_header(const uint8_t* header, num8_header_fields_t* out_fields)
{
    uint8_t tmp[NUM8_HEADER_SIZE];
    uint32_t stored_crc;
    uint32_t computed_crc;
    int i;

    if (memcmp(header, NUM8_MAGIC, 4) != 0)
    {
        return NUM8_STATUS_CORRUPTED;
    }

    out_fields->version_major = num8_load_le16(&header[4]);
    out_fields->version_minor = num8_load_le16(&header[6]);
    out_fields->header_size = num8_load_le32(&header[8]);
    out_fields->domain_size = num8_load_le64(&header[12]);
    out_fields->set_count = num8_load_le64(&header[20]);
    out_fields->flags = num8_load_le32(&header[28]);
    out_fields->payload_size = num8_load_le32(&header[32]);
    out_fields->header_crc32 = num8_load_le32(&header[36]);
    out_fields->payload_crc32 = num8_load_le32(&header[40]);
    out_fields->generation = num8_load_le64(&header[44]);
    memcpy(out_fields->reserved, &header[52], sizeof(out_fields->reserved));

    if (out_fields->version_major != NUM8_VERSION_MAJOR || out_fields->version_minor != NUM8_VERSION_MINOR)
    {
        return NUM8_STATUS_UNSUPPORTED_VERSION;
    }
    if (out_fields->header_size != NUM8_HEADER_SIZE || out_fields->payload_size != NUM8_PAYLOAD_SIZE)
    {
        return NUM8_STATUS_SIZE_MISMATCH;
    }
    if (out_fields->domain_size != NUM8_DOMAIN_SIZE || out_fields->set_count > NUM8_DOMAIN_SIZE)
    {
        return NUM8_STATUS_CORRUPTED;
    }
    if (out_fields->flags != 0u)
    {
        return NUM8_STATUS_CORRUPTED;
    }
    for (i = 0; i < (int)sizeof(out_fields->reserved); ++i)
    {
        if (out_fields->reserved[i] != 0u)
        {
            return NUM8_STATUS_CORRUPTED;
        }
    }

    memcpy(tmp, header, NUM8_HEADER_SIZE);
    num8_store_le32(&tmp[36], 0u);
    stored_crc = out_fields->header_crc32;
    computed_crc = num8_crc32(tmp, NUM8_HEADER_SIZE);
    if (stored_crc != computed_crc)
    {
        return NUM8_STATUS_HEADER_CRC_MISMATCH;
    }

    return NUM8_STATUS_OK;
}

static uint64_t num8_payload_popcount(const uint8_t* payload)
{
    static const uint8_t pop8[256] = {
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
    };
    uint64_t total = 0;
    uint32_t i;
    for (i = 0; i < NUM8_PAYLOAD_SIZE; ++i)
    {
        total += (uint64_t)pop8[payload[i]];
    }
    return total;
}

static num8_status_t num8_sync_file(FILE* f)
{
    if (fflush(f) != 0)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }
#if defined(_WIN32)
    if (_commit(_fileno(f)) != 0)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }
#else
    if (fsync(fileno(f)) != 0)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }
#endif
    return NUM8_STATUS_OK;
}

static num8_status_t num8_validate_file_snapshot(const num8_engine_t* engine)
{
    FILE* f = (FILE*)engine->file_handle;
    long cur;
    long file_size;
    uint8_t header[NUM8_HEADER_SIZE];
    num8_header_fields_t fields;
    num8_status_t st;
    uint32_t payload_crc;

    if (f == NULL)
    {
        return NUM8_STATUS_OK;
    }

    cur = ftell(f);
    if (cur < 0)
    {
        return NUM8_STATUS_IO_ERROR;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        return NUM8_STATUS_IO_ERROR;
    }
    file_size = ftell(f);
    if (file_size < 0)
    {
        return NUM8_STATUS_IO_ERROR;
    }
    if ((uint64_t)file_size != (uint64_t)NUM8_HEADER_SIZE + (uint64_t)NUM8_PAYLOAD_SIZE)
    {
        return NUM8_STATUS_SIZE_MISMATCH;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        return NUM8_STATUS_IO_ERROR;
    }

    if (fread(header, 1, NUM8_HEADER_SIZE, f) != NUM8_HEADER_SIZE)
    {
        return NUM8_STATUS_IO_ERROR;
    }

    st = num8_parse_and_validate_header(header, &fields);
    if (st != NUM8_STATUS_OK)
    {
        return st;
    }

    if (fields.payload_crc32 != 0u)
    {
        uint8_t* payload = (uint8_t*)engine->payload;
        payload_crc = num8_crc32(payload, NUM8_PAYLOAD_SIZE);
        if (payload_crc != fields.payload_crc32)
        {
            return NUM8_STATUS_PAYLOAD_CRC_MISMATCH;
        }
    }

    if (fseek(f, cur, SEEK_SET) != 0)
    {
        return NUM8_STATUS_IO_ERROR;
    }

    return NUM8_STATUS_OK;
}

num8_status_t num8_create(const char* path, num8_engine_t* engine)
{
    FILE* f;
    uint8_t header[NUM8_HEADER_SIZE];
    uint8_t* payload;
    num8_internal_t* internal;

    if (path == NULL || engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }

    num8_engine_reset(engine);

    f = fopen(path, "wb+");
    if (f == NULL)
    {
        return NUM8_STATUS_CREATE_FAILED;
    }

    payload = (uint8_t*)calloc(NUM8_PAYLOAD_SIZE, 1);
    if (payload == NULL)
    {
        fclose(f);
        return NUM8_STATUS_OUT_OF_MEMORY;
    }

    internal = (num8_internal_t*)calloc(1, sizeof(*internal));
    if (internal == NULL)
    {
        free(payload);
        fclose(f);
        return NUM8_STATUS_OUT_OF_MEMORY;
    }

    num8_fill_header_bytes(header, 0u, 0u, 0u, 0u);

    if (fwrite(header, 1, NUM8_HEADER_SIZE, f) != NUM8_HEADER_SIZE
        || fwrite(payload, 1, NUM8_PAYLOAD_SIZE, f) != NUM8_PAYLOAD_SIZE)
    {
        free(internal);
        free(payload);
        fclose(f);
        return NUM8_STATUS_IO_ERROR;
    }

    if (num8_sync_file(f) != NUM8_STATUS_OK)
    {
        free(internal);
        free(payload);
        fclose(f);
        return NUM8_STATUS_FLUSH_FAILED;
    }

    engine->payload = payload;
    engine->set_count = 0u;
    engine->generation = 0u;
    engine->flags = 0u;
    engine->is_open = 1;
    engine->is_dirty = 0;
    engine->is_read_only = 0;
    engine->file_handle = f;
    engine->map_handle = internal;

    return NUM8_STATUS_OK;
}

num8_status_t num8_open(const char* path, num8_open_mode_t mode, num8_engine_t* engine)
{
    FILE* f;
    uint8_t header[NUM8_HEADER_SIZE];
    num8_header_fields_t fields;
    uint8_t* payload;
    num8_internal_t* internal;
    num8_status_t st;

    if (path == NULL || engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }

    num8_engine_reset(engine);

    f = fopen(path, mode == NUM8_OPEN_READ_ONLY ? "rb" : "rb+");
    if (f == NULL)
    {
        return NUM8_STATUS_OPEN_FAILED;
    }

    if (fread(header, 1, NUM8_HEADER_SIZE, f) != NUM8_HEADER_SIZE)
    {
        fclose(f);
        return NUM8_STATUS_IO_ERROR;
    }

    st = num8_parse_and_validate_header(header, &fields);
    if (st != NUM8_STATUS_OK)
    {
        fclose(f);
        return st;
    }

    payload = (uint8_t*)malloc(NUM8_PAYLOAD_SIZE);
    if (payload == NULL)
    {
        fclose(f);
        return NUM8_STATUS_OUT_OF_MEMORY;
    }

    if (fread(payload, 1, NUM8_PAYLOAD_SIZE, f) != NUM8_PAYLOAD_SIZE)
    {
        free(payload);
        fclose(f);
        return NUM8_STATUS_IO_ERROR;
    }

    internal = (num8_internal_t*)calloc(1, sizeof(*internal));
    if (internal == NULL)
    {
        free(payload);
        fclose(f);
        return NUM8_STATUS_OUT_OF_MEMORY;
    }
    internal->payload_crc32 = fields.payload_crc32;
    internal->payload_crc_enabled = fields.payload_crc32 != 0u ? 1 : 0;

    if (internal->payload_crc_enabled != 0)
    {
        uint32_t payload_crc = num8_crc32(payload, NUM8_PAYLOAD_SIZE);
        if (payload_crc != internal->payload_crc32)
        {
            free(internal);
            free(payload);
            fclose(f);
            return NUM8_STATUS_PAYLOAD_CRC_MISMATCH;
        }
    }

    engine->payload = payload;
    engine->set_count = fields.set_count;
    engine->generation = fields.generation;
    engine->flags = fields.flags;
    engine->is_open = 1;
    engine->is_dirty = 0;
    engine->is_read_only = mode == NUM8_OPEN_READ_ONLY ? 1 : 0;
    engine->file_handle = f;
    engine->map_handle = internal;

    return NUM8_STATUS_OK;
}

num8_status_t num8_close(num8_engine_t* engine)
{
    FILE* f;
    num8_internal_t* internal;
    num8_status_t st;

    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (engine->is_open == 0)
    {
        return NUM8_STATUS_NOT_OPEN;
    }

    if (engine->is_read_only == 0 && engine->is_dirty != 0)
    {
        st = num8_flush(engine);
        if (st != NUM8_STATUS_OK)
        {
            return st;
        }
    }

    f = (FILE*)engine->file_handle;
    if (f != NULL)
    {
        fclose(f);
    }

    if (engine->payload != NULL)
    {
        free(engine->payload);
    }

    internal = (num8_internal_t*)engine->map_handle;
    if (internal != NULL)
    {
        free(internal);
    }

    num8_engine_reset(engine);
    return NUM8_STATUS_OK;
}

num8_status_t num8_exists_u32(const num8_engine_t* engine, uint32_t value, int* out_exists)
{
    const uint8_t* payload;

    if (engine == NULL || out_exists == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    if (!num8_is_valid_u32(value))
    {
        return NUM8_STATUS_INVALID_NUMBER;
    }

    payload = (const uint8_t*)engine->payload;
    *out_exists = num8_test_bit(payload, value);
    return NUM8_STATUS_OK;
}

num8_status_t num8_add_u32(num8_engine_t* engine, uint32_t value)
{
    uint8_t* payload;

    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    if (engine->is_read_only != 0)
    {
        return NUM8_STATUS_READ_ONLY;
    }
    if (!num8_is_valid_u32(value))
    {
        return NUM8_STATUS_INVALID_NUMBER;
    }

    payload = (uint8_t*)engine->payload;
    if (num8_test_bit(payload, value))
    {
        return NUM8_STATUS_ALREADY_EXISTS;
    }

    num8_set_bit(payload, value);
    engine->set_count++;
    engine->is_dirty = 1;
    return NUM8_STATUS_ADDED;
}

num8_status_t num8_remove_u32(num8_engine_t* engine, uint32_t value)
{
    uint8_t* payload;

    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    if (engine->is_read_only != 0)
    {
        return NUM8_STATUS_READ_ONLY;
    }
    if (!num8_is_valid_u32(value))
    {
        return NUM8_STATUS_INVALID_NUMBER;
    }

    payload = (uint8_t*)engine->payload;
    if (!num8_test_bit(payload, value))
    {
        return NUM8_STATUS_NOT_FOUND;
    }

    num8_clear_bit(payload, value);
    engine->set_count--;
    engine->is_dirty = 1;
    return NUM8_STATUS_REMOVED;
}

num8_status_t num8_exists_str(const num8_engine_t* engine, const char* number8, int* out_exists)
{
    uint32_t value;
    if (number8 == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_parse_strict_8digit(number8, &value))
    {
        return NUM8_STATUS_INVALID_FORMAT;
    }
    return num8_exists_u32(engine, value, out_exists);
}

num8_status_t num8_add_str(num8_engine_t* engine, const char* number8)
{
    uint32_t value;
    if (number8 == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_parse_strict_8digit(number8, &value))
    {
        return NUM8_STATUS_INVALID_FORMAT;
    }
    return num8_add_u32(engine, value);
}

num8_status_t num8_remove_str(num8_engine_t* engine, const char* number8)
{
    uint32_t value;
    if (number8 == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_parse_strict_8digit(number8, &value))
    {
        return NUM8_STATUS_INVALID_FORMAT;
    }
    return num8_remove_u32(engine, value);
}

num8_status_t num8_count(const num8_engine_t* engine, uint64_t* out_count)
{
    if (engine == NULL || out_count == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    *out_count = engine->set_count;
    return NUM8_STATUS_OK;
}

num8_status_t num8_clear_all(num8_engine_t* engine)
{
    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    if (engine->is_read_only != 0)
    {
        return NUM8_STATUS_READ_ONLY;
    }

    memset(engine->payload, 0, NUM8_PAYLOAD_SIZE);
    engine->set_count = 0u;
    engine->is_dirty = 1;
    return NUM8_STATUS_OK;
}

num8_status_t num8_flush(num8_engine_t* engine)
{
    FILE* f;
    uint8_t header[NUM8_HEADER_SIZE];
    num8_internal_t* internal;

    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }
    if (engine->is_read_only != 0)
    {
        return NUM8_STATUS_READ_ONLY;
    }
    if (engine->is_dirty == 0)
    {
        return NUM8_STATUS_OK;
    }

    f = (FILE*)engine->file_handle;
    if (f == NULL)
    {
        return NUM8_STATUS_IO_ERROR;
    }

    engine->generation++;

    if (fseek(f, NUM8_HEADER_SIZE, SEEK_SET) != 0)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }
    if (fwrite(engine->payload, 1, NUM8_PAYLOAD_SIZE, f) != NUM8_PAYLOAD_SIZE)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }

    internal = (num8_internal_t*)engine->map_handle;
    if (internal != NULL && internal->payload_crc_enabled != 0)
    {
        internal->payload_crc32 = num8_crc32((const uint8_t*)engine->payload, NUM8_PAYLOAD_SIZE);
    }

    num8_fill_header_bytes(
        header,
        engine->set_count,
        engine->generation,
        engine->flags,
        internal != NULL ? internal->payload_crc32 : 0u);

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }
    if (fwrite(header, 1, NUM8_HEADER_SIZE, f) != NUM8_HEADER_SIZE)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }

    if (num8_sync_file(f) != NUM8_STATUS_OK)
    {
        return NUM8_STATUS_FLUSH_FAILED;
    }

    engine->is_dirty = 0;
    return NUM8_STATUS_OK;
}

num8_status_t num8_validate(const num8_engine_t* engine)
{
    const uint8_t* payload;
    uint64_t real_count;
    num8_status_t st;
    num8_internal_t* internal;

    if (engine == NULL)
    {
        return NUM8_STATUS_INVALID_ARGUMENT;
    }
    if (!num8_engine_ready(engine))
    {
        return NUM8_STATUS_NOT_OPEN;
    }

    if (engine->set_count > NUM8_DOMAIN_SIZE || engine->flags != 0u)
    {
        return NUM8_STATUS_CORRUPTED;
    }

    payload = (const uint8_t*)engine->payload;
    real_count = num8_payload_popcount(payload);
    if (real_count != engine->set_count)
    {
        return NUM8_STATUS_CORRUPTED;
    }

    internal = (num8_internal_t*)engine->map_handle;
    if (internal != NULL && internal->payload_crc_enabled != 0)
    {
        uint32_t payload_crc = num8_crc32(payload, NUM8_PAYLOAD_SIZE);
        if (payload_crc != internal->payload_crc32)
        {
            return NUM8_STATUS_PAYLOAD_CRC_MISMATCH;
        }
    }

    if (engine->is_dirty == 0)
    {
        st = num8_validate_file_snapshot(engine);
        if (st != NUM8_STATUS_OK)
        {
            return st;
        }
    }

    return NUM8_STATUS_OK;
}
