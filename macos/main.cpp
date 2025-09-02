/*
 * JoyECS
 * -----------------------------------
 * JoyECS is a interesting ecs-impl.
 *
 */
#include "jeecs.hpp"


#include "../src/jeecs_graphic_api_interface_cocoa.hpp"

int main(int argc, char** argv)
{
    ///////////////////////////////// DEV /////////////////////////////////
    jeecs::game_universe u = jeecs::game_universe::create_universe();
    std::thread([&]() {

        je_clock_sleep_for(3.0);
        jeecs::debug::loginfo("Creating graphic host...");

        jegl_interface_config config ;
        config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
        config.m_enable_resize = true;
        config.m_msaa = 0;
        config.m_width = 512;
        config.m_height = 512;
        config.m_fps = 0;
        config.m_title = "Demo";
        config.m_userdata = nullptr;

        jegl_uhost_get_or_create_for_universe(u.handle(), &config);

        }).detach();

    jegl_cocoa_metal_application_run(argc, argv);

    jeecs::game_universe::destroy_universe(u);
}