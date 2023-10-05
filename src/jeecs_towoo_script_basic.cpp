#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <unordered_map>
#include <optional>
#include <memory>

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
                wo_integer_t m_function;
                std::vector<const typing::type_info*> m_used_components;
            };
            struct towoo_system_info
            {
                JECS_DISABLE_MOVE_AND_COPY(towoo_system_info);

                wo_vm m_base_vm;
                bool m_is_good;
                wo_integer_t m_create_function;
                wo_integer_t m_close_function;
                std::vector<towoo_step_work> m_works;

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
            std::vector<towoo_step_work> m_dependences;

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
                    m_dependences = base_info->m_works;

                    wo_value v = wo_invoke_rsfunc(m_job_vm, m_create_function, 0);
                    if (v == nullptr)
                    {
                        m_context = wo_push_empty(m_job_vm);
                        jeecs::debug::logerr("Failed to invoke 'create' function for system: '%s'.",
                            m_type->m_typename);
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
                wo_set_struct(writeval, vm, ctype->m_member_count);

                uint16_t member_idx = 0;
                auto* member_tinfo = ctype->m_member_types;
                while (member_tinfo != nullptr)
                {
                    // Set member;
                    _wo_value tmp;
                    wo_set_pointer(&tmp,
                        reinterpret_cast<void*>(
                            reinterpret_cast<intptr_t>(component)
                            + member_tinfo->m_member_offset));

                    wo_struct_set(writeval, member_idx, &tmp);

                    ++member_idx;
                    member_tinfo = member_tinfo->m_next_member;
                }
            }

            void Update()
            {
                ScriptRuntimeSystem::system_instance =
                    get_world().get_system<ScriptRuntimeSystem>();

                if (m_job_vm == nullptr)
                    return;

                wo_value tmp_elem = wo_push_empty(m_job_vm);

                for (auto& work : m_dependences)
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
                                    wo_invoke_rsfunc(m_job_vm, work.m_function, work.m_used_components.size() + 2);
                                }
                            }

                            // Update next chunk.
                            cur_chunk = je_arch_next_chunk(cur_chunk);
                        }
                    }
                }

                wo_pop_stack(m_job_vm);

                ScriptRuntimeSystem::system_instance = nullptr;
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
            ToWooBaseComponent(const ToWooBaseComponent&) = delete;
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
            + "    public let id = je::typeinfo::load(\"" + typeinfo->m_typename + "\")->val;\n"
            "}\n\n";
    }

    std::string woolang_component_type_decl =
        R"(// (C)Cinogama project.
import je;
import je::towoo::types;

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

            woolang_component_type_decl += "using " + tname + " = struct{\n";

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
bool je_towoo_register_system(
    const jeecs::typing::type_info** out_system_tinfo,
    const char* system_name,
    const char* script_path)
{
    if (jeecs_file* texfile = jeecs_file_open(script_path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        wo_vm vm = wo_create_vm();
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
                wo_close_vm(vm);
            }
            else if (create_function == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find 'create' function in '%s'.",
                    system_name, script_path);
                wo_close_vm(vm);
            }
            else if (close_function == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find 'close' in '%s'.",
                    system_name, script_path);
                wo_close_vm(vm);
            }
            else
            {
                if (nullptr == wo_run(vm))
                {
                    jeecs::debug::logerr("Failed to register: '%s', init failed: '%s'.",
                        system_name, wo_get_runtime_error(vm));
                    wo_close_vm(vm);
                }
                else
                {
                    je_towoo_unregister_system(je_typing_get_info_by_name(system_name));

                    auto* towoo_system_tinfo = je_typing_register(
                        system_name,
                        jeecs::basic::type_hash<jeecs::towoo::ToWooBaseSystem>(),
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

                    auto systinfo = std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(vm);
                    auto* sysinfo_ptr = systinfo.get();

                    sysinfo_ptr->m_is_good = false;
                    sysinfo_ptr->m_create_function = create_function;
                    sysinfo_ptr->m_close_function = close_function;

                    std::unique_lock ug1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

                    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems[towoo_system_tinfo]
                        = std::move(systinfo);

                    wo_push_pointer(vm, (void*)towoo_system_tinfo);
                    ug1.unlock();
                    if (nullptr == wo_invoke_rsfunc(vm, initfunc, 1))
                    {
                        // No need for locking
                        // ug1.lock();
                        jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                            system_name, wo_get_runtime_error(vm));
                    }
                    else
                    {
                        ug1.lock();
                        sysinfo_ptr->m_is_good = true;
                    }
                    return sysinfo_ptr->m_is_good;
                }
            }
        }
        else
        {
            jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                system_name, wo_get_compile_error(vm, WO_NEED_COLOR));
            wo_close_vm(vm);
        }
    }
    else
    {
        jeecs::debug::logerr("Failed to register: '%s' unable to open file '%s'.",
            system_name, script_path);
    }
    return false;
}

void jetowoo_finish()
{
    std::lock_guard g1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);
    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.clear();
}

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
    stepwork.m_function = wo_int(args + 1);

    wo_value requirements = args + 2;
    wo_integer_t component_arg_count = wo_int(args + 3);
    wo_value requirement_info = wo_push_empty(vm);
    wo_value elem = wo_push_empty(vm);

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
    works->m_works.push_back(stepwork);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_towoo_update_component_data(wo_vm vm, wo_value args, size_t argc)
{
    // wojeapi_towoo_register_component(name, [(name, typeinfo)])
    std::string component_name = wo_string(args + 0);

    auto* ty = je_typing_get_info_by_name(component_name.c_str());
    if (ty != nullptr)
    {
        if (ty->m_hash == jeecs::basic::type_hash<jeecs::towoo::ToWooBaseComponent>())
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
        jeecs::basic::type_hash<jeecs::towoo::ToWooBaseComponent>(),
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
        return;
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
            return;
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
    lexer->lex(result);
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
    public enum require_type
    {
        CONTAIN,        // Must have spcify component
        MAYNOT,         // May have or not have
        ANYOF,          // Must have one of 'ANYOF' components
        EXCEPT,         // Must not contain spcify component
    }

    using ToWooSystemFuncJob = struct
    {
        m_function: dynamic,
        m_requirement: vec<(require_type, int, je::typeinfo)>,
        m_argument_count: mut int,
        m_require_group: mut int,
    }
    {
        func create(f: dynamic)
        {
            return ToWooSystemFuncJob{
                m_function = f,
                m_requirement = []mut,
                m_argument_count = mut 0,
                m_require_group = mut 0,
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
        let j = ToWooSystemFuncJob::create(jobfunc: dynamic);
        regitered_works->add(j);
        return j;
    }
}

extern func _init_towoo_system(registering_system_type: je::typeinfo)
{
    using je::towoo::system;

    extern("libjoyecs", "wojeapi_towoo_register_system_job")
    func _register_towoo_system_job(
        registering_system_type: je::typeinfo,
        fn: dynamic, 
        req: array<(require_type, int, je::typeinfo)>,
        comp_count: int)=> void;

    for (let _, workinfo : regitered_works)
    {
        _register_towoo_system_job(
            registering_system_type,
            workinfo.m_function,
            workinfo.m_requirement->unsafe::cast:<array<(require_type, int, je::typeinfo)>>,
            workinfo.m_argument_count);
    }
}

#macro system!
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
        return;
    }

    if (lexer->next != "(")
    {
        lexer->error("Unexpected token, here should be '('.");
        return;
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
            return;
        }
        let type = read_component_type();
        if (type == "")
        {
            lexer->error("Missing 'type' here.");
            return;
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
            return;
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
    result += F";\nfunc {job_func_name}(context: typeof(create()), e: je::entity";
    for (let _, (argname, type, maynot) : arguments)
    {
        if (maynot)
            result += F", {argname}: option<{type}>";
        else
            result += F", {argname}: {type}";
    }
    result += "){do e; do context;\n";
    lexer->lex(result);
}
)";
