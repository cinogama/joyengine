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

                std::optional<woort_Value> m_on_active_function;
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

            // 执行代码使用的虚拟机
            woort_vm* m_job_vm;
            woort_value m_context;

            std::optional<woort_value> m_on_active_function;
            std::optional<woort_value> m_on_disable_function;
            woort_value m_create_function;
            woort_value m_close_function;
            woort_value m_work_function;

            const jeecs::typing::type_info* m_type;

            // 执行方法使用的需求器和对应的执行逻辑
            std::vector<towoo_step_work> m_pre_dependences;
            std::vector<towoo_step_work> m_dependences;
            std::vector<towoo_step_work> m_late_dependences;

            JECS_DISABLE_MOVE_AND_COPY(ToWooBaseSystem);

            ToWooBaseSystem(game_world w, const jeecs::typing::type_info* ty)
                : game_system(w)
            {
                std::shared_lock sg1(_registered_towoo_base_systems_mx);

                auto& base_info = _registered_towoo_base_systems.at(ty);
                m_type = ty;

                if (base_info->m_is_good)
                {
                    m_job_vm = woort_vm_create();

                    if (m_job_vm == nullptr)
                        jeecs::debug::logerr("Failed to create vm for system: '%s'.",
                            m_type->m_typename);
                    else
                    {
                        woort_vm* const last = woort_vm_swap(m_job_vm);
                        {
                            woort_value s;
                            if (!woort_push_reserve(6, &s))
                            {
                                jeecs::debug::logerr("Failed to reserve stack for system: '%s'.",
                                    m_type->m_typename);

                                woort_vm_close(m_job_vm);
                                m_job_vm = nullptr;
                            }
                            else
                            {
                                m_pre_dependences = base_info->m_preworks;
                                m_dependences = base_info->m_works;
                                m_late_dependences = base_info->m_lateworks;

                                m_context = s + 0;
                                woort_set_pointer(m_context, m_job_vm);

                                m_create_function = s + 1;
                                *woort_internal_value(m_create_function) = base_info->m_create_function;

                                m_close_function = s + 2;
                                *woort_internal_value(m_close_function) = base_info->m_close_function;

                                if (base_info->m_on_active_function.has_value())
                                {
                                    *woort_internal_value(s + 3) = base_info->m_on_active_function.value();
                                    m_on_active_function.emplace(s + 3);
                                }
                                if (base_info->m_on_disable_function.has_value())
                                {
                                    *woort_internal_value(s + 4) = base_info->m_on_active_function.value();
                                    m_on_disable_function.emplace(s + 4);
                                }

                                m_work_function = s + 5;

                                if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(m_context, m_create_function))
                                {
                                    jeecs::debug::logerr("Failed to invoke 'create' function for system: '%s'.",
                                        m_type->m_typename);

                                    woort_vm_close(m_job_vm);
                                    m_job_vm = nullptr;
                                }

                            }
                        }
                        woort_vm_swap(last);
                    }
                }
                else
                {
                    m_job_vm = nullptr;
                    jeecs::debug::logerr("System '%s' cannot create normally, please check the corresponding script for errors.",
                        m_type->m_typename);
                }
            }
            ~ToWooBaseSystem()
            {
                if (m_job_vm != nullptr)
                {
                    woort_vm* const last = woort_vm_swap(m_job_vm);
                    {
                        if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, m_close_function))
                        {
                            jeecs::debug::logerr("Failed to invoke 'close' function for system: '%s'.",
                                m_type->m_typename);
                        }
                    }
                    woort_vm_swap(last);
                    woort_vm_close(m_job_vm);
                }
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
                        // Set member;
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

            void update_step_work(std::vector<towoo_step_work>& works)
            {
                script::current_script_game_system_instance = this;

                if (m_job_vm == nullptr)
                    return;

                woort_vm* const last = woort_vm_swap(m_job_vm);
                {
                    for (auto& work : works)
                    {
                        if (work.m_is_single_work)
                        {
                            *woort_internal_value(m_work_function) = work.m_function;

                            // Invoke!
                            woort_invoke(WOORT_IGNORE, m_work_function);
                        }
                        else
                        {
                            work.m_dependence.update(get_world());
                            for (const auto& archinfo : work.m_dependence.m_archs)
                            {
                                auto cur_chunk = je_arch_get_chunk(archinfo.m_arch);
                                const size_t used_component_count = work.m_used_components.size();

                                woort_value s;
                                woort_push_reserve(used_component_count + 2, &s);

                                // Push context
                                woort_set_value(s + 0, m_context);

                                while (cur_chunk)
                                {
                                    auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                                    typing::version_t version;
                                    for (jeecs::typing::entity_id_in_chunk_t eid = 0; eid < archinfo.m_entity_count; ++eid)
                                    {
                                        if (jeecs::game_entity::entity_stat::READY == entity_meta_addr[eid].m_stat)
                                        {
                                            version = entity_meta_addr[eid].m_version;

                                            // game_entity{ cur_chunk, eid, version }
                                            // Valid! prepare to invoke!

                                            for (auto cmpidx = work.m_used_components.begin(); cmpidx != work.m_used_components.end(); ++cmpidx)
                                            {
                                                const size_t cmpid = cmpidx - work.m_used_components.begin();

                                                void* component = slice_requirement::base::view_base::get_component_from_archchunk_ptr(
                                                    &archinfo, cur_chunk, eid, cmpid);

                                                const auto* typeinfo = *cmpidx;

                                                const woort_value component_st = s + 2 + cmpid;
                                                switch (work.m_dependence.m_requirements[cmpid].m_require)
                                                {
                                                case jeecs::requirement::type::CONTAINS:
                                                {
                                                    create_component_struct(component_st, m_work_function, component, typeinfo);
                                                    break;
                                                }
                                                case jeecs::requirement::type::MAYNOT:
                                                {
                                                    if (component == nullptr)
                                                    {
                                                        // option::none
                                                        woort_set_option_none(component_st);
                                                    }
                                                    else
                                                    {
                                                        // option::value
                                                        create_component_struct(component_st, m_work_function, component, typeinfo);
                                                        woort_set_option_value(component_st, component_st);
                                                    }
                                                    break;
                                                }
                                                case jeecs::requirement::type::ANYOF:
                                                case jeecs::requirement::type::EXCEPT:
                                                default:
                                                    break;
                                                }
                                            }

                                            // Push entity
                                            woort_set_gchandle(
                                                s + 1,
                                                new jeecs::game_entity{
                                                    cur_chunk,
                                                    eid,
                                                    version
                                                },
                                                WOORT_IGNORE,
                                                [](void* eptr)
                                                {
                                                    delete static_cast<jeecs::game_entity*>(eptr);
                                                },
                                                nullptr);

                                            // Invoke!
                                            *woort_internal_value(m_work_function) = work.m_function;
                                            woort_invoke(WOORT_IGNORE, m_work_function);
                                        }
                                    }

                                    // Update next chunk.
                                    cur_chunk = je_arch_next_chunk(cur_chunk);
                                }

                                woort_pop(used_component_count + 2);
                            }
                        }
                    }
                }
                (void*)woort_vm_swap(last);

                script::current_script_game_system_instance = nullptr;
            }

            void OnEnable()
            {
                if (m_job_vm == nullptr)
                    return;

                if (m_on_active_function.has_value())
                {
                    woort_invoke(WOORT_IGNORE, m_on_active_function.value());
                }
            }
            void OnDisable()
            {
                if (m_job_vm == nullptr)
                    return;

                if (m_on_disable_function.has_value())
                {
                    woort_invoke(WOORT_IGNORE, m_on_disable_function.value());
                }
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
                if (m_type->m_member_types != nullptr)
                {
                    auto* member = m_type->m_member_types->m_members;
                    while (member != nullptr)
                    {
                        auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                        member->m_member_type->construct(this_member, arg);

                        if (member->m_woovalue_init_may_null != nullptr)
                        {
                            auto* val = std::launder(reinterpret_cast<script::woovalue*>(this_member));

                            woort_Value tmp;

                            // TODO: Not impled yet.
                            woort_GC_Pin_get_internal_value_without_barrier(
                                &tmp, member->m_woovalue_init_may_null, 0);

                            //wo_pin_value_get(&tmp, member->m_woovalue_init_may_null);
                            //wo_pin_value_set_dup(val->m_pin_value, &tmp);
                        }

                        member = member->m_next_member;
                    }
                }
            }
            ~ToWooBaseComponent()
            {
                if (m_type->m_member_types != nullptr)
                {
                    auto* member = m_type->m_member_types->m_members;
                    while (member != nullptr)
                    {
                        auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                        member->m_member_type->destruct(this_member);
                        member = member->m_next_member;
                    }
                }
            }
            ToWooBaseComponent(const ToWooBaseComponent& another)
                : m_type(another.m_type)
            {
                if (m_type->m_member_types != nullptr)
                {
                    auto* member = m_type->m_member_types->m_members;
                    while (member != nullptr)
                    {
                        auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                        auto* other_member = (void*)((intptr_t)&another + member->m_member_offset);

                        member->m_member_type->copy(this_member, other_member);

                        member = member->m_next_member;
                    }
                }
            }
            ToWooBaseComponent(ToWooBaseComponent&& another)
                : m_type(another.m_type)
            {
                if (m_type->m_member_types != nullptr)
                {
                    auto* member = m_type->m_member_types->m_members;
                    while (member != nullptr)
                    {
                        auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                        auto* other_member = (void*)((intptr_t)&another + member->m_member_offset);

                        member->m_member_type->move(this_member, other_member);

                        member = member->m_next_member;
                    }
                }
            }
        };
    }
}
WOORT_API woort_api wojeapi_towoo_add_component(void)
{
    auto e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    void* comp = je_ecs_world_entity_add_component(e, ty->m_id);
    if (comp != nullptr)
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
            return woort_ret_panic("Stack overflow.");

        jeecs::towoo::ToWooBaseSystem::create_component_struct(WOORT_RETURN_SLOT, s, comp, ty);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_get_component(void)
{
    auto e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    void* comp = je_ecs_world_entity_get_component(e, ty->m_id);
    if (comp != nullptr)
    {
        woort_value s;
        if (!woort_push_reserve(1, &s))
            return woort_ret_panic("Stack overflow.");

        jeecs::towoo::ToWooBaseSystem::create_component_struct(WOORT_RETURN_SLOT, s, comp, ty);
        return woort_ret_option_value(WOORT_RETURN_SLOT);
    }
    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_towoo_remove_component(void)
{
    auto e = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    auto ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(1));

    je_ecs_world_entity_remove_component(e, ty->m_id);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_towoo_member_get(void)
{
    auto ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_c2w(woort_pointer(1), WOORT_RETURN_SLOT);

    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_member_set(void)
{
    auto ty = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_w2c(woort_pointer(1), 2);

    return woort_ret_void();
}

void je_towoo_update_api()
{
    // 1. 获取所有的BasicType，为这些类型生成对应的Woolang类型
    // ATTENTION: woolang_parsing_type_decl 不能以任何方式导入 je/towoo/components，
    //      因为towoo组件需要依赖此文件加载，而打包时，components已经被载入；这将导致
    //      循环依赖，运行打包后的程序时，会出现自己未加载而需要取typeinfo失败的情况。
    std::string woolang_parsing_type_decl =
        R"(// This file is auto-generated by JoyEngineECS.
// Do not edit this file manually.
import woo::std;
import je;
)";

    std::unordered_set<std::string> generated_types;

    auto** alltypes = jedbg_get_all_registed_types();
    std::vector<const jeecs::typing::type_info*> all_registed_types;
    for (auto* idx = alltypes; *idx != nullptr; ++idx)
    {
        all_registed_types.push_back(*idx);
    }
    je_mem_free(alltypes);

    for (auto* typeinfo : all_registed_types)
    {
        if (typeinfo == nullptr)
            continue;

        // 1. Declear type parsers
        auto* script_parser_info = typeinfo->get_script_parser();

        if (script_parser_info == nullptr || false == generated_types.insert(script_parser_info->m_woolang_typename).second)
            continue;

        woolang_parsing_type_decl +=
            std::string("// Declear of '") + script_parser_info->m_woolang_typename + "'\n" + script_parser_info->m_woolang_typedecl + "\n"
            "namespace " +
            script_parser_info->m_woolang_typename + "\n{\n" + "    using type = void\n    {\n" + "        public let typeinfo = je::typeinfo::load(\"" + script_parser_info->m_woolang_typename + "\")->unwrap;\n"
            "    }\n}\n\n";
    }

    std::string woolang_component_type_decl =
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

        // 1. Declear component
        if (typeinfo->m_type_class == je_typing_class::JE_COMPONENT)
        {
            // Read namespace of typeclass
            std::string tname = typeinfo->m_typename;
            if (tname.empty())
            {
                jeecs::debug::logerr("Component type %p have an empty name, pleace check.", typeinfo);
                continue;
            }

            size_t index = tname.find_last_of(':');

            std::optional<std::string> tnamespace = std::nullopt;
            if (index < tname.size() - 1 && index >= 1)
            {
                tnamespace = std::optional(tname.substr(0, index - 1));
                tname = tname.substr(index + 1);
            }

            woolang_component_type_decl += std::string("// Declear of '") + typeinfo->m_typename + "'\n";
            if (tnamespace)
                woolang_component_type_decl += "namespace " + tnamespace.value() + "{\n";

            woolang_component_type_decl += "using " + tname + " = struct{\n    public __addr: handle,\n";

            if (typeinfo->m_member_types != nullptr)
            {
                auto* registed_member = typeinfo->m_member_types->m_members;
                while (registed_member != nullptr)
                {
                    auto* parser = registed_member->m_member_type->get_script_parser();
                    if (parser != nullptr)
                    {
                        // 对于有脚本对接类型的组件成员，在这里挂上！
                        const char* real_type_name =
                            registed_member->m_woovalue_type_may_null == nullptr
                            ? parser->m_woolang_typename
                            : registed_member->m_woovalue_type_may_null;

                        woolang_component_type_decl +=
                            std::string("    public ") + registed_member->m_member_name + ": je::towoo::member<" + real_type_name + ", " + parser->m_woolang_typename + "::type" + ">,\n";
                    }
                    registed_member = registed_member->m_next_member;
                }
            }
            woolang_component_type_decl += "}\n{\n";

            // Generate ComponentT::type::typeinfo
            woolang_component_type_decl += std::string(
                "    using type = void\n"
                "    {\n"
                "        public let typeinfo = je::typeinfo::load(\"") +
                typeinfo->m_typename + "\")->unwrap;\n"
                "    }\n";

            woolang_component_type_decl += "}\n";

            if (tnamespace)
                woolang_component_type_decl += "}\n";
            woolang_component_type_decl += "\n";
            /*
            public using Component = struct{
                member: interface<Type>,
                ...
            };
            */
        }
    }

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
    auto registered_system_fnd = jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(tinfo);
    if (registered_system_fnd == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end())
    {
        jeecs::debug::logerr("There is no towoo-system of type '%p', failed to unregister.", tinfo);
    }
    else
    {
        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.erase(registered_system_fnd);
        je_typing_unregister(tinfo);
    }
}
const jeecs::typing::type_info* je_towoo_register_system(
    const char* system_name,
    const char* script_path)
{
    const jeecs::typing::type_info* created_system_type_info = nullptr;

    if (jeecs_file* texfile = jeecs_file_open(script_path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        wo_CompileErrors* cerror;
        woort_CodeEnv* cenv = wo_load_binary(script_path, src, texfile->m_file_length, &cerror);

        jeecs_file_close(texfile);

        free(src);
        if (cenv != nullptr)
        {
            auto systinfo = std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(cenv);

            systinfo->m_on_active_function = std::nullopt;
            systinfo->m_on_disable_function = std::nullopt;

            auto* sysinfo_ptr = systinfo.get();

            sysinfo_ptr->m_is_good = false;


            woort_vm* const vmm = woort_vm_create();
            if (vmm == nullptr)
            {
                jeecs::debug::logerr("Failed to register: '%s': Create VM failed.", system_name);
            }
            else
            {
                woort_vm* const last = woort_vm_swap(vmm);
                {
                    woort_value s;
                    if (!woort_push_reserve(6, &s))
                    {
                        jeecs::debug::logerr("Failed to register: '%s': Stack overflow.", system_name);
                    }
                    else
                    {
                        // Invoke "_init_towoo_system", if failed... boom!
                        const woort_value initfunc = s + 1,
                            create_function = s + 2,
                            close_function = s + 3,
                            on_active_function = s + 4,
                            on_disable_function = s + 5;

                        //wo_integer_t create_function = _je_wo_extern_symb_rsfunc("create");
                        //wo_integer_t close_function = _je_wo_extern_symb_rsfunc("close");

                        //wo_integer_t on_active_function = _je_wo_extern_symb_rsfunc("on_active");
                        //wo_integer_t on_disable_function = _je_wo_extern_symb_rsfunc("on_disable");

                        if (!woort_load_extern_const(initfunc, cenv, "_init_towoo_system"))
                        {
                            jeecs::debug::logerr("Failed to register: '%s' cannot find '_init_towoo_system' in '%s', "
                                "forget to import je/towoo/system.wo ?",
                                system_name, script_path);
                        }
                        else if (!woort_load_extern_const(create_function, cenv, "create"))
                        {
                            jeecs::debug::logerr("Failed to register: '%s' cannot find 'create' function in '%s'.",
                                system_name, script_path);
                        }
                        else if (!woort_load_extern_const(close_function, cenv, "close"))
                        {
                            jeecs::debug::logerr("Failed to register: '%s' cannot find 'close' in '%s'.",
                                system_name, script_path);
                        }
                        else
                        {
                            if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup_codeenv(WOORT_IGNORE, cenv))
                            {
                                jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                                    system_name, woort_vm_get_runtime_error(vmm));
                            }
                            else
                            {
                                sysinfo_ptr->m_create_function = *woort_internal_value(create_function);
                                sysinfo_ptr->m_close_function = *woort_internal_value(close_function);

                                if (woort_load_extern_const(on_active_function, cenv, "on_active"))
                                    sysinfo_ptr->m_on_active_function.emplace() = *woort_internal_value(on_active_function);

                                if (woort_load_extern_const(on_disable_function, cenv, "on_disable"))
                                    sysinfo_ptr->m_on_disable_function.emplace() = *woort_internal_value(on_disable_function);

                                je_towoo_unregister_system(je_typing_get_info_by_name(system_name));

                                created_system_type_info = je_typing_register(
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

                                assert(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(created_system_type_info) ==
                                    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end());

                                do
                                {
                                    std::lock_guard ug1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

                                    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems[created_system_type_info]
                                        = std::move(systinfo);

                                } while (0);

                                woort_set_pointer(s + 0, (void*)created_system_type_info);

                                if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(WOORT_IGNORE, initfunc))
                                    // No need for locking
                                    jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                                        system_name, woort_vm_get_runtime_error(vmm));
                                else
                                    sysinfo_ptr->m_is_good = true;
                            }
                        }

                        woort_pop(6);
                    }
                }
                (void)woort_vm_swap(last);

                woort_vm_close(vmm);
            }
        }
        else
        {
            jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                system_name, wo_get_compile_error(cerror, WO_PLAIM));
            wo_compile_errors_free(cerror);
        }
    }
    else
    {
        jeecs::debug::logerr("Failed to register: '%s' unable to open file '%s'.",
            system_name, script_path);
    }
    return created_system_type_info;
}

void jetowoo_finish()
{
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);
    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.clear();
}
enum _jetowoo_job_type
{
    PRE_UPDATE,
    UPDATE,
    LATE_UPDATE,
};
WOORT_API woort_api wojeapi_towoo_register_system_job(void)
{
    // wojeapi_towoo_register_system_job(tinfo: je::typeinfo, function, requirements: array<(type, gid, typeinfo)>, arg_comp_count: int)
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

    auto* tinfo = static_cast<const jeecs::typing::type_info*>(woort_pointer(0));
    auto registered_system_fnd = jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(tinfo);
    if (registered_system_fnd == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end())
    {
        jeecs::debug::logerr("The towoo-system type: '%p' has not been registered.", tinfo);
        return woort_ret_void();
    }

    auto& works = registered_system_fnd->second;

    jeecs::towoo::ToWooBaseSystem::towoo_step_work stepwork;

    stepwork.m_function = *woort_internal_value(1);

    _jetowoo_job_type que = (_jetowoo_job_type)woort_int(2);
    const woort_value requirements = 3;
    size_t component_arg_count = (size_t)woort_int(4);
    bool is_single_work = woort_bool(5);

    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value requirement_info = s + 0;
    const woort_value elem = s + 1;

    stepwork.m_is_single_work = is_single_work;

    if (!stepwork.m_is_single_work)
    {
        const size_t requirements_count = woort_vec_len(requirements);
        for (size_t i = 0; i < requirements_count; ++i)
        {
            (void)woort_vec_get(requirement_info, requirements, i);

            woort_struct_get(elem, requirement_info, 2);
            const auto* typeinfo = static_cast<const jeecs::typing::type_info*>(woort_pointer(elem));

            woort_struct_get(elem, requirement_info, 0);
            jeecs::requirement::type ty = (jeecs::requirement::type)woort_int(elem);

            woort_struct_get(elem, requirement_info, 1);

            stepwork.m_dependence.m_requirements.push_back(
                jeecs::requirement{
                    ty,
                    (size_t)woort_int(elem),
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
WOORT_API woort_api wojeapi_towoo_update_component_data(void)
{
    // wojeapi_towoo_register_component(name, [(name, typeinfo, option<typename>)])
    std::string component_name = woort_string(0);

    auto* towoo_component_tinfo = je_typing_get_info_by_name(component_name.c_str());
    if (towoo_component_tinfo != nullptr)
    {
        if (towoo_component_tinfo->m_hash != jeecs::basic::hash_compile_time(("_towoo_component_" + component_name).c_str()))
            return woort_ret_panic("Invalid towoo component name, cannot same as native-components.");
    }

    const woort_value members = 1;
    size_t member_count = woort_vec_len(members);

    size_t component_size = sizeof(jeecs::towoo::ToWooBaseComponent);
    size_t component_allign = alignof(jeecs::towoo::ToWooBaseComponent);

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
    std::vector<_member_info> member_defs;

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value member_def = s + 0;
    const woort_value member_info = s + 1;
    const woort_value wooval_init = s + 2;

    for (size_t i = 0; i < member_count; ++i)
    {
        (void)woort_vec_get(member_def, members, i);

        woort_struct_get(member_info, member_def, 0);
        std::string member_name = woort_string(member_info);

        woort_struct_get(member_info, member_def, 1);
        auto* member_typeinfo = static_cast<const jeecs::typing::type_info*>(woort_pointer(member_info));

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
        component_allign = std::max(component_allign, member_typeinfo->m_chunk_size);
    }

    if (towoo_component_tinfo == nullptr)
        towoo_component_tinfo = je_typing_register(
            component_name.c_str(),
            jeecs::basic::hash_compile_time(("_towoo_component_" + component_name).c_str()),
            component_size,
            component_allign,
            je_typing_class::JE_COMPONENT,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);
    else
        // 此处仅更新类型的大小和对齐，并释放成员信息以供重新注册
        je_typing_reset(
            towoo_component_tinfo,
            component_size,
            component_allign,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::typing::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);

    for (auto& memberinfo : member_defs)
    {
        _wooval_type* wooval = nullptr;

        if (memberinfo.m_wooval_type.has_value())
        {
            *woort_internal_value(wooval_init) =
                memberinfo.m_wooval_type.value().m_wooval_val;
        }

        je_register_member(
            towoo_component_tinfo,
            memberinfo.m_type,
            memberinfo.m_name.c_str(),
            wooval != nullptr ? wooval->m_wooval_type.c_str() : nullptr,
            wooval != nullptr ? wooval_init : WOORT_IGNORE,
            memberinfo.m_offset);
    }

    return woort_ret_pointer((void*)towoo_component_tinfo);
}

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
T& wo_component(woort_value val)
{
    woort_Value tmp;
    wo_struct_get(&tmp, val, 0);
    return *static_cast<T*>(woort_pointer(&tmp));
}
template <typename T>
T* wo_option_component(woort_value val)
{
    woort_Value tmp;
    if (wo_option_get(&tmp, val))
    {
        wo_struct_get(&tmp, &tmp, 0);
        return static_cast<T*>(
            woort_pointer(&tmp));
    }
    return nullptr;
}

WOORT_API woort_api wojeapi_towoo_ray_create(void)
{
    return woort_ret_gchandle(
        new jeecs::math::ray(wo_vec3(0), wo_vec3(1)),
        WOORT_IGNORE,
        [](void* p)
        {
            delete (jeecs::math::ray*)p;
        },
        nullptr);
}
WOORT_API woort_api wojeapi_towoo_ray_from_camera(void)
{
    return woort_ret_gchandle(
        new jeecs::math::ray(
            wo_component<jeecs::Transform::Translation>(0),
            wo_component<jeecs::Camera::Projection>(1),
            wo_vec2(2),
            woort_bool(3)),
        WOORT_IGNORE,
        [](void* p)
        { delete (jeecs::math::ray*)p; },
        nullptr);
}
WOORT_API woort_api wojeapi_towoo_ray_intersect_entity(void)
{
    auto* ray = (jeecs::math::ray*)woort_gcpointer(0);
    auto result = ray->intersect_entity(
        wo_component<jeecs::Transform::Translation>(1),
        wo_option_component<jeecs::Renderer::Shape>(2),
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
    auto* ray = (jeecs::math::ray*)woort_gcpointer(0);

    wo_set_vec3(WOORT_RETURN_SLOT, ray->orgin);
    return woort_ret();
}
WOORT_API woort_api wojeapi_towoo_ray_direction(void)
{
    auto* ray = (jeecs::math::ray*)woort_gcpointer(0);

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
    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    auto& collisionResult = wo_component<jeecs::Physics2D::CollisionResult>(0);

    const woort_value c = s + 0;
    const woort_value elem = s + 1;
    const woort_value val = s + 2;

    woort_set_map(c);
    woort_map_reserve(c, collisionResult.results.size());

    for (auto& [rigidbody, result] : collisionResult.results)
    {
        woort_set_struct(val, 2);

        (void)woort_map_set_by_pointer(c, rigidbody, val);

        wo_set_vec2(elem, result.position);
        woort_struct_set(val, 0, elem);
        wo_set_vec2(elem, result.normalize);
        woort_struct_set(val, 1, elem);
    }

    return woort_ret_value(c);
}

WOORT_API woort_api wojeapi_towoo_physics2d_collisionresult_check(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto& collisionResult = wo_component<jeecs::Physics2D::CollisionResult>(0);
    auto& rigidbody = wo_component<jeecs::Physics2D::Rigidbody>(1);

    auto* result = collisionResult.check(&rigidbody);
    if (result != nullptr)
    {
        woort_value ret = s + 0;

        woort_set_struct(ret, 2);

        wo_set_vec2(s + 1, result->position);
        woort_struct_set(ret, 0, s + 1);

        wo_set_vec2(s + 1, result->normalize);
        woort_struct_set(ret, 1, s + 1);

        return woort_ret_option_value(ret);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_towoo_renderer_textures_bind_texture(void)
{
    auto& textures = wo_component<jeecs::Renderer::Textures>(0);
    size_t pass = (size_t)woort_int(1);
    auto* tex = (jeecs::basic::resource<jeecs::graphic::texture> *)woort_gcpointer(2);

    textures.bind_texture(pass, *tex);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_textures_get_texture(void)
{
    auto& textures = wo_component<jeecs::Renderer::Textures>(0);
    size_t pass = (size_t)woort_int(1);

    auto tex = textures.get_texture(pass);

    if (tex.has_value())
    {
        return woort_ret_option_gchandle(
            new jeecs::basic::resource<jeecs::graphic::texture>(tex.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture> *)p;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_i(void)
{
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0);
    const char* name = woort_string(1);

    shaders.set_uniform(name, (int)woort_int(2));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_r(void)
{
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0);
    const char* name = woort_string(1);

    shaders.set_uniform(name, woort_float(2));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_set_uniform_v(void)
{
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0);
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
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0);

    shaders.shaders.clear();
    const size_t setting_shaders_len = woort_vec_len(1);
    for (size_t i = 0; i < setting_shaders_len; ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 1, i);
        shaders.shaders.push_back(
            *static_cast<jeecs::basic::resource<jeecs::graphic::shader>*>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }
    return woort_ret_void(vm);
}

WOORT_API woort_api wojeapi_towoo_renderer_shaders_get_shaders(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto& shaders = wo_component<jeecs::Renderer::Shaders>(0);

    const woort_value c = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(c);

    for (auto& shad : shaders.shaders)
    {
        woort_set_gchandle(
            elem,
            new jeecs::basic::resource<jeecs::graphic::shader>(shad), 
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete std::launder(reinterpret_cast<
                    jeecs::basic::resource<jeecs::graphic::shader> *>(ptr));
            },
            nullptr);
        woort_vec_push(c, elem);
    }
    return woort_ret_value(c);
}

WOORT_API woort_api wojeapi_towoo_transform_translation_global_pos(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);

    wo_set_vec3(WOORT_RETURN_SLOT, trans.world_position);
    return woort_ret_value(WOORT_RETURN_SLOT);
}
WOORT_API woort_api wojeapi_towoo_transform_translation_global_rot(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);

    wo_set_quat(WOORT_RETURN_SLOT, trans.world_rotation);
    return woort_ret_value(WOORT_RETURN_SLOT);
}

WOORT_API woort_api wojeapi_towoo_transform_translation_parent_pos(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);
    wo_set_vec3(
        WOORT_RETURN_SLOT,
        trans.get_parent_position(
            wo_option_component<jeecs::Transform::LocalPosition>(1),
            wo_option_component<jeecs::Transform::LocalRotation>(2)));
    return woort_ret_value(WOORT_RETURN_SLOT);
}
WOORT_API woort_api wojeapi_towoo_transform_translation_parent_rot(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);

    wo_set_quat(WOORT_RETURN_SLOT, trans.get_parent_rotation(wo_option_component<jeecs::Transform::LocalRotation>(1)));
    return woort_ret_value(WOORT_RETURN_SLOT);
}

WOORT_API woort_api wojeapi_towoo_transform_translation_set_global_pos(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);
    auto pos = wo_vec3(1);
    auto* lpos = wo_option_component<jeecs::Transform::LocalPosition>(2);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(3);

    trans.set_global_position(pos, lpos, lrot);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_towoo_transform_translation_set_global_rot(void)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(0);
    auto rot = wo_quat(1);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(2);

    trans.set_global_rotation(rot, lrot);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_active_animation(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0);
    anim.animations.active_action(
        (size_t)woort_int(1), woort_string(2), woort_bool(3));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_stop_animation(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0);
    anim.animations.stop_action((size_t)woort_int(1));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_animation_frameanimation_is_playing(void)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(0);

    return woort_ret_bool(anim.animations.is_playing((size_t)woort_int(1)));
}

WOORT_API woort_api wojeapi_towoo_audio_playing_set_buffer(void)
{
    auto& playing = wo_component<jeecs::Audio::Playing>(0);

    auto* buf = (jeecs::basic::resource<jeecs::audio::buffer> *)woort_gcpointer(1);
    playing.set_buffer(*buf);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_towoo_audio_source_get_source(void)
{
    auto& source = wo_component<jeecs::Audio::Source>(0);
    return woort_ret_gchandle(
        new jeecs::basic::resource<jeecs::audio::source>(source.source),
        WOORT_IGNORE,
        [](void* p){ delete (jeecs::basic::resource<jeecs::audio::source> *)p; },
        nullptr);
}

WOORT_API woort_api wojeapi_towoo_userinterface_origin_layout(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto& origin = wo_component<jeecs::UserInterface::Origin>(0);

    auto r = wo_vec2(1);

    jeecs::math::vec2 abssize;
    jeecs::math::vec2 absoffset;
    jeecs::math::vec2 center_offset;

    origin.get_layout(r.x, r.y, &absoffset, &abssize, &center_offset);

    woort_set_struct(s + 0, 3);

    wo_set_vec2(s + 1, absoffset);
    woort_struct_set(s + 0, 0, s + 1);

    wo_set_vec2(s + 1, abssize);
    woort_struct_set(s + 0, 1, s + 1);

    wo_set_vec2(s + 1, center_offset);
    woort_struct_set(s + 0, 2, s + 1);

    return woort_ret_value(s + 0);
}

WOORT_API woort_api wojeapi_towoo_userinterface_origin_mouse_on(void)
{
    auto& origin = wo_component<jeecs::UserInterface::Origin>(0);

    auto r = wo_vec2(1);
    auto a = woort_float(2);
    auto m = wo_vec2(3);

    return woort_ret_bool(origin.mouse_on(r.x, r.y, a, m));
}
