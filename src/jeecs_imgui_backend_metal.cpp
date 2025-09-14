#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_METAL_GAPI
#include "jeecs_imgui_backend_api.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_metal.h>

void jegui_init_metal(
    jegl_context* ctx,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler,
    void* window_handle,
    MTL::Device* device)
{
    jegui_init_basic(ctx, get_img_res, apply_shader_sampler);
    ImGui_ImplGlfw_InitForOther((GLFWwindow*)window_handle, true);
    ImGui_ImplMetal_Init(device);
}

void jegui_update_metal(
    MTL::RenderPassDescriptor* rend_pass_desc,
    MTL::CommandBuffer* command_buffer,
    MTL::RenderCommandEncoder* command_encoder)
{
    ImGui_ImplMetal_NewFrame(rend_pass_desc);
    ImGui_ImplGlfw_NewFrame();

    struct MetalRendCommandStates
    {
        MTL::CommandBuffer* m_command_buffer;
        MTL::RenderCommandEncoder* m_command_encoder;
    } metal_states = { command_buffer, command_encoder };

    jegui_update_basic(
        [](void* state)
        { 
            MetalRendCommandStates* metal_states =
                reinterpret_cast<MetalRendCommandStates*>(state);

            ImGui_ImplMetal_RenderDrawData(
                ImGui::GetDrawData(),
                metal_states->m_command_buffer,
                metal_states->m_command_encoder); 
        },
        &metal_states);
}

void jegui_shutdown_metal(bool reboot)
{
    jegui_shutdown_basic(reboot);
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
#endif