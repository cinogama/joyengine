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

        MTL::Device* m_metal_device;
        MTL::CommandQueue* m_command_queue;
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

    struct metal_resource_shader_blob
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_resource_shader_blob);

        MTL::Function* m_vertex_function;
        MTL::Function* m_fragment_function;

        metal_resource_shader_blob(
            MTL::Function* vert,
            MTL::Function* frag)
            : m_vertex_function(vert)
            , m_fragment_function(frag)
        {
        }
        ~metal_resource_shader_blob()
        {
            m_vertex_function->release();
            m_fragment_function->release();
        }
    };
    struct metal_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_shader);

        // TODO: Metal 的 pipeline state 和 vulkan 的 pipeline 类似，需要根据目标
        //  缓冲区的格式等信息创建不同的 pipeline state。
        //      现在仍然是在早期开发阶段，先以实现基本功能为主。
        MTL::RenderPipelineState* m_pipeline_state;

        metal_shader(MTL::RenderPipelineState* pso)
            : m_pipeline_state(pso)
        {
        }
        ~metal_shader()
        {
            m_pipeline_state->release();
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
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        metal_context->m_frame_auto_release_pool_init_pre_update_and_release_after_commit =
            NS::AutoreleasePool::alloc()->init();

        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t ctx, jegl_update_action)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

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
)").value();

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

    jegl_resource_blob create_resource_blob(jegl_context::graphic_impl_context_t ctx, jegl_resource* res)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        switch (res->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            auto* raw_shader = res->m_raw_shader_data;
            assert(raw_shader != nullptr);

            NS::Error* error_info = nullptr;
            MTL::Library* vertex_library = nullptr;
            MTL::Library* fragment_library = nullptr;

            bool shader_load_failed = false;
            std::string error_informations;

            vertex_library = metal_context->m_metal_device->newLibrary(
                NS::String::string(raw_shader->m_vertex_msl_mac_src, NS::StringEncoding::UTF8StringEncoding),
                nullptr,
                &error_info);

            if (vertex_library == nullptr)
            {
                shader_load_failed = true;
                error_informations += "In vertex shader: \n";

                error_informations +=
                    error_info->localizedDescription()->utf8String();
            }

            fragment_library = metal_context->m_metal_device->newLibrary(
                NS::String::string(raw_shader->m_fragment_msl_mac_src, NS::StringEncoding::UTF8StringEncoding),
                nullptr,
                &error_info);

            if (fragment_library == nullptr)
            {
                shader_load_failed = true;
                error_informations += "In fragment shader: \n";
                error_informations +=
                    error_info->localizedDescription()->utf8String();
            }

            if (shader_load_failed)
            {
                jeecs::debug::logerr(
                    "Fail to load shader '%s':\n%s", res->m_path, error_informations.c_str());
                if (vertex_library != nullptr)
                    vertex_library->release();
                if (fragment_library != nullptr)
                    fragment_library->release();
                return nullptr;
            }
            else
            {
                MTL::Function* vertex_main_function =
                    vertex_library->newFunction(
                        NS::String::string("vertex_main", NS::StringEncoding::UTF8StringEncoding));
                MTL::Function* fragment_main_function =
                    fragment_library->newFunction(
                        NS::String::string("fragment_main", NS::StringEncoding::UTF8StringEncoding));

                assert(
                    vertex_main_function != nullptr
                    && fragment_main_function != nullptr);

                return new metal_resource_shader_blob(
                    vertex_main_function,
                    fragment_main_function);
            }
            break;
        }
        default:
            break;
        }
        return nullptr;
    }
    void close_resource_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob blob)
    {
        if (blob != nullptr)
        {
            delete reinterpret_cast<metal_resource_shader_blob*>(blob);
        }
    }

    void create_resource(
        jegl_context::graphic_impl_context_t ctx,
        jegl_resource_blob blob,
        jegl_resource* res)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        switch (res->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            if (blob != nullptr)
            {
                auto* shader_blob = reinterpret_cast<metal_resource_shader_blob*>(blob);
                MTL::RenderPipelineDescriptor* pDesc =
                    MTL::RenderPipelineDescriptor::alloc()->init();

                pDesc->setVertexFunction(shader_blob->m_vertex_function);
                pDesc->setFragmentFunction(shader_blob->m_fragment_function);

                pDesc->colorAttachments()->object(0)->setPixelFormat(
                    MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

                NS::Error* pError = nullptr;
                auto* pso = _pDevice->newRenderPipelineState(pDesc, &pError);
                if (pso == nullptr)
                {
                    jeecs::debug::logfatal(
                        "Fail to create pipeline state object for shader '%s':\n%s",
                        res->m_path,
                        pError->localizedDescription()->utf8String());
                    abort();
                }

                res->m_handle.m_ptr = new metal_shader(pso);
            }
            else
                res->m_handle.m_ptr = nullptr;

            break;
        }
        default:
            jeecs::debug::logfatal("Unsupported resource type to create in Metal backend.");
            break;
        }
    }
    void using_resource(jegl_context::graphic_impl_context_t, jegl_resource*)
    {
    }
    void close_resource(jegl_context::graphic_impl_context_t, jegl_resource* res)
    {
        switch (res->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            delete reinterpret_cast<metal_shader*>(res->m_handle.m_ptr);
            break;
        }
        default:
            jeecs::debug::logfatal("Unsupported resource type to close in Metal backend.");
            break;
        }
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