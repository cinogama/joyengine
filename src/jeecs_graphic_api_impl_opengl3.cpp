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
}

void glfw_callback_mouse_pos_changed(GLFWwindow* fw, double x, double y)
{
    MOUSE_POS_X = (float)x / WINDOWS_SIZE_WIDTH * 2.0f - 1.0f;
    MOUSE_POS_Y = -((float)y / WINDOWS_SIZE_HEIGHT * 2.0f - 1.0f);
}

void glfw_callback_mouse_key_clicked(GLFWwindow* fw, int key, int state, int mod)
{
    assert(key < MAX_MOUSE_BUTTON_COUNT);
    MOUSE_KEY_STATE[key] = state;
}

void glfw_callback_mouse_scroll_changed(GLFWwindow* fw, double xoffset, double yoffset)
{
    MOUSE_SCROLL_X_COUNT += (float)yoffset;
    MOUSE_SCROLL_Y_COUNT += (float)yoffset;
}

void glfw_callback_keyboard_stage_changed(GLFWwindow* fw, int key, int w, int stage, int v)
{
    assert(key < MAX_KEYBOARD_BUTTON_COUNT);
    KEYBOARD_STATE[key] = stage;
}

jegl_graphic_api::custom_interface_info_t gl_startup(jegl_thread* gthread, const jegl_interface_config* config)
{
    jeecs::debug::log("Graphic thread start!");

    GRAPHIC_THREAD_ID = std::this_thread::get_id();

    if (!glfwInit())
        jeecs::debug::log_fatal("Failed to init glfw.");

    WINDOWS_SIZE_WIDTH = config->m_windows_width ? config->m_windows_width : config->m_resolution_x;
    WINDOWS_SIZE_HEIGHT = config->m_windows_height ? config->m_windows_height : config->m_resolution_y;
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
        jeecs::debug::log_fatal("Failed to init glew: %s.", glewGetErrorString(glew_init_result));

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_2D);

    jegui_init(WINDOWS_HANDLE);

    return nullptr;
}

bool gl_update(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    glfwSwapBuffers(WINDOWS_HANDLE);
    glfwPollEvents();

    if (glfwWindowShouldClose(WINDOWS_HANDLE))
    {
        jeecs::debug::log_warn("Graphic interface has been closed, graphic thread will exit soon.");
        return false;
    }
    return true;
}

bool gl_lateupdate(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    jegui_update();
    return true;
}

void gl_shutdown(jegl_thread*, jegl_graphic_api::custom_interface_info_t)
{
    assert(GRAPHIC_THREAD_ID == std::this_thread::get_id());

    jeecs::debug::log("Graphic thread shutdown!");

    jegui_shutdown();
    glfwDestroyWindow(WINDOWS_HANDLE);

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
            jeecs::debug::log_error("Some error happend when tring compile shader %p, please check.", resource);
            glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(vertex_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                jeecs::debug::log_error("In vertex shader: \n%s", errmsg_buf.data());
            }
            glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetShaderInfoLog(fragment_shader, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                jeecs::debug::log_error("In fragment shader: \n%s", errmsg_buf.data());
            }
            glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &errmsg_len);
            if (errmsg_len > 0)
            {
                std::vector<char> errmsg_buf(errmsg_len + 1);
                glGetProgramInfoLog(shader_program, errmsg_len, &errmsg_written_len, errmsg_buf.data());
                jeecs::debug::log_error("In shader program link: \n%s", errmsg_buf.data());
            }

            jeecs::debug::log_error("Failed to compile shader %p, please check.", resource);
        }
        else
            resource->m_uint1 = shader_program;

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }
    else if (resource->m_type == jegl_resource::type::TEXTURE)
    {
        GLuint texture;
        glGenTextures(1, &texture);

        glBindTexture(GL_TEXTURE_2D, texture);

        // TODO: Enable modify them in raw data.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        GLint texture_aim_format = GL_RGBA;
        GLint texture_src_format = GL_RGBA;

        switch (resource->m_raw_texture_data->m_format)
        {
        case jegl_texture::texture_format::MONO:
            texture_src_format = GL_R; break;
        case jegl_texture::texture_format::RGB:
            texture_src_format = GL_RGB; break;
        case jegl_texture::texture_format::RGBA:
            texture_src_format = GL_RGBA; break;
        default:
            jeecs::debug::log_error("Unknown texture raw-data format.");
        }

        glTexImage2D(GL_TEXTURE_2D,
            0, texture_aim_format,
            resource->m_raw_texture_data->m_width,
            resource->m_raw_texture_data->m_height,
            0, texture_src_format,
            GL_UNSIGNED_BYTE,
            resource->m_raw_texture_data->m_pixels);

        glGenerateMipmap(GL_TEXTURE_2D);
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
    else
        jeecs::debug::log_error("Unknown resource type when initing resource %p, please check.", resource);
}

inline void _gl_using_shader_program(jegl_resource* resource)
{
    glUseProgram(resource->m_uint1);
}

inline void _gl_using_texture2d(jegl_resource* resource)
{
    glBindTexture(GL_TEXTURE_2D, resource->m_uint1);
}

inline void _gl_using_vertex(jegl_resource* resource)
{
    glBindVertexArray(resource->m_uint1);
}

void gl_using_resource(jegl_thread* gthread, jegl_resource* resource)
{
    if (resource->m_type == jegl_resource::type::SHADER)
        _gl_using_shader_program(resource);
    else if (resource->m_type == jegl_resource::type::TEXTURE)
        _gl_using_texture2d(resource);
    else if (resource->m_type == jegl_resource::type::VERTEX)
        _gl_using_vertex(resource);
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
    else
        jeecs::debug::log_error("Unknown resource type when closing resource %p, please check.", resource);
}

void gl_bind_texture(jegl_resource* texture, size_t pass)
{
    glActiveTexture(GL_TEXTURE0 + (GLint)pass);
    jegl_using_resource(texture);
}

void gl_draw_vertex_with_shader(jegl_resource* vert, jegl_resource* shader)
{
    const static GLenum DRAW_METHODS[] = {  
        GL_LINES,
        GL_LINE_LOOP,
        GL_LINE_STRIP,
        GL_TRIANGLES,
        GL_TRIANGLE_STRIP,
        GL_QUADS };

    jegl_using_resource(shader);
    jegl_using_resource(vert);
    glDrawArrays(DRAW_METHODS[vert->m_raw_vertex_data->m_type], 0, vert->m_raw_vertex_data->m_point_count);
}

JE_API void jegl_using_opengl_apis(jegl_graphic_api* write_to_apis)
{
    write_to_apis->init_interface = gl_startup;
    write_to_apis->update_interface = gl_update;
    write_to_apis->late_update_interface = gl_lateupdate;
    write_to_apis->shutdown_interface = gl_shutdown;

    write_to_apis->init_resource = gl_init_resource;
    write_to_apis->using_resource = gl_using_resource;
    write_to_apis->close_resource = gl_close_resource;

    write_to_apis->draw_vertex = gl_draw_vertex_with_shader;
    write_to_apis->bind_texture = gl_bind_texture;
}