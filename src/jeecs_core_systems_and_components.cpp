#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_editor_system.hpp"
#include "jeecs_core_physics_system.hpp"
#include "jeecs_core_script_system.hpp"

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

void jeecs_entry_register_core_systems()
{
    jeecs::typing::type_info::of<jeecs::Editor::Name>("Editor::Name");
    jeecs::typing::type_info::of<jeecs::Editor::Anchor>("Editor::Anchor");
    jeecs::typing::type_info::of<jeecs::Editor::EditorWalker>("Editor::EditorWalker");
    jeecs::typing::type_info::of<jeecs::Editor::Invisable>("Editor::Invisable");
    jeecs::typing::type_info::of<jeecs::Editor::EntityMover>("Editor::EntityMover");
    jeecs::typing::type_info::of<jeecs::Editor::EntityMoverRoot>("Editor::EntityMoverRoot");
    jeecs::typing::type_info::of<jeecs::Editor::BadShadersUniform>("Editor::BadShadersUniform");
    jeecs::typing::type_info::of<jeecs::Editor::EntitySelectBox>("Editor::EntitySelectBox");
    jeecs::typing::type_info::of<jeecs::Editor::MapTileSet>("Editor::MapTileSet");
    jeecs::typing::type_info::of<jeecs::Editor::MapTile>("Editor::MapTile");

    jeecs::typing::type_info::of<jeecs::DefaultEditorSystem>("Editor::DefaultEditorSystem");
    jeecs::typing::type_info::of<jeecs::TranslationUpdatingSystem>("Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::of<jeecs::Physics2DUpdatingSystem>("Physics::Physics2DUpdatingSystem");

    jeecs::typing::type_info::of<jeecs::FrameAnimation2DSystem>("Animation2D::FrameAnimation2DSystem");

    jeecs::typing::type_info::of<jeecs::UnlitUIGraphicPipelineSystem>("Graphic::UnlitUIGraphicPipelineSystem");
    jeecs::typing::type_info::of<jeecs::DeferLight2DGraphicPipelineSystem>("Graphic::DeferLight2DGraphicPipelineSystem");

    jeecs::typing::type_info::of<jeecs::Script::Woolang>("Script::Woolang");
    jeecs::typing::type_info::of<jeecs::ScriptRuntimeSystem>("Script::ScriptRuntimeSystem");
}