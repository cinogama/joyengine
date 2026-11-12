#define JE_IMPL
#include "jeecs/cache.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"

// A value is stored as [1 byte type tag][payload]. Lists and documents are
// terminated by JE_CACHE_BINARY_VALUE_END; strings and buffers are prefixed by
// their uint64 length. The on-disk layout is documented in jeecs.hpp.

bool je_cache_binary_document_open(
    je_file_t* file, je_cache_binary_document_t* out_doc)
{
    assert(file != nullptr);
    assert(out_doc != nullptr);

    uint8_t tag;
    if (1 != je_file_read(&tag, sizeof(tag), 1, file)
        || tag != (uint8_t)JE_CACHE_BINARY_VALUE_DOCUMENT)
        return false;

    out_doc->m_file = file;
    out_doc->m_location = je_file_tell(file);
    return true;
}

bool je_cache_binary_list_open(
    je_file_t* file, je_cache_binary_list_t* out_list)
{
    assert(file != nullptr);
    assert(out_list != nullptr);

    uint8_t tag;
    if (1 != je_file_read(&tag, sizeof(tag), 1, file)
        || tag != (uint8_t)JE_CACHE_BINARY_VALUE_LIST)
        return false;

    out_list->m_file = file;
    out_list->m_location = je_file_tell(file);
    return true;
}

bool je_cache_binary_string_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ char* dst, 
    size_t* inout_dst_len,
    const je_cache_binary_string_t* str)
{
    assert(str != nullptr);
    assert(inout_dst_len != nullptr);
    assert(dst != nullptr || *inout_dst_len == 0);

    const size_t dst_capacity = *inout_dst_len;

    // Layout written by je_cache_binary_append_string, the handle points at
    // the type tag:
    //   [je_cache_binary_value_type_t][uint64 length][char bytes...]
    if (!je_file_seek(str->m_file, (int64_t)str->m_location, JE_READ_FILE_SEEK_SET))
        return false;

    uint8_t tag;
    uint64_t str_len;
    if (1 != je_file_read(&tag, sizeof(tag), 1, str->m_file)
        || tag != (uint8_t)JE_CACHE_BINARY_VALUE_STRING
        || 1 != je_file_read(&str_len, sizeof(str_len), 1, str->m_file))
        return false;

    if (dst_capacity > 0)
    {
        // Copy as many bytes as the destination can hold; only terminate the
        // string when there is room left after the copied bytes.
        const uint64_t copy_len = str_len < dst_capacity ? str_len : (uint64_t)dst_capacity;

        if (copy_len > 0 && copy_len != je_file_read(dst, sizeof(char), (size_t)copy_len, str->m_file))
            return false;

        if (copy_len < dst_capacity)
            dst[copy_len] = '\0';
    }

    // The capacity was consumed, report the full length of the value.
    *inout_dst_len = (size_t)str_len;
    return true;
}

bool je_cache_binary_buffer_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ void* dst,
    size_t* inout_dst_len,
    const je_cache_binary_buffer_t* buf)
{
    assert(buf != nullptr);
    assert(inout_dst_len != nullptr);
    assert(dst != nullptr || *inout_dst_len == 0);

    const size_t dst_capacity = *inout_dst_len;

    // Layout written by je_cache_binary_append_buffer, the handle points at
    // the type tag:
    //   [je_cache_binary_value_type_t][uint64 length][byte data...]
    if (!je_file_seek(buf->m_file, (int64_t)buf->m_location, JE_READ_FILE_SEEK_SET))
        return false;

    uint8_t tag;
    uint64_t buf_len;
    if (1 != je_file_read(&tag, sizeof(tag), 1, buf->m_file)
        || tag != (uint8_t)JE_CACHE_BINARY_VALUE_BUFFER
        || 1 != je_file_read(&buf_len, sizeof(buf_len), 1, buf->m_file))
        return false;

    // A buffer has no terminator, so copy as many bytes as the destination
    // can hold; longer buffers are truncated.
    const uint64_t copy_len = buf_len < dst_capacity ? buf_len : (uint64_t)dst_capacity;
    if (copy_len > 0 && copy_len != je_file_read(dst, sizeof(uint8_t), (size_t)copy_len, buf->m_file))
        return false;

    // The capacity was consumed, report the full length of the value.
    *inout_dst_len = (size_t)buf_len;
    return true;
}

static bool _jeecs_cache_binary_next_value_offset(
    je_file_t* file, size_t* inout_offset)
{
    assert(file != nullptr);
    assert(inout_offset != nullptr);

    const size_t file_size = je_file_size(file);
    size_t offset = *inout_offset;

    // The value may be a container, in which case the following offset is only
    // known after its END; walk the tags until the outermost value is closed.
    // Every step consumes at least the type tag, so this always terminates.
    uint32_t depth = 0;
    for (;;)
    {
        if (offset >= file_size
            || !je_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        const size_t tag_offset = offset;

        uint8_t tag;
        if (1 != je_file_read(&tag, sizeof(tag), 1, file))
            return false;

        offset += sizeof(tag);

        size_t payload_size = 0;
        switch ((je_cache_binary_value_type_t)tag)
        {
        case JE_CACHE_BINARY_VALUE_END:
            if (depth == 0)
                return false; // END does not start a value.

            if (--depth == 0)
            {
                *inout_offset = offset;
                return true;
            }
            continue;

        case JE_CACHE_BINARY_VALUE_LIST:
        case JE_CACHE_BINARY_VALUE_DOCUMENT:
            ++depth;
            continue;

        case JE_CACHE_BINARY_VALUE_STRING:
        case JE_CACHE_BINARY_VALUE_BUFFER:
        {
            // [uint64 length][payload...]
            uint64_t len;
            if (offset + sizeof(len) > file_size
                || 1 != je_file_read(&len, sizeof(len), 1, file))
                return false;

            offset += sizeof(len);
            if (len > (uint64_t)(file_size - offset))
                return false;

            payload_size = (size_t)len;
            break;
        }

        case JE_CACHE_BINARY_VALUE_BOOL:
        case JE_CACHE_BINARY_VALUE_INT8:
        case JE_CACHE_BINARY_VALUE_UINT8:
            payload_size = sizeof(uint8_t);
            break;

        case JE_CACHE_BINARY_VALUE_INT16:
        case JE_CACHE_BINARY_VALUE_UINT16:
            payload_size = sizeof(uint16_t);
            break;

        case JE_CACHE_BINARY_VALUE_INT32:
        case JE_CACHE_BINARY_VALUE_UINT32:
        case JE_CACHE_BINARY_VALUE_FLOAT32:
            payload_size = sizeof(uint32_t);
            break;

        case JE_CACHE_BINARY_VALUE_INT64:
        case JE_CACHE_BINARY_VALUE_UINT64:
        case JE_CACHE_BINARY_VALUE_FLOAT64:
            payload_size = sizeof(uint64_t);
            break;

        default:
            jeecs::debug::logerr(
                "Invalid binary cache value type: %u at offset: %zu.",
                (unsigned)tag,
                tag_offset);
            return false;
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

enum class _jeecs_cache_binary_key_compare_result
{
    EQUAL,
    NOT_EQUAL,
    ERROR,
};

// Compares the string payload at [payload_location, payload_location + len)
// with `key`, reading the file in chunks to avoid allocating the key. Read
// failures are reported as ERROR so the caller does not mistake a broken file
// for a missing key.
static _jeecs_cache_binary_key_compare_result _jeecs_cache_binary_compare_key(
    je_file_t* file, size_t payload_location, uint64_t len, const char* key)
{
    assert(file != nullptr);
    assert(key != nullptr);

    if (len != (uint64_t)strlen(key))
        return _jeecs_cache_binary_key_compare_result::NOT_EQUAL;

    if (!je_file_seek(file, (int64_t)payload_location, JE_READ_FILE_SEEK_SET))
    {
        jeecs::debug::logerr(
            "Failed to seek to binary cache key payload at offset: %zu.",
            payload_location);

        return _jeecs_cache_binary_key_compare_result::ERROR;
    }

    char chunk[256];
    size_t remain = (size_t)len;
    while (remain > 0)
    {
        const size_t chunk_len = remain < sizeof(chunk) ? remain : sizeof(chunk);
        if (chunk_len != je_file_read(chunk, sizeof(char), chunk_len, file))
        {
            jeecs::debug::logerr(
                "Failed to read binary cache key payload at offset: %zu.",
                payload_location);

            return _jeecs_cache_binary_key_compare_result::ERROR;
        }

        if (0 != memcmp(chunk, key, chunk_len))
            return _jeecs_cache_binary_key_compare_result::NOT_EQUAL;

        key += chunk_len;
        remain -= chunk_len;
    }
    return _jeecs_cache_binary_key_compare_result::EQUAL;
}

// Reads the value whose type tag is at `location`. Scalars are copied into
// `out_value`; containers are handed back as handles over the file. LIST and
// DOCUMENT handles point at their first child, matching
// jeecs_cache_binary_*_open, while STRING and BUFFER handles point at the type
// tag, matching je_cache_binary_string_read /
// je_cache_binary_buffer_read.
static bool _jeecs_cache_binary_read_value(
    je_file_t* file,
    size_t location,
    je_cache_binary_value_type_t expected_type,
    void* out_value)
{
    assert(file != nullptr);
    assert(out_value != nullptr);

    if (!je_file_seek(file, (int64_t)location, JE_READ_FILE_SEEK_SET))
        return false;

    uint8_t tag;
    if (1 != je_file_read(&tag, sizeof(tag), 1, file)
        || tag != (uint8_t)expected_type)
        return false;

    switch ((je_cache_binary_value_type_t)tag)
    {
    case JE_CACHE_BINARY_VALUE_BOOL:
    {
        uint8_t raw;
        if (1 != je_file_read(&raw, sizeof(raw), 1, file))
            return false;

        *static_cast<bool*>(out_value) = raw != 0;
        return true;
    }

    case JE_CACHE_BINARY_VALUE_INT8:
        return 1 == je_file_read(out_value, sizeof(int8_t), 1, file);

    case JE_CACHE_BINARY_VALUE_INT16:
        return 1 == je_file_read(out_value, sizeof(int16_t), 1, file);

    case JE_CACHE_BINARY_VALUE_INT32:
        return 1 == je_file_read(out_value, sizeof(int32_t), 1, file);

    case JE_CACHE_BINARY_VALUE_INT64:
        return 1 == je_file_read(out_value, sizeof(int64_t), 1, file);

    case JE_CACHE_BINARY_VALUE_UINT8:
        return 1 == je_file_read(out_value, sizeof(uint8_t), 1, file);

    case JE_CACHE_BINARY_VALUE_UINT16:
        return 1 == je_file_read(out_value, sizeof(uint16_t), 1, file);

    case JE_CACHE_BINARY_VALUE_UINT32:
        return 1 == je_file_read(out_value, sizeof(uint32_t), 1, file);

    case JE_CACHE_BINARY_VALUE_UINT64:
        return 1 == je_file_read(out_value, sizeof(uint64_t), 1, file);

    case JE_CACHE_BINARY_VALUE_FLOAT32:
        return 1 == je_file_read(out_value, sizeof(float), 1, file);

    case JE_CACHE_BINARY_VALUE_FLOAT64:
        return 1 == je_file_read(out_value, sizeof(double), 1, file);

    case JE_CACHE_BINARY_VALUE_STRING:
    {
        je_cache_binary_string_t* str = static_cast<je_cache_binary_string_t*>(out_value);
        str->m_file = file;
        str->m_location = location;
        return true;
    }

    case JE_CACHE_BINARY_VALUE_BUFFER:
    {
        je_cache_binary_buffer_t* buf = static_cast<je_cache_binary_buffer_t*>(out_value);
        buf->m_file = file;
        buf->m_location = location;
        return true;
    }

    case JE_CACHE_BINARY_VALUE_LIST:
    {
        je_cache_binary_list_t* list = static_cast<je_cache_binary_list_t*>(out_value);
        list->m_file = file;
        list->m_location = location + sizeof(tag);
        return true;
    }

    case JE_CACHE_BINARY_VALUE_DOCUMENT:
    {
        je_cache_binary_document_t* document = static_cast<je_cache_binary_document_t*>(out_value);
        document->m_file = file;
        document->m_location = location + sizeof(tag);
        return true;
    }

    case JE_CACHE_BINARY_VALUE_END:
        break;
    }

    return false;
}

bool je_cache_binary_document_index(
    const je_cache_binary_document_t* document,
    const char* key,
    je_cache_binary_value_type_t expected_type,
    void* out_value)
{
    assert(document != nullptr);
    assert(key != nullptr);
    assert(out_value != nullptr);

    je_file_t* file = document->m_file;
    const size_t file_size = je_file_size(file);

    // A document is a sequence of [string key][value] pairs closed by END;
    // the handle points at its first key.
    size_t offset = document->m_location;
    while (offset < file_size)
    {
        if (!je_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        uint8_t tag;
        if (1 != je_file_read(&tag, sizeof(tag), 1, file))
            return false;

        if (tag == (uint8_t)JE_CACHE_BINARY_VALUE_END)
            return false; // The document is closed, the key is absent.

        if (tag != (uint8_t)JE_CACHE_BINARY_VALUE_STRING)
            return false; // Every entry starts with its key.

        uint64_t key_len;
        if (1 != je_file_read(&key_len, sizeof(key_len), 1, file))
            return false;

        const size_t key_payload = je_file_tell(file);
        if (key_len > (uint64_t)(file_size - key_payload))
            return false;

        const size_t value_location = key_payload + (size_t)key_len;

        // The first matching key wins.
        switch (_jeecs_cache_binary_compare_key(file, key_payload, key_len, key))
        {
        case _jeecs_cache_binary_key_compare_result::ERROR:
            return false;

        case _jeecs_cache_binary_key_compare_result::EQUAL:
            return _jeecs_cache_binary_read_value(
                file, value_location, expected_type, out_value);

        case _jeecs_cache_binary_key_compare_result::NOT_EQUAL:
            break;
        }

        // Not the wanted key, step over its value.
        offset = value_location;
        if (!_jeecs_cache_binary_next_value_offset(file, &offset))
            return false;
    }

    return false;
}

bool je_cache_binary_list_index(
    const je_cache_binary_list_t* list,
    size_t index,
    je_cache_binary_value_type_t expected_type,
    void* out_value)
{
    assert(list != nullptr);
    assert(out_value != nullptr);

    je_file_t* file = list->m_file;

    // A list is a sequence of values closed by END; the handle points at its
    // first value.
    size_t offset = list->m_location;
    for (size_t i = 0;; ++i)
    {
        if (!je_file_seek(file, (int64_t)offset, JE_READ_FILE_SEEK_SET))
            return false;

        uint8_t tag;
        if (1 != je_file_read(&tag, sizeof(tag), 1, file))
            return false;

        if (tag == (uint8_t)JE_CACHE_BINARY_VALUE_END)
            return false; // The list is closed, the index is out of range.

        if (i == index)
            return _jeecs_cache_binary_read_value(
                file, offset, expected_type, out_value);

        if (!_jeecs_cache_binary_next_value_offset(file, &offset))
            return false;
    }
}

bool je_cache_binary_list_iterator_open(
    const je_cache_binary_list_t* list,
    je_cache_binary_list_iterator_t* out_iterator)
{
    assert(list != nullptr);
    assert(out_iterator != nullptr);

    out_iterator->m_file = list->m_file;
    out_iterator->m_location = list->m_location;
    return true;
}

bool je_cache_binary_list_iterator_is_end(
    const je_cache_binary_list_iterator_t* iterator)
{
    assert(iterator != nullptr);

    je_file_t* file = iterator->m_file;
    if (!je_file_seek(file, (int64_t)iterator->m_location,
        JE_READ_FILE_SEEK_SET))
        return false;

    uint8_t tag;
    if (1 != je_file_read(&tag, sizeof(tag), 1, file))
        return false;

    return tag == (uint8_t)JE_CACHE_BINARY_VALUE_END;
}

bool je_cache_binary_list_iterator_read(
    const je_cache_binary_list_iterator_t* iterator,
    je_cache_binary_value_type_t expected_type,
    void* out_value)
{
    assert(iterator != nullptr);
    assert(out_value != nullptr);

    return _jeecs_cache_binary_read_value(
        iterator->m_file, iterator->m_location, expected_type, out_value);
}

bool je_cache_binary_list_iterator_advance(
    je_cache_binary_list_iterator_t* inout_iterator)
{
    assert(inout_iterator != nullptr);

    size_t offset = inout_iterator->m_location;
    if (!_jeecs_cache_binary_next_value_offset(inout_iterator->m_file, &offset))
        return false;

    inout_iterator->m_location = offset;
    return true;
}

/* ======================================= */

bool je_cache_binary_append_document(je_writing_cache_file_t* file)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_DOCUMENT;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file);
}

bool je_cache_binary_append_list(je_writing_cache_file_t* file)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_LIST;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file);
}

bool je_cache_binary_append_end(je_writing_cache_file_t* file)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_END;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file);
}

bool je_cache_binary_append_string(je_writing_cache_file_t* file, const char* str)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_STRING;
    const uint64_t str_len = (uint64_t)strlen(str);

    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&str_len, sizeof(str_len), 1, file)
        && (size_t)str_len == je_cache_file_write(str, sizeof(char), (size_t)str_len, file);
}

bool je_cache_binary_append_buffer(
    je_writing_cache_file_t* file, const void* buf, size_t buflen)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_BUFFER;
    const uint64_t len = (uint64_t)buflen;

    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&len, sizeof(len), 1, file)
        && buflen == je_cache_file_write(buf, 1, buflen, file);
}

bool je_cache_binary_append_raw_buffer(
    je_writing_cache_file_t* file, const void* buf, size_t buflen)
{
    const uint64_t len = (uint64_t)buflen;

    return 1 == je_cache_file_write(&len, sizeof(len), 1, file)
        && buflen == je_cache_file_write(buf, 1, buflen, file);
}

bool je_cache_binary_append_bool(je_writing_cache_file_t* file, bool val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_BOOL;
    const uint8_t raw = val ? 1 : 0;

    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&raw, sizeof(raw), 1, file);
}

bool je_cache_binary_append_int8(je_writing_cache_file_t* file, int8_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_INT8;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_int16(je_writing_cache_file_t* file, int16_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_INT16;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_int32(je_writing_cache_file_t* file, int32_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_INT32;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_int64(je_writing_cache_file_t* file, int64_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_INT64;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_uint8(je_writing_cache_file_t* file, uint8_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_UINT8;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_uint16(je_writing_cache_file_t* file, uint16_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_UINT16;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_uint32(je_writing_cache_file_t* file, uint32_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_UINT32;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_uint64(je_writing_cache_file_t* file, uint64_t val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_UINT64;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_float32(je_writing_cache_file_t* file, float val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_FLOAT32;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}

bool je_cache_binary_append_float64(je_writing_cache_file_t* file, double val)
{
    const uint8_t tag = (uint8_t)JE_CACHE_BINARY_VALUE_FLOAT64;
    return 1 == je_cache_file_write(&tag, sizeof(tag), 1, file)
        && 1 == je_cache_file_write(&val, sizeof(val), 1, file);
}
