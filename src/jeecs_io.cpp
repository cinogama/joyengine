#define JE_IMPL
#include "jeecs.hpp"

bool    _key_states[(int)jeecs::input::keycode::MAX_KEY_CODE] = {};
float   _mouse_pos[64][2] = {};
float   _wheel_count = 0.f;
bool    _shoudle_lock_mouse = false;


void je_io_set_keystate(jeecs::input::keycode keycode, bool keydown)
{
    _key_states[(int)keycode] = keydown;
}
void je_io_set_mousepos(int group, float x, float y)
{
    _mouse_pos[group][0] = x;
    _mouse_pos[group][1] = y;
}
void je_io_set_wheel(float count)
{
    _wheel_count = count;
}
void je_io_lock_mouse(bool lock)
{
    _shoudle_lock_mouse = lock;
}

bool je_io_is_keydown(jeecs::input::keycode keycode)
{
    return _key_states[(int)keycode];
}
void je_io_mouse_pos(int group, float* x, float* y)
{
    *x = _mouse_pos[group][0];
    *y = _mouse_pos[group][1];
}
float je_io_wheel()
{
    return _wheel_count;
}
bool je_io_should_lock_mouse()
{
    return _shoudle_lock_mouse;
}