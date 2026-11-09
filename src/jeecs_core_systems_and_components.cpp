#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"
#include "jeecs_woolang_value.hpp"
#include "jeecs_core_translation_system.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_animation_system.hpp"
#include "jeecs_core_particle_system.hpp"
#include "jeecs_core_editor_components.hpp"
#include "jeecs_core_physics_system.hpp"
#include "jeecs_core_audio_system.hpp"
#include "jeecs_core_input_system.hpp"
#include "jeecs_core_tilemap_system.hpp"

using namespace jeecs;

namespace jeecs::core_components_and_systems
{
    namespace
    {
        typing::type_unregister_guard core_type_guard;
    }

    void bootup(int, char**)
    {
        entry::module_entry(&core_type_guard);

        typing::register_type<woolang_runtime::PinValue>(&core_type_guard, nullptr);

        // ===== 组件类型注册 =====
        typing::register_type<Editor::Name>(&core_type_guard, "Editor::Name");
        typing::register_type<Editor::Prefab>(&core_type_guard, "Editor::Prefab");
        typing::register_type<Editor::EditorWalker>(&core_type_guard, "Editor::EditorWalker");
        typing::register_type<Editor::Invisible>(&core_type_guard, "Editor::Invisible");
        typing::register_type<Editor::EntityId>(&core_type_guard, "Editor::EntityId");

        // ===== 系统类型注册 =====
        typing::register_type<translation::TranslationUpdatingSystem>(&core_type_guard, "Translation::TranslationUpdatingSystem");
        typing::register_type<physics::Physics2DUpdatingSystem>(&core_type_guard, "Physics::Physics2DUpdatingSystem");
        typing::register_type<animation::AnimatorSystem>(&core_type_guard, "Animation::AnimatorSystem");
        typing::register_type<particle::ParticleSystem>(&core_type_guard, "Particle::ParticleSystem");
        typing::register_type<graphic::UserInterfaceGraphicPipelineSystem>(&core_type_guard, "Graphic::UserInterfaceGraphicPipelineSystem");
        typing::register_type<graphic::UnlitGraphicPipelineSystem>(&core_type_guard, "Graphic::UnlitGraphicPipelineSystem");
        typing::register_type<graphic::DeferLight2DGraphicPipelineSystem>(&core_type_guard, "Graphic::DeferLight2DGraphicPipelineSystem");
        typing::register_type<audio::AudioUpdatingSystem>(&core_type_guard, "Audio::AudioUpdatingSystem");
        typing::register_type<io::VirtualGamepadInputSystem>(&core_type_guard, "Input::VirtualGamepadInputSystem");
        typing::register_type<tilemap::TilemapSystem>(&core_type_guard, "Tilemap::TilemapSystem");
    }

    void shutdown()
    {
        entry::module_leave(&core_type_guard);
    }
}

// ===== C API：实体名称与调试 EID =====

const char* je_ecs_entity_name_get(const je_game_entity_t* entity)
{
    auto* c_name = (Editor::Name*)je_ecs_world_entity_component_get(
        entity, typing::id<Editor::Name>());
    if (c_name)
        return c_name->name.c_str();
    return "";
}

const char* je_ecs_entity_name_set(const je_game_entity_t* entity, const char* name)
{
    auto* c_name = (Editor::Name*)je_ecs_world_entity_component_get(
        entity, typing::id<Editor::Name>());
    if (!c_name)
        c_name = (Editor::Name*)je_ecs_world_entity_component_add(
            entity, typing::id<Editor::Name>());

    if (c_name)
        return (c_name->name = name).c_str();
    return "";
}

je_debug_eid_t je_debug_entity_uid_get(const je_game_entity_t* e)
{
    auto* eid = static_cast<Editor::EntityId*>(
        je_ecs_world_entity_component_get(
            e, typing::id<Editor::EntityId>()));
    if (eid == nullptr)
    {
        (void)je_ecs_world_entity_component_add(
            e, typing::id<Editor::EntityId>());
        return 0 /* invalid */;
    }
    return eid->eid;
}

void je_debug_entity_uid_set(const je_game_entity_t* e, je_debug_eid_t uid)
{
    auto* eid = static_cast<Editor::EntityId*>(
        je_ecs_world_entity_component_get(
            e, typing::id<Editor::EntityId>()));

    if (eid == nullptr)
    {
        eid = static_cast<Editor::EntityId*>(
            je_ecs_world_entity_component_add(
                e, typing::id<Editor::EntityId>()));
    }

    eid->eid = uid;
}
