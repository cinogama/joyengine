#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

// INPUTS
WOORT_API woort_api wojeapi_input_keydown(void)
{
    return woort_ret_bool(input::keydown((je_keycode_t)woort_int(0)));
}

WOORT_API woort_api wojeapi_input_mousedown(void)
{
    return woort_ret_bool(input::mousedown(
        (size_t)woort_int(0), (je_mousecode_t)woort_int(1)));
}

WOORT_API woort_api wojeapi_wheel_count(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto wheel = input::wheel((size_t)woort_int(0));

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, wheel.x);
    woort_struct_set_float(result, 1, wheel.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_window_size(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto winsz = input::windowsize();

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)winsz.x);
    woort_struct_set_int(result, 1, (woort_Int)winsz.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_window_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto winpos = input::windowpos();

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)winpos.x);
    woort_struct_set_int(result, 1, (woort_Int)winpos.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_mouse_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto mousepos = input::mousepos((size_t)woort_int(0));

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_int(result, 0, (woort_Int)mousepos.x);
    woort_struct_set_int(result, 1, (woort_Int)mousepos.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_mouse_view_pos(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto mouseview = input::mouseviewpos((size_t)woort_int(0));

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, mouseview.x);
    woort_struct_set_float(result, 1, mouseview.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_set_lock_mouse(void)
{
    je_io_mouse_lock_set(woort_bool(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_get_lock_mouse(void)
{
    return woort_ret_bool(je_io_mouse_lock_get());
}

WOORT_API woort_api wojeapi_input_gamepad_button(void)
{
    input::gamepad* gamepad =
        (input::gamepad*)woort_gcpointer(0);
    je_gamepadcode_t kcode =
        (je_gamepadcode_t)woort_int(1);

    return woort_ret_bool(gamepad->button(kcode));
}

WOORT_API woort_api wojeapi_input_gamepad_axis(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    input::gamepad* gamepad =
        (input::gamepad*)woort_gcpointer(0);
    je_joystickcode_t kcode =
        (je_joystickcode_t)woort_int(1);

    auto axis = gamepad->stick(kcode);

    const woort_value result = s + 0;

    woort_set_struct(result, 2);

    woort_struct_set_float(result, 0, axis.x);
    woort_struct_set_float(result, 1, axis.y);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_gamepad_actived(void)
{
    input::gamepad* gamepad =
        (input::gamepad*)woort_gcpointer(0);

    je_timestamp_ms_t actived;
    if (gamepad->actived(&actived))
        return woort_ret_option_int(actived);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_input_gamepad_get_all(void)
{
    woort_value s;

    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    auto gamepads = input::gamepad::all();

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);
    for (auto& gamepad : gamepads)
    {
        woort_set_gchandle(
            elem,
            new input::gamepad(gamepad),
            WOORT_IGNORE,
            [](void* p)
            {
                delete (input::gamepad*)p;
            },
            nullptr);
        woort_vec_push(result, elem);
    }

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_input_gamepad_last(void)
{
    auto gamepad = input::gamepad::last();
    if (gamepad.has_value())
    {
        return woort_ret_option_gchandle(
            new input::gamepad(gamepad.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete (input::gamepad*)p;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_input_update_window_size(void)
{
    je_io_window_size_set((int)woort_int(0), (int)woort_int(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_update_window_title(void)
{
    je_io_window_title_set(woort_string(0));
    return woort_ret_void();
}

// Input state updates (pushed by host/editor)
WOORT_API woort_api wojeapi_input_update_mouse_state(void)
{
    auto group = (size_t)woort_int(0);
    auto key = (je_mousecode_t)woort_int(1);
    auto down = woort_bool(2);

    je_io_mouse_state_update(group, key, down);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_update_wheel(void)
{
    auto group = (size_t)woort_int(0);
    je_io_wheel_update(group, woort_float(1), woort_float(2));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_input_update_key_state(void)
{
    auto key = (je_keycode_t)woort_int(0);
    auto down = woort_bool(1);

    je_io_key_state_update(key, down);

    return woort_ret_void();
}
