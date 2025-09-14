#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)

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
        GLuint m_vao;
        GLuint m_vbo;
        GLuint m_ebo;
        GLenum m_method;
        GLsizei m_pointcount;
    };
    struct jegl3_sampler
    {
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
        struct jegl3_shader_blob_shared
        {
            GLuint m_shader_program_instance;

            jegl3_sampler* m_samplers;
            uint32_t m_sampler_count;

            std::unordered_map<std::string, uint32_t> m_uniform_locations;
            size_t m_uniform_size;

            JECS_DISABLE_MOVE_AND_COPY(jegl3_shader_blob_shared);

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

                return jeecs::typing::INVALID_UINT32;
            }
        };

        jeecs::basic::resource<jegl3_shader_blob_shared> m_shared_blob_data;

        JECS_DISABLE_MOVE_AND_COPY(jegl3_shader_blob);

        jegl3_shader_blob(jeecs::basic::resource<jegl3_shader_blob_shared> s)
            : m_shared_blob_data(s)
        {
        }
    };
    struct jegl_gl3_shader
    {
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
    struct jegl_gl3_context
    {
        basic_interface* m_interface;

        size_t RESOLUTION_WIDTH = 0;
        size_t RESOLUTION_HEIGHT = 0;

        GLint m_last_active_pass_id = 0;
        GLuint m_binded_texture_passes[128] = {};
        GLenum m_binded_texture_passes_type[128] = {};

        jegl_gl3_shader* current_active_shader_may_null = nullptr;
        jegl_shader::depth_test_method ACTIVE_DEPTH_MODE = jegl_shader::depth_test_method::INVALID;
        jegl_shader::depth_mask_method ACTIVE_MASK_MODE = jegl_shader::depth_mask_method::INVALID;
        jegl_shader::blend_equation ACTIVE_BLEND_EQUATION = jegl_shader::blend_equation::INVALID;
        jegl_shader::blend_method ACTIVE_BLEND_SRC_MODE = jegl_shader::blend_method::INVALID;
        jegl_shader::blend_method ACTIVE_BLEND_DST_MODE = jegl_shader::blend_method::INVALID;
        jegl_shader::cull_mode ACTIVE_CULL_MODE = jegl_shader::cull_mode::INVALID;

        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_context);

        jegl_gl3_context(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
        {
            m_interface =
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
                new egl();
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
        bool get_last_binded_texture_and_type(GLuint* out_texture, GLenum* out_type)
        {
            if (m_binded_texture_passes_type[m_last_active_pass_id] != 0)
            {
                *out_texture = m_binded_texture_passes[m_last_active_pass_id];
                *out_type = m_binded_texture_passes_type[m_last_active_pass_id];

                return true;
            }
            return false;
        }
        void bind_texture_to_last_pass(GLenum type, GLuint texture)
        {
            bind_texture_pass_impl(m_last_active_pass_id, type, texture);
        }
    };
    struct jegl_gl3_uniformbuf
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_gl3_uniformbuf);

        GLuint m_uniform_buffer_object;
        GLuint m_binding_place;
        GLsizeiptr m_uniform_buffer_size;

        jegl_gl3_uniformbuf(jegl_resource* resource)
        {
            glGenBuffers(1, &m_uniform_buffer_object);
            glBindBuffer(GL_UNIFORM_BUFFER, m_uniform_buffer_object);
            glBufferData(GL_UNIFORM_BUFFER,
                resource->m_raw_uniformbuf_data->m_buffer_size,
                NULL, GL_DYNAMIC_COPY); // 预分配空间

            m_binding_place = 
                (GLuint)(resource->m_raw_uniformbuf_data->m_buffer_binding_place + 1);
            m_uniform_buffer_size = (GLsizeiptr)resource->m_raw_uniformbuf_data->m_buffer_size;

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

#ifdef JE_ENABLE_WEBGL20_GAPI
        // Get current context by emscripten_webgl_get_current_context

        // EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context =
        //     emscripten_webgl_get_current_context();

        // if (!emscripten_webgl_enable_extension(
        //     webgl_context, "WEBGL_depth_texture"))
        //{
        //     jeecs::debug::logfatal("Failed to enable WEBGL_depth_texture.");
        //     abort();
        // }
#endif

        jegui_init_gl330(
            gthread,
            [](jegl_context*, jegl_resource* res)
            {
                return (uint64_t)res->m_handle.m_uint1;
            },
            [](jegl_context*, jegl_resource* res)
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
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(userdata));

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

        if (location == jeecs::typing::INVALID_UINT32
            || context->current_active_shader_may_null == nullptr)
            return;

        auto* current_shader = context->current_active_shader_may_null;

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
        case jegl_shader::FLOAT4X4:
            data_size_byte_length = 64;
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set.");
            break;
        }

        memcpy(
            reinterpret_cast<void*>(
                reinterpret_cast<intptr_t>(current_shader->uniform_cpu_buffers) + location),
            val,
            data_size_byte_length);

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

    jegl_resource_blob gl_create_resource_blob(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource)
    {
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            GLuint vs = glCreateShader(GL_VERTEX_SHADER);
            GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
            GLuint shader_program = glCreateProgram();

#ifdef JE_ENABLE_GL330_GAPI
            glShaderSource(vs, 1, &resource->m_raw_shader_data->m_vertex_glsl_src, NULL);
            glShaderSource(fs, 1, &resource->m_raw_shader_data->m_fragment_glsl_src, NULL);
#else
            glShaderSource(vs, 1, &resource->m_raw_shader_data->m_vertex_glsles_src, NULL);
            glShaderSource(fs, 1, &resource->m_raw_shader_data->m_fragment_glsles_src, NULL);
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
                        (uint32_t)resource->m_raw_shader_data->m_sampler_count);

                for (size_t i = 0; i < resource->m_raw_shader_data->m_sampler_count; ++i)
                {
                    auto& sampler_config = resource->m_raw_shader_data->m_sampler_methods[i];
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
                auto* uniforms = resource->m_raw_shader_data->m_custom_uniforms;
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

                        last_elem_end_place = jeecs::basic::allign_size(last_elem_end_place, allign_base);
                        shared_blob->m_uniform_locations[uniforms->m_name] = last_elem_end_place;
                        last_elem_end_place += unit_size;
                    }
                    uniforms = uniforms->m_next;
                }
                shared_blob->m_uniform_size =
                    jeecs::basic::allign_size(last_elem_end_place, max_allign);

                return new jegl3_shader_blob(
                    jeecs::basic::resource<jegl3_shader_blob::jegl3_shader_blob_shared>(
                        shared_blob));
            }
            break;
        }
        case jegl_resource::type::TEXTURE:
            break;
        case jegl_resource::type::VERTEX:
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
            break;
        default:
            break;
        }
        return nullptr;
    }

    void gl_close_resource_blob(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
        {
            auto* shader_blob = std::launder(reinterpret_cast<jegl3_shader_blob*>(blob));
            delete shader_blob;
        }
    }
    void gl_init_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));
        assert(resource->m_custom_resource != nullptr);

        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            resource->m_handle.m_ptr = nullptr;
            if (blob != nullptr)
            {
                auto* shader_blob = std::launder(reinterpret_cast<jegl3_shader_blob*>(blob));

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

                auto& builtin_uniforms = resource->m_raw_shader_data->m_builtin_uniforms;

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

                auto* uniform_var = resource->m_raw_shader_data->m_custom_uniforms;
                while (uniform_var)
                {
                    if (uniform_var->m_uniform_type == jegl_shader::uniform_type::TEXTURE)
                    {
                        uniform_var->m_index = jeecs::typing::INVALID_UINT32;

                        auto count = snprintf(
                            gl_sampler_name,
                            sizeof(gl_sampler_name),
                            "je4_gl_sampler_%zu",
                            (size_t)uniform_var->m_value.ix);

                        (void)count;
                        assert(count > 0 && count < (int)sizeof(gl_sampler_name));

                        const auto location = glGetUniformLocation(shader_program, gl_sampler_name);
                        if (location != -1)
                            glUniform1i(location, (GLint)uniform_var->m_value.ix);
                    }
                    else
                    {
                        uniform_var->m_index =
                            shared_blob_data->get_built_in_location(uniform_var->m_name);
                    }

                    uniform_var = uniform_var->m_next;
                }

                auto* uniform_block = resource->m_raw_shader_data->m_custom_uniform_blocks;
                while (uniform_block)
                {
                    const GLuint uniform_block_loc = glGetUniformBlockIndex(shader_program, uniform_block->m_name);
                    if (GL_INVALID_INDEX != uniform_block_loc)
                        glUniformBlockBinding(shader_program, uniform_block_loc, 1 + (GLuint)uniform_block->m_specify_binding_place);

                    uniform_block = uniform_block->m_next;
                }

            }
            break;
        }
        case jegl_resource::type::TEXTURE:
        {
            GLuint texture;
            glGenTextures(1, &texture);

            GLint texture_aim_format = GL_RGBA;
            GLint texture_src_format = GL_RGBA;

            bool is_16bit = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::FLOAT16);
            bool is_depth = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::DEPTH);
            bool is_cube = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::CUBE);

            auto gl_texture_type = GL_TEXTURE_2D;

            GLenum last_texture_type;
            GLuint last_texture;
            bool need_restore_texture_state =
                context->get_last_binded_texture_and_type(&last_texture, &last_texture_type);

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
                            (GLsizei)resource->m_raw_texture_data->m_width,
                            (GLsizei)resource->m_raw_texture_data->m_height,
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
                        (GLsizei)resource->m_raw_texture_data->m_width,
                        (GLsizei)resource->m_raw_texture_data->m_height,
                        0,
                        GL_DEPTH_COMPONENT,
                        GL_UNSIGNED_INT,
                        NULL);
            }
            else
            {
                // Depth texture do not use color format
                switch (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
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
                            (GLsizei)resource->m_raw_texture_data->m_width,
                            (GLsizei)resource->m_raw_texture_data->m_height,
                            0, texture_src_format,
                            is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                            resource->m_raw_texture_data->m_pixels);
                    }
                }
                else
                {
                    glTexImage2D(gl_texture_type,
                        0, texture_aim_format,
                        (GLsizei)resource->m_raw_texture_data->m_width,
                        (GLsizei)resource->m_raw_texture_data->m_height,
                        0, texture_src_format,
                        is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                        resource->m_raw_texture_data->m_pixels);
                }
            }

            resource->m_handle.m_uint1 = texture;
            resource->m_handle.m_uint2 = (uint32_t)resource->m_raw_texture_data->m_format;
            static_assert(std::is_same<decltype(resource->m_handle.m_uint2), uint32_t>::value);

            if (need_restore_texture_state)
            {
                context->bind_texture_to_last_pass(
                    last_texture_type,
                    last_texture);
            }

            break;
        }
        case jegl_resource::type::VERTEX:
        {
            GLuint vao, vbo, ebo;
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER,
                resource->m_raw_vertex_data->m_vertex_length,
                resource->m_raw_vertex_data->m_vertexs,
                GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                resource->m_raw_vertex_data->m_index_count * sizeof(uint32_t),
                resource->m_raw_vertex_data->m_indexs,
                GL_STATIC_DRAW);

            size_t offset = 0;
            for (unsigned int i = 0; i < (unsigned int)resource->m_raw_vertex_data->m_format_count; i++)
            {
                size_t format_size;

                glEnableVertexAttribArray(i);

                switch (resource->m_raw_vertex_data->m_formats[i].m_type)
                {
                case jegl_vertex::data_type::FLOAT32:
                    format_size = sizeof(float);
                    glVertexAttribPointer(i, (GLint)resource->m_raw_vertex_data->m_formats[i].m_count,
                        GL_FLOAT, GL_FALSE,
                        (GLsizei)(resource->m_raw_vertex_data->m_data_size_per_point),
                        (void*)offset);
                    break;
                case jegl_vertex::data_type::INT32:
                    format_size = sizeof(int);
                    glVertexAttribIPointer(i, (GLint)resource->m_raw_vertex_data->m_formats[i].m_count,
                        GL_INT,
                        (GLsizei)(resource->m_raw_vertex_data->m_data_size_per_point),
                        (void*)offset);
                    break;
                default:
                    jeecs::debug::logfatal("Bad vertex data type.");
                    break;
                }

                offset += format_size * resource->m_raw_vertex_data->m_formats[i].m_count;
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
            vertex_data->m_method = DRAW_METHODS[(size_t)resource->m_raw_vertex_data->m_type];
            vertex_data->m_pointcount = (GLsizei)resource->m_raw_vertex_data->m_index_count;

            resource->m_handle.m_ptr = vertex_data;
            break;
        }
        case jegl_resource::type::FRAMEBUF:
        {
            GLuint fbo;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            bool already_has_attached_depth = false;

            GLenum attachment = GL_COLOR_ATTACHMENT0;

            jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
                    resource->m_raw_framebuf_data->m_output_attachments));

            for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
            {
                jegl_resource* frame_texture = attachments[i]->resource();
                assert(nullptr != frame_texture && nullptr != frame_texture->m_raw_texture_data);

                jegl_using_resource(frame_texture);

                GLenum using_attachment = attachment;
                GLenum buffer_texture_type = GL_TEXTURE_2D;

                if (0 != (frame_texture->m_raw_texture_data->m_format & jegl_texture::format::DEPTH))
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
                    frame_texture->m_handle.m_uint1,
                    0);
            }
            std::vector<GLuint> glattachments;
            for (GLenum attachment_index = GL_COLOR_ATTACHMENT0; attachment_index < attachment; ++attachment_index)
                glattachments.push_back(attachment_index);

            glDrawBuffers((GLsizei)glattachments.size(), glattachments.data());

            resource->m_handle.m_uint1 = fbo;

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
                jeecs::debug::logerr("Framebuffer(%p) not complete, status: %d.", resource, (int)status);
            break;
        }
        case jegl_resource::type::UNIFORMBUF:
        {
            resource->m_handle.m_ptr = new jegl_gl3_uniformbuf(resource);
            break;
        }
        default:
            jeecs::debug::logerr("Unknown resource type when initing resource(%p), please check.", resource);
            break;
        }
    }
    void _gl_update_depth_test_method(jegl_gl3_context* ctx, jegl_shader::depth_test_method mode)
    {
        assert(mode != jegl_shader::depth_test_method::INVALID);
        if (ctx->ACTIVE_DEPTH_MODE != mode)
        {
            ctx->ACTIVE_DEPTH_MODE = mode;

            if (mode == jegl_shader::depth_test_method::OFF)
                glDisable(GL_DEPTH_TEST);
            else
            {
                glEnable(GL_DEPTH_TEST);
                switch (mode)
                {
                case jegl_shader::depth_test_method::NEVER:
                    glDepthFunc(GL_NEVER);
                    break;
                case jegl_shader::depth_test_method::LESS:
                    glDepthFunc(GL_LESS);
                    break;
                case jegl_shader::depth_test_method::EQUAL:
                    glDepthFunc(GL_EQUAL);
                    break;
                case jegl_shader::depth_test_method::LESS_EQUAL:
                    glDepthFunc(GL_LEQUAL);
                    break;
                case jegl_shader::depth_test_method::GREATER:
                    glDepthFunc(GL_GREATER);
                    break;
                case jegl_shader::depth_test_method::NOT_EQUAL:
                    glDepthFunc(GL_NOTEQUAL);
                    break;
                case jegl_shader::depth_test_method::GREATER_EQUAL:
                    glDepthFunc(GL_GEQUAL);
                    break;
                case jegl_shader::depth_test_method::ALWAYS:
                    glDepthFunc(GL_ALWAYS);
                    break;
                default:
                    jeecs::debug::logerr("Invalid depth test method.");
                    break;
                }
            } // end else
        }
    }
    void _gl_update_depth_mask_method(jegl_gl3_context* ctx, jegl_shader::depth_mask_method mode)
    {
        assert(mode != jegl_shader::depth_mask_method::INVALID);
        if (ctx->ACTIVE_MASK_MODE != mode)
        {
            ctx->ACTIVE_MASK_MODE = mode;

            if (mode == jegl_shader::depth_mask_method::ENABLE)
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);
        }
    }
    void _gl_update_blend_equation_method(
        jegl_gl3_context* ctx, jegl_shader::blend_equation equation)
    {
        assert(equation != jegl_shader::blend_equation::INVALID);
        if (ctx->ACTIVE_BLEND_EQUATION != equation)
        {
            ctx->ACTIVE_BLEND_EQUATION = equation;
            switch (equation)
            {
            case jegl_shader::blend_equation::ADD:
                glBlendEquation(GL_FUNC_ADD);
                break;
            case jegl_shader::blend_equation::SUBTRACT:
                glBlendEquation(GL_FUNC_SUBTRACT);
                break;
            case jegl_shader::blend_equation::REVERSE_SUBTRACT:
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                break;
            case jegl_shader::blend_equation::MIN:
                glBlendEquation(GL_MIN);
                break;
            case jegl_shader::blend_equation::MAX:
                glBlendEquation(GL_MAX);
                break;
            default:
                jeecs::debug::logerr("Invalid blend equation method.");
                break;
            }
        }
    }
    void _gl_update_blend_mode_method(
        jegl_gl3_context* ctx, jegl_shader::blend_method src_mode, jegl_shader::blend_method dst_mode)
    {
        assert(src_mode != jegl_shader::blend_method::INVALID && dst_mode != jegl_shader::blend_method::INVALID);
        if (ctx->ACTIVE_BLEND_SRC_MODE != src_mode || ctx->ACTIVE_BLEND_DST_MODE != dst_mode)
        {
            ctx->ACTIVE_BLEND_SRC_MODE = src_mode;
            ctx->ACTIVE_BLEND_DST_MODE = dst_mode;

            if (src_mode == jegl_shader::blend_method::ONE && dst_mode == jegl_shader::blend_method::ZERO)
                glDisable(GL_BLEND);
            else
            {
                GLenum src_factor, dst_factor;
                switch (src_mode)
                {
                case jegl_shader::blend_method::ZERO:
                    src_factor = GL_ZERO;
                    break;
                case jegl_shader::blend_method::ONE:
                    src_factor = GL_ONE;
                    break;
                case jegl_shader::blend_method::SRC_COLOR:
                    src_factor = GL_SRC_COLOR;
                    break;
                case jegl_shader::blend_method::SRC_ALPHA:
                    src_factor = GL_SRC_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                    src_factor = GL_ONE_MINUS_SRC_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                    src_factor = GL_ONE_MINUS_SRC_COLOR;
                    break;
                case jegl_shader::blend_method::DST_COLOR:
                    src_factor = GL_DST_COLOR;
                    break;
                case jegl_shader::blend_method::DST_ALPHA:
                    src_factor = GL_DST_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                    src_factor = GL_ONE_MINUS_DST_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                    src_factor = GL_ONE_MINUS_DST_COLOR;
                    break;
                default:
                    jeecs::debug::logerr("Invalid blend src method.");
                    src_factor = GL_ONE;
                    break;
                }
                switch (dst_mode)
                {
                case jegl_shader::blend_method::ZERO:
                    dst_factor = GL_ZERO;
                    break;
                case jegl_shader::blend_method::ONE:
                    dst_factor = GL_ONE;
                    break;
                case jegl_shader::blend_method::SRC_COLOR:
                    dst_factor = GL_SRC_COLOR;
                    break;
                case jegl_shader::blend_method::SRC_ALPHA:
                    dst_factor = GL_SRC_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                    dst_factor = GL_ONE_MINUS_SRC_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                    dst_factor = GL_ONE_MINUS_SRC_COLOR;
                    break;
                case jegl_shader::blend_method::DST_COLOR:
                    dst_factor = GL_DST_COLOR;
                    break;
                case jegl_shader::blend_method::DST_ALPHA:
                    dst_factor = GL_DST_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                    dst_factor = GL_ONE_MINUS_DST_ALPHA;
                    break;
                case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                    dst_factor = GL_ONE_MINUS_DST_COLOR;
                    break;
                default:
                    jeecs::debug::logerr("Invalid blend dst method.");
                    dst_factor = GL_ZERO;
                    break;
                }
                glEnable(GL_BLEND);
                glBlendFunc(src_factor, dst_factor);
            }
        }
    }
    void _gl_update_cull_mode_method(jegl_gl3_context* ctx, jegl_shader::cull_mode mode)
    {
        assert(mode != jegl_shader::cull_mode::INVALID);
        if (ctx->ACTIVE_CULL_MODE != mode)
        {
            ctx->ACTIVE_CULL_MODE = mode;

            if (mode == jegl_shader::cull_mode::NONE)
                glDisable(GL_CULL_FACE);
            else
            {
                glEnable(GL_CULL_FACE);
                switch (mode)
                {
                case jegl_shader::cull_mode::FRONT:
                    glCullFace(GL_FRONT);
                    break;
                case jegl_shader::cull_mode::BACK:
                    glCullFace(GL_BACK);
                    break;
                default:
                    jeecs::debug::logerr("Invalid culling mode.");
                    break;
                }
            }
        }
    }

    bool _gl_using_shader_program(jegl_gl3_context* context, jegl_resource* resource)
    {
        jegl_gl3_shader* shader_instance =
            reinterpret_cast<jegl_gl3_shader*>(resource->m_handle.m_ptr);

        if (context->current_active_shader_may_null == shader_instance)
            return shader_instance != nullptr;

        context->current_active_shader_may_null = shader_instance;
        if (shader_instance == nullptr)
            return false;
    
        if (resource->m_raw_shader_data != nullptr)
        {
            _gl_update_depth_test_method(context, resource->m_raw_shader_data->m_depth_test);
            _gl_update_depth_mask_method(context, resource->m_raw_shader_data->m_depth_mask);
            _gl_update_blend_equation_method(context, resource->m_raw_shader_data->m_blend_equation);
            _gl_update_blend_mode_method(
                context,
                resource->m_raw_shader_data->m_blend_src_mode,
                resource->m_raw_shader_data->m_blend_dst_mode);
            _gl_update_cull_mode_method(context, resource->m_raw_shader_data->m_cull_mode);
        }
        _gl_bind_shader_samplers(shader_instance);
        glUseProgram(shader_instance->m_shared_blob_data->m_shader_program_instance);

        return true;
    }

    void gl_using_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            break;
        case jegl_resource::type::TEXTURE:
            if (resource->m_modified)
            {
                if (resource->m_raw_texture_data != nullptr)
                {
                    resource->m_modified = false;

                    GLenum last_texture_type;
                    GLuint last_texture;
                    bool need_restore_texture_state =
                        context->get_last_binded_texture_and_type(&last_texture, &last_texture_type);

                    // Update texture's pixels, only normal pixel data will be updated.
                    glBindTexture(GL_TEXTURE_2D, (GLuint)resource->m_handle.m_uint1);

                    // Textures FORMAT & SIZE will not be changed.
                    bool is_16bit = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::FLOAT16);
                    glTexImage2D(
                        GL_TEXTURE_2D,
                        0,
                        GL_RGBA,
                        (GLsizei)resource->m_raw_texture_data->m_width,
                        (GLsizei)resource->m_raw_texture_data->m_height,
                        0,
                        GL_RGBA,
                        is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                        resource->m_raw_texture_data->m_pixels);

                    if (need_restore_texture_state)
                    {
                        context->bind_texture_to_last_pass(
                            last_texture_type,
                            last_texture);
                    }
                }
            }
            break;
        case jegl_resource::type::VERTEX:
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
        {
            if (resource->m_modified)
            {
                resource->m_modified = false;
                if (resource->m_raw_uniformbuf_data != nullptr)
                {
                    jegl_gl3_uniformbuf* ubuf =
                        std::launder(reinterpret_cast<jegl_gl3_uniformbuf*>(resource->m_handle.m_ptr));

                    assert(resource->m_raw_uniformbuf_data->m_update_length != 0);

                    glBindBuffer(GL_UNIFORM_BUFFER, ubuf->m_uniform_buffer_object);

                    assert(resource->m_raw_uniformbuf_data->m_update_length != 0);
                    glBufferSubData(GL_UNIFORM_BUFFER,
                        resource->m_raw_uniformbuf_data->m_update_begin_offset,
                        resource->m_raw_uniformbuf_data->m_update_length,
                        resource->m_raw_uniformbuf_data->m_buffer
                        + resource->m_raw_uniformbuf_data->m_update_begin_offset);
                }
            }

            break;
        }
        default:
            jeecs::debug::logerr("Unknown resource type(%d) when using when resource %p.", (int)resource->m_type, resource);
            break;
        }
    }

    void gl_close_resource(jegl_context::graphic_impl_context_t, jegl_resource* resource)
    {
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            delete reinterpret_cast<jegl_gl3_shader*>(resource->m_handle.m_ptr);
            break;
        }
        case jegl_resource::type::TEXTURE:
            glDeleteTextures(1, &resource->m_handle.m_uint1);
            break;
        case jegl_resource::type::VERTEX:
        {
            jegl3_vertex_data* vdata = std::launder(reinterpret_cast<jegl3_vertex_data*>(resource->m_handle.m_ptr));
            glDeleteVertexArrays(1, &vdata->m_vao);
            glDeleteBuffers(1, &vdata->m_vbo);
            glDeleteBuffers(1, &vdata->m_ebo);
            delete vdata;
            break;
        }
        case jegl_resource::type::FRAMEBUF:
            glDeleteFramebuffers(1, &resource->m_handle.m_uint1);
            break;
        case jegl_resource::type::UNIFORMBUF:
            glDeleteBuffers(1, &resource->m_handle.m_uint1);
            break;
        default:
            jeecs::debug::logerr("Unknown resource type when closing resource %p, please check.", resource);
            break;
        }
    }

    bool gl_bind_shader(jegl_context::graphic_impl_context_t context, jegl_resource* shader)
    {
        jegl_gl3_context* ctx = std::launder(reinterpret_cast<jegl_gl3_context*>(context));
        return _gl_using_shader_program(ctx, shader);
    }

    void gl_bind_uniform_buffer(jegl_context::graphic_impl_context_t, jegl_resource* uniformbuf)
    {
        jegl_gl3_uniformbuf* ubuf = 
            std::launder(reinterpret_cast<jegl_gl3_uniformbuf*>(uniformbuf->m_handle.m_ptr));

        glBindBufferRange(
            GL_UNIFORM_BUFFER,
            ubuf->m_binding_place,
            ubuf->m_uniform_buffer_object,
            0,
            ubuf->m_uniform_buffer_size);
    }

    void gl_bind_texture(jegl_context::graphic_impl_context_t ctx, jegl_resource* texture, size_t pass)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));

        if (0 != ((jegl_texture::format)texture->m_handle.m_uint2 & jegl_texture::format::CUBE))
            context->bind_texture_pass_impl(
                (GLint)pass, GL_TEXTURE_CUBE_MAP, (GLuint)texture->m_handle.m_uint1);
        else
            context->bind_texture_pass_impl(
                (GLint)pass, GL_TEXTURE_2D, (GLuint)texture->m_handle.m_uint1);
    }

    void gl_draw_vertex_with_shader(jegl_context::graphic_impl_context_t ctx, jegl_resource* vert)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));
        jegl3_vertex_data* vdata = std::launder(reinterpret_cast<jegl3_vertex_data*>(vert->m_handle.m_ptr));

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
        jegl_resource* framebuffer,
        const size_t(*viewport_xywh)[4],
        const float (*clear_color_rgba)[4],
        const float* clear_depth)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));

        // Reset current binded shader.
        context->current_active_shader_may_null = nullptr;

        if (nullptr == framebuffer)
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->m_handle.m_uint1);

        size_t x = 0, y = 0, w = 0, h = 0;
        if (viewport_xywh != nullptr)
        {
            auto& viewport = *viewport_xywh;
            x = viewport[0];
            y = viewport[1];
            w = viewport[2];
            h = viewport[3];
        }

        auto* framw_buffer_raw = framebuffer != nullptr
            ? framebuffer->m_raw_framebuf_data
            : nullptr;
        if (w == 0)
            w = framw_buffer_raw != nullptr
            ? framebuffer->m_raw_framebuf_data->m_width
            : context->RESOLUTION_WIDTH;
        if (h == 0)
            h = framw_buffer_raw != nullptr
            ? framebuffer->m_raw_framebuf_data->m_height
            : context->RESOLUTION_HEIGHT;

        glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
#ifdef JE_ENABLE_GL330_GAPI
        glDepthRange(0., 1.);
#else
        glDepthRangef(0., 1.);
#endif

        GLenum clear_mask = 0;
        if (clear_color_rgba != nullptr)
        {
            auto& color = *clear_color_rgba;
            glClearColor(color[0], color[1], color[2], color[3]);

            clear_mask |= GL_COLOR_BUFFER_BIT;
        }
        if (clear_depth != nullptr)
        {
            _gl_update_depth_mask_method(context, jegl_shader::depth_mask_method::ENABLE);
#ifdef JE_ENABLE_GL330_GAPI
            glClearDepthf(*clear_depth);
#else
            glClearDepthf(*clear_depth);
#endif
            clear_mask |= GL_DEPTH_BUFFER_BIT;
        }

        if (clear_mask != 0)
            glClear(clear_mask);
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

    write_to_apis->create_resource_blob_cache = gl_create_resource_blob;
    write_to_apis->close_resource_blob_cache = gl_close_resource_blob;

    write_to_apis->create_resource = gl_init_resource;
    write_to_apis->using_resource = gl_using_resource;
    write_to_apis->close_resource = gl_close_resource;

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
