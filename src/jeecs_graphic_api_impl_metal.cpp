#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_METAL_GAPI

#include "jeecs_imgui_backend_api.hpp"

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include "jeecs_graphic_api_interface_cocoa.hpp"

namespace jeecs::graphic::api::metal
{
    struct jegl_metal_context
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_metal_context);

        struct user_interface_context_t
        {
            NS::Window* m_window;

            MTL::Device* m_metal_device;
            MTK::View* m_metal_view;
        };

        jegl_metal_context() = default;
        ~jegl_metal_context() = default;

        // m_user_interface_context will be init by `applicationDidFinishLaunching`
        user_interface_context_t* m_user_interface_context;
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
        bool m_graphic_request_to_close;
        std::optional<jegl_metal_context::user_interface_context_t> m_ui_context;
    public:
        application_delegate(graphic_syncer_host* ready_graphic_host)
            : NS::ApplicationDelegate()
            , MTK::ViewDelegate()
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

            jegl_metal_context* engine_graphic_context
                reinterpret_cast<jegl_metal_context*>(engine_raw_graphic_context->m_graphic_impl_context);

            const jegl_interface_config& engine_raw_graphic_config =
                engine_raw_graphic_context->m_config;

            engine_graphic_context->m_user_interface_context = &metal_ui_context;

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

    jegl_context::graphic_impl_context_t
        startup(jegl_context* glthread, const jegl_interface_config*, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Metal) start!");

        /*jegui_init_metal(
            glthread,
            [](jegl_context*, jegl_resource*)
            {
                return (uint64_t)nullptr;
            },
            [](jegl_context*, jegl_resource*) {});*/

        jegl_metal_context* context = new jegl_metal_context();
        return context;
    }
    void pre_shutdown(jegl_context*, jegl_context::graphic_impl_context_t, bool)
    {
    }
    void shutdown(jegl_context*, jegl_context::graphic_impl_context_t ctx, bool reboot)
    {
        jegl_metal_context* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        if (!reboot)
            jeecs::debug::log("Graphic thread (Metal) shutdown!");

        //jegui_shutdown_metal(reboot);

        delete metal_context;
    }

    jegl_update_action pre_update(jegl_context::graphic_impl_context_t)
    {
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t, jegl_update_action)
    {
        // jegui_update_metal();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob create_resource_blob(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
        return nullptr;
    }
    void close_resource_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }

    void create_resource(jegl_context::graphic_impl_context_t, jegl_resource_blob, jegl_resource*)
    {
    }
    void using_resource(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
    }
    void close_resource(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
    }

    void bind_uniform_buffer(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
    }
    bool bind_shader(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
        return true;
    }
    void bind_texture(jegl_context::graphic_impl_context_t, jegl_resource*, size_t)
    {
    }
    void draw_vertex_with_shader(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
    }

    void bind_framebuffer(jegl_context::graphic_impl_context_t, jegl_resource*, size_t, size_t, size_t, size_t)
    {
    }
    void clear_framebuffer_color(jegl_context::graphic_impl_context_t, float[4])
    {
    }
    void clear_framebuffer_depth(jegl_context::graphic_impl_context_t)
    {
    }

    void set_uniform(jegl_context::graphic_impl_context_t, uint32_t, jegl_shader::uniform_type, const void*)
    {
    }
}

void jegl_using_metal_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::metal;

    write_to_apis->interface_startup = startup;
    write_to_apis->interface_shutdown_before_resource_release = pre_shutdown;
    write_to_apis->interface_shutdown = shutdown;

    write_to_apis->update_frame_ready = pre_update;
    write_to_apis->update_draw_commit = commit_update;

    write_to_apis->create_resource_blob_cache = create_resource_blob;
    write_to_apis->close_resource_blob_cache = close_resource_blob;

    write_to_apis->create_resource = create_resource;
    write_to_apis->using_resource = using_resource;
    write_to_apis->close_resource = close_resource;

    write_to_apis->bind_uniform_buffer = bind_uniform_buffer;
    write_to_apis->bind_texture = bind_texture;
    write_to_apis->bind_shader = bind_shader;
    write_to_apis->draw_vertex = draw_vertex_with_shader;

    write_to_apis->bind_framebuf = bind_framebuffer;
    write_to_apis->clear_frame_color = clear_framebuffer_color;
    write_to_apis->clear_frame_depth = clear_framebuffer_depth;

    write_to_apis->set_uniform = set_uniform;
}

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

            jeecs::graphic::api::metal::application_delegate del(m_graphic_host);

            NS::Application* shared_application = NS::Application::sharedApplication();
            shared_application->setDelegate(&del);
            shared_application->run();

            auto_release_pool->release();
        }
    }
};

void jegl_cocoa_metal_application_run(int argc, char** argv)
{
    je_macos_context context(argc, argv);
    context.macos_loop();
}

#else
void jegl_using_metal_apis(jegl_graphic_api *write_to_apis)
{
    jeecs::debug::logfatal("METAL not available.");
}
#endif