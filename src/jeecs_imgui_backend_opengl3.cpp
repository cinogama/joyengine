#define JE_IMPL
#include "jeecs.hpp"

#if defined(JE_ENABLE_GL330_GAPI) \
 || defined(JE_ENABLE_GLES300_GAPI)
#   include <imgui.h>
#   include <imgui_impl_opengl3.h>

#ifdef JE_ENABLE_GL330_GAPI
#   include <imgui_impl_glfw.h>
#   include <GLFW/glfw3.h>
#endif

void jegui_init_gl330(
    void* (*get_img_res)(jegl_resource*),
    void (*apply_shader_sampler)(jegl_resource*),
    void* window_handle,
    bool reboot)
{
    jegui_init_basic(false, get_img_res, apply_shader_sampler);
#ifdef JE_ENABLE_GLES300_GAPI
    // TODO;
#else
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window_handle, true);
#endif
    ImGui_ImplOpenGL3_Init(nullptr);
}

void jegui_update_gl330()
{
    ImGui_ImplOpenGL3_NewFrame();
#ifdef JE_ENABLE_GLES300_GAPI
    // TODO;
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    jegui_update_basic();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void jegui_shutdown_gl330(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplOpenGL3_Shutdown();
#ifdef JE_ENABLE_GLES300_GAPI
    // TODO;
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

#endif