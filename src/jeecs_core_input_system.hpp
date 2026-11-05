#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

namespace jeecs
{
    using namespace slice_requirement;

    struct VirtualGamepadInputSystem : public game_system
    {
        VirtualGamepadInputSystem(game_world world)
            : game_system(world)
        {
        }

        void StateUpdate()
        {
            for (auto&& [vgamepad] :
                query typesof(
                    view typesof(Input::VirtualGamepad&)
                )())
            {
                float left_stick_x = 0.f, left_stick_y = 0.f;
                if (input::keydown(vgamepad.left_stick_up_left_down_right[0]))
                    left_stick_y += 1.f;
                if (input::keydown(vgamepad.left_stick_up_left_down_right[1]))
                    left_stick_x -= 1.f;
                if (input::keydown(vgamepad.left_stick_up_left_down_right[2]))
                    left_stick_y -= 1.f;
                if (input::keydown(vgamepad.left_stick_up_left_down_right[3]))
                    left_stick_x += 1.f;

                je_io_gamepad_stick_update(
                    vgamepad.gamepad, JE_JOY_L, left_stick_x, left_stick_y);

                std::unordered_map<je_gamepadcode_t, bool> key_state;
                for (auto& [kb, gp] : vgamepad.keymap)
                {
                    key_state[gp] = key_state[gp] || input::keydown(kb);
                }

                for (auto& [gp, state] : key_state)
                {
                    je_io_gamepad_button_state_update(vgamepad.gamepad, gp, state);
                }
            }
        }
    };
}