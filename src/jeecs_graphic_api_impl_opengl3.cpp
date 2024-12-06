#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_GL330_GAPI) \
 || defined(JE_ENABLE_GLES300_GAPI)

#include "jeecs_imgui_backend_api.hpp"

#ifdef JE_ENABLE_GLES300_GAPI
#   ifdef __APPLE__
#       include <OpenGLES/ES3/gl.h>
#   else
#       include <GLES3/gl3.h>
#   endif
#else
#   include <GL/glew.h>
#endif

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   include "jeecs_graphic_api_interface_egl.hpp"
#else
#   include "jeecs_graphic_api_interface_glfw.hpp"
#endif // JE_GL_USE_EGL_INSTEAD_GLFW

// Here is low-level-graphic-api impl.
// OpenGL version.
namespace jeecs::graphic::api::gl3
{
    using jegl_gl3_context = basic_interface;

    /////////////////////////////////////////////////////////////////////////////////////

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
        case GL_DEBUG_SOURCE_API:             source_type = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_type = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: source_type = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     source_type = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     source_type = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           source_type = "Other"; break;
        }

        const char* msg_type = "UNKNOWN";
        int jelog_level = JE_LOG_INFO;
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR:               jelog_level = JE_LOG_ERROR; msg_type = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: jelog_level = JE_LOG_WARNING; msg_type = "Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  jelog_level = JE_LOG_ERROR; msg_type = "Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         jelog_level = JE_LOG_WARNING; msg_type = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         jelog_level = JE_LOG_WARNING; msg_type = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              jelog_level = JE_LOG_INFO; msg_type = "Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          jelog_level = JE_LOG_INFO; msg_type = "Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           jelog_level = JE_LOG_INFO; msg_type = "Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               jelog_level = JE_LOG_INFO; msg_type = "Other"; break;
        }

        switch (severity)
        {
        case GL_DEBUG_SEVERITY_HIGH:         jelog_level = JE_LOG_FATAL; break;
        case GL_DEBUG_SEVERITY_MEDIUM:; break;
        case GL_DEBUG_SEVERITY_LOW:; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:; break;
        }

        je_log(jelog_level, "(%d)%s-%s: %s", id, source_type, msg_type, message);
    }
#endif
    jegl_context::userdata_t gl_startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_gl3_context* context = nullptr;

        if (!reboot)
        {
            jeecs::debug::log("Graphic thread (OpenGL3) start!");
        }
        context =
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
            new egl();
#else
            new glfw(reboot
                ? glfw::HOLD
#   ifdef JE_ENABLE_GL330_GAPI
                : glfw::OPENGL330
#   else
                : glfw::OPENGLES300
#   endif
            );
#endif

        context->create_interface(gthread, config);

#if !defined(JE_GL_USE_EGL_INSTEAD_GLFW) && defined(JE_ENABLE_GL330_GAPI)
        if (auto glew_init_result = glewInit(); glew_init_result != GLEW_OK)
            jeecs::debug::logfatal("Failed to init glew: %s.", glewGetErrorString(glew_init_result));
#endif


#ifdef JE_ENABLE_GL330_GAPI
#   ifndef NDEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#   endif
#endif
        glEnable(GL_DEPTH_TEST);

        jegui_init_gl330(
            [](auto* res) {return (uint64_t)res->m_handle.m_uint1; },
            [](auto* res)
            {
                if (res->m_raw_shader_data != nullptr)
                {
                    for (size_t i = 0; i < res->m_raw_shader_data->m_sampler_count; ++i)
                    {
                        auto& sampler = res->m_raw_shader_data->m_sampler_methods[i];
                        for (size_t id = 0; id < sampler.m_pass_id_count; ++id)
                        {
                            glActiveTexture(GL_TEXTURE0 + sampler.m_pass_ids[id]);
                            switch (sampler.m_mag)
                            {
                            case jegl_shader::fliter_mode::LINEAR:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); break;
                            case jegl_shader::fliter_mode::NEAREST:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); break;
                            default:
                                abort();
                            }
                            switch (sampler.m_min)
                            {
                            case jegl_shader::fliter_mode::LINEAR:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                break;
                            case jegl_shader::fliter_mode::NEAREST:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                break;
                            default:
                                abort();
                            }
                            switch (sampler.m_uwrap)
                            {
                            case jegl_shader::wrap_mode::CLAMP:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
                            case jegl_shader::wrap_mode::REPEAT:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
                            default:
                                abort();
                            }
                            switch (sampler.m_vwrap)
                            {
                            case jegl_shader::wrap_mode::CLAMP:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
                            case jegl_shader::wrap_mode::REPEAT:
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
                            default:
                                abort();
                            }
                        }
                    }
                }
            },
            context->interface_handle(),
                reboot);

        return context;
    }

    jegl_graphic_api::update_action gl_pre_update(jegl_context::userdata_t ctx)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));
        context->swap_for_opengl();
        
        switch (context->update())
        {
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_graphic_api::update_action::STOP;
        case basic_interface::update_result::PAUSE:
            return jegl_graphic_api::update_action::SKIP;
        case basic_interface::update_result::RESIZE:
            /*fallthrough*/
        case basic_interface::update_result::NORMAL:
            return jegl_graphic_api::update_action::CONTINUE;
        default:
            abort();
        }
    }

    jegl_graphic_api::update_action gl_commit_update(jegl_context::userdata_t)
    {
        jegui_update_gl330();

        // 将绘制命令异步地提交给GPU
        glFlush();        

        return jegl_graphic_api::update_action::CONTINUE;
    }
    void gl_pre_shutdown(jegl_context*, jegl_context::userdata_t, bool)
    {
    }
    void gl_shutdown(jegl_context*, jegl_context::userdata_t userdata, bool reboot)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(userdata));

        if (!reboot)
            jeecs::debug::log("Graphic thread (OpenGL3) shutdown!");

        jegui_shutdown_gl330(reboot);

        context->shutdown(reboot);

        delete context;
    }

    uint32_t gl_get_uniform_location(jegl_context::userdata_t, jegl_resource* shader, const char* name)
    {
        auto loc = glGetUniformLocation(shader->m_handle.m_uint1, name);
        if (loc == -1)
            return jeecs::typing::INVALID_UINT32;
        return (uint32_t)loc;
    }

    void gl_set_uniform(jegl_context::userdata_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val)
    {
        if (location == jeecs::typing::INVALID_UINT32)
            return;

        switch (type)
        {
        case jegl_shader::INT:
        {
            glUniform1i((GLint)location, *reinterpret_cast<const int*>(val)); 
            break;
        }
        case jegl_shader::INT2:
        {
            const int * vptr = reinterpret_cast<const int*>(val);
            glUniform2i((GLint)location, vptr[0], vptr[1]); 
            break;
        }
        case jegl_shader::INT3:
        {
            const int* vptr = reinterpret_cast<const int*>(val);
            glUniform3i((GLint)location, vptr[0], vptr[1], vptr[2]); 
            break;
        }
        case jegl_shader::INT4:
        {
            const int* vptr = reinterpret_cast<const int*>(val);
            glUniform4i((GLint)location, vptr[0], vptr[1], vptr[2], vptr[3]); 
            break;
        }
        case jegl_shader::FLOAT:
        {
            glUniform1f((GLint)location, *reinterpret_cast<const float*>(val));
            break;
        }
        case jegl_shader::FLOAT2:
        {
            const float* vptr = reinterpret_cast<const float*>(val);
            glUniform2f((GLint)location, vptr[0], vptr[1]);
            break;
        }
        case jegl_shader::FLOAT3:
        {
            const float* vptr = reinterpret_cast<const float*>(val);
            glUniform3f((GLint)location, vptr[0], vptr[1], vptr[2]);
            break;
        }
        case jegl_shader::FLOAT4:
        {
            const float* vptr = reinterpret_cast<const float*>(val);
            glUniform4f((GLint)location, vptr[0], vptr[1], vptr[2], vptr[3]);
            break;
        }
        case jegl_shader::FLOAT4X4:   
        {
            glUniformMatrix4fv((GLint)location, 1, false, reinterpret_cast<const float*>(val));
            break;
        }
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set."); break;
        }
    }

    struct jegl3_vertex_data
    {
        GLuint m_vao;
        GLuint m_vbo;
        GLenum m_method;
        GLsizei m_pointcount;
    };

    struct jegl3_resource_blob
    {
        GLuint m_vertex_shader;
        GLuint m_fragment_shader;
    };

    jegl_resource_blob gl_create_resource_blob(jegl_context::userdata_t ctx, jegl_resource* resource)
    {
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            jegl3_resource_blob* blob = new jegl3_resource_blob;

            std::string vertex_src
#ifdef JE_ENABLE_GL330_GAPI
                = "#version 330 core\n\n"
#else
                = "#version 300 es\n\n"
#endif
                ;
            std::string fragment_src = vertex_src
#ifdef JE_ENABLE_GL330_GAPI
                //;
#else
                + "precision highp float;\n\n"
#endif
                ;

            vertex_src += resource->m_raw_shader_data->m_vertex_glsl_src;
            const char* vertex_src_cstrptr = vertex_src.c_str();

            blob->m_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(blob->m_vertex_shader, 1, &vertex_src_cstrptr, NULL);
            glCompileShader(blob->m_vertex_shader);

            fragment_src += resource->m_raw_shader_data->m_fragment_glsl_src;
            const char* fragment_src_cstrptr = fragment_src.c_str();

            blob->m_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(blob->m_fragment_shader, 1, &fragment_src_cstrptr, NULL);
            glCompileShader(blob->m_fragment_shader);

            // Check this program is acceptable?
            GLint compile_result;
            GLint errmsg_len;
            GLint errmsg_written_len;

            bool shader_program_has_error = false;
            std::string error_informations;

            glGetShaderiv(blob->m_vertex_shader, GL_COMPILE_STATUS, &compile_result);
            if (compile_result != GL_TRUE)
            {
                shader_program_has_error = true;
                glGetShaderiv(blob->m_vertex_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
                if (errmsg_len > 0)
                {
                    std::vector<char> errmsg_buf(errmsg_len + 1);
                    glGetShaderInfoLog(blob->m_vertex_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                    error_informations = error_informations + "In vertex shader: \n" + errmsg_buf.data();
                }
            }

            glGetShaderiv(blob->m_fragment_shader, GL_COMPILE_STATUS, &compile_result);
            if (compile_result != GL_TRUE)
            {
                shader_program_has_error = true;
                glGetShaderiv(blob->m_fragment_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
                if (errmsg_len > 0)
                {
                    std::vector<char> errmsg_buf(errmsg_len + 1);
                    glGetShaderInfoLog(blob->m_fragment_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                    error_informations = error_informations + "In fragment shader: \n" + errmsg_buf.data();
                }
            }

            if (shader_program_has_error)
            {
                delete blob;
                jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                    resource, error_informations.c_str());
                return nullptr;
            }
            else
            {
                return blob;
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

    void gl_close_resource_blob(jegl_context::userdata_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
        {
            auto* shader_blob = std::launder(reinterpret_cast<jegl3_resource_blob*>(blob));
            glDeleteShader(shader_blob->m_vertex_shader);
            glDeleteShader(shader_blob->m_fragment_shader);
            delete shader_blob;
        }
    }

    void gl_init_resource(jegl_context::userdata_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        assert(resource->m_custom_resource != nullptr);

        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            if (blob != nullptr)
            {
                auto* shader_blob = std::launder(reinterpret_cast<jegl3_resource_blob*>(blob));
                GLuint shader_program = glCreateProgram();
                glAttachShader(shader_program, shader_blob->m_vertex_shader);
                glAttachShader(shader_program, shader_blob->m_fragment_shader);
                glLinkProgram(shader_program);

                // Check this program is acceptable?
                GLint link_result;
                GLint errmsg_len;
                GLint errmsg_written_len;

                glGetProgramiv(shader_program, GL_LINK_STATUS, &link_result);
                if (link_result != GL_TRUE)
                {
                    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &errmsg_len);
                    std::vector<char> errmsg_buf(errmsg_len + 1, '\0');
                    if (errmsg_len > 0)
                        glGetProgramInfoLog(shader_program, errmsg_len, &errmsg_written_len, errmsg_buf.data());

                    jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                        resource, errmsg_buf.data());
                }
                else
                {
                    glUseProgram(shader_program);
                    resource->m_handle.m_uint1 = shader_program;
                    auto& builtin_uniforms = resource->m_raw_shader_data->m_builtin_uniforms;

                    builtin_uniforms.m_builtin_uniform_m = gl_get_uniform_location(ctx, resource, "JOYENGINE_TRANS_M");
                    builtin_uniforms.m_builtin_uniform_mv = gl_get_uniform_location(ctx, resource, "JOYENGINE_TRANS_MV");
                    builtin_uniforms.m_builtin_uniform_mvp = gl_get_uniform_location(ctx, resource, "JOYENGINE_TRANS_MVP");

                    builtin_uniforms.m_builtin_uniform_tiling = gl_get_uniform_location(ctx, resource, "JOYENGINE_TEXTURE_TILING");
                    builtin_uniforms.m_builtin_uniform_offset = gl_get_uniform_location(ctx, resource, "JOYENGINE_TEXTURE_OFFSET");

                    builtin_uniforms.m_builtin_uniform_light2d_resolution = gl_get_uniform_location(ctx, resource, "JOYENGINE_LIGHT2D_RESOLUTION");
                    builtin_uniforms.m_builtin_uniform_light2d_decay = gl_get_uniform_location(ctx, resource, "JOYENGINE_LIGHT2D_DECAY");

                    // ATTENTION: 注意，以下参数特殊shader可能挪作他用
                    builtin_uniforms.m_builtin_uniform_local_scale = gl_get_uniform_location(ctx, resource, "JOYENGINE_LOCAL_SCALE");
                    builtin_uniforms.m_builtin_uniform_color = gl_get_uniform_location(ctx, resource, "JOYENGINE_MAIN_COLOR");

                    auto* uniform_var = resource->m_raw_shader_data->m_custom_uniforms;
                    while (uniform_var)
                    {
                        uniform_var->m_index = gl_get_uniform_location(ctx, resource, uniform_var->m_name);
                        uniform_var = uniform_var->m_next;
                    }

                    auto* uniform_block = resource->m_raw_shader_data->m_custom_uniform_blocks;
                    while (uniform_block)
                    {
                        GLuint uniform_block_loc = glGetUniformBlockIndex(shader_program, uniform_block->m_name);
                        glUniformBlockBinding(shader_program, uniform_block_loc, (GLuint)uniform_block->m_specify_binding_place);
                        uniform_block = uniform_block->m_next;
                    }
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

            glBindTexture(gl_texture_type, texture);

            assert(GL_TEXTURE_2D == gl_texture_type);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            const GLenum jegl_texture_cube_map_ways[] = {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
            };

            if (is_depth)
            {
                if (is_16bit)
                    jeecs::debug::logerr("Depth texture cannot use 16bit.");

                if (is_cube)
                {
                    for (auto way : jegl_texture_cube_map_ways)
                    {
                        glTexImage2D(way, 0, GL_DEPTH_COMPONENT,
                            (GLsizei)resource->m_raw_texture_data->m_width,
                            (GLsizei)resource->m_raw_texture_data->m_height,
                            0,
                            GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                            NULL
                        );
                    }
                }
                else
                    glTexImage2D(gl_texture_type, 0, GL_DEPTH_COMPONENT,
                        (GLsizei)resource->m_raw_texture_data->m_width,
                        (GLsizei)resource->m_raw_texture_data->m_height,
                        0,
                        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                        NULL
                    );
            }
            else
            {
                // Depth texture do not use color format
                switch (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
                {
                case jegl_texture::format::MONO:
                    texture_src_format = GL_LUMINANCE;
                    texture_aim_format = is_16bit ? GL_R16F : GL_LUMINANCE; break;
                case jegl_texture::format::RGBA:
                    texture_src_format = GL_RGBA;
                    texture_aim_format = is_16bit ? GL_RGBA16F : GL_RGBA; break;
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
            break;
        }
        case jegl_resource::type::VERTEX:
        {
            GLuint vao, vbo;
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER,
                resource->m_raw_vertex_data->m_point_count * resource->m_raw_vertex_data->m_data_size_per_point,
                resource->m_raw_vertex_data->m_vertex_datas,
                GL_STATIC_DRAW);

            size_t offset = 0;
            for (unsigned int i = 0; i < (unsigned int)resource->m_raw_vertex_data->m_format_count; i++)
            {
                size_t format_size;
                GLenum format_type;
                
                glEnableVertexAttribArray(i);

                switch (resource->m_raw_vertex_data->m_vertex_formats[i].m_type)
                {
                case jegl_vertex::data_type::FLOAT32:
                    format_size = sizeof(float);
                    glVertexAttribPointer(i, (GLint)resource->m_raw_vertex_data->m_vertex_formats[i].m_count,
                        GL_FLOAT, GL_FALSE,
                        (GLsizei)(resource->m_raw_vertex_data->m_data_size_per_point),
                        (void*)offset);
                    break;
                case jegl_vertex::data_type::INT32:
                    format_size = sizeof(int);
                    glVertexAttribIPointer(i, (GLint)resource->m_raw_vertex_data->m_vertex_formats[i].m_count,
                        GL_INT,
                        (GLsizei)(resource->m_raw_vertex_data->m_data_size_per_point),
                        (void*)offset);
                    break;
                default:
                    jeecs::debug::logfatal("Bad vertex data type.");
                    break;
                }

                offset += format_size * resource->m_raw_vertex_data->m_vertex_formats[i].m_count;
            }

            const static GLenum DRAW_METHODS[] = {
                GL_LINES,
                GL_LINE_STRIP,
                GL_TRIANGLES,
                GL_TRIANGLE_STRIP,
            };

            auto* vertex_data = new jegl3_vertex_data;
            vertex_data->m_vao = vao;
            vertex_data->m_vbo = vbo;
            vertex_data->m_method = DRAW_METHODS[(size_t)resource->m_raw_vertex_data->m_type];
            vertex_data->m_pointcount = (GLsizei)resource->m_raw_vertex_data->m_point_count;

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
                std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture>*>(
                    resource->m_raw_framebuf_data->m_output_attachments));

            for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
            {
                jegl_resource* frame_texture = attachments[i]->resouce();
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

                glFramebufferTexture2D(GL_FRAMEBUFFER, using_attachment, buffer_texture_type, frame_texture->m_handle.m_uint1, 0);
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
            GLuint uniform_buffer_object;
            glGenBuffers(1, &uniform_buffer_object);
            glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer_object);
            glBufferData(GL_UNIFORM_BUFFER,
                resource->m_raw_uniformbuf_data->m_buffer_size,
                NULL, GL_DYNAMIC_COPY); // 预分配空间

            glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)resource->m_raw_uniformbuf_data->m_buffer_binding_place,
                uniform_buffer_object, 0, resource->m_raw_uniformbuf_data->m_buffer_size);

            resource->m_handle.m_uint1 = uniform_buffer_object;
            break;
        }
        default:
            jeecs::debug::logerr("Unknown resource type when initing resource(%p), please check.", resource);
            break;
        }

    }

    thread_local static jegl_shader::depth_test_method  ACTIVE_DEPTH_MODE = jegl_shader::depth_test_method::INVALID;
    thread_local static jegl_shader::depth_mask_method  ACTIVE_MASK_MODE = jegl_shader::depth_mask_method::INVALID;
    thread_local static jegl_shader::blend_method       ACTIVE_BLEND_SRC_MODE = jegl_shader::blend_method::INVALID;
    thread_local static jegl_shader::blend_method       ACTIVE_BLEND_DST_MODE = jegl_shader::blend_method::INVALID;
    thread_local static jegl_shader::cull_mode          ACTIVE_CULL_MODE = jegl_shader::cull_mode::INVALID;

    void _gl_update_depth_test_method(jegl_shader::depth_test_method mode)
    {
        assert(mode != jegl_shader::depth_test_method::INVALID);
        if (ACTIVE_DEPTH_MODE != mode)
        {
            ACTIVE_DEPTH_MODE = mode;

            if (mode == jegl_shader::depth_test_method::OFF)
                glDisable(GL_DEPTH_TEST);
            else
            {
                glEnable(GL_DEPTH_TEST);
                switch (mode)
                {
                case jegl_shader::depth_test_method::NEVER:    glDepthFunc(GL_NEVER); break;
                case jegl_shader::depth_test_method::LESS:     glDepthFunc(GL_LESS); break;
                case jegl_shader::depth_test_method::EQUAL:    glDepthFunc(GL_EQUAL); break;
                case jegl_shader::depth_test_method::LESS_EQUAL: glDepthFunc(GL_LEQUAL); break;
                case jegl_shader::depth_test_method::GREATER:  glDepthFunc(GL_GREATER); break;
                case jegl_shader::depth_test_method::NOT_EQUAL: glDepthFunc(GL_NOTEQUAL); break;
                case jegl_shader::depth_test_method::GREATER_EQUAL: glDepthFunc(GL_GEQUAL); break;
                case jegl_shader::depth_test_method::ALWAYS:   glDepthFunc(GL_ALWAYS); break;
                default:
                    jeecs::debug::logerr("Invalid depth test method.");
                    break;
                }
            }// end else
        }
    }
    void _gl_update_depth_mask_method(jegl_shader::depth_mask_method mode)
    {
        assert(mode != jegl_shader::depth_mask_method::INVALID);
        if (ACTIVE_MASK_MODE != mode)
        {
            ACTIVE_MASK_MODE = mode;

            if (mode == jegl_shader::depth_mask_method::ENABLE)
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);
        }
    }
    void _gl_update_blend_mode_method(jegl_shader::blend_method src_mode, jegl_shader::blend_method dst_mode)
    {
        assert(src_mode != jegl_shader::blend_method::INVALID && dst_mode != jegl_shader::blend_method::INVALID);
        if (ACTIVE_BLEND_SRC_MODE != src_mode || ACTIVE_BLEND_DST_MODE != dst_mode)
        {
            ACTIVE_BLEND_SRC_MODE = src_mode;
            ACTIVE_BLEND_DST_MODE = dst_mode;

            if (src_mode == jegl_shader::blend_method::ONE && dst_mode == jegl_shader::blend_method::ZERO)
                glDisable(GL_BLEND);
            else
            {
                GLenum src_factor, dst_factor;
                switch (src_mode)
                {
                case jegl_shader::blend_method::ZERO: src_factor = GL_ZERO; break;
                case jegl_shader::blend_method::ONE: src_factor = GL_ONE; break;
                case jegl_shader::blend_method::SRC_COLOR: src_factor = GL_SRC_COLOR; break;
                case jegl_shader::blend_method::SRC_ALPHA: src_factor = GL_SRC_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA: src_factor = GL_ONE_MINUS_SRC_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR: src_factor = GL_ONE_MINUS_SRC_COLOR; break;
                case jegl_shader::blend_method::DST_COLOR: src_factor = GL_DST_COLOR; break;
                case jegl_shader::blend_method::DST_ALPHA: src_factor = GL_DST_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA: src_factor = GL_ONE_MINUS_DST_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_DST_COLOR: src_factor = GL_ONE_MINUS_DST_COLOR; break;
                default: jeecs::debug::logerr("Invalid blend src method."); break;
                }
                switch (dst_mode)
                {
                case jegl_shader::blend_method::ZERO: dst_factor = GL_ZERO; break;
                case jegl_shader::blend_method::ONE: dst_factor = GL_ONE; break;
                case jegl_shader::blend_method::SRC_COLOR: dst_factor = GL_SRC_COLOR; break;
                case jegl_shader::blend_method::SRC_ALPHA: dst_factor = GL_SRC_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA: dst_factor = GL_ONE_MINUS_SRC_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR: dst_factor = GL_ONE_MINUS_SRC_COLOR; break;
                case jegl_shader::blend_method::DST_COLOR: dst_factor = GL_DST_COLOR; break;
                case jegl_shader::blend_method::DST_ALPHA: dst_factor = GL_DST_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA: dst_factor = GL_ONE_MINUS_DST_ALPHA; break;
                case jegl_shader::blend_method::ONE_MINUS_DST_COLOR: dst_factor = GL_ONE_MINUS_DST_COLOR; break;
                default: jeecs::debug::logerr("Invalid blend src method."); break;
                }
                glEnable(GL_BLEND);
                glBlendFunc(src_factor, dst_factor);
            }

        }
    }
    void _gl_update_cull_mode_method(jegl_shader::cull_mode mode)
    {
        assert(mode != jegl_shader::cull_mode::INVALID);
        if (ACTIVE_CULL_MODE != mode)
        {
            ACTIVE_CULL_MODE = mode;

            if (mode == jegl_shader::cull_mode::NONE)
                glDisable(GL_CULL_FACE);
            else
            {
                glEnable(GL_CULL_FACE);
                switch (mode)
                {
                case jegl_shader::cull_mode::FRONT:
                    glCullFace(GL_FRONT); break;
                case jegl_shader::cull_mode::BACK:
                    glCullFace(GL_BACK); break;
                default:
                    jeecs::debug::logerr("Invalid culling mode.");
                    break;
                }
            }

        }
    }

    void _gl_using_shader_program(jegl_resource* resource)
    {
        if (resource->m_raw_shader_data != nullptr)
        {
            _gl_update_depth_test_method(resource->m_raw_shader_data->m_depth_test);
            _gl_update_depth_mask_method(resource->m_raw_shader_data->m_depth_mask);
            _gl_update_blend_mode_method(
                resource->m_raw_shader_data->m_blend_src_mode,
                resource->m_raw_shader_data->m_blend_dst_mode);
            _gl_update_cull_mode_method(resource->m_raw_shader_data->m_cull_mode);

            for (size_t i = 0; i < resource->m_raw_shader_data->m_sampler_count; ++i)
            {
                auto& sampler = resource->m_raw_shader_data->m_sampler_methods[i];
                for (size_t id = 0; id < sampler.m_pass_id_count; ++id)
                {
                    glActiveTexture(GL_TEXTURE0 + sampler.m_pass_ids[id]);
                    switch (sampler.m_mag)
                    {
                    case jegl_shader::fliter_mode::LINEAR:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); break;
                    case jegl_shader::fliter_mode::NEAREST:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); break;
                    default:
                        abort();
                    }
                    switch (sampler.m_min)
                    {
                    case jegl_shader::fliter_mode::LINEAR:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        break;
                    case jegl_shader::fliter_mode::NEAREST:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        break;
                    default:
                        abort();
                    }
                    switch (sampler.m_uwrap)
                    {
                    case jegl_shader::wrap_mode::CLAMP:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
                    case jegl_shader::wrap_mode::REPEAT:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
                    default:
                        abort();
                    }
                    switch (sampler.m_vwrap)
                    {
                    case jegl_shader::wrap_mode::CLAMP:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
                    case jegl_shader::wrap_mode::REPEAT:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
                    default:
                        abort();
                    }
                }
            }
        }
        glUseProgram(resource->m_handle.m_uint1);
    }
    void gl_close_resource(jegl_context::userdata_t ctx, jegl_resource* resource);

    void gl_using_resource(jegl_context::userdata_t ctx, jegl_resource* resource)
    {
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            break;
        case jegl_resource::type::TEXTURE:
            if (resource->m_raw_texture_data != nullptr)
            {
                if (resource->m_raw_texture_data->m_modified)
                {
                    resource->m_raw_texture_data->m_modified = false;
                    // Modified, free current resource id, reload one.
                    gl_close_resource(ctx, resource);
                    resource->m_handle.m_uint1 = 0;
                    gl_init_resource(ctx, nullptr, resource);
                }
            }
            break;
        case jegl_resource::type::VERTEX:
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
        {
            glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)resource->m_handle.m_uint1);

            if (resource->m_raw_uniformbuf_data != nullptr)
            {
                glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)resource->m_raw_uniformbuf_data->m_buffer_binding_place,
                    (GLuint)resource->m_handle.m_uint1, 0, resource->m_raw_uniformbuf_data->m_buffer_size);

                if (resource->m_raw_uniformbuf_data->m_update_length != 0)
                {
                    glBufferSubData(GL_UNIFORM_BUFFER,
                        resource->m_raw_uniformbuf_data->m_update_begin_offset,
                        resource->m_raw_uniformbuf_data->m_update_length,
                        resource->m_raw_uniformbuf_data->m_buffer + resource->m_raw_uniformbuf_data->m_update_begin_offset);

                    resource->m_raw_uniformbuf_data->m_update_begin_offset = 0;
                    resource->m_raw_uniformbuf_data->m_update_length = 0;
                }
            }
            break;
        }
        default:
            jeecs::debug::logerr("Unknown resource type(%d) when using when resource %p.", (int)resource->m_type, resource);
            break;
        }
    }

    void gl_close_resource(jegl_context::userdata_t, jegl_resource* resource)
    {
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            glDeleteProgram(resource->m_handle.m_uint1);
            break;
        case jegl_resource::type::TEXTURE:
            glDeleteTextures(1, &resource->m_handle.m_uint1);
            break;
        case jegl_resource::type::VERTEX:
        {
            jegl3_vertex_data* vdata = std::launder(reinterpret_cast<jegl3_vertex_data*>(resource->m_handle.m_ptr));
            glDeleteVertexArrays(1, &vdata->m_vao);
            glDeleteBuffers(1, &vdata->m_vbo);
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

    void gl_bind_shader(jegl_context::userdata_t, jegl_resource* shader)
    {
        _gl_using_shader_program(shader);
    }

    void gl_bind_uniform_buffer(jegl_context::userdata_t, jegl_resource* uniformbuf)
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)uniformbuf->m_raw_uniformbuf_data->m_buffer_binding_place,
            (GLuint)uniformbuf->m_handle.m_uint1, 0, uniformbuf->m_raw_uniformbuf_data->m_buffer_size);
    }

    void gl_bind_texture(jegl_context::userdata_t, jegl_resource* texture, size_t pass)
    {
        glActiveTexture(GL_TEXTURE0 + (GLint)pass);
        if (0 != ((jegl_texture::format)texture->m_handle.m_uint2 & jegl_texture::format::CUBE))
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture->m_handle.m_uint1);
        else
            glBindTexture(GL_TEXTURE_2D, texture->m_handle.m_uint1);
    }

    void gl_draw_vertex_with_shader(jegl_context::userdata_t, jegl_resource* vert)
    {
        jegl3_vertex_data* vdata = std::launder(reinterpret_cast<jegl3_vertex_data*>(vert->m_handle.m_ptr));

        glBindVertexArray(vdata->m_vao);

        if (vdata != nullptr)
            glDrawArrays(vdata->m_method, 0, vdata->m_pointcount);
    }

    void gl_set_rend_to_framebuffer(jegl_context::userdata_t ctx, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(ctx));

        if (nullptr == framebuffer)
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->m_handle.m_uint1);

        auto* framw_buffer_raw = framebuffer != nullptr ? framebuffer->m_raw_framebuf_data : nullptr;
        if (w == 0)
            w = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_width : context->m_interface_width;
        if (h == 0)
            h = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_height : context->m_interface_height;
        glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
    }
    void gl_clear_framebuffer_color(jegl_context::userdata_t, float color[4])
    {
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void gl_clear_framebuffer_depth(jegl_context::userdata_t)
    {
        _gl_update_depth_mask_method(jegl_shader::depth_mask_method::ENABLE);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::gl3;

    write_to_apis->init = gl_startup;
    write_to_apis->pre_shutdown = gl_pre_shutdown;
    write_to_apis->post_shutdown = gl_shutdown;

    write_to_apis->pre_update = gl_pre_update;
    write_to_apis->commit_update = gl_commit_update;

    write_to_apis->create_blob = gl_create_resource_blob;
    write_to_apis->close_blob = gl_close_resource_blob;

    write_to_apis->create_resource = gl_init_resource;
    write_to_apis->using_resource = gl_using_resource;
    write_to_apis->close_resource = gl_close_resource;

    write_to_apis->bind_uniform_buffer = gl_bind_uniform_buffer;
    write_to_apis->bind_texture = gl_bind_texture;
    write_to_apis->bind_shader = gl_bind_shader;
    write_to_apis->draw_vertex = gl_draw_vertex_with_shader;

    write_to_apis->bind_framebuf = gl_set_rend_to_framebuffer;
    write_to_apis->clear_color = gl_clear_framebuffer_color;
    write_to_apis->clear_depth = gl_clear_framebuffer_depth;

    write_to_apis->set_uniform = gl_set_uniform;
}
#else
void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GL330 not available.");
}
#endif
