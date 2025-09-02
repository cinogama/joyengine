/*
 * JoyECS
 * -----------------------------------
 * JoyECS is a interesting ecs-impl.
 *
 */
#include "jeecs.hpp"

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include "../src/jeecs_graphic_api_interface_cocoa.hpp"

class je_macos_context : public jeecs::game_engine_context
{
    JECS_DISABLE_MOVE_AND_COPY(je_macos_context);

    jeecs::graphic::graphic_syncer_host* m_graphic_host;

public:
    je_macos_context(int argc, char** argv)
        : jeecs::game_engine_context(argc, argv)
    {
        m_graphic_host = prepare_graphic(false /* debug now */);
    }
    ~je_macos_context()
    {
    }
    void macos_loop()
    {
        for (;;)
        {
            if (!m_graphic_host->check_context_ready_block())
                break; // If the entry script ended, exit the loop.

            // Graphic context ready, prepare for macos window.
            NS::AutoreleasePool* auto_release_pool =
                NS::AutoreleasePool::alloc()->init();

            jeecs::graphic::metal::application_delegate del(m_graphic_host);

            NS::Application* shared_application = NS::Application::sharedApplication();
            shared_application->setDelegate(&del);
            shared_application->run();

            auto_release_pool->release();
        }
    }
};

int main(int argc, char** argv)
{
    ///////////////////////////////// DEV /////////////////////////////////
    jeecs::game_universe u = jeecs::game_universe::create_universe();
    std::thread([&]() {

        je_clock_sleep_for(3.0);
        jeecs::debug::loginfo("Creating graphic host...");

        jegl_interface_config config;
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

    je_macos_context context(argc, argv);
    context.macos_loop();

    jeecs::game_universe::destroy_universe(u);
}