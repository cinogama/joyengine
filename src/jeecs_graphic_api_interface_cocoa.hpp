#pragma once

#include "jeecs.hpp"

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

#include <optional>

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
        };
    private:
        std::optional<user_interface_context_t> m_ui_context;
    public:
        application_delegate(graphic_syncer_host* ready_graphic_host)
            : NS::ApplicationDelegate()
            , NS::ViewDelegate()
            , m_engine_graphic_host(ready_graphic_host)
        {
        }
        ~application_delegate()
        {
            if (m_ui_context.has_value())
            {
                auto& context = m_ui_context.value();
            }
        }
    public:
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
            auto& context = m_ui_context.emplace();

            jegl_context* engine_raw_graphic_context =
                m_engine_graphic_host->get_graphic_context_after_context_ready();

            jegl_interface_config& engine_raw_graphic_config =
                engine_raw_graphic_context->m_config;


            context.
        }
        virtual bool applicationShouldTerminateAfterLastWindowClosed(
            NS::Application* pSender) override
        {
            return true;
        }
    };
}