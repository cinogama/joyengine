#define JE_IMPL
#include "jeecs.hpp"

bool    _key_states[(int)jeecs::input::keycode::MAX_KEY_CODE] = {};
int     _mouse_pos[64][2] = {};
float   _wheel_count[64] = {};
bool    _shoudle_lock_mouse = false;
int     _mouse_lock_place_x = 0, _mouse_lock_place_y = 0;
int     _windows_width = 0, _windows_height = 0;


void je_io_set_keystate(jeecs::input::keycode keycode, bool keydown)
{
    _key_states[(int)keycode] = keydown;
}
void je_io_set_mousepos(int group, int x, int y)
{
    _mouse_pos[group][0] = x;
    _mouse_pos[group][1] = y;
}
void je_io_set_wheel(int group, float count)
{
    _wheel_count[group] = count;
}
void je_io_set_windowsize(int x, int y)
{
    _windows_width = x;
    _windows_height = y;
}

void je_io_lock_mouse(bool lock, int x, int y)
{
    _shoudle_lock_mouse = lock;
    _mouse_lock_place_x = x;
    _mouse_lock_place_y = y;
}

bool je_io_is_keydown(jeecs::input::keycode keycode)
{
    return _key_states[(int)keycode];
}
void je_io_mouse_pos(int group, int* x, int* y)
{
    *x = _mouse_pos[group][0];
    *y = _mouse_pos[group][1];
}
float je_io_wheel(int group)
{
    return _wheel_count[group];
}
bool je_io_should_lock_mouse(int* x, int* y)
{
    *x = _mouse_lock_place_x;
    *y = _mouse_lock_place_y;
    return _shoudle_lock_mouse;
}
void je_io_windowsize(int* x, int* y)
{
    *x = _windows_width;
    *y = _windows_height;
}
bool should_update_windowsize = false;
int new_windows_width = 0, new_windows_height = 0;
void je_io_update_windowsize(int x, int y)
{
    new_windows_width = x;
    new_windows_height = y;
    should_update_windowsize = true;
}
bool je_io_should_update_windowsize(int* x, int* y)
{
    if (should_update_windowsize)
    {
        *x = new_windows_width;
        *y = new_windows_height;

        should_update_windowsize = false;
        return true;
    }
    return false;
}