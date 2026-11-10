#pragma once

// ============================================================================
// jeecs/io.hpp —— 输入/输出：键鼠/手柄/窗口 je_io_* C API 与输入枚举、jeecs::input 常量
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/clock.hpp"

// ---- 输入码（input）C 枚举 ----
// 原 jeecs::input 的 enum class 已迁移为 C 枚举（je_*），C++ 直接使用。
// 枚举项加 JE_<KIND>_ 前缀以避免不同输入码间的同名冲突（如 LEFT/RIGHT/A/B）。
typedef enum je_mousecode_t {
    JE_MOUSE_LEFT,
    JE_MOUSE_MID,
    JE_MOUSE_RIGHT,

    JE_MOUSE_CUSTOM_0 = 16,
    JE_MOUSE_CUSTOM_1,
    JE_MOUSE_CUSTOM_2,
    JE_MOUSE_CUSTOM_3,
    JE_MOUSE_CUSTOM_4,
    JE_MOUSE_CUSTOM_5,
    JE_MOUSE_CUSTOM_6,
    JE_MOUSE_CUSTOM_7,
    JE_MOUSE_CUSTOM_8,

    JE_MOUSE_COUNT,
} je_mousecode_t;

typedef enum je_keycode_t {
    JE_KEY_UNKNOWN = 0,

    JE_KEY_APOSTROPHE = '\'',
    JE_KEY_COMMA = ',',
    JE_KEY_MINUS = '-',
    JE_KEY_PERIOD = '.',
    JE_KEY_SLASH = '/',

    JE_KEY_A = 'A',
    JE_KEY_B,
    JE_KEY_C,
    JE_KEY_D,
    JE_KEY_E,
    JE_KEY_F,
    JE_KEY_G,
    JE_KEY_H,
    JE_KEY_I,
    JE_KEY_J,
    JE_KEY_K,
    JE_KEY_L,
    JE_KEY_M,
    JE_KEY_N,
    JE_KEY_O,
    JE_KEY_P,
    JE_KEY_Q,
    JE_KEY_R,
    JE_KEY_S,
    JE_KEY_T,
    JE_KEY_U,
    JE_KEY_V,
    JE_KEY_W,
    JE_KEY_X,
    JE_KEY_Y,
    JE_KEY_Z,
    JE_KEY_1 = '1',
    JE_KEY_2,
    JE_KEY_3,
    JE_KEY_4,
    JE_KEY_5,
    JE_KEY_6,
    JE_KEY_7,
    JE_KEY_8,
    JE_KEY_9,
    JE_KEY_0,
    JE_KEY_SPACE = ' ',

    JE_KEY_SEMICOLON = ';',
    JE_KEY_EQUAL = '=',
    JE_KEY_LEFT_BRACKET = '[',
    JE_KEY_BACKSLASH = '\\',
    JE_KEY_RIGHT_BRACKET = ']',
    JE_KEY_GRAVE_ACCENT = '`',

    JE_KEY_L_SHIFT = 128,
    JE_KEY_R_SHIFT,
    JE_KEY_L_CTRL,
    JE_KEY_R_CTRL,
    JE_KEY_L_ALT,
    JE_KEY_R_ALT,
    JE_KEY_TAB,
    JE_KEY_ENTER,
    JE_KEY_ESC,
    JE_KEY_BACKSPACE,

    JE_KEY_NP_0,
    JE_KEY_NP_1,
    JE_KEY_NP_2,
    JE_KEY_NP_3,
    JE_KEY_NP_4,
    JE_KEY_NP_5,
    JE_KEY_NP_6,
    JE_KEY_NP_7,
    JE_KEY_NP_8,
    JE_KEY_NP_9,
    JE_KEY_NP_DECIMAL,
    JE_KEY_NP_DIVIDE,
    JE_KEY_NP_MULTIPLY,
    JE_KEY_NP_SUBTRACT,
    JE_KEY_NP_ADD,
    JE_KEY_NP_ENTER,

    JE_KEY_UP,
    JE_KEY_DOWN,
    JE_KEY_LEFT,
    JE_KEY_RIGHT,

    JE_KEY_F1,
    JE_KEY_F2,
    JE_KEY_F3,
    JE_KEY_F4,
    JE_KEY_F5,
    JE_KEY_F6,
    JE_KEY_F7,
    JE_KEY_F8,
    JE_KEY_F9,
    JE_KEY_F10,
    JE_KEY_F11,
    JE_KEY_F12,
    JE_KEY_F13,
    JE_KEY_F14,
    JE_KEY_F15,
    JE_KEY_F16,

    JE_KEY_CUSTOM_0 = 256,
    JE_KEY_CUSTOM_1,
    JE_KEY_CUSTOM_2,
    JE_KEY_CUSTOM_3,
    JE_KEY_CUSTOM_4,
    JE_KEY_CUSTOM_5,
    JE_KEY_CUSTOM_6,
    JE_KEY_CUSTOM_7,
    JE_KEY_CUSTOM_8,

    JE_KEY_COUNT,
} je_keycode_t;

typedef enum je_gamepadcode_t {
    JE_GAMEPAD_UP,
    JE_GAMEPAD_DOWN,
    JE_GAMEPAD_LEFT,
    JE_GAMEPAD_RIGHT,

    JE_GAMEPAD_A,
    JE_GAMEPAD_B,
    JE_GAMEPAD_X,
    JE_GAMEPAD_Y,

    JE_GAMEPAD_LB,
    JE_GAMEPAD_RB,
    JE_GAMEPAD_LS,
    JE_GAMEPAD_RS,

    JE_GAMEPAD_SELECT,
    JE_GAMEPAD_START,
    JE_GAMEPAD_GUIDE,

    JE_GAMEPAD_COUNT,
} je_gamepadcode_t;

typedef enum je_joystickcode_t {
    JE_JOY_L,
    JE_JOY_R,
    JE_JOY_LT, // Use x value only.
    JE_JOY_RT, // Use x value only.

    JE_JOY_COUNT,
} je_joystickcode_t;
JE_FORCE_CAPI

/*
je_io_key_state_update [基本接口]
更新指定键的状态信息
*/
JE_API void je_io_key_state_update(je_keycode_t keycode, bool keydown);

/*
je_io_mouse_pos_update [基本接口]
更新鼠标（或触摸位置点）的坐标
    * 此操作`不会`影响光标的实际位置
*/
JE_API void je_io_mouse_pos_update(size_t group, int x, int y);

/*
je_io_mouse_state_update [基本接口]
更新鼠标（或触摸点）的状态
*/
JE_API void je_io_mouse_state_update(size_t group, je_mousecode_t key, bool keydown);

/*
je_io_window_size_update [基本接口]
更新窗口大小
    * 此操作`不会`影响窗口的实际大小
*/
JE_API void je_io_window_size_update(int x, int y);

/*
je_io_window_pos_update [基本接口]
更新窗口位置
    * 此操作`不会`影响窗口的实际位置
*/
JE_API void je_io_window_pos_update(int x, int y);

/*
je_io_wheel_update [基本接口]
更新鼠标滚轮的计数
*/
JE_API void je_io_wheel_update(size_t group, float x, float y);

/*
je_io_key_down_get [基本接口]
获取指定的按键是否被按下
*/
JE_API bool je_io_key_down_get(je_keycode_t keycode);

/*
je_io_mouse_pos_get [基本接口]
获取鼠标的坐标
*/
JE_API void je_io_mouse_pos_get(size_t group, int* out_x, int* out_y);

/*
je_io_mouse_state_get [基本接口]
获取鼠标的按键状态
*/
JE_API bool je_io_mouse_state_get(size_t group, je_mousecode_t key);

/*
je_io_window_size_get [基本接口]
获取窗口的大小
*/
JE_API void je_io_window_size_get(int* out_x, int* out_y);

/*
je_io_window_pos_get [基本接口]
获取窗口的位置
*/
JE_API void je_io_window_pos_get(int* out_x, int* out_y);

/*
je_io_wheel_get [基本接口]
获取鼠标滚轮的计数
*/
JE_API void je_io_wheel_get(size_t group, float* out_x, float* out_y);

/*
je_io_mouse_lock_set [基本接口]
设置是否需要将鼠标锁定
*/
JE_API void je_io_mouse_lock_set(bool lock);

/*
je_io_mouse_lock_get [基本接口]
获取当前是否应该锁定鼠标
*/
JE_API bool je_io_mouse_lock_get();

/*
je_io_window_size_set [基本接口]
请求对窗口大小进行调整
*/
JE_API void je_io_window_size_set(int x, int y);

/*
je_io_window_size_fetch_update [基本接口]
获取当前窗口大小是否应该调整及调整的大小
*/
JE_API bool je_io_window_size_fetch_update(int* out_x, int* out_y);

/*
je_io_window_title_set [基本接口]
请求对窗口标题进行调整
*/
JE_API void je_io_window_title_set(const char* title);

/*
je_io_window_title_fetch_update [基本接口]
获取当前是否需要对窗口标题进行调整及调整之后的内容
*/
JE_API bool je_io_window_title_fetch_update(const char** out_title);

/*
je_io_gamepad_handle_t [类型]
指示虚拟手柄实例
*/
typedef struct je_gamepad_state_t* je_io_gamepad_handle_t;

/*
je_io_gamepad_create [基本接口]
创建一个虚拟手柄实例
    * name 可用于以可读形式分辨不同的控制器设备，如果为 nullptr，则分配一个默认的名称
    * guid 可用于区分不同的物理设备，如果为nullptr，则由引擎生成。
*/
JE_API je_io_gamepad_handle_t je_io_gamepad_create(
    /* OPTIONAL */ const char* name_may_null, /* OPTIONAL */ const char* guid_may_null);
/*
je_io_gamepad_close [基本接口]
关闭一个虚拟手柄实例
*/
JE_API void je_io_gamepad_close(je_io_gamepad_handle_t gamepad);

/*
je_io_gamepad_name [基本接口]
获取指定虚拟手柄的名称
    * 名称在创建时指定，用于以可读形式分辨不同的控制器设备
参见：
    je_io_gamepad_create
*/
JE_API const char* je_io_gamepad_name(je_io_gamepad_handle_t gamepad);
/*
je_io_gamepad_guid [基本接口]
获取指定虚拟手柄的GUID
    * GUID在创建时指定，用于区分不同的物理设备
参见：
    je_io_gamepad_create
*/
JE_API const char* je_io_gamepad_guid(je_io_gamepad_handle_t gamepad);
/*
je_io_gamepad_get [基本接口]
获取所有虚拟手柄的句柄
    * count 用于指示传入的out_gamepads可容纳的最大数量，如果实际手柄数量大于count，
        则只返回前count个句柄；如果实际手柄数量小于count，则返回实际数量。
    * 作为特例，当 count 为 0 时，out_gamepads 可以为 nullptr，此时函数返回实际手柄数量。
*/
JE_API size_t je_io_gamepad_get(size_t count, /* OPTIONAL: If count == 0 */ je_io_gamepad_handle_t* out_gamepads);

/*
je_io_gamepad_is_active [基本接口]
检查指定的虚拟手柄，其对应的实际设备是否存在
    * 虚拟手柄和输入设备的对应关系通过 je_io_gamepad_create 构建，
        其不一定是实际的物理游戏手柄；例如，可以将键盘按键的按动情况
        映射到虚拟手柄上。当 je_io_gamepad_close 调用时，虚拟手柄实例
        会被销毁，此时 je_io_gamepad_is_active 返回 false。
    * 如果 out_last_pushed_time_may_null 不为 nullptr，则将返回手柄的最后
        操作时间。
参见：
    je_io_gamepad_create
    je_io_gamepad_close
*/
JE_API bool je_io_gamepad_is_active(
    je_io_gamepad_handle_t gamepad,
    /* OPTIONAL */ je_timestamp_ms_t* out_last_pushed_time_may_null);
/*
je_io_gamepad_button_down_get [基本接口]
获取指定虚拟手柄的指定按键是否被按下
    * code 是一个枚举值，用于指示按键的类型
    * 若指定的按键不存在，则始终返回 false
    * 如果手柄已经被断开（je_io_gamepad_close），调用此接口依然是合法的，但返回值始终是 false
参见：
    je_io_gamepad_close
*/
JE_API bool je_io_gamepad_button_down_get(
    je_io_gamepad_handle_t gamepad, je_gamepadcode_t code);
/*
je_io_gamepad_button_state_update [基本接口]
更新指定的虚拟手柄按键状态，可以被 je_io_gamepad_button_down_get 获取
    * code 是一个枚举值，用于指示按键的类型
    * 不允许对已经断开（je_io_gamepad_close）的手柄进行此操作
    * 如果输入坐标的模长大于1，坐标将被归一化
参见：
    je_io_gamepad_button_down_get
    je_io_gamepad_close
*/
JE_API void je_io_gamepad_button_state_update(
    je_io_gamepad_handle_t gamepad, je_gamepadcode_t code, bool down);

/*
je_io_gamepad_stick_get [基本接口]
获取指定虚拟手柄的指定摇杆的坐标
    * stickid 是一个枚举值，用于指示摇杆的类型
    * out_x 和 out_y 用于接收摇杆的坐标，坐标的模长取值范围是 [0, 1]
    * 如果手柄已经被断开（je_io_gamepad_close），调用此接口依然是合法的，但获取到的坐标都将是 0
    * 获取扳机键（LT, RT）时，请使用 x 值，y 值始终为 0
参见：
    je_io_gamepad_close
*/
JE_API void je_io_gamepad_stick_get(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float* out_x, float* out_y);
/*
je_io_gamepad_stick_update [基本接口]
更新指定虚拟手柄的指定摇杆的坐标
    * stickid 是一个枚举值，用于指示摇杆的类型
    * x 和 y 是摇杆的坐标，应当确保坐标的模长不大于1
    * 不允许对已经断开（je_io_gamepad_close）的手柄进行此操作
    * 获取扳机键（LT, RT）时，请使用 x 值，并保持y 值始终为 0
    * 当摇杆向上拨动时，y 值应当为正数；当摇杆向右拨动时，x 值应当为正数；
    * 当扳机键按下时，x 值应当为正数
参见：
    je_io_gamepad_close
*/
JE_API void je_io_gamepad_stick_update(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float x, float y);

/*
je_io_gamepad_stick_deadzone_set [基本接口]
设置指定虚拟手柄的指定摇杆的死区
    * stickid 是一个枚举值，用于指示摇杆的类型
    * 当设置摇杆的坐标（je_io_gamepad_stick_update）时，如果坐标的模长小于死区，则坐标将被视为0
*/
JE_API void je_io_gamepad_stick_deadzone_set(
    je_io_gamepad_handle_t gamepad, je_joystickcode_t stickid, float deadzone);
typedef je_mousecode_t je_Mousecode;
typedef je_keycode_t je_Keycode;
typedef je_gamepadcode_t je_Gamepadcode;
typedef je_joystickcode_t je_Joystickcode;
typedef je_io_gamepad_handle_t je_IoGamepadHandle;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::input [命名空间]
    此处定义引擎IO相关的接口、常量等
    */
    namespace input
    {
        constexpr size_t MAX_MOUSE_GROUP_COUNT = 16;

        // 原 je_mousecode_t/je_keycode_t/je_gamepadcode_t/je_joystickcode_t 的 enum class 已迁移为
        // C 枚举 je_mousecode_t/je_keycode_t/je_gamepadcode_t/je_joystickcode_t（见文件顶部 C ABI 段）。
    }
}
#endif // __cplusplus
