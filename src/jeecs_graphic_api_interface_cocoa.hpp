#pragma once

#include "jeecs.hpp"

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

namespace jeecs::graphic::metal
{
    struct window_view_layout
    {
        NS::Window* m_window;
        MTK::View* m_metal_view;

        int m_rend_rect_width;
        int m_rend_rect_height;

        JECS_DISABLE_MOVE_AND_COPY(window_view_layout);

        window_view_layout(
            const jegl_interface_config* cfg, MTL::Device* device)
        {
            const char* title = cfg->m_title;
            double width = (double)cfg->m_width;
            double height = (double)cfg->m_height;

            assert(device != nullptr);
            CGRect frame = (CGRect){ {100.0, 100.0}, {width, height} };

            auto window_config_attrib = 
                NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled;

            if (cfg->m_enable_resize)
                window_config_attrib |= NS::WindowStyleMaskResizable;

            m_window = NS::Window::alloc()->init(
                frame,
                window_config_attrib,
                NS::BackingStoreBuffered,
                false);

            m_metal_view =
                MTK::View::alloc()->init(frame, device);
            m_metal_view->setColorPixelFormat(
                MTL::PixelFormat::PixelFormatBGRA8Unorm);
            m_metal_view->setDepthStencilPixelFormat(
                MTL::PixelFormatDepth16Unorm);

            m_metal_view->setClearDepth(1.0);
            m_metal_view->setClearColor(
                MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));

            m_window->setContentView(m_metal_view);

            m_window->setTitle(
                NS::String::string(
                    title, NS::StringEncoding::UTF8StringEncoding));
            m_window->makeKeyAndOrderFront(nullptr);
        }
        ~window_view_layout()
        {
            m_metal_view->release();
            m_window->release();
        }
    };

    class application_delegate
        : public NS::ApplicationDelegate
        , public MTK::ViewDelegate
    {
        static NS::Menu* createMenuBar()
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
        graphic_syncer_host* m_engine_graphic_host;

    private:
        window_view_layout* m_window_view_layout_instance;
        bool m_graphic_request_to_close;

    public:
        application_delegate(graphic_syncer_host* ready_graphic_host)
            : NS::ApplicationDelegate()
            , MTK::ViewDelegate()
            , m_engine_graphic_host(ready_graphic_host)
            , m_graphic_request_to_close(false)
        {
        }
        ~application_delegate()
        {
        }
    public:
        virtual void drawInMTKView(MTK::View* pView) override
        {
            if (m_graphic_request_to_close)
                // Graphic has been requested to close.
                return;

            CGSize drawableSize = pView->drawableSize();
            int width = (int)drawableSize.width;
            int height = (int)drawableSize.height;

            if (m_window_view_layout_instance->m_rend_rect_width != width
                || m_window_view_layout_instance->m_rend_rect_height != height)
            {
                // Notify engine graphic host to resize.
                m_window_view_layout_instance->m_rend_rect_width = width;
                m_window_view_layout_instance->m_rend_rect_height = height;

                je_io_update_window_size(width, height);
            }

            if (!m_engine_graphic_host->frame())
            {
                // TODO: Graphic requested to close, close windows and exit app.
                m_graphic_request_to_close = true;
            }
        }

        virtual void applicationWillFinishLaunching(
            NS::Notification* pNotification) override
        {
            NS::Application* application =
                reinterpret_cast<NS::Application*>(pNotification->object());

            application->setMainMenu(createMenuBar());
            application->setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
        }
        virtual void applicationDidFinishLaunching(
            NS::Notification* pNotification) override
        {
            NS::Application* application =
                reinterpret_cast<NS::Application*>(pNotification->object());

            assert(m_engine_graphic_host != nullptr);

            jegl_context* engine_raw_graphic_context =
                m_engine_graphic_host->get_graphic_context_after_context_ready();
            auto& config = engine_raw_graphic_context->m_config;


            m_window_view_layout_instance =
                reinterpret_cast<window_view_layout*>(config.m_userdata);

            m_window_view_layout_instance->m_metal_view->setDelegate(this);

            application->activateIgnoringOtherApps(true);
        }
        virtual bool applicationShouldTerminateAfterLastWindowClosed(
            NS::Application* pSender) override
        {
            return true;
        }
    };
}