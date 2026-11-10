#pragma once

// ============================================================================
// jeecs/input.hpp —— 输入辅助（C++）：jeecs::input 查询接口、gamepad 类与 is_up/first_down/double_click 宏
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/io.hpp"
#include "jeecs/math.hpp"
#include "jeecs/basic.hpp"

#ifdef __cplusplus
namespace jeecs
{
    namespace input
    {
        inline bool keydown(je_keycode_t key)
        {
            return je_io_key_down_get(key);
        }
        inline bool mousedown(size_t group, je_mousecode_t key)
        {
            return je_io_mouse_state_get(group, key);
        }
        inline math::vec2 wheel(size_t group)
        {
            float x, y;
            je_io_wheel_get(group, &x, &y);
            return { x, y };
        }
        inline math::ivec2 mousepos(size_t group)
        {
            int x, y;
            je_io_mouse_pos_get(group, &x, &y);
            return { x, y };
        }
        inline math::vec2 mouseviewpos(size_t group)
        {
            int x, y, w, h;
            je_io_mouse_pos_get(group, &x, &y);
            je_io_window_size_get(&w, &h);
            return { ((float)x / (float)w - 0.5f) * 2.0f, ((float)y / (float)h - 0.5f) * -2.0f };
        }
        inline math::ivec2 windowsize()
        {
            int x, y;
            je_io_window_size_get(&x, &y);
            return { x, y };
        }
        inline math::ivec2 windowpos()
        {
            int x, y;
            je_io_window_pos_get(&x, &y);
            return { x, y };
        }

        template <je_type_hash_t hash_v1, int v2>
        static bool _isUp(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (!keystate) && lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template <je_type_hash_t hash_v1, int v2>
        static bool _firstDown(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (keystate) && !lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template <je_type_hash_t hash_v1, int v2>
        static bool _doubleClick(bool keystate, float i = 0.1f)
        {
            static je_timestamp_ms_t lact_click_tm = je_clock_time_stamp();
            static bool release_for_next_click = false;

            auto cur_time = je_clock_time_stamp();

            // 1. first click.
            if (keystate)
            {
                if (release_for_next_click && cur_time - lact_click_tm < (je_timestamp_ms_t)(i * 1000.f))
                {
                    // Is Double click!
                    lact_click_tm = 0; // reset the time.
                    release_for_next_click = false;
                    return true;
                }
                // Release for a long time, re calc the time
                if (!release_for_next_click || cur_time - lact_click_tm > (je_timestamp_ms_t)(i * 1000.f))
                {
                    lact_click_tm = cur_time;
                    release_for_next_click = false;
                }
            }
            else
            {
                if (!release_for_next_click && cur_time - lact_click_tm < (je_timestamp_ms_t)(i * 1000.f))
                    release_for_next_click = true;
            }
            return false;
        }

        [[maybe_unused]] void is_up(...);
        [[maybe_unused]] void first_down(...);
        [[maybe_unused]] void double_click(...); // just for fool ide

        class gamepad
        {
            je_io_gamepad_handle_t m_gamepad_handle;

        public:
            gamepad(je_io_gamepad_handle_t gamepad_handle)
                : m_gamepad_handle(gamepad_handle)
            {
            }
            gamepad(const gamepad&) = default;
            gamepad(gamepad&&) = default;
            gamepad& operator=(const gamepad&) = default;
            gamepad& operator=(gamepad&&) = default;
            ~gamepad() = default;

            bool button(je_gamepadcode_t button) const
            {
                return je_io_gamepad_button_down_get(m_gamepad_handle, button);
            }
            math::vec2 stick(je_joystickcode_t stick) const
            {
                float x, y;
                je_io_gamepad_stick_get(m_gamepad_handle, stick, &x, &y);
                return { x, y };
            }
            bool actived(je_timestamp_ms_t* out_last_update_time_may_null) const
            {
                return je_io_gamepad_is_active(
                    m_gamepad_handle, out_last_update_time_may_null);
            }

        private:
            static std::vector<je_io_gamepad_handle_t> _get_all_handle()
            {
                size_t current_gamepad_count = je_io_gamepad_get(0, nullptr);
                std::vector<je_io_gamepad_handle_t> gamepad_handles(current_gamepad_count);

                je_io_gamepad_get(current_gamepad_count, gamepad_handles.data());
                return gamepad_handles;
            }

        public:
            static std::vector<gamepad> all()
            {
                auto gamepad_handles = _get_all_handle();

                std::vector<gamepad> gamepads;
                gamepads.reserve(gamepad_handles.size());

                for (auto& handle : gamepad_handles)
                {
                    gamepads.emplace_back(handle);
                }
                return gamepads;
            }
            static std::optional<gamepad> last()
            {
                auto gamepad_handles = _get_all_handle();

                je_timestamp_ms_t last_update_time = 0;
                size_t last_index = SIZE_MAX;

                const size_t size = gamepad_handles.size();
                const je_io_gamepad_handle_t* data = gamepad_handles.data();

                for (size_t i = 0; i < size; ++i)
                {
                    je_timestamp_ms_t cur_time;
                    if (je_io_gamepad_is_active(data[i], &cur_time))
                    {
                        if (cur_time > last_update_time)
                        {
                            last_update_time = cur_time;
                            last_index = i;
                        }
                    }
                }

                if (last_index != SIZE_MAX)
                    return gamepad(gamepad_handles[last_index]);

                return std::nullopt;
            }
        };

#define is_up _isUp<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
#define first_down _firstDown<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
#define double_click _doubleClick<jeecs::basic::hash_compile_time(__FILE__), __LINE__>
    }
}
#endif // __cplusplus
