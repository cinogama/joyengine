#pragma once

#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs
{
    namespace Editor
    {
        struct Invisable
        {
            // Entity with this component will not display in editor.
        };
        struct EditorWalker
        {
            // Walker entity will have a child camera and controled by user.
        };
    }

    struct DefaultEditorSystem : public game_shared_system
    {
        void EditorWalkerWork(
            Transform::LocalPosition* position,
            Transform::LocalRotation* rotation)
        {
            have_editor_walker = true;


        }
        void EditorCameraWork(
            Transform::LocalPosition* position,
            Transform::LocalRotation* rotation)
        {

        }

        DefaultEditorSystem(game_universe universe)
            : game_shared_system(universe)
        {
            register_system_func(&DefaultEditorSystem::EditorWalkerWork,
                {
                    contain<Editor::EditorWalker>(),
                    except<Camera::Projection>(),
                    system_write(&have_editor_walker),
                });
            register_system_func(&DefaultEditorSystem::EditorCameraWork,
                {
                    contain<Editor::EditorWalker>(),
                    contain<Camera::Projection>()
                });

            // MakeSureEditorCameraExists must work after EditorWalkerWork
            register_system_func(&DefaultEditorSystem::MakeSureEditorCameraExists,
                {
                    system_read(&have_editor_walker),
                });
        }
    };
}