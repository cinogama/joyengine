#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/core.hpp"
#include "jeecs/debug.hpp"
#include "jeecs/file.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

struct dynamic_parser_impl_t
{
    woort_Value m_saving;
    woort_Value m_restoring;
    woort_Value m_edit;

    // saving/restoring 来自 types 脚本，edit 来自 editor 脚本，
    // 三者相互独立：任一脚本缺失或未提供某槽位时对应标志为 false。
    bool m_has_saving;
    bool m_has_restoring;
    bool m_has_edit;

    const je_typeinfo_script_parser_t*
        m_script_parser;
};

struct dynamic_parser_global_context_t
{
    using parser_table_t =
        unordered_map<je_type_id_t, unique_ptr<dynamic_parser_impl_t>>;

    mutex _je_dynamic_parser_mx;

    parser_table_t _je_dynamic_parser_impls;
    woort_CodeEnv* _je_dynamic_parser_types_cenv;
    woort_CodeEnv* _je_dynamic_parser_editor_cenv;
};

static dynamic_parser_global_context_t _je_dynamic_parser_global_context;

static void _je_dynamic_parser_clear()
{
    if (_je_dynamic_parser_global_context._je_dynamic_parser_types_cenv != nullptr)
    {
        woort_codeenv_drop(_je_dynamic_parser_global_context._je_dynamic_parser_types_cenv);
        _je_dynamic_parser_global_context._je_dynamic_parser_types_cenv = nullptr;
    }
    if (_je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv != nullptr)
    {
        woort_codeenv_drop(_je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv);
        _je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv = nullptr;
    }

    _je_dynamic_parser_global_context._je_dynamic_parser_impls.clear();

    assert(_je_dynamic_parser_global_context._je_dynamic_parser_impls.empty());
}

static void _je_dynamic_parser_refresh(woort_value tmp)
{
    _je_dynamic_parser_global_context._je_dynamic_parser_impls.clear();

    auto** types = je_debug_types_registered_get_all();

    auto** cur_type = types;
    while (*cur_type)
    {
        auto* script_parser = typing::get_script_parser(*cur_type);
        if (script_parser != nullptr)
        {
            string script_woolang_typename = script_parser->m_woolang_typename;

            woort_Value* const tmp_internal_storage = woort_internal_value(tmp);

            auto p = make_unique<dynamic_parser_impl_t>();

            if (_je_dynamic_parser_global_context._je_dynamic_parser_types_cenv != nullptr)
            {
                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_types_cenv,
                    (script_woolang_typename + "::parser::saving").c_str()))
                {
                    p->m_saving = *tmp_internal_storage;
                    p->m_has_saving = true;
                }

                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_types_cenv,
                    (script_woolang_typename + "::parser::restoring").c_str()))
                {
                    p->m_restoring = *tmp_internal_storage;
                    p->m_has_restoring = true;
                }
            }

            if (_je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv != nullptr)
            {
                if (woort_load_extern_const(
                    tmp,
                    _je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv,
                    (script_woolang_typename + "::parser::edit").c_str()))
                {
                    p->m_edit = *tmp_internal_storage;
                    p->m_has_edit = true;
                }
            }

            if (p->m_has_saving || p->m_has_restoring || p->m_has_edit)
            {
                p->m_script_parser = script_parser;
                _je_dynamic_parser_global_context._je_dynamic_parser_impls.emplace(
                    (*cur_type)->m_id, move(p));
            }
        }

        ++cur_type;
    }

    je_mem_free(types);
}

static optional<string> _je_dynamic_parser_update_script(
    woort_CodeEnv*& target_cenv, const char* path)
{
    using namespace std;

    auto* file = je_file_open(path);
    if (file == nullptr)
        return optional("Failed to open '"s + path + "'.");

    size_t filelen = je_file_size(file);

    char* const content = (char*)malloc(filelen + 1);
    if (content == nullptr)
    {
        return optional("Out of memory.");
    }

    (void)je_file_read(content, sizeof(char), filelen, file);

    wo_CompileErrors* cerror;
    woort_CodeEnv* const cenv = wo_load_binary(path, content, filelen, &cerror);
    free(content);

    if (cenv == nullptr)
    {
        optional<string> r;
        r.emplace(wo_get_compile_error(cerror, WO_PLAIM));

        wo_compile_errors_free(cerror);

        return r;
    }

    woort_vm* const vmm = woort_vm_create();
    if (vmm == nullptr)
    {
        woort_codeenv_drop(cenv);
        return optional("Out of memory.");
    }

    woort_vm* const last = woort_vm_swap(vmm);
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
        {
            (void)woort_vm_swap(last);
            woort_codeenv_drop(cenv);
            woort_vm_close(vmm);

            return optional("Stack overflow.");
        }

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup(WOORT_IGNORE, cenv, true))
        {
            (void)woort_vm_swap(last);

            optional<string> r;
            r.emplace(woort_vm_get_runtime_error(vmm));

            woort_codeenv_drop(cenv);
            woort_vm_close(vmm);
            return r;
        }

        // 只替换目标槽位的旧脚本，再从两个脚本环境全量重建解析表；
        // 另一个槽位不受影响。
        if (target_cenv != nullptr)
            woort_codeenv_drop(target_cenv);
        target_cenv = cenv;
        _je_dynamic_parser_refresh(s);
    }
    (void)woort_vm_swap(last);
    woort_vm_close(vmm);

    return nullopt;
}

////////////////////////////////////////////////////////////////

static woort_api _wojeapi_dynamic_parser_update(woort_CodeEnv*& target_cenv)
{
    optional<string> result;

    const woort_U8CString parser_path = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        result = _je_dynamic_parser_update_script(target_cenv, parser_path);
    }
    (void)woort_vm_swap(last);

    if (result.has_value())
        return woort_ret_result_err_string(result.value().c_str());
    return woort_ret_result_ok_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_update_types(void)
{
    return _wojeapi_dynamic_parser_update(
        _je_dynamic_parser_global_context._je_dynamic_parser_types_cenv);
}

WOORT_API woort_api wojeapi_dynamic_parser_update_editor(void)
{
    return _wojeapi_dynamic_parser_update(
        _je_dynamic_parser_global_context._je_dynamic_parser_editor_cenv);
}

WOORT_API woort_api wojeapi_dynamic_parser_refresh(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    woort_vm* const last = woort_vm_swap(nullptr);
    lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
    (void)woort_vm_swap(last);

    _je_dynamic_parser_refresh(s);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_clear(void)
{
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);
        _je_dynamic_parser_clear();
    }
    (void)woort_vm_swap(last);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_dynamic_parser_save(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const je_type_info_t*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(
        type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        auto* val = woort_pointer(1);
        auto& parser = fnd->second;

        if (!parser->m_has_saving)
            return woort_ret_option_none();

        const woort_value value = s + 0;
        const woort_value func = s + 1;

        parser->m_script_parser->m_script_parse_c2w(val, value);

        *woort_internal_value(func) = parser->m_saving;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(value, func))
            return woort_ret_panic("Failed to invoke `save` callback.");

        return woort_ret_option_value(value);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_dynamic_parser_restore(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const je_type_info_t*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        void* val = woort_pointer(1);
        const char* dat = woort_string(2);
        auto& parser = fnd->second;

        if (!parser->m_has_restoring)
            return woort_ret_bool(false);

        const woort_value result = s + 0;
        const woort_value func = s + 1;

        woort_set_string(result, dat);

        *woort_internal_value(func) = parser->m_restoring;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(result, func))
            return woort_ret_panic("Failed to invoke `restore` callback.");

        parser->m_script_parser->m_script_parse_w2c(val, result);
        return woort_ret_bool(true);
    }
    return woort_ret_bool(false);
}

WOORT_API woort_api wojeapi_dynamic_parser_edit(void)
{
    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    lock_guard g1(_je_dynamic_parser_global_context._je_dynamic_parser_mx);

    auto* type = (const je_type_info_t*)woort_pointer(0);
    auto fnd = _je_dynamic_parser_global_context._je_dynamic_parser_impls.find(
        type->m_id);

    if (fnd != _je_dynamic_parser_global_context._je_dynamic_parser_impls.end())
    {
        auto* val = woort_pointer(1);
        const char* tag = woort_string(2);
        auto& parser = fnd->second;

        if (!parser->m_has_edit)
            return woort_ret_option_none();

        const woort_value value = s + 0;
        const woort_value tag_slot = s + 1;
        const woort_value func = s + 2;

        parser->m_script_parser->m_script_parse_c2w(val, value);
        woort_set_string(tag_slot, tag);

        *woort_internal_value(func) = parser->m_edit;

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(value, func))
            return woort_ret_panic("Failed to invoke `edit` callback.");

        const bool updated = woort_option_get(value, value);
        if (updated)
            parser->m_script_parser->m_script_parse_w2c(val, value);

        return woort_ret_option_bool(updated);
    }
    return woort_ret_option_none();
}
