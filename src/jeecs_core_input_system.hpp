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
    struct VirtualGamepadInputSystem : public game_system
    {
        VirtualGamepadInputSystem(game_world world)
            : game_system(world)
        {
        }

        void StateUpdate(jeecs::selector &selector)
        {
            selector.exec(
                [](Input::VirtualGamepad &vgamepad)
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

                    je_io_gamepad_update_stick(
                        vgamepad.gamepad, input::joystickcode::L, left_stick_x, left_stick_y);

                    std::unordered_map<input::gamepadcode, bool> key_state;
                    for (auto &[kb, gp] : vgamepad.keymap)
                    {
                        key_state[gp] = key_state[gp] || input::keydown(kb);
                    }

                    for (auto &[gp, state] : key_state)
                    {
                        je_io_gamepad_update_button_state(vgamepad.gamepad, gp, state);
                    }
                });
        }
    };
}