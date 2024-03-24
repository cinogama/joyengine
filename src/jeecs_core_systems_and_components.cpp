#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_editor_system.hpp"
#include "jeecs_core_physics_system.hpp"
#include "jeecs_core_script_system.hpp"
#include "jeecs_core_audio_system.hpp"

WO_API wo_api wojeapi_deltatime(wo_vm vm, wo_value args)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only get delta time in world with Script::ScriptRuntimeSystem.");
        return wo_ret_real(vm, 0.);
    }

    return wo_ret_real(vm, jeecs::ScriptRuntimeSystem::system_instance->deltatime());
}

WO_API wo_api wojeapi_smooth_deltatime(wo_vm vm, wo_value args)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only get delta time in world with Script::ScriptRuntimeSystem.");
        return wo_ret_real(vm, 0.);
    }

    return wo_ret_real(vm, jeecs::ScriptRuntimeSystem::system_instance->deltatimed());
}

WO_API wo_api wojeapi_startup_coroutine(wo_vm vm, wo_value args)
{
    if (jeecs::ScriptRuntimeSystem::system_instance == nullptr)
    {
        jeecs::debug::logerr("You can only start up coroutine in Script or another Coroutine.");
        return wo_ret_void(vm);
    }

    // start_coroutine(workjob, (args))
    wo_value arguments = args + 1;
    auto argument_count = wo_lengthof(arguments);

    wo_vm co_vmm = wo_borrow_vm(vm);
    wo_value cofunc = wo_push_val(co_vmm, args + 0);

    for (auto i = argument_count; i > 0; --i)
    {
        wo_struct_get(wo_push_empty(co_vmm), arguments, (uint16_t)(i - 1));
    }

    wo_dispatch_value(co_vmm, cofunc, argument_count);

    jeecs::ScriptRuntimeSystem::system_instance->dispatch_coroutine_vm(co_vmm);

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

WO_API wo_api wojeapi_entity_get_prefab_path(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
        return wo_ret_option_string(vm, prefab->path.c_str());
    return wo_ret_option_none(vm);
}

WO_API wo_api wojeapi_entity_set_prefab_path(wo_vm vm, wo_value args)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
    {
        prefab->path = wo_string(args + 1);
    }
    return wo_ret_void(vm);
}

void jeecs_entry_register_core_systems(jeecs::typing::type_unregister_guard* guard)
{
    jeecs::typing::type_info::register_type<jeecs::script::woovalue>(guard, nullptr);

    jeecs::typing::type_info::register_type<jeecs::Editor::Name>(guard, "Editor::Name");
    jeecs::typing::type_info::register_type<jeecs::Editor::Prefab>(guard, "Editor::Prefab");
    jeecs::typing::type_info::register_type<jeecs::Editor::EditorWalker>(guard, "Editor::EditorWalker");
    jeecs::typing::type_info::register_type<jeecs::Editor::Invisable>(guard, "Editor::Invisable");
    jeecs::typing::type_info::register_type<jeecs::Editor::EntityMover>(guard, "Editor::EntityMover");
    jeecs::typing::type_info::register_type<jeecs::Editor::EntityMoverRoot>(guard, "Editor::EntityMoverRoot");
    jeecs::typing::type_info::register_type<jeecs::Editor::BadShadersUniform>(guard, "Editor::BadShadersUniform");
    jeecs::typing::type_info::register_type<jeecs::Editor::EntitySelectBox>(guard, "Editor::EntitySelectBox");

    jeecs::typing::type_info::register_type<jeecs::DefaultEditorSystem>(guard, "Editor::DefaultEditorSystem");
    jeecs::typing::type_info::register_type<jeecs::TranslationUpdatingSystem>(guard, "Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::register_type<jeecs::Physics2DUpdatingSystem>(guard, "Physics::Physics2DUpdatingSystem");

    jeecs::typing::type_info::register_type<jeecs::FrameAnimation2DSystem>(guard, "Animation2D::FrameAnimation2DSystem");

    jeecs::typing::type_info::register_type<jeecs::UserInterfaceGraphicPipelineSystem>(guard, "Graphic::UserInterfaceGraphicPipelineSystem");
    jeecs::typing::type_info::register_type<jeecs::UnlitGraphicPipelineSystem>(guard, "Graphic::UnlitGraphicPipelineSystem");
    jeecs::typing::type_info::register_type<jeecs::DeferLight2DGraphicPipelineSystem>(guard, "Graphic::DeferLight2DGraphicPipelineSystem");

    jeecs::typing::type_info::register_type<jeecs::ScriptRuntimeSystem>(guard, "Script::ScriptRuntimeSystem");
    jeecs::typing::type_info::register_type<jeecs::AudioUpdatingSystem>(guard, "Audio::AudioUpdatingSystem");
}