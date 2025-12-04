#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_METAL_GAPI

#include "jeecs_imgui_backend_api.hpp"

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

// #include "jeecs_graphic_api_interface_cocoa.hpp"
#include "jeecs_graphic_api_interface_glfw.hpp"

namespace jeecs::graphic::api::metal
{
    struct metal_shader;
    struct metal_framebuffer;

    struct jegl_metal_context
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_metal_context);

        graphic::glfw* m_interface;

        MTL::Device* m_metal_device;
        CA::MetalLayer* m_metal_layer;
        MTL::CommandQueue* m_command_queue;
        MTL::Texture* m_main_depth_texture_target;
        NS::AutoreleasePool* m_frame_auto_release;

        /* Render context */
        struct render_runtime_states
        {
            size_t m_frame_counter;

            int m_screen_width;
            int m_screen_height;

            CA::MetalDrawable* m_main_this_frame_drawable;
            MTL::RenderPassDescriptor* m_main_render_pass_descriptor;

            metal_shader* m_current_target_shader;
            metal_framebuffer* m_current_target_framebuffer_may_null;

            MTL::CommandBuffer* m_currnet_command_buffer;
            MTL::RenderCommandEncoder* m_current_command_encoder;
        };
        render_runtime_states m_render_states;

        void create_main_depth_texture(int w, int h)
        {
            MTL::TextureDescriptor* depthTexDesc = MTL::TextureDescriptor::alloc()->init();
            depthTexDesc->setWidth(w);
            depthTexDesc->setHeight(h);
            depthTexDesc->setDepth(1);
            depthTexDesc->setPixelFormat(MTL::PixelFormatDepth16Unorm);
            depthTexDesc->setTextureType(MTL::TextureType2D);
            depthTexDesc->setStorageMode(MTL::StorageModePrivate);
            depthTexDesc->setUsage(MTL::TextureUsageRenderTarget);
            m_main_depth_texture_target = m_metal_device->newTexture(depthTexDesc);
            depthTexDesc->release();
        }
        void recreate_main_depth_texture(int w, int h)
        {
            m_main_depth_texture_target->release();
            create_main_depth_texture(w, h);
        }

        jegl_metal_context(const jegl_interface_config* cfg, bool reboot)
        {
            m_metal_device =
                MTL::CreateSystemDefaultDevice();
            m_command_queue = m_metal_device->newCommandQueue();

            m_interface = new glfw(reboot ? glfw::HOLD : glfw::METAL);

            m_metal_layer = CA::MetalLayer::layer();
            m_metal_layer->setDevice(m_metal_device);

            m_render_states.m_screen_width = (int)cfg->m_width;
            m_render_states.m_screen_height = (int)cfg->m_height;

            create_main_depth_texture(cfg->m_width, cfg->m_height);

            m_frame_auto_release = nullptr;
            m_render_states.m_frame_counter = 1;
        }
        ~jegl_metal_context()
        {
            // Must release window before device.
            delete m_interface;

            m_command_queue->release();
            m_metal_device->release();
            m_main_depth_texture_target->release();
        }
    };

    struct metal_resource_shader_blob
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_resource_shader_blob);

        struct shared_state
        {
            JECS_DISABLE_MOVE_AND_COPY(shared_state);

            jegl_metal_context* m_context;
            MTL::Function* m_vertex_function;
            MTL::Function* m_fragment_function;

            MTL::VertexDescriptor* m_vertex_descriptor;
            MTL::DepthStencilState* m_depth_stencil_state;

            bool m_blend_enabled;
            MTL::BlendFactor m_blend_src_factor;
            MTL::BlendFactor m_blend_dst_factor;
            MTL::BlendOperation m_blend_operation;

            MTL::CullMode m_cull_mode;

            struct sampler_structs
            {
                MTL::SamplerState* m_sampler;
                uint32_t m_sampler_id;
            };
            std::vector<sampler_structs> m_samplers;
            std::unordered_map<metal_framebuffer*, MTL::RenderPipelineState*>
                m_pipeline_states;

            shared_state(
                jegl_metal_context* ctx,
                MTL::Function* vert,
                MTL::Function* frag,
                MTL::VertexDescriptor* vdesc,
                MTL::DepthStencilState* dstate,
                bool blend_enabled,
                MTL::BlendFactor src_bdesc,
                MTL::BlendFactor dst_bdesc,
                MTL::BlendOperation op_bdesc,
                MTL::CullMode cmode)
                : m_context(ctx)
                , m_vertex_function(vert)
                , m_fragment_function(frag)
                , m_vertex_descriptor(vdesc)
                , m_depth_stencil_state(dstate)
                , m_blend_enabled(blend_enabled)
                , m_blend_src_factor(src_bdesc)
                , m_blend_dst_factor(dst_bdesc)
                , m_blend_operation(op_bdesc)
                , m_cull_mode(cmode)
            {
            }
            ~shared_state();
        };

        basic::resource<shared_state> m_shared_state;

        std::unordered_map<std::string, uint32_t>
            m_uniform_locations;
        size_t m_uniform_size;

        metal_resource_shader_blob(
            jegl_metal_context* ctx,
            MTL::Function* vert,
            MTL::Function* frag,
            MTL::VertexDescriptor* vdesc,
            MTL::DepthStencilState* dstate,
            bool blend_enabled,
            MTL::BlendFactor src_bdesc,
            MTL::BlendFactor dst_bdesc,
            MTL::BlendOperation op_bdesc,
            MTL::CullMode cmode)
            : m_shared_state(
                new shared_state(
                    ctx,
                    vert,
                    frag,
                    vdesc,
                    dstate,
                    blend_enabled,
                    src_bdesc,
                    dst_bdesc,
                    op_bdesc,
                    cmode))
        {
        }
        ~metal_resource_shader_blob() = default;

        uint32_t get_built_in_location(const std::string& name) const
        {
            auto fnd = m_uniform_locations.find(name);
            if (fnd != m_uniform_locations.end())
                return fnd->second;

            return jeecs::graphic::INVALID_UNIFORM_LOCATION;
        }
    };
    struct metal_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_shader);

        basic::resource<metal_resource_shader_blob::shared_state> m_shared_state;

        size_t m_uniform_cpu_buffer_size;
        bool m_uniform_buffer_updated;
        void* m_uniform_cpu_buffer;

        bool m_draw_for_r2b;

        size_t m_command_commit_round;
        size_t m_next_allocate_uniform_buffer_index;
        std::vector<MTL::Buffer*> m_allocated_uniform_buffers;

        uint32_t m_ndc_scale_uniform_id;

        MTL::Buffer* allocate_buffer_to_update(jegl_metal_context* ctx)
        {
            if (m_next_allocate_uniform_buffer_index >= m_allocated_uniform_buffers.size())
            {
                if (m_command_commit_round != ctx->m_render_states.m_frame_counter)
                {
                    m_command_commit_round = ctx->m_render_states.m_frame_counter;
                    m_next_allocate_uniform_buffer_index = 0;
                }

                auto* buf = ctx->m_metal_device->newBuffer(
                    m_uniform_cpu_buffer_size,
                    MTL::ResourceStorageModeShared);

                m_allocated_uniform_buffers.push_back(buf);
                ++m_next_allocate_uniform_buffer_index;

                return buf;
            }
            return m_allocated_uniform_buffers[m_next_allocate_uniform_buffer_index++];
        }
        MTL::Buffer* get_last_usable_buffer(jegl_metal_context* ctx)
        {
            if (m_next_allocate_uniform_buffer_index == 0)
                return allocate_buffer_to_update(ctx);
            else if (m_command_commit_round != ctx->m_render_states.m_frame_counter)
            {
                if (m_next_allocate_uniform_buffer_index != 0)
                    // Make sure use the newest UBO first.
                    std::swap(
                        m_allocated_uniform_buffers[0],
                        m_allocated_uniform_buffers[m_next_allocate_uniform_buffer_index - 1]);

                m_command_commit_round = ctx->m_render_states.m_frame_counter;
                m_next_allocate_uniform_buffer_index = 1;
            }
            return m_allocated_uniform_buffers[m_next_allocate_uniform_buffer_index - 1];
        }

        metal_shader(
            metal_resource_shader_blob* blob)
            : m_shared_state(blob->m_shared_state)
            , m_uniform_cpu_buffer_size(blob->m_uniform_size)
            , m_uniform_buffer_updated(false)
            , m_draw_for_r2b(false)
            , m_command_commit_round(0)
            , m_next_allocate_uniform_buffer_index(0)
        {
            if (m_uniform_cpu_buffer_size != 0)
            {
                m_uniform_cpu_buffer = malloc(m_uniform_cpu_buffer_size);
                assert(m_uniform_cpu_buffer != nullptr);

                memset(m_uniform_cpu_buffer, 0, m_uniform_cpu_buffer_size);
            }
            else
                m_uniform_cpu_buffer = nullptr;
        }
        ~metal_shader()
        {
            if (m_uniform_cpu_buffer != nullptr)
            {
                free(m_uniform_cpu_buffer);

                for (auto buf : m_allocated_uniform_buffers)
                    buf->release();
                m_allocated_uniform_buffers.clear();
            }
            assert(m_allocated_uniform_buffers.size() == 0);
        }
    };
    struct metal_uniform_buffer
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_uniform_buffer);

        MTL::Buffer* m_uniform_buffer;
        uint32_t m_binding_place;

        metal_uniform_buffer(jegl_metal_context* ctx, jegl_uniform_buffer* res)
        {
            m_uniform_buffer =
                ctx->m_metal_device->newBuffer(
                    res->m_buffer,
                    res->m_buffer_size,
                    MTL::ResourceStorageModeShared);
            m_binding_place =
                (uint32_t)(res->m_buffer_binding_place + 2);
        }
        ~metal_uniform_buffer()
        {
            m_uniform_buffer->release();
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
    struct metal_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_texture);

        MTL::Texture* m_texture;
        MTL::PixelFormat m_pixel_format;

        metal_texture(MTL::Texture* tex, MTL::PixelFormat fmt)
            : m_texture(tex)
            , m_pixel_format(fmt)
        {
        }
        ~metal_texture()
        {
            m_texture->release();
        }
    };
    struct metal_framebuffer
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_framebuffer);

        MTL::RenderPassDescriptor* m_render_pass_descriptor;

        size_t m_frame_width;
        size_t m_frame_height;

        bool m_has_depth_attachment;
        std::vector<MTL::PixelFormat> m_color_attachment_formats;

        std::unordered_set<metal_resource_shader_blob::shared_state*>
            m_linked_shaders;

        metal_framebuffer(size_t w, size_t h)
            : m_frame_width(w)
            , m_frame_height(h)
            , m_has_depth_attachment(false)
        {
            m_render_pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
        }
        ~metal_framebuffer()
        {
            m_render_pass_descriptor->release();
            for (auto* linked_state : m_linked_shaders)
            {
                auto fnd = linked_state->m_pipeline_states.find(this);
                if (fnd != linked_state->m_pipeline_states.end())
                {
                    fnd->second->release();
                    linked_state->m_pipeline_states.erase(fnd);
                }
            }
        }
    };

    metal_resource_shader_blob::shared_state::~shared_state()
    {
        for (auto& [fb, state] : m_pipeline_states)
        {
            if (fb != nullptr)
                fb->m_linked_shaders.erase(this);

            state->release();
        }

        for (auto& sampler : m_samplers)
            sampler.m_sampler->release();

        m_vertex_function->release();
        m_fragment_function->release();
        m_vertex_descriptor->release();
        m_depth_stencil_state->release();
    }

    jegl_context::graphic_impl_context_t
        startup(jegl_context* glthread, const jegl_interface_config* cfg, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Metal) start!");

        jegl_metal_context* context = new jegl_metal_context(cfg, reboot);

        // If glfw enabled.
        context->m_interface->create_interface(cfg);

        GLFWwindow* glfw_window =
            reinterpret_cast<GLFWwindow*>(context->m_interface->interface_handle());
        NS::Window* metal_window =
            reinterpret_cast<NS::Window*>(glfwGetCocoaWindow(glfw_window));

        NS::View* metal_content_view = metal_window->contentView();

        metal_content_view->setLayer(context->m_metal_layer);
        metal_content_view->setWantsLayer(true);
        metal_content_view->setOpaque(true);

        jegui_init_metal(
            glthread,
            [](jegl_context*, jegl_texture* res)
            {
                metal_texture* tex =
                    reinterpret_cast<metal_texture*>(res->m_handle.m_ptr);

                return (uint64_t)tex->m_texture;
            },
            [](jegl_context* /*ctx*/, jegl_shader* /*res*/)
            {
                //auto* metal_context =
                //    reinterpret_cast<jegl_metal_context*>(ctx->m_graphic_impl_context);
                //auto* shader_instance = reinterpret_cast<metal_shader*>(res->m_handle.m_ptr);
                //auto& shader_shared_state = *shader_instance->m_shared_state;
                //for (const auto& sampler_struct : shader_shared_state.m_samplers)
                //{
                //    metal_context->m_render_states.m_current_command_encoder->setFragmentSamplerState(
                //        sampler_struct.m_sampler,
                //        sampler_struct.m_sampler_id);
                //}
            },
            // If glfw enabled.
            glfw_window,
            context->m_metal_device);

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

        jegui_shutdown_metal(reboot);

        // If glfw enabled.
        metal_context->m_interface->shutdown(reboot);

        delete metal_context;
    }

    jegl_update_action pre_update(jegl_context::graphic_impl_context_t ctx)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        // 每帧开始之前，创建新的自动释放池
        metal_context->m_frame_auto_release = NS::AutoreleasePool::alloc()->init();

        // Update frame counter.
        ++metal_context->m_render_states.m_frame_counter;

        // Reset render target states.
        metal_context->m_render_states.m_current_target_framebuffer_may_null = nullptr;
        metal_context->m_render_states.m_current_target_shader = nullptr;

        metal_context->m_render_states.m_main_this_frame_drawable =
            metal_context->m_metal_layer->nextDrawable();

        metal_context->m_render_states.m_main_render_pass_descriptor =
            MTL::RenderPassDescriptor::renderPassDescriptor();

        metal_context->m_render_states.m_main_render_pass_descriptor
            ->colorAttachments()
            ->object(0)
            ->setTexture(
                metal_context->m_render_states.m_main_this_frame_drawable->texture());
        metal_context->m_render_states.m_main_render_pass_descriptor
            ->depthAttachment()
            ->setTexture(metal_context->m_main_depth_texture_target);

        metal_context->m_render_states.m_currnet_command_buffer = nullptr;
        metal_context->m_render_states.m_current_command_encoder = nullptr;

        // If glfw enabled.
        switch (metal_context->m_interface->update())
        {
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_update_action::JEGL_UPDATE_STOP;
            goto _label_jegl_metal_normal_job;
        case basic_interface::update_result::PAUSE:
            return jegl_update_action::JEGL_UPDATE_SKIP;
        case basic_interface::update_result::RESIZE:
            je_io_get_window_size(
                &metal_context->m_render_states.m_screen_width,
                &metal_context->m_render_states.m_screen_height);
            metal_context->m_metal_layer->setDrawableSize(
                CGSizeMake(
                    metal_context->m_render_states.m_screen_width,
                    metal_context->m_render_states.m_screen_height));
            metal_context->recreate_main_depth_texture(
                metal_context->m_render_states.m_screen_width,
                metal_context->m_render_states.m_screen_height);
            [[fallthrough]];
        case basic_interface::update_result::NORMAL:
        _label_jegl_metal_normal_job:
            return jegl_update_action::JEGL_UPDATE_CONTINUE;
        }
    }

    void bind_framebuffer(
        jegl_context::graphic_impl_context_t ctx,
        jegl_frame_buffer* fb,
        const int32_t(*viewport_xywh)[4],
        const jegl_frame_buffer_clear_operation* clear_operations);

    jegl_update_action commit_update(
        jegl_context::graphic_impl_context_t ctx, jegl_update_action)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        // 渲染工作结束，检查，结束当前编码器，提交命令缓冲区，然后切换到默认帧缓冲
        bind_framebuffer(ctx, nullptr, nullptr, nullptr);

        jegui_update_metal(
            metal_context->m_render_states.m_main_render_pass_descriptor,
            metal_context->m_render_states.m_currnet_command_buffer,
            metal_context->m_render_states.m_current_command_encoder);

        metal_context->m_render_states.m_current_command_encoder->endEncoding();
        metal_context->m_render_states.m_currnet_command_buffer->presentDrawable(
            metal_context->m_render_states.m_main_this_frame_drawable);
        metal_context->m_render_states.m_currnet_command_buffer->commit();

        // 帧结束后释放自动释放池
        metal_context->m_frame_auto_release->release();

        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }


    jegl_resource_blob create_shader_blob(jegl_context::graphic_impl_context_t ctx, jegl_shader* shader)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        NS::Error* error_info = nullptr;
        MTL::Library* vertex_library = nullptr;
        MTL::Library* fragment_library = nullptr;

        bool shader_load_failed = false;
        std::string error_informations;

        vertex_library = metal_context->m_metal_device->newLibrary(
            NS::String::string(shader->m_vertex_msl_mac_src, NS::StringEncoding::UTF8StringEncoding),
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
            NS::String::string(shader->m_fragment_msl_mac_src, NS::StringEncoding::UTF8StringEncoding),
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
                "Fail to load shader '%s':\n%s",
                shader->m_handle.m_path_may_null_if_builtin != nullptr
                ? shader->m_handle.m_path_may_null_if_builtin
                : "<builtin>",
                error_informations.c_str());
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

            for (size_t i = 0; i < shader->m_vertex_in_count; ++i)
            {
                auto* attribute = vertex_descriptor->attributes()->object(i);
                attribute->setBufferIndex(0);
                attribute->setOffset(current_offset);

                unsigned int attribute_size = 0;

                switch (shader->m_vertex_in[i])
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
            if (shader->m_vertex_in_count > 0)
            {
                auto* layout = vertex_descriptor->layouts()->object(0);
                layout->setStride(MTL::BufferLayoutStrideDynamic); // 使用动态stride
                layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
            }

            MTL::DepthStencilDescriptor* depth_stencil_descriptor =
                MTL::DepthStencilDescriptor::alloc()->init();

            switch (shader->m_depth_mask)
            {
            case jegl_shader::depth_mask_method::DISABLE:
                depth_stencil_descriptor->setDepthWriteEnabled(false);
                break;
            case jegl_shader::depth_mask_method::ENABLE:
                depth_stencil_descriptor->setDepthWriteEnabled(true);
                break;
            default:
                jeecs::debug::logfatal("Unsupported depth mask mode.");
                abort();
                break;
            }

            switch (shader->m_depth_test)
            {
            case jegl_shader::depth_test_method::NEVER:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionNever);
                break;
            case jegl_shader::depth_test_method::LESS:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
                break;
            case jegl_shader::depth_test_method::EQUAL:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionEqual);
                break;
            case jegl_shader::depth_test_method::LESS_EQUAL:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionLessEqual);
                break;
            case jegl_shader::depth_test_method::GREATER:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionGreater);
                break;
            case jegl_shader::depth_test_method::NOT_EQUAL:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionNotEqual);
                break;
            case jegl_shader::depth_test_method::GREATER_EQUAL:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionGreaterEqual);
                break;
            case jegl_shader::depth_test_method::ALWAYS:
                depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionAlways);
                break;
            default:
                jeecs::debug::logfatal("Unsupported depth test mode.");
                abort();
                break;
            }

            MTL::DepthStencilState* depth_stencil_state =
                metal_context->m_metal_device->newDepthStencilState(depth_stencil_descriptor);
            depth_stencil_descriptor->release();

            auto blend_equation_cvt = [](jegl_shader::blend_equation eq)
                {
                    switch (eq)
                    {
                    case jegl_shader::blend_equation::DISABLED:
                        return MTL::BlendOperationAdd; // Not used.
                    case jegl_shader::blend_equation::ADD:
                        return MTL::BlendOperationAdd;
                    case jegl_shader::blend_equation::SUBTRACT:
                        return MTL::BlendOperationSubtract;
                    case jegl_shader::blend_equation::REVERSE_SUBTRACT:
                        return MTL::BlendOperationReverseSubtract;
                    case jegl_shader::blend_equation::MIN:
                        return MTL::BlendOperationMin;
                    case jegl_shader::blend_equation::MAX:
                        return MTL::BlendOperationMax;
                    default:
                        jeecs::debug::logfatal("Unsupported blend equation.");
                        abort();
                    }
                };

            auto blend_method_cvt = [](jegl_shader::blend_method method)
                {
                    switch (method)
                    {
                    case jegl_shader::blend_method::ZERO:
                        return MTL::BlendFactorZero;
                    case jegl_shader::blend_method::ONE:
                        return MTL::BlendFactorOne;
                    case jegl_shader::blend_method::SRC_COLOR:
                        return MTL::BlendFactorSourceColor;
                    case jegl_shader::blend_method::SRC_ALPHA:
                        return MTL::BlendFactorSourceAlpha;
                    case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                        return MTL::BlendFactorOneMinusSourceAlpha;
                    case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                        return MTL::BlendFactorOneMinusSourceColor;
                    case jegl_shader::blend_method::DST_COLOR:
                        return MTL::BlendFactorDestinationColor;
                    case jegl_shader::blend_method::DST_ALPHA:
                        return MTL::BlendFactorDestinationAlpha;
                    case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                        return MTL::BlendFactorOneMinusDestinationAlpha;
                    case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                        return MTL::BlendFactorOneMinusDestinationColor;
                    default:
                        jeecs::debug::logfatal("Unsupported blend method.");
                        abort();
                    }
                };

            MTL::CullMode cull_mode;

            switch (shader->m_cull_mode)
            {
            case jegl_shader::cull_mode::NONE:
                cull_mode = MTL::CullModeNone;
                break;
            case jegl_shader::cull_mode::FRONT:
                cull_mode = MTL::CullModeFront;
                break;
            case jegl_shader::cull_mode::BACK:
                cull_mode = MTL::CullModeBack;
                break;
            default:
                jeecs::debug::logfatal("Unsupported cull mode.");
                abort();
                break;
            }

            bool blend_enabled = true;

            if (shader->m_blend_equation == jegl_shader::blend_equation::DISABLED)
                blend_enabled = false;

            metal_resource_shader_blob* shader_blob =
                new metal_resource_shader_blob(
                    metal_context,
                    vertex_main_function,
                    fragment_main_function,
                    vertex_descriptor,
                    depth_stencil_state,
                    blend_enabled,
                    blend_method_cvt(shader->m_blend_src_mode),
                    blend_method_cvt(shader->m_blend_dst_mode),
                    blend_equation_cvt(shader->m_blend_equation),
                    cull_mode);

            // Update uniforms layout.
            uint32_t last_elem_end_place = 0;
            size_t max_allign = 4;
            auto* uniforms = shader->m_custom_uniforms;
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
                case jegl_shader::uniform_type::FLOAT2X2:
                    unit_size = 16;
                    allign_base = 8;
                case jegl_shader::uniform_type::FLOAT3X3:
                    unit_size = 48;
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

            // Create samplers
            for (size_t i = 0; i < shader->m_sampler_count; ++i)
            {
                const auto& sampler_method = shader->m_sampler_methods[i];

                auto filter_mode_cvt = [](jegl_shader::fliter_mode fmode)
                    {
                        switch (fmode)
                        {
                        case jegl_shader::fliter_mode::NEAREST:
                            return MTL::SamplerMinMagFilterNearest;
                        case jegl_shader::fliter_mode::LINEAR:
                            return MTL::SamplerMinMagFilterLinear;
                        default:
                            jeecs::debug::logfatal("Unsupported filter mode.");
                            return MTL::SamplerMinMagFilterNearest;
                        }
                    };
                auto wrap_mode_cvt = [](jegl_shader::wrap_mode wmode)
                    {
                        switch (wmode)
                        {
                        case jegl_shader::wrap_mode::REPEAT:
                            return MTL::SamplerAddressModeRepeat;
                        case jegl_shader::wrap_mode::CLAMP:
                            return MTL::SamplerAddressModeClampToEdge;
                        default:
                            jeecs::debug::logfatal("Unsupported wrap mode.");
                            return MTL::SamplerAddressModeClampToEdge;
                        }
                    };

                MTL::SamplerDescriptor* sampler_desc =
                    MTL::SamplerDescriptor::alloc()->init();

                sampler_desc->setMinFilter(filter_mode_cvt(sampler_method.m_min));
                sampler_desc->setMagFilter(filter_mode_cvt(sampler_method.m_mag));
                sampler_desc->setSAddressMode(wrap_mode_cvt(sampler_method.m_uwrap));
                sampler_desc->setTAddressMode(wrap_mode_cvt(sampler_method.m_vwrap));

                MTL::SamplerState* sampler_state =
                    metal_context->m_metal_device->newSamplerState(sampler_desc);

                sampler_desc->release();

                metal_resource_shader_blob::shared_state::sampler_structs sampler_struct;
                sampler_struct.m_sampler = sampler_state;
                sampler_struct.m_sampler_id = sampler_method.m_sampler_id;

                shader_blob->m_shared_state->m_samplers.push_back(sampler_struct);
            }
            return shader_blob;
        }
    }
    void close_shader_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob blob)
    {
        delete reinterpret_cast<metal_resource_shader_blob*>(blob);
    }
    jegl_resource_blob create_texture_blob(jegl_context::graphic_impl_context_t, jegl_texture*)
    {
        return nullptr;
    }
    void close_texture_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }
    jegl_resource_blob create_vertex_blob(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
        return nullptr;
    }
    void close_vertex_blob(jegl_context::graphic_impl_context_t, jegl_resource_blob)
    {
    }

    void init_shader(jegl_context::graphic_impl_context_t, jegl_resource_blob blob, jegl_shader* shader)
    {
        if (blob != nullptr)
        {
            auto* shader_blob = reinterpret_cast<metal_resource_shader_blob*>(blob);

            metal_shader* metal_shader_instance = new metal_shader(shader_blob);
            shader->m_handle.m_ptr = metal_shader_instance;

            // Read and fetch uniform locations.
            auto& builtin_uniforms = shader->m_builtin_uniforms;

            builtin_uniforms.m_builtin_uniform_ndc_scale = shader_blob->get_built_in_location("JE_NDC_SCALE");
            builtin_uniforms.m_builtin_uniform_m = shader_blob->get_built_in_location("JE_M");
            builtin_uniforms.m_builtin_uniform_mv = shader_blob->get_built_in_location("JE_MV");
            builtin_uniforms.m_builtin_uniform_mvp = shader_blob->get_built_in_location("JE_MVP");
            builtin_uniforms.m_builtin_uniform_tiling = shader_blob->get_built_in_location("JE_UV_TILING");
            builtin_uniforms.m_builtin_uniform_offset = shader_blob->get_built_in_location("JE_UV_OFFSET");
            builtin_uniforms.m_builtin_uniform_light2d_resolution =
                shader_blob->get_built_in_location("JE_LIGHT2D_RESOLUTION");
            builtin_uniforms.m_builtin_uniform_light2d_decay =
                shader_blob->get_built_in_location("JE_LIGHT2D_DECAY");

            metal_shader_instance->m_ndc_scale_uniform_id =
                builtin_uniforms.m_builtin_uniform_ndc_scale;

            // ATTENTION: 注意，以下参数特殊shader可能挪作他用
            builtin_uniforms.m_builtin_uniform_local_scale = shader_blob->get_built_in_location("JE_LOCAL_SCALE");
            builtin_uniforms.m_builtin_uniform_color = shader_blob->get_built_in_location("JE_COLOR");

            auto* uniforms = shader->m_custom_uniforms;
            while (uniforms != nullptr)
            {
                uniforms->m_index = shader_blob->get_built_in_location(uniforms->m_name);
                uniforms = uniforms->m_next;
            }
        }
        else
            shader->m_handle.m_ptr = nullptr;
    }
    void init_texture(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob, jegl_texture* texture)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        MTL::TextureDescriptor* texture_desc = MTL::TextureDescriptor::alloc()->init();

        bool float16 = 0 != (texture->m_format & jegl_texture::format::FLOAT16);
        bool is_cube = 0 != (texture->m_format & jegl_texture::format::CUBE);
        bool is_framebuf = 0 != (texture->m_format & jegl_texture::format::FRAMEBUF);
        bool is_depth = 0 != (texture->m_format & jegl_texture::format::DEPTH);

        (void)is_cube;
        assert(!is_cube); // TODO;

        texture_desc->setWidth((uint32_t)texture->m_width);
        texture_desc->setHeight((uint32_t)texture->m_height);
        texture_desc->setTextureType(MTL::TextureType2D);

        if (is_framebuf)
        {
            texture_desc->setUsage(
                MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
            texture_desc->setStorageMode(MTL::StorageModePrivate);
        }
        else
        {
            texture_desc->setUsage(MTL::TextureUsageShaderRead);
            texture_desc->setStorageMode(MTL::StorageModeManaged);
        }

        MTL::PixelFormat texture_format;

        if (is_depth)
        {
            assert(is_framebuf);
            texture_format = MTL::PixelFormatDepth16Unorm;
        }
        else
        {
            switch (texture->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
            {
            case jegl_texture::format::MONO:
                texture_format =
                    float16 ? MTL::PixelFormatR16Float : MTL::PixelFormatR8Unorm;
                break;
            case jegl_texture::format::RGBA:
                texture_format =
                    float16 ? MTL::PixelFormatRGBA16Float : MTL::PixelFormatRGBA8Unorm;
                break;
            default:
                jeecs::debug::logfatal("Unsupported texture color format.");
                abort();
                break;
            }
        }

        texture_desc->setPixelFormat(texture_format);

        MTL::Texture* texture_instance =
            metal_context->m_metal_device->newTexture(texture_desc);

        if (texture->m_format & jegl_texture::format::FRAMEBUF)
        {
            // No need to upload data.
        }
        else
        {
            texture_instance->replaceRegion(
                MTL::Region(
                    0,
                    0,
                    0,
                    (uint32_t)texture->m_width,
                    (uint32_t)texture->m_height,
                    1),
                0,
                texture->m_pixels,
                (uint32_t)texture->m_width
                * (float16 ? 2 : 1)
                * (texture->m_format & jegl_texture::format::COLOR_DEPTH_MASK));
        }
        texture->m_handle.m_ptr = new metal_texture(texture_instance, texture_format);
        texture_desc->release();
    }
    void init_vertex(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob, jegl_vertex* vertex)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        // Create vertex buffer
        MTL::Buffer* vertex_buffer = metal_context->m_metal_device->newBuffer(
            vertex->m_vertexs,
            vertex->m_vertex_length,
            MTL::ResourceStorageModeShared);

        // Create index buffer
        MTL::Buffer* index_buffer = metal_context->m_metal_device->newBuffer(
            vertex->m_indices,
            vertex->m_index_count * sizeof(uint32_t),
            MTL::ResourceStorageModeShared);

        // Map primitive types
        MTL::PrimitiveType primitive_type;
        switch (vertex->m_type)
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
            primitive_type = MTL::PrimitiveTypePoint;
            break;
        }

        vertex->m_handle.m_ptr = new metal_vertex(
            primitive_type,
            vertex_buffer,
            index_buffer,
            vertex->m_index_count,
            vertex->m_data_size_per_point);
    }
    void init_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer* fbuffer)
    {
        metal_framebuffer* framebuf = new metal_framebuffer(
            fbuffer->m_width,
            fbuffer->m_height);

        jeecs::basic::resource<jeecs::graphic::texture>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
                fbuffer->m_output_attachments);

        size_t color_attachment_count = 0;

        for (size_t i = 0; i < fbuffer->m_attachment_count; ++i)
        {
            auto* attachment_resource = attachments[i]->resource();
            jegl_bind_texture(attachment_resource, 0);

            if (attachment_resource->m_format & jegl_texture::format::DEPTH)
            {
                // Is depth attachment.
                framebuf->m_has_depth_attachment = true;

                MTL::RenderPassDepthAttachmentDescriptor* depth_attachment_desc = framebuf
                    ->m_render_pass_descriptor
                    ->depthAttachment();

                auto* texture_instance = reinterpret_cast<metal_texture*>(
                    attachment_resource->m_handle.m_ptr);
                depth_attachment_desc->setTexture(texture_instance->m_texture);
                depth_attachment_desc->setLoadAction(MTL::LoadActionDontCare);
                depth_attachment_desc->setStoreAction(MTL::StoreActionStore);
                depth_attachment_desc->setClearDepth(1.0);
            }
            else
            {
                // Is color attachment.
                MTL::RenderPassColorAttachmentDescriptor* color_attachment_desc = framebuf
                    ->m_render_pass_descriptor
                    ->colorAttachments()
                    ->object(color_attachment_count);

                auto* texture_instance = reinterpret_cast<metal_texture*>(
                    attachment_resource->m_handle.m_ptr);

                framebuf->m_color_attachment_formats.push_back(
                    texture_instance->m_pixel_format);

                color_attachment_desc->setTexture(texture_instance->m_texture);
                color_attachment_desc->setLoadAction(MTL::LoadActionDontCare);
                color_attachment_desc->setStoreAction(MTL::StoreActionStore);
                color_attachment_desc->setClearColor(
                    MTL::ClearColor{
                        0.0, 0.0, 0.0, 1.0 });

                ++color_attachment_count;
            }
        }
        fbuffer->m_handle.m_ptr = framebuf;
    }
    void init_ubuffer(jegl_context::graphic_impl_context_t ctx, jegl_uniform_buffer* ubuffer)
    {
        jegl_metal_context* metal_context =
            reinterpret_cast<jegl_metal_context*>(ctx);

        ubuffer->m_handle.m_ptr = new metal_uniform_buffer(metal_context, ubuffer);
    }

    void update_shader(jegl_context::graphic_impl_context_t, jegl_shader*)
    {
    }
    void update_texture(jegl_context::graphic_impl_context_t, jegl_texture*)
    {
        // TODO;
    }
    void update_vertex(jegl_context::graphic_impl_context_t, jegl_vertex*)
    {
    }
    void update_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer*)
    {
    }
    void update_ubuffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer* ubuffer)
    {
        metal_uniform_buffer* ubuf =
            reinterpret_cast<metal_uniform_buffer*>(ubuffer->m_handle.m_ptr);

        assert(ubuffer->m_update_length != 0);
        void* buffer_contents = ubuf->m_uniform_buffer->contents();

        memcpy(
            reinterpret_cast<void*>(
                reinterpret_cast<intptr_t>(buffer_contents)
                + ubuffer->m_update_begin_offset),
            reinterpret_cast<void*>(
                reinterpret_cast<intptr_t>(ubuffer->m_buffer)
                + ubuffer->m_update_begin_offset),
            ubuffer->m_update_length);
    }

    void close_shader(jegl_context::graphic_impl_context_t, jegl_shader* shader)
    {
        delete reinterpret_cast<metal_shader*>(shader->m_handle.m_ptr);
    }
    void close_texture(jegl_context::graphic_impl_context_t, jegl_texture* texture)
    {
        delete reinterpret_cast<metal_texture*>(texture->m_handle.m_ptr);
    }
    void close_vertex(jegl_context::graphic_impl_context_t, jegl_vertex* vertex)
    {
        delete reinterpret_cast<metal_vertex*>(vertex->m_handle.m_ptr);
    }
    void close_framebuffer(jegl_context::graphic_impl_context_t, jegl_frame_buffer* fbuffer)
    {
        delete reinterpret_cast<metal_framebuffer*>(fbuffer->m_handle.m_ptr);
    }
    void close_ubuffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer* ubuffer)
    {
        delete reinterpret_cast<metal_uniform_buffer*>(ubuffer->m_handle.m_ptr);
    }

    void bind_uniform_buffer(jegl_context::graphic_impl_context_t ctx, jegl_uniform_buffer* res)
    {
        metal_uniform_buffer* ubuf =
            reinterpret_cast<metal_uniform_buffer*>(res->m_handle.m_ptr);

        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        metal_context->m_render_states.m_current_command_encoder->setVertexBuffer(
            ubuf->m_uniform_buffer, 0, ubuf->m_binding_place);
        metal_context->m_render_states.m_current_command_encoder->setFragmentBuffer(
            ubuf->m_uniform_buffer, 0, ubuf->m_binding_place);
    }
    bool bind_shader(jegl_context::graphic_impl_context_t ctx, jegl_shader* res)
    {
        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);
        auto* shader_instance = reinterpret_cast<metal_shader*>(res->m_handle.m_ptr);

        if (metal_context->m_render_states.m_current_target_shader == shader_instance)
            return shader_instance != nullptr;

        metal_context->m_render_states.m_current_target_shader = shader_instance;
        if (shader_instance == nullptr)
            return false;

        auto& shader_shared_state = *shader_instance->m_shared_state;
        auto* current_target_framebuffer =
            metal_context->m_render_states.m_current_target_framebuffer_may_null;

        MTL::RenderPipelineState* pso;

        auto fnd = shader_shared_state.m_pipeline_states.find(
            current_target_framebuffer);

        if (fnd != shader_shared_state.m_pipeline_states.end())
            pso = fnd->second;
        else
        {
            auto apply_blend_setting_to_attachment =
                [&](MTL::RenderPipelineColorAttachmentDescriptor* desc)
                {
                    desc->setBlendingEnabled(shader_shared_state.m_blend_enabled);
                    if (shader_shared_state.m_blend_enabled)
                    {
                        desc->setRgbBlendOperation(shader_shared_state.m_blend_operation);
                        desc->setAlphaBlendOperation(shader_shared_state.m_blend_operation);
                        desc->setSourceRGBBlendFactor(shader_shared_state.m_blend_src_factor);
                        desc->setSourceAlphaBlendFactor(shader_shared_state.m_blend_src_factor);
                        desc->setDestinationRGBBlendFactor(shader_shared_state.m_blend_dst_factor);
                        desc->setDestinationAlphaBlendFactor(shader_shared_state.m_blend_dst_factor);
                    }
                };

            MTL::RenderPipelineDescriptor* pDesc =
                MTL::RenderPipelineDescriptor::alloc()->init();

            pDesc->setVertexFunction(shader_shared_state.m_vertex_function);
            pDesc->setFragmentFunction(shader_shared_state.m_fragment_function);
            pDesc->setVertexDescriptor(shader_shared_state.m_vertex_descriptor);

            if (current_target_framebuffer != nullptr)
            {
                for (size_t i = 0; i < current_target_framebuffer->m_color_attachment_formats.size(); ++i)
                {
                    auto* desc = pDesc->colorAttachments()->object(i);
                    desc->setPixelFormat(
                        current_target_framebuffer->m_color_attachment_formats[i]);

                    apply_blend_setting_to_attachment(desc);
                }
                if (current_target_framebuffer->m_has_depth_attachment)
                    pDesc->setDepthAttachmentPixelFormat(
                        MTL::PixelFormatDepth16Unorm);
            }
            else
            {
                // Create pso for main framebuffer
                auto* desc = pDesc->colorAttachments()->object(0);
                desc->setPixelFormat(
                    MTL::PixelFormat::PixelFormatBGRA8Unorm);

                apply_blend_setting_to_attachment(desc);

                pDesc->setDepthAttachmentPixelFormat(
                    MTL::PixelFormatDepth16Unorm);
            }

            NS::Error* pError = nullptr;
            pso = metal_context->m_metal_device->newRenderPipelineState(pDesc, &pError);
            if (pso == nullptr)
            {
                jeecs::debug::logfatal(
                    "Fail to create pipeline state object for shader '%s':\n%s",
                    res->m_handle.m_path_may_null_if_builtin != nullptr
                    ? res->m_handle.m_path_may_null_if_builtin
                    : "<builtin>",
                    pError->localizedDescription()->utf8String());
                abort();
            }

            do
            {
                auto result = shader_shared_state.m_pipeline_states.insert(
                    std::make_pair(current_target_framebuffer, pso));
                (void)result;
                assert(result.second);
            } while (0);

            if (current_target_framebuffer != nullptr)
            {
                auto result = current_target_framebuffer->m_linked_shaders.insert(
                    &shader_shared_state);
                (void)result;
                assert(result.second);
            }

            pDesc->release();
        }

        // 开始绑定使用的 PSO, 深度缓冲描述符，面剪裁描述符等
        metal_context->m_render_states.m_current_command_encoder->setRenderPipelineState(pso);

        metal_context->m_render_states.m_current_command_encoder->setDepthStencilState(
            shader_shared_state.m_depth_stencil_state);
        metal_context->m_render_states.m_current_command_encoder->setCullMode(
            shader_shared_state.m_cull_mode);

        if (current_target_framebuffer == nullptr)
            metal_context->m_render_states.m_current_command_encoder->setFrontFacingWinding(
                MTL::WindingCounterClockwise);
        else
            metal_context->m_render_states.m_current_command_encoder->setFrontFacingWinding(
                MTL::WindingClockwise);

        for (const auto& sampler_struct : shader_shared_state.m_samplers)
        {
            metal_context->m_render_states.m_current_command_encoder->setFragmentSamplerState(
                sampler_struct.m_sampler,
                sampler_struct.m_sampler_id);
        }

        return true;
    }
    void bind_texture(jegl_context::graphic_impl_context_t ctx, jegl_texture* res, size_t pass)
    {
        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);
        auto* texture_instance = reinterpret_cast<metal_texture*>(res->m_handle.m_ptr);

        // 由于其他图形库不支持在顶点着色器中使用纹理采样，所以这里不绑定顶点着色器纹理
        metal_context->m_render_states.m_current_command_encoder->setFragmentTexture(
            texture_instance->m_texture, (uint32_t)pass);
    }
    void set_uniform(
        jegl_context::graphic_impl_context_t ctx,
        uint32_t location,
        jegl_shader::uniform_type type,
        const void* val)
    {
        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        auto* current_shader = metal_context->m_render_states.m_current_target_shader;

        if (location == jeecs::graphic::INVALID_UNIFORM_LOCATION
            || current_shader == nullptr)
            return;

        current_shader->m_uniform_buffer_updated = true;

        void* target_buffer = reinterpret_cast<void*>(
            reinterpret_cast<intptr_t>(
                current_shader->m_uniform_cpu_buffer) + location);

        size_t data_size_byte_length = 0;
        switch (type)
        {
        case jegl_shader::INT:
        case jegl_shader::FLOAT:
            data_size_byte_length = 4;
            break;
        case jegl_shader::INT2:
        case jegl_shader::FLOAT2:
            data_size_byte_length = 8;
            break;
        case jegl_shader::INT3:
        case jegl_shader::FLOAT3:
            data_size_byte_length = 12;
            break;
        case jegl_shader::INT4:
        case jegl_shader::FLOAT4:
            data_size_byte_length = 16;
            break;
        case jegl_shader::FLOAT2X2:
            data_size_byte_length = 16;
            break;
        case jegl_shader::FLOAT3X3:
        {
            float* target_storage = reinterpret_cast<float*>(target_buffer);
            const float* source_storage = reinterpret_cast<const float*>(val);

            memcpy(target_storage, source_storage, 12);
            memcpy(target_storage + 4, source_storage + 3, 12);
            memcpy(target_storage + 8, source_storage + 6, 12);

            return;
        }
        case jegl_shader::FLOAT4X4:
            data_size_byte_length = 64;
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set.");
            break;
        }

        memcpy(target_buffer, val, data_size_byte_length);
    }

    void draw_vertex_with_shader(jegl_context::graphic_impl_context_t ctx, jegl_vertex* res)
    {
        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);
        auto* vertex_instance =
            reinterpret_cast<metal_vertex*>(res->m_handle.m_ptr);

        auto* current_shader = metal_context->m_render_states.m_current_target_shader;
        assert(current_shader != nullptr);

        if (current_shader->m_uniform_cpu_buffer_size != 0)
        {
            if (metal_context->m_render_states.m_current_target_framebuffer_may_null == nullptr)
            {
                if (current_shader->m_draw_for_r2b)
                {
                    // 正准备渲染到屏幕，但是此前shader被用于渲染到缓冲区，重置 NDC 矫正
                    const float ndc_scale[4] = { 1.f, 1.f, 1.f, 1.f };

                    current_shader->m_draw_for_r2b = false;
                    set_uniform(
                        ctx,
                        current_shader->m_ndc_scale_uniform_id,
                        jegl_shader::uniform_type::FLOAT4,
                        ndc_scale);
                }
            }
            else
            {
                if (!current_shader->m_draw_for_r2b)
                {
                    // 和上面的情况相反，准备渲染到缓冲区，准备 NDC 矫正
                    const float ndc_scale_r2b[4] = { 1.f, -1.f, 1.f, 1.f };
                    current_shader->m_draw_for_r2b = true;
                    set_uniform(
                        ctx,
                        current_shader->m_ndc_scale_uniform_id,
                        jegl_shader::uniform_type::FLOAT4,
                        ndc_scale_r2b);
                }
            }
        }

        MTL::Buffer* current_shader_uniform_buffer;
        if (current_shader->m_uniform_buffer_updated)
        {
            current_shader->m_uniform_buffer_updated = false;
            current_shader_uniform_buffer = current_shader->allocate_buffer_to_update(metal_context);

            // 将CPU端的uniform数据更新到GPU缓冲区
            void* buffer_contents = current_shader_uniform_buffer->contents();
            memcpy(
                buffer_contents,
                current_shader->m_uniform_cpu_buffer,
                current_shader->m_uniform_cpu_buffer_size);
        }
        else
            current_shader_uniform_buffer = current_shader->get_last_usable_buffer(metal_context);

        metal_context->m_render_states.m_current_command_encoder->setVertexBuffer(
            vertex_instance->m_vertex_buffer, 0, vertex_instance->m_vertex_stride, 0);

        // Binding shader uniform.
        metal_context->m_render_states.m_current_command_encoder->setVertexBuffer(
            current_shader_uniform_buffer, 0, 0 + 1);
        metal_context->m_render_states.m_current_command_encoder->setFragmentBuffer(
            current_shader_uniform_buffer, 0, 0 + 1);

        metal_context->m_render_states.m_current_command_encoder->drawIndexedPrimitives(
            vertex_instance->m_primitive_type,
            vertex_instance->m_index_count,
            MTL::IndexType::IndexTypeUInt32,
            vertex_instance->m_index_buffer,
            0);
    }

    void set_framebuffer_clear(
        jegl_metal_context* metal_context,
        metal_framebuffer* fb_maynull,
        const jegl_frame_buffer_clear_operation* clear_operations)
    {
        uint64_t color_attachment_cleared_mask = 0;
        bool depth_cleared = false;

        assert(fb_maynull == nullptr || fb_maynull->m_color_attachment_formats.size() < 64);

        while (clear_operations != nullptr)
        {
            switch (clear_operations->m_type)
            {
            case jegl_frame_buffer_clear_operation::clear_type::COLOR:
            {
                color_attachment_cleared_mask
                    |= (1ull << clear_operations->m_color.m_color_attachment_idx);

                MTL::RenderPassColorAttachmentDescriptor* color_attachment_desc = nullptr;
                if (fb_maynull == nullptr)
                {
                    if (clear_operations->m_color.m_color_attachment_idx == 0)
                    {
                        color_attachment_desc =
                            metal_context->m_render_states.m_main_render_pass_descriptor
                            ->colorAttachments()
                            ->object(0);
                    }
                }
                else
                {
                    if (clear_operations->m_color.m_color_attachment_idx <
                        fb_maynull->m_color_attachment_formats.size())
                    {
                        color_attachment_desc =
                            fb_maynull->m_render_pass_descriptor
                            ->colorAttachments()
                            ->object(clear_operations->m_color.m_color_attachment_idx);
                    }
                }

                if (color_attachment_desc != nullptr)
                {
                    color_attachment_desc->setLoadAction(MTL::LoadActionClear);
                    color_attachment_desc->setClearColor(
                        MTL::ClearColor{
                            clear_operations->m_color.m_clear_color_rgba[0],
                            clear_operations->m_color.m_clear_color_rgba[1],
                            clear_operations->m_color.m_clear_color_rgba[2],
                            clear_operations->m_color.m_clear_color_rgba[3] });
                }
                break;
            }
            case jegl_frame_buffer_clear_operation::clear_type::DEPTH:
            {
                MTL::RenderPassDepthAttachmentDescriptor* depth_attachment_desc = nullptr;
                depth_cleared = true;

                if (fb_maynull == nullptr)
                {
                    depth_attachment_desc = metal_context
                        ->m_render_states
                        .m_main_render_pass_descriptor
                        ->depthAttachment();
                }
                else
                {
                    if (fb_maynull->m_has_depth_attachment)
                    {
                        depth_attachment_desc = fb_maynull
                            ->m_render_pass_descriptor
                            ->depthAttachment();
                    }
                }

                if (depth_attachment_desc != nullptr)
                {
                    depth_attachment_desc->setLoadAction(MTL::LoadActionClear);
                    depth_attachment_desc->setClearDepth(clear_operations->m_depth.m_clear_depth);
                }

                break;
            }
            default:
                jeecs::debug::logfatal("Unknown framebuffer clear operation.");
                abort();
                break;
            }
            clear_operations = clear_operations->m_next;
        }

        if (!depth_cleared)
        {
            if (fb_maynull == nullptr)
            {
                metal_context
                    ->m_render_states
                    .m_main_render_pass_descriptor
                    ->depthAttachment()
                    ->setLoadAction(MTL::LoadActionLoad);
            }
            else
            {
                if (fb_maynull->m_has_depth_attachment)
                {
                    fb_maynull
                        ->m_render_pass_descriptor
                        ->depthAttachment()
                        ->setLoadAction(MTL::LoadActionLoad);
                }
            }
        }

        size_t i = fb_maynull == nullptr ? 1 : fb_maynull->m_color_attachment_formats.size();
        for (; i > 0; --i)
        {
            if ((color_attachment_cleared_mask & (1ull << (i - 1))) == 0)
            {
                MTL::RenderPassColorAttachmentDescriptor* color_attachment_desc = nullptr;
                if (fb_maynull == nullptr)
                {
                    assert(i - 1 == 0);
                    color_attachment_desc =
                        metal_context->m_render_states.m_main_render_pass_descriptor
                        ->colorAttachments()
                        ->object(0);
                }
                else
                {
                    color_attachment_desc =
                        fb_maynull->m_render_pass_descriptor
                        ->colorAttachments()
                        ->object(i - 1);
                }
                color_attachment_desc->setLoadAction(MTL::LoadActionLoad);
            }
        }
    }

    void bind_framebuffer(
        jegl_context::graphic_impl_context_t ctx,
        jegl_frame_buffer* fb,
        const int32_t(*viewport_xywh)[4],
        const jegl_frame_buffer_clear_operation* clear_operations)
    {
        auto* metal_context = reinterpret_cast<jegl_metal_context*>(ctx);

        // Reset current binded shader.
        metal_context->m_render_states.m_current_target_shader = nullptr;

        // Finish last command encoder and buffer if any.
        if (metal_context->m_render_states.m_current_command_encoder != nullptr)
        {
            assert(metal_context->m_render_states.m_currnet_command_buffer != nullptr);

            metal_context->m_render_states.m_current_command_encoder->endEncoding();
            metal_context->m_render_states.m_currnet_command_buffer->commit();
        }

        metal_framebuffer* target_framebuf_may_null = fb == nullptr
            ? nullptr
            : reinterpret_cast<metal_framebuffer*>(fb->m_handle.m_ptr);

        set_framebuffer_clear(metal_context, target_framebuf_may_null, clear_operations);

        // Create a new command buffer and encoder for the new framebuffer
        auto* target_framebuffer_desc = target_framebuf_may_null != nullptr
            ? target_framebuf_may_null->m_render_pass_descriptor
            : metal_context->m_render_states.m_main_render_pass_descriptor;

        metal_context->m_render_states.m_currnet_command_buffer =
            metal_context->m_command_queue->commandBuffer();
        metal_context->m_render_states.m_current_command_encoder =
            metal_context->m_render_states.m_currnet_command_buffer->renderCommandEncoder(
                target_framebuffer_desc);

        // Set viewport
        int32_t x = 0, y = 0, w = 0, h = 0;
        if (viewport_xywh != nullptr)
        {
            auto& v = *viewport_xywh;
            x = v[0];
            y = v[1];
            w = v[2];
            h = v[3];
        }

        const int32_t buf_h =
            static_cast<int32_t>(
                target_framebuf_may_null != nullptr
                ? target_framebuf_may_null->m_frame_height
                : metal_context->m_render_states.m_screen_height);

        if (w == 0)
        {
            w = static_cast<int32_t>(
                target_framebuf_may_null != nullptr
                ? target_framebuf_may_null->m_frame_width
                : metal_context->m_render_states.m_screen_width);
        }
        if (h == 0)
            h = buf_h;

        MTL::Viewport viewport;
        viewport.originX = (double)x;
        viewport.originY = (double)(buf_h - y - h);
        viewport.width = w;
        viewport.height = h;
        viewport.znear = 0.0;
        viewport.zfar = 1.0;

        metal_context->m_render_states.m_current_command_encoder->setViewport(
            viewport);

        metal_context->m_render_states.m_current_target_framebuffer_may_null =
            target_framebuf_may_null;
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

    write_to_apis->shader_create_blob = create_shader_blob;
    write_to_apis->texture_create_blob = create_texture_blob;
    write_to_apis->vertex_create_blob = create_vertex_blob;

    write_to_apis->shader_close_blob = close_shader_blob;
    write_to_apis->texture_close_blob = close_texture_blob;
    write_to_apis->vertex_close_blob = close_vertex_blob;

    write_to_apis->shader_init = init_shader;
    write_to_apis->texture_init = init_texture;
    write_to_apis->vertex_init = init_vertex;
    write_to_apis->framebuffer_init = init_framebuffer;
    write_to_apis->ubuffer_init = init_ubuffer;

    write_to_apis->shader_update = update_shader;
    write_to_apis->texture_update = update_texture;
    write_to_apis->vertex_update = update_vertex;
    write_to_apis->framebuffer_update = update_framebuffer;
    write_to_apis->ubuffer_update = update_ubuffer;

    write_to_apis->shader_close = close_shader;
    write_to_apis->texture_close = close_texture;
    write_to_apis->vertex_close = close_vertex;
    write_to_apis->framebuffer_close = close_framebuffer;
    write_to_apis->ubuffer_close = close_ubuffer;

    write_to_apis->bind_uniform_buffer = bind_uniform_buffer;
    write_to_apis->bind_texture = bind_texture;
    write_to_apis->bind_shader = bind_shader;
    write_to_apis->draw_vertex = draw_vertex_with_shader;

    write_to_apis->bind_framebuf = bind_framebuffer;

    write_to_apis->set_uniform = set_uniform;
}

#else
void jegl_using_metal_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("METAL not available.");
}
#endif
