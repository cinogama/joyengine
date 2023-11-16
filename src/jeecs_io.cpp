#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs::input
{
    struct _je_mouse_state
    {
        bool    _key_states[(size_t)mousecode::_COUNT];
        int     _pos_x;
        int     _pos_y;
        float   _wheel_x;
        float   _wheel_y;
    };

    struct _je_io_state
    {
        bool    _key_states[(size_t)keycode::_COUNT];
        _je_mouse_state     _mouses[MAX_MOUSE_GROUP_COUNT];
        int     _windows_width;
        int     _windows_height;

        bool    _shoudle_lock_mouse;
        int     _mouse_lock_place_x, _mouse_lock_place_y;

        bool    _should_update_windowsize;
        int     _new_windows_width, _new_windows_height;

        bool            _should_update_windowtitle;
        std::string     _new_windowtitle;
    };
}

jeecs::input::_je_io_state _state = {};

void je_io_update_keystate(jeecs::input::keycode keycode, bool keydown)
{
    _state._key_states[(int)keycode] = keydown;
}
void je_io_update_mousepos(size_t group, int x, int y)
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
void je_io_update_windowsize(int x, int y)
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

bool je_io_get_keydown(jeecs::input::keycode keycode)
{
    return _state._key_states[(int)keycode];
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
void je_io_get_windowsize(int* out_x, int* out_y)
{
    *out_x = _state._windows_width;
    *out_y = _state._windows_height;
}

void je_io_set_windowsize(int x, int y)
{
    _state._new_windows_width = x;
    _state._new_windows_height = y;
    _state._should_update_windowsize = true;
}
bool je_io_fetch_update_windowsize(int* out_x, int* out_y)
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

void je_io_set_windowtitle(const char* title)
{
    _state._new_windowtitle = title;
    _state._should_update_windowtitle = true;
}

bool je_io_fetch_update_windowtitle(const char** out_title)
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