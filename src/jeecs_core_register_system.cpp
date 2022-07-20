#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"

void jeecs_entry_register_core_systems()
{
    jeecs::typing::type_info::of<jeecs::TranslationUpdatingSystem>("Translation::TranslationUpdatingSystem");
    jeecs::typing::type_info::of<jeecs::DefaultGraphicPipelineSystem>("Graphic::DefaultGraphicPipelineSystem");
}