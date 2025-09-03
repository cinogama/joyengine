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

        MTL::Device*        m_metal_device;
        MTL::CommandQueue*  m_command_queue;
        jeecs::graphic::metal::window_view_layout*
                            m_window_and_view_layout;

        NS::AutoreleasePool* 
                            m_frame_auto_release_pool_init_pre_update_and_release_after_commit;

        jegl_metal_context(const jegl_interface_config* cfg)
        {
            m_metal_device = 
                MTL::CreateSystemDefaultDevice();
            m_command_queue = m_metal_device->newCommandQueue();

            m_window_and_view_layout =
                new jeecs::graphic::metal::window_view_layout(
                    cfg->m_title,
                    (double)cfg->m_width,
                    (double)cfg->m_height,
                    m_metal_device);
        }
        ~jegl_metal_context()
        {
            // Must release window before device.
            delete m_window_and_view_layout;
            m_command_queue->release();
            m_metal_device->release();
        }
    };

    jegl_context::graphic_impl_context_t
        startup(jegl_context* glthread, const jegl_interface_config* cfg, bool reboot)
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

        jegl_metal_context* context = new jegl_metal_context(cfg);

        // Pass the window and view to `applicationDidFinishLaunching`
        const_cast<jegl_interface_config*>(cfg)->m_userdata =
            context->m_window_and_view_layout;

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

    jegl_update_action pre_update(jegl_context::graphic_impl_context_t ctx)
    {
        jegl_metal_context* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        metal_context->m_frame_auto_release_pool_init_pre_update_and_release_after_commit =
            NS::AutoreleasePool::alloc()->init();

        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t ctx, jegl_update_action)
    {
        jegl_metal_context* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        // jegui_update_metal();

        static basic::resource<graphic::shader> sd =
            graphic::shader::create("!/test.shader", R"(
// Mono.shader
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

SHARED  (true);
ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex  : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
    };
}

public func frag(_: v2f)
{
    return fout{
        color = vec4!(1., 1., 1., 1.),
    };
}
)");

        /*
        初期开发，暂时在这里随便写写画画
        */
        MTL::CommandBuffer* pCmd = 
            metal_context->m_command_queue->commandBuffer();
        MTL::RenderPassDescriptor* pRpd = 
            metal_context->m_window_and_view_layout->m_metal_view->currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);

        pEnc->endEncoding();
        pCmd->presentDrawable(
            metal_context->m_window_and_view_layout->m_metal_view->currentDrawable());
        pCmd->commit();

        metal_context->m_frame_auto_release_pool_init_pre_update_and_release_after_commit->release();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob create_resource_blob(jegl_context::graphic_impl_context_t, jegl_resource* res)
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

            jeecs::graphic::metal::application_delegate del(m_graphic_host);

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
void jegl_using_metal_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("METAL not available.");
}
#endif