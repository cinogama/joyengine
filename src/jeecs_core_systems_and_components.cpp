#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_editor_system.hpp"
#include "jeecs_core_physics_system.hpp"
#include "jeecs_core_script_system.hpp"
#include "jeecs_core_audio_system.hpp"
#include "jeecs_core_input_system.hpp"

WOORT_API woort_api wojeapi_deltatime(void)
{
    if (jeecs::script::current_script_game_system_instance == nullptr)
        return woort_ret_panic("Cannot get deltatime from the outside of script game system.");

    return woort_ret_real(jeecs::script::current_script_game_system_instance->deltatime());
}

WOORT_API woort_api wojeapi_smooth_deltatime(void)
{
    if (jeecs::script::current_script_game_system_instance == nullptr)
        return woort_ret_panic("Cannot get smooth_deltatime from the outside of script game system.");

    return woort_ret_real(jeecs::script::current_script_game_system_instance->deltatimed());
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

WOORT_API woort_api wojeapi_entity_get_prefab_path(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
        return woort_ret_option_string(prefab->path.c_str());
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_entity_set_prefab_path(void)
{
    jeecs::game_entity* const entity = static_cast<jeecs::game_entity*>(woort_gcpointer(0));
    if (auto* prefab = entity->get_component<jeecs::Editor::Prefab>())
    {
        prefab->path = woort_string(1);
    }
    return woort_ret_void();
}

void _jeecs_entry_register_core_systems(jeecs::typing::type_unregister_guard* guard)
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
    jeecs::typing::type_info::register_type<jeecs::Editor::EntityId>(guard, "Editor::EntityId");

    jeecs::typing::type_info::register_type<jeecs::DefaultEditorSystem>(guard, "Editor::DefaultEditorSystem");
    jeecs::typing::type_info::register_type<jeecs::TranslationUpdatingSystem>(guard, "Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::register_type<jeecs::Physics2DUpdatingSystem>(guard, "Physics::Physics2DUpdatingSystem");

    jeecs::typing::type_info::register_type<jeecs::FrameAnimationSystem>(guard, "Animation::FrameAnimationSystem");

    jeecs::typing::type_info::register_type<jeecs::UserInterfaceGraphicPipelineSystem>(guard, "Graphic::UserInterfaceGraphicPipelineSystem");
    jeecs::typing::type_info::register_type<jeecs::UnlitGraphicPipelineSystem>(guard, "Graphic::UnlitGraphicPipelineSystem");
    jeecs::typing::type_info::register_type<jeecs::DeferLight2DGraphicPipelineSystem>(guard, "Graphic::DeferLight2DGraphicPipelineSystem");

    jeecs::typing::type_info::register_type<jeecs::AudioUpdatingSystem>(guard, "Audio::AudioUpdatingSystem");
    jeecs::typing::type_info::register_type<jeecs::VirtualGamepadInputSystem>(guard, "Input::VirtualGamepadInputSystem");
}