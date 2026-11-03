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
    JE_BINARY_VALUE_END,
};

bool je_binary_document_open(jeecs_file* file, je_binary_document* out_doc)
{
    je_binary_value_type type;
    if (1 != jeecs_file_read(&type, sizeof(type), 1, file)
        || type != je_binary_value_type::JE_BINARY_VALUE_DOCUMENT)
        return false;

    out_doc->m_file = file;
    out_doc->m_location = jeecs_file_tell(file);
    return true;
}

bool je_binary_list_open(jeecs_file* file, je_binary_list* out_doc)
{
    je_binary_value_type type;
    if (1 != jeecs_file_read(&type, sizeof(type), 1, file)
        || type != je_binary_value_type::JE_BINARY_VALUE_LIST)
        return false;

    out_doc->m_file = file;
    out_doc->m_location = jeecs_file_tell(file);
    return true;
}

bool je_binary_string_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ char* dst, 
    size_t* inout_dst_len,
    const je_binary_string* str)
{
    assert(str != nullptr);
    assert(inout_dst_len != nullptr);
    assert(dst != nullptr || *inout_dst_len == 0);

    const size_t dst_capacity = *inout_dst_len;

    // Layout written by je_binary_append_string, the handle points at the
    // type tag:
    //   [je_binary_value_type][uint64 length][char bytes...]
    if (!jeecs_file_seek(str->m_file, (int64_t)str->m_location, JE_READ_FILE_SEEK_SET))
        return false;

    je_binary_value_type type;
    uint64_t str_len;
    if (1 != jeecs_file_read(&type, sizeof(type), 1, str->m_file)
        || type != je_binary_value_type::JE_BINARY_VALUE_STRING
        || 1 != jeecs_file_read(&str_len, sizeof(str_len), 1, str->m_file))
        return false;

    if (dst_capacity > 0)
    {
        // Copy as many bytes as the destination can hold; only terminate the
        // string when there is room left after the copied bytes.
        const uint64_t copy_len = str_len < dst_capacity ? str_len : (uint64_t)dst_capacity;

        if (copy_len > 0 && copy_len != jeecs_file_read(dst, sizeof(char), (size_t)copy_len, str->m_file))
            return false;

        if (copy_len < dst_capacity)
            dst[copy_len] = '\0';
    }

    // The capacity was consumed, report the full length of the value.
    *inout_dst_len = (size_t)str_len;
    return true;
}

bool je_binary_buffer_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ void* dst,
    size_t* inout_dst_len,
    const je_binary_buffer* buf)
{
    assert(buf != nullptr);
    assert(inout_dst_len != nullptr);
    assert(dst != nullptr || *inout_dst_len == 0);

    const size_t dst_capacity = *inout_dst_len;

    // Layout written by je_binary_append_buffer, the handle points at the
    // type tag:
    //   [je_binary_value_type][uint64 length][byte data...]
    if (!jeecs_file_seek(buf->m_file, (int64_t)buf->m_location, JE_READ_FILE_SEEK_SET))
        return false;

    je_binary_value_type type;
    uint64_t buf_len;
    if (1 != jeecs_file_read(&type, sizeof(type), 1, buf->m_file)
        || type != je_binary_value_type::JE_BINARY_VALUE_BUFFER
        || 1 != jeecs_file_read(&buf_len, sizeof(buf_len), 1, buf->m_file))
        return false;

    // A buffer has no terminator, so copy as many bytes as the destination
    // can hold; longer buffers are truncated.
    const uint64_t copy_len = buf_len < dst_capacity ? buf_len : (uint64_t)dst_capacity;
    if (copy_len > 0 && copy_len != jeecs_file_read(dst, sizeof(uint8_t), (size_t)copy_len, buf->m_file))
        return false;

    // The capacity was consumed, report the full length of the value.
    *inout_dst_len = (size_t)buf_len;
    return true;
}

static bool _je_binary_next_value_offset(
    jeecs_file* file, size_t* inout_offset)
{
    assert(file != nullptr);
    assert(inout_offset != nullptr);

    const size_t file_size = jeecs_file_size(file);
    size_t offset = *inout_offset;

    // The value may be a container, in which case the following offset is only
    // known after its END; walk the tags until the outermost value is closed.
    // Every step consumes at least the type tag, so this always terminates.
    uint32_t depth = 0;
    for (;;)
    {
        if (offset >= file_size
            || !jeecs_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        je_binary_value_type type;
        if (1 != jeecs_file_read(&type, sizeof(type), 1, file))
            return false;

        offset += sizeof(type);

        size_t payload_size = 0;
        switch (type)
        {
        case je_binary_value_type::JE_BINARY_VALUE_END:
            if (depth == 0)
                return false; // END does not start a value.

            if (--depth == 0)
            {
                *inout_offset = offset;
                return true;
            }
            continue;

        case je_binary_value_type::JE_BINARY_VALUE_LIST:
        case je_binary_value_type::JE_BINARY_VALUE_DOCUMENT:
            ++depth;
            continue;

        case je_binary_value_type::JE_BINARY_VALUE_STRING:
        case je_binary_value_type::JE_BINARY_VALUE_BUFFER:
        {
            // [uint64 length][payload...]
            uint64_t len;
            if (offset + sizeof(len) > file_size
                || 1 != jeecs_file_read(&len, sizeof(len), 1, file))
                return false;

            offset += sizeof(len);
            if (len > (uint64_t)(file_size - offset))
                return false;

            payload_size = (size_t)len;
            break;
        }

        case je_binary_value_type::JE_BINARY_VALUE_BOOL:
        case je_binary_value_type::JE_BINARY_VALUE_INT8:
        case je_binary_value_type::JE_BINARY_VALUE_UINT8:
            payload_size = sizeof(uint8_t);
            break;

        case je_binary_value_type::JE_BINARY_VALUE_INT16:
        case je_binary_value_type::JE_BINARY_VALUE_UINT16:
            payload_size = sizeof(uint16_t);
            break;

        case je_binary_value_type::JE_BINARY_VALUE_INT32:
        case je_binary_value_type::JE_BINARY_VALUE_UINT32:
        case je_binary_value_type::JE_BINARY_VALUE_FLOAT32:
            payload_size = sizeof(uint32_t);
            break;

        case je_binary_value_type::JE_BINARY_VALUE_INT64:
        case je_binary_value_type::JE_BINARY_VALUE_UINT64:
        case je_binary_value_type::JE_BINARY_VALUE_FLOAT64:
            payload_size = sizeof(uint64_t);
            break;
        }

        if (payload_size > file_size - offset)
            return false;

        offset += payload_size;

        if (depth == 0)
        {
            *inout_offset = offset;
            return true;
        }
    }
}

// Compares the string payload at [payload_location, payload_location + len)
// with `key`, reading the file in chunks to avoid allocating the key.
static bool _je_binary_key_equals(
    jeecs_file* file, size_t payload_location, uint64_t len, const char* key)
{
    assert(file != nullptr);
    assert(key != nullptr);

    if (len != (uint64_t)strlen(key))
        return false;

    if (!jeecs_file_seek(file, (int64_t)payload_location, JE_READ_FILE_SEEK_SET))
        return false;

    char chunk[256];
    size_t remain = (size_t)len;
    while (remain > 0)
    {
        const size_t chunk_len = remain < sizeof(chunk) ? remain : sizeof(chunk);
        if (chunk_len != jeecs_file_read(chunk, sizeof(char), chunk_len, file)
            || 0 != memcmp(chunk, key, chunk_len))
            return false;

        key += chunk_len;
        remain -= chunk_len;
    }
    return true;
}

// Reads the value whose type tag is at `location`. Scalars are copied into
// `out_value`; containers are handed back as handles over the file. LIST and
// DOCUMENT handles point at their first child, matching je_binary_*_open,
// while STRING and BUFFER handles point at the type tag, matching
// je_binary_string_read / je_binary_buffer_read.
static bool _je_binary_read_value(
    jeecs_file* file,
    size_t location,
    je_binary_value_type expected_type,
    void* out_value)
{
    assert(file != nullptr);
    assert(out_value != nullptr);

    if (!jeecs_file_seek(file, (int64_t)location, JE_READ_FILE_SEEK_SET))
        return false;

    je_binary_value_type type;
    if (1 != jeecs_file_read(&type, sizeof(type), 1, file)
        || type != expected_type)
        return false;

    switch (type)
    {
    case je_binary_value_type::JE_BINARY_VALUE_BOOL:
    {
        uint8_t raw;
        if (1 != jeecs_file_read(&raw, sizeof(raw), 1, file))
            return false;

        *static_cast<bool*>(out_value) = raw != 0;
        return true;
    }

    case je_binary_value_type::JE_BINARY_VALUE_INT8:
        return 1 == jeecs_file_read(out_value, sizeof(int8_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_INT16:
        return 1 == jeecs_file_read(out_value, sizeof(int16_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_INT32:
        return 1 == jeecs_file_read(out_value, sizeof(int32_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_INT64:
        return 1 == jeecs_file_read(out_value, sizeof(int64_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_UINT8:
        return 1 == jeecs_file_read(out_value, sizeof(uint8_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_UINT16:
        return 1 == jeecs_file_read(out_value, sizeof(uint16_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_UINT32:
        return 1 == jeecs_file_read(out_value, sizeof(uint32_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_UINT64:
        return 1 == jeecs_file_read(out_value, sizeof(uint64_t), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_FLOAT32:
        return 1 == jeecs_file_read(out_value, sizeof(float), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_FLOAT64:
        return 1 == jeecs_file_read(out_value, sizeof(double), 1, file);

    case je_binary_value_type::JE_BINARY_VALUE_STRING:
    {
        je_binary_string* str = static_cast<je_binary_string*>(out_value);
        str->m_file = file;
        str->m_location = location;
        return true;
    }

    case je_binary_value_type::JE_BINARY_VALUE_BUFFER:
    {
        je_binary_buffer* buf = static_cast<je_binary_buffer*>(out_value);
        buf->m_file = file;
        buf->m_location = location;
        return true;
    }

    case je_binary_value_type::JE_BINARY_VALUE_LIST:
    {
        je_binary_list* list = static_cast<je_binary_list*>(out_value);
        list->m_file = file;
        list->m_location = location + sizeof(type);
        return true;
    }

    case je_binary_value_type::JE_BINARY_VALUE_DOCUMENT:
    {
        je_binary_document* document = static_cast<je_binary_document*>(out_value);
        document->m_file = file;
        document->m_location = location + sizeof(type);
        return true;
    }

    case je_binary_value_type::JE_BINARY_VALUE_END:
        break;
    }

    return false;
}

bool je_binary_document_index(
    const je_binary_document* document,
    const char* key,
    je_binary_value_type expected_type,
    void* out_value)
{
    assert(document != nullptr);
    assert(key != nullptr);
    assert(out_value != nullptr);

    jeecs_file* file = document->m_file;
    const size_t file_size = jeecs_file_size(file);

    // A document is a sequence of [string key][value] pairs closed by END;
    // the handle points at its first key.
    size_t offset = document->m_location;
    while (offset < file_size)
    {
        if (!jeecs_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        je_binary_value_type type;
        if (1 != jeecs_file_read(&type, sizeof(type), 1, file))
            return false;

        if (type == je_binary_value_type::JE_BINARY_VALUE_END)
            return false; // The document is closed, the key is absent.

        if (type != je_binary_value_type::JE_BINARY_VALUE_STRING)
            return false; // Every entry starts with its key.

        uint64_t key_len;
        if (1 != jeecs_file_read(&key_len, sizeof(key_len), 1, file))
            return false;

        const size_t key_payload = jeecs_file_tell(file);
        if (key_len > (uint64_t)(file_size - key_payload))
            return false;

        const size_t value_location = key_payload + (size_t)key_len;

        // The first matching key wins.
        if (_je_binary_key_equals(file, key_payload, key_len, key))
            return _je_binary_read_value(
                file, value_location, expected_type, out_value);

        // Not the wanted key, step over its value.
        offset = value_location;
        if (!_je_binary_next_value_offset(file, &offset))
            return false;
    }

    return false;
}

bool je_binary_list_index(
    const je_binary_list* list,
    size_t index,
    je_binary_value_type expected_type,
    void* out_value)
{
    assert(list != nullptr);
    assert(out_value != nullptr);

    jeecs_file* file = list->m_file;

    // A list is a sequence of values closed by END; the handle points at its
    // first value.
    size_t offset = list->m_location;
    for (size_t i = 0;; ++i)
    {
        if (!jeecs_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        je_binary_value_type type;
        if (1 != jeecs_file_read(&type, sizeof(type), 1, file))
            return false;

        if (type == je_binary_value_type::JE_BINARY_VALUE_END)
            return false; // The list is closed, the index is out of range.

        if (i == index)
            return _je_binary_read_value(
                file, offset, expected_type, out_value);

        if (!_je_binary_next_value_offset(file, &offset))
            return false;
    }
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

bool je_binary_append_end(je_writing_cache_file* file)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_END;
    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file);
}

bool je_binary_append_string(je_writing_cache_file* file, const char* str)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_STRING;
    const uint64_t str_len = (uint64_t)strlen(str);

    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&str_len, sizeof(str_len), 1, file)
        && (size_t)str_len == jeecs_write_cache_file(str, sizeof(char), (size_t)str_len, file);
}

bool je_binary_append_buffer(je_writing_cache_file* file, const void* buf, size_t buflen)
{
    const je_binary_value_type type = je_binary_value_type::JE_BINARY_VALUE_BUFFER;
    const uint64_t len = (uint64_t)buflen;

    return 1 == jeecs_write_cache_file(&type, sizeof(type), 1, file)
        && 1 == jeecs_write_cache_file(&len, sizeof(len), 1, file)
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
