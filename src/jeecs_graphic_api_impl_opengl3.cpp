#define JE_IMPL
#include "jeecs.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "jeecs_imgui_api.hpp"

// Here is low-level-graphic-api impl.
// OpenGL version.

constexpr size_t MAX_MOUSE_BUTTON_COUNT = 16;
constexpr size_t MAX_KEYBOARD_BUTTON_COUNT = 65535;

thread_local size_t WINDOWS_SIZE_WIDTH = 0;
thread_local size_t WINDOWS_SIZE_HEIGHT = 0;
thread_local const char* WINDOWS_TITLE = nullptr;
thread_local GLFWwindow* WINDOWS_HANDLE = nullptr;
thread_local std::thread::id GRAPHIC_THREAD_ID;

thread_local float MOUSE_POS_X = 0;
thread_local float MOUSE_POS_Y = 0;
thread_local int MOUSE_KEY_STATE[MAX_MOUSE_BUTTON_COUNT];
thread_local float MOUSE_SCROLL_X_COUNT = 0.f;
thread_local float MOUSE_SCROLL_Y_COUNT = 0.f;
thread_local int KEYBOARD_STATE[MAX_KEYBOARD_BUTTON_COUNT];


void glfw_callback_windows_size_changed(GLFWwindow* fw, int x, int y)
{
    WINDOWS_SIZE_WIDTH = (size_t)x;
    WINDOWS_SIZE_HEIGHT = (size_t)y;

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
        je_io_set_keystate(jeecs::input::keycode::ENTER, stage); break;
    case GLFW_KEY_ESCAPE:
        je_io_set_keystate(jeecs::input::keycode::ESC, stage); break;
    case GLFW_KEY_BACKSPACE:
        je_io_set_keystate(jeecs::input::keycode::BACKSPACE, stage); break;
    default:
        je_io_set_keystate((jeecs::input::keycode)key, stage); break;
    }

}

void gl_prepare()
{
    if (!glfwInit())
        jeecs::debug::logfatal("Failed to init glfw.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

}

jegl_graphic_api::custom_interface_info_t gl_startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
{
    jeecs::debug::log("Graphic thread start!");

    GRAPHIC_THREAD_ID = std::this_thread::get_id();

    WINDOWS_SIZE_WIDTH = config->m_windows_width ? config->m_windows_width : config->m_resolution_x;
    WINDOWS_SIZE_HEIGHT = config->m_windows_height ? config->m_windows_height : config->m_resolution_y;

    je_io_set_windowsize((int)WINDOWS_SIZE_WIDTH, (int)WINDOWS_SIZE_HEIGHT);

    WINDOWS_TITLE = config->m_title ? config->m_title : WINDOWS_TITLE;

    WINDOWS_HANDLE = glfwCreateWindow((int)WINDOWS_SIZE_WIDTH, (int)WINDOWS_SIZE_HEIGHT, WINDOWS_TITLE,
        config->m_fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);

    glfwMakeContextCurrent(WINDOWS_HANDLE);
    glfwSetWindowSizeCallback(WINDOWS_HANDLE, glfw_callback_windows_size_changed);
    glfwSetCursorPosCallback(WINDOWS_HANDLE, glfw_callback_mouse_pos_changed);
    glfwSetMouseButtonCallback(WINDOWS_HANDLE, glfw_callback_mouse_key_clicked);
    glfwSetScrollCallback(WINDOWS_HANDLE, glfw_callback_mouse_scroll_changed);
    glfwSetKeyCallback(WINDOWS_HANDLE, glfw_callback_keyboard_stage_changed);
    glfwSwapInterval(0);

    if (auto glew_init_result = glewInit(); glew_init_result != GLEW_OK)
        jeecs::debug::logfatal("Failed to init glew: %s.", glewGetErrorString(glew_init_result));

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    jegui_init(WINDOWS_HANDLE, reboot);

    return nullptr;
}

bool gl_pre_update(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    glfwSwapBuffers(WINDOWS_HANDLE);

    return true;
}

bool gl_update(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    glfwPollEvents();
    int mouse_lock_x, mouse_lock_y;
    if (je_io_should_lock_mouse(&mouse_lock_x, &mouse_lock_y))
        glfwSetCursorPos(WINDOWS_HANDLE, mouse_lock_x, mouse_lock_y);

    int window_width, window_height;
    if (je_io_should_update_windowsize(&window_width, &window_height))
        glfwSetWindowSize(WINDOWS_HANDLE, window_width, window_height);

    if (glfwWindowShouldClose(WINDOWS_HANDLE))
    {
        jeecs::debug::logwarn("Graphic interface has been closed, graphic thread will exit soon.");
        return false;
    }
    return true;
}

thread_local double _last_swap_buffer_tpoint = 0.;

bool gl_lateupdate(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    jegui_update();
    return true;
}

void gl_shutdown(jegl_thread*, jegl_graphic_api::custom_interface_info_t, bool reboot)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    jeecs::debug::log("Graphic thread shutdown!");

    jegui_shutdown(reboot);
    glfwDestroyWindow(WINDOWS_HANDLE);
}

void gl_finish()
{
    glfwTerminate();
}

int gl_get_uniform_location(jegl_resource* shader, const char* name)
{
    return (int)glGetUniformLocation(shader->m_uint1, name);
}

void gl_set_uniform(jegl_resource*, int location, jegl_shader::uniform_type type, const void* val)
{
    switch (type)
    {
    case jegl_shader::INT:
        glUniform1i((GLuint)location, *(const int*)val); break;
    case jegl_shader::FLOAT:
        glUniform1f((GLuint)location, *(const float*)val); break;
    case jegl_shader::FLOAT2:
        glUniform2f((GLuint)location
            , ((const jeecs::math::vec2*)val)->x
            , ((const jeecs::math::vec2*)val)->y); break;
    case jegl_shader::FLOAT3:
        glUniform3f((GLuint)location
            , ((const jeecs::math::vec3*)val)->x
            , ((const jeecs::math::vec3*)val)->y
            , ((const jeecs::math::vec3*)val)->z); break;
    case jegl_shader::FLOAT4:
        glUniform4f((GLuint)location
            , ((const jeecs::math::vec4*)val)->x
            , ((const jeecs::math::vec4*)val)->y
            , ((const jeecs::math::vec4*)val)->z
            , ((const jeecs::math::vec4*)val)->w); break;
    case jegl_shader::FLOAT4X4:
        glUniformMatrix4fv((GLuint)location, 1, false, (float*)val); break;
    default:
        jeecs::debug::logerr("Unknown uniform variable type to set."); break;
    }
}

void gl_init_resource(jegl_thread* gthread, jegl_resource* resource)
{
    if (resource->m_type == jegl_resource::type::SHADER)
    {
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &resource->m_raw_shader_data->m_vertex_glsl_src, NULL);
        glCompileShader(vertex_shader);

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &resource->m_raw_shader_data->m_fragment_glsl_src, NULL);
        glCompileShader(fragment_shader);

        GLuint shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        // Check this program is acceptable?
        GLint errmsg_len;
        GLint errmsg_written_len;
        glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &errmsg_len);
        if (errmsg_len > 0)
        {
            std::string error_informations;
            glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(vertex_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                error_informations = error_informations + "In vertex shader: \n" + errmsg_buf.data();
            }
            glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(fragment_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                error_informations = error_informations + "In fragment shader: \n" + errmsg_buf.data();
            }
            glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetProgramInfoLog(shader_program, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                error_informations = error_informations + "In shader program link: \n" + errmsg_buf.data();
            }
            jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                resource, error_informations.c_str());
        }
        else
        {
            resource->m_uint1 = shader_program;
            auto& builtin_uniforms = resource->m_raw_shader_data->m_builtin_uniforms;

            builtin_uniforms.m_builtin_uniform_m = gl_get_uniform_location(resource, "JOYENGINE_TRANS_M");
            builtin_uniforms.m_builtin_uniform_v = gl_get_uniform_location(resource, "JOYENGINE_TRANS_V");
            builtin_uniforms.m_builtin_uniform_p = gl_get_uniform_location(resource, "JOYENGINE_TRANS_P");

            builtin_uniforms.m_builtin_uniform_mvp = gl_get_uniform_location(resource, "JOYENGINE_TRANS_MVP");
            builtin_uniforms.m_builtin_uniform_mv = gl_get_uniform_location(resource, "JOYENGINE_TRANS_MV");
            builtin_uniforms.m_builtin_uniform_vp = gl_get_uniform_location(resource, "JOYENGINE_TRANS_VP");

            builtin_uniforms.m_builtin_uniform_tiling = gl_get_uniform_location(resource, "JOYENGINE_TEXTURE_TILING");
            builtin_uniforms.m_builtin_uniform_offset = gl_get_uniform_location(resource, "JOYENGINE_TEXTURE_OFFSET");

            builtin_uniforms.m_builtin_uniform_shadow2d_resolution = gl_get_uniform_location(resource, "JOYENGINE_SHADOW2D_RESOLUTION");
            builtin_uniforms.m_builtin_uniform_light2d_decay = gl_get_uniform_location(resource, "JOYENGINE_LIGHT2D_DECAY");

            // ATTENTION: 注意，以下参数特殊shader可能挪作他用
            builtin_uniforms.m_builtin_uniform_local_scale = gl_get_uniform_location(resource, "JOYENGINE_LOCAL_SCALE");
            builtin_uniforms.m_builtin_uniform_color = gl_get_uniform_location(resource, "JOYENGINE_MAIN_COLOR");
            
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }
    else if (resource->m_type == jegl_resource::type::TEXTURE)
    {
        GLuint texture;
        glGenTextures(1, &texture);

        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        bool is_16bit = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::COLOR16);
        bool is_depth = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::DEPTH);
        bool is_cube = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::CUBE);
        static_assert(jegl_texture::texture_format::MSAA_MASK == 0xFF00);
        uint8_t msaa_level = (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::MSAA_MASK) >> 8;

        auto gl_texture_type = GL_TEXTURE_2D;

        if (msaa_level != 0)
            gl_texture_type = GL_TEXTURE_2D_MULTISAMPLE;

        glBindTexture(gl_texture_type, texture);

        if (msaa_level == 0)
        {
            // Apply wrap setting
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::texture_sampling::WRAP_X_METHOD_MASK)
            {
            case jegl_texture::texture_sampling::CLAMP_EDGE_X:
                glTexParameteri(gl_texture_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
            case jegl_texture::texture_sampling::REPEAT_X:
                glTexParameteri(gl_texture_type, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
            default:
                jeecs::debug::logerr("Unknown texture wrap method in x(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::texture_sampling::WRAP_Y_METHOD_MASK)
            {
            case jegl_texture::texture_sampling::CLAMP_EDGE_Y:
                glTexParameteri(gl_texture_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
            case jegl_texture::texture_sampling::REPEAT_Y:
                glTexParameteri(gl_texture_type, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
            default:
                jeecs::debug::logerr("Unknown texture wrap method in y(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }

            // Apply fliter setting
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::texture_sampling::MIN_FILTER_MASK)
            {
            case jegl_texture::texture_sampling::MIN_LINEAR:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR); break;
            case jegl_texture::texture_sampling::MIN_NEAREST:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
            case jegl_texture::texture_sampling::MIN_LINEAR_LINEAR_MIP:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
            case jegl_texture::texture_sampling::MIN_NEAREST_LINEAR_MIP:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); break;
            case jegl_texture::texture_sampling::MIN_LINEAR_NEAREST_MIP:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); break;
            case jegl_texture::texture_sampling::MIN_NEAREST_NEAREST_MIP:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); break;
            default:
                jeecs::debug::logerr("Unknown texture min filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::texture_sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::texture_sampling::MAG_LINEAR:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR); break;
            case jegl_texture::texture_sampling::MAG_NEAREST:
                glTexParameteri(gl_texture_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST); break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
        }
        if (is_depth)
        {
            if (is_16bit)
                jeecs::debug::logerr("Depth texture cannot use 16bit.");

            if (is_cube)
                assert(0); // todo;
            else if (msaa_level != 0)
                glTexImage2DMultisample(gl_texture_type,
                    msaa_level,
                    GL_DEPTH_COMPONENT,
                    (GLsizei)resource->m_raw_texture_data->m_width,
                    (GLsizei)resource->m_raw_texture_data->m_height,
                    GL_FALSE);
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
            switch (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::COLOR_DEPTH_MASK)
            {
            case jegl_texture::texture_format::MONO:
                texture_src_format = GL_LUMINANCE;
                texture_aim_format = is_16bit ? GL_LUMINANCE16F_ARB : GL_LUMINANCE; break;
            case jegl_texture::texture_format::RGB:
                texture_src_format = GL_RGB;
                texture_aim_format = is_16bit ? GL_RGB16F : GL_RGB; break;
            case jegl_texture::texture_format::RGBA:
                texture_src_format = GL_RGBA;
                texture_aim_format = is_16bit ? GL_RGBA16F : GL_RGBA; break;
            default:
                jeecs::debug::logerr("Unknown texture raw-data format.");
            }

            if (is_cube)
                assert(0); // todo;
            if (msaa_level != 0)
                glTexImage2DMultisample(gl_texture_type,
                    msaa_level,
                    texture_aim_format,
                    (GLsizei)resource->m_raw_texture_data->m_width,
                    (GLsizei)resource->m_raw_texture_data->m_height,
                    GL_FALSE);
            else
                glTexImage2D(gl_texture_type,
                    0, texture_aim_format,
                    (GLsizei)resource->m_raw_texture_data->m_width,
                    (GLsizei)resource->m_raw_texture_data->m_height,
                    0, texture_src_format,
                    is_16bit ? GL_FLOAT : GL_UNSIGNED_BYTE,
                    resource->m_raw_texture_data->m_pixels);

            glGenerateMipmap(gl_texture_type);
        }
        resource->m_uint1 = texture;
    }
    else if (resource->m_type == jegl_resource::type::VERTEX)
    {
        GLuint vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            resource->m_raw_vertex_data->m_point_count * resource->m_raw_vertex_data->m_data_count_per_point * sizeof(float),
            resource->m_raw_vertex_data->m_vertex_datas,
            GL_STATIC_DRAW);

        size_t offset = 0;
        for (unsigned int i = 0; i < (unsigned int)resource->m_raw_vertex_data->m_format_count; i++)
        {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, (GLint)resource->m_raw_vertex_data->m_vertex_formats[i],
                GL_FLOAT, GL_FALSE,
                (GLsizei)(resource->m_raw_vertex_data->m_data_count_per_point * sizeof(float)),
                (void*)(offset * sizeof(float)));

            offset += resource->m_raw_vertex_data->m_vertex_formats[i];
        }
        resource->m_uint1 = vao;
        resource->m_uint2 = vbo;
    }
    else if (resource->m_type == jegl_resource::type::FRAMEBUF)
    {
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        bool already_has_attached_depth = false;

        GLenum attachment = GL_COLOR_ATTACHMENT0;
        for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
        {
            jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                (jeecs::basic::resource<jeecs::graphic::texture>*)resource->m_raw_framebuf_data->m_output_attachments;

            jegl_resource* frame_texture = attachments[i]->resouce();
            assert(nullptr != frame_texture && nullptr != frame_texture->m_raw_texture_data);

            jegl_using_resource(frame_texture);

            GLenum using_attachment = attachment;
            GLenum buffer_texture_type = GL_TEXTURE_2D;

            if (0 != (frame_texture->m_raw_texture_data->m_format & jegl_texture::texture_format::DEPTH))
            {
                if (already_has_attached_depth)
                    jeecs::debug::logerr("Framebuffer(%p) attach depth buffer repeatedly.", resource);
                already_has_attached_depth = true;
                using_attachment = GL_DEPTH_ATTACHMENT;
            }
            else
                ++attachment;

            if (0 != (frame_texture->m_raw_texture_data->m_format & jegl_texture::texture_format::MSAA_MASK))
                // Texture using msaa
                buffer_texture_type = GL_TEXTURE_2D_MULTISAMPLE;

            glFramebufferTexture2D(GL_FRAMEBUFFER, using_attachment, buffer_texture_type, frame_texture->m_uint1, 0);
        }
        std::vector<GLuint> attachments;
        for (GLenum attachment_index = GL_COLOR_ATTACHMENT0; attachment_index < attachment; ++attachment_index)
            attachments.push_back(attachment_index);

        glDrawBuffers((GLsizei)attachments.size(), attachments.data());

        resource->m_uint1 = fbo;
    }
    else if (resource->m_type == jegl_resource::type::UNIFORMBUF)
    {
        GLuint uniform_buffer_object;
        glGenBuffers(1, &uniform_buffer_object);
        glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer_object);
        glBufferData(GL_UNIFORM_BUFFER,
            resource->m_raw_uniformbuf_data->m_buffer_size,
            NULL, GL_DYNAMIC_COPY); // 预分配空间

        glBindBufferRange(GL_UNIFORM_BUFFER, (GLuint)resource->m_raw_uniformbuf_data->m_buffer_binding_place,
            uniform_buffer_object, 0, resource->m_raw_uniformbuf_data->m_buffer_size);

        resource->m_uint1 = uniform_buffer_object;
    }
    else
        jeecs::debug::logerr("Unknown resource type when initing resource(%p), please check.", resource);
}

thread_local static jegl_shader::depth_test_method  ACTIVE_DEPTH_MODE = jegl_shader::depth_test_method::INVALID;
thread_local static jegl_shader::depth_mask_method  ACTIVE_MASK_MODE = jegl_shader::depth_mask_method::INVALID;
thread_local static jegl_shader::alpha_test_method  ACTIVE_ALPHA_MODE = jegl_shader::alpha_test_method::INVALID;
thread_local static jegl_shader::blend_method       ACTIVE_BLEND_SRC_MODE = jegl_shader::blend_method::INVALID;
thread_local static jegl_shader::blend_method       ACTIVE_BLEND_DST_MODE = jegl_shader::blend_method::INVALID;
thread_local static jegl_shader::cull_mode          ACTIVE_CULL_MODE = jegl_shader::cull_mode::INVALID;

inline void _gl_update_depth_test_method(jegl_shader::depth_test_method mode)
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
inline void _gl_update_depth_mask_method(jegl_shader::depth_mask_method mode)
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

inline void _gl_update_blend_mode_method(jegl_shader::blend_method src_mode, jegl_shader::blend_method dst_mode)
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
            case jegl_shader::blend_method::CONST_COLOR: src_factor = GL_CONSTANT_COLOR; break;
            case jegl_shader::blend_method::CONST_ALPHA: src_factor = GL_CONSTANT_ALPHA; break;
            case jegl_shader::blend_method::ONE_MINUS_CONST_COLOR: src_factor = GL_ONE_MINUS_CONSTANT_COLOR; break;
            case jegl_shader::blend_method::ONE_MINUS_CONST_ALPHA: src_factor = GL_ONE_MINUS_CONSTANT_ALPHA; break;
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
            case jegl_shader::blend_method::CONST_COLOR: dst_factor = GL_CONSTANT_COLOR; break;
            case jegl_shader::blend_method::CONST_ALPHA: dst_factor = GL_CONSTANT_ALPHA; break;
            case jegl_shader::blend_method::ONE_MINUS_CONST_COLOR: dst_factor = GL_ONE_MINUS_CONSTANT_COLOR; break;
            case jegl_shader::blend_method::ONE_MINUS_CONST_ALPHA: dst_factor = GL_ONE_MINUS_CONSTANT_ALPHA; break;
            default: jeecs::debug::logerr("Invalid blend src method."); break;
            }
            glEnable(GL_BLEND);
            glBlendFunc(src_factor, dst_factor);
        }

    }
}
inline void _gl_update_cull_mode_method(jegl_shader::cull_mode mode)
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
            case jegl_shader::cull_mode::ALL:
                glCullFace(GL_FRONT_AND_BACK); break;
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
inline void _gl_update_shader_state(jegl_shader* shader)
{
    _gl_update_depth_test_method(shader->m_depth_test);
    _gl_update_depth_mask_method(shader->m_depth_mask);
    _gl_update_blend_mode_method(shader->m_blend_src_mode, shader->m_blend_dst_mode);
    _gl_update_cull_mode_method(shader->m_cull_mode);
}

inline void _gl_using_shader_program(jegl_resource* resource)
{
    // TODO; Move update shader uniforms here.
    assert(resource->m_raw_shader_data);
    _gl_update_shader_state(resource->m_raw_shader_data);
    glUseProgram(resource->m_uint1);
}

inline void _gl_using_texture2d(jegl_thread* gthread, jegl_resource* resource)
{
    assert(resource->m_raw_texture_data);

    if (resource->m_raw_texture_data->m_modified)
    {
        resource->m_raw_texture_data->m_modified = false;
        // Modified, free current resource id, reload one.
        glDeleteTextures(1, &resource->m_uint1);

        gl_init_resource(gthread, resource);
    }

    if (0 != (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::MSAA_MASK))
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, resource->m_uint1);
    else if (0 != (resource->m_raw_texture_data->m_format & jegl_texture::texture_format::CUBE))
        glBindTexture(GL_TEXTURE_CUBE_MAP, resource->m_uint1);
    else
        glBindTexture(GL_TEXTURE_2D, resource->m_uint1);
}

void gl_using_resource(jegl_thread* gthread, jegl_resource* resource)
{
    if (resource->m_type == jegl_resource::type::SHADER)
        _gl_using_shader_program(resource);
    else if (resource->m_type == jegl_resource::type::TEXTURE)
        _gl_using_texture2d(gthread, resource);
    else if (resource->m_type == jegl_resource::type::VERTEX)
        glBindVertexArray(resource->m_uint1);
    else if (resource->m_type == jegl_resource::type::FRAMEBUF)
        glBindFramebuffer(GL_FRAMEBUFFER, resource->m_uint1);
    else if (resource->m_type == jegl_resource::type::UNIFORMBUF)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, resource->m_uint1);

        auto* uniform_buffer = resource->m_raw_uniformbuf_data;
        if (uniform_buffer->m_update_length != 0)
        {
            glBufferSubData(GL_UNIFORM_BUFFER,
                resource->m_raw_uniformbuf_data->m_update_begin_offset,
                resource->m_raw_uniformbuf_data->m_update_length,
                resource->m_raw_uniformbuf_data->m_buffer + resource->m_raw_uniformbuf_data->m_update_begin_offset);

            resource->m_raw_uniformbuf_data->m_update_begin_offset = 0;
            resource->m_raw_uniformbuf_data->m_update_length = 0;
        }
    }
    else
        jeecs::debug::logerr("Unknown resource type(%d) when using when resource %p.", (int)resource->m_type, resource);
}

void gl_close_resource(jegl_thread* gthread, jegl_resource* resource)
{
    if (resource->m_type == jegl_resource::type::SHADER)
        glDeleteProgram(resource->m_uint1);
    else if (resource->m_type == jegl_resource::type::TEXTURE)
        glDeleteTextures(1, &resource->m_uint1);
    else if (resource->m_type == jegl_resource::type::VERTEX)
    {
        glDeleteVertexArrays(1, &resource->m_uint1);
        glDeleteBuffers(1, &resource->m_uint2);
    }
    else if (resource->m_type == jegl_resource::type::FRAMEBUF)
        glDeleteFramebuffers(1, &resource->m_uint1);
    else if (resource->m_type == jegl_resource::type::UNIFORMBUF)
        glDeleteBuffers(1, &resource->m_uint1);
    else
        jeecs::debug::logerr("Unknown resource type when closing resource %p, please check.", resource);
}

void gl_bind_texture(jegl_resource* texture, size_t pass)
{
    glActiveTexture(GL_TEXTURE0 + (GLint)pass);
    jegl_using_resource(texture);
}

void gl_draw_vertex_with_shader(jegl_resource* vert)
{
    const static GLenum DRAW_METHODS[] = {
        GL_LINES,
        GL_LINE_LOOP,
        GL_LINE_STRIP,
        GL_TRIANGLES,
        GL_TRIANGLE_STRIP,
        GL_QUADS };

    jegl_using_resource(vert);
    glDrawArrays(DRAW_METHODS[vert->m_raw_vertex_data->m_type], 0, (GLsizei)vert->m_raw_vertex_data->m_point_count);
}

void gl_set_rend_to_framebuffer(jegl_thread*, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    if (nullptr == framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else
        jegl_using_resource(framebuffer);

    glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
}
void gl_clear_framebuffer_color(jegl_thread*, jegl_resource* framebuffer)
{
    if (nullptr == framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else
        jegl_using_resource(framebuffer);

    glClear(GL_COLOR_BUFFER_BIT);
}
void gl_clear_framebuffer(jegl_thread*, jegl_resource* framebuffer)
{
    if (nullptr == framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else
        jegl_using_resource(framebuffer);

    _gl_update_depth_mask_method(jegl_shader::depth_mask_method::ENABLE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void gl_clear_framebuffer_depth(jegl_thread*, jegl_resource* framebuffer)
{
    if (nullptr == framebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else
        jegl_using_resource(framebuffer);

    _gl_update_depth_mask_method(jegl_shader::depth_mask_method::ENABLE);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void gl_get_windows_size(jegl_thread*, size_t* w, size_t* h)
{
    *w = WINDOWS_SIZE_WIDTH;
    *h = WINDOWS_SIZE_HEIGHT;
}


JE_API void jegl_using_opengl_apis(jegl_graphic_api* write_to_apis)
{
    write_to_apis->prepare_interface = gl_prepare;
    write_to_apis->finish_interface = gl_finish;

    write_to_apis->init_interface = gl_startup;
    write_to_apis->pre_update_interface = gl_pre_update;
    write_to_apis->update_interface = gl_update;
    write_to_apis->late_update_interface = gl_lateupdate;
    write_to_apis->shutdown_interface = gl_shutdown;

    write_to_apis->get_windows_size = gl_get_windows_size;

    write_to_apis->init_resource = gl_init_resource;
    write_to_apis->using_resource = gl_using_resource;
    write_to_apis->close_resource = gl_close_resource;

    write_to_apis->draw_vertex = gl_draw_vertex_with_shader;
    write_to_apis->bind_texture = gl_bind_texture;

    write_to_apis->set_rend_buffer = gl_set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer = gl_clear_framebuffer;
    write_to_apis->clear_rend_buffer_color = gl_clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = gl_clear_framebuffer_depth;

    write_to_apis->get_uniform_location = gl_get_uniform_location;
    write_to_apis->set_uniform = gl_set_uniform;
}