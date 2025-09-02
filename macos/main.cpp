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

    class macos_application_delegate : public NS::ApplicationDelegate
    {
        jeecs::graphic::graphic_syncer_host*
            m_je_graphic_host;

        NS::Window* m_window;
        jeecs::graphic::metal::metal_view_delegate* m_view_delegate;
    public:
        macos_application_delegate(
            jeecs::graphic::graphic_syncer_host* graphic_host)
            : NS::ApplicationDelegate()
            , m_je_graphic_host(graphic_host)
            , m_window(nullptr)
            , m_mtk_view(nullptr)
            , m_device(nullptr)
            , m_view_delegate(nullptr)
        {
        }
        ~macos_application_delegate()
        {
            delete m_view_delegate;
            m_window->release();
        }

        NS::Menu* createMenuBar()
        {
            using NS::StringEncoding::UTF8StringEncoding;

            NS::Menu* pMainMenu = NS::Menu::alloc()->init();
            NS::MenuItem* pAppMenuItem = NS::MenuItem::alloc()->init();
            NS::Menu* pAppMenu = NS::Menu::alloc()->init(NS::String::string("Appname", UTF8StringEncoding));

            NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
            NS::String* quitItemName = NS::String::string("Quit ", UTF8StringEncoding)->stringByAppendingString(appName);
            SEL quitCb = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* pSender) {
                auto pApp = NS::Application::sharedApplication();
                pApp->terminate(pSender);
                });

            NS::MenuItem* pAppQuitItem = pAppMenu->addItem(quitItemName, quitCb, NS::String::string("q", UTF8StringEncoding));
            pAppQuitItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);
            pAppMenuItem->setSubmenu(pAppMenu);

            NS::MenuItem* pWindowMenuItem = NS::MenuItem::alloc()->init();
            NS::Menu* pWindowMenu = NS::Menu::alloc()->init(NS::String::string("Window", UTF8StringEncoding));

            SEL closeWindowCb = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {
                auto pApp = NS::Application::sharedApplication();
                pApp->windows()->object< NS::Window >(0)->close();
                });
            NS::MenuItem* pCloseWindowItem = pWindowMenu->addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindowCb, NS::String::string("w", UTF8StringEncoding));
            pCloseWindowItem->setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

            pWindowMenuItem->setSubmenu(pWindowMenu);

            pMainMenu->addItem(pAppMenuItem);
            pMainMenu->addItem(pWindowMenuItem);

            pAppMenuItem->release();
            pWindowMenuItem->release();
            pAppMenu->release();
            pWindowMenu->release();

            return pMainMenu->autorelease();
        }

        virtual void applicationWillFinishLaunching(
            NS::Notification* pNotification) override
        {
            NS::Menu* pMenu = createMenuBar();
            NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
            pApp->setMainMenu(pMenu);
            pApp->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
        }
        virtual void applicationDidFinishLaunching(
            NS::Notification* pNotification) override
        {
            const auto& engine_graphic_config =
                m_je_graphic_host->get_graphic_context_after_context_ready()->m_config;

            CGRect frame = (CGRect){
                {
                    100.0,
                    100.0,
                },
                {
                    (double)engine_graphic_config.m_width,
                    (double)engine_graphic_config.m_height,
                }
            };

            m_window = NS::Window::alloc()->init(
                frame,
                NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled,
                NS::BackingStoreBuffered,
                false);

            jeecs::graphic::metal::m_view_delegate =
                new metal_view_delegate(
                    m_window, m_je_graphic_host);

            m_window->setTitle(
                NS::String::string(
                    engine_graphic_config.m_title,
                    NS::StringEncoding::UTF8StringEncoding));

            m_window->makeKeyAndOrderFront(nullptr);

            NS::Application* pApp = reinterpret_cast<NS::Application*>(pNotification->object());
            pApp->activateIgnoringOtherApps(true);
        }
        virtual bool applicationShouldTerminateAfterLastWindowClosed(
            NS::Application* pSender) override
        {
            return true;
        }
    };

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

            macos_application_delegate del(m_graphic_host);

            NS::Application* shared_application = NS::Application::sharedApplication();
            shared_application->setDelegate(&del);
            shared_application->run();

            auto_release_pool->release();
        }
    }
};

int main(int argc, char** argv)
{
    je_macos_context context(argc, argv);
    context.macos_loop();
}