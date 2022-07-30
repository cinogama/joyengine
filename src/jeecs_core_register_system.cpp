#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_editor_system.hpp"

void jeecs_entry_register_core_systems()
{
    jeecs::typing::type_info::of<jeecs::TranslationUpdatingSystem>("Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::of<jeecs::DefaultGraphicPipelineSystem>("Graphic::DefaultGraphicPipelineSystem");

    jeecs::typing::type_info::of<jeecs::Editor::EditorWalker>("Editor::EditorWalker");
    jeecs::typing::type_info::of<jeecs::Editor::Invisable>("Editor::Invisable");
    jeecs::typing::type_info::of<jeecs::Editor::EditorLife>("Editor::EditorLife");
    jeecs::typing::type_info::of<jeecs::Editor::EntityMover>("Editor::EntityMover");
    jeecs::typing::type_info::of<jeecs::DefaultEditorSystem>("Editor::DefaultEditorSystem");
}