#define JE_IMPL
#include "jeecs.hpp"

typedef struct je_binary_document
{
    jeecs_file* m_file;
    size_t m_location;

} je_binary_document;

typedef struct je_binary_list
{
    jeecs_file* m_file;
    size_t m_location;
}je_binary_list;

typedef struct je_binary_string
{
    jeecs_file* m_file;
    size_t m_location;
}je_binary_string;

typedef struct je_binary_buffer
{
    jeecs_file* m_file;
    size_t m_location;
}je_binary_buffer;

enum class je_binary_value_type : uint8_t
{
    JE_BINARY_VALUE_BOOL,
    JE_BINARY_VALUE_INT8,
    JE_BINARY_VALUE_INT16,
    JE_BINARY_VALUE_INT32,
    JE_BINARY_VALUE_INT64,
    JE_BINARY_VALUE_UINT8,
    JE_BINARY_VALUE_UINT16,
    JE_BINARY_VALUE_UINT32,
    JE_BINARY_VALUE_UINT64,
    JE_BINARY_VALUE_FLOAT32,
    JE_BINARY_VALUE_FLOAT64,
    JE_BINARY_VALUE_STRING,
    JE_BINARY_VALUE_BUFFER,
    JE_BINARY_VALUE_LIST,
    JE_BINARY_VALUE_DOCUMENT,
};

void je_binary_document_open(jeecs_file* file, je_binary_document* out_doc)
{
    out_doc->m_file = file;
    out_doc->m_location = jeecs_file_tell(file);
}

/* ======================================= */

bool je_binary_append_document(je_writing_cache_file* file)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_DOCUMENT;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file);
}

bool je_binary_append_list(je_writing_cache_file* file)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_LIST;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file);
}

bool je_binary_append_string(je_writing_cache_file* file, const char* str)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_STRING;
    const size_t str_len = strlen(str);

    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && str_len == jeecs_write_cache_file(str, sizeof(char), str_len, file);
}

bool je_binary_append_buffer(je_writing_cache_file* file, const void* buf, size_t buflen)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_BUFFER;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && buflen == jeecs_write_cache_file(buf, 1, buflen, file);
}

bool je_binary_append_int8(je_writing_cache_file* file, int8_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_INT8;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_int16(je_writing_cache_file* file, int16_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_INT16;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_int32(je_writing_cache_file* file, int32_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_INT32;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_int64(je_writing_cache_file* file, int64_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_INT64;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_uint8(je_writing_cache_file* file, uint8_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_UINT8;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_uint16(je_writing_cache_file* file, uint16_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_UINT16;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_uint32(je_writing_cache_file* file, uint32_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_UINT32;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_uint64(je_writing_cache_file* file, uint64_t val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_UINT64;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_float32(je_writing_cache_file* file, float val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_FLOAT32;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}

bool je_binary_append_float64(je_writing_cache_file* file, double val)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_FLOAT64;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&val, sizeof(val), 1, file);
}
