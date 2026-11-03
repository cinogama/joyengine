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

enum class je_binary_value_type: uint8_t
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
    JE_BINARY_VALUE_LIST,
    JE_BINARY_VALUE_DOCUMENT,
};

void je_binary_document_open(jeecs_file* file, je_binary_document* out_doc)
{
    out_doc->m_file = file;
    out_doc->m_location = jeecs_file_tell(file);
}

bool je_binary_document_index(
    const je_binary_document* doc, 
    const char* key, 
    je_binary_value_type expected_type,
    void* out_value)
{

}


