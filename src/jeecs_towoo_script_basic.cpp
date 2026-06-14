#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <unordered_map>
#include <optional>
#include <memory>
#include <cmath>

#include "jeecs_core_script_system.hpp"

/*
TooWooo~
toWoo JoyEngine generated wrap code for woolang.
*/

namespace jeecs
{
    namespace towoo
    {
        struct ToWooBaseSystem : game_system
        {
            struct towoo_step_work
            {
                dependence m_dependence;
                woort_Value m_function;
                std::vector<const typing::type_info*> m_used_components;
                bool m_is_single_work;
            };
            struct towoo_system_info
            {
                JECS_DISABLE_MOVE_AND_COPY(towoo_system_info);

                woort_CodeEnv* m_code_env;
                bool m_is_good;

                std::optional<woort_Value> m_on_enable_function;
                std::optional<woort_Value> m_on_disable_function;
                woort_Value m_create_function;
                woort_Value m_close_function;

                std::vector<towoo_step_work> m_preworks;
                std::vector<towoo_step_work> m_works;
                std::vector<towoo_step_work> m_lateworks;

                towoo_system_info(woort_CodeEnv* cenv)
                    : m_code_env(cenv)
                {}
                ~towoo_system_info()
                {
                    woort_CodeEnv_drop(m_code_env);
                }
            };

            inline static std::shared_mutex _registered_towoo_base_systems_mx;
            inline static std::unordered_map<
                const typing::type_info*, std::unique_ptr<towoo_system_info>>
                _registered_towoo_base_systems;

            woort_vm* m_job_vm;
            woort_value m_context;

            std::optional<woort_value> m_on_enable_function;
            std::optional<woort_value> m_on_disable_function;
            woort_value m_create_function;
            woort_value m_close_function;
            woort_value m_work_function;

            const typing::type_info* m_type;

            std::vector<towoo_step_work> m_pre_dependences;
            std::vector<towoo_step_work> m_dependences;
            std::vector<towoo_step_work> m_late_dependences;

            JECS_DISABLE_MOVE_AND_COPY(ToWooBaseSystem);

            ToWooBaseSystem(game_world w, const typing::type_info* ty)
                : game_system(w)
                , m_type(ty)
                , m_job_vm(nullptr)
            {
                std::shared_lock sg1(_registered_towoo_base_systems_mx);

                auto& base_info = _registered_towoo_base_systems.at(m_type);

                if (!base_info->m_is_good)
                {
                    jeecs::debug::logerr("System '%s' cannot create normally, "
                        "please check the corresponding script for errors.",
                        m_type->m_typename);
                    return;
                }

                m_pre_dependences = base_info->m_preworks;
                m_dependences = base_info->m_works;
                m_late_dependences = base_info->m_lateworks;

                _init_job_vm(*base_info);
            }

            ~ToWooBaseSystem()
            {
                if (m_job_vm == nullptr)
                    return;

                woort_vm* const last = woort_vm_swap(m_job_vm);
                {
                    if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, m_close_function))
                    {
                        jeecs::debug::logerr("Failed to invoke 'close' function for system: '%s'.",
                            m_type->m_typename);
                    }
                }
                (void)woort_vm_swap(last);

                _drop();
            }

            static void create_component_struct(
                woort_value writeval,
                woort_value tmpval,
                void* component,
                const typing::type_info* ctype)
            {
                assert(component != nullptr);

                if (ctype->m_member_types != nullptr)
                {
                    woort_set_struct(writeval, ctype->m_member_types->m_member_count + 1);

                    uint16_t member_idx = 0;
                    auto* member_tinfo = ctype->m_member_types->m_members;
                    while (member_tinfo != nullptr)
                    {
                        woort_set_pointer(tmpval,
                            reinterpret_cast<void*>(
                                reinterpret_cast<intptr_t>(component)
                                + member_tinfo->m_member_offset));

                        woort_struct_set(writeval, member_idx + 1, tmpval);

                        ++member_idx;
                        member_tinfo = member_tinfo->m_next_member;
                    }
                }
                else
                {
                    woort_set_struct(writeval, 1);
                }

                woort_set_pointer(tmpval, component);
                woort_struct_set(writeval, 0, tmpval);
            }

            void _drop()
            {
                assert(m_job_vm != nullptr);
                woort_vm_close(m_job_vm);
                m_job_vm = nullptr;
            }

        private:
            void _init_job_vm(towoo_system_info& base_info)
            {
                m_job_vm = woort_vm_create();
                if (m_job_vm == nullptr)
                {
                    jeecs::debug::logerr("Failed to create vm for system: '%s'.",
                        m_type->m_typename);
                    return;
                }

                woort_vm* const last = woort_vm_swap(m_job_vm);
                {
                    woort_value stack_base;
                    if (!woort_push_reserve(6, &stack_base))
                    {
                        jeecs::debug::logerr("Failed to reserve stack for system: '%s'.",
                            m_type->m_typename);
                        woort_vm_close(m_job_vm);
                        m_job_vm = nullptr;
                    }
                    else
                    {
                        m_context = stack_base + 0;
                        woort_set_pointer(m_context, m_job_vm);

                        m_create_function = stack_base + 1;
                        *woort_internal_value(m_create_function) = base_info.m_create_function;

                        m_close_function = stack_base + 2;
                        *woort_internal_value(m_close_function) = base_info.m_close_function;

                        if (base_info.m_on_enable_function.has_value())
                        {
                            *woort_internal_value(stack_base + 3) = base_info.m_on_enable_function.value();
                            m_on_enable_function.emplace(stack_base + 3);
                        }
                        if (base_info.m_on_disable_function.has_value())
                        {
                            *woort_internal_value(stack_base + 4) = base_info.m_on_disable_function.value();
                            m_on_disable_function.emplace(stack_base + 4);
                        }

                        m_work_function = stack_base + 5;

                        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(m_context, m_create_function))
                        {
                            jeecs::debug::logerr("Failed to invoke 'create' function for system: '%s'.",
                                m_type->m_typename);
                            woort_vm_close(m_job_vm);
                            m_job_vm = nullptr;
                        }
                    }
                }
                (void)woort_vm_swap(last);
            }

            void _invoke_single_work(const towoo_step_work& work, bool& aborted)
            {
                *woort_internal_value(m_work_function) = work.m_function;
                if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, m_work_function))
                    aborted = true;
            }

            void _invoke_multi_work(towoo_step_work& work, bool& aborted)
            {
                work.m_dependence.update(get_world());
                for (const auto& archinfo : work.m_dependence.m_archs)
                {
                    auto cur_chunk = je_arch_get_chunk(archinfo.m_arch);
                    const size_t used_component_count = work.m_used_components.size();

                    woort_value stack_base;
                    if (!woort_push_reserve(used_component_count + 2, &stack_base))
                        break;

                    woort_set_value(stack_base + 0, m_context);

                    while (cur_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                        typing::version_t version;
                        for (typing::entity_id_in_chunk_t eid = 0;
                            eid < archinfo.m_entity_count; ++eid)
                        {
                            if (game_entity::entity_stat::READY != entity_meta_addr[eid].m_stat)
                                continue;

                            version = entity_meta_addr[eid].m_version;

                            for (auto cmpidx = work.m_used_components.begin();
                                cmpidx != work.m_used_components.end(); ++cmpidx)
                            {
                                const size_t cmpid = cmpidx - work.m_used_components.begin();
                                void* component = slice_requirement::base::view_base::get_component_from_archchunk_ptr(
                                    &archinfo, cur_chunk, eid, cmpid);
                                const auto* typeinfo = *cmpidx;
                                const woort_value component_st = stack_base + 2 + cmpid;

                                switch (work.m_dependence.m_requirements[cmpid].m_require)
                                {
                                case requirement::type::CONTAINS:
                                    create_component_struct(component_st, m_work_function, component, typeinfo);
                                    break;
                                case requirement::type::MAYNOT:
                                    if (component == nullptr)
                                        woort_set_option_none(component_st);
                                    else
                                    {
                                        create_component_struct(component_st, m_work_function, component, typeinfo);
                                        woort_set_option_value(component_st, component_st);
                                    }
                                    break;
                                case requirement::type::ANYOF:
                                case requirement::type::EXCEPT:
                                default:
                                    break;
                                }
                            }

                            woort_set_gchandle(
                                stack_base + 1,
                                new game_entity{ cur_chunk, eid, version },
                                WOORT_IGNORE,
                                [](void* eptr)
                                {
                                    delete static_cast<game_entity*>(eptr);
                                },
                                nullptr);

                            *woort_internal_value(m_work_function) = work.m_function;
                            if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, m_work_function))
                            {
                                aborted = true;
                                return;
                            }
                        }
                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }

                    woort_pop(used_component_count + 2);
                }
            }

            void update_step_work(std::vector<towoo_step_work>& works)
            {
                script::current_script_game_system_instance = this;

                if (m_job_vm == nullptr)
                    return;

                bool aborted = false;
                woort_vm* const last = woort_vm_swap(m_job_vm);
                {
                    for (auto& work : works)
                    {
                        if (work.m_is_single_work)
                            _invoke_single_work(work, aborted);
                        else
                            _invoke_multi_work(work, aborted);

                        if (aborted)
                            break;
                    }
                }
                (void)woort_vm_swap(last);

                if (aborted)
                    _drop();

                script::current_script_game_system_instance = nullptr;
            }

            woort_callstatus _invoke_lifecycle(
                const std::optional<woort_value>& func, const char* name)
            {
                if (m_job_vm == nullptr || !func.has_value())
                    return WOORT_VM_CALL_STATUS_NORMAL;

                woort_callstatus result;
                woort_vm* const last = woort_vm_swap(m_job_vm);
                {
                    result = woort_invoke(WOORT_IGNORE, func.value());
                }
                (void)woort_vm_swap(last);

                if (result != WOORT_VM_CALL_STATUS_NORMAL)
                {
                    jeecs::debug::logerr("Failed to invoke '%s' for system: '%s'.",
                        name, m_type->m_typename);
                    _drop();
                }

                return result;
            }

        public:
            void OnEnable()
            {
                _invoke_lifecycle(m_on_enable_function, "on_enable");
            }
            void OnDisable()
            {
                _invoke_lifecycle(m_on_disable_function, "on_disable");
            }
            void PreUpdate()
            {
                update_step_work(m_pre_dependences);
            }
            void Update()
            {
                update_step_work(m_dependences);
            }
            void LateUpdate()
            {
                update_step_work(m_late_dependences);
            }
        };

        struct ToWooBaseComponent
        {
            const jeecs::typing::type_info* m_type;

            ToWooBaseComponent(void* arg, const jeecs::typing::type_info* ty)
                : m_type(ty)
            {
                if (m_type->m_member_types == nullptr)
                    return;

                auto* member = m_type->m_member_types->m_members;
                while (member != nullptr)
                {
                    auto* this_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(this) + member->m_member_offset);
                    member->m_member_type->construct(this_member, arg);

                    if (member->m_woovalue_init_may_null != nullptr)
                    {
                        auto* val = std::launder(
                            reinterpret_cast<script::woovalue*>(this_member));

                        woort_Value tmp;
                        const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                        {
                            woort_GCPin_get_internal_without_barrier(
                                &tmp, member->m_woovalue_init_may_null, 0);

                            woort_GCPin_set_dup_boxed_internal(
                                val->m_pin_value, 0, &tmp);
                        }
                        if (entry_tmp_gc_guard)
                            woort_GC_sync_marking_unlock();
                    }

                    member = member->m_next_member;
                }
            }

            ~ToWooBaseComponent()
            {
                if (m_type->m_member_types == nullptr)
                    return;

                auto* member = m_type->m_member_types->m_members;
                while (member != nullptr)
                {
                    auto* this_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(this) + member->m_member_offset);
                    member->m_member_type->destruct(this_member);
                    member = member->m_next_member;
                }
            }

            ToWooBaseComponent(const ToWooBaseComponent& another)
                : m_type(another.m_type)
            {
                if (m_type->m_member_types == nullptr)
                    return;

                auto* member = m_type->m_member_types->m_members;
                while (member != nullptr)
                {
                    auto* this_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(this) + member->m_member_offset);
                    auto* other_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(&another) + member->m_member_offset);

                    member->m_member_type->copy(this_member, other_member);
                    member = member->m_next_member;
                }
            }

            ToWooBaseComponent(ToWooBaseComponent&& another)
                : m_type(another.m_type)
            {
                if (m_type->m_member_types == nullptr)
                    return;

                auto* member = m_type->m_member_types->m_members;
                while (member != nullptr)
                {
                    auto* this_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(this) + member->m_member_offset);
                    auto* other_member = reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(&another) + member->m_member_offset);

                    member->m_member_type->move(this_member, other_member);
                    member = member->m_next_member;
                }
            }
        };
    }
}

// ==========================================================================
// File-scope helpers and WOORT_API / free functions (global scope)
// ==========================================================================

void je_towoo_unregister_system(const jeecs::typing::type_info* tinfo);

namespace
{
    //
    // Helpers for je_towoo_update_api
    //

    std::vector<const jeecs::typing::type_info*> _gather_all_registed_types()
    {
        auto** alltypes = jedbg_get_all_registed_types();
        std::vector<const jeecs::typing::type_info*> all_registed_types;
        for (auto* idx = alltypes; *idx != nullptr; ++idx)
            all_registed_types.push_back(*idx);
        je_mem_free(alltypes);
        return all_registed_types;
    }

    std::string _generate_type_decl(
        const std::vector<const jeecs::typing::type_info*>& all_registed_types)
    {
        std::string type_decl =
            R"(// This file is auto-generated by JoyEngineECS.
// Do not edit this file manually.
import woo::std;
import je;
)";

        std::unordered_set<std::string> generated_types;

        for (auto* typeinfo : all_registed_types)
        {
            if (typeinfo == nullptr)
                continue;

            auto* script_parser_info = typeinfo->get_script_parser();
            if (script_parser_info == nullptr
                || false == generated_types.insert(script_parser_info->m_woolang_typename).second)
                continue;

            type_decl +=
                std::string("// Declear of '") + script_parser_info->m_woolang_typename + "'\n"
                + script_parser_info->m_woolang_typedecl + "\n"
                "namespace "
                + script_parser_info->m_woolang_typename + "\n{\n"
                "    using type = void\n    {\n"
                "        public let typeinfo = je::typeinfo::load(\""
                + script_parser_info->m_woolang_typename + "\")->unwrap;\n"
                "    }\n}\n\n";
        }

        return type_decl;
    }

    std::string _generate_component_decl(
        const std::vector<const jeecs::typing::type_info*>& all_registed_types)
    {
        std::string component_decl =
            R"(// This file is auto-generated by JoyEngineECS.
// Do not edit this file manually.
import woo::std;

import je;
import je::towoo;
import je::towoo::types;
)";

        for (auto* typeinfo : all_registed_types)
        {
            if (typeinfo == nullptr)
                continue;

            if (typeinfo->m_type_class != je_typing_class::JE_COMPONENT)
                continue;

            std::string tname = typeinfo->m_typename;
            if (tname.empty())
            {
                jeecs::debug::logerr("Component type %p have an empty name, pleace check.", typeinfo);
                continue;
            }

            const size_t index = tname.find_last_of(':');
            std::optional<std::string> tnamespace = std::nullopt;
            if (index < tname.size() - 1 && index >= 1)
            {
                tnamespace = std::optional(tname.substr(0, index - 1));
                tname = tname.substr(index + 1);
            }

            component_decl += std::string("// Declear of '") + typeinfo->m_typename + "'\n";
            if (tnamespace)
                component_decl += "namespace " + tnamespace.value() + "{\n";

            component_decl += "using " + tname + " = struct{\n    public __addr: handle,\n";

            if (typeinfo->m_member_types != nullptr)
            {
                auto* registed_member = typeinfo->m_member_types->m_members;
                while (registed_member != nullptr)
                {
                    auto* parser = registed_member->m_member_type->get_script_parser();
                    if (parser != nullptr)
                    {
                        const char* real_type_name =
                            registed_member->m_woovalue_type_may_null == nullptr
                            ? parser->m_woolang_typename
                            : registed_member->m_woovalue_type_may_null;

                        component_decl +=
                            std::string("    public ") + registed_member->m_member_name
                            + ": je::towoo::member<" + real_type_name + ", "
                            + parser->m_woolang_typename + "::type" + ">,\n";
                    }
                    registed_member = registed_member->m_next_member;
                }
            }
            component_decl += "}\n{\n";

            component_decl += std::string(
                "    using type = void\n"
                "    {\n"
                "        public let typeinfo = je::typeinfo::load(\"")
                + typeinfo->m_typename + "\")->unwrap;\n"
                "    }\n";

            component_decl += "}\n";

            if (tnamespace)
                component_decl += "}\n";
            component_decl += "\n";
        }

        return component_decl;
    }

    //
    // Helpers for je_towoo_register_system
    //

    bool _load_system_functions(
        woort_CodeEnv* cenv,
        woort_value initfunc,
        woort_value create_function,
        woort_value close_function,
        woort_value on_enable_function,
        woort_value on_disable_function,
        const char* system_name,
        const char* script_path)
    {
        if (!woort_load_extern_const(initfunc, cenv, "_init_towoo_system"))
        {
            jeecs::debug::logerr(
                "Failed to register: '%s' cannot find '_init_towoo_system' in '%s', "
                "forget to import je/towoo/system.wo ?",
                system_name, script_path);
            return false;
        }
        if (!woort_load_extern_const(create_function, cenv, "create"))
        {
            jeecs::debug::logerr(
                "Failed to register: '%s' cannot find 'create' function in '%s'.",
                system_name, script_path);
            return false;
        }
        if (!woort_load_extern_const(close_function, cenv, "close"))
        {
            jeecs::debug::logerr(
                "Failed to register: '%s' cannot find 'close' in '%s'.",
                system_name, script_path);
            return false;
        }
        return true;
    }

    bool _boot_and_extract_functions(
        woort_vm* vmm,
        woort_CodeEnv* cenv,
        woort_value create_function,
        woort_value close_function,
        woort_value on_enable_function,
        woort_value on_disable_function,
        jeecs::towoo::ToWooBaseSystem::towoo_system_info* sysinfo_ptr,
        const char* system_name)
    {
        if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup_codeenv(WOORT_IGNORE, cenv))
        {
            jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                system_name, woort_vm_get_runtime_error(vmm));
            return false;
        }

        sysinfo_ptr->m_create_function = *woort_internal_value(create_function);
        sysinfo_ptr->m_close_function = *woort_internal_value(close_function);

        if (woort_load_extern_const(on_enable_function, cenv, "on_enable"))
            sysinfo_ptr->m_on_enable_function.emplace() = *woort_internal_value(on_enable_function);

        if (woort_load_extern_const(on_disable_function, cenv, "on_disable"))
            sysinfo_ptr->m_on_disable_function.emplace() = *woort_internal_value(on_disable_function);

        return true;
    }

    const jeecs::typing::type_info* _register_system_type(const char* system_name)
    {
        je_towoo_unregister_system(je_typing_get_info_by_name(system_name));

        const jeecs::typing::type_info* created_system_type_info = je_typing_register(
            system_name,
            jeecs::basic::hash_compile_time(system_name),
            sizeof(jeecs::towoo::ToWooBaseSystem),
            alignof(jeecs::towoo::ToWooBaseSystem),
            je_typing_class::JE_SYSTEM,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::constructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::destructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::copier,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::mover);

        je_register_system_updater(
            created_system_type_info,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::on_enable,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::on_disable,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::pre_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::state_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::physics_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::transform_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::late_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::commit_update,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseSystem>::graphic_update);

        return created_system_type_info;
    }

    void _init_system_with_vm(
        woort_vm* vmm,
        const jeecs::typing::type_info* created_system_type_info,
        jeecs::towoo::ToWooBaseSystem::towoo_system_info* sysinfo_ptr,
        woort_value initfunc,
        woort_value stack_base,
        const char* system_name)
    {
        woort_set_pointer(stack_base + 0,
            const_cast<jeecs::typing::type_info*>(created_system_type_info));

        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, initfunc))
        {
            jeecs::debug::logerr(
                "Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                system_name, woort_vm_get_runtime_error(vmm));
        }
        else
        {
            sysinfo_ptr->m_is_good = true;
        }
    }

    //
    // Helpers for wojeapi_towoo_update_component_data
    //

    struct _wooval_type
    {
        std::string m_wooval_type;
        woort_Value m_wooval_val;
    };
    struct _member_info
    {
        std::string m_name;
        std::optional<_wooval_type> m_wooval_type;
        const jeecs::typing::type_info* m_type;
        size_t m_offset;
    };

    std::pair<std::vector<_member_info>, std::pair<size_t, size_t>>
        _parse_member_defs(woort_value members)
    {
        const size_t member_count = woort_vec_len(members);

        size_t component_size = sizeof(jeecs::towoo::ToWooBaseComponent);
        size_t component_align = alignof(jeecs::towoo::ToWooBaseComponent);
        std::vector<_member_info> member_defs;

        woort_value stack_base;
        if (!woort_push_reserve(3, &stack_base))
        {
            woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow.");
            return { {}, { component_size, component_align } };
        }

        const woort_value member_def = stack_base + 0;
        const woort_value member_info = stack_base + 1;
        const woort_value wooval_init = stack_base + 2;

        for (size_t i = 0; i < member_count; ++i)
        {
            (void)woort_vec_get(member_def, members, i);

            woort_struct_get(member_info, member_def, 0);
            const std::string member_name = woort_string(member_info);

            woort_struct_get(member_info, member_def, 1);
            auto* member_typeinfo =
                static_cast<const jeecs::typing::type_info*>(woort_pointer(member_info));

            std::optional<_wooval_type> member_wooval_type = std::nullopt;
            woort_struct_get(member_info, member_def, 2);
            if (woort_option_get(member_info, member_info))
            {
                woort_struct_get(member_info, member_info, 0);
                woort_struct_get(wooval_init, member_info, 1);

                _wooval_type wt{ woort_string(member_info) };
                wt.m_wooval_val = *woort_internal_value(wooval_init);

                member_wooval_type = std::optional(wt);
            }

            component_size = jeecs::basic::allign_size(component_size, member_typeinfo->m_align);
            member_defs.push_back(_member_info{
                member_name, member_wooval_type, member_typeinfo, component_size });

            component_size += member_typeinfo->m_chunk_size;
            component_align = std::max(component_align, member_typeinfo->m_align);
        }

        return { member_defs, { component_size, component_align } };
    }

    void _register_component_members(
        const jeecs::typing::type_info* towoo_component_tinfo,
        const std::vector<_member_info>& member_defs)
    {
        woort_value wooval_init;
        if (!woort_push_reserve(1, &wooval_init))
        {
            woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow.");
            return;
        }

        for (auto& memberinfo : member_defs)
        {
            const _wooval_type* wooval = memberinfo.m_wooval_type.has_value()
                ? &memberinfo.m_wooval_type.value()
                : nullptr;

            if (wooval != nullptr)
                *woort_internal_value(wooval_init) = wooval->m_wooval_val;

            je_register_member(
                towoo_component_tinfo,
                memberinfo.m_type,
                memberinfo.m_name.c_str(),
                wooval != nullptr ? wooval->m_wooval_type.c_str() : nullptr,
                wooval != nullptr ? wooval_init : WOORT_IGNORE,
                memberinfo.m_offset);
        }
    }

}

// ==========================================================================
// WOORT_API wrappers for component operations
// ==========================================================================

WOORT_API woort_api wojeapi_towoo_add_component(void)
{
    auto* e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto* ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    void* comp = je_ecs_world_entity_add_component(e, ty->m_id);
    if (comp != nullptr)
    {
        woort_value stack_base;
        if (!woort_push_reserve(1, &stack_base))
            return woort_ret_panic("Stack overflow.");

        jeecs::towoo::ToWooBaseSystem::create_component_struct(
            WOORT_RETURN_SLOT, stack_base, comp, ty);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_get_component(void)
{
    auto* e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto* ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    void* comp = je_ecs_world_entity_get_component(e, ty->m_id);
    if (comp != nullptr)
    {
        woort_value stack_base;
        if (!woort_push_reserve(1, &stack_base))
            return woort_ret_panic("Stack overflow.");

        jeecs::towoo::ToWooBaseSystem::create_component_struct(
            WOORT_RETURN_SLOT, stack_base, comp, ty);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_remove_component(void)
{
    auto* e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto* ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    je_ecs_world_entity_remove_component(e, ty->m_id);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_towoo_member_get(void)
{
    auto* ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_c2w(woort_pointer(1), WOORT_RETURN_SLOT);

    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_member_set(void)
{
    auto* ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_w2c(woort_pointer(1), 2);

    return woort_ret_void();
}

// ==========================================================================
// System registration / lifecycle
// ==========================================================================

void je_towoo_update_api()
{
    // ATTENTION: woolang_parsing_type_decl cannot import je/towoo/components in any way,
    // because towoo components depend on this file being loaded, and during packaging,
    // components are already loaded; this would cause circular dependency, where the
    // typeinfo lookup fails because the dependency isn't yet loaded.
    const auto all_registed_types = _gather_all_registed_types();

    const std::string woolang_parsing_type_decl = _generate_type_decl(all_registed_types);
    const std::string woolang_component_type_decl = _generate_component_decl(all_registed_types);

    if (!woort_vfs_create(
        "je/towoo/types.wo",
        woolang_parsing_type_decl.c_str(),
        woolang_parsing_type_decl.size(),
        true))
    {
        jeecs::debug::logfatal("Unable to regenerate 'je/towoo/types.wo' please check.");
    }
    if (!woort_vfs_create(
        "je/towoo/components.wo",
        woolang_component_type_decl.c_str(),
        woolang_component_type_decl.size(),
        true))
    {
        jeecs::debug::logfatal("Unable to regenerate 'je/towoo/components.wo' please check.");
    }
}

void je_towoo_unregister_system(const jeecs::typing::type_info* tinfo)
{
    if (tinfo == nullptr)
        return;

    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);
    auto registered_system_fnd =
        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(tinfo);
    if (registered_system_fnd
        == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end())
    {
        jeecs::debug::logerr(
            "There is no towoo-system of type '%p', failed to unregister.", tinfo);
    }
    else
    {
        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.erase(
            registered_system_fnd);
        je_typing_unregister(tinfo);
    }
}

const jeecs::typing::type_info* je_towoo_register_system(
    const char* system_name,
    const char* script_path)
{
    const jeecs::typing::type_info* created_system_type_info = nullptr;

    jeecs_file* texfile = jeecs_file_open(script_path);
    if (texfile == nullptr)
    {
        jeecs::debug::logerr("Failed to register: '%s' unable to open file '%s'.",
            system_name, script_path);
        return nullptr;
    }

    char* src = static_cast<char*>(malloc(texfile->m_file_length + 1));
    jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
    src[texfile->m_file_length] = 0;

    wo_CompileErrors* cerror;
    woort_CodeEnv* cenv = wo_load_binary(script_path, src, texfile->m_file_length, &cerror);

    jeecs_file_close(texfile);
    free(src);

    if (cenv == nullptr)
    {
        jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
            system_name, wo_get_compile_error(cerror, WO_PLAIM));
        wo_compile_errors_free(cerror);
        return nullptr;
    }

    auto systinfo =
        std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(cenv);
    systinfo->m_on_enable_function = std::nullopt;
    systinfo->m_on_disable_function = std::nullopt;
    systinfo->m_is_good = false;

    auto* sysinfo_ptr = systinfo.get();

    woort_vm* const vmm = woort_vm_create();
    if (vmm == nullptr)
    {
        jeecs::debug::logerr("Failed to register: '%s': Create VM failed.", system_name);
        return nullptr;
    }

    woort_vm* const last = woort_vm_swap(vmm);
    {
        woort_value stack_base;
        if (!woort_push_reserve(6, &stack_base))
        {
            jeecs::debug::logerr("Failed to register: '%s': Stack overflow.", system_name);
        }
        else
        {
            const woort_value initfunc = stack_base + 1;
            const woort_value create_function = stack_base + 2;
            const woort_value close_function = stack_base + 3;
            const woort_value on_enable_function = stack_base + 4;
            const woort_value on_disable_function = stack_base + 5;

            if (_load_system_functions(cenv, initfunc, create_function, close_function,
                on_enable_function, on_disable_function, system_name, script_path)
                && _boot_and_extract_functions(vmm, cenv, create_function, close_function,
                    on_enable_function, on_disable_function, sysinfo_ptr, system_name))
            {
                created_system_type_info = _register_system_type(system_name);

                assert(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(
                    created_system_type_info)
                    == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end());

                {
                    std::lock_guard ug1(
                        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);
                    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems
                        [created_system_type_info] = std::move(systinfo);
                }

                _init_system_with_vm(vmm, created_system_type_info,
                    sysinfo_ptr, initfunc, stack_base, system_name);
            }
            woort_pop(6);
        }
    }
    (void)woort_vm_swap(last);
    woort_vm_close(vmm);

    return created_system_type_info;
}

void jetowoo_finish()
{
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);
    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.clear();
}

// ==========================================================================
// System job registration
// ==========================================================================

enum _jetowoo_job_type
{
    PRE_UPDATE,
    UPDATE,
    LATE_UPDATE,
};

WOORT_API woort_api wojeapi_towoo_register_system_job(void)
{
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

    auto* tinfo = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));
    auto registered_system_fnd =
        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(tinfo);
    if (registered_system_fnd
        == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end())
    {
        jeecs::debug::logerr("The towoo-system type: '%p' has not been registered.", tinfo);
        return woort_ret_void();
    }

    auto& works = registered_system_fnd->second;

    jeecs::towoo::ToWooBaseSystem::towoo_step_work stepwork;
    stepwork.m_function = *woort_internal_value(1);

    const _jetowoo_job_type que = static_cast<_jetowoo_job_type>(woort_int(2));
    const woort_value requirements = 3;
    const size_t component_arg_count = static_cast<size_t>(woort_int(4));
    const bool is_single_work = woort_bool(5);

    woort_value stack_base;
    if (!woort_push_reserve(2, &stack_base))
        return woort_ret_panic("Stack overflow.");

    const woort_value requirement_info = stack_base + 0;
    const woort_value elem = stack_base + 1;

    stepwork.m_is_single_work = is_single_work;

    if (!stepwork.m_is_single_work)
    {
        const size_t requirements_count = woort_vec_len(requirements);
        for (size_t i = 0; i < requirements_count; ++i)
        {
            (void)woort_vec_get(requirement_info, requirements, i);

            woort_struct_get(elem, requirement_info, 2);
            const auto* typeinfo =
                static_cast<const jeecs::typing::type_info*>(woort_pointer(elem));

            woort_struct_get(elem, requirement_info, 0);
            const jeecs::requirement::type ty =
                static_cast<jeecs::requirement::type>(woort_int(elem));

            woort_struct_get(elem, requirement_info, 1);

            stepwork.m_dependence.m_requirements.push_back(
                jeecs::requirement{
                    ty,
                    static_cast<size_t>(woort_int(elem)),
                    typeinfo->m_id
                });

            if (i < component_arg_count)
                stepwork.m_used_components.push_back(typeinfo);
        }
    }

    switch (que)
    {
    case _jetowoo_job_type::PRE_UPDATE:
        works->m_preworks.push_back(stepwork);
        break;
    case _jetowoo_job_type::UPDATE:
        works->m_works.push_back(stepwork);
        break;
    case _jetowoo_job_type::LATE_UPDATE:
        works->m_lateworks.push_back(stepwork);
        break;
    }

    return woort_ret_void();
}

// ==========================================================================
// Dynamic component type registration
// ==========================================================================

WOORT_API woort_api wojeapi_towoo_update_component_data(void)
{
    const std::string component_name = woort_string(0);

    auto* towoo_component_tinfo = je_typing_get_info_by_name(component_name.c_str());
    if (towoo_component_tinfo != nullptr)
    {
        if (towoo_component_tinfo->m_hash
            != jeecs::basic::hash_compile_time(
                ("_towoo_component_" + component_name).c_str()))
            return woort_ret_panic(
                "Invalid towoo component name, cannot same as native-components.");
    }

    const woort_value members = 1;

    auto [member_defs, size_align] = _parse_member_defs(members);
    const size_t component_size = size_align.first;
    const size_t component_align = size_align.second;

    if (towoo_component_tinfo == nullptr)
    {
        towoo_component_tinfo = je_typing_register(
            component_name.c_str(),
            jeecs::basic::hash_compile_time(
                ("_towoo_component_" + component_name).c_str()),
            component_size,
            component_align,
            je_typing_class::JE_COMPONENT,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);
    }
    else
    {
        je_typing_reset(
            towoo_component_tinfo,
            component_size,
            component_align,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);
    }

    _register_component_members(towoo_component_tinfo, member_defs);

    return woort_ret_pointer(
        const_cast<jeecs::typing::type_info*>(towoo_component_tinfo));
}

// ==========================================================================
// Math conversion functions
// ==========================================================================

jeecs::math::vec2 wo_vec2(woort_value val)
{
    jeecs::math::vec2 result;
    result.x = woort_struct_get_float(val, 0);
    result.y = woort_struct_get_float(val, 1);
    return result;
}
jeecs::math::vec3 wo_vec3(woort_value val)
{
    jeecs::math::vec3 result;
    result.x = woort_struct_get_float(val, 0);
    result.y = woort_struct_get_float(val, 1);
    result.z = woort_struct_get_float(val, 2);
    return result;
}
jeecs::math::vec4 wo_vec4(woort_value val)
{
    jeecs::math::vec4 result;
    result.x = woort_struct_get_float(val, 0);
    result.y = woort_struct_get_float(val, 1);
    result.z = woort_struct_get_float(val, 2);
    result.w = woort_struct_get_float(val, 3);
    return result;
}
jeecs::math::quat wo_quat(woort_value val)
{
    jeecs::math::quat result;
    result.x = woort_struct_get_float(val, 0);
    result.y = woort_struct_get_float(val, 1);
    result.z = woort_struct_get_float(val, 2);
    result.w = woort_struct_get_float(val, 3);
    return result;
}

void wo_set_vec2(woort_value target, const jeecs::math::vec2& v)
{
    woort_set_struct(target, 2);
    woort_struct_set_float(target, 0, v.x);
    woort_struct_set_float(target, 1, v.y);
}
void wo_set_vec3(woort_value target, const jeecs::math::vec3& v)
{
    woort_set_struct(target, 3);
    woort_struct_set_float(target, 0, v.x);
    woort_struct_set_float(target, 1, v.y);
    woort_struct_set_float(target, 2, v.z);
}
void wo_set_vec4(woort_value target, const jeecs::math::vec4& v)
{
    woort_set_struct(target, 4);
    woort_struct_set_float(target, 0, v.x);
    woort_struct_set_float(target, 1, v.y);
    woort_struct_set_float(target, 2, v.z);
    woort_struct_set_float(target, 3, v.w);
}
void wo_set_quat(woort_value target, const jeecs::math::quat& v)
{
    woort_set_struct(target, 4);
    woort_struct_set_float(target, 0, v.x);
    woort_struct_set_float(target, 1, v.y);
    woort_struct_set_float(target, 2, v.z);
    woort_struct_set_float(target, 3, v.w);
}

template <typename T>
T& wo_component(woort_value val, woort_value tmp)
{
    woort_struct_get(tmp, val, 0);
    return *static_cast<T*>(woort_pointer(tmp));
}

template <typename T>
T* wo_option_component(woort_value val, woort_value tmp)
{
    if (woort_option_get(tmp, val))
    {
        woort_struct_get(tmp, tmp, 0);
        return static_cast<T*>(woort_pointer(tmp));
    }
    return nullptr;
}

// ==========================================================================
// Native API wrappers
// ==========================================================================

WOORT_API woort_api wojeapi_towoo_ray_create(void)
{
    return woort_ret_gchandle(
        new jeecs::math::ray(wo_vec3(0), wo_vec3(1)),
        WOORT_IGNORE,
        [](void* p) { delete static_cast<jeecs::math::ray*>(p); },
        nullptr);
}
WOORT_API woort_api wojeapi_towoo_ray_from_camera(void)
{
    return woort_ret_gchandle(
        new jeecs::math::ray(
            wo_component<jeecs::Transform::Translation>(0, WOORT_RETURN_SLOT),
            wo_component<jeecs::Camera::Projection>(1, WOORT_RETURN_SLOT),
            wo_vec2(2),
            woort_bool(3)),
        WOORT_IGNORE,
        [](void* p) { delete static_cast<jeecs::math::ray*>(p); },
        nullptr);
}
WOORT_API woort_api wojeapi_towoo_ray_intersect_entity(void)
{
    auto* ray = static_cast<jeecs::math::ray*>(woort_gcpointer(0));
    auto result = ray->intersect_entity(
        wo_component<jeecs::Transform::Translation>(1, WOORT_RETURN_SLOT),
        wo_option_component<jeecs::Renderer::Shape>(2, WOORT_RETURN_SLOT),
        woort_bool(3));

    if (result.intersected)
    {
        wo_set_vec3(WOORT_RETURN_SLOT, result.place);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_ray_origin(void)
{
    auto* ray = static_cast<jeecs::math::ray*>(woort_gcpointer(0));
    wo_set_vec3(WOORT_RETURN_SLOT, ray->orgin);
    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_ray_direction(void)
{
    auto* ray = static_cast<jeecs::math::ray*>(woort_gcpointer(0));
    wo_set_vec3(WOORT_RETURN_SLOT, ray->direction);
    return woort_ret();
}

WOORT_API woort_api wojeapi_towoo_math_sqrt(void)
{
    return woort_ret_real(sqrt(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_sin(void)
{
    return woort_ret_real(sin(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_cos(void)
{
    return woort_ret_real(cos(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_tan(void)
{
    return woort_ret_real(tan(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_asin(void)
{
    return woort_ret_real(asin(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_acos(void)
{
    return woort_ret_real(acos(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_atan(void)
{
    return woort_ret_real(atan(woort_real(0)));
}
WOORT_API woort_api wojeapi_towoo_math_atan2(void)
{
    return woort_ret_real(atan2(woort_real(0), woort_real(1)));
}
WOORT_API woort_api wojeapi_towoo_math_quat_slerp(void)
{
    wo_set_quat(
        WOORT_RETURN_SLOT,
        jeecs::math::quat::slerp(
            wo_quat(0),
            wo_quat(1),
            woort_float(2)));
    return woort_ret();
}

WOORT_API woort_api wojeapi_towoo_physics2d_collisionresult_all(void)
{
    woort_value stack_base;
    if (!woort_push_reserve(3, &stack_base))
        return woort_ret_panic("Stack overflow.");

    auto& collisionResult =
        wo_component<jeecs::Physics2D::CollisionResult>(0, WOORT_RETURN_SLOT);

    const woort_value result_container = stack_base + 0;
    const woort_value elem = stack_base + 1;
    const woort_value val = stack_base + 2;

    woort_set_map(result_container);
    woort_map_reserve(result_container, collisionResult.results.size());

    for (auto& [rigidbody, result] : collisionResult.results)
    {
        woort_set_struct(val, 2);
        (void)woort_map_set_by_pointer(result_container, rigidbody, val);

        wo_set_vec2(elem, result.position);
        woort_struct_set(val, 0, elem);
        wo_set_vec2(elem, result.normalize);
        woort_struct_set(val, 1, elem);
    }

    return woort_ret_value(result_container);
}

WOORT_API woort_api wojeapi_towoo_physics2d_collisionresult_check(void)
{
    woort_value stack_base;
    if (!woort_push_reserve(2, &stack_base))
        return woort_ret_panic("Stack overflow.");

    auto& collisionResult =
        wo_component<jeecs::Physics2D::CollisionResult>(0, WOORT_RETURN_SLOT);
    auto& rigidbody =
        wo_component<jeecs::Physics2D::Rigidbody>(1, WOORT_RETURN_SLOT);

    auto* result = collisionResult.check(&rigidbody);
    if (result != nullptr)
    {
        woort_value ret = stack_base + 0;

        woort_set_struct(ret, 2);

        wo_set_vec2(stack_base + 1, result->position);
        woort_struct_set(ret, 0, stack_base + 1);

        wo_set_vec2(stack_base + 1, result->normalize);
        woort_struct_set(ret, 1, stack_base + 1);

        return woort_ret_option_value(ret);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_towoo_renderer_textures_bind_texture(void)
{
    auto& textures =
        wo_component<jeecs::Renderer::Textures>(0, WOORT_RETURN_SLOT);

    const size_t pass = static_cast<size_t>(woort_int(1));
    auto* tex =
        static_cast<jeecs::basic::resource<jeecs::graphic::texture>*>(woort_gcpointer(2));

    textures.bind_texture(pass, *tex);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_textures_get_texture(void)
{
    auto& textures =
        wo_component<jeecs::Renderer::Textures>(0, WOORT_RETURN_SLOT);
    const size_t pass = static_cast<size_t>(woort_int(1));

    auto tex = textures.get_texture(pass);

    if (tex.has_value())
    {
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::texture>(tex.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete static_cast<jeecs::basic::resource<jeecs::graphic::texture>*>(p);
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_i(void)
{
    auto& shaders =
        wo_component<jeecs::Renderer::Shaders>(0, WOORT_RETURN_SLOT);

    const char* name = woort_string(1);
    shaders.set_uniform(name, static_cast<int>(woort_int(2)));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_r(void)
{
    auto& shaders =
        wo_component<jeecs::Renderer::Shaders>(0, WOORT_RETURN_SLOT);

    const char* name = woort_string(1);
    shaders.set_uniform(name, woort_float(2));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_v(void)
{
    auto& shaders =
        wo_component<jeecs::Renderer::Shaders>(0, WOORT_RETURN_SLOT);

    const char* name = woort_string(1);
    const woort_value val = 2;

    switch (woort_struct_len(val))
    {
    case 2:
        shaders.set_uniform(name, wo_vec2(val));
        break;
    case 3:
        shaders.set_uniform(name, wo_vec3(val));
        break;
    case 4:
        shaders.set_uniform(name, wo_vec4(val));
        break;
    default:
        return woort_ret_panic("Unknown value type when set_uniform.");
    }
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_shaders(void)
{
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0, WOORT_RETURN_SLOT);

    shaders.shaders.clear();
    const size_t setting_shaders_len = woort_vec_len(1);
    for (size_t i = 0; i < setting_shaders_len; ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 1, i);
        shaders.shaders.push_back(
            *static_cast<jeecs::basic::resource<jeecs::graphic::shader>*>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_get_shaders(void)
{
    woort_value stack_base;
    if (!woort_push_reserve(2, &stack_base))
        return woort_ret_panic("Stack overflow.");

    auto& shaders =
        wo_component<jeecs::Renderer::Shaders>(0, WOORT_RETURN_SLOT);

    const woort_value result_container = stack_base + 0;
    const woort_value elem = stack_base + 1;

    woort_set_vec(result_container);

    for (auto& shad : shaders.shaders)
    {
        woort_set_gchandle(
            elem,
            new jeecs::basic::resource<jeecs::graphic::shader>(shad),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete std::launder(reinterpret_cast<
                    jeecs::basic::resource<jeecs::graphic::shader>*>(ptr));
            },
            nullptr);
        woort_vec_push(result_container, elem);
    }
    return woort_ret_value(result_container);
}

WOORT_API woort_api wojeapi_towoo_transform_translation_global_pos(void)
{
    auto& trans =
        wo_component<jeecs::Transform::Translation>(0, WOORT_RETURN_SLOT);

    wo_set_vec3(WOORT_RETURN_SLOT, trans.world_position);
    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_transform_translation_global_rot(void)
{
    auto& trans =
        wo_component<jeecs::Transform::Translation>(0, WOORT_RETURN_SLOT);

    wo_set_quat(WOORT_RETURN_SLOT, trans.world_rotation);
    return woort_ret();
}

WOORT_API woort_api wojeapi_towoo_transform_translation_parent_pos(void)
{
    auto& trans =
        wo_component<jeecs::Transform::Translation>(0, WOORT_RETURN_SLOT);
    wo_set_vec3(
        WOORT_RETURN_SLOT,
        trans.get_parent_position(
            wo_option_component<jeecs::Transform::LocalPosition>(1, WOORT_RETURN_SLOT),
            wo_option_component<jeecs::Transform::LocalRotation>(2, WOORT_RETURN_SLOT)));
    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_transform_translation_parent_rot(void)
{
    auto& trans =
        wo_component<jeecs::Transform::Translation>(0, WOORT_RETURN_SLOT);

    wo_set_quat(WOORT_RETURN_SLOT, trans.get_parent_rotation(
        wo_option_component<jeecs::Transform::LocalRotation>(1, WOORT_RETURN_SLOT)));
    return woort_ret();
}

WOORT_API woort_api wojeapi_towoo_transform_translation_set_global_pos(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(
        0, WOORT_RETURN_SLOT);
    auto pos = wo_vec3(1);
    auto* lpos = wo_option_component<jeecs::Transform::LocalPosition>(
        2, WOORT_RETURN_SLOT);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(
        3, WOORT_RETURN_SLOT);

    trans.set_global_position(pos, lpos, lrot);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_towoo_transform_translation_set_global_rot(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(
        0, WOORT_RETURN_SLOT);
    auto rot = wo_quat(1);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(
        2, WOORT_RETURN_SLOT);

    trans.set_global_rotation(rot, lrot);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_active_animation(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0, WOORT_RETURN_SLOT);
    anim.animations.active_action(
        static_cast<size_t>(woort_int(1)), woort_string(2), woort_bool(3));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_stop_animation(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0, WOORT_RETURN_SLOT);
    anim.animations.stop_action(static_cast<size_t>(woort_int(1)));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_is_playing(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0, WOORT_RETURN_SLOT);

    return woort_ret_bool(anim.animations.is_playing(static_cast<size_t>(woort_int(1))));
}

WOORT_API woort_api wojeapi_towoo_audio_playing_set_buffer(void)
{
    auto& playing = wo_component<jeecs::Audio::Playing>(0, WOORT_RETURN_SLOT);

    auto* buf =
        static_cast<jeecs::basic::resource<jeecs::audio::buffer>*>(woort_gcpointer(1));
    playing.set_buffer(*buf);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_audio_source_get_source(void)
{
    auto& source = wo_component<jeecs::Audio::Source>(0, WOORT_RETURN_SLOT);
    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::audio::source>(source.source),
        WOORT_IGNORE,
        [](void* p)
        { delete static_cast<jeecs::basic::resource<jeecs::audio::source>*>(p); },
        nullptr);
}

WOORT_API woort_api wojeapi_towoo_userinterface_origin_layout(void)
{
    woort_value stack_base;
    if (!woort_push_reserve(2, &stack_base))
        return woort_ret_panic("Stack overflow.");

    auto& origin = wo_component<jeecs::UserInterface::Origin>(0, WOORT_RETURN_SLOT);

    auto r = wo_vec2(1);

    jeecs::math::vec2 abssize;
    jeecs::math::vec2 absoffset;
    jeecs::math::vec2 center_offset;

    origin.get_layout(r.x, r.y, &absoffset, &abssize, &center_offset);

    woort_set_struct(stack_base + 0, 3);

    wo_set_vec2(stack_base + 1, absoffset);
    woort_struct_set(stack_base + 0, 0, stack_base + 1);

    wo_set_vec2(stack_base + 1, abssize);
    woort_struct_set(stack_base + 0, 1, stack_base + 1);

    wo_set_vec2(stack_base + 1, center_offset);
    woort_struct_set(stack_base + 0, 2, stack_base + 1);

    return woort_ret_value(stack_base + 0);
}

WOORT_API woort_api wojeapi_towoo_userinterface_origin_mouse_on(void)
{
    auto& origin = wo_component<jeecs::UserInterface::Origin>(0, WOORT_RETURN_SLOT);

    auto r = wo_vec2(1);
    auto a = woort_float(2);
    auto m = wo_vec2(3);

    return woort_ret_bool(origin.mouse_on(r.x, r.y, a, m));
}
