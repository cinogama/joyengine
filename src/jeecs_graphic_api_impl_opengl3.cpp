#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_graphic_render_internal.hpp"

#if defined(JE_ENABLE_GL330_GAPI)       \
    || defined(JE_ENABLE_GLES300_GAPI)  \
    || defined(JE_ENABLE_WEBGL20_GAPI)

#include "jeecs_imgui_backend_api.hpp"

#ifdef JE_ENABLE_GLES300_GAPI
// #   ifdef __APPLE__
// #       include <OpenGLES/ES3/gl.h>
// #   else
#include <GLES3/gl3.h>
// #   endif
#elif defined(JE_ENABLE_WEBGL20_GAPI)
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#include "jeecs_graphic_api_interface_egl.hpp"
#else
#include "jeecs_graphic_api_interface_glfw.hpp"
#endif // JE_GL_USE_EGL_INSTEAD_GLFW

#ifdef max
#   undef max // Fuck windows.
#endif
#ifdef min
#   undef min // Fuck windows.
#endif
// Here is low-level-graphic-api impl.
// OpenGL version.
namespace jeecs::graphic::api::gl3
{
    struct jegl3_vertex_data
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl3_vertex_data);

        GLuint m_vao;
        GLuint m_vbo;
        GLuint m_ebo;
        GLenum m_method;
        GLsizei m_pointcount;

        jegl3_vertex_data() = default;
        ~jegl3_vertex_data()
        {
            glDeleteVertexArrays(1, &m_vao);
            glDeleteBuffers(1, &m_vbo);
            glDeleteBuffers(1, &m_ebo);
        }
    };
    struct jegl3_sampler
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl3_sampler);

        GLuint m_sampler;
        std::vector<uint32_t> m_passes;

        jegl3_sampler()
        {
            glGenSamplers(1, &m_sampler);
        }
        ~jegl3_sampler()
        {
            glDeleteSamplers(1, &m_sampler);
        }
    };
    struct jegl3_shader_blob
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl3_shader_blob);
        struct jegl3_shader_blob_shared
        {
            JECS_DISABLE_MOVE_AND_COPY(jegl3_shader_blob_shared);

            GLuint m_shader_program_instance;

            jegl3_sampler* m_samplers;
            uint32_t m_sampler_count;

            std::unordered_map<std::string, uint32_t> m_uniform_locations;

            GLenum m_depth_test_method;
            GLboolean m_depth_write_mask;
            GLenum m_blend_equation;
            GLenum m_blend_src_mode;
            GLenum m_blend_dst_mode;
            GLenum m_cull_face_method;

            jegl3_shader_blob_shared(GLuint shader_instance, uint32_t sampler_count)
                : m_shader_program_instance(shader_instance)
                , m_samplers(new jegl3_sampler[sampler_count])
                , m_sampler_count(sampler_count)
                , m_uniform_locations{}
            {}
            ~jegl3_shader_blob_shared()
            {
                glDeleteProgram(m_shader_program_instance);
                delete[] m_samplers;
            }

            uint32_t get_built_in_location(const std::string& name) const
            {
                auto fnd = m_uniform_locations.find(name);
                if (fnd != m_uniform_locations.end())
                    return fnd->second;

                return jeecs::graphic::INVALID_UNIFORM_LOCATION;
            }
        };

        jeecs::basic::resource<jegl3_shader_blob_shared> m_shared_blob_data;

        jegl3_shader_blob(jeecs::basic::resource<jegl3_shader_blob_shared> s)
            : m_shared_blob_data(s)
        {}
    };
    struct jegl_gl3_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_shader);

        // The per-draw uniform block is uploaded through the context's UBO
        // ring by the render API; the shader only needs the compiled program
        // and its reflection data.
        jeecs::basic::resource<jegl3_shader_blob::jegl3_shader_blob_shared>
            m_shared_blob_data;

        jegl_gl3_shader(jegl3_shader_blob* blob)
            : m_shared_blob_data(blob->m_shared_blob_data)
        {}
    };
    struct jegl_gl3_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_texture);

        GLuint m_texture_id;
        GLenum m_target; // GL_TEXTURE_2D / _3D / _2D_ARRAY / _CUBE_MAP / _2D_MULTISAMPLE
        jegl_texture_common::format m_texture_format;
        size_t m_width;
        size_t m_height;

        jegl_gl3_texture(GLuint id, GLenum target, jegl_texture_common::format fmt, size_t w, size_t h)
            : m_texture_id(id)
            , m_target(target)
            , m_texture_format(fmt)
            , m_width(w)
            , m_height(h)
        {}
        ~jegl_gl3_texture()
        {
            glDeleteTextures(1, &m_texture_id);
        }
    };
    struct jegl_gl3_framebuf
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_framebuf);

        size_t m_frame_width;
        size_t m_frame_height;

        GLuint m_fbo;

        jegl_gl3_framebuf(size_t w, size_t h, GLuint fbo)
            : m_frame_width(w)
            , m_frame_height(h)
            , m_fbo(fbo)
        {}
        ~jegl_gl3_framebuf()
        {
            glDeleteFramebuffers(1, &m_fbo);
        }
    };

    struct jegl_gl3_context
    {
        basic_interface* m_interface;

        size_t RESOLUTION_WIDTH = 0;
        size_t RESOLUTION_HEIGHT = 0;

        jegl_gl3_shader* current_active_shader_may_null = nullptr;
        GLenum ACTIVE_DEPTH_MODE = GL_INVALID_ENUM;

        // 此处使用 GLenum 是因为 GLboolean 无法准确表示未初始化状态，
        // 我们需要知道这玩意儿实际上是 GLboolean
        GLenum /*GLboolean*/ ACTIVE_MASK_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_EQUATION = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_SRC_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_DST_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_CULL_MODE = GL_INVALID_ENUM;

        // Render API (v2) state.
        GLuint m_render_uniform_ring = 0;
        GLsizeiptr m_render_uniform_ring_size = 4 * 1024 * 1024;
        GLintptr m_render_uniform_ring_offset = 0;
        GLint m_render_uniform_alignment = 256;

        jegl_shader* m_render_current_shader_resource = nullptr;
        jegl3_vertex_data* m_render_current_vertex = nullptr;

        // Clears of the default framebuffer need an explicit sRGB encode on
        // desktop GL because glClearBufferfv bypasses FRAMEBUFFER_SRGB.
        bool m_render_target_is_default = false;

        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_context);

        jegl_gl3_context(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
        {
            m_interface =
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
                new egl(egl::OPENGLES300);
#else
                new glfw(reboot
                    ? glfw::HOLD
#ifdef JE_ENABLE_GL330_GAPI
                    : glfw::OPENGL330
#else
                    : glfw::OPENGLES300
#endif
                );
#endif
        }
        ~jegl_gl3_context()
        {
            delete m_interface;
        }

        void bind_texture_pass_impl(GLint pass, GLenum type, GLuint texture)
        {
            glActiveTexture(GL_TEXTURE0 + pass);
            glBindTexture(type, texture);
        }
        void bind_sampler_pass_impl(GLuint pass, GLuint sampler)
        {
            glBindSampler(pass, sampler);
        }
    };
    struct jegl_gl3_uniformbuf
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_uniformbuf);

        GLuint m_uniform_buffer_object;
        GLuint m_binding_place;
        GLsizeiptr m_uniform_buffer_size;

        jegl_gl3_uniformbuf(jegl_uniform_buffer* resource)
        {
            glGenBuffers(1, &m_uniform_buffer_object);
            glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer_object);
            glBufferData(GL_UNIFORM_BUFFER,
                resource->m_buffer_size,
                NULL, GL_DYNAMIC_DRAW); // 预分配空间

            m_binding_place =
                (GLuint)(resource->m_buffer_binding_place + 1);
            m_uniform_buffer_size = (GLsizeiptr)resource->m_buffer_size;

            glBindBufferRange(
                GL_UNIFORM_BUFFER,
                m_binding_place,
                m_uniform_buffer_object,
                0,
                m_uniform_buffer_size);
        }
        ~jegl_gl3_uniformbuf()
        {
            glDeleteBuffers(1, &m_uniform_buffer_object);
        }
    };

    void _gl_bind_shader_samplers(jegl_gl3_context* ctx, jegl_gl3_shader* shader_instance)
    {
        const auto& blob_shared = shader_instance->m_shared_blob_data;
        for (size_t i = 0; i < blob_shared->m_sampler_count; i += 1)
        {
            auto& sampler = blob_shared->m_samplers[i];
            for (auto id : sampler.m_passes)
                ctx->bind_sampler_pass_impl(id, sampler.m_sampler);
        }
    }

#ifdef JE_ENABLE_GL330_GAPI
    void APIENTRY glDebugOutput(GLenum source,
        GLenum type,
        unsigned int id,
        GLenum severity,
        GLsizei length,
        const char* message,
        const void* userParam)
    {
        // ignore non-significant error/warning codes
        if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
            return;

        const char* source_type = "UNKNOWN";
        switch (source)
        {
        case GL_DEBUG_SOURCE_API:
            source_type = "API";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source_type = "Window System";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source_type = "Shader Compiler";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            source_type = "Third Party";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            source_type = "Application";
            break;
        case GL_DEBUG_SOURCE_OTHER:
            source_type = "Other";
            break;
        }

        const char* msg_type = "UNKNOWN";
        int jelog_level = JE_LOG_INFO;
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR:
            jelog_level = JE_LOG_ERROR;
            msg_type = "Error";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            jelog_level = JE_LOG_WARNING;
            msg_type = "Deprecated Behaviour";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            jelog_level = JE_LOG_ERROR;
            msg_type = "Undefined Behaviour";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            jelog_level = JE_LOG_WARNING;
            msg_type = "Portability";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            jelog_level = JE_LOG_WARNING;
            msg_type = "Performance";
            break;
        case GL_DEBUG_TYPE_MARKER:
            jelog_level = JE_LOG_INFO;
            msg_type = "Marker";
            break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
            jelog_level = JE_LOG_INFO;
            msg_type = "Push Group";
            break;
        case GL_DEBUG_TYPE_POP_GROUP:
            jelog_level = JE_LOG_INFO;
            msg_type = "Pop Group";
            break;
        case GL_DEBUG_TYPE_OTHER:
            jelog_level = JE_LOG_INFO;
            msg_type = "Other";
            break;
        }

        switch (severity)
        {
        case GL_DEBUG_SEVERITY_HIGH:
            jelog_level = JE_LOG_FATAL;
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:;
            break;
        case GL_DEBUG_SEVERITY_LOW:;
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:;
            break;
        }

        je_log(jelog_level, "(%d)%s-%s: %s", id, source_type, msg_type, message);
    }
#endif
    je_GraphicImplContext gl_startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_gl3_context* context = new jegl_gl3_context(gthread, config, reboot);

        if (!reboot)
        {
            jeecs::debug::log("Graphic thread (OpenGL3) start!");
        }

        context->m_interface->create_interface(config);

#if !defined(JE_GL_USE_EGL_INSTEAD_GLFW) && defined(JE_ENABLE_GL330_GAPI)
        if (auto glew_init_result = glewInit(); glew_init_result != GLEW_OK)
            jeecs::debug::logfatal("Failed to init glew: %s.", glewGetErrorString(glew_init_result));
#endif

#ifdef JE_ENABLE_GL330_GAPI
#if JE4_CURRENT_PLATFORM != JE4_PLATFORM_MACOS && !defined(NDEBUG)
        if (glDebugMessageCallback != nullptr && glDebugMessageControl != nullptr)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
#endif
#endif
        glEnable(GL_DEPTH_TEST);
#if defined(JE_ENABLE_GL330_GAPI) && defined(GL_FRAMEBUFFER_SRGB)
        // Desktop GL: shaders render linear; the default framebuffer performs
        // the final linear-to-sRGB encode. Requires an sRGB-capable window.
        glEnable(GL_FRAMEBUFFER_SRGB);
#endif
        jegui_init_gl330(
            gthread,
            [](jegl_context*, jegl_texture_2d* res)
            {
                jegl_gl3_texture* texture_instance =
                    static_cast<jegl_gl3_texture*>(res->m_handle.m_ptr);

                return (uint64_t)texture_instance->m_texture_id;
            },
            [](jegl_context* jctx, jegl_shader* res)
            {
                jegl_gl3_shader* shader_instance =
                    static_cast<jegl_gl3_shader*>(res->m_handle.m_ptr);

                assert(shader_instance != nullptr);

                const auto& blob_shared = shader_instance->m_shared_blob_data;
                for (size_t i = 0; i < blob_shared->m_sampler_count; i += 1)
                {
                    auto& sampler = blob_shared->m_samplers[i];
                    for (auto id : sampler.m_passes)
                    {
                        glBindSampler(id, sampler.m_sampler);
                    }
                }
            },
            context->m_interface->interface_handle(),
            reboot);

        return context;
    }

    jegl_update_action gl_pre_update(je_GraphicImplContext ctx)
    {
        jegl_gl3_context* context = static_cast<jegl_gl3_context*>(ctx);

        switch (context->m_interface->update())
        {
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_update_action::JEGL_UPDATE_STOP;
            goto _label_jegl_gl3_normal_job;
        case basic_interface::update_result::PAUSE:
            return jegl_update_action::JEGL_UPDATE_SKIP;
        case basic_interface::update_result::RESIZE:
        {
            int width, height;
            je_io_get_window_size(&width, &height);

            context->RESOLUTION_WIDTH = (size_t)width;
            context->RESOLUTION_HEIGHT = (size_t)height;
        }
        /*fallthrough*/
        [[fallthrough]];
        case basic_interface::update_result::NORMAL:
        _label_jegl_gl3_normal_job:
            context->m_interface->swap_for_opengl();

            // Reset current_active_shader_may_null to make sure bind correctly.
            context->current_active_shader_may_null = nullptr;
            return jegl_update_action::JEGL_UPDATE_CONTINUE;
        default:
            abort();
        }
    }

    jegl_update_action gl_commit_update(
        je_GraphicImplContext, jegl_update_action)
    {
        // 回到默认帧缓冲区
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        jegui_update_gl330();

        // 将绘制命令异步地提交给GPU
        glFlush();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    void gl_pre_shutdown(jegl_context*, je_GraphicImplContext, bool)
    {}
    void gl_shutdown(jegl_context*, je_GraphicImplContext userdata, bool reboot)
    {
        jegl_gl3_context* context = static_cast<jegl_gl3_context*>(userdata);

        if (!reboot)
            jeecs::debug::log("Graphic thread (OpenGL3) shutdown!");

        jegui_shutdown_gl330(reboot);

        context->m_interface->shutdown(reboot);
        delete context;
    }
        jegl_resource_blob shader_create_resource_blob(
        je_GraphicImplContext ctx, jegl_shader* resource)
    {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint shader_program = glCreateProgram();

#ifdef JE_ENABLE_GL330_GAPI
        glShaderSource(vs, 1, &resource->m_vertex_glsl_src, NULL);
        glShaderSource(fs, 1, &resource->m_fragment_glsl_src, NULL);
#else
        glShaderSource(vs, 1, &resource->m_vertex_glsles_src, NULL);
        glShaderSource(fs, 1, &resource->m_fragment_glsles_src, NULL);
#endif
        glCompileShader(vs);
        glCompileShader(fs);

        // Check this program is acceptable?
        GLint compile_result;
        GLint errmsg_len;
        GLint errmsg_written_len;

        bool shader_program_has_error = false;
        std::string error_informations;

        glGetShaderiv(vs, GL_COMPILE_STATUS, &compile_result);
        if (compile_result != GL_TRUE)
        {
            shader_program_has_error = true;
            glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(vs, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                error_informations =
                    error_informations
                    + "In vertex shader: \n"
                    + errmsg_buf.data();
            }
        }

        glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_result);
        if (compile_result != GL_TRUE)
        {
            shader_program_has_error = true;
            glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(fs, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                error_informations =
                    error_informations
                    + "In fragment shader: \n"
                    + errmsg_buf.data();
            }
        }

        if (!shader_program_has_error)
        {
            GLint link_result;
            GLint errmsg_len;
            GLint errmsg_written_len;

            glAttachShader(shader_program, vs);
            glAttachShader(shader_program, fs);
            glLinkProgram(shader_program);

            glGetProgramiv(shader_program, GL_LINK_STATUS, &link_result);
            if (link_result != GL_TRUE)
            {
                shader_program_has_error = true;
                glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &errmsg_len);

                std::vector<char> errmsg_buf(errmsg_len + 1, '\0');
                if (errmsg_len > 0)
                    glGetProgramInfoLog(shader_program, errmsg_len, &errmsg_written_len, errmsg_buf.data());

                error_informations =
                    error_informations
                    + "In linking shader program: \n"
                    + errmsg_buf.data();
            }
        }

        glDeleteShader(vs);
        glDeleteShader(fs);

        if (shader_program_has_error)
        {
            glDeleteProgram(shader_program);

            jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                resource, error_informations.c_str());
            return nullptr;
        }
        else
        {
            auto* shared_blob =
                new jegl3_shader_blob::jegl3_shader_blob_shared(
                    shader_program,
                    (uint32_t)resource->m_sampler_count);

            for (size_t i = 0; i < resource->m_sampler_count; ++i)
            {
                auto& sampler_config = resource->m_sampler_methods[i];
                auto& samplers = shared_blob->m_samplers[i];

                switch (sampler_config.m_mag)
                {
                case jegl_shader::fliter_mode::LINEAR:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    break;
                case jegl_shader::fliter_mode::NEAREST:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    break;
                default:
                    abort();
                }
                switch (sampler_config.m_min)
                {
                case jegl_shader::fliter_mode::LINEAR:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    break;
                case jegl_shader::fliter_mode::NEAREST:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    break;
                default:
                    abort();
                }
                switch (sampler_config.m_uwrap)
                {
                case jegl_shader::wrap_mode::CLAMP:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    break;
                case jegl_shader::wrap_mode::REPEAT:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    break;
                default:
                    abort();
                }
                switch (sampler_config.m_vwrap)
                {
                case jegl_shader::wrap_mode::CLAMP:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    break;
                case jegl_shader::wrap_mode::REPEAT:
                    glSamplerParameteri(samplers.m_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    break;
                default:
                    abort();
                }

                samplers.m_passes.resize((size_t)sampler_config.m_pass_id_count);
                for (uint64_t i = 0; i < sampler_config.m_pass_id_count; i += 1)
                    samplers.m_passes.at(i) = sampler_config.m_pass_ids[i];
            }

            uint32_t last_elem_end_place = 0;
            auto* uniforms = resource->m_custom_uniforms;
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
                case jegl_shader::uniform_type::COLOR:
                    unit_size = 16;
                    allign_base = 16;
                    break;
                case jegl_shader::uniform_type::FLOAT2X2:
                    unit_size = 16;
                    allign_base = 8;
                    break;
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
                    last_elem_end_place = jeecs::basic::allign_size(last_elem_end_place, allign_base);
                    shared_blob->m_uniform_locations[uniforms->m_name] = last_elem_end_place;
                    last_elem_end_place += unit_size;
                }
                uniforms = uniforms->m_next;
            }

            switch (resource->m_depth_test)
            {
            case jegl_shader::depth_test_method::NEVER:
                shared_blob->m_depth_test_method = GL_NEVER;
                break;
            case jegl_shader::depth_test_method::LESS:
                shared_blob->m_depth_test_method = GL_LESS;
                break;
            case jegl_shader::depth_test_method::EQUAL:
                shared_blob->m_depth_test_method = GL_EQUAL;
                break;
            case jegl_shader::depth_test_method::LESS_EQUAL:
                shared_blob->m_depth_test_method = GL_LEQUAL;
                break;
            case jegl_shader::depth_test_method::GREATER:
                shared_blob->m_depth_test_method = GL_GREATER;
                break;
            case jegl_shader::depth_test_method::NOT_EQUAL:
                shared_blob->m_depth_test_method = GL_NOTEQUAL;
                break;
            case jegl_shader::depth_test_method::GREATER_EQUAL:
                shared_blob->m_depth_test_method = GL_GEQUAL;
                break;
            case jegl_shader::depth_test_method::ALWAYS:
                shared_blob->m_depth_test_method = GL_ALWAYS;
                break;
            default:
                jeecs::debug::logerr("Invalid depth test method.");
                break;
            }

            switch (resource->m_depth_mask)
            {
            case jegl_shader::depth_mask_method::DISABLE:
                shared_blob->m_depth_write_mask = GL_FALSE;
                break;
            case jegl_shader::depth_mask_method::ENABLE:
                shared_blob->m_depth_write_mask = GL_TRUE;
                break;
            default:
                jeecs::debug::logerr("Invalid depth write mask method.");
                break;
            }

            switch (resource->m_blend_equation)
            {
            case jegl_shader::blend_equation::DISABLED:
                shared_blob->m_blend_equation = GL_INVALID_ENUM;
                break;
            case jegl_shader::blend_equation::ADD:
                shared_blob->m_blend_equation = GL_FUNC_ADD;
                break;
            case jegl_shader::blend_equation::SUBTRACT:
                shared_blob->m_blend_equation = GL_FUNC_SUBTRACT;
                break;
            case jegl_shader::blend_equation::REVERSE_SUBTRACT:
                shared_blob->m_blend_equation = GL_FUNC_REVERSE_SUBTRACT;
                break;
            case jegl_shader::blend_equation::MIN:
                shared_blob->m_blend_equation = GL_MIN;
                break;
            case jegl_shader::blend_equation::MAX:
                shared_blob->m_blend_equation = GL_MAX;
                break;
            default:
                jeecs::debug::logerr("Invalid blend equation method.");
                break;
            }

            auto cast_to_gl_blend_factor =
                [](jegl_shader::blend_method mode)
                {
                    switch (mode)
                    {
                    case jegl_shader::blend_method::ZERO: return GL_ZERO;
                    case jegl_shader::blend_method::ONE: return GL_ONE;
                    case jegl_shader::blend_method::SRC_COLOR: return GL_SRC_COLOR;
                    case jegl_shader::blend_method::SRC_ALPHA: return GL_SRC_ALPHA;
                    case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
                    case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR: return  GL_ONE_MINUS_SRC_COLOR;
                    case jegl_shader::blend_method::DST_COLOR: return GL_DST_COLOR;
                    case jegl_shader::blend_method::DST_ALPHA: return GL_DST_ALPHA;
                    case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
                    case jegl_shader::blend_method::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
                    default:
                        jeecs::debug::logerr("Invalid blend src method.");
                        return GL_ONE;
                    }
                };

            shared_blob->m_blend_src_mode =
                cast_to_gl_blend_factor(resource->m_blend_src_mode);
            shared_blob->m_blend_dst_mode =
                cast_to_gl_blend_factor(resource->m_blend_dst_mode);

            switch (resource->m_cull_mode)
            {
            case jegl_shader::cull_mode::NONE:
                shared_blob->m_cull_face_method = GL_NONE;
                break;
            case jegl_shader::cull_mode::FRONT:
                shared_blob->m_cull_face_method = GL_FRONT;
                break;
            case jegl_shader::cull_mode::BACK:
                shared_blob->m_cull_face_method = GL_BACK;
                break;
            default:
                jeecs::debug::logerr("Invalid cull face method.");
                break;
            }

            return new jegl3_shader_blob(
                jeecs::basic::resource<jegl3_shader_blob::jegl3_shader_blob_shared>(
                    shared_blob));
        }
    }
    void shader_close_resource_blob(
        je_GraphicImplContext ctx, jegl_resource_blob blob)
    {
        delete static_cast<jegl3_shader_blob*>(blob);
    }

    template<typename TexT>
    jegl_resource_blob texture_create_resource_blob(
        je_GraphicImplContext ctx, TexT* resource)
    {
        return nullptr;
    }
    void texture_close_resource_blob(
        je_GraphicImplContext ctx, jegl_resource_blob blob)
    {}

    jegl_resource_blob vertex_create_resource_blob(
        je_GraphicImplContext ctx, jegl_vertex* resource)
    {
        return nullptr;
    }
    void vertex_close_resource_blob(
        je_GraphicImplContext ctx, jegl_resource_blob blob)
    {}

    void shader_init(
        je_GraphicImplContext ctx,
        jegl_resource_blob blob,
        jegl_shader* resource)
    {
        resource->m_handle.m_ptr = nullptr;
        if (blob != nullptr)
        {
            auto* shader_blob = static_cast<jegl3_shader_blob*>(blob);

            auto shader_program = shader_blob->m_shared_blob_data->m_shader_program_instance;
            glUseProgram(shader_program);

            // The engine reserves UBO binding 0 for the per-draw uniform block.
            // Bind it explicitly instead of relying on the linker-assigned slot.
            const GLuint shader_uniform_block =
                glGetUniformBlockIndex(shader_program, "SHADER_UNIFORM");
            if (GL_INVALID_INDEX != shader_uniform_block)
                glUniformBlockBinding(shader_program, shader_uniform_block, 0);

            // NOTE: JoyEngine 约定 ubo 0 为着色器自定义的 uniform 变量
            //  之前 OpenGL 版本的实现中，直接储存在着色器本身的 uniform 变量里，但是现在因为旧的着色器代码的
            //  生成被弃用，不再这么做，而是模拟 DX 和 Vulkan 的储存方式
            // 
            // NOTE: Uniform block 不需要额外的初始化，之后自然会被更新

            jegl_gl3_shader* shader_instance =
                new jegl_gl3_shader(shader_blob);

            resource->m_handle.m_ptr = shader_instance;

            auto shared_blob_data = shader_instance->m_shared_blob_data.get();
            auto& builtin_uniforms = resource->m_builtin_uniforms;

            builtin_uniforms.m_builtin_uniform_ndc_scale = shared_blob_data->get_built_in_location("JE_NDC_SCALE");
            builtin_uniforms.m_builtin_uniform_m = shared_blob_data->get_built_in_location("JE_M");
            builtin_uniforms.m_builtin_uniform_mv = shared_blob_data->get_built_in_location("JE_MV");
            builtin_uniforms.m_builtin_uniform_mvp = shared_blob_data->get_built_in_location("JE_MVP");

            builtin_uniforms.m_builtin_uniform_tiling = shared_blob_data->get_built_in_location("JE_UV_TILING");
            builtin_uniforms.m_builtin_uniform_offset = shared_blob_data->get_built_in_location("JE_UV_OFFSET");

            builtin_uniforms.m_builtin_uniform_light2d_resolution =
                shared_blob_data->get_built_in_location("JE_LIGHT2D_RESOLUTION");
            builtin_uniforms.m_builtin_uniform_light2d_decay =
                shared_blob_data->get_built_in_location("JE_LIGHT2D_DECAY");
            builtin_uniforms.m_builtin_uniform_shadow_ellipse =
                shared_blob_data->get_built_in_location("JE_SHADOW_ELLIPSE");

            // ATTENTION: 注意，以下参数特殊shader可能挪作他用
            builtin_uniforms.m_builtin_uniform_local_scale = shared_blob_data->get_built_in_location("JE_LOCAL_SCALE");
            builtin_uniforms.m_builtin_uniform_color = shared_blob_data->get_built_in_location("JE_COLOR");

            // "je4_gl_sampler_%zu", see _jegl_regenerate_and_alloc_glsl_from_spir_v_combined
            char gl_sampler_name[64];

            auto* uniform_var = resource->m_custom_uniforms;
            while (uniform_var)
            {
                if (uniform_var->m_uniform_type == jegl_shader::uniform_type::TEXTURE)
                {
                    uniform_var->m_index = jeecs::graphic::INVALID_UNIFORM_LOCATION;

                    auto count = snprintf(
                        gl_sampler_name,
                        sizeof(gl_sampler_name),
                        "je4_gl_sampler_%zu",
                        (size_t)uniform_var->m_value.m_int);

                    (void)count;
                    assert(count > 0 && count < (int)sizeof(gl_sampler_name));

                    const auto location = glGetUniformLocation(shader_program, gl_sampler_name);
                    if (location != -1)
                        glUniform1i(location, (GLint)uniform_var->m_value.m_int);
                }
                else
                {
                    uniform_var->m_index =
                        shared_blob_data->get_built_in_location(uniform_var->m_name);
                }

                uniform_var = uniform_var->m_next;
            }

            auto* uniform_block = resource->m_custom_uniform_blocks;
            while (uniform_block)
            {
                const GLuint uniform_block_loc = glGetUniformBlockIndex(shader_program, uniform_block->m_name);
                if (GL_INVALID_INDEX != uniform_block_loc)
                    glUniformBlockBinding(shader_program, uniform_block_loc, 1 + (GLuint)uniform_block->m_specify_binding_place);

                uniform_block = uniform_block->m_next;
            }
        }
    }
    void shader_update(
        je_GraphicImplContext,
        jegl_shader* resource)
    {}
    void shader_close(
        je_GraphicImplContext,
        jegl_shader* resource)
    {
        jegl_gl3_shader* shader_instance =
            static_cast<jegl_gl3_shader*>(resource->m_handle.m_ptr);

        if (shader_instance != nullptr)
            delete shader_instance;
    }

    // 根据纹理颜色格式选取 GL 的内部格式与源数据格式，
    // 返回 false 表示无法识别的格式
    static bool _gl3_pick_color_formats(
        jegl_texture_common::format fmt, bool is_16bit,
        GLint& texture_src_format, GLint& texture_aim_format)
    {
        const bool is_srgb =
            0 != (fmt & jegl_texture_common::format::SRGB);
        switch (fmt & jegl_texture_common::format::COLOR_DEPTH_MASK)
        {
        case jegl_texture_common::format::MONO:
            texture_src_format = GL_RED;
            texture_aim_format = is_16bit ? GL_R16F : GL_R8;
            return true;
        case jegl_texture_common::format::RGBA:
            texture_src_format = GL_RGBA;
            texture_aim_format = is_16bit
                ? GL_RGBA16F
                : (is_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA);
            return true;
        default:
            jeecs::debug::logerr("Unknown texture raw-data format.");
            return false;
        }
    }

    void texture_2d_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_texture_2d* resource)
    {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        const bool is_depth = 0 != (resource->m_format & jegl_texture_common::format::DEPTH);

        if (is_depth)
        {
            if (is_16bit)
                jeecs::debug::logerr("Depth texture cannot use 16bit.");

            glTexImage2D(GL_TEXTURE_2D, 0,
#if defined(JE_ENABLE_WEBGL20_GAPI)
                GL_DEPTH_COMPONENT24,
#else
                GL_DEPTH_COMPONENT,
#endif
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                0,
                GL_DEPTH_COMPONENT,
                GL_UNSIGNED_INT,
                NULL);
        }
        else
        {
            GLint texture_aim_format = GL_RGBA;
            GLint texture_src_format = GL_RGBA;
            if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
                return;

            glTexImage2D(GL_TEXTURE_2D,
                0, texture_aim_format,
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                0, texture_src_format,
                is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                resource->m_pixels);
        }

        resource->m_handle.m_ptr = new jegl_gl3_texture(
            texture, GL_TEXTURE_2D, resource->m_format, resource->m_width, resource->m_height);
    }

    void texture_3d_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_texture_3d* resource)
    {
        if (0 != (resource->m_format & jegl_texture_common::format::DEPTH))
        {
            jeecs::debug::logerr("3D depth texture is not supported.");
            return;
        }

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;
        if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
            return;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);

        glTexImage3D(GL_TEXTURE_3D,
            0, texture_aim_format,
            (GLsizei)resource->m_width,
            (GLsizei)resource->m_height,
            (GLsizei)resource->m_depth,
            0, texture_src_format,
            is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
            resource->m_pixels);

        resource->m_handle.m_ptr = new jegl_gl3_texture(
            texture, GL_TEXTURE_3D, resource->m_format, resource->m_width, resource->m_height);
    }

    void texture_array_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_texture_array* resource)
    {
        if (0 != (resource->m_format & jegl_texture_common::format::DEPTH))
        {
            jeecs::debug::logerr("Texture array of depth format is not supported.");
            return;
        }

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;
        if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
            return;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

        glTexImage3D(GL_TEXTURE_2D_ARRAY,
            0, texture_aim_format,
            (GLsizei)resource->m_width,
            (GLsizei)resource->m_height,
            (GLsizei)resource->m_layers,
            0, texture_src_format,
            is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
            resource->m_pixels);

        resource->m_handle.m_ptr = new jegl_gl3_texture(
            texture, GL_TEXTURE_2D_ARRAY, resource->m_format, resource->m_width, resource->m_height);
    }

    void texture_cube_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_texture_cube* resource)
    {
        const GLenum jegl_texture_cube_map_ways[] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        const bool is_depth = 0 != (resource->m_format & jegl_texture_common::format::DEPTH);
        const size_t face_pixels =
            resource->m_width * resource->m_height
            * (resource->m_format & jegl_texture_common::format::COLOR_DEPTH_MASK);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

        for (size_t face_idx = 0; face_idx < 6; ++face_idx)
        {
            const void* face_data = resource->m_pixels != nullptr
                ? resource->m_pixels + face_idx * face_pixels
                : nullptr;

            if (is_depth)
            {
                if (is_16bit)
                    jeecs::debug::logerr("Depth texture cannot use 16bit.");

                glTexImage2D(jegl_texture_cube_map_ways[face_idx], 0,
#if defined(JE_ENABLE_WEBGL20_GAPI)
                    GL_DEPTH_COMPONENT24,
#else
                    GL_DEPTH_COMPONENT,
#endif
                    (GLsizei)resource->m_width,
                    (GLsizei)resource->m_height,
                    0,
                    GL_DEPTH_COMPONENT,
                    GL_UNSIGNED_INT,
                    NULL);
            }
            else
            {
                GLint texture_aim_format = GL_RGBA;
                GLint texture_src_format = GL_RGBA;
                if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
                    return;

                glTexImage2D(jegl_texture_cube_map_ways[face_idx], 0, texture_aim_format,
                    (GLsizei)resource->m_width,
                    (GLsizei)resource->m_height,
                    0, texture_src_format,
                    is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                    const_cast<void*>(face_data));
            }
        }

        resource->m_handle.m_ptr = new jegl_gl3_texture(
            texture, GL_TEXTURE_CUBE_MAP, resource->m_format, resource->m_width, resource->m_height);
    }

    void texture_msaa_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_texture_msaa* resource)
    {
#if defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
        (void)resource;
        jeecs::debug::logerr("MSAA texture is not supported by OpenGLES3/WebGL2.");
#else
        const bool is_depth = 0 != (resource->m_format & jegl_texture_common::format::DEPTH);
        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);

        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        if (is_depth)
            texture_aim_format = GL_DEPTH_COMPONENT;
        else if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
            return;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture);

        glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE,
            (GLsizei)resource->m_samples,
            texture_aim_format,
            (GLsizei)resource->m_width,
            (GLsizei)resource->m_height,
            GL_TRUE);

        resource->m_handle.m_ptr = new jegl_gl3_texture(
            texture, GL_TEXTURE_2D_MULTISAMPLE, resource->m_format, resource->m_width, resource->m_height);
#endif
    }

    void texture_2d_update(
        je_GraphicImplContext,
        jegl_texture_2d* resource)
    {
        jegl_gl3_texture* texture_instance =
            static_cast<jegl_gl3_texture*>(resource->m_handle.m_ptr);

        // Update texture's pixels, only normal pixel data will be updated.
        // NOTE: 深度纹理不支持更新
        if (0 != (resource->m_format & jegl_texture_common::format::DEPTH))
        {
            jeecs::debug::logerr("Depth texture update is not supported.");
            return;
        }

        glBindTexture(GL_TEXTURE_2D, texture_instance->m_texture_id);

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
            return;

        const bool same_size =
            texture_instance->m_width == resource->m_width &&
            texture_instance->m_height == resource->m_height;

        if (same_size)
        {
            // 原地更新，避免重新分配纹理存储
            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                texture_src_format,
                is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                resource->m_pixels);
        }
        else
        {
            // 尺寸发生变化，需要重新分配纹理存储
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                texture_aim_format,
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                0,
                texture_src_format,
                is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                resource->m_pixels);

            texture_instance->m_width = resource->m_width;
            texture_instance->m_height = resource->m_height;
        }
    }

    template<typename TexT>
    static void _gl3_update_texture_3d_like(
        TexT* resource, GLint target, GLsizei depth_or_layers)
    {
        jegl_gl3_texture* texture_instance =
            static_cast<jegl_gl3_texture*>(resource->m_handle.m_ptr);

        if (0 != (resource->m_format & jegl_texture_common::format::DEPTH))
        {
            jeecs::debug::logerr("Depth texture update is not supported.");
            return;
        }

        glBindTexture(target, texture_instance->m_texture_id);

        const bool is_16bit = 0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        if (!_gl3_pick_color_formats(resource->m_format, is_16bit, texture_src_format, texture_aim_format))
            return;

        const bool same_size =
            texture_instance->m_width == resource->m_width &&
            texture_instance->m_height == resource->m_height;

        if (same_size)
        {
            glTexSubImage3D(
                target,
                0,
                0, 0, 0,
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                depth_or_layers,
                texture_src_format,
                is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                resource->m_pixels);
        }
        else
        {
            glTexImage3D(
                target,
                0,
                texture_aim_format,
                (GLsizei)resource->m_width,
                (GLsizei)resource->m_height,
                depth_or_layers,
                0,
                texture_src_format,
                is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                resource->m_pixels);

            texture_instance->m_width = resource->m_width;
            texture_instance->m_height = resource->m_height;
        }
    }

    void texture_3d_update(
        je_GraphicImplContext,
        jegl_texture_3d* resource)
    {
        _gl3_update_texture_3d_like(resource, GL_TEXTURE_3D, (GLsizei)resource->m_depth);
    }

    void texture_array_update(
        je_GraphicImplContext,
        jegl_texture_array* resource)
    {
        _gl3_update_texture_3d_like(resource, GL_TEXTURE_2D_ARRAY, (GLsizei)resource->m_layers);
    }

    void texture_cube_update(
        je_GraphicImplContext,
        jegl_texture_cube* resource)
    {
        (void)resource;
        // NOTE: Cube 纹理不支持更新
        jeecs::debug::logerr("Cube texture update is not supported.");
    }

    void texture_msaa_update(
        je_GraphicImplContext,
        jegl_texture_msaa* resource)
    {
        (void)resource;
        // NOTE: MSAA 纹理只存在于 GPU 侧，不支持 CPU 侧像素更新
        jeecs::debug::logerr("MSAA texture update is not supported.");
    }

    template<typename TexT>
    void texture_close(
        je_GraphicImplContext,
        TexT* resource)
    {
        delete static_cast<jegl_gl3_texture*>(resource->m_handle.m_ptr);
    }

    void vertex_init(
        je_GraphicImplContext,
        jegl_resource_blob,
        jegl_vertex* resource)
    {
        GLuint vao, vbo, ebo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            resource->m_vertex_length,
            resource->m_vertexs,
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        // 索引缓冲按分配容量上传（m_index_count 可能被
        // jegl_update_vertex_buffer 收缩为激活数量，绘制数量另存于
        // m_pointcount）
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            resource->m_index_capacity * sizeof(uint32_t),
            resource->m_indices,
            GL_STATIC_DRAW);

        size_t offset = 0;
        for (unsigned int i = 0; i < (unsigned int)resource->m_format_count; i++)
        {
            size_t format_size;

            glEnableVertexAttribArray(i);

            switch (resource->m_formats[i].m_type)
            {
            case jegl_vertex::data_type::FLOAT32:
                format_size = sizeof(float);
                glVertexAttribPointer(i, (GLint)resource->m_formats[i].m_count,
                    GL_FLOAT, GL_FALSE,
                    (GLsizei)(resource->m_data_size_per_point),
                    (void*)offset);
                break;
            case jegl_vertex::data_type::INT32:
                format_size = sizeof(int);
                glVertexAttribIPointer(i, (GLint)resource->m_formats[i].m_count,
                    GL_INT,
                    (GLsizei)(resource->m_data_size_per_point),
                    (void*)offset);
                break;
            default:
                jeecs::debug::logfatal("Bad vertex data type.");
                break;
            }

            offset += format_size * resource->m_formats[i].m_count;
        }

        const static GLenum DRAW_METHODS[] = {
            GL_LINE_STRIP,
            GL_TRIANGLES,
            GL_TRIANGLE_STRIP,
        };

        auto* vertex_data = new jegl3_vertex_data;
        vertex_data->m_vao = vao;
        vertex_data->m_vbo = vbo;
        vertex_data->m_ebo = ebo;
        vertex_data->m_method = DRAW_METHODS[(size_t)resource->m_type];
        vertex_data->m_pointcount = (GLsizei)resource->m_index_count;

        resource->m_handle.m_ptr = vertex_data;
    }
    void vertex_update(
        je_GraphicImplContext,
        jegl_vertex* resource)
    {
        jegl3_vertex_data* vertex_data =
            static_cast<jegl3_vertex_data*>(resource->m_handle.m_ptr);

        // 整体重传顶点数据（索引缓冲内容不变），并同步激活索引数量。
        // 每次 glBufferData 重指定：首次将 STATIC 升级为 DYNAMIC 用途（对齐
        // DX11 后端的 IMMUTABLE→DYNAMIC 懒升级）；之后每次重指定同时孤儿化
        // 旧数据存储，等价于 Map(WRITE_DISCARD)，避免 GPU 仍在读取旧数据时
        // glBufferSubData 阻塞图形线程
        glBindBuffer(GL_ARRAY_BUFFER, vertex_data->m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            resource->m_vertex_length,
            resource->m_vertexs,
            GL_DYNAMIC_DRAW);

        vertex_data->m_pointcount = (GLsizei)resource->m_index_count;
    }
    void vertex_close(
        je_GraphicImplContext,
        jegl_vertex* resource)
    {
        delete static_cast<jegl3_vertex_data*>(resource->m_handle.m_ptr);
    }

    void ubuffer_init(
        je_GraphicImplContext,
        jegl_uniform_buffer* resource)
    {
        resource->m_handle.m_ptr = new jegl_gl3_uniformbuf(resource);
    }
    void ubuffer_update(
        je_GraphicImplContext,
        jegl_uniform_buffer* resource)
    {
        jegl_gl3_uniformbuf* ubuf =
            static_cast<jegl_gl3_uniformbuf*>(resource->m_handle.m_ptr);

        assert(resource->m_update_length != 0);

        glBindBuffer(GL_UNIFORM_BUFFER, ubuf->m_uniform_buffer_object);

        assert(resource->m_update_length != 0);
        glBufferSubData(GL_UNIFORM_BUFFER,
            resource->m_update_begin_offset,
            resource->m_update_length,
            resource->m_buffer + resource->m_update_begin_offset);
    }
    void ubuffer_close(
        je_GraphicImplContext,
        jegl_uniform_buffer* resource)
    {
        delete static_cast<jegl_gl3_uniformbuf*>(resource->m_handle.m_ptr);
    }

    void framebuffer_init(
        je_GraphicImplContext,
        jegl_frame_buffer* resource)
    {
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        bool already_has_attached_depth = false;

        GLenum attachment = GL_COLOR_ATTACHMENT0;

        jeecs::basic::resource<jeecs::graphic::texture2d>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture2d> *>(
                resource->m_output_attachments);

        for (size_t i = 0; i < resource->m_attachment_count; ++i)
        {
            jegl_texture_2d* frame_texture = attachments[i]->resource();

            // Ensure the attachment texture has a lazy-initialized GL object
            // before glFramebufferTexture2D.
            (void)_jegl_prepare_texture_2d_resource(frame_texture);

            GLenum using_attachment = attachment;
            GLenum buffer_texture_type = GL_TEXTURE_2D;

            if (0 != (frame_texture->m_format & jegl_texture_common::format::DEPTH))
            {
                if (already_has_attached_depth)
                    jeecs::debug::logerr("Framebuffer(%p) attach depth buffer repeatedly.", resource);
                already_has_attached_depth = true;
                using_attachment = GL_DEPTH_ATTACHMENT;
            }
            else
                ++attachment;

            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                using_attachment,
                buffer_texture_type,
                static_cast<jegl_gl3_texture*>(frame_texture->m_handle.m_ptr)->m_texture_id,
                0);
        }
        std::vector<GLuint> glattachments;
        for (GLenum attachment_index = GL_COLOR_ATTACHMENT0; attachment_index < attachment; ++attachment_index)
            glattachments.push_back(attachment_index);

        if (glattachments.empty())
        {
            // 深度纹理没有颜色附件时，需要显式设置 draw buffer 为 GL_NONE，
            // 否则默认的 GL_COLOR_ATTACHMENT0 因无对应附件会导致 framebuffer 不完整。
            // 使用 glDrawBuffers 而非 glDrawBuffer，因为 GLES3/WebGL2 仅有 glDrawBuffers。
            const GLenum none_buf = GL_NONE;
            glDrawBuffers(1, &none_buf);
        }
        else
            glDrawBuffers((GLsizei)glattachments.size(), glattachments.data());

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            jeecs::debug::logerr("Framebuffer(%p) not complete, status: %d.", resource, (int)status);

        resource->m_handle.m_ptr = new jegl_gl3_framebuf(
            resource->m_width,
            resource->m_height,
            fbo);
    }
    void framebuffer_update(
        je_GraphicImplContext,
        jegl_frame_buffer* resource)
    {}
    void framebuffer_close(
        je_GraphicImplContext,
        jegl_frame_buffer* resource)
    {
        delete static_cast<jegl_gl3_framebuf*>(resource->m_handle.m_ptr);
    }

    void _gl_update_depth_test_method(jegl_gl3_context* ctx, GLenum mode)
    {
        if (ctx->ACTIVE_DEPTH_MODE != mode)
        {
            ctx->ACTIVE_DEPTH_MODE = mode;
            glDepthFunc(mode);
        }
    }
    void _gl_update_depth_mask_method(jegl_gl3_context* ctx, GLboolean mode)
    {
        const GLenum mode_enum = static_cast<GLenum>(mode);

        if (ctx->ACTIVE_MASK_MODE != mode_enum)
        {
            ctx->ACTIVE_MASK_MODE = mode_enum;
            glDepthMask(mode);
        }
    }
    bool _gl_update_blend_equation_method(
        jegl_gl3_context* ctx, GLenum equation)
    {
        if (ctx->ACTIVE_BLEND_EQUATION != equation)
        {
            ctx->ACTIVE_BLEND_EQUATION = equation;

            if (GL_INVALID_ENUM == equation)
            {
                glDisable(GL_BLEND);
                return false;
            }
            else
            {
                glEnable(GL_BLEND);
                glBlendEquation(equation);
                return true;
            }
        }
        return equation != GL_INVALID_ENUM;
    }
    void _gl_update_blend_mode_method(
        jegl_gl3_context* ctx, GLenum src_mode, GLenum dst_mode)
    {
        if (ctx->ACTIVE_BLEND_SRC_MODE != src_mode || ctx->ACTIVE_BLEND_DST_MODE != dst_mode)
        {
            ctx->ACTIVE_BLEND_SRC_MODE = src_mode;
            ctx->ACTIVE_BLEND_DST_MODE = dst_mode;

            glBlendFunc(src_mode, dst_mode);
        }
    }
    void _gl_update_cull_mode_method(jegl_gl3_context* ctx, GLenum mode)
    {
        if (ctx->ACTIVE_CULL_MODE != mode)
        {
            ctx->ACTIVE_CULL_MODE = mode;

            if (mode == GL_NONE)
                glDisable(GL_CULL_FACE);
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(mode);
            }
        }
    }

    void gl_bind_uniform_buffer(je_GraphicImplContext, jegl_uniform_buffer* uniformbuf)
    {
        jegl_gl3_uniformbuf* ubuf =
            static_cast<jegl_gl3_uniformbuf*>(uniformbuf->m_handle.m_ptr);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            ubuf->m_binding_place,
            ubuf->m_uniform_buffer_object,
            0,
            ubuf->m_uniform_buffer_size);
    }

    template<typename TexT>
    void gl_bind_texture(je_GraphicImplContext ctx, TexT* texture, size_t pass)
    {
        jegl_gl3_context* context = static_cast<jegl_gl3_context*>(ctx);
        jegl_gl3_texture* texture_instance =
            static_cast<jegl_gl3_texture*>(texture->m_handle.m_ptr);

        context->bind_texture_pass_impl(
            (GLint)pass,
            texture_instance->m_target,
            texture_instance->m_texture_id);
    }

            /////////////////////////////////////////////////////////////////////////
    // Render API (v2)
    /////////////////////////////////////////////////////////////////////////

    struct jegl_gl3_render_sampler
    {
        GLuint m_sampler;
    };

    GLenum gl_render_depth_func(jegl_shader::depth_test_method method)
    {
        switch (method)
        {
        case jegl_shader::depth_test_method::NEVER: return GL_NEVER;
        case jegl_shader::depth_test_method::LESS: return GL_LESS;
        case jegl_shader::depth_test_method::EQUAL: return GL_EQUAL;
        case jegl_shader::depth_test_method::LESS_EQUAL: return GL_LEQUAL;
        case jegl_shader::depth_test_method::GREATER: return GL_GREATER;
        case jegl_shader::depth_test_method::NOT_EQUAL: return GL_NOTEQUAL;
        case jegl_shader::depth_test_method::GREATER_EQUAL: return GL_GEQUAL;
        case jegl_shader::depth_test_method::ALWAYS: return GL_ALWAYS;
        default:
            assert(false);
            return GL_LESS;
        }
    }

    GLenum gl_render_blend_equation(jegl_shader::blend_equation equation)
    {
        switch (equation)
        {
        case jegl_shader::blend_equation::ADD: return GL_FUNC_ADD;
        case jegl_shader::blend_equation::SUBTRACT: return GL_FUNC_SUBTRACT;
        case jegl_shader::blend_equation::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
        case jegl_shader::blend_equation::MIN: return GL_MIN;
        case jegl_shader::blend_equation::MAX: return GL_MAX;
        default:
            assert(false);
            return GL_FUNC_ADD;
        }
    }

    GLenum gl_render_blend_factor(jegl_shader::blend_method method)
    {
        switch (method)
        {
        case jegl_shader::blend_method::ZERO: return GL_ZERO;
        case jegl_shader::blend_method::ONE: return GL_ONE;
        case jegl_shader::blend_method::SRC_COLOR: return GL_SRC_COLOR;
        case jegl_shader::blend_method::SRC_ALPHA: return GL_SRC_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case jegl_shader::blend_method::DST_COLOR: return GL_DST_COLOR;
        case jegl_shader::blend_method::DST_ALPHA: return GL_DST_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        default:
            assert(false);
            return GL_ONE;
        }
    }

    GLenum gl_render_cull_mode(jegl_shader::cull_mode mode)
    {
        switch (mode)
        {
        case jegl_shader::cull_mode::NONE: return GL_NONE;
        case jegl_shader::cull_mode::FRONT: return GL_FRONT;
        case jegl_shader::cull_mode::BACK: return GL_BACK;
        default:
            assert(false);
            return GL_NONE;
        }
    }

    void gl_render_pipeline_init(je_GraphicImplContext, jegl_pipeline*)
    {}

    void gl_render_pipeline_close(je_GraphicImplContext, jegl_pipeline*)
    {}

    void gl_render_pipeline_bind(
        je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        auto* shader_instance = static_cast<jegl_gl3_shader*>(
            pipeline->m_shader->m_handle.m_ptr);

        assert(shader_instance != nullptr);

#ifndef NDEBUG
        // The canonical layout must agree with the layout computed by the GL
        // backend from the same uniform list.
        auto check_builtin_offset =
            [](jegl_uniform_slot slot, uint32_t backend_offset)
            {
                if (slot.m_offset != JEGL_UNIFORM_SLOT_NONE
                    && backend_offset != jeecs::graphic::INVALID_UNIFORM_LOCATION)
                    assert(slot.m_offset == backend_offset);
            };

        const auto& builtin = pipeline->m_shader->m_builtin_uniforms;
        check_builtin_offset(pipeline->m_builtin_slots.m_model, builtin.m_builtin_uniform_m);
        check_builtin_offset(pipeline->m_builtin_slots.m_model_view, builtin.m_builtin_uniform_mv);
        check_builtin_offset(pipeline->m_builtin_slots.m_model_view_projection, builtin.m_builtin_uniform_mvp);
        check_builtin_offset(pipeline->m_builtin_slots.m_local_scale, builtin.m_builtin_uniform_local_scale);
        check_builtin_offset(pipeline->m_builtin_slots.m_uv_tiling, builtin.m_builtin_uniform_tiling);
        check_builtin_offset(pipeline->m_builtin_slots.m_uv_offset, builtin.m_builtin_uniform_offset);
        check_builtin_offset(pipeline->m_builtin_slots.m_color, builtin.m_builtin_uniform_color);
        check_builtin_offset(pipeline->m_builtin_slots.m_light2d_resolution, builtin.m_builtin_uniform_light2d_resolution);
        check_builtin_offset(pipeline->m_builtin_slots.m_light2d_decay, builtin.m_builtin_uniform_light2d_decay);
        check_builtin_offset(pipeline->m_builtin_slots.m_shadow_ellipse, builtin.m_builtin_uniform_shadow_ellipse);
#endif

        context->m_render_current_shader_resource = pipeline->m_shader;

        if (context->current_active_shader_may_null != shader_instance)
        {
            context->current_active_shader_may_null = shader_instance;
            glUseProgram(
                shader_instance->m_shared_blob_data->m_shader_program_instance);
        }

        _gl_bind_shader_samplers(context, shader_instance);

        const auto& state = pipeline->m_state;

        if (state.m_depth_test)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        _gl_update_depth_test_method(
            context, gl_render_depth_func(state.m_depth_func));
        _gl_update_depth_mask_method(
            context, state.m_depth_write ? GL_TRUE : GL_FALSE);

        if (state.m_blend)
        {
            glEnable(GL_BLEND);
            _gl_update_blend_equation_method(
                context,
                gl_render_blend_equation(state.m_blend_equation));
            _gl_update_blend_mode_method(
                context,
                gl_render_blend_factor(state.m_blend_src_color),
                gl_render_blend_factor(state.m_blend_dst_color));
            glBlendFuncSeparate(
                gl_render_blend_factor(state.m_blend_src_color),
                gl_render_blend_factor(state.m_blend_dst_color),
                gl_render_blend_factor(state.m_blend_src_alpha),
                gl_render_blend_factor(state.m_blend_dst_alpha));
        }
        else
            _gl_update_blend_equation_method(context, GL_INVALID_ENUM);

        _gl_update_cull_mode_method(
            context, gl_render_cull_mode(state.m_cull));

        glFrontFace(
            state.m_front_face == jegl_front_face::CLOCKWISE
            ? GL_CW
            : GL_CCW);
        glColorMask(
            (state.m_color_write_mask & 0x1) != 0,
            (state.m_color_write_mask & 0x2) != 0,
            (state.m_color_write_mask & 0x4) != 0,
            (state.m_color_write_mask & 0x8) != 0);
    }

    void gl_render_sampler_init(
        je_GraphicImplContext, jegl_sampler* sampler)
    {
        auto* native = new jegl_gl3_render_sampler{ 0 };
        glGenSamplers(1, &native->m_sampler);

        auto cvt_filter = [](jegl_filter filter) -> GLint
        {
            return filter == jegl_filter::LINEAR ? GL_LINEAR : GL_NEAREST;
        };
        auto cvt_wrap = [](jegl_wrap_mode wrap) -> GLint
        {
            switch (wrap)
            {
            case jegl_wrap_mode::CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
            case jegl_wrap_mode::REPEAT: return GL_REPEAT;
            case jegl_wrap_mode::MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
            default:
                assert(false);
                return GL_CLAMP_TO_EDGE;
            }
        };

        const auto& desc = sampler->m_desc;
        glSamplerParameteri(
            native->m_sampler, GL_TEXTURE_MIN_FILTER, cvt_filter(desc.m_min_filter));
        glSamplerParameteri(
            native->m_sampler, GL_TEXTURE_MAG_FILTER, cvt_filter(desc.m_mag_filter));
        glSamplerParameteri(
            native->m_sampler, GL_TEXTURE_WRAP_S, cvt_wrap(desc.m_u_wrap));
        glSamplerParameteri(
            native->m_sampler, GL_TEXTURE_WRAP_T, cvt_wrap(desc.m_v_wrap));
        glSamplerParameteri(
            native->m_sampler, GL_TEXTURE_WRAP_R, cvt_wrap(desc.m_w_wrap));

        sampler->m_backend_ptr = native;
    }

    void gl_render_sampler_close(
        je_GraphicImplContext, jegl_sampler* sampler)
    {
        auto* native = static_cast<jegl_gl3_render_sampler*>(
            sampler->m_backend_ptr);

        if (native != nullptr)
        {
            glDeleteSamplers(1, &native->m_sampler);
            delete native;
            sampler->m_backend_ptr = nullptr;
        }
    }

    void gl_render_sampler_bind(
        je_GraphicImplContext ctx, uint32_t sampler_id, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        auto* native = static_cast<jegl_gl3_render_sampler*>(
            sampler->m_backend_ptr);

        assert(native != nullptr);
        assert(context->m_render_current_shader_resource != nullptr);

        auto* shader = context->m_render_current_shader_resource;
        bool found = false;

        for (size_t i = 0; i < shader->m_sampler_count; ++i)
        {
            const auto& method = shader->m_sampler_methods[i];
            if (method.m_sampler_id != sampler_id)
                continue;

            found = true;
            for (uint64_t pass = 0; pass < method.m_pass_id_count; ++pass)
                glBindSampler(method.m_pass_ids[pass], native->m_sampler);
        }

        assert(found);
        (void)found;
    }

    void gl_render_begin_pass(
        je_GraphicImplContext ctx,
        /* OPTIONAL */ jegl_frame_buffer* target,
        const jegl_viewport* viewport)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);

        context->current_active_shader_may_null = nullptr;
        context->m_render_current_shader_resource = nullptr;
        context->m_render_current_vertex = nullptr;
        context->m_render_target_is_default = (target == nullptr);

        if (target != nullptr)
        {
            auto* framebuffer = static_cast<jegl_gl3_framebuf*>(
                target->m_handle.m_ptr);
            assert(framebuffer != nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->m_fbo);
        }
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(
            (GLint)viewport->x,
            (GLint)viewport->y,
            (GLsizei)viewport->width,
            (GLsizei)viewport->height);
    }

    void gl_render_clear_color(
        je_GraphicImplContext ctx, uint32_t attachment, const float rgba[4])
    {
#if defined(JE_ENABLE_GL330_GAPI) && defined(GL_FRAMEBUFFER_SRGB)
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        if (context->m_render_target_is_default)
        {
            // sRGB conversion applies to fragment writes and blending, but
            // not to clears; encode the linear clear color before writing.
            const jeecs::math::vec4 srgb =
                jeecs::math::color(rgba[0], rgba[1], rgba[2], rgba[3]).to_srgb();
            const float encoded[4] = { srgb.x, srgb.y, srgb.z, srgb.w };
            glClearBufferfv(GL_COLOR, (GLint)attachment, encoded);
            return;
        }
#else
        (void)ctx;
#endif
        glClearBufferfv(GL_COLOR, (GLint)attachment, rgba);
    }

    void gl_render_clear_depth(
        je_GraphicImplContext ctx, float depth)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        _gl_update_depth_mask_method(context, GL_TRUE);
        glClearBufferfv(GL_DEPTH, 0, &depth);
    }

    void gl_render_end_pass(je_GraphicImplContext)
    {}

    void gl_render_bind_vertex(
        je_GraphicImplContext ctx, jegl_vertex* vertex)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        auto* vertex_data = static_cast<jegl3_vertex_data*>(
            vertex->m_handle.m_ptr);

        assert(vertex_data != nullptr);
        context->m_render_current_vertex = vertex_data;
        glBindVertexArray(vertex_data->m_vao);
    }

    void gl_render_bind_uniform_buffer(
        je_GraphicImplContext ctx, jegl_uniform_buffer* uniform_buffer)
    {
        gl_bind_uniform_buffer(ctx, uniform_buffer);
    }

    void gl_render_bind_texture_2d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_2d* texture)
    {
        gl_bind_texture<jegl_texture_2d>(ctx, texture, pass);
    }

    void gl_render_bind_texture_3d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_3d* texture)
    {
        gl_bind_texture<jegl_texture_3d>(ctx, texture, pass);
    }

    void gl_render_bind_texture_array(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_array* texture)
    {
        gl_bind_texture<jegl_texture_array>(ctx, texture, pass);
    }

    void gl_render_bind_texture_cube(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_cube* texture)
    {
        gl_bind_texture<jegl_texture_cube>(ctx, texture, pass);
    }

    void gl_render_upload_uniform_block(
        jegl_gl3_context* context,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        assert(uniform_data != nullptr);
        assert(uniform_size != 0);
        assert(context->m_render_uniform_ring != 0);

        const GLint alignment = context->m_render_uniform_alignment;
        GLintptr offset =
            (context->m_render_uniform_ring_offset + alignment - 1)
            / alignment * alignment;

        if (offset + uniform_size > context->m_render_uniform_ring_size)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, context->m_render_uniform_ring);
            glBufferData(
                GL_UNIFORM_BUFFER,
                context->m_render_uniform_ring_size,
                nullptr,
                GL_STREAM_DRAW);
            offset = 0;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, context->m_render_uniform_ring);
        glBufferSubData(
            GL_UNIFORM_BUFFER, offset, uniform_size, uniform_data);
        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            0,
            context->m_render_uniform_ring,
            offset,
            uniform_size);

        context->m_render_uniform_ring_offset = offset + uniform_size;
    }

    void gl_render_draw_indexed(
        je_GraphicImplContext ctx,
        uint32_t index_count,
        uint32_t first_index,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        auto* vertex_data = context->m_render_current_vertex;

        assert(vertex_data != nullptr);
        assert(index_count > 0);
        assert(instance_count > 0);

        assert(context->current_active_shader_may_null != nullptr);
        assert(context->m_render_current_shader_resource != nullptr);
        assert(
            context->m_render_current_shader_resource->m_uniform_block_size
            == uniform_size);

        if (uniform_size != 0)
            gl_render_upload_uniform_block(context, uniform_data, uniform_size);

        const void* index_offset = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(first_index) * sizeof(uint32_t));

        if (instance_count > 1)
        {
            glDrawElementsInstanced(
                vertex_data->m_method,
                (GLsizei)index_count,
                GL_UNSIGNED_INT,
                index_offset,
                (GLsizei)instance_count);
        }
        else
        {
            glDrawElements(
                vertex_data->m_method,
                (GLsizei)index_count,
                GL_UNSIGNED_INT,
                index_offset);
        }
    }

    void gl_render_draw(
        je_GraphicImplContext ctx,
        uint32_t vertex_count,
        uint32_t first_vertex,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);
        auto* vertex_data = context->m_render_current_vertex;

        assert(vertex_data != nullptr);
        assert(vertex_count > 0);
        assert(instance_count > 0);

        assert(context->current_active_shader_may_null != nullptr);
        assert(context->m_render_current_shader_resource != nullptr);
        assert(
            context->m_render_current_shader_resource->m_uniform_block_size
            == uniform_size);

        if (uniform_size != 0)
            gl_render_upload_uniform_block(context, uniform_data, uniform_size);

        if (instance_count > 1)
        {
            glDrawArraysInstanced(
                vertex_data->m_method,
                (GLint)first_vertex,
                (GLsizei)vertex_count,
                (GLsizei)instance_count);
        }
        else
        {
            glDrawArrays(
                vertex_data->m_method,
                (GLint)first_vertex,
                (GLsizei)vertex_count);
        }
    }

    void gl_render_commit_begin(je_GraphicImplContext ctx)
    {
        auto* context = static_cast<jegl_gl3_context*>(ctx);

        if (context->m_render_uniform_ring == 0)
        {
            glGenBuffers(1, &context->m_render_uniform_ring);
            glBindBuffer(GL_UNIFORM_BUFFER, context->m_render_uniform_ring);
            glBufferData(
                GL_UNIFORM_BUFFER,
                context->m_render_uniform_ring_size,
                nullptr,
                GL_STREAM_DRAW);

            glGetIntegerv(
                GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                &context->m_render_uniform_alignment);
            if (context->m_render_uniform_alignment <= 0)
                context->m_render_uniform_alignment = 256;
        }
        else
        {
            glBindBuffer(GL_UNIFORM_BUFFER, context->m_render_uniform_ring);
            glBufferData(
                GL_UNIFORM_BUFFER,
                context->m_render_uniform_ring_size,
                nullptr,
                GL_STREAM_DRAW);
        }

        context->m_render_uniform_ring_offset = 0;
    }

    void gl_render_commit_end(je_GraphicImplContext)
    {
        // Restore the assumptions of the legacy immediate path.
        glEnable(GL_DEPTH_TEST);
        glFrontFace(GL_CCW);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}

void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::gl3;

    write_to_apis->interface_startup = gl_startup;
    write_to_apis->interface_shutdown_before_resource_release = gl_pre_shutdown;
    write_to_apis->interface_shutdown = gl_shutdown;

    write_to_apis->update_frame_ready = gl_pre_update;
    write_to_apis->update_draw_commit = gl_commit_update;

    write_to_apis->shader_create_blob = shader_create_resource_blob;
    write_to_apis->texture_2d_create_blob = texture_create_resource_blob<jegl_texture_2d>;
    write_to_apis->texture_3d_create_blob = texture_create_resource_blob<jegl_texture_3d>;
    write_to_apis->texture_array_create_blob = texture_create_resource_blob<jegl_texture_array>;
    write_to_apis->texture_cube_create_blob = texture_create_resource_blob<jegl_texture_cube>;
    write_to_apis->texture_msaa_create_blob = texture_create_resource_blob<jegl_texture_msaa>;
    write_to_apis->vertex_create_blob = vertex_create_resource_blob;

    write_to_apis->shader_close_blob = shader_close_resource_blob;
    write_to_apis->texture_close_blob = texture_close_resource_blob;
    write_to_apis->vertex_close_blob = vertex_close_resource_blob;

    write_to_apis->shader_init = shader_init;
    write_to_apis->texture_2d_init = texture_2d_init;
    write_to_apis->texture_3d_init = texture_3d_init;
    write_to_apis->texture_array_init = texture_array_init;
    write_to_apis->texture_cube_init = texture_cube_init;
    write_to_apis->texture_msaa_init = texture_msaa_init;
    write_to_apis->vertex_init = vertex_init;
    write_to_apis->framebuffer_init = framebuffer_init;
    write_to_apis->ubuffer_init = ubuffer_init;

    write_to_apis->shader_update = shader_update;
    write_to_apis->texture_2d_update = texture_2d_update;
    write_to_apis->texture_3d_update = texture_3d_update;
    write_to_apis->texture_array_update = texture_array_update;
    write_to_apis->texture_cube_update = texture_cube_update;
    write_to_apis->texture_msaa_update = texture_msaa_update;
    write_to_apis->vertex_update = vertex_update;
    write_to_apis->framebuffer_update = framebuffer_update;
    write_to_apis->ubuffer_update = ubuffer_update;

    write_to_apis->shader_close = shader_close;
    write_to_apis->texture_2d_close = texture_close<jegl_texture_2d>;
    write_to_apis->texture_3d_close = texture_close<jegl_texture_3d>;
    write_to_apis->texture_array_close = texture_close<jegl_texture_array>;
    write_to_apis->texture_cube_close = texture_close<jegl_texture_cube>;
    write_to_apis->texture_msaa_close = texture_close<jegl_texture_msaa>;
    write_to_apis->vertex_close = vertex_close;
    write_to_apis->framebuffer_close = framebuffer_close;
    write_to_apis->ubuffer_close = ubuffer_close;

    write_to_apis->pipeline_init = gl_render_pipeline_init;
    write_to_apis->pipeline_close = gl_render_pipeline_close;
    write_to_apis->pipeline_bind = gl_render_pipeline_bind;

    write_to_apis->sampler_init = gl_render_sampler_init;
    write_to_apis->sampler_close = gl_render_sampler_close;
    write_to_apis->sampler_bind = gl_render_sampler_bind;

    write_to_apis->begin_pass = gl_render_begin_pass;
    write_to_apis->clear_color = gl_render_clear_color;
    write_to_apis->clear_depth = gl_render_clear_depth;
    write_to_apis->end_pass = gl_render_end_pass;

    write_to_apis->bind_vertex = gl_render_bind_vertex;
    write_to_apis->bind_uniform_buffer = gl_render_bind_uniform_buffer;
    write_to_apis->bind_texture_2d = gl_render_bind_texture_2d;
    write_to_apis->bind_texture_3d = gl_render_bind_texture_3d;
    write_to_apis->bind_texture_array = gl_render_bind_texture_array;
    write_to_apis->bind_texture_cube = gl_render_bind_texture_cube;

    write_to_apis->draw_indexed = gl_render_draw_indexed;
    write_to_apis->draw = gl_render_draw;

    write_to_apis->commit_begin = gl_render_commit_begin;
    write_to_apis->commit_end = gl_render_commit_end;
}
#else
void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GL3 not available.");
}
#endif
