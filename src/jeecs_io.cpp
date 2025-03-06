#define JE_IMPL
#include "jeecs.hpp"

#include <optional>
#include <vector>
#include <queue>
#include <shared_mutex>

struct _je_mouse_state
{
    bool    _key_states[(uint8_t)jeecs::input::mousecode::_COUNT];
    int     _pos_x;
    int     _pos_y;
    float   _wheel_x;
    float   _wheel_y;
};
struct _je_gamepad_state
{
    bool        _enabled;
    bool        _key_states[(uint8_t)jeecs::input::gamepadcode::_COUNT];
    jeecs::math::vec2  
                _stick_states[(uint8_t)jeecs::input::joystickcode::_COUNT];
};

struct _je_basic_io_state
{
    
    std::shared_mutex                   _gamepads_mx;
    std::queue<je_io_gamepad_handle_t>  _gamepads_free_slot;
    std::list<_je_gamepad_state>        _gamepads;

    _je_mouse_state _mouses[jeecs::input::MAX_MOUSE_GROUP_COUNT];
    bool            _key_states[(uint16_t)jeecs::input::keycode::_COUNT];

    int             _windows_width;
    int             _windows_height;

    bool            _shoudle_lock_mouse;
    int             _mouse_lock_place_x,
                    _mouse_lock_place_y;

    bool            _should_update_windowsize;
    int             _new_windows_width,
                    _new_windows_height;

    bool            _should_update_windowtitle;
    std::string     _new_windowtitle;
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
        auto& mouse_stat = _state._mouses[group];
        mouse_stat._pos_x = x;
        mouse_stat._pos_y = y;
    }
}
void je_io_update_wheel(size_t group, float x, float y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto& mouse_stat = _state._mouses[group];
        mouse_stat._wheel_x = x;
        mouse_stat._wheel_y = y;
    }
}
void je_io_update_window_size(int x, int y)
{
    _state._windows_width = x;
    _state._windows_height = y;
}

void je_io_set_lock_mouse(bool lock, int x, int y)
{
    _state._shoudle_lock_mouse = lock;
    _state._mouse_lock_place_x = x;
    _state._mouse_lock_place_y = y;
}

bool je_io_get_key_down(jeecs::input::keycode keycode)
{
    return _state._key_states[(uint16_t)keycode];
}

void je_io_get_mouse_pos(size_t group, int* out_x, int* out_y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto& mouse_stat = _state._mouses[group];
        *out_x = mouse_stat._pos_x;
        *out_y = mouse_stat._pos_y;
    }
    else
    {
        *out_x = 0;
        *out_y = 0;
    }
}
void je_io_get_wheel(size_t group, float* out_x, float* out_y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto& mouse_stat = _state._mouses[group];
        *out_x = mouse_stat._wheel_x;
        *out_y = mouse_stat._wheel_y;
    }
    else
    {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}
bool je_io_get_lock_mouse(int* out_x, int* out_y)
{
    *out_x = _state._mouse_lock_place_x;
    *out_y = _state._mouse_lock_place_y;
    return _state._shoudle_lock_mouse;
}
void je_io_get_window_size(int* out_x, int* out_y)
{
    *out_x = _state._windows_width;
    *out_y = _state._windows_height;
}

void je_io_set_window_size(int x, int y)
{
    _state._new_windows_width = x;
    _state._new_windows_height = y;
    _state._should_update_windowsize = true;
}
bool je_io_fetch_update_window_size(int* out_x, int* out_y)
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

void je_io_set_window_title(const char* title)
{
    _state._new_windowtitle = title;
    _state._should_update_windowtitle = true;
}

bool je_io_fetch_update_window_title(const char** out_title)
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

je_io_gamepad_handle_t je_io_create_gamepad()
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

    jeecs::debug::loginfo("Gamepad virtual device: %p created.", gamepad);
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
    jeecs::debug::loginfo("Gamepad virtual device: %p closed.", gamepad);

    std::lock_guard g(_state._gamepads_mx);
    _state._gamepads_free_slot.push(gamepad);
}

size_t je_io_gamepad_get_count()
{
    std::shared_lock g(_state._gamepads_mx);
    return _state._gamepads.size();
}
je_io_gamepad_handle_t je_io_gamepad_get(size_t idx)
{
    std::shared_lock g(_state._gamepads_mx);
    if (idx < _state._gamepads.size())
    {
        auto it = _state._gamepads.begin();
        std::advance(it, idx);
        return &(*it);
    }
    return nullptr;
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
    gamepad->_key_states[(size_t)code] = down;
}

void je_io_gamepad_get_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float* out_x, float* out_y)
{
    if (!gamepad->_enabled)
    {
        *out_x = 0.f;
        *out_y = 0.f;
        return;
    }
    auto& position = gamepad->_stick_states[(uint8_t)stickid];
    *out_x = position.x;
    *out_y = position.y;
}
void je_io_gamepad_update_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float x, float y)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }
    gamepad->_stick_states[(uint8_t)stickid] = jeecs::math::vec2(x, y).unit();
}