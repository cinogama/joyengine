#define JE_IMPL
#include "jeecs.hpp"

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
            size_t m_uniform_size;

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
                , m_uniform_size(0)
            {
            }
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
        {
        }
    };
    struct jegl_gl3_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_shader);

        jeecs::basic::resource<jegl3_shader_blob::jegl3_shader_blob_shared>
            m_shared_blob_data;
        size_t uniform_buffer_size;
        size_t uniform_buffer_update_offset;
        size_t uniform_buffer_update_size;

        void* uniform_cpu_buffers;
        GLuint uniforms;

        jegl_gl3_shader(jegl3_shader_blob* blob)
            : m_shared_blob_data(blob->m_shared_blob_data)
            , uniform_buffer_size(blob->m_shared_blob_data->m_uniform_size)
            , uniform_buffer_update_offset(0)
            , uniform_buffer_update_size(0)
        {
            if (uniform_buffer_size > 0)
            {
                glGenBuffers(1, &uniforms);
                glBindBuffer(GL_UNIFORM_BUFFER, uniforms);
                glBufferData(
                    GL_UNIFORM_BUFFER,
                    (GLsizeiptr)uniform_buffer_size,
                    NULL,
                    GL_DYNAMIC_DRAW);

                uniform_cpu_buffers = malloc(uniform_buffer_size);
                assert(uniform_cpu_buffers != nullptr);

                memset(uniform_cpu_buffers, 0, uniform_buffer_size);
            }
            else
                uniform_cpu_buffers = nullptr;
        }
        ~jegl_gl3_shader()
        {
            if (uniform_cpu_buffers != nullptr)
            {
                glDeleteBuffers(1, &uniforms);
                free(uniform_cpu_buffers);
            }
        }
    };
    struct jegl_gl3_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_texture);

        GLuint m_texture_id;
        jegl_texture::format m_texture_format;

        jegl_gl3_texture(GLuint id, jegl_texture::format fmt)
            : m_texture_id(id)
            , m_texture_format(fmt)
        {
        }
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
        {
        }
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

        GLuint m_binded_texture_passes[128] = {};
        GLenum m_binded_texture_passes_type[128] = {};

        jegl_gl3_shader* current_active_shader_may_null = nullptr;
        GLenum ACTIVE_DEPTH_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_MASK_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_EQUATION = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_SRC_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_BLEND_DST_MODE = GL_INVALID_ENUM;
        GLenum ACTIVE_CULL_MODE = GL_INVALID_ENUM;

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
            assert(pass < 128);
            if (m_binded_texture_passes[pass] != texture || m_binded_texture_passes_type[pass] != texture)
            {
                glActiveTexture(GL_TEXTURE0 + pass);
                glBindTexture(type, texture);
                m_binded_texture_passes[pass] = texture;
                m_binded_texture_passes_type[pass] = type;
            }
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
                NULL, GL_DYNAMIC_COPY); // 预分配空间

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

    void _gl_bind_shader_samplers(jegl_gl3_shader* shader_instance)
    {
        const auto& blob_shared = shader_instance->m_shared_blob_data;
        for (size_t i = 0; i < blob_shared->m_sampler_count; i += 1)
        {
            auto& sampler = blob_shared->m_samplers[i];
            for (auto id : sampler.m_passes)
                glBindSampler((GLuint)id, sampler.m_sampler);
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
    jegl_context::graphic_impl_context_t gl_startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
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
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif
#endif
        glEnable(GL_DEPTH_TEST);
        jegui_init_gl330(
            gthread,
            [](jegl_context*, jegl_texture* res)
            {
                jegl_gl3_texture* texture_instance =
                    reinterpret_cast<jegl_gl3_texture*>(res->m_handle.m_ptr); \

                    return (uint64_t)texture_instance->m_texture_id;
            },
            [](jegl_context*, jegl_shader* res)
            {
                jegl_gl3_shader* shader_instance =
                    reinterpret_cast<jegl_gl3_shader*>(res->m_handle.m_ptr);

                assert(shader_instance != nullptr);
                _gl_bind_shader_samplers(shader_instance);
            },
            context->m_interface->interface_handle(),
            reboot);

        return context;
    }

    jegl_update_action gl_pre_update(jegl_context::graphic_impl_context_t ctx)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(ctx);

        context->current_active_shader_may_null = nullptr;

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

            // Reset context->current_active_shader_may_null to make sure bind correctly.
            context->current_active_shader_may_null = nullptr;
            return jegl_update_action::JEGL_UPDATE_CONTINUE;
        default:
            abort();
        }
    }

    jegl_update_action gl_commit_update(
        jegl_context::graphic_impl_context_t, jegl_update_action)
    {
        // 回到默认帧缓冲区
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        jegui_update_gl330();

        // 将绘制命令异步地提交给GPU
        glFlush();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    void gl_pre_shutdown(jegl_context*, jegl_context::graphic_impl_context_t, bool)
    {
    }
    void gl_shutdown(jegl_context*, jegl_context::graphic_impl_context_t userdata, bool reboot)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(userdata);

        if (!reboot)
            jeecs::debug::log("Graphic thread (OpenGL3) shutdown!");

        jegui_shutdown_gl330(reboot);

        context->m_interface->shutdown(reboot);
        delete context;
    }
    void gl_set_uniform(
        jegl_context::graphic_impl_context_t ctx,
        uint32_t location,
        jegl_shader::uniform_type type,
        const void* val)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(ctx);

        if (location == jeecs::graphic::INVALID_UNIFORM_LOCATION
            || context->current_active_shader_may_null == nullptr)
            return;

        auto* current_shader = context->current_active_shader_may_null;
        auto* target_buffer = reinterpret_cast<void*>(
            reinterpret_cast<intptr_t>(
                current_shader->uniform_cpu_buffers) + location);

        bool continuous_copy = true;
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
            continuous_copy = false;
            data_size_byte_length = 48;

            float* target_storage = reinterpret_cast<float*>(target_buffer);
            const float* source_storage = reinterpret_cast<const float*>(val);

            memcpy(target_storage + 0, source_storage + 0, 12);
            memcpy(target_storage + 4, source_storage + 3, 12);
            memcpy(target_storage + 8, source_storage + 6, 12);
            break;
        }
        case jegl_shader::FLOAT4X4:
            data_size_byte_length = 64;
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set.");
            break;
        }

        if (continuous_copy)
            memcpy(target_buffer, val, data_size_byte_length);

        if (current_shader->uniform_buffer_update_size == 0)
        {
            current_shader->uniform_buffer_update_offset = location;
            current_shader->uniform_buffer_update_size = data_size_byte_length;
        }
        else
        {
            const size_t new_begin = std::min(
                current_shader->uniform_buffer_update_offset,
                static_cast<size_t>(location));

            const size_t new_end = std::max(
                current_shader->uniform_buffer_update_offset + current_shader->uniform_buffer_update_size,
                location + data_size_byte_length);

            current_shader->uniform_buffer_update_offset = new_begin;
            current_shader->uniform_buffer_update_size = new_end - new_begin;
        }
    }

    jegl_resource_blob shader_create_resource_blob(
        jegl_context::graphic_impl_context_t ctx, jegl_shader* resource)
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
            size_t max_allign = 4;
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
                    max_allign = std::max(max_allign, allign_base);

                    last_elem_end_place = jeecs::basic::allign_size(last_elem_end_place, allign_base);
                    shared_blob->m_uniform_locations[uniforms->m_name] = last_elem_end_place;
                    last_elem_end_place += unit_size;
                }
                uniforms = uniforms->m_next;
            }
            shared_blob->m_uniform_size =
                jeecs::basic::allign_size(last_elem_end_place, max_allign);

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
        jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob)
    {
        delete reinterpret_cast<jegl3_shader_blob*>(blob);
    }

    jegl_resource_blob texture_create_resource_blob(
        jegl_context::graphic_impl_context_t ctx, jegl_texture* resource)
    {
        return nullptr;
    }
    void texture_close_resource_blob(
        jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob)
    {
    }

    jegl_resource_blob vertex_create_resource_blob(
        jegl_context::graphic_impl_context_t ctx, jegl_vertex* resource)
    {
        return nullptr;
    }
    void vertex_close_resource_blob(
        jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob)
    {
    }

    void shader_init(
        jegl_context::graphic_impl_context_t ctx,
        jegl_resource_blob blob,
        jegl_shader* resource)
    {
        resource->m_handle.m_ptr = nullptr;
        if (blob != nullptr)
        {
            auto* shader_blob = reinterpret_cast<jegl3_shader_blob*>(blob);

            auto shader_program = shader_blob->m_shared_blob_data->m_shader_program_instance;
            glUseProgram(shader_program);

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
        jegl_context::graphic_impl_context_t,
        jegl_shader* resource)
    {
    }
    void shader_close(
        jegl_context::graphic_impl_context_t,
        jegl_shader* resource)
    {
        jegl_gl3_shader* shader_instance =
            reinterpret_cast<jegl_gl3_shader*>(resource->m_handle.m_ptr);

        if (shader_instance != nullptr)
            delete shader_instance;
    }

    void texture_init(
        jegl_context::graphic_impl_context_t,
        jegl_resource_blob,
        jegl_texture* resource)
    {
        GLuint texture;
        glGenTextures(1, &texture);

        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        bool is_16bit = 0 != (resource->m_format & jegl_texture::format::FLOAT16);
        bool is_depth = 0 != (resource->m_format & jegl_texture::format::DEPTH);
        bool is_cube = 0 != (resource->m_format & jegl_texture::format::CUBE);

        auto gl_texture_type = GL_TEXTURE_2D;
        glBindTexture(gl_texture_type, texture);

        assert(GL_TEXTURE_2D == gl_texture_type);

        const GLenum jegl_texture_cube_map_ways[] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z };

        if (is_depth)
        {
            if (is_16bit)
                jeecs::debug::logerr("Depth texture cannot use 16bit.");

            if (is_cube)
            {
                for (auto way : jegl_texture_cube_map_ways)
                {
                    glTexImage2D(way, 0,
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
            }
            else
                glTexImage2D(gl_texture_type, 0,
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
            // Depth texture do not use color format
            switch (resource->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
            {
            case jegl_texture::format::MONO:
                texture_src_format = GL_LUMINANCE;
                texture_aim_format = is_16bit ? GL_R16F : GL_LUMINANCE;
                break;
            case jegl_texture::format::RGBA:
                texture_src_format = GL_RGBA;
                texture_aim_format = is_16bit ? GL_RGBA16F : GL_RGBA;
                break;
            default:
                jeecs::debug::logerr("Unknown texture raw-data format.");
            }

            if (is_cube)
            {
                for (auto way : jegl_texture_cube_map_ways)
                {
                    glTexImage2D(way, 0, texture_aim_format,
                        (GLsizei)resource->m_width,
                        (GLsizei)resource->m_height,
                        0, texture_src_format,
                        is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                        resource->m_pixels);
                }
            }
            else
            {
                glTexImage2D(gl_texture_type,
                    0, texture_aim_format,
                    (GLsizei)resource->m_width,
                    (GLsizei)resource->m_height,
                    0, texture_src_format,
                    is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                    resource->m_pixels);
            }
        }

        resource->m_handle.m_ptr = new jegl_gl3_texture(texture, resource->m_format);
    }
    void texture_update(
        jegl_context::graphic_impl_context_t,
        jegl_texture* resource)
    {
        jegl_gl3_texture* texture_instance =
            reinterpret_cast<jegl_gl3_texture*>(resource->m_handle.m_ptr);

        // Update texture's pixels, only normal pixel data will be updated.
        glBindTexture(GL_TEXTURE_2D, texture_instance->m_texture_id);

        // Textures FORMAT & SIZE will not be changed.
        bool is_16bit = 0 != (resource->m_format & jegl_texture::format::FLOAT16);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            (GLsizei)resource->m_width,
            (GLsizei)resource->m_height,
            0,
            GL_RGBA,
            is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
            resource->m_pixels);
    }
    void texture_close(
        jegl_context::graphic_impl_context_t,
        jegl_texture* resource)
    {
        delete reinterpret_cast<jegl_gl3_texture*>(resource->m_handle.m_ptr);
    }

    void vertex_init(
        jegl_context::graphic_impl_context_t,
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
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            resource->m_index_count * sizeof(uint32_t),
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
        jegl_context::graphic_impl_context_t,
        jegl_vertex* resource)
    {
    }
    void vertex_close(
        jegl_context::graphic_impl_context_t,
        jegl_vertex* resource)
    {
        delete reinterpret_cast<jegl3_vertex_data*>(resource->m_handle.m_ptr);
    }

    void ubuffer_init(
        jegl_context::graphic_impl_context_t,
        jegl_uniform_buffer* resource)
    {
        resource->m_handle.m_ptr = new jegl_gl3_uniformbuf(resource);
    }
    void ubuffer_update(
        jegl_context::graphic_impl_context_t,
        jegl_uniform_buffer* resource)
    {
        jegl_gl3_uniformbuf* ubuf =
            reinterpret_cast<jegl_gl3_uniformbuf*>(resource->m_handle.m_ptr);

        assert(resource->m_update_length != 0);

        glBindBuffer(GL_UNIFORM_BUFFER, ubuf->m_uniform_buffer_object);

        assert(resource->m_update_length != 0);
        glBufferSubData(GL_UNIFORM_BUFFER,
            resource->m_update_begin_offset,
            resource->m_update_length,
            resource->m_buffer + resource->m_update_begin_offset);
    }
    void ubuffer_close(
        jegl_context::graphic_impl_context_t,
        jegl_uniform_buffer* resource)
    {
        delete reinterpret_cast<jegl_gl3_uniformbuf*>(resource->m_handle.m_ptr);
    }

    void framebuffer_init(
        jegl_context::graphic_impl_context_t,
        jegl_frame_buffer* resource)
    {
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        bool already_has_attached_depth = false;

        GLenum attachment = GL_COLOR_ATTACHMENT0;

        jeecs::basic::resource<jeecs::graphic::texture>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
                resource->m_output_attachments);

        for (size_t i = 0; i < resource->m_attachment_count; ++i)
        {
            jegl_texture* frame_texture = attachments[i]->resource();

            jegl_bind_texture(frame_texture, 0);

            GLenum using_attachment = attachment;
            GLenum buffer_texture_type = GL_TEXTURE_2D;

            if (0 != (frame_texture->m_format & jegl_texture::format::DEPTH))
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
                reinterpret_cast<jegl_gl3_texture*>(frame_texture->m_handle.m_ptr)->m_texture_id,
                0);
        }
        std::vector<GLuint> glattachments;
        for (GLenum attachment_index = GL_COLOR_ATTACHMENT0; attachment_index < attachment; ++attachment_index)
            glattachments.push_back(attachment_index);

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
        jegl_context::graphic_impl_context_t,
        jegl_frame_buffer* resource)
    {
    }
    void framebuffer_close(
        jegl_context::graphic_impl_context_t,
        jegl_frame_buffer* resource)
    {
        delete reinterpret_cast<jegl_gl3_framebuf*>(resource->m_handle.m_ptr);
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
        if (ctx->ACTIVE_MASK_MODE != mode)
        {
            ctx->ACTIVE_MASK_MODE = mode;
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

    bool _gl_using_shader_program(jegl_gl3_context* context, jegl_shader* resource)
    {
        jegl_gl3_shader* shader_instance =
            reinterpret_cast<jegl_gl3_shader*>(resource->m_handle.m_ptr);

        if (context->current_active_shader_may_null == shader_instance)
            return shader_instance != nullptr;

        context->current_active_shader_may_null = shader_instance;
        if (shader_instance == nullptr)
            return false;

        auto* shared_blob_state = shader_instance->m_shared_blob_data.get();

        _gl_update_depth_test_method(context, shared_blob_state->m_depth_test_method);
        _gl_update_depth_mask_method(context, shared_blob_state->m_depth_write_mask);
        if (_gl_update_blend_equation_method(context, shared_blob_state->m_blend_equation))
            _gl_update_blend_mode_method(
                context,
                shared_blob_state->m_blend_src_mode,
                shared_blob_state->m_blend_dst_mode);
        _gl_update_cull_mode_method(context, shared_blob_state->m_cull_face_method);
        _gl_bind_shader_samplers(shader_instance);
        glUseProgram(shader_instance->m_shared_blob_data->m_shader_program_instance);

        return true;
    }

    bool gl_bind_shader(jegl_context::graphic_impl_context_t context, jegl_shader* shader)
    {
        jegl_gl3_context* ctx = reinterpret_cast<jegl_gl3_context*>(context);
        return _gl_using_shader_program(ctx, shader);
    }

    void gl_bind_uniform_buffer(jegl_context::graphic_impl_context_t, jegl_uniform_buffer* uniformbuf)
    {
        jegl_gl3_uniformbuf* ubuf =
            reinterpret_cast<jegl_gl3_uniformbuf*>(uniformbuf->m_handle.m_ptr);

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            ubuf->m_binding_place,
            ubuf->m_uniform_buffer_object,
            0,
            ubuf->m_uniform_buffer_size);
    }

    void gl_bind_texture(jegl_context::graphic_impl_context_t ctx, jegl_texture* texture, size_t pass)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(ctx);
        jegl_gl3_texture* texture_instance =
            reinterpret_cast<jegl_gl3_texture*>(texture->m_handle.m_ptr);

        if (0 != (texture_instance->m_texture_format & jegl_texture::format::CUBE))
            context->bind_texture_pass_impl(
                (GLint)pass, GL_TEXTURE_CUBE_MAP, texture_instance->m_texture_id);
        else
            context->bind_texture_pass_impl(
                (GLint)pass, GL_TEXTURE_2D, texture_instance->m_texture_id);
    }

    void gl_draw_vertex_with_shader(jegl_context::graphic_impl_context_t ctx, jegl_vertex* vert)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(ctx);
        jegl3_vertex_data* vdata = reinterpret_cast<jegl3_vertex_data*>(vert->m_handle.m_ptr);

        auto* current_shader = context->current_active_shader_may_null;
        assert(current_shader != nullptr);

        if (current_shader->uniform_cpu_buffers != nullptr)
        {
            glBindBufferRange(GL_UNIFORM_BUFFER, 0,
                current_shader->uniforms, 0, current_shader->uniform_buffer_size);

            if (current_shader->uniform_buffer_update_size != 0)
            {
                glBindBuffer(GL_UNIFORM_BUFFER, current_shader->uniforms);

                glBufferSubData(GL_UNIFORM_BUFFER,
                    current_shader->uniform_buffer_update_offset,
                    current_shader->uniform_buffer_update_size,
                    reinterpret_cast<void*>(
                        reinterpret_cast<intptr_t>(current_shader->uniform_cpu_buffers)
                        + current_shader->uniform_buffer_update_offset));

                current_shader->uniform_buffer_update_size = 0;
            }
        }

        glBindVertexArray(vdata->m_vao);
        glDrawElements(vdata->m_method, vdata->m_pointcount, GL_UNSIGNED_INT, 0);
    }

    void gl_set_rend_to_framebuffer(
        jegl_context::graphic_impl_context_t ctx,
        jegl_frame_buffer* framebuffer,
        const int32_t(*viewport_xywh)[4],
        const jegl_frame_buffer_clear_operation* clear_operations)
    {
        jegl_gl3_context* context = reinterpret_cast<jegl_gl3_context*>(ctx);

        // Reset current binded shader.
        context->current_active_shader_may_null = nullptr;

        jegl_gl3_framebuf* framebuffer_instance = nullptr;
        if (framebuffer != nullptr)
            framebuffer_instance = reinterpret_cast<jegl_gl3_framebuf*>(
                framebuffer->m_handle.m_ptr);

        if (nullptr == framebuffer_instance)
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_instance->m_fbo);

        int32_t x = 0, y = 0, w = 0, h = 0;
        if (viewport_xywh != nullptr)
        {
            auto& viewport = *viewport_xywh;
            x = viewport[0];
            y = viewport[1];
            w = viewport[2];
            h = viewport[3];
        }

        if (w == 0)
            w = static_cast<int32_t>(
                framebuffer_instance != nullptr
                ? framebuffer_instance->m_frame_width
                : context->RESOLUTION_WIDTH);
        if (h == 0)
            h = static_cast<int32_t>(
                framebuffer_instance != nullptr
                ? framebuffer_instance->m_frame_height
                : context->RESOLUTION_HEIGHT);

        glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);

        while (clear_operations != nullptr)
        {
            switch (clear_operations->m_type)
            {
            case jegl_frame_buffer_clear_operation::clear_type::COLOR:
                glClearBufferfv(
                    GL_COLOR,
                    (GLint)clear_operations->m_color.m_color_attachment_idx,
                    clear_operations->m_color.m_clear_color_rgba);
                break;
            case jegl_frame_buffer_clear_operation::clear_type::DEPTH:
            {
                const float gl_depth =
                    clear_operations->m_depth.m_clear_depth * 2.f - 1.f;

                _gl_update_depth_mask_method(context, GL_TRUE);
                glClearBufferfv(
                    GL_DEPTH,
                    0,
                    &gl_depth);
                break;
            }
            default:
                jeecs::debug::logfatal("Unknown framebuffer clear operation.");
                abort();
                break;
            }
            clear_operations = clear_operations->m_next;
        }
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
    write_to_apis->texture_create_blob = texture_create_resource_blob;
    write_to_apis->vertex_create_blob = vertex_create_resource_blob;

    write_to_apis->shader_close_blob = shader_close_resource_blob;
    write_to_apis->texture_close_blob = texture_close_resource_blob;
    write_to_apis->vertex_close_blob = vertex_close_resource_blob;

    write_to_apis->shader_init = shader_init;
    write_to_apis->texture_init = texture_init;
    write_to_apis->vertex_init = vertex_init;
    write_to_apis->framebuffer_init = framebuffer_init;
    write_to_apis->ubuffer_init = ubuffer_init;

    write_to_apis->shader_update = shader_update;
    write_to_apis->texture_update = texture_update;
    write_to_apis->vertex_update = vertex_update;
    write_to_apis->framebuffer_update = framebuffer_update;
    write_to_apis->ubuffer_update = ubuffer_update;

    write_to_apis->shader_close = shader_close;
    write_to_apis->texture_close = texture_close;
    write_to_apis->vertex_close = vertex_close;
    write_to_apis->framebuffer_close = framebuffer_close;
    write_to_apis->ubuffer_close = ubuffer_close;

    write_to_apis->bind_uniform_buffer = gl_bind_uniform_buffer;
    write_to_apis->bind_texture = gl_bind_texture;
    write_to_apis->bind_shader = gl_bind_shader;
    write_to_apis->draw_vertex = gl_draw_vertex_with_shader;

    write_to_apis->bind_framebuf = gl_set_rend_to_framebuffer;

    write_to_apis->set_uniform = gl_set_uniform;
}
#else
void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GL3 not available.");
}
#endif
