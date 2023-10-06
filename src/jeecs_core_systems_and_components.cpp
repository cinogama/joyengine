#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_editor_system.hpp"
#include "jeecs_core_physics_system.hpp"
#include "jeecs_core_script_system.hpp"
#include "jeecs_core_audio_system.hpp"

WO_API wo_api wojeapi_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only get delta time in world with Script::ScriptRuntimeSystem.");
        return wo_ret_real(vm, 0.);
    }

    return wo_ret_real(vm, jeecs::ScriptRuntimeSystem::system_instance->real_deltatimed());
}

WO_API wo_api wojeapi_smooth_deltatime(wo_vm vm, wo_value args, size_t argc)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only get delta time in world with Script::ScriptRuntimeSystem.");
        return wo_ret_real(vm, 0.);
    }

    return wo_ret_real(vm, jeecs::ScriptRuntimeSystem::system_instance->deltatimed());
}

WO_API wo_api wojeapi_startup_coroutine(wo_vm vm, wo_value args, size_t argc)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only start up coroutine in Script or another Coroutine.");
        return wo_ret_void(vm);
    }

    // start_coroutine(workjob, (args))
    wo_value cofunc = args + 0;
    wo_value arguments = args + 1;
    auto argument_count = wo_lengthof(arguments);

    wo_vm co_vmm = wo_borrow_vm(vm);

    for (auto i = argument_count; i > 0; --i)
    {
        wo_struct_get(wo_push_empty(co_vmm), arguments, (uint16_t)(i - 1));
    }

    if (wo_valuetype(cofunc) == WO_INTEGER_TYPE)
        wo_dispatch_rsfunc(co_vmm, wo_int(cofunc), argument_count);
    else
        wo_dispatch_closure(co_vmm, cofunc, argument_count);

    std::lock_guard sg1(jeecs::ScriptRuntimeSystem::system_instance
        ->_coroutine_list_mx);

    jeecs::ScriptRuntimeSystem::system_instance
        ->_coroutine_list.push_back(co_vmm);

    return wo_ret_void(vm);
}


const char* je_ecs_get_name_of_entity(const jeecs::game_entity* entity)
{
    jeecs::Editor::Name* c_name = entity->get_component<jeecs::Editor::Name>();
    if (c_name)
        return c_name->name.c_str();
    return "";
}
const char* je_ecs_set_name_of_entity(const jeecs::game_entity* entity, const char* name)
{
    jeecs::Editor::Name* c_name = entity->get_component<jeecs::Editor::Name>();
    if (!c_name)
        c_name = entity->add_component<jeecs::Editor::Name>();

    if (c_name)
        return (c_name->name = name).c_str();
    return "";
}

WO_API wo_api wojeapi_entity_get_prefab_path(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
        return wo_ret_option_string(vm, prefab->path.c_str());
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_entity_set_prefab_path(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
    {
        prefab->path = wo_string(args + 1);
    }
    return wo_ret_void(vm);
}

void jeecs_entry_register_core_systems()
{
    jeecs::typing::type_info::of<jeecs::Editor::Name>("Editor::Name");
    jeecs::typing::type_info::of<jeecs::Editor::Prefab>("Editor::Prefab");
    jeecs::typing::type_info::of<jeecs::Editor::EditorWalker>("Editor::EditorWalker");
    jeecs::typing::type_info::of<jeecs::Editor::Invisable>("Editor::Invisable");
    jeecs::typing::type_info::of<jeecs::Editor::EntityMover>("Editor::EntityMover");
    jeecs::typing::type_info::of<jeecs::Editor::EntityMoverRoot>("Editor::EntityMoverRoot");
    jeecs::typing::type_info::of<jeecs::Editor::BadShadersUniform>("Editor::BadShadersUniform");
    jeecs::typing::type_info::of<jeecs::Editor::EntitySelectBox>("Editor::EntitySelectBox");
    jeecs::typing::type_info::of<jeecs::Editor::NewCreatedEntity>("Editor::NewCreatedEntity");    

    jeecs::typing::type_info::of<jeecs::DefaultEditorSystem>("Editor::DefaultEditorSystem");
    jeecs::typing::type_info::of<jeecs::TranslationUpdatingSystem>("Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::of<jeecs::Physics2DUpdatingSystem>("Physics::Physics2DUpdatingSystem");

    jeecs::typing::type_info::of<jeecs::FrameAnimation2DSystem>("Animation2D::FrameAnimation2DSystem");

    jeecs::typing::type_info::of<jeecs::UserInterfaceGraphicPipelineSystem>("Graphic::UserInterfaceGraphicPipelineSystem");
    jeecs::typing::type_info::of<jeecs::UnlitGraphicPipelineSystem>("Graphic::UnlitGraphicPipelineSystem");
    jeecs::typing::type_info::of<jeecs::DeferLight2DGraphicPipelineSystem>("Graphic::DeferLight2DGraphicPipelineSystem");

    jeecs::typing::type_info::of<jeecs::ScriptRuntimeSystem>("Script::ScriptRuntimeSystem");
    jeecs::typing::type_info::of<jeecs::AudioUpdatingSystem>("Audio::AudioUpdatingSystem");
}