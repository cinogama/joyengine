#define JE_IMPL
#include "jeecs.hpp"

bool    _key_states[(int)je_keycode::MAX_KEY_CODE] = {};
float   _mouse_pos[64][2] = {};
float   _wheel_count = 0.f;

void je_io_set_keystate(je_keycode keycode, bool keydown)
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

bool je_io_is_keydown(je_keycode keycode)
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
