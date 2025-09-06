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
    struct metal_shader;
    struct jegl_metal_context
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_metal_context);

        MTL::Device* m_metal_device;
        MTL::CommandQueue* m_command_queue;
        jeecs::graphic::metal::window_view_layout*
            m_window_and_view_layout;

        NS::AutoreleasePool* m_frame_auto_release;

        /* Render context */
        struct render_runtime_states
        {
            metal_shader* m_current_target_shader;
        };
        render_runtime_states m_render_states;

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

            m_frame_auto_release = nullptr;
        }
        ~jegl_metal_context()
        {
            // Must release window before device.
            delete m_window_and_view_layout;
            m_command_queue->release();
            m_metal_device->release();

            if (m_frame_auto_release != nullptr)
                m_frame_auto_release->release();
        }
    };

    struct metal_resource_shader_blob
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_resource_shader_blob);

        struct shared_state
        {
            JECS_DISABLE_MOVE_AND_COPY(shared_state);

            jegl_metal_context* m_context;

            // TODO: Metal 的 pipeline state 和 vulkan 的 pipeline 类似，需要根据目标
            //  缓冲区的格式等信息创建不同的 pipeline state。
            //      现在仍然是在早期开发阶段，先以实现基本功能为主。
            MTL::RenderPipelineState* m_pipeline_state;

            shared_state(jegl_metal_context* ctx)
                : m_context(ctx)
            {
            }
            ~shared_state()
            {
                m_pipeline_state->release();
            }
        };

        MTL::Function* m_vertex_function;
        MTL::Function* m_fragment_function;
        MTL::VertexDescriptor* m_vertex_descriptor;

        basic::resource<shared_state> m_shared_state;

        std::unordered_map<std::string, uint32_t>
            m_uniform_locations;
        size_t m_uniform_size;

        metal_resource_shader_blob(
            jegl_metal_context* ctx,
            MTL::Function* vert,
            MTL::Function* frag,
            MTL::VertexDescriptor* vdesc)
            : m_vertex_function(vert)
            , m_fragment_function(frag)
            , m_vertex_descriptor(vdesc)
            , m_shared_state(new shared_state(ctx))
        {
        }
        ~metal_resource_shader_blob()
        {
            m_vertex_function->release();
            m_fragment_function->release();
            m_vertex_descriptor->release();
        }
    };
    struct metal_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_shader);

        basic::resource<metal_resource_shader_blob::shared_state> m_shared_state;

        size_t m_uniform_cpu_buffer_size;
        bool m_uniform_updated;
        void* m_uniform_cpu_buffer;

        MTL::Buffer* m_uniforms;

        metal_shader(
            metal_resource_shader_blob* blob,
            MTL::RenderPipelineState* pso /* tmp */)
            : m_shared_state(blob->m_shared_state)
            , m_uniform_cpu_buffer_size(blob->m_uniform_size)
            , m_uniform_updated(false)
        {
            m_shared_state->m_pipeline_state = pso;
            if (m_uniform_cpu_buffer_size != 0)
            {
                m_uniform_cpu_buffer = malloc(m_uniform_cpu_buffer_size);
                assert(m_uniform_cpu_buffer != nullptr);

                memset(m_uniform_cpu_buffer, 0, m_uniform_cpu_buffer_size);

                m_uniforms = m_shared_state->m_context->m_metal_device->newBuffer(
                    m_uniform_cpu_buffer_size,
                    MTL::ResourceStorageModeShared);
            }
            else
                m_uniform_cpu_buffer = nullptr;
        }
        ~metal_shader()
        {
            if (m_uniform_cpu_buffer != nullptr)
            {
                free(m_uniform_cpu_buffer);
                m_uniforms->release();
            }
        }
    };
    struct metal_vertex
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_vertex);

        MTL::PrimitiveType m_primitive_type;
        MTL::Buffer* m_vertex_buffer;
        MTL::Buffer* m_index_buffer;
        uint32_t m_index_count;
        uint32_t m_vertex_stride; // 添加实际顶点步长信息

        metal_vertex(
            MTL::PrimitiveType primitive_type,
            MTL::Buffer* vertex_buffer,
            MTL::Buffer* index_buffer,
            uint32_t index_count,
            uint32_t vertex_stride)
            : m_primitive_type(primitive_type)
            , m_vertex_buffer(vertex_buffer)
            , m_index_buffer(index_buffer)
            , m_index_count(index_count)
            , m_vertex_stride(vertex_stride)
        {
        }
        ~metal_vertex()
        {
            if (m_vertex_buffer)
                m_vertex_buffer->release();
            if (m_index_buffer)
                m_index_buffer->release();
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

        // 每帧开始之前，创建新的自动释放池（如果还没有的话）
        if (metal_context->m_frame_auto_release == nullptr)
        {
            metal_context->m_frame_auto_release =
                NS::AutoreleasePool::alloc()->init();
        }

        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t ctx, jegl_update_action)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        // jegui_update_metal();
        const float pdata[] = {
            -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
        };
        static basic::resource<graphic::vertex> vt =
            graphic::vertex::create(
                jegl_vertex::type::TRIANGLESTRIP,
                pdata,
                sizeof(pdata),
                { 0, 1, 2 },
                {
                    {jegl_vertex::data_type::FLOAT32, 3},
                    {jegl_vertex::data_type::FLOAT32, 3},
                }).value();

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
        color   : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos     : float4,
        color   : float3,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color   : float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = vec4!(v.vertex, 1.),
        color = v.color,
    };
}

public func frag(v: v2f)
{
    return fout{
        color = vec4!(v.color, 1.),
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

                jegl_using_resource(sd->resource());
                jegl_using_resource(vt->resource());

                auto* shader_instance = std::launder(reinterpret_cast<metal_shader*>(sd->resource()->m_handle.m_ptr));
                auto* vertex_instance = std::launder(reinterpret_cast<metal_vertex*>(vt->resource()->m_handle.m_ptr));

                pEnc->setRenderPipelineState(
                    shader_instance->m_shared_state->m_pipeline_state);
                pEnc->setVertexBuffer(
                    vertex_instance->m_vertex_buffer,
                    0, 
                    vertex_instance->m_vertex_stride,
                    0);
                pEnc->drawIndexedPrimitives(
                    vertex_instance->m_primitive_type,
                    vertex_instance->m_index_count,
                    MTL::IndexType::IndexTypeUInt32,
                    vertex_instance->m_index_buffer,
                    0);

                pEnc->endEncoding();
                pCmd->presentDrawable(
                    metal_context->m_window_and_view_layout->m_metal_view->currentDrawable());
                pCmd->commit();

                // 帧结束后释放自动释放池
                if (metal_context->m_frame_auto_release != nullptr)
                {
                    metal_context->m_frame_auto_release->release();
                    metal_context->m_frame_auto_release = nullptr;
                }

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

                vertex_library->release();
                fragment_library->release();

                MTL::VertexDescriptor* vertex_descriptor =
                    MTL::VertexDescriptor::alloc()->init();

                // 计算实际的顶点数据布局
                unsigned int current_offset = 0;

                for (size_t i = 0; i < raw_shader->m_vertex_in_count; ++i)
                {
                    auto* attribute = vertex_descriptor->attributes()->object(i);
                    attribute->setBufferIndex(0);
                    attribute->setOffset(current_offset);

                    unsigned int attribute_size = 0;

                    switch (raw_shader->m_vertex_in[i])
                    {
                    case jegl_shader::uniform_type::INT:
                        attribute->setFormat(MTL::VertexFormatInt);
                        attribute_size = sizeof(int);
                        break;
                    case jegl_shader::uniform_type::INT2:
                        attribute->setFormat(MTL::VertexFormatInt2);
                        attribute_size = 2 * sizeof(int);
                        break;
                    case jegl_shader::uniform_type::INT3:
                        attribute->setFormat(MTL::VertexFormatInt3);
                        attribute_size = 3 * sizeof(int);
                        break;
                    case jegl_shader::uniform_type::INT4:
                        attribute->setFormat(MTL::VertexFormatInt4);
                        attribute_size = 4 * sizeof(int);
                        break;
                    case jegl_shader::uniform_type::FLOAT:
                        attribute->setFormat(MTL::VertexFormatFloat);
                        attribute_size = sizeof(float);
                        break;
                    case jegl_shader::uniform_type::FLOAT2:
                        attribute->setFormat(MTL::VertexFormatFloat2);
                        attribute_size = 2 * sizeof(float);
                        break;
                    case jegl_shader::uniform_type::FLOAT3:
                        attribute->setFormat(MTL::VertexFormatFloat3);
                        attribute_size = 3 * sizeof(float);
                        break;
                    case jegl_shader::uniform_type::FLOAT4:
                        attribute->setFormat(MTL::VertexFormatFloat4);
                        attribute_size = 4 * sizeof(float);
                        break;
                    default:
                        abort();
                    }

                    current_offset += attribute_size;
                }

                // 设置buffer layout，使用动态stride
                if (raw_shader->m_vertex_in_count > 0)
                {
                    auto* layout = vertex_descriptor->layouts()->object(0);
                    layout->setStride(MTL::BufferLayoutStrideDynamic); // 使用动态stride
                    layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
                }

                metal_resource_shader_blob* shader_blob =
                    new metal_resource_shader_blob(
                        metal_context,
                        vertex_main_function,
                        fragment_main_function,
                        vertex_descriptor);

                uint32_t last_elem_end_place = 0;
                size_t max_allign = 4;
                auto* uniforms = raw_shader->m_custom_uniforms;
                while (uniforms != nullptr)
                {
                    size_t unit_size = 0;
                    size_t allign_base = 0;
                    switch (uniforms->m_uniform_type)
                    {
                    case jegl_shader::uniform_type::INT:
                    case jegl_shader::uniform_type::FLOAT:
                        unit_size = 4;
                        allign_base = 4;
                        break;
                    case jegl_shader::uniform_type::INT2:
                    case jegl_shader::uniform_type::FLOAT2:
                        unit_size = 8;
                        allign_base = 8;
                        break;
                    case jegl_shader::uniform_type::INT3:
                    case jegl_shader::uniform_type::FLOAT3:
                        unit_size = 12;
                        allign_base = 16;
                        break;
                    case jegl_shader::uniform_type::INT4:
                    case jegl_shader::uniform_type::FLOAT4:
                        unit_size = 16;
                        allign_base = 16;
                        break;
                    case jegl_shader::uniform_type::FLOAT4X4:
                        unit_size = 64;
                        allign_base = 16;
                        break;
                    case jegl_shader::uniform_type::TEXTURE:
                        break;
                    }

                    if (unit_size != 0)
                    {
                        max_allign = std::max(max_allign, allign_base);

                        last_elem_end_place =
                            jeecs::basic::allign_size(last_elem_end_place, allign_base);
                        shader_blob->m_uniform_locations[uniforms->m_name] = last_elem_end_place;
                        last_elem_end_place += unit_size;
                    }
                    uniforms = uniforms->m_next;
                }
                shader_blob->m_uniform_size =
                    jeecs::basic::allign_size(last_elem_end_place, max_allign);

                return shader_blob;
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
                pDesc->setVertexDescriptor(shader_blob->m_vertex_descriptor);

                // Create a default vertex descriptor for the shader
                /*
                MTL::VertexDescriptor* vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
                auto* attribute = vertex_descriptor->attributes()->object(0);
                attribute->setBufferIndex(0);
                attribute->setOffset(0);
                attribute->setFormat(MTL::VertexFormatFloat3);

                auto* layout = vertex_descriptor->layouts()->object(0);
                layout->setStride(3 * sizeof(float));
                layout->setStepFunction(MTL::VertexStepFunctionPerVertex);

                pDesc->setVertexDescriptor(vertex_descriptor);
                */

                pDesc->colorAttachments()->object(0)->setPixelFormat(
                    MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

                NS::Error* pError = nullptr;
                auto* pso = metal_context->m_metal_device->newRenderPipelineState(pDesc, &pError);
                if (pso == nullptr)
                {
                    jeecs::debug::logfatal(
                        "Fail to create pipeline state object for shader '%s':\n%s",
                        res->m_path,
                        pError->localizedDescription()->utf8String());
                    abort();
                }

                pDesc->release();

                metal_shader* metal_shader_instance = new metal_shader(pso);
                metal_shader_instance->m_uniform_cpu_buffer_size =
                    shader_blob->m_uniform_size;
                if (metal_shader_instance->m_uniform_cpu_buffer_size != 0)
                {
                    metal_shader_instance->m_uniforms =
                        metal_context->m_metal_device->newBuffer(
                            metal_shader_instance->m_uniform_cpu_buffer_size,
                            MTL::ResourceStorageModeShared);
                    metal_shader_instance->m_uniform_updated = false;
                    metal_shader_instance->m_uniform_cpu_buffer =
                        malloc(metal_shader_instance->m_uniform_cpu_buffer_size);
                }
                else
                {
                    metal_shader_instance->m_uniforms = nullptr;
                    metal_shader_instance->m_uniform_updated = false;
                    metal_shader_instance->m_uniform_cpu_buffer = nullptr;
                }

                res->m_handle.m_ptr = metal_shader_instance;
            }
            else
                res->m_handle.m_ptr = nullptr;

            break;
        }
        case jegl_resource::type::VERTEX:
        {
            auto* raw_vertex_data = res->m_raw_vertex_data;
            assert(raw_vertex_data != nullptr);

            // Create vertex buffer
            MTL::Buffer* vertex_buffer = metal_context->m_metal_device->newBuffer(
                raw_vertex_data->m_vertexs,
                raw_vertex_data->m_vertex_length,
                MTL::ResourceStorageModeShared);

            // Create index buffer
            MTL::Buffer* index_buffer = metal_context->m_metal_device->newBuffer(
                raw_vertex_data->m_indexs,
                raw_vertex_data->m_index_count * sizeof(uint32_t),
                MTL::ResourceStorageModeShared);

            // Map primitive types
            MTL::PrimitiveType primitive_type;
            switch (raw_vertex_data->m_type)
            {
            case jegl_vertex::type::LINESTRIP:
                primitive_type = MTL::PrimitiveTypeLineStrip;
                break;
            case jegl_vertex::type::TRIANGLES:
                primitive_type = MTL::PrimitiveTypeTriangle;
                break;
            case jegl_vertex::type::TRIANGLESTRIP:
                primitive_type = MTL::PrimitiveTypeTriangleStrip;
                break;
            default:
                jeecs::debug::logfatal("Unsupported vertex primitive type.");
                break;
            }

            res->m_handle.m_ptr = new metal_vertex(
                primitive_type,
                vertex_buffer,
                index_buffer,
                raw_vertex_data->m_index_count,
                raw_vertex_data->m_data_size_per_point); // 传入实际顶点大小
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
        case jegl_resource::type::VERTEX:
        {
            delete reinterpret_cast<metal_vertex*>(res->m_handle.m_ptr);
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
    bool bind_shader(jegl_context::graphic_impl_context_t ctx, jegl_resource* res)
    {
        //assert(res->m_type == jegl_resource::type::SHADER);
        //
        //auto* shader_instance = reinterpret_cast<metal_shader*>(res->m_handle.m_ptr);
        //if (shader_instance == nullptr)
        //    return false;

        //auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);
        //metal_context->m_render_states.m_current_target_shader = shader_instance;



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