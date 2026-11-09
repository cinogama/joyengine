#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

// ECS TYPEINFO
WOORT_API woort_api wojeapi_type_of_name(void)
{
    auto* const je_type_info_t =
        typing::of(woort_string(0));

    if (je_type_info_t == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(je_type_info_t);
}

WOORT_API woort_api wojeapi_type_of_id(void)
{
    auto* const je_type_info_t =
        typing::of((je_type_id_t)woort_int(0));

    if (je_type_info_t == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(je_type_info_t);
}

WOORT_API woort_api wojeapi_get_all_registed_types(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    auto** types = je_debug_types_registered_get_all();

    auto** cur_type = types;
    while (*cur_type)
    {
        woort_set_box_pointer(elem, (void*)*(cur_type++));
        woort_vec_push(out_array, elem);
    }

    je_mem_free(types);
    return woort_ret_value(out_array);
}

WOORT_API woort_api wojeapi_type_is_component(void)
{
    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);
    return woort_ret_bool(type->m_type_class == je_typing_class_t::JE_COMPONENT);
}

WOORT_API woort_api wojeapi_type_is_system(void)
{
    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);
    return woort_ret_bool(typing::is_system(type));
}

WOORT_API woort_api wojeapi_type_id(void)
{
    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);
    return woort_ret_int(type->m_id);
}

WOORT_API woort_api wojeapi_type_name(void)
{
    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);
    return woort_ret_string(type->m_typename);
}

WOORT_API woort_api wojeapi_script_type_name(void)
{
    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);
    auto* parser = typing::get_script_parser(type);
    if (parser != nullptr)
        return woort_ret_option_string(parser->m_woolang_typename);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_type_members(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const je_type_info_t* type = (const je_type_info_t*)woort_pointer(0);

    const woort_value result = s + 0;
    const woort_value elem = s + 1;
    const woort_value elem2 = s + 2;

    woort_set_vec(result);

    if (type->m_member_types != nullptr)
    {
        auto* member_iter = type->m_member_types->m_members;
        while (member_iter != nullptr)
        {
            woort_set_struct(elem, 2);

            woort_set_string(elem2, member_iter->m_member_name);
            woort_struct_set(elem, 0, elem2);
            woort_set_pointer(elem2, (void*)member_iter->m_member_type);
            woort_struct_set(elem, 1, elem2);

            woort_vec_push(result, elem);

            member_iter = member_iter->m_next_member;
        }
    }

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_type_basic_type(void)
{
    enum basic_type
    {
        INT,
        INT2,
        BOOL,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        STRING,
        QUAT,
    };
    basic_type type = (basic_type)woort_int(0);

    switch (type)
    {
    case INT:
        return woort_ret_pointer((void*)typing::of<int>());
    case INT2:
        return woort_ret_pointer((void*)typing::of<math::ivec2>());
    case BOOL:
        return woort_ret_pointer((void*)typing::of<bool>());
    case FLOAT:
        return woort_ret_pointer((void*)typing::of<float>());
    case FLOAT2:
        return woort_ret_pointer((void*)typing::of<math::vec2>());
    case FLOAT3:
        return woort_ret_pointer((void*)typing::of<math::vec3>());
    case FLOAT4:
        return woort_ret_pointer((void*)typing::of<math::vec4>());
    case STRING:
        return woort_ret_pointer((void*)typing::of<basic::string>());
    case QUAT:
        return woort_ret_pointer((void*)typing::of<math::quat>());
    default:
        return woort_ret_panic("Unknown basic type.");
    }
}
