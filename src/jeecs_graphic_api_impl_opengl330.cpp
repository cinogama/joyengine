#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_GL330_GAPI) \
 || defined(JE_ENABLE_GLES320_GAPI)

#include "jeecs_imgui_backend_api.hpp"


#ifdef JE_ENABLE_GLES320_GAPI
#   ifdef __APPLE__
#       include <OpenGLES/ES3/gl.h>
#   else
#       include <GLES3/gl3.h>
#       include <EGL/egl.h>
#       include <EGL/eglext.h>
#   endif
#else
#   include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Here is low-level-graphic-api impl.
// OpenGL version.
namespace jeecs::graphic::api::gl330
{
    struct jegl_gl3_context
    {
        size_t WINDOWS_SIZE_WIDTH = 0;
        size_t WINDOWS_SIZE_HEIGHT = 0;
        const char* WINDOWS_TITLE = nullptr;
        GLFWwindow* WINDOWS_HANDLE = nullptr;
    };

    void glfw_callback_windows_size_changed(GLFWwindow* fw, int x, int y)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(glfwGetWindowUserPointer(fw)));

        context->WINDOWS_SIZE_WIDTH = (size_t)x;
        context->WINDOWS_SIZE_HEIGHT = (size_t)y;

        je_io_set_windowsize(x, y);
    }

    void glfw_callback_mouse_pos_changed(GLFWwindow* fw, double x, double y)
    {
        je_io_set_mousepos(0, (int)x, (int)y);
    }

    void glfw_callback_mouse_key_clicked(GLFWwindow* fw, int key, int state, int mod)
    {
        switch (key)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            je_io_set_keystate(jeecs::input::keycode::MOUSE_L_BUTTION, state); break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            je_io_set_keystate(jeecs::input::keycode::MOUSE_M_BUTTION, state); break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            je_io_set_keystate(jeecs::input::keycode::MOUSE_R_BUTTION, state); break;
        default:
            // do nothing.
            break;
        }
    }

    void glfw_callback_mouse_scroll_changed(GLFWwindow* fw, double xoffset, double yoffset)
    {
        je_io_set_wheel(0, je_io_wheel(0) + (float)yoffset);
    }

    void glfw_callback_keyboard_stage_changed(GLFWwindow* fw, int key, int w, int stage, int v)
    {
        assert(GLFW_KEY_A == 'A');
        assert(GLFW_KEY_0 == '0');
        switch (key)
        {
        case GLFW_KEY_LEFT_SHIFT:
            je_io_set_keystate(jeecs::input::keycode::L_SHIFT, stage); break;
        case GLFW_KEY_LEFT_ALT:
            je_io_set_keystate(jeecs::input::keycode::L_ALT, stage); break;
        case GLFW_KEY_LEFT_CONTROL:
            je_io_set_keystate(jeecs::input::keycode::L_CTRL, stage); break;
        case GLFW_KEY_TAB:
            je_io_set_keystate(jeecs::input::keycode::TAB, stage); break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            je_io_set_keystate(jeecs::input::keycode::ENTER, stage); break;
        case GLFW_KEY_ESCAPE:
            je_io_set_keystate(jeecs::input::keycode::ESC, stage); break;
        case GLFW_KEY_BACKSPACE:
            je_io_set_keystate(jeecs::input::keycode::BACKSPACE, stage); break;
        default:
            je_io_set_keystate((jeecs::input::keycode)key, stage); break;
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

    jegl_thread::custom_thread_data_t gl_startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_gl3_context* context = new jegl_gl3_context;
       
        return context;
    }

    bool gl_pre_update(jegl_thread::custom_thread_data_t ctx)
    {
        return false;
    }

    bool gl_update(jegl_thread::custom_thread_data_t ctx)
    {
        return false;
    }

    bool gl_lateupdate(jegl_thread::custom_thread_data_t ctx)
    {
        return false;
    }

    void gl_shutdown(jegl_thread*, jegl_thread::custom_thread_data_t userdata, bool reboot)
    {
        jegl_gl3_context* context = std::launder(reinterpret_cast<jegl_gl3_context*>(userdata));
        jegui_shutdown_basic(reboot);
        if (!reboot)
            jeecs::debug::log("Graphic thread (OpenGL330) shutdown!");

        delete context;
    }

    uint32_t gl_get_uniform_location(jegl_thread::custom_thread_data_t, jegl_resource* shader, const char* name)
    {
        return 0;
    }

    void gl_set_uniform(jegl_thread::custom_thread_data_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val)
    {
       
    }

    struct gl3_vertex_data
    {
        GLuint m_vao;
        GLuint m_vbo;
        GLenum m_method;
        GLsizei m_pointcount;
    };

    struct gl_resource_blob
    {
        GLuint m_vertex_shader;
        GLuint m_fragment_shader;

        JECS_DISABLE_MOVE_AND_COPY(gl_resource_blob);

        gl_resource_blob() = default;
        ~gl_resource_blob()
        {
            glDeleteShader(m_vertex_shader);
            glDeleteShader(m_fragment_shader);
        }
    };

    jegl_resource_blob gl_create_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        return nullptr;
    }

    void gl_close_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob)
    {
    }

    void gl_init_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
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
                        /* if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
                             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                         else
                             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);*/
                        break;
                    case jegl_shader::fliter_mode::NEAREST:
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        /*if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                        else
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);*/
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
    void gl_close_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource);
    inline void _gl_using_texture2d(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
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
        if (0 != ((jegl_texture::format)resource->m_handle.m_uint2 & jegl_texture::format::CUBE))
            glBindTexture(GL_TEXTURE_CUBE_MAP, resource->m_handle.m_uint1);
        else
            glBindTexture(GL_TEXTURE_2D, resource->m_handle.m_uint1);
    }

    void gl_using_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
    }

    void gl_close_resource(jegl_thread::custom_thread_data_t, jegl_resource* resource)
    {
    }

    void gl_bind_texture(jegl_thread::custom_thread_data_t, jegl_resource* texture, size_t pass)
    {
    }

    void gl_draw_vertex_with_shader(jegl_thread::custom_thread_data_t, jegl_resource* vert)
    {
      
    }

    void gl_set_rend_to_framebuffer(jegl_thread::custom_thread_data_t ctx, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
    {
        
    }
    void gl_clear_framebuffer_color(jegl_thread::custom_thread_data_t, float color[4])
    {

    }

    void gl_clear_framebuffer_depth(jegl_thread::custom_thread_data_t)
    {

    }
}

void jegl_using_opengl330_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::gl330;

    write_to_apis->init_interface = gl_startup;
    write_to_apis->shutdown_interface = gl_shutdown;

    write_to_apis->pre_update_interface = gl_pre_update;
    write_to_apis->update_interface = gl_update;
    write_to_apis->late_update_interface = gl_lateupdate;

    write_to_apis->create_resource_blob = gl_create_resource_blob;
    write_to_apis->close_resource_blob = gl_close_resource_blob;

    write_to_apis->init_resource = gl_init_resource;
    write_to_apis->using_resource = gl_using_resource;
    write_to_apis->close_resource = gl_close_resource;

    write_to_apis->draw_vertex = gl_draw_vertex_with_shader;
    write_to_apis->bind_texture = gl_bind_texture;

    write_to_apis->set_rend_buffer = gl_set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer_color = gl_clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = gl_clear_framebuffer_depth;

    write_to_apis->set_uniform = gl_set_uniform;
}
#else
void jegl_using_opengl330_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("GL330 not available.");
}
#endif
