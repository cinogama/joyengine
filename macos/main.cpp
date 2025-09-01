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

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

class je_macos_context : public jeecs::game_engine_context
{
    JECS_DISABLE_MOVE_AND_COPY(je_macos_context);

    jeecs::graphic::graphic_syncer_host* m_graphic_host;

    class macos_demo_renderer
    {
    public:
        macos_demo_renderer(MTL::Device* pDevice)
            : _pDevice(pDevice->retain())
        {
            _pCommandQueue = _pDevice->newCommandQueue();
        }
        ~macos_demo_renderer()
        {
            _pCommandQueue->release();
            _pDevice->release();
        }
        void draw(MTK::View* pView)
        {
            NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

            MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
            MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
            MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
            pEnc->endEncoding();
            pCmd->presentDrawable(pView->currentDrawable());
            pCmd->commit();

            pPool->release();
        }

    private:
        MTL::Device* _pDevice;
        MTL::CommandQueue* _pCommandQueue;
    };

    class macos_je_mtk_view_delegate : public MTK::ViewDelegate
    {
        bool m_graphic_request_to_close;
        jeecs::graphic::graphic_syncer_host* m_je_graphic_host;
        macos_demo_renderer* m_renderer;

    public:
        macos_je_mtk_view_delegate(
            MTL::Device* pDevice,
            jeecs::graphic::graphic_syncer_host* graphic_host)
            : MTK::ViewDelegate()
            , m_graphic_request_to_close(false)
            , m_je_graphic_host(graphic_host)
            , m_renderer(new macos_demo_renderer(pDevice))
        {
        }
        virtual ~macos_je_mtk_view_delegate() override
        {
            if (!m_graphic_request_to_close)
            {
                // NOTE: view delegate has been destructed, but graphic host
                //  has not been requested to close.
                // This may happen when the window is closed by user in macos.

                jegl_sync_shutdown(
                    m_je_graphic_host->get_graphic_context_after_context_ready(), 
                    false);
            }
            delete m_renderer;
        }

        virtual void drawInMTKView(MTK::View* pView) override
        {
            //if (m_graphic_request_to_close)
            //    // Graphic has been requested to close.
            //    return;

            //if (!m_je_graphic_host->frame())
            //{
            //    // TODO: Graphic requested to close, close windows and exit app.
            //    m_graphic_request_to_close = true;
            //}
            m_renderer->draw(pView);
        }
    };
    class macos_je_application_delegate : public NS::ApplicationDelegate
    {
        jeecs::graphic::graphic_syncer_host* 
            m_je_graphic_host;

        NS::Window* m_window;
        MTK::View* m_mtk_view;
        MTL::Device* m_device;
        macos_je_mtk_view_delegate* m_view_delegate;
    public:
        macos_je_application_delegate(
            jeecs::graphic::graphic_syncer_host* graphic_host)
            : NS::ApplicationDelegate()
            , m_je_graphic_host(graphic_host)
            , m_window(nullptr)
            , m_mtk_view(nullptr)
            , m_device(nullptr)
            , m_view_delegate(nullptr)
        {
        }
        ~macos_je_application_delegate()
        {
            m_mtk_view->release();
            m_window->release();
            m_device->release();
            delete m_view_delegate;
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

            m_device = MTL::CreateSystemDefaultDevice();

            m_mtk_view = MTK::View::alloc()->init(frame, m_device);
            m_mtk_view->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
            m_mtk_view->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));

            m_view_delegate = new macos_je_mtk_view_delegate(m_device, m_je_graphic_host);
            m_mtk_view->setDelegate(m_view_delegate);

            m_window->setContentView(m_mtk_view);
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

            macos_je_application_delegate del(m_graphic_host);

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