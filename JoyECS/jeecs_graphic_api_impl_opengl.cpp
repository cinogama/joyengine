#pragma once

#define JE_IMPL
#include "jeecs.hpp"

// Here is low-level-graphic-api impl.
// OpenGL version.

std::atomic_flag _graphic_terminate_flag;

void opengl_graphic_work_thread(jegl_interface_config config)
{
    double time_per_sec = 1.0 / config.m_fps;
    double current_frame_time = je_clock_time();
    while (_graphic_terminate_flag.test_and_set())
    {
        // Ready for rend..



        if (je_clock_time() - current_frame_time >= 1.0)
            current_frame_time = je_clock_time();

        je_clock_sleep_until(current_frame_time += time_per_sec);
    }
}

jegl_thread* jegl_start_graphic_thread(jegl_interface_config config)
{
    jegl_thread* th = jeecs::basic::create_new<jegl_thread>();

    // Take place.
    _graphic_terminate_flag.test_and_set();

    th->_m_thread =
        jeecs::basic::create_new<std::thread>(
            opengl_graphic_work_thread,
            config);

    return th;
}

void jegl_terminate_graphic_thread(jegl_thread* thread)
{
    assert(_graphic_terminate_flag.test_and_set());
    _graphic_terminate_flag.clear();

    thread->_m_thread->join();
}