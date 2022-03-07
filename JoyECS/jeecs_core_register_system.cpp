#pragma once

#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.h"

void jeecs_entry_register_core_systems()
{
    jeecs::typing::type_info::of<jeecs::TranslationUpdatingSystem>("jeecs::TranslationUpdatingSystem");
    jeecs::typing::type_info::of<jeecs::DefaultGraphicPipelineSystem>("jeecs::DefaultGraphicPipelineSystem");
}