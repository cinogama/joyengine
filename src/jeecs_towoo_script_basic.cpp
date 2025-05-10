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

wo_integer_t _je_wo_extern_symb_rsfunc(wo_vm vm, wo_string_t name)
{
    wo_integer_t result;
    [[maybe_unused]] wo_handle_t initfunc_jit_func;

    if (wo_extern_symb(vm, name, &result, &initfunc_jit_func))
        return result;

    return 0;
}

namespace jeecs
{
    namespace towoo
    {
        struct ToWooBaseSystem : game_system
        {
            struct towoo_step_work
            {
                dependence m_dependence;
                _wo_value m_function;
                std::vector<const typing::type_info*> m_used_components;
                bool m_is_single_work;
            };
            struct towoo_system_info
            {
                JECS_DISABLE_MOVE_AND_COPY(towoo_system_info);

                wo_vm m_base_vm;
                bool m_is_good;

                std::optional<wo_integer_t> m_on_active_function;
                std::optional<wo_integer_t> m_on_disable_function;
                wo_integer_t m_create_function;
                wo_integer_t m_close_function;

                std::vector<towoo_step_work> m_preworks;
                std::vector<towoo_step_work> m_works;
                std::vector<towoo_step_work> m_lateworks;

                towoo_system_info(wo_vm vm)
                    : m_base_vm(vm)
                {

                }
                ~towoo_system_info()
                {
                    wo_close_vm(m_base_vm);
                }
            };

            inline static std::shared_mutex _registered_towoo_base_systems_mx;
            inline static std::unordered_map<const typing::type_info*, std::unique_ptr<towoo_system_info>>
                _registered_towoo_base_systems;

            // 执行代码使用的虚拟机
            wo_vm m_job_vm;
            wo_pin_value m_context;

            std::optional<wo_integer_t> m_on_active_function;
            std::optional<wo_integer_t> m_on_disable_function;
            wo_integer_t m_create_function;
            wo_integer_t m_close_function;

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
                    m_job_vm = wo_borrow_vm(base_info->m_base_vm);

                    m_create_function = base_info->m_create_function;
                    m_close_function = base_info->m_close_function;
                    m_on_active_function = base_info->m_on_active_function;
                    m_on_disable_function = base_info->m_on_disable_function;

                    m_pre_dependences = base_info->m_preworks;
                    m_dependences = base_info->m_works;
                    m_late_dependences = base_info->m_lateworks;

                    wo_value m_job_vm_s = wo_reserve_stack(m_job_vm, 1, nullptr);
                    wo_set_pointer(m_job_vm_s + 0, w.handle());

                    wo_value v = wo_invoke_rsfunc(m_job_vm, m_create_function, 1, nullptr, &m_job_vm_s);
                    wo_pop_stack(m_job_vm, 1);

                    if (v == nullptr)
                    {
                        jeecs::debug::logerr("Failed to invoke 'create' function for system: '%s'.",
                            m_type->m_typename);
                        wo_release_vm(m_job_vm);
                        m_job_vm = nullptr;
                    }
                    else
                    {
                        m_context = wo_create_pin_value();
                        wo_pin_value_set(m_context, v);
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
                    wo_value s = wo_reserve_stack(m_job_vm, 1, nullptr);

                    wo_pin_value_get(s, m_context);
                    wo_value v = wo_invoke_rsfunc(m_job_vm, m_close_function, 1, nullptr, &s);
                    if (v == nullptr)
                    {
                        jeecs::debug::logerr("Failed to invoke 'close' function for system: '%s'.",
                            m_type->m_typename);
                    }

                    wo_release_vm(m_job_vm);
                    wo_close_pin_value(m_context);
                }
            }

            static void create_component_struct(
                wo_value writeval,
                wo_vm vm, 
                void* component, 
                const typing::type_info* ctype)
            {
                assert(component != nullptr);

                _wo_value tmp;  // 此处不能 wo_push_empty(vm); 或者需要pop，否则栈不平
                // 另外此处不需要 wo_push_empty 因为我们不会往里面放GC对象

                if (ctype->m_member_types != nullptr)
                {
                    wo_set_struct(writeval, vm, ctype->m_member_types->m_member_count + 1);

                    uint16_t member_idx = 0;
                    auto* member_tinfo = ctype->m_member_types->m_members;
                    while (member_tinfo != nullptr)
                    {
                        // Set member;
                        wo_set_pointer(&tmp,
                            reinterpret_cast<void*>(
                                reinterpret_cast<intptr_t>(component)
                                + member_tinfo->m_member_offset));

                        wo_struct_set(writeval, member_idx + 1, &tmp);

                        ++member_idx;
                        member_tinfo = member_tinfo->m_next_member;
                    }
                }
                else
                {
                    wo_set_struct(writeval, vm, 1);
                }

                wo_set_pointer(&tmp, component);
                wo_struct_set(writeval, 0, &tmp);
            }

            void update_step_work(std::vector<towoo_step_work>& works)
            {
                ScriptRuntimeSystem::system_instance =
                    get_world().get_system<ScriptRuntimeSystem>();

                if (m_job_vm == nullptr)
                    return;

                for (auto& work : works)
                {
                    if (work.m_is_single_work)
                    {
                        wo_value s = wo_reserve_stack(m_job_vm, 1, nullptr);
                        wo_pin_value_get(s, m_context);

                        // Invoke!
                        wo_invoke_value(m_job_vm, &work.m_function, 1, nullptr, &s);
                        wo_pop_stack(m_job_vm, 1);
                    }
                    else
                    {
                        work.m_dependence.update(get_world());
                        for (auto* archinfo : work.m_dependence.m_archs)
                        {
                            auto cur_chunk = je_arch_get_chunk(archinfo->m_arch);
                            while (cur_chunk)
                            {
                                auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                                typing::version_t version;
                                for (size_t eid = 0; eid < archinfo->m_entity_count; ++eid)
                                {
                                    if (selector::get_entity_avaliable_version(entity_meta_addr, eid, &version))
                                    {
                                        // game_entity{ cur_chunk, eid, version }
                                        // Valid! prepare to invoke!
                                        const size_t used_component_count = work.m_used_components.size();
                                        wo_value s = wo_reserve_stack(m_job_vm, used_component_count + 2 + 1, nullptr);

                                        wo_value tmp_elem = s + used_component_count + 2;

                                        for (auto cmpidx = work.m_used_components.begin(); cmpidx != work.m_used_components.end(); ++cmpidx)
                                        {
                                            const size_t cmpid = cmpidx - work.m_used_components.begin();

                                            void* component = selector::get_component_from_archchunk_ptr(archinfo, cur_chunk, eid, cmpid);
                                            const auto* typeinfo = *cmpidx;

                                            wo_value component_st = s + 2 + cmpid;
                                            switch (work.m_dependence.m_requirements[cmpid].m_require)
                                            {
                                            case jeecs::requirement::type::CONTAIN:
                                            {
                                                create_component_struct(component_st, m_job_vm, component, typeinfo);
                                                break;
                                            }
                                            case jeecs::requirement::type::MAYNOT:
                                            {
                                                if (component == nullptr)
                                                {
                                                    // option::none
                                                    wo_set_option_none(component_st, m_job_vm);
                                                }
                                                else
                                                {
                                                    // option::value
                                                    create_component_struct(tmp_elem, m_job_vm, component, typeinfo);
                                                    wo_set_option_val(component_st, m_job_vm, tmp_elem);
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
                                        wo_set_gchandle(s + 1, m_job_vm, new jeecs::game_entity{ cur_chunk , eid, version }, nullptr,
                                            [](void* eptr) {delete std::launder(reinterpret_cast<jeecs::game_entity*>(eptr)); });

                                        // Push context
                                        wo_pin_value_get(s + 0, m_context);

                                        // Invoke!
                                        wo_invoke_value(m_job_vm, &work.m_function, used_component_count + 2, nullptr, &s);
                                        wo_pop_stack(m_job_vm, used_component_count + 2 + 1);
                                    }
                                }

                                // Update next chunk.
                                cur_chunk = je_arch_next_chunk(cur_chunk);
                            }
                        }
                    }
                }

                ScriptRuntimeSystem::system_instance = nullptr;
            }

            void OnEnable()
            {
                if (m_job_vm == nullptr)
                    return;

                if (m_on_active_function.has_value())
                {
                    wo_value s = wo_reserve_stack(m_job_vm, 1, nullptr);
                    wo_pin_value_get(s, m_context);
                    // Invoke!
                    wo_invoke_rsfunc(m_job_vm, m_on_active_function.value(), 1, nullptr, &s);
                    wo_pop_stack(m_job_vm, 1);
                }
            }
            void OnDisable()
            {
                if (m_job_vm == nullptr)
                    return;
                if (m_on_disable_function.has_value())
                {
                    wo_value s = wo_reserve_stack(m_job_vm, 1, nullptr);
                    wo_pin_value_get(s, m_context);
                    // Invoke!
                    wo_invoke_rsfunc(m_job_vm, m_on_disable_function.value(), 1, nullptr, &s);
                    wo_pop_stack(m_job_vm, 1);
                }
            }

            void PreUpdate(jeecs::selector&)
            {
                update_step_work(m_pre_dependences);
            }
            void Update(jeecs::selector&)
            {
                update_step_work(m_dependences);
            }
            void LateUpdate(jeecs::selector&)
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

                            _wo_value tmp;
                            wo_pin_value_get(&tmp, member->m_woovalue_init_may_null);
                            wo_pin_value_set_dup(val->m_pin_value, &tmp);
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
                :m_type(another.m_type)
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
                :m_type(another.m_type)
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
WO_API wo_api wojeapi_towoo_add_component(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    void* comp = je_ecs_world_entity_add_component(e, ty->m_id);
    if (comp != nullptr)
    {
        wo_value val = s + 0;
        jeecs::towoo::ToWooBaseSystem::create_component_struct(val, vm, comp, ty);
        return wo_ret_option_val(vm, val);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_get_component(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    void* comp = je_ecs_world_entity_get_component(e, ty->m_id);
    if (comp != nullptr)
    {
        wo_value val = s + 0;
        jeecs::towoo::ToWooBaseSystem::create_component_struct(val, vm, comp, ty);
        return wo_ret_option_val(vm, val);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_remove_component(wo_vm vm, wo_value args)
{
    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    je_ecs_world_entity_remove_component(e, ty->m_id);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_towoo_member_get(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 0)));

    wo_value val = s + 0;

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_c2w(wo_pointer(args + 1), vm, val);

    return wo_ret_val(vm, val);
}
WO_API wo_api wojeapi_towoo_member_set(wo_vm vm, wo_value args)
{
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 0)));

    assert(ty->get_script_parser() != nullptr);
    ty->get_script_parser()->m_script_parse_w2c(wo_pointer(args + 1), vm, args + 2);

    return wo_ret_void(vm);
}

void je_towoo_update_api()
{
    // 1. 获取所有的BasicType，为这些类型生成对应的Woolang类型
    // ATTENTION: woolang_parsing_type_decl 不能以任何方式导入 je/towoo/components，
    //      因为towoo组件需要依赖此文件加载，而打包时，components已经被载入；这将导致
    //      循环依赖，运行打包后的程序时，会出现自己未加载而需要取typeinfo失败的情况。
    std::string woolang_parsing_type_decl =
        R"(// (C)Cinogama project.
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

        if (script_parser_info == nullptr
            || false == generated_types.insert(script_parser_info->m_woolang_typename).second)
            continue;

        woolang_parsing_type_decl +=
            std::string("// Declear of '")
            + script_parser_info->m_woolang_typename + "'\n"
            + script_parser_info->m_woolang_typedecl + "\n"
            "namespace " + script_parser_info->m_woolang_typename + "\n{\n"
            + "    using type = void\n    {\n"
            + "        public let typeinfo = je::typeinfo::load(\"" + script_parser_info->m_woolang_typename + "\")->unwrap;\n"
            "    }\n}\n\n";
    }

    std::string woolang_component_type_decl =
        R"(// (C)Cinogama project.
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
                            std::string("    public ") + registed_member->m_member_name + ": je::towoo::member<"
                            + real_type_name
                            + ", "
                            + parser->m_woolang_typename + "::type"
                            + ">,\n";
                    }
                    registed_member = registed_member->m_next_member;
                }
            }
            woolang_component_type_decl += "}\n{\n";

            // Generate ComponentT::type::typeinfo
            woolang_component_type_decl += std::string(
                "    using type = void\n"
                "    {\n"
                "        public let typeinfo = je::typeinfo::load(\"") + typeinfo->m_typename + "\")->unwrap;\n"
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

    if (!wo_virtual_source("je/towoo/types.wo", woolang_parsing_type_decl.c_str(), true))
        jeecs::debug::logfatal("Unable to regenerate 'je/towoo/types.wo' please check.");
    if (!wo_virtual_source("je/towoo/components.wo", woolang_component_type_decl.c_str(), true))
        jeecs::debug::logfatal("Unable to regenerate 'je/towoo/components.wo' please check.");
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
    wo_vm vm = wo_create_vm();
    auto systinfo = std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(vm);

    systinfo->m_on_active_function = std::nullopt;
    systinfo->m_on_disable_function = std::nullopt;
    systinfo->m_create_function = 0;
    systinfo->m_close_function = 0;

    auto* sysinfo_ptr = systinfo.get();

    sysinfo_ptr->m_is_good = false;

    if (jeecs_file* texfile = jeecs_file_open(script_path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        bool result = wo_load_binary(vm, script_path, src, texfile->m_file_length);

        jeecs_file_close(texfile);

        free(src);
        if (result)
        {
            // Invoke "_init_towoo_system", if failed... boom!
            wo_integer_t initfunc = _je_wo_extern_symb_rsfunc(vm, "_init_towoo_system");

            wo_integer_t create_function = _je_wo_extern_symb_rsfunc(vm, "create");
            wo_integer_t close_function = _je_wo_extern_symb_rsfunc(vm, "close");

            wo_integer_t on_active_function = _je_wo_extern_symb_rsfunc(vm, "on_active");
            wo_integer_t on_disable_function = _je_wo_extern_symb_rsfunc(vm, "on_disable");

            if (initfunc == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find '_init_towoo_system' in '%s', "
                    "forget to import je/towoo/system.wo ?",
                    system_name, script_path);
            }
            else if (create_function == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find 'create' function in '%s'.",
                    system_name, script_path);
            }
            else if (close_function == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find 'close' in '%s'.",
                    system_name, script_path);
            }
            else
            {
                wo_jit(vm);
                if (nullptr == wo_run(vm))
                {
                    jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                        system_name, wo_get_runtime_error(vm));
                }
                else
                {
                    sysinfo_ptr->m_create_function = create_function;
                    sysinfo_ptr->m_close_function = close_function;

                    if (on_active_function != 0)
                        sysinfo_ptr->m_on_active_function = on_active_function;

                    if (on_disable_function != 0)
                        sysinfo_ptr->m_on_disable_function = on_disable_function;

                    je_towoo_unregister_system(je_typing_get_info_by_name(system_name));

                    auto* towoo_system_tinfo = je_typing_register(
                        system_name,
                        jeecs::basic::hash_compile_time(system_name),
                        sizeof(jeecs::towoo::ToWooBaseSystem),
                        alignof(jeecs::towoo::ToWooBaseSystem),
                        je_typing_class::JE_SYSTEM,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::constructor,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::destructor,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::copier,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::mover);

                    je_register_system_updater(
                        towoo_system_tinfo,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::on_enable,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::on_disable,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::pre_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::state_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::physics_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::transform_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::late_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::commit_update,
                        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::graphic_update);

                    assert(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(towoo_system_tinfo) ==
                        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end());

                    do
                    {
                        std::lock_guard ug1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

                        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems[towoo_system_tinfo]
                            = std::move(systinfo);
                    } while (0);

                    wo_value s = wo_reserve_stack(vm, 1, nullptr);
                    wo_set_pointer(s + 0, (void*)towoo_system_tinfo);

                    if (nullptr == wo_invoke_rsfunc(vm, initfunc, 1, nullptr, &s))
                        // No need for locking
                        jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                            system_name, wo_get_runtime_error(vm));
                    else
                        sysinfo_ptr->m_is_good = true;

                    wo_pop_stack(vm, 1);

                    return towoo_system_tinfo;
                }
            }
        }
        else
        {
            jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                system_name, wo_get_compile_error(vm, WO_NEED_COLOR));
        }
    }
    else
    {
        jeecs::debug::logerr("Failed to register: '%s' unable to open file '%s'.",
            system_name, script_path);
    }
    return nullptr;
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
WO_API wo_api wojeapi_towoo_register_system_job(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    // wojeapi_towoo_register_system_job(tinfo: je::typeinfo, function, requirements: array<(type, gid, typeinfo)>, arg_comp_count: int)
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

    auto* tinfo = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    auto registered_system_fnd = jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(tinfo);
    if (registered_system_fnd == jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end())
    {
        jeecs::debug::logerr("The towoo-system type: '%p' has not been registered.", tinfo);
        return wo_ret_void(vm);
    }

    auto& works = registered_system_fnd->second;

    jeecs::towoo::ToWooBaseSystem::towoo_step_work stepwork;

    assert(wo_valuetype(args + 1) != WO_CLOSURE_TYPE);
    wo_set_val(&stepwork.m_function, args + 1);

    _jetowoo_job_type que = (_jetowoo_job_type)wo_int(args + 2);
    wo_value requirements = args + 3;
    wo_integer_t component_arg_count = wo_int(args + 4);
    bool is_single_work = wo_bool(args + 5);
    wo_value requirement_info = s + 0;
    wo_value elem = s + 1;

    stepwork.m_is_single_work = is_single_work;

    if (!stepwork.m_is_single_work)
    {
        for (wo_integer_t i = 0; i < wo_lengthof(requirements); ++i)
        {
            wo_arr_get(requirement_info, requirements, i);

            wo_struct_get(elem, requirement_info, 2);
            const auto* typeinfo = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(
                wo_pointer(elem)));

            wo_struct_get(elem, requirement_info, 0);
            jeecs::requirement::type ty = (jeecs::requirement::type)wo_int(elem);

            wo_struct_get(elem, requirement_info, 1);

            stepwork.m_dependence.m_requirements.push_back(
                jeecs::requirement(
                    ty,
                    wo_int(elem),
                    typeinfo->m_id
                ));

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

    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_towoo_update_component_data(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);

    // wojeapi_towoo_register_component(name, [(name, typeinfo, option<typename>)])
    std::string component_name = wo_string(args + 0);

    auto* towoo_component_tinfo = je_typing_get_info_by_name(component_name.c_str());
    if (towoo_component_tinfo != nullptr)
    {
        if (towoo_component_tinfo->m_hash != jeecs::basic::hash_compile_time(("_towoo_component_" + component_name).c_str()))
            return wo_ret_panic(vm, "Invalid towoo component name, cannot same as native-components.");
    }

    wo_value members = args + 1;
    wo_integer_t member_count = wo_lengthof(members);

    size_t component_size = sizeof(jeecs::towoo::ToWooBaseComponent);
    size_t component_allign = alignof(jeecs::towoo::ToWooBaseComponent);

    struct _wooval_type
    {
        std::string m_wooval_type;
        wo_value    m_wooval_val;
    };
    struct _member_info
    {
        std::string m_name;
        std::optional<_wooval_type> m_wooval_type;
        const jeecs::typing::type_info* m_type;
        size_t m_offset;
    };
    std::vector<_member_info> member_defs;
    wo_value member_def = s + 0;
    wo_value member_info = s + 1;
    wo_value wooval_init = s + 2;
    for (wo_integer_t i = 0; i < member_count; ++i)
    {
        wo_arr_get(member_def, members, i);
        wo_struct_get(member_info, member_def, 0);
        std::string member_name = wo_string(member_info);

        wo_struct_get(member_info, member_def, 1);
        auto* member_typeinfo = (const jeecs::typing::type_info*)
            wo_pointer(member_info);

        std::optional<_wooval_type> member_wooval_type = std::nullopt;
        wo_struct_get(member_info, member_def, 2);
        if (wo_option_get(member_info, member_info))
        {
            wo_struct_get(wooval_init, member_info, 1);
            wo_struct_get(member_info, member_info, 0);
            member_wooval_type = std::optional(
                _wooval_type
                {
                    wo_string(member_info),
                    wooval_init
                });
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
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);
    else
        // 此处仅更新类型的大小和对齐，并释放成员信息以供重新注册
        je_typing_reset(
            towoo_component_tinfo,
            component_size,
            component_allign,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
            jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::mover);


    for (auto& memberinfo : member_defs)
    {
        _wooval_type* wooval = memberinfo.m_wooval_type.has_value()
            ? &memberinfo.m_wooval_type.value()
            : nullptr;
        je_register_member(
            towoo_component_tinfo,
            memberinfo.m_type,
            memberinfo.m_name.c_str(),
            wooval != nullptr ? wooval->m_wooval_type.c_str() : nullptr,
            wooval != nullptr ? wooval->m_wooval_val : nullptr,
            memberinfo.m_offset);
    }

    return wo_ret_pointer(vm, (void*)towoo_component_tinfo);
}

jeecs::math::vec2 wo_vec2(wo_value val)
{
    jeecs::math::vec2 result;
    _wo_value tmp;
    wo_struct_get(&tmp, val, 0);
    result.x = wo_float(&tmp);
    wo_struct_get(&tmp, val, 1);
    result.y = wo_float(&tmp);

    return result;
}
jeecs::math::vec3 wo_vec3(wo_value val)
{
    jeecs::math::vec3 result;
    _wo_value tmp;
    wo_struct_get(&tmp, val, 0);
    result.x = wo_float(&tmp);
    wo_struct_get(&tmp, val, 1);
    result.y = wo_float(&tmp);
    wo_struct_get(&tmp, val, 2);
    result.z = wo_float(&tmp);

    return result;
}
jeecs::math::vec4 wo_vec4(wo_value val)
{
    jeecs::math::vec4 result;
    _wo_value tmp;
    wo_struct_get(&tmp, val, 0);
    result.x = wo_float(&tmp);
    wo_struct_get(&tmp, val, 1);
    result.y = wo_float(&tmp);
    wo_struct_get(&tmp, val, 2);
    result.z = wo_float(&tmp);
    wo_struct_get(&tmp, val, 3);
    result.w = wo_float(&tmp);

    return result;
}
jeecs::math::quat wo_quat(wo_value val)
{
    jeecs::math::quat result;
    _wo_value tmp;
    wo_struct_get(&tmp, val, 0);
    result.x = wo_float(&tmp);
    wo_struct_get(&tmp, val, 1);
    result.y = wo_float(&tmp);
    wo_struct_get(&tmp, val, 2);
    result.z = wo_float(&tmp);
    wo_struct_get(&tmp, val, 3);
    result.w = wo_float(&tmp);

    return result;
}

void wo_set_vec2(wo_value target, wo_vm vm, const jeecs::math::vec2& v)
{
    wo_set_struct(target, vm, 2);
    _wo_value tmp;
    wo_set_float(&tmp, v.x);
    wo_struct_set(target, 0, &tmp);
    wo_set_float(&tmp, v.y);
    wo_struct_set(target, 1, &tmp);
}
void wo_set_vec3(wo_value target, wo_vm vm, const jeecs::math::vec3& v)
{
    wo_set_struct(target, vm, 3);
    _wo_value tmp;
    wo_set_float(&tmp, v.x);
    wo_struct_set(target, 0, &tmp);
    wo_set_float(&tmp, v.y);
    wo_struct_set(target, 1, &tmp);
    wo_set_float(&tmp, v.z);
    wo_struct_set(target, 2, &tmp);
}
void wo_set_vec4(wo_value target, wo_vm vm, const jeecs::math::vec4& v)
{
    wo_set_struct(target, vm, 4);
    _wo_value tmp;
    wo_set_float(&tmp, v.x);
    wo_struct_set(target, 0, &tmp);
    wo_set_float(&tmp, v.y);
    wo_struct_set(target, 1, &tmp);
    wo_set_float(&tmp, v.z);
    wo_struct_set(target, 2, &tmp);
    wo_set_float(&tmp, v.w);
    wo_struct_set(target, 3, &tmp);
}
void wo_set_quat(wo_value target, wo_vm vm, const jeecs::math::quat& v)
{
    wo_set_struct(target, vm, 4);
    _wo_value tmp;
    wo_set_float(&tmp, v.x);
    wo_struct_set(target, 0, &tmp);
    wo_set_float(&tmp, v.y);
    wo_struct_set(target, 1, &tmp);
    wo_set_float(&tmp, v.z);
    wo_struct_set(target, 2, &tmp);
    wo_set_float(&tmp, v.w);
    wo_struct_set(target, 3, &tmp);
}

template<typename T>
T& wo_component(wo_value val)
{
    _wo_value tmp;
    wo_struct_get(&tmp, val, 0);
    return *std::launder(reinterpret_cast<T*>(wo_pointer(&tmp)));
}
template<typename T>
T* wo_option_component(wo_value val)
{
    _wo_value tmp;
    if (wo_option_get(&tmp, val))
    {
        wo_struct_get(&tmp, &tmp, 0);
        return std::launder(reinterpret_cast<T*>(
            wo_pointer(&tmp)));
    }
    return nullptr;
}

WO_API wo_api wojeapi_towoo_ray_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
        new jeecs::math::ray(wo_vec3(args + 0), wo_vec3(args + 1)),
        nullptr,
        [](void* p) {delete(jeecs::math::ray*)p; });
}
WO_API wo_api wojeapi_towoo_ray_from_camera(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm,
        new jeecs::math::ray(
            wo_component<jeecs::Transform::Translation>(args + 0),
            wo_component<jeecs::Camera::Projection>(args + 1),
            wo_vec2(args + 2),
            wo_bool(args + 3)
        ),
        nullptr,
        [](void* p) {delete(jeecs::math::ray*)p; });
}
WO_API wo_api wojeapi_towoo_ray_intersect_entity(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);
    auto result = ray->intersect_entity(
        wo_component<jeecs::Transform::Translation>(args + 1),
        wo_option_component<jeecs::Renderer::Shape>(args + 2),
        wo_bool(args + 3));

    if (result.intersected)
    {
        wo_set_vec3(s + 0, vm, result.place);
        return wo_ret_option_val(vm, s + 0);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_ray_origin(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);

    wo_set_vec3(s + 0, vm, ray->orgin);
    return wo_ret_val(vm, s + 0);
}
WO_API wo_api wojeapi_towoo_ray_direction(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);

    wo_set_vec3(s + 0, vm, ray->direction);
    return wo_ret_val(vm, s + 0);
}
WO_API wo_api wojeapi_towoo_math_sqrt(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, sqrt(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_sin(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, sin(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_cos(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, cos(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_tan(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, tan(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_asin(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, asin(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_acos(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, acos(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_atan(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, atan(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_atan2(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, atan2(wo_real(args + 0), wo_real(args + 1)));
}
WO_API wo_api wojeapi_towoo_math_quat_slerp(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    wo_set_quat(
        s + 0,
        vm,
        jeecs::math::quat::slerp(
            wo_quat(args + 0),
            wo_quat(args + 1),
            wo_float(args + 2))
    );
    return wo_ret_val(vm, s + 0);
}

WO_API wo_api wojeapi_towoo_physics2d_collisionresult_all(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);
    auto& collisionResult = wo_component<jeecs::Physics2D::CollisionResult>(args + 0);

    wo_value c = s + 0;
    auto key = s + 1;
    auto val = s + 2;

    wo_set_map(c, vm, collisionResult.results.size());

    for (auto& [rigidbody, result] : collisionResult.results)
    {
        wo_set_pointer(key, rigidbody);
        wo_set_struct(val, vm, 2);

        wo_map_set(c, key, val);

        wo_set_vec2(key, vm, result.position);
        wo_struct_set(val, 0, key);
        wo_set_vec2(key, vm, result.normalize);
        wo_struct_set(val, 1, key);
    }

    return wo_ret_val(vm, c);
}

WO_API wo_api wojeapi_towoo_physics2d_collisionresult_check(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    auto& collisionResult = wo_component<jeecs::Physics2D::CollisionResult>(args + 0);
    auto& rigidbody = wo_component<jeecs::Physics2D::Rigidbody>(args + 1);

    auto* result = collisionResult.check(&rigidbody);
    if (result != nullptr)
    {
        wo_value ret = s + 0;

        wo_set_struct(ret, vm, 2);

        wo_set_vec2(s + 1, vm, result->position);
        wo_struct_set(ret, 0, s + 1);

        wo_set_vec2(s + 1, vm, result->normalize);
        wo_struct_set(ret, 1, s + 1);

        return wo_ret_option_val(vm, ret);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_towoo_renderer_textures_bind_texture(wo_vm vm, wo_value args)
{
    auto& textures = wo_component<jeecs::Renderer::Textures>(args + 0);
    size_t pass = (size_t)wo_int(args + 1);
    auto* tex = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 2);

    textures.bind_texture(pass, *tex);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_renderer_textures_get_texture(wo_vm vm, wo_value args)
{
    auto& textures = wo_component<jeecs::Renderer::Textures>(args + 0);
    size_t pass = (size_t)wo_int(args + 1);

    auto tex = textures.get_texture(pass);

    if (tex != nullptr)
    {
        return wo_ret_option_gchandle(vm,
            new jeecs::basic::resource<jeecs::graphic::texture>(tex),
            nullptr,
            [](void* p)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture>*)p;
            });
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_towoo_renderer_shaders_set_uniform(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto& shaders = wo_component<jeecs::Renderer::Shaders>(args + 0);
    const char* name = wo_string(args + 1);
    wo_value val = args + 2;

    auto valtype = wo_valuetype(val);
    switch (valtype)
    {
    case WO_INTEGER_TYPE:
        shaders.set_uniform(name, (int)wo_int(val)); break;
    case WO_REAL_TYPE:
        shaders.set_uniform(name, wo_float(val)); break;
    case WO_STRUCT_TYPE:
    {
        wo_value v = s + 0;
        switch (wo_lengthof(val))
        {
        case 2:
        {
            shaders.set_uniform(name, wo_vec2(val));
            break;
        }
        case 3:
        {
            shaders.set_uniform(name, wo_vec3(val));
            break;
        }
        case 4:
        {
            shaders.set_uniform(name, wo_vec4(val));
            break;
        }
        default:
            return wo_ret_panic(vm, "Unknown value type when set_uniform.");
        }
        break;
    }
    default:
        return wo_ret_panic(vm, "Unknown value type when set_uniform.");
    }
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_renderer_shaders_set_shaders(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(args + 0);

    wo_value c = s + 0;

    shaders.shaders.clear();
    auto setting_shaders_len = wo_lengthof(args + 1);
    for (wo_integer_t i = 0; i < setting_shaders_len; ++i)
    {
        wo_arr_get(c, args + 1, i);
        shaders.shaders.push_back(
            *std::launder(reinterpret_cast<
                jeecs::basic::resource<jeecs::graphic::shader>*>(
                    wo_pointer(c))));
    }

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_renderer_shaders_get_shaders(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);
    auto& shaders = wo_component<jeecs::Renderer::Shaders>(args + 0);

    wo_value c = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(c, vm, 0);

    for (auto& shad : shaders.shaders)
    {
        wo_set_gchandle(elem, vm,
            new jeecs::basic::resource<jeecs::graphic::shader>(shad), nullptr,
            [](void* ptr)
            {
                delete std::launder(reinterpret_cast<
                    jeecs::basic::resource<jeecs::graphic::shader>*>(ptr));
            });
        wo_arr_add(c, elem);
    }

    return wo_ret_val(vm, c);
}

WO_API wo_api wojeapi_towoo_transform_translation_global_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);

    wo_set_vec3(s + 0, vm, trans.world_position);
    return wo_ret_val(vm, s + 0);
}
WO_API wo_api wojeapi_towoo_transform_translation_global_rot(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);

    wo_set_quat(s + 0, vm, trans.world_rotation);
    return wo_ret_val(vm, s + 0);
}

WO_API wo_api wojeapi_towoo_transform_translation_parent_pos(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);

    wo_set_vec3(s + 0, vm, trans.get_parent_position(
        wo_option_component<jeecs::Transform::LocalPosition>(args + 1),
        wo_option_component<jeecs::Transform::LocalRotation>(args + 2)));
    return wo_ret_val(vm, s + 0);
}
WO_API wo_api wojeapi_towoo_transform_translation_parent_rot(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);

    wo_set_quat(s + 0, vm, trans.get_parent_rotation(
        wo_option_component<jeecs::Transform::LocalRotation>(args + 1)));
    return wo_ret_val(vm, s + 0);
}

WO_API wo_api wojeapi_towoo_transform_translation_set_global_pos(wo_vm vm, wo_value args)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);
    auto pos = wo_vec3(args + 1);
    auto* lpos = wo_option_component<jeecs::Transform::LocalPosition>(args + 2);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(args + 3);

    trans.set_global_position(pos, lpos, lrot);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_towoo_transform_translation_set_global_rot(wo_vm vm, wo_value args)
{
    auto& trans = wo_component<jeecs::Transform::Translation>(args + 0);
    auto rot = wo_quat(args + 1);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(args + 2);

    trans.set_global_rotation(rot, lrot);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_animation_frameanimation_active_animation(wo_vm vm, wo_value args)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(args + 0);
    anim.animations.active_action(
        (size_t)wo_int(args + 1), wo_string(args + 2), wo_bool(args + 3));

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_animation_frameanimation_is_playing(wo_vm vm, wo_value args)
{
    auto& anim = wo_component<jeecs::Animation::FrameAnimation>(args + 0);

    return wo_ret_bool(vm, anim.animations.is_playing((size_t)wo_int(args + 1)));
}

WO_API wo_api wojeapi_towoo_audio_playing_set_buffer(wo_vm vm, wo_value args)
{
    auto& playing = wo_component<jeecs::Audio::Playing>(args + 0);

    auto* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 1);
    playing.set_buffer(*buf);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_audio_source_get_source(wo_vm vm, wo_value args)
{
    auto& source = wo_component<jeecs::Audio::Source>(args + 0);
    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::audio::source>(source.source),
        nullptr,
        [](void* p) {delete (jeecs::basic::resource<jeecs::audio::source>*)p; });
}

WO_API wo_api wojeapi_towoo_userinterface_origin_layout(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);
    auto& origin = wo_component<jeecs::UserInterface::Origin>(args + 0);

    auto r = wo_vec2(args + 1);

    jeecs::math::vec2 abssize;
    jeecs::math::vec2 absoffset;
    jeecs::math::vec2 center_offset;

    origin.get_layout(r.x, r.y, &absoffset, &abssize, &center_offset);

    wo_set_struct(s + 0, vm, 3);

    wo_set_vec2(s + 1, vm, absoffset);
    wo_struct_set(s + 0, 0, s + 1);

    wo_set_vec2(s + 1, vm, abssize);
    wo_struct_set(s + 0, 1, s + 1);

    wo_set_vec2(s + 1, vm, center_offset);
    wo_struct_set(s + 0, 2, s + 1);

    return wo_ret_val(vm, s + 0);
}

WO_API wo_api wojeapi_towoo_userinterface_origin_mouse_on(wo_vm vm, wo_value args)
{
    auto& origin = wo_component<jeecs::UserInterface::Origin>(args + 0);

    auto r = wo_vec2(args + 1);
    auto a = wo_float(args + 2);
    auto m = wo_vec2(args + 3);

    return wo_ret_bool(vm, origin.mouse_on(r.x, r.y, a, m));
}
