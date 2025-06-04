#define JE_IMPL
#include "jeecs.hpp"

#include <optional>
#include <list>
#include <queue>
#include <shared_mutex>

struct _je_mouse_state
{
    bool _key_states[(uint8_t)jeecs::input::mousecode::_COUNT];
    int _pos_x;
    int _pos_y;
    float _wheel_x;
    float _wheel_y;
};
struct _je_gamepad_state
{
    bool _enabled;
    std::string _name;
    std::string _guid;
    jeecs::typing::ms_stamp_t
        _last_update_time;

    bool _key_states[(uint8_t)jeecs::input::gamepadcode::_COUNT];

    struct __je_gamepad_stick_state
    {
        jeecs::math::vec2 _position;
        float _deadzone;
    };
    __je_gamepad_stick_state _stick_states[(uint8_t)jeecs::input::joystickcode::_COUNT];
};

struct _je_basic_io_state
{

    std::shared_mutex _gamepads_mx;
    std::queue<je_io_gamepad_handle_t> _gamepads_free_slot;
    std::list<_je_gamepad_state> _gamepads;

    _je_mouse_state _mouses[jeecs::input::MAX_MOUSE_GROUP_COUNT];
    bool _key_states[(uint16_t)jeecs::input::keycode::_COUNT];

    int _windows_width;
    int _windows_height;

    int _windows_pos_x,
        _windows_pos_y;

    bool _shoudle_lock_mouse;

    bool _should_update_windowsize;
    int _new_windows_width,
        _new_windows_height;

    // bool _should_update_windowpos;
    // int _new_windows_pos_x,
    //     _new_windows_pos_y;

    bool _should_update_windowtitle;
    std::string _new_windowtitle;
};
_je_basic_io_state _state = {};

void je_io_update_key_state(jeecs::input::keycode keycode, bool keydown)
{
    _state._key_states[(uint16_t)keycode] = keydown;
}
void je_io_update_mouse_pos(size_t group, int x, int y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        mouse_stat._pos_x = x;
        mouse_stat._pos_y = y;
    }
}
void je_io_update_wheel(size_t group, float x, float y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        mouse_stat._wheel_x = x;
        mouse_stat._wheel_y = y;
    }
}
void je_io_update_window_size(int x, int y)
{
    _state._windows_width = x;
    _state._windows_height = y;
}
void je_io_update_window_pos(int x, int y)
{
    _state._windows_pos_x = x;
    _state._windows_pos_y = y;
}
void je_io_set_lock_mouse(bool lock)
{
    _state._shoudle_lock_mouse = lock;
}

bool je_io_get_key_down(jeecs::input::keycode keycode)
{
    return _state._key_states[(uint16_t)keycode];
}

void je_io_get_mouse_pos(size_t group, int *out_x, int *out_y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        *out_x = mouse_stat._pos_x;
        *out_y = mouse_stat._pos_y;
    }
    else
    {
        *out_x = 0;
        *out_y = 0;
    }
}
void je_io_get_wheel(size_t group, float *out_x, float *out_y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        *out_x = mouse_stat._wheel_x;
        *out_y = mouse_stat._wheel_y;
    }
    else
    {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}
bool je_io_get_lock_mouse()
{
    return _state._shoudle_lock_mouse;
}
void je_io_get_window_size(int *out_x, int *out_y)
{
    *out_x = _state._windows_width;
    *out_y = _state._windows_height;
}
void je_io_get_window_pos(int *out_x, int *out_y)
{
    *out_x = _state._windows_pos_x;
    *out_y = _state._windows_pos_y;
}
void je_io_set_window_size(int x, int y)
{
    _state._new_windows_width = x;
    _state._new_windows_height = y;
    _state._should_update_windowsize = true;
}
bool je_io_fetch_update_window_size(int *out_x, int *out_y)
{
    if (_state._should_update_windowsize)
    {
        *out_x = _state._new_windows_width;
        *out_y = _state._new_windows_height;

        _state._should_update_windowsize = false;
        return true;
    }
    return false;
}

void je_io_set_window_title(const char *title)
{
    _state._new_windowtitle = title;
    _state._should_update_windowtitle = true;
}

bool je_io_fetch_update_window_title(const char **out_title)
{
    if (_state._should_update_windowtitle)
    {
        *out_title = _state._new_windowtitle.c_str();
        _state._should_update_windowtitle = false;
        return true;
    }
    return false;
}

bool je_io_get_mouse_state(size_t group, jeecs::input::mousecode key)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
        return _state._mouses[group]._key_states[(size_t)key];
    return false;
}

void je_io_update_mouse_state(size_t group, jeecs::input::mousecode key, bool keydown)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
        _state._mouses[group]._key_states[(size_t)key] = keydown;
}

je_io_gamepad_handle_t je_io_create_gamepad(
    const char *name_may_null, const char *guid_may_null)
{
    std::lock_guard g(_state._gamepads_mx);

    je_io_gamepad_handle_t gamepad;
    do
    {
        if (_state._gamepads_free_slot.empty())
        {
            gamepad = &_state._gamepads.emplace_back();
        }
        else
        {
            gamepad = _state._gamepads_free_slot.front();
            _state._gamepads_free_slot.pop();
        }
    } while (0);

    if (name_may_null != nullptr)
        gamepad->_name = name_may_null;
    else
    {
        char name[48];
        sprintf(name, "Default virtual gamepad %p", gamepad);

        gamepad->_name = name;
    }

    if (guid_may_null != nullptr)
        gamepad->_guid = guid_may_null;
    else
    {
        auto uuid_instance = jeecs::typing::uuid::generate();

        char uuid[48];
        sprintf(uuid, "%016llX-%016llX", uuid_instance.a, uuid_instance.b);

        gamepad->_guid = uuid;
    }

    gamepad->_last_update_time = je_clock_time_stamp();

    for (size_t i = 0; i < (uint8_t)jeecs::input::gamepadcode::_COUNT; ++i)
        gamepad->_key_states[i] = false;

    for (size_t i = 0; i < (uint8_t)jeecs::input::joystickcode::_COUNT; ++i)
    {
        auto &stick = gamepad->_stick_states[i];
        stick._position = jeecs::math::vec2();
        stick._deadzone = 0.f;
    }

    jeecs::debug::loginfo("Gamepad device: '%s'[%s](%p) created.",
                          gamepad->_name.c_str(), gamepad->_guid.c_str(), gamepad);

    gamepad->_enabled = true;
    return gamepad;
}
void je_io_close_gamepad(je_io_gamepad_handle_t gamepad)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Gamepad: `%p` is already closed.", gamepad);
        return;
    }
    gamepad->_enabled = false;

    jeecs::debug::loginfo("Gamepad device: '%s'[%s](%p) closed.",
                          gamepad->_name.c_str(), gamepad->_guid.c_str(), gamepad);

    std::lock_guard g(_state._gamepads_mx);
    _state._gamepads_free_slot.push(gamepad);
}
const char *je_io_gamepad_name(je_io_gamepad_handle_t gamepad)
{
    return gamepad->_name.c_str();
}
const char *je_io_gamepad_guid(je_io_gamepad_handle_t gamepad)
{
    return gamepad->_guid.c_str();
}
size_t je_io_gamepad_get(size_t count, je_io_gamepad_handle_t *out_gamepads)
{
    std::shared_lock g(_state._gamepads_mx);

    if (count == 0)
        return _state._gamepads.size();

    auto fetch_count = std::min(count, _state._gamepads.size());
    auto iter = _state._gamepads.begin();

    for (size_t i = 0; i < fetch_count; ++i)
    {
        out_gamepads[i] = &(*iter);
        ++iter;
    }
    return fetch_count;
}
bool je_io_gamepad_is_active(
    je_io_gamepad_handle_t gamepad,
    jeecs::typing::ms_stamp_t *out_last_pushed_time_may_null)
{
    if (out_last_pushed_time_may_null != nullptr)
        *out_last_pushed_time_may_null = gamepad->_last_update_time;

    return gamepad->_enabled;
}
bool je_io_gamepad_get_button_down(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code)
{
    if (!gamepad->_enabled)
        return false;

    return gamepad->_key_states[(size_t)code];
}
void je_io_gamepad_update_button_state(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code, bool down)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }

    if ((gamepad->_key_states[(size_t)code] = down))
        gamepad->_last_update_time = je_clock_time_stamp();
}

void je_io_gamepad_get_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float *out_x, float *out_y)
{
    if (!gamepad->_enabled)
    {
        *out_x = 0.f;
        *out_y = 0.f;
        return;
    }
    auto &stick = gamepad->_stick_states[(uint8_t)stickid];
    *out_x = stick._position.x;
    *out_y = stick._position.y;
}
void je_io_gamepad_update_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float x, float y)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }

    const jeecs::math::vec2 stick_position(x, y);
    auto &stick = gamepad->_stick_states[(uint8_t)stickid];

    auto length = stick_position.length();
    if (stick_position.length() <= stick._deadzone)
        stick._position = jeecs::math::vec2(0.f, 0.f);
    else
    {
        stick._position = stick_position / std::max(length, 1.f);
        gamepad->_last_update_time = je_clock_time_stamp();
    }
}

void je_io_gamepad_stick_set_deadzone(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float deadzone)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }
    gamepad->_stick_states[(uint8_t)stickid]._deadzone = deadzone;
}
