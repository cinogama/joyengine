#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <unordered_map>
#include <optional>
#include <memory>

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

            // 执行方法使用的需求器和对应的执行逻辑
            std::vector<towoo_step_work> m_dependences;

            JECS_DISABLE_MOVE_AND_COPY(ToWooBaseSystem);

            ToWooBaseSystem(game_world w, const jeecs::typing::type_info* ty)
                : game_system(w)
            {
                std::shared_lock sg1(_registered_towoo_base_systems_mx);

                auto& base_info = _registered_towoo_base_systems.at(ty);
                m_job_vm = wo_borrow_vm(base_info->m_base_vm);
                m_dependences = base_info->m_works;



            }
            ~ToWooBaseSystem()
            {
                wo_release_vm(m_job_vm);
            }

            void Update()
            {
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

                                        wo_value component_st = nullptr;
                                        if (work.m_dependence.m_requirements[argidx - 1].m_require == jeecs::requirement::type::MAYNOT)
                                        {
                                            wo_value option_comp = wo_push_struct(m_job_vm, 2);
                                            if (component == nullptr)
                                                // option::none
                                                wo_set_int(wo_struct_get(option_comp, 0), 2);
                                            else
                                            {
                                                // option::value
                                                wo_set_int(wo_struct_get(option_comp, 0), 1);
                                                component_st = wo_struct_get(option_comp, 1);
                                            }
                                        }
                                        else
                                            component_st = wo_push_empty(m_job_vm);

                                        if (component_st != nullptr)
                                        {
                                            wo_set_struct(component_st, typeinfo->m_member_count);

                                            uint16_t member_idx = 0;
                                            auto* member_tinfo = typeinfo->m_member_types;
                                            while (member_tinfo != nullptr)
                                            {
                                                // Set member;

                                                wo_set_pointer(wo_struct_get(component_st, member_idx),
                                                    reinterpret_cast<void*>(
                                                        reinterpret_cast<intptr_t>(component)
                                                        + member_tinfo->m_member_offset));

                                                ++member_idx;
                                                member_tinfo = member_tinfo->m_next_member;
                                            }
                                        }

                                        // Push entity
                                        wo_push_gchandle(m_job_vm, new jeecs::game_entity{ cur_chunk , eid, version }, nullptr,
                                            [](void* eptr) {delete std::launder(reinterpret_cast<jeecs::game_entity*>(eptr)); });

                                        // Invoke!
                                        wo_invoke_rsfunc(m_job_vm, work.m_function, work.m_used_components.size() + 1);
                                    }
                                }
                            }

                            // Update next chunk.
                            cur_chunk = je_arch_next_chunk(cur_chunk);
                        }
                    }
                }
            }
        };
    }
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
            size_t index = tname.find_last_of(':');

            std::optional<std::string> tnamespace = std::nullopt;
            if (index + 1 < tname.size() && index >= 1)
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
                        + ", " + registed_member->m_member_type->m_script_parser_info->m_woolang_typename
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
    std::unique_lock ug1(jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems_mx);

    auto* old_typeinfo = je_typing_get_info_by_name(system_name);
    if (old_typeinfo != nullptr)
    {
        // 同名系统未解除注册，在此报错
        jeecs::debug::logerr("The towoo-system named: '%s' has been registered.", system_name);
    }
    else if (jeecs_file* texfile = jeecs_file_open(script_path))
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
            auto* towoo_system_tinfo = jeecs::typing::type_info::of<
                jeecs::towoo::ToWooBaseSystem>(system_name);

            // Invoke "_init_towoo_system", if failed... boom!
            wo_integer_t initfunc = wo_extern_symb(vm, "_init_towoo_system");
            if (initfunc == 0)
            {
                jeecs::debug::logerr("Failed to register: '%s' cannot find '_init_towoo_system' in '%s', "
                    "forget to import je/towoo/system.wo ?",
                    system_name, script_path);
            }
            else
            {
                if (nullptr == wo_run(vm))
                {
                    jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                        system_name, wo_get_runtime_error(vm));
                }
                else
                {
                    // Set system info.
                    jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems[towoo_system_tinfo]
                        = std::make_unique<jeecs::towoo::ToWooBaseSystem::towoo_system_info>(vm);

                    ug1.unlock();
                    wo_push_pointer(vm, (void*)towoo_system_tinfo);
                    if (nullptr == wo_invoke_rsfunc(vm, initfunc, 1))
                    {
                        ug1.lock();
                        jeecs::debug::logerr("Failed to register: '%s', '_init_towoo_system' failed: '%s'.",
                            system_name, wo_get_runtime_error(vm));

                        // 解除注册，不需要释放vm，erase的同时，vm已经被释放了。
                        jeecs::towoo::ToWooBaseSystem::_registered_towoo_base_systems.erase(towoo_system_tinfo);
                    }
                    else
                        return towoo_system_tinfo;
                }
            }
        }
        else
        {
            jeecs::debug::logerr("Failed to register: '%s' failed to compile:\n%s",
                system_name, wo_get_compile_error(vm, WO_NEED_COLOR));
        }
        wo_close_vm(vm);
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
    for (wo_integer_t i = 0; i < wo_lengthof(requirements); ++i)
    {
        wo_value requirement_info = wo_arr_get(requirements, i);

        const auto* typeinfo = std::launder(reinterpret_cast<const jeecs::typing::type_info*>(
            wo_pointer(wo_struct_get(requirement_info, 2))));

        stepwork.m_dependence.m_requirements.push_back(
            jeecs::requirement(
                (jeecs::requirement::type)wo_int(wo_struct_get(requirement_info, 0)),
                wo_int(wo_struct_get(requirement_info, 1)),
                typeinfo->m_id
            ));

        if (i < component_arg_count)
            stepwork.m_used_components.push_back(typeinfo);

    }
    works->m_works.push_back(stepwork);

    return wo_ret_void(vm);
}

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
        }
        public func maynot<CompT>(self: ToWooSystemFuncJob)
        {
            self.m_requirement->add((require_type::MAYNOT, self.m_require_group, je::towoo::tid:<CompT>()));
            self.m_require_group += 1;
            self.m_argument_count += 1;
        }
        public func except<CompT>(self: ToWooSystemFuncJob)
        {
            self.m_requirement->add((require_type::EXCEPT, self.m_require_group, je::towoo::tid:<CompT>()));
            self.m_require_group += 1;
            self.m_argument_count += 1;
        }
        public func anyof(self: ToWooSystemFuncJob, ts: array<je::typeinfo>)
        {
            for (let _, t: ts)
                self.m_requirement->add((require_type::EXCEPT, self.m_require_group, t));
            self.m_require_group += 1;
            self.m_argument_count += 1;
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
    let mut result = F"je::towoo::system::register_job_function({job_func_name}\x29";
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
        result += F"->except:<{type}>(false\x29";
    for (let _, req : requirements.anyof)
    {
        result += F"->anyof([";
        for (let _, t : req)
            result += F"je::towoo::tid:<{t}>(),";
        result += "]\x29";
    }
    result += F";\n func {job_func_name}(e: je::entity";
    for (let _, (argname, type, maynot) : arguments)
    {
        if (maynot)
            result += F", {argname}: option<{type}>";
        else
            result += F", {argname}: {type}";
    }
    result += "){do e;\n";
    lexer->lex(result);
}



)";
