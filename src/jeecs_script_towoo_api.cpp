#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

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
}
*/

namespace jeecs
{
    namespace towoo
    {
        struct ToWooBaseSystem : game_system
        {
            wo_vm m_job_vm;
            std::vector<std::unique_ptr<>>
        };
    }
}

void je_update_woolang_api()
{
    // 1. 获取所有的BasicType，为这些类型生成对应的Woolang类型
    std::string woolang_parsing_type_decl = "// (C)Cinogama project.\n;\n";

    std::unordered_set<std::string> generated_types;

    auto** alltypes = jedbg_get_all_registed_types();
    std::vector<const jeecs::typing::type_info*> all_registed_types;
    for (auto* idx = alltypes; *idx != nullptr;++idx)
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
            std::string("// Declear of '") + script_parser_info->m_woolang_typename + "'\n"
            + script_parser_info->m_woolang_typedecl + "\n\n";
    }

    std::string woolang_component_type_decl =
        R"(// (C)Cinogama project.
import je;
import je::api::type;

using member<T> = (handle, je::typeinfo)
{
    extern("?", "?")
    public func get<T>(self: member<T>)=> T;

    extern("?", "?")
    public func set<T>(self: member<T>, val: T)=> void;
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
                        std::string("    ") + registed_member->m_member_name + ": member<"
                        + registed_member->m_member_type->m_script_parser_info->m_woolang_typename + ">,\n";
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

    if (!wo_virtual_source("je/api/type.wo", woolang_parsing_type_decl.c_str(), true))
        jeecs::debug::logfatal("Unable to regenerate 'je/api/type.wo' please check.");
    if (!wo_virtual_source("je/api/components.wo", woolang_component_type_decl.c_str(), true))
        jeecs::debug::logfatal("Unable to regenerate 'je/api/components.wo' please check.");
}
