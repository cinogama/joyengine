#pragma once

#include "jeecs.hpp"

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

namespace jeecs::graphic::metal
{
    class application_delegate
        : public NS::ApplicationDelegate
        , public NS::ViewDelegate
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
    public:
        struct user_interface_context_t
        {
            NS::Window* m_window;

            MTL::Device* m_metal_device;
            MTK::View* m_metal_view;
        };
    private:
        bool m_graphic_request_to_close;
        std::optional<user_interface_context_t> m_ui_context;
    public:
        application_delegate(graphic_syncer_host* ready_graphic_host)
            : NS::ApplicationDelegate()
            , NS::ViewDelegate()
            , m_engine_graphic_host(ready_graphic_host)
            , m_graphic_request_to_close(false)
            , m_ui_context(std::nullopt)
        {
        }
        ~application_delegate()
        {
            if (m_ui_context.has_value())
            {
                auto& context = m_ui_context.value();

                context.m_metal_view->release();
                context.m_metal_device->release();
                context.m_window->release();
            }
        }
    public:
        virtual void drawInMTKView(MTK::View* pView) override
        {
            if (m_graphic_request_to_close)
                // Graphic has been requested to close.
                return;

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
            auto& metal_ui_context = m_ui_context.emplace();

            jegl_context* engine_raw_graphic_context =
                m_engine_graphic_host->get_graphic_context_after_context_ready();

            jegl_interface_config& engine_raw_graphic_config =
                engine_raw_graphic_context->m_config;

            CGRect frame = (CGRect){
                {100.0, 100.0},
                {
                    (double)engine_raw_graphic_config.m_width,
                    (double)engine_raw_graphic_config.m_height} };

            metal_ui_context.m_window = NS::Window::alloc()->init(
                frame,
                NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled,
                NS::BackingStoreBuffered,
                false);

            metal_ui_context.m_metal_device = MTL::CreateSystemDefaultDevice();
            assert(metal_ui_context.m_metal_device != nullptr);

            metal_ui_context.m_metal_view =
                MTK::View::alloc()->init(frame, metal_ui_context.m_metal_device);
            metal_ui_context.m_metal_view->setColorPixelFormat(
                MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
            metal_ui_context.m_metal_view->setClearColor(
                MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));

            metal_ui_context.m_metal_view->setDelegate(this);
            metal_ui_context.m_window->setContentView(metal_ui_context.m_metal_view);

            metal_ui_context.m_window->setTitle(
                NS::String::string(
                    engine_raw_graphic_config.m_title,
                    NS::StringEncoding::UTF8StringEncoding));
            metal_ui_context.m_window->makeKeyAndOrderFront(nullptr);

            application->activateIgnoringOtherApps(true);
        }
        virtual bool applicationShouldTerminateAfterLastWindowClosed(
            NS::Application* pSender) override
        {
            return true;
        }
    };
}