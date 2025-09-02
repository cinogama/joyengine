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

    je_macos_context context(argc, argv);
    context.macos_loop();

    jeecs::game_universe::destroy_universe(u);
}