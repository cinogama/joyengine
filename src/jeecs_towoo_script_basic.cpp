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

设想中的代码：
ecs_system! MovementDemoSystem
{
  select! (
    localpos: Transform::LocalPosition,
    speed: MyScript::Speed?,
    jump: MyScript::Jump?,
  )
    anyof MyScript::Speed, MyScript::Jump;
    expected ;
    exclude ;
  {
    let speedv = speed->>\s=s.value->get();->or(10.);
    localpos.pos->set(localpos.pos->get + (speedv * deltatime(),0.,0.): vec3);
  }

  select! (
    localpos: Transform::LocalPosition,
    speed: MyScript::Speed?,
    jump: MyScript::Jump?,
  )
    anyof MyScript::Speed, MyScript::Jump;
    expected ;
    exclude ;
  {
    let speedv = speed->>\s=s.value->get();->or(10.);
    localpos.pos->set(localpos.pos->get + (speedv * deltatime(),0.,0.): vec3);
  }
}
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
                _wo_value m_function;
                std::vector<const typing::type_info*> m_used_components;
                bool m_is_single_work;
            };
            struct towoo_system_info
            {
                JECS_DISABLE_MOVE_AND_COPY(towoo_system_info);

                wo_vm m_base_vm;
                bool m_is_good;
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
            wo_value m_context;
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

                    m_pre_dependences = base_info->m_preworks;
                    m_dependences = base_info->m_works;
                    m_late_dependences = base_info->m_lateworks;

                    wo_push_pointer(m_job_vm, w.handle());
                    wo_value v = wo_invoke_rsfunc(m_job_vm, m_create_function, 1);
                    if (v == nullptr)
                    {
                        jeecs::debug::logerr("Failed to invoke 'create' function for system: '%s'.",
                            m_type->m_typename);
                        wo_release_vm(m_job_vm);
                        m_job_vm = nullptr;
                    }
                    else
                        m_context = wo_push_val(m_job_vm, v);
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
                    wo_push_val(m_job_vm, m_context);
                    wo_value v = wo_invoke_rsfunc(m_job_vm, m_close_function, 1);
                    if (v == nullptr)
                    {
                        jeecs::debug::logerr("Failed to invoke 'close' function for system: '%s'.",
                            m_type->m_typename);
                    }

                    wo_release_vm(m_job_vm);
                }
            }

            static void create_component_struct(wo_value writeval, wo_vm vm, void* component, const typing::type_info* ctype)
            {
                assert(component != nullptr);
                wo_set_struct(writeval, vm, ctype->m_member_count + 1);

                _wo_value tmp;
                wo_set_pointer(&tmp, component);
                wo_struct_set(writeval, 0, &tmp);

                uint16_t member_idx = 0;
                auto* member_tinfo = ctype->m_member_types;
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

            void update_step_work(std::vector<towoo_step_work>& works)
            {
                ScriptRuntimeSystem::system_instance =
                    get_world().get_system<ScriptRuntimeSystem>();

                if (m_job_vm == nullptr)
                    return;

                wo_value tmp_elem = wo_push_empty(m_job_vm);

                for (auto& work : works)
                {
                    if (work.m_is_single_work)
                    {
                        wo_push_val(m_job_vm, m_context);
                        // Invoke!
                        wo_invoke_value(m_job_vm, &work.m_function, 1);
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
                                        for (size_t argidx = work.m_used_components.size(); argidx > 0; --argidx)
                                        {
                                            void* component = selector::get_component_from_archchunk_ptr(archinfo, cur_chunk, eid, argidx - 1);
                                            const auto* typeinfo = work.m_used_components[argidx - 1];

                                            wo_value component_st = wo_push_empty(m_job_vm);
                                            if (work.m_dependence.m_requirements[argidx - 1].m_require == jeecs::requirement::type::MAYNOT)
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
                                            }
                                            else
                                            {
                                                create_component_struct(component_st, m_job_vm, component, typeinfo);
                                            }
                                        }

                                        // Push entity
                                        wo_push_gchandle(m_job_vm, new jeecs::game_entity{ cur_chunk , eid, version }, nullptr,
                                            [](void* eptr) {delete std::launder(reinterpret_cast<jeecs::game_entity*>(eptr)); });

                                        // Push context
                                        wo_push_val(m_job_vm, m_context);

                                        // Invoke!
                                        wo_invoke_value(m_job_vm, &work.m_function, work.m_used_components.size() + 2);
                                    }
                                }

                                // Update next chunk.
                                cur_chunk = je_arch_next_chunk(cur_chunk);
                            }
                        }
                    }
                }

                wo_pop_stack(m_job_vm);

                ScriptRuntimeSystem::system_instance = nullptr;
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
                auto* member = m_type->m_member_types;
                while (member != nullptr)
                {
                    auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                    member->m_member_type->construct(this_member, arg);
                    member = member->m_next_member;
                }
            }
            ~ToWooBaseComponent()
            {
                auto* member = m_type->m_member_types;
                while (member != nullptr)
                {
                    auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                    member->m_member_type->destruct(this_member);
                    member = member->m_next_member;
                }
            }
            ToWooBaseComponent(const ToWooBaseComponent& another)
                :m_type(another.m_type)
            {
                auto* member = m_type->m_member_types;
                while (member != nullptr)
                {
                    auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                    auto* other_member = (void*)((intptr_t)&another + member->m_member_offset);

                    member->m_member_type->copy(this_member, other_member);

                    member = member->m_next_member;
                }
            }
            ToWooBaseComponent(ToWooBaseComponent&& another)
                :m_type(another.m_type)
            {
                auto* member = m_type->m_member_types;
                while (member != nullptr)
                {
                    auto* this_member = (void*)((intptr_t)this + member->m_member_offset);
                    auto* other_member = (void*)((intptr_t)&another + member->m_member_offset);

                    member->m_member_type->move(this_member, other_member);

                    member = member->m_next_member;
                }
            }
        };
    }
}
WO_API wo_api wojeapi_towoo_add_component(wo_vm vm, wo_value args, size_t argc)
{
    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    void* comp = je_ecs_world_entity_add_component(e, ty);
    if (comp != nullptr)
    {
        wo_value val = wo_push_empty(vm);
        jeecs::towoo::ToWooBaseSystem::create_component_struct(val, vm, comp, ty);
        return wo_ret_option_val(vm, val);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_get_component(wo_vm vm, wo_value args, size_t argc)
{
    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    void* comp = je_ecs_world_entity_get_component(e, ty);
    if (comp != nullptr)
    {
        wo_value val = wo_push_empty(vm);
        jeecs::towoo::ToWooBaseSystem::create_component_struct(val, vm, comp, ty);
        return wo_ret_option_val(vm, val);
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_remove_component(wo_vm vm, wo_value args, size_t argc)
{
    auto e = std::launder(reinterpret_cast<jeecs::game_entity*>(wo_pointer(args + 0)));
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 1)));

    je_ecs_world_entity_remove_component(e, ty);
    return wo_ret_void(vm);
}
WO_API wo_api wojeapi_towoo_member_get(wo_vm vm, wo_value args, size_t argc)
{
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 0)));

    wo_value val = wo_push_empty(vm);

    assert(ty->m_script_parser_info != nullptr);
    ty->m_script_parser_info->m_script_parse_c2w(vm, val, wo_pointer(args + 1));

    return wo_ret_val(vm, val);
}
WO_API wo_api wojeapi_towoo_member_set(wo_vm vm, wo_value args, size_t argc)
{
    auto ty = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(wo_pointer(args + 0)));

    assert(ty->m_script_parser_info != nullptr);
    ty->m_script_parser_info->m_script_parse_w2c(vm, args + 2, wo_pointer(args + 1));

    return wo_ret_void(vm);
}

void je_towoo_update_api()
{
    // 1. 获取所有的BasicType，为这些类型生成对应的Woolang类型
    std::string woolang_parsing_type_decl =
        R"(// (C)Cinogama project.
namespace je::towoo
{
    public func tid<T>()
    {
        return typeof(std::declval:<T>())::id;
    }

    using member<T, IdT> = handle
    {
        public func get<T, IdT>(self: member<T, IdT>)=> T
        {
            extern("libjoyecs", "wojeapi_towoo_member_get")
            func member_get_impl<T, IdT>(type: je::typeinfo, self: member<T, IdT>)=> T;
        
            return member_get_impl(tid:<IdT>(), self);
        }
    
        public func set<T, IdT>(self: member<T, IdT>, val: T)=> void
        {
            extern("libjoyecs", "wojeapi_towoo_member_set")
            func member_set_impl<T, IdT>(type: je::typeinfo, self: member<T, IdT>, val: T)=> void;

            member_set_impl(tid:<IdT>(), self, val);
        }
    }
}

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
        auto* script_parser_info = typeinfo->m_script_parser_info;

        if (script_parser_info == nullptr
            || generated_types.find(
                script_parser_info->m_woolang_typename) != generated_types.end())
            continue;

        generated_types.insert(script_parser_info->m_woolang_typename);
        woolang_parsing_type_decl +=
            std::string("// Declear of '")
            + script_parser_info->m_woolang_typename + "'\n"
            + script_parser_info->m_woolang_typedecl + "\n"
            "using " + script_parser_info->m_woolang_typename + "_tid = void\n{\n"
            + "    public let id = je::typeinfo::load(\"" + script_parser_info->m_woolang_typename + "\")->val;\n"
            "}\n\n";
    }

    std::string woolang_component_type_decl =
        R"(// (C)Cinogama project.
import je;
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
                tnamespace = std::make_optional(tname.substr(0, index - 1));
                tname = tname.substr(index + 1);
            }

            woolang_component_type_decl += std::string("// Declear of '") + typeinfo->m_typename + "'\n";
            if (tnamespace)
                woolang_component_type_decl += "namespace " + tnamespace.value() + "{\n";

            woolang_component_type_decl += "using " + tname + " = struct{\n    __addr: handle,\n";

            auto* registed_member = typeinfo->m_member_types;
            while (registed_member != nullptr)
            {
                if (registed_member->m_member_type->m_script_parser_info != nullptr)
                {
                    // 对于有脚本对接类型的组件成员，在这里挂上！
                    woolang_component_type_decl +=
                        std::string("    ") + registed_member->m_member_name + ": je::towoo::member<"
                        + registed_member->m_member_type->m_script_parser_info->m_woolang_typename
                        + ", "
                        + registed_member->m_member_type->m_script_parser_info->m_woolang_typename
                        + "_tid>,\n";
                }
                registed_member = registed_member->m_next_member;
            }
            woolang_component_type_decl += "}\n{\n";

            // Generate ComponentT::id
            woolang_component_type_decl += "    public let id = je::typeinfo::load(\"";
            woolang_component_type_decl += typeinfo->m_typename;
            woolang_component_type_decl += "\")->val;\n";

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
    je_towoo_unregister_system(je_typing_get_info_by_name(system_name));

    auto* towoo_system_tinfo = je_typing_register(
        system_name,
        jeecs::basic::hash_compile_time(system_name),
        sizeof(jeecs::towoo::ToWooBaseSystem),
        alignof(jeecs::towoo::ToWooBaseSystem),
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::constructor,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::destructor,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::copier,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::mover,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::to_string,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::parse,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::state_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::pre_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::script_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::late_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::apply_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseSystem>::commit_update,
        je_typing_class::JE_SYSTEM);

    assert(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.find(towoo_system_tinfo) ==
        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.end());

    wo_vm vm = wo_create_vm();
    auto systinfo = std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(vm);
    systinfo->m_create_function = 0;
    systinfo->m_close_function = 0;
    auto* sysinfo_ptr = systinfo.get();

    do
    {
        std::lock_guard ug1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems[towoo_system_tinfo]
            = std::move(systinfo);
    } while (0);

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
            wo_integer_t create_function = wo_extern_symb(vm, "create");
            wo_integer_t close_function = wo_extern_symb(vm, "close");
            wo_integer_t initfunc = wo_extern_symb(vm, "_init_towoo_system");
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

                    wo_push_pointer(vm, (void*)towoo_system_tinfo);
                    if (nullptr == wo_invoke_rsfunc(vm, initfunc, 1))
                        // No need for locking
                        jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                            system_name, wo_get_runtime_error(vm));
                    else
                        sysinfo_ptr->m_is_good = true;

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
WO_API wo_api wojeapi_towoo_register_system_job(wo_vm vm, wo_value args, size_t argc)
{
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
    wo_value requirement_info = wo_push_empty(vm);
    wo_value elem = wo_push_empty(vm);

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
WO_API wo_api wojeapi_towoo_update_component_data(wo_vm vm, wo_value args, size_t argc)
{
    // wojeapi_towoo_register_component(name, [(name, typeinfo)])
    std::string component_name = wo_string(args + 0);

    auto* ty = je_typing_get_info_by_name(component_name.c_str());
    if (ty != nullptr)
    {
        if (ty->m_hash == jeecs::basic::hash_compile_time(("_towoo_component_" + component_name).c_str()))
            je_typing_unregister(ty);
        else
            return wo_ret_halt(vm, "Invalid towoo component name, cannot same as native-components.");
    }

    wo_value members = args + 1;
    wo_integer_t member_count = wo_lengthof(members);

    size_t component_size = sizeof(jeecs::towoo::ToWooBaseComponent);
    size_t component_allign = alignof(jeecs::towoo::ToWooBaseComponent);

    struct _member_info
    {
        std::string m_name;
        const jeecs::typing::type_info* m_type;
        size_t m_offset;
    };
    std::vector<_member_info> member_defs;
    wo_value member_def = wo_push_empty(vm);
    wo_value member_info = wo_push_empty(vm);
    for (wo_integer_t i = 0; i < member_count; ++i)
    {
        wo_arr_get(member_def, members, i);
        wo_struct_get(member_info, member_def, 0);
        std::string member_name = wo_string(member_info);

        wo_struct_get(member_info, member_def, 1);
        auto* member_typeinfo = (const jeecs::typing::type_info*)
            wo_pointer(member_info);

        component_size = jeecs::basic::allign_size(component_size, member_typeinfo->m_align);
        member_defs.push_back(_member_info{ member_name, member_typeinfo, component_size });

        component_size += member_typeinfo->m_chunk_size;
        component_allign = std::max(component_allign, member_typeinfo->m_chunk_size);
    }

    auto* towoo_component_tinfo = je_typing_register(
        component_name.c_str(),
        jeecs::basic::hash_compile_time(("_towoo_component_" + component_name).c_str()),
        component_size,
        component_allign,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::constructor,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::destructor,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::copier,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::mover,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::to_string,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::parse,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::state_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::pre_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::script_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::late_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::apply_update,
        jeecs::basic::default_functions<jeecs::towoo::ToWooBaseComponent>::commit_update,
        je_typing_class::JE_COMPONENT);

    for (auto& memberinfo : member_defs)
    {
        je_register_member(
            towoo_component_tinfo,
            memberinfo.m_type,
            memberinfo.m_name.c_str(),
            memberinfo.m_offset);
    }

    return wo_ret_pointer(vm, (void*)towoo_component_tinfo);
}

const char* jeecs_towoo_component_path = "je/towoo/component.wo";
const char* jeecs_towoo_component_src = R"(// (C)Cinogama. 
import je;
import je::towoo::types;

namespace je::towoo::component
{
    extern("libjoyecs", "wojeapi_towoo_update_component_data")
    public func update_component_declare(name: string, members: array<(string, typeinfo)>)=> typeinfo;

    let registered_member_infoms = {}mut: map<string, typeinfo>;
    public func register_member<T>(name: string)
    {
       registered_member_infoms->set(name, tid:<T>());
    }
}

extern func _init_towoo_component(name: string)
{
    return je::towoo::component::update_component_declare(
        name,
        je::towoo::component::registered_member_infoms
            ->unmapping,
    );
}

#macro component
{
    /*
    component!
    {
        member_name: type,
    }
    */

    if (lexer->next != "{")
    {
        lexer->error("Unexpected token, here should be '{'.");
        return "";
    }

    let decls = []mut: vec<(string, string)>;
    for (;;)
    {
        let name = lexer->next;
        if (name == "")
            lexer->error("Unexpected EOF.");
        else if (name == ",")
            continue;
        else if (name == "}")
            break;
        
        if (lexer->next != ":")
        {
            lexer->error("Unexpected token, here should be ':'.");
            return "";
        }
        let mut type = "";
        for (;;)
        {
            let ty = lexer->peek;
            if (ty == "," || ty == "}" || ty == "")
                break;
            type += lexer->next;
        }
        decls->add((name, type));
    }
    let mut result = ";";
    for (let _, (name, type) : decls)
    {
        result += F"je::towoo::component::register_member:<{type}_tid>({name->enstring});";
    }
    return result;
}
)";

const char* jeecs_towoo_system_path = "je/towoo/system.wo";
const char* jeecs_towoo_system_src = R"(// (C)Cinogama. 

import woo::std;

import je;
import je::towoo::types;
import je::towoo::components;

namespace je::towoo::system
{
    public enum job_type
    {
        PRE_UPDATE,
        UPDATE,
        LATE_UPDATE,
    };
    public enum require_type
    {
        CONTAIN,        // Must have spcify component
        MAYNOT,         // May have or not have
        ANYOF,          // Must have one of 'ANYOF' components
        EXCEPT,         // Must not contain spcify component
    }

    let mut _current_job_type = job_type::PRE_UPDATE;
    public func _set_current_job_type(t: job_type)
    {
        if (t >= _current_job_type)
            _current_job_type = t;
        else
        {
            let names = {
                [job_type::PRE_UPDATE] = "PRE_UPDATE",
                [job_type::UPDATE] = "UPDATE",
                [job_type::LATE_UPDATE] = "LATE_UPDATE",
            };
            je::logerr(F"The order of work is incorrect: {names[t]} must happend after {names[_current_job_type]}");
            std::halt("The order of work is incorrect.");
        }
    }

    using ToWooSystemFuncJob = struct
    {
        m_function: dynamic,
        m_job_type: job_type,
        m_requirement: vec<(require_type, int, je::typeinfo)>,
        m_argument_count: mut int,
        m_require_group: mut int,
        m_is_single_work: bool,
    }
    {
        func create(f: dynamic, iswork: bool)
        {
            return ToWooSystemFuncJob{
                m_function = f,
                m_job_type = _current_job_type,
                m_requirement = []mut,
                m_argument_count = mut 0,
                m_require_group = mut 0,
                m_is_single_work = iswork,
            };
        }
        public func contain<CompT>(self: ToWooSystemFuncJob, is_arg: bool)
        {
            self.m_requirement->add((require_type::CONTAIN, self.m_require_group, je::towoo::tid:<CompT>()));
            self.m_require_group += 1;
            if (is_arg)
                self.m_argument_count += 1;
            return self;
        }
        public func maynot<CompT>(self: ToWooSystemFuncJob)
        {
            self.m_requirement->add((require_type::MAYNOT, self.m_require_group, je::towoo::tid:<CompT>()));
            self.m_require_group += 1;
            self.m_argument_count += 1;
            return self;
        }
        public func except<CompT>(self: ToWooSystemFuncJob)
        {
            self.m_requirement->add((require_type::EXCEPT, self.m_require_group, je::towoo::tid:<CompT>()));
            self.m_require_group += 1;
            self.m_argument_count += 1;
            return self;
        }
        public func anyof(self: ToWooSystemFuncJob, ts: array<je::typeinfo>)
        {
            for (let _, t: ts)
                self.m_requirement->add((require_type::EXCEPT, self.m_require_group, t));
            self.m_require_group += 1;
            self.m_argument_count += 1;
            return self;
        }
    }

    let regitered_works = []mut: vec<ToWooSystemFuncJob>;

    public func register_job_function<FT>(jobfunc: FT)
        where std::declval:<nothing>() is typeof(jobfunc([]...));
    {
        let j = ToWooSystemFuncJob::create(jobfunc: dynamic, false);
        regitered_works->add(j);
        return j;
    }
    public func register_work_function<FT>(jobfunc: FT)
        where std::declval:<nothing>() is typeof(jobfunc([]...));
    {
        let j = ToWooSystemFuncJob::create(jobfunc: dynamic, true);
        regitered_works->add(j);
    }
}

extern func _init_towoo_system(registering_system_type: je::typeinfo)
{
    using je::towoo::system;

    extern("libjoyecs", "wojeapi_towoo_register_system_job")
    func _register_towoo_system_job(
        registering_system_type: je::typeinfo,
        fn: dynamic, 
        job_queue: job_type,
        req: array<(require_type, int, je::typeinfo)>,
        comp_count: int,
        is_single_work: bool)=> void;

    for (let _, workinfo : regitered_works)
    {
        _register_towoo_system_job(
            registering_system_type,
            workinfo.m_function,
            workinfo.m_job_type,
            workinfo.m_requirement->unsafe::cast:<array<(require_type, int, je::typeinfo)>>,
            workinfo.m_argument_count,
            workinfo.m_is_single_work);
    }
}
#macro PRE_UPDATE
{
    return "je::towoo::system::_set_current_job_type(je::towoo::system::job_type::PRE_UPDATE);";
}
#macro UPDATE
{
    return "je::towoo::system::_set_current_job_type(je::towoo::system::job_type::UPDATE);";
}
#macro LATE_UPDATE
{
    return "je::towoo::system::_set_current_job_type(je::towoo::system::job_type::LATE_UPDATE);";
}
#macro work
{
    /*
        work! JobName()
        {
            // BODY
        }
    */
    let job_func_name = lexer->next;
    if (job_func_name == "")
    {
        lexer->error("Unexpected EOF");
        return "";
    }

    if (lexer->next != "(")
    {
        lexer->error("Unexpected token, here should be '('.");
        return "";
    }
    if (lexer->next != "\x29")
    {
        lexer->error("Unexpected token, here should be ')'.");
        return "";
    }
    if (lexer->next != "{")
    {
        lexer->error("Unexpected token, here should be '{'.");
        return "";
    }

    let mut result = F"do je::towoo::system::register_work_function({job_func_name});";
    result += F"func {job_func_name}(context: typeof(create(std::declval:<je::world>())))\{do context;";

    return result;
}
#macro system
{
    /*
        system! JobName(
            localpos: Transform::LocalPosition
            color: Renderer::Color?
            textures: Renderer::Textures?
            )
            except Transform::LocalToParent, Transform::LocalToParent;
            anyof Renderer::Color, Renderer::Textures;
        {
            // BODY
        }
    */

    let job_func_name = lexer->next;
    if (job_func_name == "")
    {
        lexer->error("Unexpected EOF");
        return "";
    }

    if (lexer->next != "(")
    {
        lexer->error("Unexpected token, here should be '('.");
        return "";
    }

    let read_component_type = func()
        {
            let mut result = "";
            for (;;)
            {
                let token = lexer->peek;
                if (token == "\x29" || token == ";" || token == "," || token == "?" || token == "")
                    break;

                result += lexer->next;
            }
            return result;
        };

    let arguments = []mut: vec<(string, string, /*may not?*/bool)>;
    // Read arguments here.
    for (;;)
    {
        let name = lexer->next;
        if (name == ",")
            continue;
        else if (name == "\x29" || name == "")
            break;
        
        if (lexer->next != ":")
        {
            lexer->error("Unexpected token, here should be ':'.");
            return "";
        }
        let type = read_component_type();
        if (type == "")
        {
            lexer->error("Missing 'type' here.");
            return "";
        }

        let maynot = lexer->peek == "?";
        if (maynot)
            do lexer->next;
        arguments->add((name, type, maynot));
    }

    // Read contain, except, anyof
    let requirements = struct{
        contain = []mut: vec<string>,
        except = []mut: vec<string>,
        anyof = []mut: vec<array<string>>,
    };

    let read_type_list = func()
        {
            let types = []mut: vec<string>;
            for (;;)
            {
                let t = read_component_type();
                if (t != "")
                    types->add(t);

                let token = lexer->next;
                if (token == ",")
                    continue;
                else if (token == ";" || token == "")
                    break;
            }
            return types;
        };

    for (;;)
    {
        let require = lexer->next;
        if (require == "{")
            break;
        else if (require == "contain")
        {
            for (let _, t : read_type_list())
                requirements.contain->add(t);
        }
        else if (require == "except")
        {
            for (let _, t : read_type_list())
                requirements.except->add(t);
        }
        else if (require == "anyof")
        {
            requirements.anyof->add(read_type_list() as vec<string>->unsafe::cast :<array<string>>);
        }
        else
        {
            lexer->error("Unexpected token, here should be 'contain', 'except', 'anyof' or '{'.");
            return "";
        }
    }

    // OK Generate!
    let mut result = F"do je::towoo::system::register_job_function({job_func_name}\x29";
    for (let _, (_, type, maynot) : arguments)
    {
        if (maynot)
            result += F"->maynot:<{type}>(\x29";
        else
            result += F"->contain:<{type}>(true\x29";
    }
    for (let _, type : requirements.contain)
        result += F"->contain:<{type}>(false\x29";
    for (let _, type : requirements.except)
        result += F"->except:<{type}>(\x29";
    for (let _, req : requirements.anyof)
    {
        result += F"->anyof([";
        for (let _, t : req)
            result += F"je::towoo::tid:<{t}>(),";
        result += "]\x29";
    }
    result += F";\nfunc {job_func_name}(context: typeof(create(std::declval:<je::world>())), e: je::entity";
    for (let _, (argname, type, maynot) : arguments)
    {
        if (maynot)
            result += F", {argname}: option<{type}>";
        else
            result += F", {argname}: {type}";
    }
    result += "){do e; do context;\n";
    return result;
}
)";

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

wo_value wo_push_vec2(wo_vm vm, const jeecs::math::vec2& v)
{
    wo_value result = wo_push_empty(vm);
    wo_set_vec2(result, vm, v);
    return result;
}
wo_value wo_push_vec3(wo_vm vm, const jeecs::math::vec3& v)
{
    wo_value result = wo_push_empty(vm);
    wo_set_vec3(result, vm, v);
    return result;
}
wo_value wo_push_vec4(wo_vm vm, const jeecs::math::vec4& v)
{
    wo_value result = wo_push_empty(vm);
    wo_set_vec4(result, vm, v);
    return result;
}
wo_value wo_push_quat(wo_vm vm, const jeecs::math::quat& v)
{
    wo_value result = wo_push_empty(vm);
    wo_set_quat(result, vm, v);
    return result;
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
        return std::launder(reinterpret_cast<T*>(
            wo_pointer(&tmp)));
    return nullptr;
}

WO_API wo_api wojeapi_towoo_ray_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new jeecs::math::ray(wo_vec3(args + 0), wo_vec3(args + 1)),
        nullptr,
        [](void* p) {delete(jeecs::math::ray*)p; });
}
WO_API wo_api wojeapi_towoo_ray_from_camera(wo_vm vm, wo_value args, size_t argc)
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
WO_API wo_api wojeapi_towoo_ray_intersect_entity(wo_vm vm, wo_value args, size_t argc)
{
    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);
    auto result = ray->intersect_entity(
        wo_component<jeecs::Transform::Translation>(args + 1),
        wo_option_component<jeecs::Renderer::Shape>(args + 2),
        wo_float(args + 3));
    if (result.intersected)
    {
        return wo_ret_option_val(vm, wo_push_vec3(vm, result.place));
    }
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_towoo_ray_origin(wo_vm vm, wo_value args, size_t argc)
{
    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);
    return wo_ret_val(vm, wo_push_vec3(vm, ray->orgin));
}
WO_API wo_api wojeapi_towoo_ray_direction(wo_vm vm, wo_value args, size_t argc)
{
    auto* ray = (jeecs::math::ray*)wo_pointer(args + 0);
    return wo_ret_val(vm, wo_push_vec3(vm, ray->direction));
}
WO_API wo_api wojeapi_towoo_math_sqrt(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, sqrt(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_sin(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, sin(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_cos(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, cos(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_tan(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, tan(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_asin(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, asin(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_acos(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, acos(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_atan(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, atan(wo_real(args + 0)));
}
WO_API wo_api wojeapi_towoo_math_atan2(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_real(vm, atan2(wo_real(args + 0), wo_real(args + 1)));
}
WO_API wo_api wojeapi_towoo_math_quat_slerp(wo_vm vm, wo_value args, size_t argc)
{
    wo_value q = wo_push_quat(vm,
        jeecs::math::quat::slerp(
            wo_quat(args + 0),
            wo_quat(args + 1),
            wo_float(args + 2))
    );
    return wo_ret_val(vm, q);
}

WO_API wo_api wojeapi_towoo_physics2d_collisionresult_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* collisionResult = (jeecs::Physics2D::CollisionResult*)wo_pointer(c);
   
    wo_set_map(c, vm);
    auto key = wo_push_empty(vm);
    auto val = wo_push_empty(vm);
    for (auto&[rigidbody, result] : collisionResult->results)
    {
        jeecs::towoo::ToWooBaseSystem::create_component_struct(
            key, vm, rigidbody, jeecs::typing::type_info::of<jeecs::Physics2D::Rigidbody>());
        wo_set_struct(val, vm, 2);

        wo_map_set(c, key, val);

        wo_set_vec2(key, vm, result.position);
        wo_struct_set(val, 0, key);
        wo_set_vec2(key, vm, result.normalize);
        wo_struct_set(val, 1, key);
    }

    return wo_ret_val(vm, c);
}

WO_API wo_api wojeapi_towoo_physics2d_collisionresult_check(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* collisionResult = (jeecs::Physics2D::CollisionResult*)wo_pointer(c);

    wo_struct_get(c, args + 1, 0);
    auto* rigidbody = (jeecs::Physics2D::Rigidbody*)wo_pointer(c);

    auto* result = collisionResult->check(rigidbody);
    if (result != nullptr)
    {
        wo_value ret = wo_push_struct(vm, 2);
        wo_struct_set(ret, 0, wo_push_vec2(vm, result->position));
        wo_struct_set(ret, 1, wo_push_vec2(vm, result->normalize));

        return wo_ret_option_val(vm, ret);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_towoo_renderer_textures_bind_texture(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* textures = (jeecs::Renderer::Textures*)wo_pointer(c);
    size_t pass = (size_t)wo_int(args + 1);
    auto* tex = (jeecs::basic::resource<jeecs::graphic::texture>*)wo_pointer(args + 2);

    textures->bind_texture(pass, *tex);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_renderer_textures_get_texture(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* textures = (jeecs::Renderer::Textures*)wo_pointer(c);
    size_t pass = (size_t)wo_int(args + 1);

    auto tex = textures->get_texture(pass);

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

WO_API wo_api wojeapi_towoo_renderer_shaders_set_uniform(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* shaders = (jeecs::Renderer::Shaders*)wo_pointer(c);
    const char* name = wo_string(args + 1);
    wo_value val = args + 2;

    auto valtype = wo_valuetype(val);
    switch (valtype)
    {
    case WO_INTEGER_TYPE:
        shaders->set_uniform(name, (int)wo_int(val)); break;
    case WO_REAL_TYPE:
        shaders->set_uniform(name, wo_float(val)); break;
    case WO_STRUCT_TYPE:
    {
        wo_value v = wo_push_empty(vm);
        switch (wo_lengthof(val))
        {
        case 2:
        {
            jeecs::math::vec2 v2;
            wo_struct_get(v, val, 0);
            v2.x = wo_float(v);
            wo_struct_get(v, val, 1);
            v2.y = wo_float(v);
            shaders->set_uniform(name, v2);
            break;
        }
        case 3:
        {
            jeecs::math::vec3 v3;
            wo_struct_get(v, val, 0);
            v3.x = wo_float(v);
            wo_struct_get(v, val, 1);
            v3.y = wo_float(v);
            wo_struct_get(v, val, 2);
            v3.z = wo_float(v);
            shaders->set_uniform(name, v3);
            break;
        }
        case 4:
        {
            jeecs::math::vec4 v4;
            wo_struct_get(v, val, 0);
            v4.x = wo_float(v);
            wo_struct_get(v, val, 1);
            v4.y = wo_float(v);
            wo_struct_get(v, val, 2);
            v4.z = wo_float(v);
            wo_struct_get(v, val, 3);
            v4.w = wo_float(v);
            shaders->set_uniform(name, v4);
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

WO_API wo_api wojeapi_towoo_transform_translation_global_pos(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* trans = (jeecs::Transform::Translation*)wo_pointer(c);

    return wo_ret_val(vm, wo_push_vec3(vm, trans->world_position));
}
WO_API wo_api wojeapi_towoo_transform_translation_global_rot(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* trans = (jeecs::Transform::Translation*)wo_pointer(c);

    return wo_ret_val(vm, wo_push_quat(vm, trans->world_rotation));
}

WO_API wo_api wojeapi_towoo_animation2D_frameanimation_active_animation(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* anim = (jeecs::Animation2D::FrameAnimation*)wo_pointer(c);

    anim->animations.active_action(
        (size_t)wo_int(args + 1), wo_string(args + 2), wo_bool(args + 3));

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_audio_playing_set_buffer(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* playing = (jeecs::Audio::Playing*)wo_pointer(c);

    auto* buf = (jeecs::basic::resource<jeecs::audio::buffer>*)wo_pointer(args + 1);
    playing->set_buffer(*buf);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_audio_source_get_source(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* playing = (jeecs::Audio::Source*)wo_pointer(c);

    return wo_ret_gchandle(vm,
        new jeecs::basic::resource<jeecs::audio::source>(playing->source),
        nullptr,
        [](void* p) {delete (jeecs::basic::resource<jeecs::audio::source>*)p; });
}

WO_API wo_api wojeapi_towoo_userinterface_origin_layout(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* origin = (jeecs::UserInterface::Origin*)wo_pointer(c);

    auto r = wo_vec2(args + 1);
    auto range = origin->get_layout(r.x, r.y);

    auto result = wo_push_struct(vm, 2);
    wo_struct_set(result, 0, wo_push_vec2(vm, jeecs::math::vec2(range.x, range.y)));
    wo_struct_set(result, 1, wo_push_vec2(vm, jeecs::math::vec2(range.z, range.w)));

    return wo_ret_val(vm, result);
}

/*
    namespace LocalPosition
    {
        extern("libjoyecs", "wojeapi_towoo_transform_localposition_get_parent_global_pos")
            public func get_parent_global_position(self: LocalPosition, trans: Translation, rot: option<LocalRotation>)=> vec3;

        extern("libjoyecs", "wojeapi_towoo_transform_localposition_set_global_pos")
            public func set_global_position(self: LocalPosition, pos: vec3, trans: Translation, rot: option<LocalRotation>)=> void;
    }
    namespace LocalRotation
    {
        extern("libjoyecs", "wojeapi_towoo_transform_localrotation_get_parent_global_rot")
            public func get_parent_global_rotation(self: LocalRotation, trans: Translation)=> quat;

        extern("libjoyecs", "wojeapi_towoo_transform_localrotation_set_global_rot")
            public func set_global_position(self: LocalRotation, rot: quat, trans: Translation)=> void;
    }*/

WO_API wo_api wojeapi_towoo_transform_localrotation_get_parent_global_rot(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* lrot = (jeecs::Transform::LocalRotation*)wo_pointer(c);

    wo_struct_get(c, args + 1, 0);
    auto* translation = (jeecs::Transform::Translation*)wo_pointer(c);

    return wo_ret_val(vm,
        wo_push_quat(vm, lrot->get_parent_global_rotation(*translation)));
}

WO_API wo_api wojeapi_towoo_transform_localrotation_set_global_rot(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* lrot = (jeecs::Transform::LocalRotation*)wo_pointer(c);

    auto rot = wo_quat(args + 1);

    wo_struct_get(c, args + 2, 0);
    auto* translation = (jeecs::Transform::Translation*)wo_pointer(c);

    lrot->set_global_rotation(rot, *translation);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_transform_localposition_get_parent_global_pos(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* lpos = (jeecs::Transform::LocalPosition*)wo_pointer(c);

    wo_struct_get(c, args + 1, 0);
    auto* translation = (jeecs::Transform::Translation*)wo_pointer(c);

    wo_struct_get(c, args + 2, 0);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(c);

    return wo_ret_val(vm, 
        wo_push_vec3(vm, lpos->get_parent_global_position(*translation, lrot)));
}
WO_API wo_api wojeapi_towoo_transform_localposition_set_global_pos(wo_vm vm, wo_value args, size_t argc)
{
    wo_value c = wo_push_empty(vm);
    wo_struct_get(c, args + 0, 0);
    auto* lpos = (jeecs::Transform::LocalPosition*)wo_pointer(c);

    auto pos = wo_vec3(args + 1);

    wo_struct_get(c, args + 2, 0);
    auto* translation = (jeecs::Transform::Translation*)wo_pointer(c);

    wo_struct_get(c, args + 3, 0);
    auto* lrot = wo_option_component<jeecs::Transform::LocalRotation>(c);

    lpos->set_global_position(pos, *translation, lrot);
    return wo_ret_void(vm);
}

const char* jeecs_towoo_path = "je/towoo.wo";
const char* jeecs_towoo_src = R"(// (C)Cinogama. 
import je::towoo::types;
import je::towoo::components;

namespace je::entity::towoo
{
    extern("libjoyecs", "wojeapi_towoo_add_component")
        private func _add_component<T>(self: entity, tid: je::typeinfo)=> option<T>;
    extern("libjoyecs", "wojeapi_towoo_get_component")
        private func _get_component<T>(self: entity, tid: je::typeinfo)=> option<T>;
    extern("libjoyecs", "wojeapi_towoo_remove_component")
        private func _remove_component<T>(self: entity, tid: je::typeinfo)=> void;

    public func add_component<T>(self: entity)=> option<T>
        where typeof(std::declval:<T>())::id is je::typeinfo;
    {
        return _add_component:<T>(self, typeof(std::declval:<T>())::id);
    }
    public func get_component<T>(self: entity)=> option<T>
        where typeof(std::declval:<T>())::id is je::typeinfo;
    {
        return _get_component:<T>(self, typeof(std::declval:<T>())::id);
    }
    public func remove_component<T>(self: entity)=> void
        where typeof(std::declval:<T>())::id is je::typeinfo;
    {
        _remove_component:<T>(self, typeof(std::declval:<T>())::id);
    }
}

namespace je::mathf
{
    extern("libjoyecs", "wojeapi_towoo_math_sqrt")
        public func sqrt(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_sin")
        public func sin(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_cos")
        public func cos(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_tan")
        public func tan(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_asin")
        public func asin(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_acos")
        public func acos(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_atan")
        public func atan(v: real)=> real;

    extern("libjoyecs", "wojeapi_towoo_math_atan2")
        public func atan2(y: real, x: real)=> real;

    public func abs<T>(a: T)
        where a is int || a is real;
    {
        if (a < 0: T)
            return -a;
        return a;
    }

    public func sign(a: real)
    {
        if (a < 0.)
            return -1.;
        else if(a > 0.)
            return 1.;
        return 0.;
    }

    public func lerp<T>(a: T, b: T, deg: real)
        where !((a * deg) is pending);
    {
        return a * (1. - deg) + b * deg;
    }

    public func clamp<T>(a: T, min: T, max: T)
        where !((a < a) is pending);
    {
        if (a < min)
            return min;
        if (max < a)
            return max;
        return a;
    }

    public let PI = 3.14159265359;
    public let RAD2DEG = 180. / PI;
    public let DEG2RAD = PI / 180.;
}
namespace vec2
{
    public func operator + (a: vec2, b: vec2)
    {
        return (a[0] + b[0], a[1] + b[1]): vec2;
    }
    public func operator - (a: vec2, b: vec2)
    {
        return (a[0] - b[0], a[1] - b[1]): vec2;
    }
    public func operator * (a: vec2, b)
        where b is vec2 || b is real;
    {
        if (b is vec2)
            return (a[0] * b[0], a[1] * b[1]): vec2;
        else
            return (a[0] * b, a[1] * b): vec2;
    }
    public func operator / (a: vec2, b)
        where b is vec2 || b is real;
    {
        if (b is vec2)
            return (a[0] / b[0], a[1] / b[1]): vec2;
        else
            return (a[0] / b, a[1] / b): vec2;
    }
    public func length(self: vec2)
    {
        let (x, y) = self;
        return je::mathf::sqrt(x*x + y*y);
    }
    public func unit(self: vec2)
    {
        let (x, y) = self;
        let length = self->length;
        if (length == 0.)
            return self;
        return (x / length, y / length): vec2;
    }
    public func dot(self: vec2, b: vec2)
    {
        return self[0] * b[0] + self[1] * b[1];
    }
}
namespace vec3
{
    public func operator + (a: vec3, b: vec3)
    {
        return (a[0] + b[0], a[1] + b[1], a[2] + b[2]): vec3;
    }
    public func operator - (a: vec3, b: vec3)
    {
        return (a[0] - b[0], a[1] - b[1], a[2] - b[2]): vec3;
    }
    public func operator * (a: vec3, b)
        where b is vec3 || b is real;
    {
        if (b is vec3)
            return (a[0] * b[0], a[1] * b[1], a[2] * b[2]): vec3;
        else
            return (a[0] * b, a[1] * b, a[2] * b): vec3;
    }
    public func operator / (a: vec3, b)
        where b is vec3 || b is real;
    {
        if (b is vec3)
            return (a[0] / b[0], a[1] / b[1], a[2] / b[2]): vec3;
        else
            return (a[0] / b, a[1] / b, a[2] / b): vec3;
    }

    public func length(self: vec3)
    {
        let (x, y, z) = self;
        return je::mathf::sqrt(x*x + y*y + z*z);
    }
    public func unit(self: vec3)
    {
        let (x, y, z) = self;
        let length = self->length;
        if (length == 0.)
            return self;
        return (x/length, y/length, z/length): vec3;
    }
    public func dot(self: vec3, b: vec3)
    {
        return self[0] * b[0] + self[1] * b[1] + self[2] * b[2];
    }
    public func cross(self: vec3, b: vec3)
    {
        return (
            self[1] * b[2] - self[2] * b[1],
            self[2] * b[0] - self[0] * b[2],
            self[0] * b[1] - self[1] * b[0]
        ): vec3;
    }
}
namespace vec4
{
    public func operator + (a: vec4, b: vec4)
    {
        return (a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]): vec4;
    }
    public func operator - (a: vec4, b: vec4)
    {
        return (a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]): vec4;
    }
    public func operator * (a: vec4, b)
        where b is vec4 || b is real;
    {
        if (b is vec4)
            return (a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]): vec4;
        else
            return (a[0] * b, a[1] * b, a[2] * b, a[3] * b): vec4;
    }
    public func operator / (a: vec4, b)
        where b is vec4 || b is real;
    {
        if (b is vec4)
            return (a[0] / b[0], a[1] / b[1], a[2] / b[2], a[3] / b[3]): vec4;
        else
            return (a[0] / b, a[1] / b, a[2] / b, a[3] / b): vec4;
    }

    public func length(self: vec4)
    {
        let (x, y, z, w) = self;
        return je::mathf::sqrt(x*x + y*y + z*z + w*w);
    }
    public func unit(self: vec4)
    {
        let (x, y, z, w) = self;
        let length = self->length;
        if (length == 0.)
            return self;
        return (x/length, y/length, z/length, w/length): vec4;
    }
    public func dot(self: vec4, b: vec4)
    {
        return self[0] * b[0] + self[1] * b[1] + self[2] * b[2] + self[3] * b[3];
    }
}
namespace ivec2
{
    public func operator + (a: ivec2, b: ivec2)
    {
        return (a[0] + b[0], a[1] + b[1]): ivec2;
    }
    public func operator - (a: ivec2, b: ivec2)
    {
        return (a[0] - b[0], a[1] - b[1]): ivec2;
    }
    public func operator * (a: ivec2, b)
        where b is ivec2 || b is int;
    {
        if (b is ivec2)
            return (a[0] * b[0], a[1] * b[1]): ivec2;
        else
            return (a[0] * b, a[1] * b): ivec2;
    }
    public func operator / (a: ivec2, b)
        where b is ivec2 || b is int;
    {
        if (b is ivec2)
            return (a[0] / b[0], a[1] / b[1]): ivec2;
        else
            return (a[0] / b, a[1] / b): ivec2;
    }
}
namespace quat
{
    public func euler(yaw: real, pitch: real, roll: real)
    {
        let yangle = 0.5 * yaw * je::mathf::DEG2RAD;
        let pangle = 0.5 * pitch * je::mathf::DEG2RAD;
        let rangle = 0.5 * roll * je::mathf::DEG2RAD;

        let ysin = je::mathf::sin(yangle),
            ycos = je::mathf::cos(yangle),
            psin = je::mathf::sin(pangle),
            pcos = je::mathf::cos(pangle),
            rsin = je::mathf::sin(rangle),
            rcos = je::mathf::cos(rangle);

        let y = rcos * psin * ycos + rsin * pcos * ysin,
            x = rcos * pcos * ysin - rsin * psin * ycos,
            z = rsin * pcos * ycos - rcos * psin * ysin,
            w = rcos * pcos * ycos + rsin * psin * ysin;
        let mag = x*x + y*y + z*z + w*w;
        return (x/mag, y/mag, z/mag, w/mag): quat;
    }
    public func axis(a: vec3, ang: real)
    {
        let sv = je::mathf::sin(ang * 0.5 * je::mathf::DEG2RAD);
        let cv = je::mathf::cos(ang * 0.5 * je::mathf::DEG2RAD);
        return (a[0] * sv, a[1] * sv, a[2] * sv, cv): quat;
    }
    public func rotation(a: vec3, b: vec3)
    {
        let axi = b->cross(a);
        let angle = je::mathf::RAD2DEG * je::mathf::acos(b->dot(a) / (b->length * a->length));
        return axis(axi->unit, angle);
    }
    public func delta_angle(a: quat, b: quat)
    {
        let cos_theta = a->dot(b);
        if (cos_theta > 0.)
            return 2. * je::mathf::RAD2DEG * je::mathf::acos(cos_theta);
        else
            return -2. * je::mathf::RAD2DEG * je::mathf::acos(-cos_theta);
    }
    public func dot(self: quat, b: quat)
    {
        return self[0] * b[0] + self[1] * b[1] + self[2] * b[2] + self[3] * b[3];
    }
    public func inverse(self: quat)
    {
        return (-self[0], -self[1], -self[2], self[3]): quat;
    }
    public func euler_angle(self: quat)
    {
        let (x, y, z, w) = self;
        let yaw = je::mathf::atan2(2. * (w * x + z * y), 1. - 2. * (x * x + y * y));
        let pitch = je::mathf::asin(je::mathf::clamp(2. * (w * y - x * z), -1., 1.));
        let roll = je::mathf::atan2(2. * (w * z + x * y), 1. - 2. * (z * z + y * y));
        return (je::mathf::RAD2DEG * yaw, je::mathf::RAD2DEG * pitch, je::mathf::RAD2DEG * roll): vec3;
    }
    public func lerp(a: quat, b: quat, t: real)
    {
        return (
            je::mathf::lerp(a[0], b[0], t),
            je::mathf::lerp(a[1], b[1], t),
            je::mathf::lerp(a[2], b[2], t),
            je::mathf::lerp(a[3], b[3], t),
        ): quat;
    }
    
    extern("libjoyecs", "wojeapi_towoo_math_quat_slerp")
    public func slerp(a: quat, b: quat, t: real)=> quat;

    public func operator * (self: quat, b)
        where b is vec3 || b is quat;
    {
        if (b is vec3)
        {
            let w = self[3];
            let u = (self[0], self[1], self[2]): vec3;
            return u * (2. * u->dot(b))
                + b * (w * w - u->dot(u))
                + u->cross(b) * (2. * w);
        }
        else
        {
            let (x1, y1, z1, w1) = self;
            let (x2, y2, z2, w2) = b;
            
            let v1 = (x1, y1, z1): vec3;
            let v2 = (x2, y2, z2): vec3;

            let w3 = w1 * w2 - v1->dot(v2);
            let v3 = v1->cross(v2) + v2 * w1 + v1 * w2;
            return (v3[0], v3[1], v3[2], w3): quat;
        }
    }
}
namespace je::towoo
{
    using ray = gchandle
    {
        extern("libjoyecs", "wojeapi_towoo_ray_create")
        public func create(ori: vec3, dir: vec3)=> ray;

        extern("libjoyecs", "wojeapi_towoo_ray_from_camera")
        public func from(trans: Transform::Translation, proj: Camera::Projection, screen_pos: vec2, orth: bool)=> ray;

        extern("libjoyecs", "wojeapi_towoo_ray_intersect_entity")
        public func intersect(self: ray, trans: Transform::Translation, shap: option<Renderer::Shape>, inrange: real)=> option<vec3>;

        extern("libjoyecs", "wojeapi_towoo_ray_origin")
        public func origin(self: ray)=> vec3;

        extern("libjoyecs", "wojeapi_towoo_ray_direction")
        public func direction(self: ray)=> vec3;
    }
}
namespace Transform
{
    namespace Translation
    {
        extern("libjoyecs", "wojeapi_towoo_transform_translation_global_pos")
            public func get_global_pos(self: Translation)=> vec3;

        extern("libjoyecs", "wojeapi_towoo_transform_translation_global_rot")
            public func get_global_rot(self: Translation)=> quat;
    }
    namespace LocalPosition
    {
        extern("libjoyecs", "wojeapi_towoo_transform_localposition_get_parent_global_pos")
            public func get_parent_global_position(self: LocalPosition, trans: Translation, rot: option<LocalRotation>)=> vec3;

        extern("libjoyecs", "wojeapi_towoo_transform_localposition_set_global_pos")
            public func set_global_position(self: LocalPosition, pos: vec3, trans: Translation, rot: option<LocalRotation>)=> void;
    }
    namespace LocalRotation
    {
        extern("libjoyecs", "wojeapi_towoo_transform_localrotation_get_parent_global_rot")
            public func get_parent_global_rotation(self: LocalRotation, trans: Translation)=> quat;

        extern("libjoyecs", "wojeapi_towoo_transform_localrotation_set_global_rot")
            public func set_global_position(self: LocalRotation, rot: quat, trans: Translation)=> void;
    }
}
namespace UserInterface
{
    namespace Origin
    {
        extern("libjoyecs", "wojeapi_towoo_userinterface_origin_layout")
            public func get_layout(self: Origin, display_range: vec2)=> (vec2, vec2);
    }
}
namespace Renderer
{
    namespace Textures
    {
        extern("libjoyecs", "wojeapi_towoo_renderer_textures_bind_texture")
            public func bind_texture(self: Textures, pass: int, text: je::graphic::texture)=> void;

        extern("libjoyecs", "wojeapi_towoo_renderer_textures_get_texture")
            public func get_texture(self: Textures, pass: int)=> option<je::graphic::texture>;
    }
    namespace Shaders
    {
        extern("libjoyecs", "wojeapi_towoo_renderer_shaders_set_uniform")
            public func set_uniform<T>(self: Shaders, name: string, val: T)=> void
                where val is int
                    || val is real
                    || val is vec2
                    || val is vec3
                    || val is vec4;
    }
}
namespace Physics2D
{
    namespace CollisionResult
    {
        using collide_result = struct{position: vec2, normalize: vec2};

        extern("libjoyecs", "wojeapi_towoo_physics2d_collisionresult_all")
            public func all(self: CollisionResult)=> dict<Rigidbody, collide_result>;

        extern("libjoyecs", "wojeapi_towoo_physics2d_collisionresult_check")
            public func check(self: CollisionResult, another: Rigidbody)=> option<collide_result>;
    }
}
namespace Animation2D
{
    namespace FrameAnimation
    {
        extern("libjoyecs", "wojeapi_towoo_animation2D_frameanimation_active_animation")
            public func active_action(self: FrameAnimation, animation_id: int, name: string, loop: bool)=> void;
    }
}
namespace Audio
{
    namespace Source
    {
        extern("libjoyecs", "wojeapi_towoo_audio_source_get_source")
            public func set_buffer(self: Source)=> je::audio::source;
    }
    namespace Playing
    {
        extern("libjoyecs", "wojeapi_towoo_audio_playing_set_buffer")
            public func set_buffer(self: Playing, buf: je::audio::buffer)=> void;
    }
}
)";
