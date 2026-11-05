#define JE_IMPL
#include "jeecs.hpp"

#include <optional>
#include <list>
#include <queue>
#include <shared_mutex>

struct _je_mouse_state
{
    bool _key_states[(uint8_t)JE_MOUSE_COUNT];
    int _pos_x;
    int _pos_y;
    float _wheel_x;
    float _wheel_y;
};
struct je_gamepad_state_t
{
    bool _enabled;
    std::string _name;
    std::string _guid;
    je_timestamp_ms_t
        _last_update_time;

    bool _key_states[(uint8_t)JE_GAMEPAD_COUNT];

    struct je_gamepad_stick_state_t
    {
        jeecs::math::vec2 _position;
        float _deadzone;
    };
    je_gamepad_stick_state_t _stick_states[(uint8_t)JE_JOY_COUNT];
};

struct _je_basic_io_state
{
    std::shared_mutex _gamepads_mx;
    std::queue<je_io_gamepad_handle_t> _gamepads_free_slot;
    std::list<je_gamepad_state_t> _gamepads;

    _je_mouse_state _mouses[jeecs::input::MAX_MOUSE_GROUP_COUNT];
    bool _key_states[(uint16_t)JE_KEY_COUNT];

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

void je_io_key_state_update(je_keycode_t keycode, bool keydown)
{
    _state._key_states[(uint16_t)keycode] = keydown;
}
void je_io_mouse_pos_update(size_t group, int x, int y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        mouse_stat._pos_x = x;
        mouse_stat._pos_y = y;
    }
}
void je_io_wheel_update(size_t group, float x, float y)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
    {
        auto &mouse_stat = _state._mouses[group];
        mouse_stat._wheel_x = x;
        mouse_stat._wheel_y = y;
    }
}
void je_io_window_size_update(int x, int y)
{
    _state._windows_width = x;
    _state._windows_height = y;
}
void je_io_window_pos_update(int x, int y)
{
    _state._windows_pos_x = x;
    _state._windows_pos_y = y;
}
void je_io_mouse_lock_set(bool lock)
{
    _state._shoudle_lock_mouse = lock;
}

bool je_io_key_down_get(je_keycode_t keycode)
{
    return _state._key_states[(uint16_t)keycode];
}

void je_io_mouse_pos_get(size_t group, int *out_x, int *out_y)
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
void je_io_wheel_get(size_t group, float *out_x, float *out_y)
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
bool je_io_mouse_lock_get()
{
    return _state._shoudle_lock_mouse;
}
void je_io_window_size_get(int *out_x, int *out_y)
{
    *out_x = _state._windows_width;
    *out_y = _state._windows_height;
}
void je_io_window_pos_get(int *out_x, int *out_y)
{
    *out_x = _state._windows_pos_x;
    *out_y = _state._windows_pos_y;
}
void je_io_window_size_set(int x, int y)
{
    _state._new_windows_width = x;
    _state._new_windows_height = y;
    _state._should_update_windowsize = true;
}
bool je_io_window_size_fetch_update(int *out_x, int *out_y)
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

void je_io_window_title_set(const char *title)
{
    _state._new_windowtitle = title;
    _state._should_update_windowtitle = true;
}

bool je_io_window_title_fetch_update(const char **out_title)
{
    if (_state._should_update_windowtitle)
    {
        *out_title = _state._new_windowtitle.c_str();
        _state._should_update_windowtitle = false;
        return true;
    }
    return false;
}

bool je_io_mouse_state_get(size_t group, je_mousecode_t key)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
        return _state._mouses[group]._key_states[(size_t)key];
    return false;
}

void je_io_mouse_state_update(size_t group, je_mousecode_t key, bool keydown)
{
    if (group < jeecs::input::MAX_MOUSE_GROUP_COUNT)
        _state._mouses[group]._key_states[(size_t)key] = keydown;
}

je_io_gamepad_handle_t je_io_gamepad_create(
    /* OPTIONAL */ const char *name_may_null, /* OPTIONAL */ const char *guid_may_null)
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
        int result = snprintf(name, sizeof(name), "Default virtual gamepad %p", gamepad);
        assert(result > 0 && result < (int)sizeof(name));
        (void)result;

        gamepad->_name = name;
    }

    if (guid_may_null != nullptr)
        gamepad->_guid = guid_may_null;
    else
    {
        auto uuid_instance = jeecs::typing::uuid::generate();

        char uuid[48];
        int result = snprintf(uuid, sizeof(uuid), "%016llX-%016llX", 
            static_cast<long long unsigned int>(uuid_instance.a),
            static_cast<long long unsigned int>(uuid_instance.b));
        assert(result > 0 && result < (int)sizeof(uuid));
        (void)result;

        gamepad->_guid = uuid;
    }

    gamepad->_last_update_time = je_clock_time_stamp();

    for (size_t i = 0; i < (uint8_t)JE_GAMEPAD_COUNT; ++i)
        gamepad->_key_states[i] = false;

    for (size_t i = 0; i < (uint8_t)JE_JOY_COUNT; ++i)
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
void je_io_gamepad_close(je_io_gamepad_handle_t gamepad)
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
size_t je_io_gamepad_get(size_t count, /* OPTIONAL: If count == 0 */ je_io_gamepad_handle_t *out_gamepads)
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
    /* OPTIONAL */ je_timestamp_ms_t *out_last_pushed_time_may_null)
{
    if (out_last_pushed_time_may_null != nullptr)
        *out_last_pushed_time_may_null = gamepad->_last_update_time;

    return gamepad->_enabled;
}
bool je_io_gamepad_button_down_get(
    je_io_gamepad_handle_t gamepad, je_gamepadcode_t code)
{
    if (!gamepad->_enabled)
        return false;

    return gamepad->_key_states[(size_t)code];
}
void je_io_gamepad_button_state_update(
    je_io_gamepad_handle_t gamepad, je_gamepadcode_t code, bool down)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }

    if ((gamepad->_key_states[(size_t)code] = down))
        gamepad->_last_update_time = je_clock_time_stamp();
}

void je_io_gamepad_stick_get(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float *out_x, float *out_y)
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
void je_io_gamepad_stick_update(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float x, float y)
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

void je_io_gamepad_stick_deadzone_set(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float deadzone)
{
    if (!gamepad->_enabled)
    {
        jeecs::debug::logerr("Cannot update for closed gamepad: %p.", gamepad);
        return;
    }
    gamepad->_stick_states[(uint8_t)stickid]._deadzone = deadzone;
}
