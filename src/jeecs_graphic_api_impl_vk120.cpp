#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_graphic_render_internal.hpp"

#ifdef JE_ENABLE_VK120_GAPI

#include "jeecs_imgui_backend_api.hpp"

#include <vulkan/vulkan.h>

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   include <vulkan/vulkan_win32.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#   include <vulkan/vulkan_android.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_LINUX
#   include <vulkan/vulkan_xlib.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_MACOS
#   include <vulkan/vulkan_macos.h>
#   define JE4_VK_USE_DYNAMIC_VK_LIB 0
#else
#   error Unsupport platform.
#   define JE4_VK_USE_DYNAMIC_VK_LIB 1
#endif

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#include "jeecs_graphic_api_interface_egl.hpp"
#else
#include "jeecs_graphic_api_interface_glfw.hpp"
#endif // JE_GL_USE_EGL_INSTEAD_GLFW

#include <optional>
#include <array>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

#undef max
#undef min

namespace jeecs::graphic::api::vk120
{
#define JE_VK_CHECK(expr)                                                        \
    do                                                                           \
    {                                                                            \
        VkResult je_vk_check_result = (expr);                                    \
        if (je_vk_check_result != VK_SUCCESS)                                    \
        {                                                                        \
            jeecs::debug::logfatal("Vulkan call failed: %s (VkResult=%d).",      \
                #expr, (int)je_vk_check_result);                                 \
            std::abort();                                                        \
        }                                                                        \
    } while (0)

    static bool _vk_format_is_srgb(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
            return true;
        default:
            return false;
        }
    }

#if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
#   define VK_API_PLATFORM_API_LIST \
    VK_API_DECL(vkCreateWin32SurfaceKHR)
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
#   define VK_API_PLATFORM_API_LIST \
    VK_API_DECL(vkCreateAndroidSurfaceKHR)
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_LINUX
#   define VK_API_PLATFORM_API_LIST \
    VK_API_DECL(vkCreateXlibSurfaceKHR)
#elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_MACOS
#   define VK_API_PLATFORM_API_LIST \
    VK_API_DECL(vkCreateMacOSSurfaceMVK)
#else
#error Unsupport platform.
#endif

#define VK_API_LIST                                         \
    VK_API_DECL(vkEnumeratePhysicalDevices);                \
    VK_API_DECL(vkGetPhysicalDeviceProperties);             \
    VK_API_DECL(vkGetPhysicalDeviceFeatures);               \
    VK_API_DECL(vkEnumerateDeviceExtensionProperties);      \
                                                            \
    VK_API_DECL(vkGetPhysicalDeviceQueueFamilyProperties);  \
    VK_API_DECL(vkGetPhysicalDeviceFormatProperties);       \
    VK_API_DECL(vkGetPhysicalDeviceImageFormatProperties);  \
    VK_API_DECL(vkCreateDevice);                            \
    VK_API_DECL(vkDestroyDevice);                           \
    VK_API_DECL(vkGetDeviceQueue);                          \
    VK_API_DECL(vkQueueSubmit);                             \
    VK_API_DECL(vkQueueWaitIdle);                           \
    VK_API_DECL(vkDeviceWaitIdle);                          \
                                                            \
    VK_API_DECL(vkAcquireNextImageKHR);                     \
    VK_API_DECL(vkQueuePresentKHR);                         \
                                                            \
    VK_API_DECL(vkDestroySurfaceKHR);                       \
                                                            \
    VK_API_DECL(vkCreateSwapchainKHR);                      \
    VK_API_DECL(vkDestroySwapchainKHR);                     \
    VK_API_DECL(vkGetSwapchainImagesKHR);                   \
                                                            \
    VK_API_DECL(vkCreateSampler);                           \
    VK_API_DECL(vkDestroySampler);                          \
                                                            \
    VK_API_DECL(vkCreateImage);                             \
    VK_API_DECL(vkDestroyImage);                            \
    VK_API_DECL(vkBindImageMemory);                         \
    VK_API_DECL(vkGetImageMemoryRequirements);              \
                                                            \
    VK_API_DECL(vkCreateImageView);                         \
    VK_API_DECL(vkDestroyImageView);                        \
                                                            \
    VK_API_DECL(vkAllocateMemory);                          \
    VK_API_DECL(vkFreeMemory);                              \
    VK_API_DECL(vkMapMemory);                               \
    VK_API_DECL(vkUnmapMemory);                             \
    VK_API_DECL(vkBindBufferMemory);                        \
    VK_API_DECL(vkGetBufferMemoryRequirements);             \
    VK_API_DECL(vkCreateBuffer);                            \
    VK_API_DECL(vkDestroyBuffer);                           \
    VK_API_DECL(vkFlushMappedMemoryRanges);                 \
                                                            \
    VK_API_DECL(vkCreateShaderModule);                      \
    VK_API_DECL(vkDestroyShaderModule);                     \
                                                            \
    VK_API_DECL(vkCreatePipelineLayout);                    \
    VK_API_DECL(vkDestroyPipelineLayout);                   \
                                                            \
    VK_API_DECL(vkCreateGraphicsPipelines);                 \
    VK_API_DECL(vkDestroyPipeline);                         \
    VK_API_DECL(vkCreatePipelineCache);                     \
    VK_API_DECL(vkDestroyPipelineCache);                    \
                                                            \
    VK_API_DECL(vkCreateFramebuffer);                       \
    VK_API_DECL(vkDestroyFramebuffer);                      \
                                                            \
    VK_API_DECL(vkCreateCommandPool);                       \
    VK_API_DECL(vkDestroyCommandPool);                      \
    VK_API_DECL(vkAllocateCommandBuffers);                  \
    VK_API_DECL(vkFreeCommandBuffers);                      \
    VK_API_DECL(vkBeginCommandBuffer);                      \
    VK_API_DECL(vkResetCommandBuffer);                      \
    VK_API_DECL(vkEndCommandBuffer);                        \
    VK_API_DECL(vkCmdBeginRenderPass);                      \
    VK_API_DECL(vkCmdEndRenderPass);                        \
    VK_API_DECL(vkCmdBindPipeline);                         \
    VK_API_DECL(vkCmdDraw);                                 \
    VK_API_DECL(vkCmdSetViewport);                          \
    VK_API_DECL(vkCmdSetScissor);                           \
    VK_API_DECL(vkCmdClearAttachments);                     \
    VK_API_DECL(vkCmdBindDescriptorSets);                   \
    VK_API_DECL(vkCmdCopyBufferToImage);                    \
    VK_API_DECL(vkCmdPipelineBarrier);                      \
    VK_API_DECL(vkCmdPushConstants);                        \
    VK_API_DECL(vkCmdDrawIndexed);                          \
    VK_API_DECL(vkCmdBindVertexBuffers);                    \
    VK_API_DECL(vkCmdBindIndexBuffer);                      \
                                                            \
    VK_API_DECL(vkCreateSemaphore);                         \
    VK_API_DECL(vkDestroySemaphore);                        \
    VK_API_DECL(vkWaitSemaphores);                          \
                                                            \
    VK_API_DECL(vkCreateFence);                             \
    VK_API_DECL(vkDestroyFence);                            \
    VK_API_DECL(vkWaitForFences);                           \
    VK_API_DECL(vkResetFences);                             \
                                                            \
    VK_API_DECL(vkCreateDescriptorSetLayout);               \
    VK_API_DECL(vkDestroyDescriptorSetLayout);              \
                                                            \
    VK_API_DECL(vkCreateRenderPass);                        \
    VK_API_DECL(vkDestroyRenderPass);                       \
                                                            \
    VK_API_DECL(vkCreateDescriptorPool);                    \
    VK_API_DECL(vkDestroyDescriptorPool);                   \
    VK_API_DECL(vkResetDescriptorPool);                     \
    VK_API_DECL(vkAllocateDescriptorSets);                  \
    VK_API_DECL(vkFreeDescriptorSets);                      \
    VK_API_DECL(vkUpdateDescriptorSets);                    \
                                                            \
    VK_API_DECL(vkGetPhysicalDeviceSurfaceSupportKHR);      \
    VK_API_DECL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
    VK_API_DECL(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
    VK_API_DECL(vkGetPhysicalDeviceSurfacePresentModesKHR); \
                                                            \
    VK_API_DECL(vkCreateDebugUtilsMessengerEXT);            \
    VK_API_DECL(vkDestroyDebugUtilsMessengerEXT);           \
                                                            \
    VK_API_DECL(vkGetPhysicalDeviceMemoryProperties);       \
    VK_API_DECL(vkGetPhysicalDeviceProperties2);            \
                                                            \
    VK_API_PLATFORM_API_LIST

    struct jegl_vk120_context;

    struct jevk12_framebuffer;
    struct jevk12_shader_blob
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_shader_blob);
        struct blob_data
        {
            JECS_DISABLE_MOVE_AND_COPY(blob_data);

            blob_data() = default;
            ~blob_data();

            inline static const VkDynamicState m_dynamic_states[] =
            {
                // VkDynamicState::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT,
                VkDynamicState::VK_DYNAMIC_STATE_SCISSOR,
            };

            jegl_vk120_context* m_context = nullptr;
            jegl_shader* m_shader_resource = nullptr;

            VkShaderModule m_vertex_shader_module = VK_NULL_HANDLE;
            VkShaderModule m_fragment_shader_module = VK_NULL_HANDLE;
            VkPipelineShaderStageCreateInfo m_shader_stage_infos[2] = {};

            std::vector<VkVertexInputAttributeDescription> m_vertex_input_attribute_descriptions;
            size_t m_shader_output_color_attachment_count = 0;

            VkViewport m_viewport = {};
            VkRect2D m_scissor = {};
            VkPipelineViewportStateCreateInfo m_viewport_state_create_info = {};
            VkPipelineDynamicStateCreateInfo m_dynamic_state_create_info = {};
            VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

            struct sampler_data
            {
                VkSampler m_vk_sampler = VK_NULL_HANDLE;
                uint32_t m_sampler_id = 0;
            };
            std::vector<sampler_data> m_samplers;
            struct texture_sampler_binding
            {
                uint32_t m_pass = 0;
                uint32_t m_sampler_id = 0;
            };
            std::vector<texture_sampler_binding> m_texture_sampler_bindings;
            std::unordered_map<std::string, uint32_t> m_uniform_locations;
            size_t m_uniform_size = 0;
        };

        jeecs::basic::resource<blob_data> m_blob_data;

        jevk12_shader_blob(blob_data* d)
            : m_blob_data(jeecs::basic::resource<blob_data>(d))
        {
        }

        uint32_t get_built_in_location(const std::string& name) const
        {
            auto fnd = m_blob_data->m_uniform_locations.find(name);
            if (fnd != m_blob_data->m_uniform_locations.end())
                return fnd->second;

            return jeecs::graphic::INVALID_UNIFORM_LOCATION;
        }
    };
    struct jevk12_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_texture);

        VkImage m_vk_texture_image = VK_NULL_HANDLE;
        VkDeviceMemory m_vk_texture_image_memory = VK_NULL_HANDLE;
        VkDeviceSize m_vk_texture_memory_offset = 0;
        VkDeviceSize m_vk_texture_memory_size = 0;

        VkImageView m_vk_texture_image_view = VK_NULL_HANDLE;
        VkFormat m_vk_texture_format = VK_FORMAT_UNDEFINED;
        VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_depth = 1;
        uint32_t m_layers = 1;
        uint32_t m_samples = 1;

        bool m_owns_image = true;

        jevk12_texture() = default;
    };
    struct jevk12_uniformbuf
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_uniformbuf);

        VkBuffer m_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_uniform_buffer_memory = VK_NULL_HANDLE;
        VkDeviceSize m_memory_offset = 0;
        VkDeviceSize m_memory_size = 0;
        VkDeviceSize m_buffer_size = 0;
        void* m_mapped = nullptr;

        uint32_t m_real_binding_place = 0;

        jevk12_uniformbuf() = default;
    };
    struct jevk12_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_shader);

        jeecs::basic::resource<jevk12_shader_blob::blob_data> m_blob_data;

        size_t m_uniform_cpu_buffer_size = 0;
        uint8_t* m_uniform_cpu_buffer = nullptr;

        jevk12_shader(jevk12_shader_blob* blob)
            : m_blob_data(blob->m_blob_data)
            , m_uniform_cpu_buffer_size(blob->m_blob_data->m_uniform_size)
        {
            if (m_uniform_cpu_buffer_size != 0)
            {
                m_uniform_cpu_buffer =
                    static_cast<uint8_t*>(malloc(m_uniform_cpu_buffer_size));
                assert(m_uniform_cpu_buffer != nullptr);
                memset(m_uniform_cpu_buffer, 0, m_uniform_cpu_buffer_size);
            }
            else
                m_uniform_cpu_buffer = nullptr;
        }
        ~jevk12_shader();
    };
    struct jevk12_framebuffer
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_framebuffer);

        // 由 context 的 render pass 缓存持有，本结构不负责销毁。
        VkRenderPass m_rendpass = VK_NULL_HANDLE;

        std::vector<jevk12_texture*> m_color_attachments;
        jevk12_texture* m_resolve_attachment = nullptr;
        jevk12_texture* m_depth_attachment = nullptr;
        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

        size_t m_width = 0;
        size_t m_height = 0;

        VkSampleCountFlagBits m_sample_count = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout m_color_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_resolve_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_depth_initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_color_final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageLayout m_resolve_final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkImageLayout m_depth_final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        bool m_is_screen_framebuffer = false;

        jevk12_framebuffer() = default;
    };
    struct jevk12_vertex
    {
        JECS_DISABLE_MOVE_AND_COPY(jevk12_vertex);

        jevk12_vertex() = default;

        VkBuffer m_vk_vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vk_vertex_buffer_memory = VK_NULL_HANDLE;
        VkDeviceSize m_vertex_memory_offset = 0;
        VkDeviceSize m_vertex_memory_size = 0;
        void* m_mapped = nullptr;

        VkBuffer m_vk_index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vk_index_buffer_memory = VK_NULL_HANDLE;
        VkDeviceSize m_index_memory_offset = 0;
        VkDeviceSize m_index_memory_size = 0;

        uint32_t m_vertex_point_count = 0;
        VkDeviceSize m_size = 0;
        VkDeviceSize m_stride = 0;

        jegl_vertex::type m_topology = jegl_vertex::type::TRIANGLES;
    };

    struct jegl_vk120_context
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_vk120_context);

        jegl_vk120_context() = default;

#if JE4_VK_USE_DYNAMIC_VK_LIB
        struct vklibrary_instance_proxy
        {
            JECS_DISABLE_MOVE_AND_COPY(vklibrary_instance_proxy);

            woort_Dylib* _instance;

            vklibrary_instance_proxy()
            {
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_WINDOWS
                _instance = woort_dylib_load("je/graphiclib/vulkan-1", "vulkan-1.dll", nullptr, false);
#   elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
                _instance = woort_dylib_load("je/graphiclib/vulkan", "libvulkan.so", nullptr, false);
#   elif JE4_CURRENT_PLATFORM == JE4_PLATFORM_LINUX
                _instance = woort_dylib_load("je/graphiclib/vulkan", "libvulkan.so.1", nullptr, false);
                if (_instance == nullptr)
                    _instance = woort_dylib_load("je/graphiclib/vulkan", "libvulkan.so", nullptr, false);
#   else
#       error Unsupport platform for dynamic vulkan library loading.
#   endif
                if (_instance == nullptr)
                {
                    jeecs::debug::logfatal("Failed to get vulkan library instance.");
                }
            }
            ~vklibrary_instance_proxy()
            {
                assert(_instance != nullptr);
                woort_dylib_unload(_instance, WOORT_DYLIB_UNREF);
            }

            template <typename FT>
            FT api(const char* name)
            {
                auto* result_ft = reinterpret_cast<FT>(woort_dylib_load_func(_instance, name));
                if (result_ft == nullptr)
                {
                    jeecs::debug::logfatal("Failed to get vulkan library api: '%s'.", name);
                }
                return result_ft;
            }
        };

        vklibrary_instance_proxy vk_proxy;

#   define VK_API_DECL(name) PFN_##name name = vk_proxy.api<PFN_##name>(#name);

        VK_API_DECL(vkCreateInstance);
        VK_API_DECL(vkDestroyInstance);

        VK_API_DECL(vkGetInstanceProcAddr);

        VK_API_DECL(vkEnumerateInstanceVersion);
        VK_API_DECL(vkEnumerateInstanceLayerProperties);
        VK_API_DECL(vkEnumerateInstanceExtensionProperties);

#   undef VK_API_DECL
#endif
        jegl_context* _vk_jegl_context;

        // 一些配置项
        size_t _vk_msaa_config;
        VkSampleCountFlagBits _vk_active_msaa_samples = VK_SAMPLE_COUNT_1_BIT;
        bool _vk_vsync_config;

        // Vk的全局实例
        VkInstance _vk_instance;
        basic_interface* _vk_jegl_interface;

        VkPhysicalDevice _vk_device;
        uint32_t _vk_device_queue_graphic_family_index;
        uint32_t _vk_device_queue_present_family_index;
        VkDevice _vk_logic_device;
        VkQueue _vk_logic_device_graphic_queue;
        VkQueue _vk_logic_device_present_queue;

        VkSurfaceKHR _vk_surface;
        VkSurfaceCapabilitiesKHR _vk_surface_capabilities;
        VkSurfaceFormatKHR _vk_surface_format;
        VkPresentModeKHR _vk_surface_present_mode;
        VkFormat _vk_depth_format;

        // When the surface offers no sRGB format, all screen-bound draws are
        // redirected to a linear offscreen framebuffer; a woshader-based
        // encode pass then writes it to the real swapchain image.
        bool _vk_use_srgb_encode_pass = false;
        bool _vk_encoding_srgb_screen = false;
        jegl_texture_2d* _vk_srgb_encode_screen_color_resource = nullptr;
        jevk12_texture* _vk_srgb_encode_screen_color_texture = nullptr;
        jevk12_texture* _vk_srgb_encode_screen_depth_texture = nullptr;
        jevk12_framebuffer* _vk_srgb_encode_screen_framebuffer = nullptr;
        size_t _vk_srgb_encode_screen_width = 0;
        size_t _vk_srgb_encode_screen_height = 0;

        VkFence _vk_queue_commit_fence;

        size_t RESOLUTION_WIDTH;
        size_t RESOLUTION_HEIGHT;

        enum descriptor_set_type : uint32_t
        {
            UNIFORM_VARIABLES = 0,
            UNIFORM_BUFFER,
            TEXTURE,
            SAMPLER,

            COUNT,
            DEAR_IMGUI_TEXTURE = COUNT,
            BEGIN = UNIFORM_VARIABLES,
        };
        VkDescriptorSetLayout _vk_global_descriptor_set_layouts[descriptor_set_type::COUNT + 1] = {};

        template <VkDescriptorType DESC_SET_TYPE, VkShaderStageFlags STAGE, size_t MAX_BINDING>
        VkDescriptorSetLayout create_descriptor_set_layout()
        {
            VkDescriptorSetLayoutBinding ubo_layout_binding[MAX_BINDING] = {};
            for (uint32_t i = 0; i < MAX_BINDING; ++i)
            {
                ubo_layout_binding[i].binding = i;
                ubo_layout_binding[i].descriptorType = DESC_SET_TYPE;
                ubo_layout_binding[i].descriptorCount = 1;
                ubo_layout_binding[i].stageFlags = STAGE;
                ubo_layout_binding[i].pImmutableSamplers = nullptr;
            }

            VkDescriptorSetLayoutCreateInfo ubo_layout_create_info = {};
            ubo_layout_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            ubo_layout_create_info.pNext = nullptr;
            ubo_layout_create_info.flags = 0;
            ubo_layout_create_info.bindingCount = MAX_BINDING;
            ubo_layout_create_info.pBindings = ubo_layout_binding;

            VkDescriptorSetLayout result_layout;
            if (VK_SUCCESS != vkCreateDescriptorSetLayout(
                _vk_logic_device,
                &ubo_layout_create_info,
                nullptr,
                &result_layout))
            {
                jeecs::debug::logfatal("Failed to create vk120 uniform variables descriptor set layout.");
                abort();
            }
            return result_layout;
        }

        void destroy_descriptor_set_layout(VkDescriptorSetLayout layout)
        {
            vkDestroyDescriptorSetLayout(_vk_logic_device, layout, nullptr);
        }

        struct swapchain_image_content
        {
            jegl_vk120_context* m_main_context = nullptr;

            jevk12_framebuffer* m_framebuffer = nullptr;
            jevk12_texture* m_color_attachment = nullptr;
            jevk12_texture* m_depth_attachment = nullptr;
            jevk12_texture* m_msaa_attachment = nullptr;

            swapchain_image_content(
                jegl_vk120_context* main_contant,
                jevk12_framebuffer* framebuffer,
                jevk12_texture* color_attachment,
                jevk12_texture* depth_attachment,
                jevk12_texture* msaa_attachment = nullptr)
                : m_main_context(main_contant)
                , m_framebuffer(framebuffer)
                , m_color_attachment(color_attachment)
                , m_depth_attachment(depth_attachment)
                , m_msaa_attachment(msaa_attachment)
            {
            }
            ~swapchain_image_content()
            {
                m_main_context->destroy_frame_buffer(m_framebuffer);

                if (m_color_attachment != nullptr)
                    m_main_context->destroy_texture_instance(m_color_attachment);
                if (m_depth_attachment != nullptr)
                    m_main_context->destroy_texture_instance(m_depth_attachment);
                if (m_msaa_attachment != nullptr)
                    m_main_context->destroy_texture_instance(m_msaa_attachment);
            }
        };

        static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

        struct frame_resources
        {
            JECS_DISABLE_MOVE_AND_COPY(frame_resources);
            frame_resources() = default;

            VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
            VkFence m_fence = VK_NULL_HANDLE;
            VkSemaphore m_image_available = VK_NULL_HANDLE;
            VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;

            VkBuffer m_ubo_ring_buffer = VK_NULL_HANDLE;
            VkDeviceMemory m_ubo_ring_memory = VK_NULL_HANDLE;
            VkDeviceSize m_ubo_ring_memory_offset = 0;
            VkDeviceSize m_ubo_ring_memory_size = 0;
            void* m_ubo_ring_mapped = nullptr;
            VkDeviceSize m_ubo_ring_size = 0;
            VkDeviceSize m_ubo_ring_offset = 0;

            bool m_recording = false;
            bool m_has_swapchain_image = false;
            bool m_submitted = false;

            VkDeviceSize allocate_uniform_block(VkDeviceSize size, VkDeviceSize alignment)
            {
                if (size == 0)
                    size = 1;
                if (alignment == 0)
                    alignment = 1;

                VkDeviceSize aligned = (m_ubo_ring_offset + alignment - 1) & ~(alignment - 1);
                if (aligned + size > m_ubo_ring_size)
                {
                    jeecs::debug::logfatal(
                        "VK120: per-frame uniform ring exhausted (need %llu bytes, %llu available). "
                        "Consider increasing the ring size.",
                        (unsigned long long)size,
                        (unsigned long long)(m_ubo_ring_size > aligned ? m_ubo_ring_size - aligned : 0));
                }

                m_ubo_ring_offset = aligned + size;
                return aligned;
            }
        };

        struct descriptor_set_allocator
        {
            JECS_DISABLE_MOVE_AND_COPY(descriptor_set_allocator);

            constexpr static size_t MAX_LAYOUT_COUNT = 16;
            constexpr static size_t MAX_UBO_LAYOUT = MAX_LAYOUT_COUNT;
            constexpr static size_t MAX_TEXTURE_LAYOUT = MAX_LAYOUT_COUNT;
            constexpr static size_t MAX_SAMPLER_LAYOUT = MAX_LAYOUT_COUNT;

            jegl_vk120_context* m_context = nullptr;

            VkDescriptorBufferInfo m_binded_ubos[MAX_UBO_LAYOUT] = {};
            VkDescriptorImageInfo m_binded_textures[MAX_TEXTURE_LAYOUT] = {};
            VkDescriptorImageInfo m_binded_samplers[MAX_SAMPLER_LAYOUT] = {};
            VkPipelineLayout m_binded_pipeline_layout = VK_NULL_HANDLE;

            bool m_ubo_dirty = true;
            bool m_texture_dirty = true;
            bool m_sampler_dirty = true;

            VkDescriptorSet m_ubo_set = VK_NULL_HANDLE;
            VkDescriptorSet m_texture_set = VK_NULL_HANDLE;
            VkDescriptorSet m_sampler_set = VK_NULL_HANDLE;

            std::unordered_map<jevk12_shader*, VkDescriptorSet> m_uniform_vars_sets;

            explicit descriptor_set_allocator(jegl_vk120_context* context)
                : m_context(context)
            {
                assert(m_context != nullptr);

                for (auto& ubo : m_binded_ubos)
                {
                    ubo.buffer = VK_NULL_HANDLE;
                    ubo.offset = 0;
                    ubo.range = VK_WHOLE_SIZE;
                }
                for (auto& texture : m_binded_textures)
                {
                    texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    texture.imageView = VK_NULL_HANDLE;
                    texture.sampler = VK_NULL_HANDLE;
                }
                for (auto& sampler : m_binded_samplers)
                {
                    sampler.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    sampler.imageView = VK_NULL_HANDLE;
                    sampler.sampler = VK_NULL_HANDLE;
                }
            }

            void begin_frame()
            {
                m_binded_pipeline_layout = VK_NULL_HANDLE;
                m_ubo_dirty = true;
                m_texture_dirty = true;
                m_sampler_dirty = true;
                m_ubo_set = VK_NULL_HANDLE;
                m_texture_set = VK_NULL_HANDLE;
                m_sampler_set = VK_NULL_HANDLE;
                m_uniform_vars_sets.clear();

                for (auto& ubo : m_binded_ubos)
                    ubo.buffer = VK_NULL_HANDLE;
                for (auto& texture : m_binded_textures)
                    texture.imageView = VK_NULL_HANDLE;
                for (auto& sampler : m_binded_samplers)
                    sampler.sampler = VK_NULL_HANDLE;
            }

            void begin_pass()
            {
                m_binded_pipeline_layout = VK_NULL_HANDLE;
                m_ubo_dirty = true;
                m_texture_dirty = true;
                m_sampler_dirty = true;
                m_ubo_set = VK_NULL_HANDLE;
                m_texture_set = VK_NULL_HANDLE;
                m_sampler_set = VK_NULL_HANDLE;

                for (auto& ubo : m_binded_ubos)
                    ubo.buffer = VK_NULL_HANDLE;
                for (auto& texture : m_binded_textures)
                    texture.imageView = VK_NULL_HANDLE;
                for (auto& sampler : m_binded_samplers)
                    sampler.sampler = VK_NULL_HANDLE;
            }

            void bind_uniform_buffer(uint32_t binding, VkBuffer buffer)
            {
                if (binding >= MAX_UBO_LAYOUT)
                {
                    jeecs::debug::logwarn(
                        "Uniform buffer binding %u exceeds max layout count %zu, ignored.",
                        binding, MAX_UBO_LAYOUT);
                    return;
                }
                if (m_binded_ubos[binding].buffer != buffer)
                {
                    m_binded_ubos[binding].buffer = buffer;
                    m_binded_ubos[binding].offset = 0;
                    m_binded_ubos[binding].range = VK_WHOLE_SIZE;
                    m_ubo_dirty = true;
                }
            }

            void bind_texture(uint32_t binding, VkImageView image_view)
            {
                if (binding >= MAX_TEXTURE_LAYOUT)
                {
                    jeecs::debug::logwarn(
                        "Texture binding %u exceeds max layout count %zu, ignored.",
                        binding, MAX_TEXTURE_LAYOUT);
                    return;
                }
                if (m_binded_textures[binding].imageView != image_view)
                {
                    m_binded_textures[binding].imageView = image_view;
                    m_texture_dirty = true;
                }
            }

            void bind_sampler(uint32_t binding, VkSampler sampler)
            {
                if (binding >= MAX_SAMPLER_LAYOUT)
                {
                    jeecs::debug::logwarn(
                        "Sampler binding %u exceeds max layout count %zu, ignored.",
                        binding, MAX_SAMPLER_LAYOUT);
                    return;
                }
                if (m_binded_samplers[binding].sampler != sampler)
                {
                    m_binded_samplers[binding].sampler = sampler;
                    m_sampler_dirty = true;
                }
            }

            void bind_pipeline_layout(VkPipelineLayout pipeline_layout)
            {
                m_binded_pipeline_layout = pipeline_layout;
            }

            VkDescriptorSet allocate_descriptor_set(VkDescriptorSetLayout layout)
            {
                VkDescriptorSetAllocateInfo alloc_info = {};
                alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                alloc_info.pNext = nullptr;
                alloc_info.descriptorPool = m_context->current_frame().m_descriptor_pool;
                alloc_info.descriptorSetCount = 1;
                alloc_info.pSetLayouts = &layout;

                VkDescriptorSet result = VK_NULL_HANDLE;
                JE_VK_CHECK(m_context->vkAllocateDescriptorSets(
                    m_context->_vk_logic_device, &alloc_info, &result));
                return result;
            }

            VkDescriptorSet uniform_vars_set(jevk12_shader* shader)
            {
                assert(shader != nullptr);

                auto found = m_uniform_vars_sets.find(shader);
                if (found != m_uniform_vars_sets.end())
                    return found->second;

                VkDescriptorSet set = allocate_descriptor_set(
                    m_context->_vk_global_descriptor_set_layouts[
                        descriptor_set_type::UNIFORM_VARIABLES]);

                VkDescriptorBufferInfo buffer_info = {};
                buffer_info.buffer = m_context->current_frame().m_ubo_ring_buffer;
                buffer_info.offset = 0;
                buffer_info.range = std::max<VkDeviceSize>(
                    (VkDeviceSize)shader->m_uniform_cpu_buffer_size, 16);

                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstSet = set;
                write.dstBinding = 0;
                write.dstArrayElement = 0;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                write.pBufferInfo = &buffer_info;

                m_context->vkUpdateDescriptorSets(
                    m_context->_vk_logic_device, 1, &write, 0, nullptr);

                m_uniform_vars_sets.emplace(shader, set);
                return set;
            }

            VkDescriptorSet ensure_ubo_set()
            {
                if (!m_ubo_dirty && m_ubo_set != VK_NULL_HANDLE)
                    return m_ubo_set;

                m_ubo_set = allocate_descriptor_set(
                    m_context->_vk_global_descriptor_set_layouts[
                        descriptor_set_type::UNIFORM_BUFFER]);

                VkWriteDescriptorSet writes[MAX_UBO_LAYOUT] = {};
                uint32_t write_count = 0;
                for (uint32_t i = 0; i < MAX_UBO_LAYOUT; ++i)
                {
                    if (m_binded_ubos[i].buffer == VK_NULL_HANDLE)
                        continue;

                    auto& write = writes[write_count++];
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = m_ubo_set;
                    write.dstBinding = i;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    write.pBufferInfo = &m_binded_ubos[i];
                }
                if (write_count != 0)
                {
                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device, write_count, writes, 0, nullptr);
                }
                m_ubo_dirty = false;
                return m_ubo_set;
            }

            VkDescriptorSet ensure_texture_set()
            {
                if (!m_texture_dirty && m_texture_set != VK_NULL_HANDLE)
                    return m_texture_set;

                m_texture_set = allocate_descriptor_set(
                    m_context->_vk_global_descriptor_set_layouts[
                        descriptor_set_type::TEXTURE]);

                VkWriteDescriptorSet writes[MAX_TEXTURE_LAYOUT] = {};
                uint32_t write_count = 0;
                for (uint32_t i = 0; i < MAX_TEXTURE_LAYOUT; ++i)
                {
                    if (m_binded_textures[i].imageView == VK_NULL_HANDLE)
                        continue;

                    auto& write = writes[write_count++];
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = m_texture_set;
                    write.dstBinding = i;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    write.pImageInfo = &m_binded_textures[i];
                }
                if (write_count != 0)
                {
                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device, write_count, writes, 0, nullptr);
                }
                m_texture_dirty = false;
                return m_texture_set;
            }

            VkDescriptorSet ensure_sampler_set()
            {
                if (!m_sampler_dirty && m_sampler_set != VK_NULL_HANDLE)
                    return m_sampler_set;

                m_sampler_set = allocate_descriptor_set(
                    m_context->_vk_global_descriptor_set_layouts[
                        descriptor_set_type::SAMPLER]);

                VkWriteDescriptorSet writes[MAX_SAMPLER_LAYOUT] = {};
                uint32_t write_count = 0;
                for (uint32_t i = 0; i < MAX_SAMPLER_LAYOUT; ++i)
                {
                    if (m_binded_samplers[i].sampler == VK_NULL_HANDLE)
                        continue;

                    auto& write = writes[write_count++];
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = m_sampler_set;
                    write.dstBinding = i;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    write.pImageInfo = &m_binded_samplers[i];
                }
                if (write_count != 0)
                {
                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device, write_count, writes, 0, nullptr);
                }
                m_sampler_dirty = false;
                return m_sampler_set;
            }

            void bind_all(
                VkCommandBuffer command_buffer,
                VkDescriptorSet uniform_vars,
                uint32_t dynamic_offset)
            {
                assert(m_binded_pipeline_layout != VK_NULL_HANDLE);

                VkDescriptorSet sets[descriptor_set_type::COUNT] = {
                    uniform_vars,
                    ensure_ubo_set(),
                    ensure_texture_set(),
                    ensure_sampler_set() };

                m_context->vkCmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_binded_pipeline_layout,
                    0,
                    descriptor_set_type::COUNT,
                    sets,
                    1,
                    &dynamic_offset);
            }
        };

        struct memory_allocator
        {
            JECS_DISABLE_MOVE_AND_COPY(memory_allocator);

            struct block
            {
                VkDeviceMemory m_memory = VK_NULL_HANDLE;
                VkDeviceSize m_size = 0;
                uint32_t m_memory_type = 0;
                void* m_mapped = nullptr;
                std::vector<std::pair<VkDeviceSize, VkDeviceSize>> m_free_ranges;
            };
            struct allocation
            {
                VkDeviceMemory m_memory = VK_NULL_HANDLE;
                VkDeviceSize m_offset = 0;
                VkDeviceSize m_size = 0;
                void* m_mapped = nullptr;
            };

            jegl_vk120_context* m_context = nullptr;
            VkDeviceSize m_buffer_image_granularity = 1;
            VkDeviceSize m_default_device_block_size = 64ull * 1024 * 1024;
            VkDeviceSize m_default_host_block_size = 16ull * 1024 * 1024;
            std::vector<block> m_blocks;

            explicit memory_allocator(jegl_vk120_context* context)
                : m_context(context)
            {
                assert(m_context != nullptr);
            }
            ~memory_allocator()
            {
                for (auto& block : m_blocks)
                {
                    if (block.m_mapped != nullptr)
                    {
                        m_context->vkUnmapMemory(
                            m_context->_vk_logic_device, block.m_memory);
                        block.m_mapped = nullptr;
                    }
                    if (block.m_memory != VK_NULL_HANDLE)
                        m_context->vkFreeMemory(
                            m_context->_vk_logic_device, block.m_memory, nullptr);
                }
            }

            // 一个 VkDeviceMemory 同一时间只能被 map 一次。host-visible block
            // 在创建/首次使用时整体持久映射，子分配只使用 mapped + offset。
            void ensure_block_mapped(block& target, bool host_visible)
            {
                if (!host_visible || target.m_mapped != nullptr)
                    return;

                JE_VK_CHECK(m_context->vkMapMemory(
                    m_context->_vk_logic_device,
                    target.m_memory,
                    0,
                    target.m_size,
                    0,
                    &target.m_mapped));
            }

            static VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment)
            {
                if (alignment <= 1)
                    return value;
                return (value + alignment - 1) & ~(alignment - 1);
            }

            bool try_allocate_from_block(
                block& target,
                VkDeviceSize size,
                VkDeviceSize alignment,
                allocation& out)
            {
                for (size_t i = 0; i < target.m_free_ranges.size(); ++i)
                {
                    const VkDeviceSize range_begin = target.m_free_ranges[i].first;
                    const VkDeviceSize range_end =
                        range_begin + target.m_free_ranges[i].second;
                    const VkDeviceSize aligned =
                        align_up(range_begin, alignment);

                    if (aligned + size > range_end)
                        continue;

                    const VkDeviceSize front_size = aligned - range_begin;
                    const VkDeviceSize back_begin = aligned + size;
                    const VkDeviceSize back_size = range_end - back_begin;

                    std::vector<std::pair<VkDeviceSize, VkDeviceSize>> replacement;
                    if (front_size != 0)
                        replacement.push_back({ range_begin, front_size });
                    if (back_size != 0)
                        replacement.push_back({ back_begin, back_size });

                    if (replacement.empty())
                    {
                        target.m_free_ranges.erase(
                            target.m_free_ranges.begin() + (std::ptrdiff_t)i);
                    }
                    else
                    {
                        target.m_free_ranges[i] = replacement[0];
                        if (replacement.size() == 2)
                        {
                            target.m_free_ranges.insert(
                                target.m_free_ranges.begin() + (std::ptrdiff_t)i + 1,
                                replacement[1]);
                        }
                    }

                    out.m_memory = target.m_memory;
                    out.m_offset = aligned;
                    out.m_size = size;
                    out.m_mapped = target.m_mapped != nullptr
                        ? (uint8_t*)target.m_mapped + aligned
                        : nullptr;
                    return true;
                }
                return false;
            }

            allocation allocate(
                const VkMemoryRequirements& requirements,
                VkMemoryPropertyFlags properties,
                VkDeviceSize extra_alignment)
            {
                const VkDeviceSize alignment =
                    std::max(requirements.alignment, extra_alignment);
                const uint32_t memory_type = m_context->find_memory_type(
                    requirements.memoryTypeBits, properties);
                const bool host_visible =
                    0 != (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

                for (auto& block : m_blocks)
                {
                    if (block.m_memory_type != memory_type)
                        continue;

                    ensure_block_mapped(block, host_visible);

                    allocation result;
                    if (try_allocate_from_block(
                        block, requirements.size, alignment, result))
                    {
                        return result;
                    }
                }

                const VkDeviceSize default_block_size = host_visible
                    ? m_default_host_block_size
                    : m_default_device_block_size;
                VkDeviceSize block_size = std::max<VkDeviceSize>(
                    default_block_size, requirements.size + alignment);
                block_size = align_up(block_size, 4096);

                VkMemoryAllocateInfo allocate_info = {};
                allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocate_info.pNext = nullptr;
                allocate_info.allocationSize = block_size;
                allocate_info.memoryTypeIndex = memory_type;

                VkDeviceMemory memory = VK_NULL_HANDLE;
                JE_VK_CHECK(m_context->vkAllocateMemory(
                    m_context->_vk_logic_device,
                    &allocate_info,
                    nullptr,
                    &memory));

                block new_block;
                new_block.m_memory = memory;
                new_block.m_size = block_size;
                new_block.m_memory_type = memory_type;
                new_block.m_free_ranges.push_back({ 0, block_size });
                m_blocks.push_back(std::move(new_block));

                ensure_block_mapped(m_blocks.back(), host_visible);

                allocation result;
                if (!try_allocate_from_block(
                    m_blocks.back(), requirements.size, alignment, result))
                {
                    jeecs::debug::logfatal(
                        "VK120: failed to suballocate from a freshly created memory block.");
                }
                return result;
            }

            void free(const allocation& allocation)
            {
                if (allocation.m_memory == VK_NULL_HANDLE || allocation.m_size == 0)
                    return;

                for (auto& block : m_blocks)
                {
                    if (block.m_memory != allocation.m_memory)
                        continue;

                    if (allocation.m_offset + allocation.m_size > block.m_size)
                    {
                        jeecs::debug::logfatal(
                            "VK120: freeing a range outside its memory block.");
                    }

                    for (const auto& range : block.m_free_ranges)
                    {
                        const bool overlaps =
                            allocation.m_offset < range.first + range.second
                            && range.first < allocation.m_offset + allocation.m_size;
                        if (overlaps)
                        {
                            jeecs::debug::logfatal(
                                "VK120: double free or overlapping memory range detected.");
                        }
                    }

                    block.m_free_ranges.push_back(
                        { allocation.m_offset, allocation.m_size });
                    std::sort(
                        block.m_free_ranges.begin(), block.m_free_ranges.end());

                    std::vector<std::pair<VkDeviceSize, VkDeviceSize>> merged;
                    for (const auto& range : block.m_free_ranges)
                    {
                        if (!merged.empty()
                            && merged.back().first + merged.back().second
                            == range.first)
                        {
                            merged.back().second += range.second;
                        }
                        else
                        {
                            merged.push_back(range);
                        }
                    }
                    block.m_free_ranges = std::move(merged);
                    return;
                }

                jeecs::debug::logfatal(
                    "VK120: freeing an allocation from an unknown memory block.");
            }
        };

        descriptor_set_allocator* _vk_descriptor_set_allocator = nullptr;
        memory_allocator* _vk_memory_allocator = nullptr;

        VkSwapchainKHR _vk_swapchain = VK_NULL_HANDLE;
        std::vector<swapchain_image_content*> _vk_swapchain_images;

        // 每个交换链图像一个 render-finished 信号量。按帧槽复用会让信号量
        // 在 present 仍等待它时被下一次提交再次 signal（VUID-vkQueueSubmit-
        // pSignalSemaphores-00067）。重新 acquire 同一张图即可保证其 present
        // 已完成，此时复用对应信号量是安全的。
        std::vector<VkSemaphore> _vk_image_render_finished_semaphores;

        VkSampler _vk_dear_imgui_sampler = VK_NULL_HANDLE;
        VkDescriptorPool _vk_dear_imgui_descriptor_pool = VK_NULL_HANDLE;

        VkCommandPool _vk_frame_command_pool = VK_NULL_HANDLE;
        VkCommandPool _vk_temp_command_pool = VK_NULL_HANDLE;

        std::array<frame_resources, FRAMES_IN_FLIGHT> _vk_frames = {};
        uint32_t _vk_current_frame_index = 0;

        // Following is runtime vk states.
        uint32_t _vk_presenting_swapchain_image_index = UINT32_MAX;
        swapchain_image_content* _vk_current_swapchain_image_content = nullptr;

        jevk12_framebuffer* _vk_current_target_framebuffer = nullptr;
        VkCommandBuffer _vk_current_command_buffer = VK_NULL_HANDLE;
        jevk12_shader* _vk_current_binded_shader = nullptr;
        jegl_pipeline* _vk_current_pipeline = nullptr;
        jevk12_vertex* _vk_current_vertex = nullptr;
        jegl_shader* _vk_current_shader_resource = nullptr;

        bool _vk_frame_recording = false;
        bool _vk_frame_has_swapchain_image = false;

        bool _vk_pending_present = false;
        uint32_t _vk_pending_present_image_index = UINT32_MAX;
        bool _vk_swapchain_out_of_date = false;
        bool _vk_imgui_initialized = false;

        VkPipelineCache _vk_pipeline_cache = VK_NULL_HANDLE;
        VkDeviceSize _vk_min_uniform_buffer_offset_alignment = 1;

        using pso_key_t =
            std::tuple<jegl_pipeline*, jevk12_framebuffer*, uint32_t, uint32_t, uint32_t>;
        std::map<pso_key_t, VkPipeline> _vk_pso_cache;

        using render_pass_key_t =
            std::tuple<std::vector<VkFormat>, VkFormat, VkFormat, uint32_t,
                int, int, int, int, int, int>;
        std::map<render_pass_key_t, VkRenderPass> _vk_render_pass_cache;

        std::unordered_map<VkImageView, VkDescriptorSet> _vk_imgui_texture_cache;

        frame_resources& current_frame()
        {
            assert(_vk_current_frame_index < FRAMES_IN_FLIGHT);
            return _vk_frames[_vk_current_frame_index];
        }

        /////////////////////////////////////////////////////////////////////
        VkCommandBuffer begin_temp_command_buffer_records()
        {
            VkCommandBufferAllocateInfo allocate_info = {};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.pNext = nullptr;
            allocate_info.commandPool = _vk_temp_command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;

            VkCommandBuffer result = VK_NULL_HANDLE;
            JE_VK_CHECK(vkAllocateCommandBuffers(
                _vk_logic_device, &allocate_info, &result));

            VkCommandBufferBeginInfo command_buffer_begin_info = {};
            command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            command_buffer_begin_info.pNext = nullptr;
            command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            command_buffer_begin_info.pInheritanceInfo = nullptr;

            JE_VK_CHECK(vkBeginCommandBuffer(result, &command_buffer_begin_info));
            return result;
        }
        void end_temp_command_buffer_record(VkCommandBuffer cmd_buffer)
        {
            JE_VK_CHECK(vkEndCommandBuffer(cmd_buffer));

            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &cmd_buffer;

            JE_VK_CHECK(vkQueueSubmit(
                _vk_logic_device_graphic_queue, 1, &submit_info, _vk_queue_commit_fence));
            JE_VK_CHECK(vkWaitForFences(
                _vk_logic_device, 1, &_vk_queue_commit_fence, VK_TRUE, UINT64_MAX));
            JE_VK_CHECK(vkResetFences(_vk_logic_device, 1, &_vk_queue_commit_fence));

            vkFreeCommandBuffers(
                _vk_logic_device, _vk_temp_command_pool, 1, &cmd_buffer);
        }

        VkSemaphore create_semaphore()
        {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_create_info.pNext = nullptr;
            semaphore_create_info.flags = 0;

            VkSemaphore result = VK_NULL_HANDLE;
            if (VK_SUCCESS != vkCreateSemaphore(
                _vk_logic_device,
                &semaphore_create_info,
                nullptr,
                &result))
            {
                jeecs::debug::logfatal("Failed to create vk120 semaphore.");
            }
            return result;
        }
        void destroy_semaphore(VkSemaphore semaphore)
        {
            vkDestroySemaphore(_vk_logic_device, semaphore, nullptr);
        }

        VkFence create_fence(bool signaled = false)
        {
            VkFenceCreateInfo fence_create_info = {};
            fence_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_create_info.pNext = nullptr;
            fence_create_info.flags =
                signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

            VkFence result = VK_NULL_HANDLE;
            JE_VK_CHECK(vkCreateFence(
                _vk_logic_device, &fence_create_info, nullptr, &result));
            return result;
        }
        void destroy_fence(VkFence fence)
        {
            if (fence != VK_NULL_HANDLE)
                vkDestroyFence(_vk_logic_device, fence, nullptr);
        }

        void create_frame_descriptor_pool(frame_resources& frame)
        {
            constexpr uint32_t MAX_FRAME_DESCRIPTOR_SETS = 2048;

            VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_FRAME_DESCRIPTOR_SETS },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAME_DESCRIPTOR_SETS * 16 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_FRAME_DESCRIPTOR_SETS * 16 },
                { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_FRAME_DESCRIPTOR_SETS * 16 } };

            VkDescriptorPoolCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            create_info.pNext = nullptr;
            create_info.flags = 0;
            create_info.maxSets = MAX_FRAME_DESCRIPTOR_SETS;
            create_info.poolSizeCount =
                (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize));
            create_info.pPoolSizes = pool_sizes;

            JE_VK_CHECK(vkCreateDescriptorPool(
                _vk_logic_device, &create_info, nullptr, &frame.m_descriptor_pool));
        }

        void create_frame_uniform_ring(frame_resources& frame)
        {
            constexpr VkDeviceSize UBO_RING_SIZE = 8 * 1024 * 1024;

            alloc_vk_device_buffer_memory(
                (size_t)UBO_RING_SIZE,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &frame.m_ubo_ring_buffer,
                &frame.m_ubo_ring_memory,
                &frame.m_ubo_ring_memory_offset,
                &frame.m_ubo_ring_memory_size,
                &frame.m_ubo_ring_mapped);

            frame.m_ubo_ring_size = UBO_RING_SIZE;
            frame.m_ubo_ring_offset = 0;
        }

        void create_frame_resources()
        {
            VkCommandPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.pNext = nullptr;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = _vk_device_queue_graphic_family_index;

            JE_VK_CHECK(vkCreateCommandPool(
                _vk_logic_device, &pool_info, nullptr, &_vk_frame_command_pool));

            pool_info.flags =
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            JE_VK_CHECK(vkCreateCommandPool(
                _vk_logic_device, &pool_info, nullptr, &_vk_temp_command_pool));

            for (auto& frame : _vk_frames)
            {
                VkCommandBufferAllocateInfo allocate_info = {};
                allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocate_info.pNext = nullptr;
                allocate_info.commandPool = _vk_frame_command_pool;
                allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate_info.commandBufferCount = 1;

                JE_VK_CHECK(vkAllocateCommandBuffers(
                    _vk_logic_device, &allocate_info, &frame.m_command_buffer));

                frame.m_fence = create_fence(true);
                frame.m_image_available = create_semaphore();
                frame.m_recording = false;
                frame.m_has_swapchain_image = false;
                frame.m_submitted = false;

                create_frame_descriptor_pool(frame);
                create_frame_uniform_ring(frame);
            }

            _vk_current_frame_index = 0;
            _vk_frame_recording = false;
            _vk_frame_has_swapchain_image = false;
        }

        void destroy_frame_resources()
        {
            for (auto& frame : _vk_frames)
            {
                if (frame.m_ubo_ring_buffer != VK_NULL_HANDLE)
                    vkDestroyBuffer(_vk_logic_device, frame.m_ubo_ring_buffer, nullptr);
                if (frame.m_ubo_ring_memory != VK_NULL_HANDLE)
                {
                    free_vk_memory(
                        frame.m_ubo_ring_memory,
                        frame.m_ubo_ring_memory_offset,
                        frame.m_ubo_ring_memory_size);
                }

                destroy_fence(frame.m_fence);
                destroy_semaphore(frame.m_image_available);

                if (frame.m_descriptor_pool != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(
                        _vk_logic_device, frame.m_descriptor_pool, nullptr);
                }

                if (frame.m_command_buffer != VK_NULL_HANDLE)
                {
                    vkFreeCommandBuffers(
                        _vk_logic_device,
                        _vk_frame_command_pool,
                        1,
                        &frame.m_command_buffer);
                }
            }

            if (_vk_temp_command_pool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(
                    _vk_logic_device, _vk_temp_command_pool, nullptr);
                _vk_temp_command_pool = VK_NULL_HANDLE;
            }
            if (_vk_frame_command_pool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(
                    _vk_logic_device, _vk_frame_command_pool, nullptr);
                _vk_frame_command_pool = VK_NULL_HANDLE;
            }
        }

        VkRenderPass get_or_create_render_pass(
            const std::vector<jevk12_texture*>& color_attachments,
            jevk12_texture* resolve_attachment,
            jevk12_texture* depth_attachment,
            VkImageLayout color_initial_layout,
            VkImageLayout color_final_layout,
            VkImageLayout resolve_initial_layout,
            VkImageLayout resolve_final_layout,
            VkImageLayout depth_initial_layout,
            VkImageLayout depth_final_layout,
            VkSampleCountFlagBits sample_count)
        {
            if (resolve_attachment != nullptr && color_attachments.size() != 1)
            {
                jeecs::debug::logfatal(
                    "VK120: multisample resolve is only supported with a single color attachment.");
            }

            std::vector<VkFormat> color_formats;
            color_formats.reserve(color_attachments.size());
            for (auto* color : color_attachments)
                color_formats.push_back(color->m_vk_texture_format);

            const VkFormat depth_format =
                depth_attachment != nullptr ? depth_attachment->m_vk_texture_format : VK_FORMAT_UNDEFINED;
            const VkFormat resolve_format =
                resolve_attachment != nullptr ? resolve_attachment->m_vk_texture_format : VK_FORMAT_UNDEFINED;

            render_pass_key_t key{
                color_formats,
                depth_format,
                resolve_format,
                (uint32_t)sample_count,
                (int)color_initial_layout,
                (int)color_final_layout,
                (int)depth_initial_layout,
                (int)depth_final_layout,
                (int)resolve_initial_layout,
                (int)resolve_final_layout };

            auto found = _vk_render_pass_cache.find(key);
            if (found != _vk_render_pass_cache.end())
                return found->second;

            const size_t resolve_index = color_attachments.size();
            const size_t depth_index =
                color_attachments.size() + (resolve_attachment == nullptr ? 0 : 1);

            std::vector<VkAttachmentDescription> attachments(
                color_attachments.size()
                + (resolve_attachment == nullptr ? 0 : 1)
                + (depth_attachment == nullptr ? 0 : 1));
            std::vector<VkAttachmentReference> color_refs(color_attachments.size());
            VkAttachmentReference resolve_ref = {};
            VkAttachmentReference depth_ref = {};

            for (size_t i = 0; i < color_attachments.size(); ++i)
            {
                auto& attachment = attachments[i];
                attachment.flags = 0;
                attachment.format = color_attachments[i]->m_vk_texture_format;
                attachment.samples = sample_count;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = color_initial_layout;
                attachment.finalLayout = color_final_layout;

                color_refs[i].attachment = (uint32_t)i;
                color_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            if (resolve_attachment != nullptr)
            {
                auto& attachment = attachments[resolve_index];
                attachment.flags = 0;
                attachment.format = resolve_attachment->m_vk_texture_format;
                attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = resolve_initial_layout;
                attachment.finalLayout = resolve_final_layout;

                resolve_ref.attachment = (uint32_t)resolve_index;
                resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            if (depth_attachment != nullptr)
            {
                auto& attachment = attachments[depth_index];
                attachment.flags = 0;
                attachment.format = depth_attachment->m_vk_texture_format;
                attachment.samples = sample_count;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = depth_initial_layout;
                attachment.finalLayout = depth_final_layout;

                depth_ref.attachment = (uint32_t)depth_index;
                depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            VkSubpassDescription subpass = {};
            subpass.flags = 0;
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.inputAttachmentCount = 0;
            subpass.pInputAttachments = nullptr;
            subpass.colorAttachmentCount = (uint32_t)color_refs.size();
            subpass.pColorAttachments = color_refs.empty() ? nullptr : color_refs.data();
            subpass.pResolveAttachments =
                resolve_attachment != nullptr ? &resolve_ref : nullptr;
            subpass.pDepthStencilAttachment =
                depth_attachment != nullptr ? &depth_ref : nullptr;
            subpass.preserveAttachmentCount = 0;
            subpass.pPreserveAttachments = nullptr;

            VkSubpassDependency dependencies[2] = {};

            auto& entry_dependency = dependencies[0];
            entry_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            entry_dependency.dstSubpass = 0;
            entry_dependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                | VK_PIPELINE_STAGE_TRANSFER_BIT;
            entry_dependency.srcAccessMask =
                VK_ACCESS_SHADER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            entry_dependency.dstStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            entry_dependency.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            if (depth_attachment != nullptr)
            {
                entry_dependency.srcStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                entry_dependency.srcAccessMask |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                entry_dependency.dstStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                entry_dependency.dstAccessMask |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            auto& exit_dependency = dependencies[1];
            exit_dependency.srcSubpass = 0;
            exit_dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
            exit_dependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            exit_dependency.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            exit_dependency.dstStageMask =
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_TRANSFER_BIT;
            exit_dependency.dstAccessMask =
                VK_ACCESS_SHADER_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            if (depth_attachment != nullptr)
            {
                exit_dependency.srcStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                exit_dependency.srcAccessMask |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                exit_dependency.dstStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                exit_dependency.dstAccessMask |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            VkRenderPassCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            create_info.pNext = nullptr;
            create_info.flags = 0;
            create_info.attachmentCount = (uint32_t)attachments.size();
            create_info.pAttachments = attachments.empty() ? nullptr : attachments.data();
            create_info.subpassCount = 1;
            create_info.pSubpasses = &subpass;
            create_info.dependencyCount = 2;
            create_info.pDependencies = dependencies;

            VkRenderPass render_pass = VK_NULL_HANDLE;
            JE_VK_CHECK(vkCreateRenderPass(
                _vk_logic_device, &create_info, nullptr, &render_pass));

            _vk_render_pass_cache.emplace(std::move(key), render_pass);
            return render_pass;
        }

        jevk12_framebuffer* create_frame_buffer(
            size_t w,
            size_t h,
            const std::vector<jevk12_texture*>& attachment_colors,
            jevk12_texture* resolve_attachment,
            jevk12_texture* attachment_depth,
            VkImageLayout final_layout,
            bool is_screen_framebuffer)
        {
            jevk12_framebuffer* result = new jevk12_framebuffer{};
            result->m_width = w;
            result->m_height = h;
            result->m_is_screen_framebuffer = is_screen_framebuffer;
            result->m_color_attachments = attachment_colors;
            result->m_resolve_attachment = resolve_attachment;
            result->m_depth_attachment = attachment_depth;

            VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
            if (!attachment_colors.empty())
                sample_count = (VkSampleCountFlagBits)attachment_colors.front()->m_samples;
            else if (attachment_depth != nullptr)
                sample_count = (VkSampleCountFlagBits)attachment_depth->m_samples;

            for (auto* color : attachment_colors)
            {
                if ((VkSampleCountFlagBits)color->m_samples != sample_count)
                {
                    jeecs::debug::logfatal(
                        "VK120: framebuffer color attachments have mismatched sample counts.");
                }
            }
            if (attachment_depth != nullptr
                && (VkSampleCountFlagBits)attachment_depth->m_samples != sample_count)
            {
                jeecs::debug::logfatal(
                    "VK120: framebuffer depth attachment sample count does not match color attachments.");
            }
            if (resolve_attachment != nullptr && resolve_attachment->m_samples != 1)
            {
                jeecs::debug::logfatal(
                    "VK120: the resolve target must be a single-sample image.");
            }
            result->m_sample_count = sample_count;

            // 多重采样颜色附件不能作为普通纹理采样，因此固定在 COLOR_ATTACHMENT 布局。
            result->m_color_final_layout =
                sample_count != VK_SAMPLE_COUNT_1_BIT
                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : final_layout;
            result->m_resolve_final_layout = final_layout;
            result->m_depth_final_layout =
                (final_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                    || sample_count != VK_SAMPLE_COUNT_1_BIT)
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                : final_layout;

            // 离屏深度附件需要能在后续 pass 中作为纹理采样；在创建 framebuffer 时
            // 把它迁到 render pass 的 canonical initial/final layout。
            if (!is_screen_framebuffer
                && attachment_depth != nullptr
                && attachment_depth->m_current_layout != result->m_depth_final_layout)
            {
                transition_image_layout(
                    attachment_depth, result->m_depth_final_layout);
            }

            // 屏幕 framebuffer 的交换链图像在 acquire 前不能 transition，
            // 首帧会在自己的命令缓冲里从 UNDEFINED 迁到 PRESENT_SRC；
            // render pass 的 canonical initial layout 仍按 PRESENT_SRC 记录。
            result->m_color_initial_layout = attachment_colors.empty()
                ? result->m_color_final_layout
                : (is_screen_framebuffer && sample_count == VK_SAMPLE_COUNT_1_BIT
                    ? final_layout
                    : attachment_colors.front()->m_current_layout);
            result->m_resolve_initial_layout = resolve_attachment != nullptr
                ? (is_screen_framebuffer
                    ? final_layout
                    : resolve_attachment->m_current_layout)
                : result->m_resolve_final_layout;
            result->m_depth_initial_layout = attachment_depth != nullptr
                ? attachment_depth->m_current_layout
                : result->m_depth_final_layout;

            result->m_rendpass = get_or_create_render_pass(
                attachment_colors,
                resolve_attachment,
                attachment_depth,
                result->m_color_initial_layout,
                result->m_color_final_layout,
                result->m_resolve_initial_layout,
                result->m_resolve_final_layout,
                result->m_depth_initial_layout,
                result->m_depth_final_layout,
                sample_count);

            std::vector<VkImageView> attachment_image_views;
            attachment_image_views.reserve(
                result->m_color_attachments.size()
                + (resolve_attachment == nullptr ? 0 : 1)
                + (attachment_depth == nullptr ? 0 : 1));

            for (auto* color : result->m_color_attachments)
                attachment_image_views.push_back(color->m_vk_texture_image_view);
            if (resolve_attachment != nullptr)
                attachment_image_views.push_back(resolve_attachment->m_vk_texture_image_view);
            if (attachment_depth != nullptr)
                attachment_image_views.push_back(attachment_depth->m_vk_texture_image_view);

            VkFramebufferCreateInfo framebuffer_create_info = {};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.pNext = nullptr;
            framebuffer_create_info.flags = 0;
            framebuffer_create_info.renderPass = result->m_rendpass;
            framebuffer_create_info.attachmentCount = (uint32_t)attachment_image_views.size();
            framebuffer_create_info.pAttachments = attachment_image_views.data();
            framebuffer_create_info.width = (uint32_t)w;
            framebuffer_create_info.height = (uint32_t)h;
            framebuffer_create_info.layers = 1;

            JE_VK_CHECK(vkCreateFramebuffer(
                _vk_logic_device, &framebuffer_create_info, nullptr, &result->m_framebuffer));

            return result;
        }
        void destroy_srgb_encode_screen_target()
        {
            if (_vk_srgb_encode_screen_framebuffer != nullptr)
            {
                destroy_frame_buffer(_vk_srgb_encode_screen_framebuffer);
                _vk_srgb_encode_screen_framebuffer = nullptr;
            }
            if (_vk_srgb_encode_screen_depth_texture != nullptr)
            {
                destroy_texture_instance(
                    _vk_srgb_encode_screen_depth_texture);
                _vk_srgb_encode_screen_depth_texture = nullptr;
            }
            if (_vk_srgb_encode_screen_color_resource != nullptr)
            {
                jegl_close_texture_2d(
                    _vk_srgb_encode_screen_color_resource);
                _vk_srgb_encode_screen_color_resource = nullptr;
            }
            _vk_srgb_encode_screen_color_texture = nullptr;
            _vk_srgb_encode_screen_width = 0;
            _vk_srgb_encode_screen_height = 0;
        }
        bool ensure_srgb_encode_screen_target(size_t width, size_t height)
        {
            if (width == 0 || height == 0)
                return false;

            if (_vk_srgb_encode_screen_framebuffer != nullptr
                && _vk_srgb_encode_screen_color_resource != nullptr
                && _vk_srgb_encode_screen_width == width
                && _vk_srgb_encode_screen_height == height)
                return true;

            destroy_srgb_encode_screen_target();

            _vk_srgb_encode_screen_color_resource = jegl_create_texture_2d(
                width, height,
                (jegl_texture_common::format)(
                    jegl_texture_common::format::FRAMEBUF
                    | jegl_texture_common::format::RGBA));
            if (!_jegl_prepare_texture_2d_resource(
                    _vk_srgb_encode_screen_color_resource))
            {
                jeecs::debug::logerr(
                    "VK120: failed to create the linear offscreen screen texture.");
                destroy_srgb_encode_screen_target();
                return false;
            }

            _vk_srgb_encode_screen_color_texture =
                static_cast<jevk12_texture*>(
                    _vk_srgb_encode_screen_color_resource->m_handle.m_ptr);

            // 回退路径固定使用 1x 离屏目标，避免依赖与 surface 格式不同的
            // MSAA 支持；编码 pass 仍可写入交换链自身的 MSAA framebuffer。
            _vk_srgb_encode_screen_depth_texture = create_framebuf_texture(
                (jegl_texture_common::format)(
                    jegl_texture_common::format::DEPTH
                    | jegl_texture_common::format::FRAMEBUF),
                width, height,
                VK_SAMPLE_COUNT_1_BIT);

            _vk_srgb_encode_screen_framebuffer = create_frame_buffer(
                width, height,
                { _vk_srgb_encode_screen_color_texture },
                nullptr,
                _vk_srgb_encode_screen_depth_texture,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                false);

            _vk_srgb_encode_screen_width = width;
            _vk_srgb_encode_screen_height = height;
            return true;
        }
        void destroy_pipelines_for_framebuffer(jevk12_framebuffer* fb)
        {
            for (auto it = _vk_pso_cache.begin(); it != _vk_pso_cache.end();)
            {
                if (std::get<1>(it->first) == fb)
                {
                    vkDestroyPipeline(_vk_logic_device, it->second, nullptr);
                    it = _vk_pso_cache.erase(it);
                }
                else
                    ++it;
            }
        }
        void destroy_frame_buffer(jevk12_framebuffer* fb)
        {
            if (fb == nullptr)
                return;

            destroy_pipelines_for_framebuffer(fb);

            if (fb->m_framebuffer != VK_NULL_HANDLE)
                vkDestroyFramebuffer(_vk_logic_device, fb->m_framebuffer, nullptr);

            // m_rendpass 由 render pass 缓存持有，不能在这里销毁。
            delete fb;
        }
        void destroy_swap_chain()
        {
            for (auto* image : _vk_swapchain_images)
                delete image;

            _vk_swapchain_images.clear();

            destroy_srgb_encode_screen_target();

            for (auto semaphore : _vk_image_render_finished_semaphores)
                destroy_semaphore(semaphore);
            _vk_image_render_finished_semaphores.clear();

            _vk_current_swapchain_image_content = nullptr;
            _vk_presenting_swapchain_image_index = UINT32_MAX;

            if (_vk_swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(_vk_logic_device, _vk_swapchain, nullptr);
                _vk_swapchain = VK_NULL_HANDLE;
            }
        }

        void recreate_swap_chain_for_current_surface()
        {
            int _w, _h;
            je_io_get_window_size(&_w, &_h);

            RESOLUTION_WIDTH = (size_t)_w;
            RESOLUTION_HEIGHT = (size_t)_h;

            // 在此处开始创建交换链
            JE_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                _vk_device, _vk_surface, &_vk_surface_capabilities));

            VkExtent2D used_extent = { (uint32_t)RESOLUTION_WIDTH, (uint32_t)RESOLUTION_HEIGHT };
            if (_vk_surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                used_extent = _vk_surface_capabilities.currentExtent;
            }
            else
            {
                used_extent.width = std::max(
                    _vk_surface_capabilities.minImageExtent.width,
                    std::min(
                        _vk_surface_capabilities.maxImageExtent.width,
                        used_extent.width));
                used_extent.height =
                    std::max(
                        _vk_surface_capabilities.minImageExtent.height,
                        std::min(
                            _vk_surface_capabilities.maxImageExtent.height,
                            used_extent.height));
            }

            // 获取交换链支持的格式
            uint32_t vk_surface_format_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(_vk_device, _vk_surface, &vk_surface_format_count, nullptr);

            std::vector<VkSurfaceFormatKHR> vk_surface_formats(vk_surface_format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(_vk_device, _vk_surface, &vk_surface_format_count, vk_surface_formats.data());
            if (vk_surface_formats.empty())
                jeecs::debug::logfatal("VK120: the surface reports no supported formats.");

            // 获取交换链支持的呈现模式
            uint32_t vk_present_mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(_vk_device, _vk_surface, &vk_present_mode_count, nullptr);

            std::vector<VkPresentModeKHR> vk_present_modes(vk_present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(_vk_device, _vk_surface, &vk_present_mode_count, vk_present_modes.data());
            if (vk_present_modes.empty())
                jeecs::debug::logfatal("VK120: the surface reports no supported present modes.");

            // Prefer an sRGB swapchain: scene shaders output linear values and
            // the backbuffer write performs the final linear-to-sRGB encode.
            _vk_surface_format = {};
            if (vk_surface_formats.size() == 1
                && vk_surface_formats.front().format == VK_FORMAT_UNDEFINED)
            {
                _vk_surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
                _vk_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }
            else
            {
                const VkFormat preferred_formats[] = {
                    VK_FORMAT_B8G8R8A8_SRGB,
                    VK_FORMAT_R8G8B8A8_SRGB,
                };
                bool found_support_format = false;
                for (VkFormat preferred : preferred_formats)
                {
                    for (auto& format : vk_surface_formats)
                    {
                        if (format.format == preferred
                            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                        {
                            found_support_format = true;
                            _vk_surface_format = format;
                            break;
                        }
                    }
                    if (found_support_format)
                        break;
                }
                if (!found_support_format)
                {
                    // Last resort: an 8-bit UNORM surface. The engine still
                    // renders linear, but this surface cannot hardware-encode.
                    for (auto& format : vk_surface_formats)
                    {
                        if (format.format == VK_FORMAT_B8G8R8A8_UNORM
                            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                        {
                            found_support_format = true;
                            _vk_surface_format = format;
                            break;
                        }
                    }
                }
                if (!found_support_format)
                    _vk_surface_format = vk_surface_formats.front();
            }

            // No sRGB surface format: render normally and run a full-screen
            // linear-to-sRGB encode pass before presenting.
            _vk_use_srgb_encode_pass =
                !_vk_format_is_srgb(_vk_surface_format.format);
            if (_vk_use_srgb_encode_pass)
            {
                jeecs::debug::logwarn(
                    "VK120: surface format %d has no sRGB variant; "
                    "using the full-screen sRGB encode pass.",
                    (int)_vk_surface_format.format);
            }

            // 获取受支持的深度数据格式
            _vk_depth_format = VK_FORMAT_UNDEFINED;
            std::vector<VkFormat> vk_depth_formats = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM };

            for (auto& format : vk_depth_formats)
            {
                VkFormatProperties format_properties;
                vkGetPhysicalDeviceFormatProperties(_vk_device, format, &format_properties);

                if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                {
                    _vk_depth_format = format;
                    break;
                }
            }

            if (_vk_depth_format == VK_FORMAT_UNDEFINED)
            {
                jeecs::debug::logfatal("Failed to find suitable depth format.");
            }

            // 选择当前设备和表面格式实际支持的多重采样数量。
            _vk_active_msaa_samples = VK_SAMPLE_COUNT_1_BIT;
            if (_vk_msaa_config > 1)
            {
                VkPhysicalDeviceProperties device_properties = {};
                vkGetPhysicalDeviceProperties(_vk_device, &device_properties);

                VkSampleCountFlags supported_samples =
                    device_properties.limits.framebufferColorSampleCounts
                    & device_properties.limits.framebufferDepthSampleCounts;

                VkImageFormatProperties format_properties = {};
                if (vkGetPhysicalDeviceImageFormatProperties(
                    _vk_device,
                    _vk_surface_format.format,
                    VK_IMAGE_TYPE_2D,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    0,
                    &format_properties) == VK_SUCCESS)
                {
                    supported_samples &= format_properties.sampleCounts;
                }
                else
                {
                    supported_samples = 0;
                }

                VkSampleCountFlagBits requested =
                    clamp_sample_count((uint32_t)_vk_msaa_config);
                while (requested != VK_SAMPLE_COUNT_1_BIT
                    && 0 == (supported_samples & requested))
                {
                    requested = (VkSampleCountFlagBits)((uint32_t)requested >> 1);
                }

                if (requested != VK_SAMPLE_COUNT_1_BIT)
                {
                    VkImageFormatProperties depth_format_properties = {};
                    if (vkGetPhysicalDeviceImageFormatProperties(
                        _vk_device,
                        _vk_depth_format,
                        VK_IMAGE_TYPE_2D,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        0,
                        &depth_format_properties) == VK_SUCCESS)
                    {
                        while (requested != VK_SAMPLE_COUNT_1_BIT
                            && 0 == (depth_format_properties.sampleCounts & requested))
                        {
                            requested =
                                (VkSampleCountFlagBits)((uint32_t)requested >> 1);
                        }
                    }
                    else
                    {
                        requested = VK_SAMPLE_COUNT_1_BIT;
                    }
                }

                _vk_active_msaa_samples = requested;
                if (requested == VK_SAMPLE_COUNT_1_BIT)
                {
                    jeecs::debug::logwarn(
                        "VK120: MSAA x%zu is not supported by this device/format, "
                        "rendering will run at 1x.",
                        _vk_msaa_config);
                }
            }

            // 选择呈现模式。注意:MAILBOX 与 FIFO 都是"无撕裂、受刷新率限制"的同步模式，
            // 只有 IMMEDIATE 才是真正的不限帧/可能撕裂模式。
            // 这里的策略是:
            //   _vk_vsync_config == true (config->m_fps == 0) -> 严格 FIFO(标准 VSync);
            //   _vk_vsync_config == false -> 优先 MAILBOX(低延迟但仍无撕裂), 否则 IMMEDIATE。
            bool vk_present_mode_mailbox_supported = false;
            bool vk_present_mode_fifo_supported = false;
            bool vk_present_mode_immediate_supported = false;

            for (auto& present_mode : vk_present_modes)
            {
                if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                    vk_present_mode_mailbox_supported = true;
                else if (present_mode == VK_PRESENT_MODE_FIFO_KHR)
                    vk_present_mode_fifo_supported = true;
                else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
                    vk_present_mode_immediate_supported = true;
            }

            if (vk_present_mode_fifo_supported && _vk_vsync_config)
                _vk_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            else if (vk_present_mode_mailbox_supported && !_vk_vsync_config)
                _vk_surface_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (vk_present_mode_immediate_supported)
                _vk_surface_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            else if (vk_present_mode_fifo_supported)
                // FIFO 由 Vulkan 保证一定可用，作为最后的兜底
                _vk_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            else
            {
                jeecs::debug::logfatal("Failed to create vk120 swapchain, unsupported present mode.");
            }

            _vk_presenting_swapchain_image_index = UINT32_MAX;

            // 尝试三重缓冲？
            uint32_t swapchain_image_count = _vk_surface_capabilities.minImageCount + 1;
            if (swapchain_image_count > _vk_surface_capabilities.maxImageCount)
                swapchain_image_count = _vk_surface_capabilities.maxImageCount;

            VkSwapchainCreateInfoKHR swapchain_create_info = {};
            swapchain_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchain_create_info.pNext = nullptr;
            swapchain_create_info.flags = 0;
            swapchain_create_info.surface = _vk_surface;
            swapchain_create_info.minImageCount = swapchain_image_count;
            swapchain_create_info.imageFormat = _vk_surface_format.format;
            swapchain_create_info.imageColorSpace = _vk_surface_format.colorSpace;
            swapchain_create_info.imageExtent = used_extent;
            swapchain_create_info.imageArrayLayers = 1;
            swapchain_create_info.imageUsage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            uint32_t queueFamilyIndices[] = {
                _vk_device_queue_graphic_family_index,
                _vk_device_queue_present_family_index };
            if (_vk_device_queue_graphic_family_index != _vk_device_queue_present_family_index)
            {
                // 如果图形簇和提交簇不是同一个，那么就完蛋了，得开CONCURRENT模式
                swapchain_create_info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_CONCURRENT;
                swapchain_create_info.queueFamilyIndexCount = 2;
                swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
            }
            else
            {
                // 如果图形簇和提交簇是同一个，那么就不用开CONCURRENT模式，获取更好的性能
                swapchain_create_info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
                swapchain_create_info.queueFamilyIndexCount = 0;
                swapchain_create_info.pQueueFamilyIndices = nullptr;
            }

            swapchain_create_info.preTransform = _vk_surface_capabilities.currentTransform;

            VkCompositeAlphaFlagBitsKHR composite_alpha =
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            if (0 == (_vk_surface_capabilities.supportedCompositeAlpha
                & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
            {
                const VkCompositeAlphaFlagBitsKHR candidates[] = {
                    VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
                composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
                for (auto candidate : candidates)
                {
                    if (0 != (_vk_surface_capabilities.supportedCompositeAlpha & candidate))
                    {
                        composite_alpha = candidate;
                        break;
                    }
                }
            }
            swapchain_create_info.compositeAlpha = composite_alpha;
            swapchain_create_info.presentMode = _vk_surface_present_mode;
            swapchain_create_info.clipped = VK_TRUE;

            swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

            // 调用方必须保证没有在途的帧；这里再等待一次，避免在录制/提交期间销毁交换链。
            JE_VK_CHECK(vkDeviceWaitIdle(_vk_logic_device));

            if (_vk_swapchain != VK_NULL_HANDLE)
                destroy_swap_chain();

            _vk_presenting_swapchain_image_index = UINT32_MAX;
            _vk_current_swapchain_image_content = nullptr;
            _vk_pending_present = false;
            _vk_pending_present_image_index = UINT32_MAX;
            _vk_swapchain_out_of_date = false;

            JE_VK_CHECK(vkCreateSwapchainKHR(
                _vk_logic_device, &swapchain_create_info, nullptr, &_vk_swapchain));

            uint32_t swapchain_real_image_count = 0;
            JE_VK_CHECK(vkGetSwapchainImagesKHR(
                _vk_logic_device, _vk_swapchain, &swapchain_real_image_count, nullptr));

            if (swapchain_real_image_count == 0)
                jeecs::debug::logfatal("VK120: swapchain returned no images.");

            std::vector<VkImage> swapchain_images(swapchain_real_image_count);
            JE_VK_CHECK(vkGetSwapchainImagesKHR(
                _vk_logic_device,
                _vk_swapchain,
                &swapchain_real_image_count,
                swapchain_images.data()));

            _vk_swapchain_images.resize(swapchain_real_image_count);

            _vk_image_render_finished_semaphores.reserve(swapchain_real_image_count);
            for (uint32_t i = 0; i < swapchain_real_image_count; ++i)
                _vk_image_render_finished_semaphores.push_back(create_semaphore());

            for (uint32_t i = 0; i < swapchain_real_image_count; ++i)
            {
                jevk12_texture* color_attachment =
                    create_framebuf_texture_with_swapchain_image(
                        swapchain_images[i],
                        _vk_surface_format.format,
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        used_extent.width,
                        used_extent.height);

                // 交换链图像在 acquire 之前不能做布局转换；首帧会在
                // begin_frame 后（acquire 之后）把它从 UNDEFINED 迁到
                // render pass 期望的 PRESENT_SRC。

                jevk12_texture* msaa_attachment = nullptr;
                jevk12_texture* framebuffer_color = color_attachment;

                if (_vk_active_msaa_samples != VK_SAMPLE_COUNT_1_BIT)
                {
                    msaa_attachment = new jevk12_texture{};
                    msaa_attachment->m_width = used_extent.width;
                    msaa_attachment->m_height = used_extent.height;

                    create_image(
                        msaa_attachment,
                        _vk_surface_format.format,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                        VK_IMAGE_TYPE_2D, 1, 1,
                        _vk_active_msaa_samples,
                        0,
                        VK_IMAGE_VIEW_TYPE_2D);
                    transition_image_layout(
                        msaa_attachment,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                    framebuffer_color = msaa_attachment;
                }

                jevk12_texture* depth_attachment = create_framebuf_texture(
                    (jegl_texture_common::format)(
                        jegl_texture_common::format::DEPTH
                        | jegl_texture_common::format::FRAMEBUF),
                    used_extent.width,
                    used_extent.height,
                    _vk_active_msaa_samples);

                auto* framebuffer = create_frame_buffer(
                    (size_t)used_extent.width,
                    (size_t)used_extent.height,
                    { framebuffer_color },
                    _vk_active_msaa_samples != VK_SAMPLE_COUNT_1_BIT
                    ? color_attachment
                    : nullptr,
                    depth_attachment,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    true);

                _vk_swapchain_images.at(i) =
                    new swapchain_image_content(
                        this, framebuffer, color_attachment, depth_attachment,
                        msaa_attachment);
            }

            if (_vk_use_srgb_encode_pass)
            {
                ensure_srgb_encode_screen_target(
                    used_extent.width, used_extent.height);
            }
        }

#define VK_API_DECL(name) PFN_##name name
        VK_API_LIST;
#undef VK_API_DECL

#ifndef NDEBUG
        VkDebugUtilsMessengerEXT _vk_debug_manager;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT _1,
            const VkDebugUtilsMessengerCallbackDataEXT* info,
            void* userdata)
        {
            // 同时写到 stderr，保证无控制台的 GUI 进程也能立即看到验证层输出。
            std::fprintf(stderr, "[Vulkan] %s\n", info->pMessage);
            std::fflush(stderr);

            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                debug::logerr("[Vulkan] %s", info->pMessage);
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                debug::logwarn("[Vulkan] %s", info->pMessage);
            else
                debug::log("[Vulkan] %s", info->pMessage);

            return VK_FALSE;
        }
#endif

        struct physics_device_info
        {
            uint32_t queue_graphic_family_index;
            uint32_t queue_present_family_index;
        };

        std::optional<physics_device_info> check_physics_device_is_suatable(
            VkPhysicalDevice device,
            bool vk_validation_layer_supported,
            const std::vector<const char*>& required_device_layers,
            const std::vector<const char*>& required_device_extensions)
        {
            assert(_vk_surface != VK_NULL_HANDLE);

            VkPhysicalDeviceProperties prop = {};
            vkGetPhysicalDeviceProperties(device, &prop);

            uint32_t vk_queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &vk_queue_family_count, nullptr);

            if (vk_queue_family_count == 0)
            {
                jeecs::debug::logwarn("Physical device '%s' has no queue families.", prop.deviceName);
                return std::nullopt;
            }

            std::vector<VkQueueFamilyProperties> vk_queue_families(vk_queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &vk_queue_family_count, vk_queue_families.data());

            bool is_graphic_device = false;
            uint32_t queue_graphic_family_index = 0;

            bool is_present_support = false;
            uint32_t queue_present_family_index = 0;

            for (auto& queue_family : vk_queue_families)
            {
                if (queue_family.queueCount > 0)
                {
                    auto family_index = (uint32_t)(&queue_family - vk_queue_families.data());

                    if (!is_graphic_device && 0 != (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    {
                        is_graphic_device = true;
                        queue_graphic_family_index = family_index;
                    }
                    if (!is_present_support)
                    {
                        VkBool32 present_support = false;
                        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
                            device, family_index, _vk_surface, &present_support);

                        // Fix: Add error handling for surface support check
                        if (result != VK_SUCCESS)
                        {
                            jeecs::debug::logwarn("Failed to check surface support for device '%s', family %u: %d",
                                prop.deviceName, family_index, result);
                            continue;
                        }

                        if (present_support)
                        {
                            is_present_support = true;
                            queue_present_family_index = family_index;
                        }
                    }
                    if (is_graphic_device && is_present_support)
                        break;
                }
            }

            if (is_graphic_device && is_present_support)
            {
                uint32_t vk_device_extension_count = 0;
                VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, nullptr);
                if (result != VK_SUCCESS)
                {
                    jeecs::debug::logwarn("Failed to enumerate device extensions for '%s': %d", prop.deviceName, result);
                    return std::nullopt;
                }

                std::vector<VkExtensionProperties> vk_device_extensions(vk_device_extension_count);
                result = vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, vk_device_extensions.data());
                if (result != VK_SUCCESS)
                {
                    jeecs::debug::logwarn("Failed to get device extension properties for '%s': %d", prop.deviceName, result);
                    return std::nullopt;
                }

                std::unordered_set<std::string> required_extensions(
                    required_device_extensions.begin(), required_device_extensions.end());

                for (auto& contained_extension : vk_device_extensions)
                {
                    required_extensions.erase(contained_extension.extensionName);
                }

                if (required_extensions.empty())
                {
                    physics_device_info result;
                    result.queue_graphic_family_index = queue_graphic_family_index;
                    result.queue_present_family_index = queue_present_family_index;

                    jeecs::debug::loginfo(
                        "VK120: suitable device '%s'.", prop.deviceName);

                    return std::optional(result);
                }
                else
                {
                    jeecs::debug::logwarn("Device '%s' missing required extensions:", prop.deviceName);
                    for (const auto& missing_ext : required_extensions)
                    {
                        jeecs::debug::logwarn("  - %s", missing_ext.c_str());
                    }
                }
            }
            return std::nullopt;
        }

        jegl_vk120_context(
            jegl_context* ctx,
            const jegl_interface_config* config,
            bool reboot)
        {
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
            _vk_jegl_interface = new egl(egl::VULKAN120);
#else
            glfwInitVulkanLoader(vkGetInstanceProcAddr);
            _vk_jegl_interface = new glfw(reboot ? glfw::HOLD : glfw::VULKAN120);
#endif
            _vk_jegl_interface->create_interface(config);

            _vk_jegl_context = ctx;

            _vk_current_target_framebuffer = nullptr;
            _vk_current_command_buffer = VK_NULL_HANDLE;
            _vk_current_binded_shader = nullptr;
            _vk_current_pipeline = nullptr;
            _vk_current_vertex = nullptr;
            _vk_current_shader_resource = nullptr;
            _vk_current_swapchain_image_content = nullptr;
            _vk_frame_recording = false;
            _vk_frame_has_swapchain_image = false;
            _vk_pending_present = false;
            _vk_pending_present_image_index = UINT32_MAX;
            _vk_presenting_swapchain_image_index = UINT32_MAX;

            _vk_msaa_config = config->m_msaa;
            if (_vk_msaa_config == 0)
                _vk_msaa_config = 1;

            _vk_vsync_config = config->m_fps == 0;

            _vk_swapchain = VK_NULL_HANDLE;

            // 获取所有支持的层
            uint32_t vk_layer_count;
            vkEnumerateInstanceLayerProperties(&vk_layer_count, nullptr);

            std::vector<VkLayerProperties> vk_available_layers((size_t)vk_layer_count);
            vkEnumerateInstanceLayerProperties(&vk_layer_count, vk_available_layers.data());

            // 获取所有支持的拓展
            uint32_t vk_extension_count;
            vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr);

            std::vector<VkExtensionProperties> vk_available_extensions((size_t)vk_extension_count);
            vkEnumerateInstanceExtensionProperties(
                nullptr, &vk_extension_count, vk_available_extensions.data());

            bool vk_validation_layer_supported = false;
#ifndef NDEBUG
            if (vk_available_layers.end() == std::find_if(
                vk_available_layers.begin(),
                vk_available_layers.end(),
                [](const VkLayerProperties& prop)
                {
                    return strcmp("VK_LAYER_KHRONOS_validation", prop.layerName) == 0;
                }))
                jeecs::debug::logwarn("'VK_LAYER_KHRONOS_validation' not supported, skip.");
            else
                vk_validation_layer_supported = true;
#endif

            VkApplicationInfo application_info = {};
            application_info.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application_info.pNext = nullptr;
            application_info.pApplicationName = config->m_title;
            application_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
            application_info.pEngineName = "JoyEngineECS";
#undef JE_VERSION_WRAP
#define JE_VERSION_WRAP(A, B, C) VK_MAKE_API_VERSION(0, A, B, C)
            application_info.engineVersion = JE_CORE_VERSION;
            application_info.apiVersion = VK_API_VERSION_1_2;

            VkInstanceCreateInfo instance_create_info = {};
            instance_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_create_info.pNext = nullptr;
            instance_create_info.flags = 0;
            instance_create_info.pApplicationInfo = &application_info;

            std::vector<const char*> required_layers = {};
            if (vk_validation_layer_supported)
                required_layers.push_back("VK_LAYER_KHRONOS_validation");

            instance_create_info.enabledLayerCount = (uint32_t)required_layers.size();
            instance_create_info.ppEnabledLayerNames = required_layers.data();

            std::vector<const char*> required_extensions;
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
            required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
            required_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#   else
#       error "Unsupported platform for EGL."
#   endif
#else
            unsigned int glfw_extension_count = 0;
            const char** glfw_extensions = nullptr;
            glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

            required_extensions.insert(
                required_extensions.end(), glfw_extensions, glfw_extensions + glfw_extension_count);
#endif
            if (vk_validation_layer_supported)
                required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            instance_create_info.enabledExtensionCount = (uint32_t)required_extensions.size();
            instance_create_info.ppEnabledExtensionNames = required_extensions.data();

            VkResult create_instance_result = vkCreateInstance(
                &instance_create_info, nullptr, &_vk_instance);

            if (create_instance_result != VK_SUCCESS)
            {
                jeecs::debug::logfatal(
                    "Failed to create vk120 instance (VkResult=%d).",
                    (int)create_instance_result);
                std::abort();
            }

            // 在此初始化vkAPI
#if JE4_VK_USE_DYNAMIC_VK_LIB
#   define VK_API_DECL(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(_vk_instance, #name));
#else
#   define VK_API_DECL(name) name = ::name;
#endif
            VK_API_LIST;
#undef VK_API_DECL

            for (const char* required_layer : required_layers)
            {
                if (vk_available_layers.end() == std::find_if(
                    vk_available_layers.begin(),
                    vk_available_layers.end(),
                    [required_layer](const VkLayerProperties& prop)
                    {
                        return strcmp(required_layer, prop.layerName) == 0;
                    }))
                {
                    jeecs::debug::logfatal("Required layer '%s', but current instance is not supported.", required_layer);
                }
            }

            // 创建Surface，并且绑定窗口句柄
#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   if JE4_CURRENT_PLATFORM == JE4_PLATFORM_ANDROID
            const auto* egl_context = static_cast<const jeecs::graphic::egl::egl_context*>(
                _vk_jegl_interface->interface_handle());

            VkAndroidSurfaceCreateInfoKHR android_surface_create_info{};
            android_surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            android_surface_create_info.pNext = nullptr;
            android_surface_create_info.flags = 0;
            android_surface_create_info.window = egl_context->m_window;

            if (VK_SUCCESS != vkCreateAndroidSurfaceKHR(
                _vk_instance,
                &android_surface_create_info,
                nullptr /* pAllocator */,
                &_vk_surface))
            {
                jeecs::debug::logfatal("Failed to create vk120 android surface.");
            }
#   else
#       error "Unsupported platform for EGL."
#   endif
#else
            VkResult surface_result = glfwCreateWindowSurface(
                _vk_instance, (GLFWwindow*)_vk_jegl_interface->interface_handle(), nullptr, &_vk_surface);
            if (VK_SUCCESS != surface_result)
            {
                const char* error_message = nullptr;
                auto eno = glfwGetError(&error_message);

                jeecs::debug::logfatal("Failed to create vk120 glfw surface: %s(%d).",
                    error_message, eno);
            }
#endif

            // 获取可接受的设备

            uint32_t enum_deveice_count = 0;
            vkEnumeratePhysicalDevices(_vk_instance, &enum_deveice_count, nullptr);

            std::vector<VkPhysicalDevice> all_physics_devices((size_t)enum_deveice_count);
            vkEnumeratePhysicalDevices(_vk_instance, &enum_deveice_count, all_physics_devices.data());

            // 设备应当满足的层和扩展
            std::vector<const char*> required_device_layers = {};
            if (vk_validation_layer_supported)
                required_device_layers.push_back("VK_LAYER_KHRONOS_validation");

            std::vector<const char*> required_device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            _vk_device = nullptr;
            int best_device_score = -1;
            physics_device_info best_device_info = {};

            for (auto& device : all_physics_devices)
            {
                auto info = check_physics_device_is_suatable(
                    device,
                    vk_validation_layer_supported,
                    required_device_layers,
                    required_device_extensions);

                if (!info)
                    continue;

                VkPhysicalDeviceProperties device_properties = {};
                vkGetPhysicalDeviceProperties(device, &device_properties);

                int device_score = 1;
                switch (device_properties.deviceType)
                {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    device_score = 4;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    device_score = 3;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    device_score = 2;
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    device_score = 0;
                    break;
                default:
                    device_score = 1;
                    break;
                }

                if (device_score > best_device_score)
                {
                    best_device_score = device_score;
                    _vk_device = device;
                    best_device_info = *info;
                }
            }

            if (_vk_device != nullptr)
            {
                _vk_device_queue_graphic_family_index =
                    best_device_info.queue_graphic_family_index;
                _vk_device_queue_present_family_index =
                    best_device_info.queue_present_family_index;
            }

            if (_vk_device == nullptr)
            {
                jeecs::debug::logfatal("Failed to get vk120 device.");
            }

#ifndef NDEBUG
            _vk_debug_manager = VK_NULL_HANDLE;

            if (vk_validation_layer_supported)
            {
                VkDebugUtilsMessengerCreateInfoEXT debug_layer_config = {};
                debug_layer_config.sType =
                    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debug_layer_config.messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debug_layer_config.messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debug_layer_config.pfnUserCallback = &debug_callback;

                assert(vkCreateDebugUtilsMessengerEXT != nullptr);
                if (VK_SUCCESS != vkCreateDebugUtilsMessengerEXT(
                    _vk_instance, &debug_layer_config, nullptr, &_vk_debug_manager))
                {
                    jeecs::debug::logfatal("Failed to set up debug layer callback.");
                }
            }
#endif

            // 非常好，到这一步已经创建完vk的实例，也拿到所需的物理设备。现在是时候
            // 创建vk所需的逻辑设备了
            std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

            // 图形簇和提交簇可能是同一个簇，所以这里用set去重
            // NOTE: Vulkan 不允许为同一个簇创建不同的队列
            std::set<uint32_t> unique_queue_family_indices = {
                _vk_device_queue_graphic_family_index,
                _vk_device_queue_present_family_index };

            float queue_priority = 1.0f;

            for (uint32_t queue_family_index : unique_queue_family_indices)
            {
                VkDeviceQueueCreateInfo queue_create_info = {};
                queue_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_info.pNext = nullptr;
                queue_create_info.flags = 0;
                queue_create_info.queueFamilyIndex = queue_family_index;
                queue_create_info.queueCount = 1;
                queue_create_info.pQueuePriorities = &queue_priority;

                queue_create_infos.push_back(queue_create_info);
            }

            VkPhysicalDeviceFeatures supported_features = {};
            vkGetPhysicalDeviceFeatures(_vk_device, &supported_features);

            VkPhysicalDeviceFeatures device_features = {};
            if (supported_features.independentBlend)
                device_features.independentBlend = VK_TRUE;
            else
            {
                jeecs::debug::logwarn(
                    "VK120: independentBlend is not supported by this device; "
                    "per-render-target blend state will be limited.");
            }

            VkDeviceCreateInfo device_create_info = {};
            device_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pNext = nullptr;
            device_create_info.flags = 0;
            device_create_info.queueCreateInfoCount = (uint32_t)queue_create_infos.size();
            device_create_info.pQueueCreateInfos = queue_create_infos.data();

            // 设备层从 Vulkan 1.0 起就不再生效，校验层只应通过 instance 层启用。
            device_create_info.enabledLayerCount = 0;
            device_create_info.ppEnabledLayerNames = nullptr;

            device_create_info.enabledExtensionCount = (uint32_t)required_device_extensions.size();
            device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
            device_create_info.pEnabledFeatures = &device_features;

            JE_VK_CHECK(vkCreateDevice(
                _vk_device, &device_create_info, nullptr, &_vk_logic_device));

            vkGetDeviceQueue(
                _vk_logic_device,
                _vk_device_queue_graphic_family_index,
                0,
                &_vk_logic_device_graphic_queue);
            vkGetDeviceQueue(
                _vk_logic_device,
                _vk_device_queue_present_family_index,
                0,
                &_vk_logic_device_present_queue);

            if (_vk_logic_device_graphic_queue == VK_NULL_HANDLE
                || _vk_logic_device_present_queue == VK_NULL_HANDLE)
            {
                jeecs::debug::logfatal("VK120: failed to get device queues.");
            }

            VkPhysicalDeviceProperties device_properties = {};
            vkGetPhysicalDeviceProperties(_vk_device, &device_properties);
            _vk_min_uniform_buffer_offset_alignment = std::max<VkDeviceSize>(
                1, device_properties.limits.minUniformBufferOffsetAlignment);

            _vk_memory_allocator = new memory_allocator(this);
            _vk_memory_allocator->m_buffer_image_granularity = std::max<VkDeviceSize>(
                1, device_properties.limits.bufferImageGranularity);

            _vk_queue_commit_fence = create_fence();

            // 创建描述符集的布局信息
            _vk_global_descriptor_set_layouts[descriptor_set_type::UNIFORM_VARIABLES] =
                create_descriptor_set_layout<
                VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                VkShaderStageFlagBits::VK_SHADER_STAGE_ALL, 1>();

            _vk_global_descriptor_set_layouts[descriptor_set_type::UNIFORM_BUFFER] =
                create_descriptor_set_layout<
                VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                descriptor_set_allocator::MAX_UBO_LAYOUT>();

            _vk_global_descriptor_set_layouts[descriptor_set_type::TEXTURE] =
                create_descriptor_set_layout<
                VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                descriptor_set_allocator::MAX_TEXTURE_LAYOUT>();

            _vk_global_descriptor_set_layouts[descriptor_set_type::SAMPLER] =
                create_descriptor_set_layout<
                VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER,
                VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                descriptor_set_allocator::MAX_SAMPLER_LAYOUT>();

            _vk_descriptor_set_allocator = new descriptor_set_allocator(this);

            // 为imgui创建描述符集池
            VkDescriptorPoolSize pool_sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

            VkDescriptorPoolCreateInfo imgui_descriptor_pool_info = {};
            imgui_descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            imgui_descriptor_pool_info.pNext = nullptr;
            imgui_descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            imgui_descriptor_pool_info.maxSets =
                (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize) * 1000);
            imgui_descriptor_pool_info.poolSizeCount =
                (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize));
            imgui_descriptor_pool_info.pPoolSizes = pool_sizes;

            JE_VK_CHECK(vkCreateDescriptorPool(
                _vk_logic_device,
                &imgui_descriptor_pool_info,
                nullptr,
                &_vk_dear_imgui_descriptor_pool));

            VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
            pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
            pipeline_cache_create_info.pNext = nullptr;
            pipeline_cache_create_info.flags = 0;
            pipeline_cache_create_info.initialDataSize = 0;
            pipeline_cache_create_info.pInitialData = nullptr;

            JE_VK_CHECK(vkCreatePipelineCache(
                _vk_logic_device,
                &pipeline_cache_create_info,
                nullptr,
                &_vk_pipeline_cache));

            create_frame_resources();

            recreate_swap_chain_for_current_surface();

            _vk_dear_imgui_sampler = create_sampler(
                jegl_shader::fliter_mode::LINEAR,
                jegl_shader::fliter_mode::LINEAR,
                jegl_shader::fliter_mode::LINEAR,
                jegl_shader::wrap_mode::CLAMP,
                jegl_shader::wrap_mode::CLAMP);

            imgui_init();
        }
        void pre_shutdown()
        {
            vkDeviceWaitIdle(_vk_logic_device);
        }
        void shutdown(bool reboot)
        {
            jegui_shutdown_vk120(reboot);
            _vk_imgui_initialized = false;

            destroy_swap_chain();
            destroy_frame_resources();

            for (auto& [key, pipeline] : _vk_pso_cache)
            {
                (void)key;
                vkDestroyPipeline(_vk_logic_device, pipeline, nullptr);
            }
            _vk_pso_cache.clear();

            if (_vk_pipeline_cache != VK_NULL_HANDLE)
            {
                vkDestroyPipelineCache(_vk_logic_device, _vk_pipeline_cache, nullptr);
                _vk_pipeline_cache = VK_NULL_HANDLE;
            }

            for (auto& [key, render_pass] : _vk_render_pass_cache)
            {
                (void)key;
                vkDestroyRenderPass(_vk_logic_device, render_pass, nullptr);
            }
            _vk_render_pass_cache.clear();

            destroy_fence(_vk_queue_commit_fence);
            _vk_queue_commit_fence = VK_NULL_HANDLE;

            delete _vk_descriptor_set_allocator;
            _vk_descriptor_set_allocator = nullptr;

            for (auto layout : _vk_global_descriptor_set_layouts)
            {
                if (layout != VK_NULL_HANDLE)
                    destroy_descriptor_set_layout(layout);
            }

            if (_vk_dear_imgui_sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(_vk_logic_device, _vk_dear_imgui_sampler, nullptr);
                _vk_dear_imgui_sampler = VK_NULL_HANDLE;
            }
            if (_vk_dear_imgui_descriptor_pool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(
                    _vk_logic_device, _vk_dear_imgui_descriptor_pool, nullptr);
                _vk_dear_imgui_descriptor_pool = VK_NULL_HANDLE;
            }

            delete _vk_memory_allocator;
            _vk_memory_allocator = nullptr;

            vkDestroyDevice(_vk_logic_device, nullptr);
#ifndef NDEBUG
            if (_vk_debug_manager != VK_NULL_HANDLE)
            {
                assert(vkDestroyDebugUtilsMessengerEXT != nullptr);
                vkDestroyDebugUtilsMessengerEXT(_vk_instance, _vk_debug_manager, nullptr);
                _vk_debug_manager = VK_NULL_HANDLE;
            }
#endif
            vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
            vkDestroyInstance(_vk_instance, nullptr);
        }
        /////////////////////////////////////////////////////
        /////////////////////////////////////////////////////
        uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
        {
            VkPhysicalDeviceMemoryProperties memory_properties;
            vkGetPhysicalDeviceMemoryProperties(_vk_device, &memory_properties);

            for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
            {
                if ((type_filter & (1 << i)) &&
                    (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    return i;
                }
            }
            jeecs::debug::logfatal("Failed to find suitable memory type.");
            abort();
        };

        memory_allocator::allocation allocate_vk_memory(
            const VkMemoryRequirements& requirement,
            VkMemoryPropertyFlags properties,
            VkDeviceSize extra_alignment = 0)
        {
            return _vk_memory_allocator->allocate(
                requirement, properties, extra_alignment);
        }

        void free_vk_memory(
            VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
        {
            memory_allocator::allocation allocation;
            allocation.m_memory = memory;
            allocation.m_offset = offset;
            allocation.m_size = size;
            _vk_memory_allocator->free(allocation);
        }

        void alloc_vk_device_buffer_memory(
            size_t buffer_size,
            VkBufferUsageFlagBits buffer_usage,
            VkMemoryPropertyFlags properties,
            VkBuffer* out_device_buffer,
            VkDeviceMemory* out_device_memory,
            VkDeviceSize* out_memory_offset = nullptr,
            VkDeviceSize* out_memory_size = nullptr,
            void** out_mapped = nullptr)
        {
            VkBufferCreateInfo buffer_create_info = {};
            buffer_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.pNext = nullptr;
            buffer_create_info.flags = 0;
            buffer_create_info.size = (VkDeviceSize)std::max<size_t>(buffer_size, 1);
            buffer_create_info.usage = buffer_usage;
            buffer_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;

            JE_VK_CHECK(vkCreateBuffer(
                _vk_logic_device,
                &buffer_create_info,
                nullptr,
                out_device_buffer));

            VkMemoryRequirements buffer_memory_requirements;
            vkGetBufferMemoryRequirements(
                _vk_logic_device,
                *out_device_buffer,
                &buffer_memory_requirements);

            auto allocation = allocate_vk_memory(
                buffer_memory_requirements,
                properties,
                _vk_memory_allocator->m_buffer_image_granularity);

            *out_device_memory = allocation.m_memory;
            if (out_memory_offset != nullptr)
                *out_memory_offset = allocation.m_offset;
            if (out_memory_size != nullptr)
                *out_memory_size = allocation.m_size;
            if (out_mapped != nullptr)
                *out_mapped = allocation.m_mapped;

            JE_VK_CHECK(vkBindBufferMemory(
                _vk_logic_device,
                *out_device_buffer,
                allocation.m_memory,
                allocation.m_offset));
        }
        /////////////////////////////////////////////////////
        VkSampler create_sampler(
            jegl_shader::fliter_mode mag,
            jegl_shader::fliter_mode min,
            jegl_shader::fliter_mode mip,
            jegl_shader::wrap_mode u,
            jegl_shader::wrap_mode v)
        {
            VkSamplerCreateInfo sampler_create_info = {};
            sampler_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_create_info.pNext = nullptr;
            sampler_create_info.flags = 0;

            // 线性或临近采样
            switch (mag)
            {
            case jegl_shader::fliter_mode::LINEAR:
                sampler_create_info.magFilter = VkFilter::VK_FILTER_LINEAR;
                break;
            case jegl_shader::fliter_mode::NEAREST:
                sampler_create_info.magFilter = VkFilter::VK_FILTER_NEAREST;
                break;
            }
            switch (min)
            {
            case jegl_shader::fliter_mode::LINEAR:
                sampler_create_info.minFilter = VkFilter::VK_FILTER_LINEAR;
                break;
            case jegl_shader::fliter_mode::NEAREST:
                sampler_create_info.minFilter = VkFilter::VK_FILTER_NEAREST;
                break;
            }
            switch (mip)
            {
            case jegl_shader::fliter_mode::LINEAR:
                sampler_create_info.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
                break;
            case jegl_shader::fliter_mode::NEAREST:
                sampler_create_info.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
                break;
            }

            // 边缘采样方式
            switch (u)
            {
            case jegl_shader::wrap_mode::CLAMP:
                sampler_create_info.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;
            case jegl_shader::wrap_mode::REPEAT:
                sampler_create_info.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            }
            switch (v)
            {
            case jegl_shader::wrap_mode::CLAMP:
                sampler_create_info.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                break;
            case jegl_shader::wrap_mode::REPEAT:
                sampler_create_info.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
                break;
            }
            // JoyEngine的纹理没有W方向，现在先随便塞个值
            sampler_create_info.addressModeW = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;

            // 边缘采样颜色， 由于引擎没有提供边缘采样颜色的选项，所以这里也是随便填一个
            sampler_create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK;

            // 乱七八糟的其他设置，统统忽略
            sampler_create_info.mipLodBias = 0.0f;

            // 各向异性滤波，此处也简单忽略。不启用
            sampler_create_info.anisotropyEnable = VK_FALSE;

            sampler_create_info.maxAnisotropy = 1.0f;
            sampler_create_info.compareEnable = VK_FALSE;
            sampler_create_info.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
            sampler_create_info.minLod = 0.0f;
            sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;

            VkSampler result;
            if (VK_SUCCESS != vkCreateSampler(
                _vk_logic_device,
                &sampler_create_info,
                nullptr,
                &result))
            {
                jeecs::debug::logfatal("Failed to create vk120 sampler.");
            }
            return result;
        }
        jevk12_shader_blob* create_shader_blob(jegl_shader* resource)
        {
            jevk12_shader_blob::blob_data* shader_blob = new jevk12_shader_blob::blob_data{};
            assert(resource != nullptr);

            shader_blob->m_context = this;
            shader_blob->m_shader_resource = resource;

            // 此处创建采样器
            shader_blob->m_samplers.resize(resource->m_sampler_count);
            for (size_t i = 0; i < resource->m_sampler_count; ++i)
            {
                const jegl_shader::sampler_method& method = resource->m_sampler_methods[i];

                shader_blob->m_samplers[i].m_vk_sampler =
                    create_sampler(
                        method.m_mag,
                        method.m_min,
                        method.m_mip,
                        method.m_uwrap,
                        method.m_vwrap);
                shader_blob->m_samplers[i].m_sampler_id = method.m_sampler_id;
            }

            shader_blob->m_texture_sampler_bindings.resize(resource->m_texture_count);
            for (size_t i = 0; i < resource->m_texture_count; ++i)
            {
                shader_blob->m_texture_sampler_bindings[i].m_pass =
                    resource->m_textures[i].m_pass;
                shader_blob->m_texture_sampler_bindings[i].m_sampler_id =
                    resource->m_textures[i].m_sampler_id;
            }

            VkShaderModuleCreateInfo vertex_shader_module_create_info = {};
            vertex_shader_module_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vertex_shader_module_create_info.pNext = nullptr;
            vertex_shader_module_create_info.flags = 0;
            vertex_shader_module_create_info.codeSize =
                resource->m_vertex_spirv_count * sizeof(jegl_shader::spir_v_code_t);
            vertex_shader_module_create_info.pCode = resource->m_vertex_spirv_codes;

            if (VK_SUCCESS != vkCreateShaderModule(
                _vk_logic_device,
                &vertex_shader_module_create_info,
                nullptr,
                &shader_blob->m_vertex_shader_module))
            {
                jeecs::debug::logfatal("Failed to create vk120 vertex shader module.");
            }

            VkShaderModuleCreateInfo fragment_shader_module_create_info = {};
            fragment_shader_module_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            fragment_shader_module_create_info.pNext = nullptr;
            fragment_shader_module_create_info.flags = 0;
            fragment_shader_module_create_info.codeSize =
                resource->m_fragment_spirv_count * sizeof(jegl_shader::spir_v_code_t);
            fragment_shader_module_create_info.pCode = resource->m_fragment_spirv_codes;

            if (VK_SUCCESS != vkCreateShaderModule(
                _vk_logic_device,
                &fragment_shader_module_create_info,
                nullptr,
                &shader_blob->m_fragment_shader_module))
            {
                jeecs::debug::logfatal("Failed to create vk120 fragment shader module.");
            }

            VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
            vertex_shader_stage_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertex_shader_stage_info.pNext = nullptr;
            vertex_shader_stage_info.flags = 0;
            vertex_shader_stage_info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
            vertex_shader_stage_info.module = shader_blob->m_vertex_shader_module;
            vertex_shader_stage_info.pName = "vertex_main";

            VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
            fragment_shader_stage_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_shader_stage_info.pNext = nullptr;
            fragment_shader_stage_info.flags = 0;
            fragment_shader_stage_info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_shader_stage_info.module = shader_blob->m_fragment_shader_module;
            fragment_shader_stage_info.pName = "fragment_main";

            shader_blob->m_shader_stage_infos[0] = vertex_shader_stage_info;
            shader_blob->m_shader_stage_infos[1] = fragment_shader_stage_info;

            // 预备管线所需的资源~

            shader_blob->m_vertex_input_attribute_descriptions.resize(
                resource->m_vertex_in_count);

            size_t vertex_point_data_size = 0;
            for (size_t i = 0; i < resource->m_vertex_in_count; ++i)
            {
                shader_blob->m_vertex_input_attribute_descriptions[i].binding = 0;
                shader_blob->m_vertex_input_attribute_descriptions[i].location = (uint32_t)i;
                shader_blob->m_vertex_input_attribute_descriptions[i].offset = (uint32_t)vertex_point_data_size;
                switch (resource->m_vertex_in[i])
                {
                case jegl_shader::uniform_type::INT:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32_SINT;
                    vertex_point_data_size += sizeof(int32_t);
                    break;
                case jegl_shader::uniform_type::INT2:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32_SINT;
                    vertex_point_data_size += sizeof(int32_t) * 2;
                    break;
                case jegl_shader::uniform_type::INT3:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32B32_SINT;
                    vertex_point_data_size += sizeof(int32_t) * 3;
                    break;
                case jegl_shader::uniform_type::INT4:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32B32A32_SINT;
                    vertex_point_data_size += sizeof(int32_t) * 4;
                    break;
                case jegl_shader::uniform_type::FLOAT:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32_SFLOAT;
                    vertex_point_data_size += sizeof(float);
                    break;
                case jegl_shader::uniform_type::FLOAT2:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format =
                        VkFormat::VK_FORMAT_R32G32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 2;
                    break;
                case jegl_shader::uniform_type::FLOAT3:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format =
                        VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 3;
                    break;
                case jegl_shader::uniform_type::FLOAT4:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format =
                        VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 4;
                    break;
                    // NOTE: 矩阵类型(FLOAT2X2/FLOAT3X3/FLOAT4X4)作为顶点属性未被支持，
                    // 与 DX11/Metal 后端保持一致；落入下面的 default 分支并报错。
                default:
                    jeecs::debug::logfatal(
                        "Unsupported vertex input type '%d' in shader '%s'.",
                        (int)resource->m_vertex_in[i],
                        resource->m_handle.m_path_may_null_if_builtin != nullptr
                        ? resource->m_handle.m_path_may_null_if_builtin
                        : "<builtin>");
                }
            }

            shader_blob->m_shader_output_color_attachment_count =
                resource->m_fragment_out_count;


            shader_blob->m_viewport = {};
            shader_blob->m_viewport.x = 0;
            shader_blob->m_viewport.y = 0;
            shader_blob->m_viewport.width = (float)_vk_surface_capabilities.currentExtent.width;
            shader_blob->m_viewport.height = (float)_vk_surface_capabilities.currentExtent.height;
            shader_blob->m_viewport.minDepth = 0;
            shader_blob->m_viewport.maxDepth = 1;

            shader_blob->m_scissor = {};
            shader_blob->m_scissor.offset = { 0, 0 };
            shader_blob->m_scissor.extent = _vk_surface_capabilities.currentExtent;

            shader_blob->m_viewport_state_create_info = {};
            shader_blob->m_viewport_state_create_info.sType =
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            shader_blob->m_viewport_state_create_info.pNext = nullptr;
            shader_blob->m_viewport_state_create_info.flags = 0;
            shader_blob->m_viewport_state_create_info.viewportCount = 1;
            shader_blob->m_viewport_state_create_info.pViewports = &shader_blob->m_viewport;
            shader_blob->m_viewport_state_create_info.scissorCount = 1;
            shader_blob->m_viewport_state_create_info.pScissors = &shader_blob->m_scissor;


            // 使用引擎在 shader 包装层计算的规范 std140 布局，避免后端重复推导
            // 导致偏移漂移（GL 后端在 Debug 下也会做同样的交叉校验）。
            shader_blob->m_uniform_size = resource->m_uniform_block_size;
            for (auto* uniforms = resource->m_custom_uniforms;
                uniforms != nullptr;
                uniforms = uniforms->m_next)
            {
                if (uniforms->m_uniform_type != jegl_shader::uniform_type::TEXTURE
                    && uniforms->m_offset != jeecs::graphic::INVALID_UNIFORM_LOCATION)
                {
                    shader_blob->m_uniform_locations[uniforms->m_name] =
                        uniforms->m_offset;
                }
            }

            // 动态配置~！
            // InputAssemblyState 和 Viewport 都应当允许动态配置
            shader_blob->m_dynamic_state_create_info = {};
            shader_blob->m_dynamic_state_create_info.sType =
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            shader_blob->m_dynamic_state_create_info.pNext = nullptr;
            shader_blob->m_dynamic_state_create_info.flags = 0;
            shader_blob->m_dynamic_state_create_info.dynamicStateCount =
                sizeof(jevk12_shader_blob::blob_data::m_dynamic_states) / sizeof(VkDynamicState);
            shader_blob->m_dynamic_state_create_info.pDynamicStates = jevk12_shader_blob::blob_data::m_dynamic_states;

            // 创建管道布局
            VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
            pipeline_layout_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.pNext = nullptr;
            pipeline_layout_create_info.flags = 0;
            pipeline_layout_create_info.setLayoutCount = (uint32_t)descriptor_set_type::COUNT;
            pipeline_layout_create_info.pSetLayouts = _vk_global_descriptor_set_layouts;
            pipeline_layout_create_info.pushConstantRangeCount = 0;
            pipeline_layout_create_info.pPushConstantRanges = nullptr;

            if (VK_SUCCESS != vkCreatePipelineLayout(
                _vk_logic_device,
                &pipeline_layout_create_info,
                nullptr,
                &shader_blob->m_pipeline_layout))
            {
                jeecs::debug::logfatal("Failed to create vk120 pipeline layout.");
            }

            auto* result = new jevk12_shader_blob(shader_blob);
            return result;
        }
        void destroy_shader_blob(jevk12_shader_blob* blob)
        {
            delete blob;
        }

        jevk12_shader* create_shader_with_blob(jevk12_shader_blob* blob)
        {
            assert(blob != nullptr);

            jevk12_shader* shader = new jevk12_shader(blob);

            return shader;
        }
        void destroy_shader(jevk12_shader* shader_may_null)
        {
            if (shader_may_null != nullptr)
                delete shader_may_null;
        }

        jevk12_vertex* create_vertex_instance(jegl_vertex* resource)
        {
            jevk12_vertex* vertex = new jevk12_vertex();

            vertex->m_topology = resource->m_type;

            const size_t vertex_data_size = resource->m_vertex_length;
            const size_t index_data_size =
                resource->m_index_capacity * sizeof(uint32_t);

            // Vulkan 不允许 size 为 0 的 buffer；空顶点资源使用 1 字节占位。
            const size_t vertex_buffer_size = std::max<size_t>(vertex_data_size, 1);
            const size_t index_buffer_size = std::max<size_t>(index_data_size, 1);

            // 获取所需分配的内存类型
            alloc_vk_device_buffer_memory(
                vertex_buffer_size,
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &vertex->m_vk_vertex_buffer,
                &vertex->m_vk_vertex_buffer_memory,
                &vertex->m_vertex_memory_offset,
                &vertex->m_vertex_memory_size,
                &vertex->m_mapped);

            if (vertex_data_size != 0 && resource->m_vertexs != nullptr)
            {
                if (vertex->m_mapped == nullptr)
                {
                    jeecs::debug::logfatal(
                        "VK120: vertex buffer memory is not host-visible.");
                }

                memcpy(
                    vertex->m_mapped,
                    resource->m_vertexs,
                    vertex_data_size);
            }

            // 创建索引缓冲区：按分配容量上传（m_index_count 可能已被
            // jegl_update_vertex_buffer 收缩为激活数量，绘制数量另存于
            // m_vertex_point_count）
            void* index_mapped = nullptr;
            alloc_vk_device_buffer_memory(
                index_buffer_size,
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &vertex->m_vk_index_buffer,
                &vertex->m_vk_index_buffer_memory,
                &vertex->m_index_memory_offset,
                &vertex->m_index_memory_size,
                &index_mapped);

            if (index_data_size != 0 && resource->m_indices != nullptr)
            {
                if (index_mapped == nullptr)
                {
                    jeecs::debug::logfatal(
                        "VK120: index buffer memory is not host-visible.");
                }

                memcpy(
                    index_mapped,
                    resource->m_indices,
                    index_data_size);
            }

            vertex->m_vertex_point_count = (uint32_t)resource->m_index_count;
            vertex->m_size = (VkDeviceSize)vertex_data_size;
            vertex->m_stride = (VkDeviceSize)resource->m_data_size_per_point;

            return vertex;
        }
        void destroy_vertex_instance(jevk12_vertex* vertex)
        {
            if (vertex == nullptr)
                return;

            if (vertex->m_vk_vertex_buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(_vk_logic_device, vertex->m_vk_vertex_buffer, nullptr);
            if (vertex->m_vk_vertex_buffer_memory != VK_NULL_HANDLE)
            {
                free_vk_memory(
                    vertex->m_vk_vertex_buffer_memory,
                    vertex->m_vertex_memory_offset,
                    vertex->m_vertex_memory_size);
            }

            if (vertex->m_vk_index_buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(_vk_logic_device, vertex->m_vk_index_buffer, nullptr);
            if (vertex->m_vk_index_buffer_memory != VK_NULL_HANDLE)
            {
                free_vk_memory(
                    vertex->m_vk_index_buffer_memory,
                    vertex->m_index_memory_offset,
                    vertex->m_index_memory_size);
            }

            delete vertex;
        }

        VkImageView create_image_view(
            VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
            VkImageViewType view_type,
            uint32_t layer_count)
        {
            VkImageViewCreateInfo image_view_create_info = {};
            image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.pNext = nullptr;
            image_view_create_info.flags = 0;
            image_view_create_info.image = image;
            image_view_create_info.viewType = view_type;
            image_view_create_info.format = format;
            image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.subresourceRange.aspectMask = aspect_flags;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = layer_count;

            VkImageView result = VK_NULL_HANDLE;
            JE_VK_CHECK(vkCreateImageView(
                _vk_logic_device, &image_view_create_info, nullptr, &result));
            return result;
        }

        static VkImageAspectFlags vk_aspect_for_format(VkFormat format)
        {
            switch (format)
            {
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }

        VkFormat vk_format_for_texture(jegl_texture_common::format format)
        {
            if (0 != (format & jegl_texture_common::format::DEPTH))
                return _vk_depth_format;

            const bool is_float16 =
                0 != (format & jegl_texture_common::format::FLOAT16);
            const bool is_srgb =
                0 != (format & jegl_texture_common::format::SRGB);
            if (0 != (format & jegl_texture_common::format::RGBA))
            {
                return is_float16
                    ? VK_FORMAT_R16G16B16A16_SFLOAT
                    : (is_srgb
                        ? VK_FORMAT_R8G8B8A8_SRGB
                        : VK_FORMAT_R8G8B8A8_UNORM);
            }
            return is_float16 ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R8_UNORM;
        }

        void create_image(
            jevk12_texture* texture,
            VkFormat format,
            VkImageUsageFlags usage,
            VkImageType image_type,
            uint32_t image_depth,
            uint32_t array_layers,
            VkSampleCountFlagBits samples,
            VkImageCreateFlags image_flags,
            VkImageViewType view_type)
        {
            assert(texture != nullptr);

            VkImageCreateInfo image_create_info = {};
            image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_create_info.pNext = nullptr;
            image_create_info.flags = image_flags;
            image_create_info.imageType = image_type;
            image_create_info.format = format;
            image_create_info.extent.width = texture->m_width;
            image_create_info.extent.height = texture->m_height;
            image_create_info.extent.depth = image_depth;
            image_create_info.mipLevels = 1;
            image_create_info.arrayLayers = array_layers;
            image_create_info.samples = samples;
            image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_create_info.usage = usage;
            image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_create_info.queueFamilyIndexCount = 0;
            image_create_info.pQueueFamilyIndices = nullptr;
            image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            JE_VK_CHECK(vkCreateImage(
                _vk_logic_device, &image_create_info, nullptr, &texture->m_vk_texture_image));

            VkMemoryRequirements image_memory_requirements;
            vkGetImageMemoryRequirements(
                _vk_logic_device, texture->m_vk_texture_image, &image_memory_requirements);

            auto image_allocation = allocate_vk_memory(
                image_memory_requirements,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                _vk_memory_allocator->m_buffer_image_granularity);
            texture->m_vk_texture_image_memory = image_allocation.m_memory;
            texture->m_vk_texture_memory_offset = image_allocation.m_offset;
            texture->m_vk_texture_memory_size = image_allocation.m_size;

            JE_VK_CHECK(vkBindImageMemory(
                _vk_logic_device,
                texture->m_vk_texture_image,
                texture->m_vk_texture_image_memory,
                texture->m_vk_texture_memory_offset));

            texture->m_vk_texture_format = format;
            texture->m_aspect = vk_aspect_for_format(format);
            texture->m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            texture->m_depth = image_depth;
            texture->m_layers = array_layers;
            texture->m_samples = (uint32_t)samples;
            texture->m_owns_image = true;

            texture->m_vk_texture_image_view = create_image_view(
                texture->m_vk_texture_image, format,
                (0 != (texture->m_aspect & VK_IMAGE_ASPECT_DEPTH_BIT))
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : texture->m_aspect,
                view_type, array_layers);
        }

        jevk12_texture* create_framebuf_texture_with_swapchain_image(
            VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
            uint32_t width, uint32_t height)
        {
            jevk12_texture* texture = new jevk12_texture{};

            texture->m_vk_texture_image = image;
            texture->m_vk_texture_image_memory = VK_NULL_HANDLE;
            texture->m_vk_texture_image_view = create_image_view(
                image, format, aspect_flags,
                VK_IMAGE_VIEW_TYPE_2D, 1);
            texture->m_vk_texture_format = format;
            texture->m_aspect = aspect_flags;
            texture->m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            texture->m_width = width;
            texture->m_height = height;
            texture->m_samples = 1;
            texture->m_owns_image = false;

            return texture;
        }

        jevk12_texture* create_framebuf_texture(
            jegl_texture_common::format format, size_t w, size_t h,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT)
        {
            assert(0 != (format & jegl_texture_common::format::FRAMEBUF));

            jevk12_texture* texture = new jevk12_texture{};
            texture->m_width = (uint32_t)w;
            texture->m_height = (uint32_t)h;

            if (0 != (format & jegl_texture_common::format::DEPTH))
            {
                create_image(
                    texture,
                    _vk_depth_format,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | (samples == VK_SAMPLE_COUNT_1_BIT
                        ? VK_IMAGE_USAGE_SAMPLED_BIT
                        : 0),
                    VK_IMAGE_TYPE_2D, 1, 1,
                    samples,
                    0,
                    VK_IMAGE_VIEW_TYPE_2D);
                transition_image_layout(
                    texture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
            else
            {
                create_image(
                    texture,
                    vk_format_for_texture(format),
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    | (samples == VK_SAMPLE_COUNT_1_BIT
                        ? VK_IMAGE_USAGE_SAMPLED_BIT
                        : 0),
                    VK_IMAGE_TYPE_2D, 1, 1,
                    samples,
                    0,
                    VK_IMAGE_VIEW_TYPE_2D);
                transition_image_layout(
                    texture,
                    samples == VK_SAMPLE_COUNT_1_BIT
                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }

            return texture;
        }

        void record_transition_barrier(
            VkCommandBuffer command_buffer,
            jevk12_texture* texture,
            VkImageLayout new_layout)
        {
            assert(texture != nullptr);
            assert(texture->m_vk_texture_image != VK_NULL_HANDLE);

            VkImageLayout old_layout = texture->m_current_layout;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = texture->m_vk_texture_image;
            barrier.subresourceRange.aspectMask = texture->m_aspect;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = texture->m_layers;

            auto stage_and_access = [](VkImageLayout layout,
                VkPipelineStageFlags& stage, VkAccessFlags& access)
                {
                    access = 0;
                    switch (layout)
                    {
                    case VK_IMAGE_LAYOUT_UNDEFINED:
                        stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                        stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                        access = VK_ACCESS_TRANSFER_WRITE_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                        stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                        access = VK_ACCESS_TRANSFER_READ_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                        stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                        access = VK_ACCESS_SHADER_READ_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                        stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                        stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                            | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                        access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                        break;
                    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                        stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                        break;
                    default:
                        stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                        break;
                    }
                };

            VkPipelineStageFlags source_stage;
            VkPipelineStageFlags destination_stage;
            stage_and_access(old_layout, source_stage, barrier.srcAccessMask);
            stage_and_access(new_layout, destination_stage, barrier.dstAccessMask);

            vkCmdPipelineBarrier(
                command_buffer,
                source_stage, destination_stage,
                0,
                0, nullptr,
                0, nullptr,
                1,
                &barrier);
        }

        void transition_image_layout(
            jevk12_texture* texture, VkImageLayout new_layout)
        {
            if (texture == nullptr || texture->m_vk_texture_image == VK_NULL_HANDLE)
                return;
            if (texture->m_current_layout == new_layout)
                return;

            VkCommandBuffer command_buffer = begin_temp_command_buffer_records();
            record_transition_barrier(command_buffer, texture, new_layout);
            end_temp_command_buffer_record(command_buffer);

            texture->m_current_layout = new_layout;
        }

        void record_image_layout_transition(
            jevk12_texture* texture, VkImageLayout new_layout)
        {
            if (texture == nullptr || texture->m_vk_texture_image == VK_NULL_HANDLE)
                return;
            if (texture->m_current_layout == new_layout)
                return;

            if (_vk_frame_recording && _vk_current_command_buffer != VK_NULL_HANDLE)
            {
                record_transition_barrier(
                    _vk_current_command_buffer, texture, new_layout);
                texture->m_current_layout = new_layout;
            }
            else
            {
                transition_image_layout(texture, new_layout);
            }
        }

        template<typename TexT>
        void trans_and_update_texture_pixels(
            jevk12_texture* texture, TexT* texture_raw_data,
            bool use_modified_rect = false)
        {
            assert(texture != nullptr);
            assert(texture_raw_data != nullptr);

            if (texture_raw_data->m_pixels == nullptr)
            {
                // 合法用法：仅在 GPU 侧创建纹理，没有 CPU 像素需要上传。
                if (texture->m_current_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    && texture->m_current_layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                {
                    transition_image_layout(
                        texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
                return;
            }

            constexpr bool is_3d = std::is_same_v<TexT, jegl_texture_3d>;
            constexpr bool is_array = std::is_same_v<TexT, jegl_texture_array>;
            constexpr bool is_cube = std::is_same_v<TexT, jegl_texture_cube>;

            uint32_t slice_count = 1;
            if constexpr (is_3d)
                slice_count = (uint32_t)texture_raw_data->m_depth;
            else if constexpr (is_array)
                slice_count = (uint32_t)texture_raw_data->m_layers;
            else if constexpr (is_cube)
                slice_count = 6;

            size_t pixel_size =
                (size_t)(texture_raw_data->m_format
                    & jegl_texture_common::format::COLOR_DEPTH_MASK);
            if (0 != (texture_raw_data->m_format
                & jegl_texture_common::format::FLOAT16))
            {
                // RGBA16F / R16F 每个通道占 2 字节。
                pixel_size *= 2;
            }

            size_t upload_min_x = 0;
            size_t upload_min_y = 0;
            size_t upload_max_x = texture_raw_data->m_width;
            size_t upload_max_y = texture_raw_data->m_height;
            bool full_upload = true;

            if constexpr (!is_3d && !is_array && !is_cube)
            {
                if (use_modified_rect
                    && texture_raw_data->m_width != 0
                    && texture_raw_data->m_height != 0)
                {
                    upload_min_x = texture_raw_data->m_modified_min_x;
                    upload_min_y = texture_raw_data->m_modified_min_y;
                    upload_max_x = texture_raw_data->m_modified_max_x;
                    upload_max_y = texture_raw_data->m_modified_max_y;

                    if (upload_min_x <= upload_max_x
                        && upload_min_y <= upload_max_y
                        && upload_max_x < texture_raw_data->m_width
                        && upload_max_y < texture_raw_data->m_height)
                    {
                        full_upload = false;
                    }
                }
            }

            size_t staging_size;
            size_t buffer_offset;
            uint32_t row_length;
            uint32_t image_height;
            VkOffset3D image_offset;
            VkExtent3D image_extent;

            if (full_upload)
            {
                staging_size = texture_raw_data->m_width * texture_raw_data->m_height
                    * slice_count * pixel_size;
                buffer_offset = 0;
                row_length = 0;
                image_height = 0;
                image_offset = { 0, 0, 0 };
                uint32_t extent_depth = 1;
                if constexpr (is_3d)
                    extent_depth = (uint32_t)texture_raw_data->m_depth;

                image_extent = {
                    (uint32_t)texture_raw_data->m_width,
                    (uint32_t)texture_raw_data->m_height,
                    extent_depth };
            }
            else
            {
                const size_t sub_width = upload_max_x - upload_min_x + 1;
                const size_t sub_height = upload_max_y - upload_min_y + 1;
                staging_size = sub_width * sub_height * pixel_size;
                buffer_offset = 0;
                row_length = (uint32_t)sub_width;
                image_height = (uint32_t)sub_height;
                image_offset = {
                    (int32_t)upload_min_x,
                    (int32_t)upload_min_y,
                    0 };
                image_extent = {
                    (uint32_t)sub_width,
                    (uint32_t)sub_height,
                    1 };
            }

            if (staging_size == 0)
                return;

            VkBuffer texture_data_buffer = VK_NULL_HANDLE;
            VkDeviceMemory texture_data_buffer_memory = VK_NULL_HANDLE;
            VkDeviceSize texture_data_memory_offset = 0;
            VkDeviceSize texture_data_memory_size = 0;
            void* data = nullptr;

            alloc_vk_device_buffer_memory(
                staging_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &texture_data_buffer,
                &texture_data_buffer_memory,
                &texture_data_memory_offset,
                &texture_data_memory_size,
                &data);

            if (data == nullptr)
            {
                jeecs::debug::logfatal(
                    "VK120: staging buffer memory is not host-visible.");
            }

            if (full_upload)
            {
                memcpy(data,
                    texture_raw_data->m_pixels,
                    staging_size);
            }
            else
            {
                const size_t source_row_size =
                    texture_raw_data->m_width * pixel_size;
                const size_t sub_row_size =
                    (upload_max_x - upload_min_x + 1) * pixel_size;
                for (size_t y = upload_min_y; y <= upload_max_y; ++y)
                {
                    memcpy(
                        (uint8_t*)data + (y - upload_min_y) * sub_row_size,
                        (uint8_t*)texture_raw_data->m_pixels + y * source_row_size
                            + upload_min_x * pixel_size,
                        sub_row_size);
                }
            }

            transition_image_layout(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkCommandBuffer command_buffer = begin_temp_command_buffer_records();
            {
                VkBufferImageCopy region = {};
                // bufferOffset 是相对于缓冲起点的字节偏移；局部上传时缓冲只含有效子矩形。
                region.bufferOffset = 0;
                region.bufferRowLength = row_length;
                region.bufferImageHeight = image_height;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = is_3d ? 1 : (uint32_t)slice_count;
                region.imageOffset = image_offset;
                region.imageExtent = image_extent;

                vkCmdCopyBufferToImage(
                    command_buffer,
                    texture_data_buffer,
                    texture->m_vk_texture_image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &region);
            }
            end_temp_command_buffer_record(command_buffer);
            transition_image_layout(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            vkDestroyBuffer(_vk_logic_device, texture_data_buffer, nullptr);
            free_vk_memory(
                texture_data_buffer_memory,
                texture_data_memory_offset,
                texture_data_memory_size);
        }

        VkSampleCountFlagBits clamp_sample_count(uint32_t samples)
        {
            uint32_t clamped = samples == 0 ? 1 : samples;
            uint32_t result = 1;
            while ((result << 1) <= clamped)
                result <<= 1;
            return (VkSampleCountFlagBits)result;
        }

        VkSampleCountFlagBits clamp_supported_sample_count(
            VkFormat format,
            VkImageUsageFlags usage,
            VkSampleCountFlagBits requested)
        {
            if (requested == VK_SAMPLE_COUNT_1_BIT)
                return requested;

            VkImageFormatProperties format_properties = {};
            if (vkGetPhysicalDeviceImageFormatProperties(
                _vk_device,
                format,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,
                usage,
                0,
                &format_properties) != VK_SUCCESS)
            {
                return VK_SAMPLE_COUNT_1_BIT;
            }

            while (requested != VK_SAMPLE_COUNT_1_BIT
                && 0 == (format_properties.sampleCounts & requested))
            {
                requested = (VkSampleCountFlagBits)((uint32_t)requested >> 1);
            }
            return requested;
        }

        template<typename TexT>
        jevk12_texture* create_texture_instance(TexT* resource)
        {
            constexpr bool is_3d = std::is_same_v<TexT, jegl_texture_3d>;
            constexpr bool is_array = std::is_same_v<TexT, jegl_texture_array>;
            constexpr bool is_cube = std::is_same_v<TexT, jegl_texture_cube>;
            constexpr bool is_msaa = std::is_same_v<TexT, jegl_texture_msaa>;

            if constexpr (is_msaa)
            {
                jevk12_texture* texture = new jevk12_texture{};
                texture->m_width = resource->m_width;
                texture->m_height = resource->m_height;

                const bool is_depth =
                    0 != (resource->m_format & jegl_texture_common::format::DEPTH);
                const VkFormat format = vk_format_for_texture(resource->m_format);
                const VkImageUsageFlags attachment_usage = is_depth
                    ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                    : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                const VkSampleCountFlagBits requested_samples =
                    clamp_sample_count((uint32_t)resource->m_samples);
                const VkSampleCountFlagBits supported_samples =
                    clamp_supported_sample_count(format, attachment_usage, requested_samples);

                if (supported_samples != requested_samples)
                {
                    jeecs::debug::logwarn(
                        "VK120: MSAA texture x%u is not supported for this format, using x%u.",
                        (uint32_t)requested_samples, (uint32_t)supported_samples);
                }
                texture->m_samples = (uint32_t)supported_samples;

                create_image(
                    texture,
                    format,
                    attachment_usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_TYPE_2D, 1, 1,
                    supported_samples,
                    0,
                    VK_IMAGE_VIEW_TYPE_2D);

                transition_image_layout(
                    texture,
                    is_depth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                return texture;
            }
            else if constexpr (!is_3d && !is_array && !is_cube)
            {
                if (0 != (resource->m_format & jegl_texture_common::format::FRAMEBUF))
                {
                    auto* texture = create_framebuf_texture(
                        resource->m_format, resource->m_width, resource->m_height);
                    return texture;
                }

                jevk12_texture* texture = new jevk12_texture{};
                texture->m_width = resource->m_width;
                texture->m_height = resource->m_height;

                const VkFormat image_format = vk_format_for_texture(resource->m_format);
                const bool is_depth =
                    0 != (resource->m_format & jegl_texture_common::format::DEPTH);

                create_image(
                    texture,
                    image_format,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_IMAGE_TYPE_2D, 1, 1,
                    VK_SAMPLE_COUNT_1_BIT,
                    0,
                    VK_IMAGE_VIEW_TYPE_2D);

                if (is_depth)
                {
                    // 深度纹理没有 CPU 像素数据，直接进入可采样布局。
                    transition_image_layout(
                        texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
                else
                {
                    trans_and_update_texture_pixels(texture, resource);
                }
                return texture;
            }
            else
            {
                jevk12_texture* texture = new jevk12_texture{};
                texture->m_width = resource->m_width;
                texture->m_height = resource->m_height;

                const VkFormat image_format = vk_format_for_texture(resource->m_format);
                const bool is_depth =
                    0 != (resource->m_format & jegl_texture_common::format::DEPTH);

                VkImageType vk_image_type = VK_IMAGE_TYPE_2D;
                uint32_t vk_image_depth = 1;
                uint32_t vk_array_layers = 1;
                VkImageCreateFlags vk_image_flags = 0;
                VkImageViewType vk_view_type = VK_IMAGE_VIEW_TYPE_2D;

                if constexpr (is_3d)
                {
                    vk_image_type = VK_IMAGE_TYPE_3D;
                    vk_image_depth = (uint32_t)resource->m_depth;
                    vk_view_type = VK_IMAGE_VIEW_TYPE_3D;
                }
                else if constexpr (is_array)
                {
                    vk_array_layers = (uint32_t)resource->m_layers;
                    vk_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                }
                else if constexpr (is_cube)
                {
                    vk_array_layers = 6;
                    vk_image_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                    vk_view_type = VK_IMAGE_VIEW_TYPE_CUBE;
                }

                create_image(
                    texture,
                    image_format,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    vk_image_type,
                    vk_image_depth,
                    vk_array_layers,
                    VK_SAMPLE_COUNT_1_BIT,
                    vk_image_flags,
                    vk_view_type);

                if (is_depth)
                {
                    transition_image_layout(
                        texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
                else
                {
                    trans_and_update_texture_pixels(texture, resource);
                }
                return texture;
            }
        }
        void destroy_texture_instance(jevk12_texture* texture)
        {
            if (texture == nullptr)
                return;

            if (_vk_imgui_initialized && texture->m_vk_texture_image_view != VK_NULL_HANDLE)
            {
                auto found = _vk_imgui_texture_cache.find(
                    texture->m_vk_texture_image_view);
                if (found != _vk_imgui_texture_cache.end())
                {
                    ImGui_ImplVulkan_RemoveTexture(found->second);
                    _vk_imgui_texture_cache.erase(found);
                }
            }

            if (texture->m_vk_texture_image_view != VK_NULL_HANDLE)
                vkDestroyImageView(_vk_logic_device, texture->m_vk_texture_image_view, nullptr);

            if (texture->m_owns_image
                && texture->m_vk_texture_image != VK_NULL_HANDLE)
            {
                vkDestroyImage(_vk_logic_device, texture->m_vk_texture_image, nullptr);
            }
            if (texture->m_vk_texture_image_memory != VK_NULL_HANDLE)
            {
                free_vk_memory(
                    texture->m_vk_texture_image_memory,
                    texture->m_vk_texture_memory_offset,
                    texture->m_vk_texture_memory_size);
            }

            delete texture;
        }

        jevk12_uniformbuf* create_uniform_buffer_with_size(uint32_t real_binding_place, size_t size)
        {
            jevk12_uniformbuf* uniformbuf = new jevk12_uniformbuf{};

            uniformbuf->m_real_binding_place = real_binding_place;
            uniformbuf->m_buffer_size = std::max<size_t>(size, 1);

            // 获取所需分配的内存类型
            alloc_vk_device_buffer_memory(
                uniformbuf->m_buffer_size,
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &uniformbuf->m_uniform_buffer,
                &uniformbuf->m_uniform_buffer_memory,
                &uniformbuf->m_memory_offset,
                &uniformbuf->m_memory_size,
                &uniformbuf->m_mapped);

            return uniformbuf;
        }
        jevk12_uniformbuf* create_uniform_buffer(jegl_uniform_buffer* resource)
        {
            return create_uniform_buffer_with_size(
                (uint32_t)resource->m_buffer_binding_place,
                resource->m_buffer_size);
        }
        void destroy_uniform_buffer(jevk12_uniformbuf* uniformbuf)
        {
            if (uniformbuf == nullptr)
                return;
            if (uniformbuf->m_uniform_buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(_vk_logic_device, uniformbuf->m_uniform_buffer, nullptr);
            if (uniformbuf->m_uniform_buffer_memory != VK_NULL_HANDLE)
            {
                free_vk_memory(
                    uniformbuf->m_uniform_buffer_memory,
                    uniformbuf->m_memory_offset,
                    uniformbuf->m_memory_size);
            }
            delete uniformbuf;
        }
        void update_uniform_buffer_with_range(jevk12_uniformbuf* ubuf, void* data, size_t offset, size_t size)
        {
            if (ubuf == nullptr || data == nullptr || size == 0)
                return;
            if (offset + size > ubuf->m_buffer_size)
            {
                jeecs::debug::logerr(
                    "VK120: uniform buffer update exceeds the allocated buffer size.");
                return;
            }

            if (ubuf->m_mapped == nullptr)
            {
                jeecs::debug::logerr(
                    "VK120: uniform buffer memory is not persistently mapped.");
                return;
            }

            memcpy(
                (uint8_t*)ubuf->m_mapped + offset,
                data,
                size);
        }
        void update_uniform_buffer(jegl_uniform_buffer* resource)
        {
            jevk12_uniformbuf* uniformbuf =
                static_cast<jevk12_uniformbuf*>(resource->m_handle.m_ptr);

            assert(resource->m_update_length != 0);
            update_uniform_buffer_with_range(
                uniformbuf,
                resource->m_buffer + resource->m_update_begin_offset,
                resource->m_update_begin_offset,
                resource->m_update_length);
        }

        /////////////////////////////////////////////////////
        void end_render_pass()
        {
            if (_vk_current_target_framebuffer == nullptr)
                return;

            vkCmdEndRenderPass(_vk_current_command_buffer);

            // render pass 会在结束时把附件迁移到 finalLayout，这里同步跟踪状态。
            for (auto* color : _vk_current_target_framebuffer->m_color_attachments)
            {
                if (color != nullptr)
                    color->m_current_layout = _vk_current_target_framebuffer->m_color_final_layout;
            }
            if (_vk_current_target_framebuffer->m_depth_attachment != nullptr)
            {
                _vk_current_target_framebuffer->m_depth_attachment->m_current_layout =
                    _vk_current_target_framebuffer->m_depth_final_layout;
            }
            if (_vk_current_target_framebuffer->m_resolve_attachment != nullptr)
            {
                _vk_current_target_framebuffer->m_resolve_attachment->m_current_layout =
                    _vk_current_target_framebuffer->m_resolve_final_layout;
            }

            _vk_current_target_framebuffer = nullptr;
        }

        void wait_current_frame_slot()
        {
            auto& frame = current_frame();
            if (frame.m_submitted)
            {
                JE_VK_CHECK(vkWaitForFences(
                    _vk_logic_device, 1, &frame.m_fence, VK_TRUE, UINT64_MAX));
                frame.m_submitted = false;
            }
        }

        void begin_frame_recording(bool has_swapchain_image)
        {
            if (_vk_frame_recording)
            {
                if (_vk_frame_has_swapchain_image != has_swapchain_image)
                {
                    jeecs::debug::logfatal(
                        "VK120: frame recording mode changed while the frame is being recorded.");
                }
                return;
            }

            wait_current_frame_slot();

            auto& frame = current_frame();
            assert(frame.m_command_buffer != VK_NULL_HANDLE);

            JE_VK_CHECK(vkResetCommandBuffer(frame.m_command_buffer, 0));

            frame.m_ubo_ring_offset = 0;
            JE_VK_CHECK(vkResetDescriptorPool(
                _vk_logic_device, frame.m_descriptor_pool, 0));
            _vk_descriptor_set_allocator->begin_frame();

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.pNext = nullptr;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            begin_info.pInheritanceInfo = nullptr;
            JE_VK_CHECK(vkBeginCommandBuffer(frame.m_command_buffer, &begin_info));

            frame.m_recording = true;
            frame.m_has_swapchain_image = has_swapchain_image;

            _vk_frame_recording = true;
            _vk_frame_has_swapchain_image = has_swapchain_image;
            _vk_current_command_buffer = frame.m_command_buffer;
            _vk_current_target_framebuffer = nullptr;
        }

        void submit_frame()
        {
            if (!_vk_frame_recording)
                return;

            end_render_pass();

            auto& frame = current_frame();
            JE_VK_CHECK(vkEndCommandBuffer(frame.m_command_buffer));

            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;

            VkSemaphore wait_semaphore = frame.m_image_available;
            VkSemaphore signal_semaphore = VK_NULL_HANDLE;
            VkPipelineStageFlags wait_stage =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if (frame.m_has_swapchain_image)
            {
                signal_semaphore =
                    _vk_image_render_finished_semaphores[
                        _vk_presenting_swapchain_image_index];

                submit_info.waitSemaphoreCount = 1;
                submit_info.pWaitSemaphores = &wait_semaphore;
                submit_info.pWaitDstStageMask = &wait_stage;
                submit_info.signalSemaphoreCount = 1;
                submit_info.pSignalSemaphores = &signal_semaphore;
            }

            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &frame.m_command_buffer;

            JE_VK_CHECK(vkResetFences(
                _vk_logic_device, 1, &frame.m_fence));
            JE_VK_CHECK(vkQueueSubmit(
                _vk_logic_device_graphic_queue, 1, &submit_info, frame.m_fence));

            frame.m_submitted = true;
            frame.m_recording = false;

            if (frame.m_has_swapchain_image)
            {
                _vk_pending_present = true;
                _vk_pending_present_image_index = _vk_presenting_swapchain_image_index;
            }

            _vk_frame_recording = false;
            _vk_frame_has_swapchain_image = false;
            _vk_current_command_buffer = VK_NULL_HANDLE;
            _vk_current_target_framebuffer = nullptr;

            _vk_current_frame_index =
                (_vk_current_frame_index + 1) % FRAMES_IN_FLIGHT;
        }

        void recreate_swapchain_and_reinit_imgui()
        {
            vkDeviceWaitIdle(_vk_logic_device);

            _vk_frame_recording = false;
            _vk_frame_has_swapchain_image = false;
            _vk_current_target_framebuffer = nullptr;
            _vk_current_command_buffer = VK_NULL_HANDLE;
            _vk_pending_present = false;
            _vk_pending_present_image_index = UINT32_MAX;

            const VkRenderPass previous_render_pass =
                _vk_imgui_initialized
                ? imgui_target_render_pass()
                : VK_NULL_HANDLE;
            const size_t previous_image_count = _vk_swapchain_images.size();

            recreate_swap_chain_for_current_surface();

            const VkRenderPass current_render_pass =
                imgui_target_render_pass();

            if (_vk_imgui_initialized
                && previous_render_pass == current_render_pass
                && previous_image_count == _vk_swapchain_images.size()
                && current_render_pass != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_SetMinImageCount(std::max<uint32_t>(
                    2, _vk_surface_capabilities.minImageCount));
            }
            else
            {
                if (_vk_imgui_initialized)
                {
                    jegui_shutdown_vk120(true);
                    _vk_imgui_initialized = false;
                    _vk_imgui_texture_cache.clear();
                }
                imgui_init();
            }
        }

        void present()
        {
            if (!_vk_pending_present
                || _vk_pending_present_image_index == UINT32_MAX)
            {
                return;
            }

            VkSemaphore render_finished =
                _vk_image_render_finished_semaphores[_vk_pending_present_image_index];

            VkSwapchainKHR swapchains[] = { _vk_swapchain };
            VkPresentInfoKHR present_info = {};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.pNext = nullptr;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &render_finished;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = swapchains;
            present_info.pImageIndices = &_vk_pending_present_image_index;
            present_info.pResults = nullptr;

            VkResult present_result =
                vkQueuePresentKHR(_vk_logic_device_present_queue, &present_info);

            _vk_pending_present = false;
            _vk_pending_present_image_index = UINT32_MAX;

            if (present_result == VK_ERROR_OUT_OF_DATE_KHR
                || present_result == VK_SUBOPTIMAL_KHR)
            {
                _vk_swapchain_out_of_date = true;
            }
            else if (present_result != VK_SUCCESS)
            {
                jeecs::debug::logwarn(
                    "Failed to present swap chain image (vkResult=%d).",
                    (int)present_result);
            }
        }

        bool acquire_swapchain_image()
        {
            auto& frame = current_frame();

            for (;;)
            {
                VkResult acquire_result = vkAcquireNextImageKHR(
                    _vk_logic_device,
                    _vk_swapchain,
                    UINT64_MAX,
                    frame.m_image_available,
                    VK_NULL_HANDLE,
                    &_vk_presenting_swapchain_image_index);

                if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    recreate_swapchain_and_reinit_imgui();
                    continue;
                }
                if (acquire_result == VK_SUCCESS
                    || acquire_result == VK_SUBOPTIMAL_KHR)
                {
                    break;
                }

                jeecs::debug::logfatal(
                    "Failed to acquire swap chain image (vkResult=%d).",
                    (int)acquire_result);
            }

            _vk_current_swapchain_image_content =
                _vk_swapchain_images[_vk_presenting_swapchain_image_index];
            return true;
        }

        void late_update()
        {
            if (!_vk_frame_recording)
                return;

            if (_vk_frame_has_swapchain_image)
            {
                cmd_begin_frame_buffer(nullptr, 0, 0, 0, 0);
                jegui_update_vk120(_vk_current_command_buffer);
                end_render_pass();

                // Surface has no sRGB format: the whole frame was rendered
                // into the linear offscreen target; encode it onto the real
                // swapchain image before presenting.
                if (_vk_use_srgb_encode_pass)
                {
                    _vk_encoding_srgb_screen = true;
                    jegui_commit_srgb_encode_pass(
                        _vk_srgb_encode_screen_color_resource);
                    _vk_encoding_srgb_screen = false;
                }
            }

            submit_frame();
        }

        /////////////////////////////////////////////////////
        void cmd_begin_frame_buffer(
            jevk12_framebuffer* framebuf, int32_t x, int32_t y, int32_t w, int32_t h)
        {
            end_render_pass();

            const bool wants_screen = (framebuf == nullptr);

            if (!_vk_frame_recording)
            {
                if (wants_screen)
                {
                    if (!_vk_frame_has_swapchain_image
                        || _vk_current_swapchain_image_content == nullptr)
                    {
                        jeecs::debug::logerr(
                            "VK120: begin_pass(nullptr) called without an acquired swapchain image.");
                        return;
                    }
                    begin_frame_recording(true);
                }
                else
                {
                    begin_frame_recording(false);
                }
            }

            jevk12_framebuffer* resolved_framebuffer = framebuf;
            if (wants_screen)
            {
                if (!_vk_frame_has_swapchain_image
                    || _vk_current_swapchain_image_content == nullptr)
                {
                    jeecs::debug::logerr(
                        "VK120: cannot render to the screen without an acquired swapchain image.");
                    return;
                }

                // 没有硬件 sRGB 写入控制时，把“绘制到屏幕”重定向到线性离屏
                // framebuffer；帧末再由编码 pass 绘制到真正的 swapchain。
                // 目标创建失败时 ensure 已记录错误，这里退回直接绘制。
                resolved_framebuffer =
                    (_vk_use_srgb_encode_pass && !_vk_encoding_srgb_screen
                        && _vk_srgb_encode_screen_framebuffer != nullptr)
                    ? _vk_srgb_encode_screen_framebuffer
                    : _vk_current_swapchain_image_content->m_framebuffer;
            }

            _vk_current_target_framebuffer = resolved_framebuffer;

            // 确保附件处于 render pass 期望的 initial layout。离屏/MSAA 附件可能在
            // 上一个 pass 结束后处于 SHADER_READ_ONLY，而 render pass 期望别的布局。
            for (auto* color : _vk_current_target_framebuffer->m_color_attachments)
            {
                if (color != nullptr
                    && color->m_current_layout
                    != _vk_current_target_framebuffer->m_color_initial_layout)
                {
                    record_image_layout_transition(
                        color, _vk_current_target_framebuffer->m_color_initial_layout);
                }
            }
            if (auto* depth = _vk_current_target_framebuffer->m_depth_attachment;
                depth != nullptr
                && depth->m_current_layout
                != _vk_current_target_framebuffer->m_depth_initial_layout)
            {
                record_image_layout_transition(
                    depth, _vk_current_target_framebuffer->m_depth_initial_layout);
            }
            if (auto* resolve = _vk_current_target_framebuffer->m_resolve_attachment;
                resolve != nullptr
                && resolve->m_current_layout
                != _vk_current_target_framebuffer->m_resolve_initial_layout)
            {
                record_image_layout_transition(
                    resolve, _vk_current_target_framebuffer->m_resolve_initial_layout);
            }

            _vk_descriptor_set_allocator->begin_pass();

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderPass = _vk_current_target_framebuffer->m_rendpass;
            render_pass_begin_info.framebuffer = _vk_current_target_framebuffer->m_framebuffer;
            render_pass_begin_info.renderArea.offset = { 0, 0 };
            render_pass_begin_info.renderArea.extent.width =
                (uint32_t)_vk_current_target_framebuffer->m_width;
            render_pass_begin_info.renderArea.extent.height =
                (uint32_t)_vk_current_target_framebuffer->m_height;
            render_pass_begin_info.clearValueCount = 0;
            render_pass_begin_info.pClearValues = nullptr;

            vkCmdBeginRenderPass(
                _vk_current_command_buffer,
                &render_pass_begin_info,
                VK_SUBPASS_CONTENTS_INLINE);

            // NOTE: 屏幕目标的 viewport 纵向坐标需要翻转，以兼容 opengl3 / dx11 的约定。
            if (w == 0)
                w = static_cast<int32_t>(_vk_current_target_framebuffer->m_width);
            if (h == 0)
                h = static_cast<int32_t>(_vk_current_target_framebuffer->m_height);

            VkViewport viewport = {};
            viewport.x = (float)x;
            viewport.width = (float)w;

            if (framebuf == nullptr)
            {
                viewport.y =
                    (float)_vk_current_target_framebuffer->m_height - (float)y;
                viewport.height = -(float)h;
            }
            else
            {
                viewport.y = (float)y;
                viewport.height = (float)h;
            }

            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(
                _vk_current_command_buffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = VkOffset2D{ (int32_t)0, (int32_t)0 };
            scissor.extent = VkExtent2D{
                (uint32_t)_vk_current_target_framebuffer->m_width,
                (uint32_t)_vk_current_target_framebuffer->m_height };
            vkCmdSetScissor(
                _vk_current_command_buffer, 0, 1, &scissor);
        }
        void cmd_clear_frame_buffer_color(size_t attachment_id, const float color[4])
        {
            assert(_vk_current_target_framebuffer != nullptr);

            if (attachment_id < _vk_current_target_framebuffer->m_color_attachments.size())
            {
                VkClearValue clear_color = {};
                clear_color.color = VkClearColorValue{ { color[0], color[1], color[2], color[3] } };

                VkClearAttachment clear_attachment = {};
                clear_attachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                clear_attachment.colorAttachment = (uint32_t)attachment_id;
                clear_attachment.clearValue = clear_color;

                VkClearRect clear_rect = {};
                clear_rect.baseArrayLayer = 0;
                clear_rect.layerCount = 1;
                clear_rect.rect.offset = { 0, 0 };
                clear_rect.rect.extent = {
                    (uint32_t)_vk_current_target_framebuffer->m_width,
                    (uint32_t)_vk_current_target_framebuffer->m_height };

                vkCmdClearAttachments(
                    _vk_current_command_buffer,
                    1,
                    &clear_attachment,
                    1,
                    &clear_rect);
            }
        }
        void cmd_clear_frame_buffer_depth(float clear_depth)
        {
            assert(_vk_current_target_framebuffer != nullptr);

            VkClearAttachment clear_attachment = {};
            clear_attachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            clear_attachment.colorAttachment = 0;
            clear_attachment.clearValue.depthStencil.depth = clear_depth;
            clear_attachment.clearValue.depthStencil.stencil = 0;

            VkClearRect clear_rect = {};
            clear_rect.baseArrayLayer = 0;
            clear_rect.layerCount = 1;
            clear_rect.rect.offset = { 0, 0 };
            clear_rect.rect.extent = {
                (uint32_t)_vk_current_target_framebuffer->m_width,
                (uint32_t)_vk_current_target_framebuffer->m_height };

            vkCmdClearAttachments(
                _vk_current_command_buffer,
                1,
                &clear_attachment,
                1,
                &clear_rect);
        }
        bool cmd_bind_shader_pipeline(jevk12_shader* shader_may_null)
        {
            assert(_vk_current_target_framebuffer != nullptr);

            if (_vk_current_binded_shader == shader_may_null)
                return shader_may_null != nullptr;

            _vk_current_binded_shader = shader_may_null;
            if (shader_may_null == nullptr)
                return false;

            for (auto& sampler : shader_may_null->m_blob_data->m_samplers)
            {
                for (auto& binding :
                    shader_may_null->m_blob_data->m_texture_sampler_bindings)
                {
                    if (binding.m_sampler_id == sampler.m_sampler_id)
                    {
                        _vk_descriptor_set_allocator->bind_sampler(
                            binding.m_pass, sampler.m_vk_sampler);
                    }
                }
            }
            return true;
        }

        static VkCompareOp vk_compare_op(jegl_shader::depth_test_method method)
        {
            switch (method)
            {
            case jegl_shader::depth_test_method::NEVER:
                return VK_COMPARE_OP_NEVER;
            case jegl_shader::depth_test_method::LESS:
                return VK_COMPARE_OP_LESS;
            case jegl_shader::depth_test_method::EQUAL:
                return VK_COMPARE_OP_EQUAL;
            case jegl_shader::depth_test_method::LESS_EQUAL:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case jegl_shader::depth_test_method::GREATER:
                return VK_COMPARE_OP_GREATER;
            case jegl_shader::depth_test_method::NOT_EQUAL:
                return VK_COMPARE_OP_NOT_EQUAL;
            case jegl_shader::depth_test_method::GREATER_EQUAL:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case jegl_shader::depth_test_method::ALWAYS:
                return VK_COMPARE_OP_ALWAYS;
            default:
                jeecs::debug::logerr("Invalid depth test method, using LESS.");
                return VK_COMPARE_OP_LESS;
            }
        }

        static VkBlendFactor vk_blend_factor(jegl_shader::blend_method method)
        {
            switch (method)
            {
            case jegl_shader::blend_method::ZERO:
                return VK_BLEND_FACTOR_ZERO;
            case jegl_shader::blend_method::ONE:
                return VK_BLEND_FACTOR_ONE;
            case jegl_shader::blend_method::SRC_COLOR:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case jegl_shader::blend_method::SRC_ALPHA:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case jegl_shader::blend_method::DST_COLOR:
                return VK_BLEND_FACTOR_DST_COLOR;
            case jegl_shader::blend_method::DST_ALPHA:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            default:
                jeecs::debug::logerr("Invalid blend method, using ONE.");
                return VK_BLEND_FACTOR_ONE;
            }
        }

        static VkBlendOp vk_blend_op(jegl_shader::blend_equation equation)
        {
            switch (equation)
            {
            case jegl_shader::blend_equation::ADD:
                return VK_BLEND_OP_ADD;
            case jegl_shader::blend_equation::SUBTRACT:
                return VK_BLEND_OP_SUBTRACT;
            case jegl_shader::blend_equation::REVERSE_SUBTRACT:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case jegl_shader::blend_equation::MIN:
                return VK_BLEND_OP_MIN;
            case jegl_shader::blend_equation::MAX:
                return VK_BLEND_OP_MAX;
            default:
                jeecs::debug::logerr("Invalid blend equation, using ADD.");
                return VK_BLEND_OP_ADD;
            }
        }

        static VkColorComponentFlags vk_color_write_mask(uint8_t mask)
        {
            VkColorComponentFlags result = 0;
            if (0 != (mask & 0x1))
                result |= VK_COLOR_COMPONENT_R_BIT;
            if (0 != (mask & 0x2))
                result |= VK_COLOR_COMPONENT_G_BIT;
            if (0 != (mask & 0x4))
                result |= VK_COLOR_COMPONENT_B_BIT;
            if (0 != (mask & 0x8))
                result |= VK_COLOR_COMPONENT_A_BIT;
            return result;
        }

        void destroy_pipelines_for_pipeline(jegl_pipeline* pipeline)
        {
            for (auto it = _vk_pso_cache.begin(); it != _vk_pso_cache.end();)
            {
                if (std::get<0>(it->first) == pipeline)
                {
                    vkDestroyPipeline(_vk_logic_device, it->second, nullptr);
                    it = _vk_pso_cache.erase(it);
                }
                else
                    ++it;
            }
        }

        void destroy_pipelines_for_shader(jegl_shader* shader)
        {
            for (auto it = _vk_pso_cache.begin(); it != _vk_pso_cache.end();)
            {
                jegl_pipeline* pipeline = std::get<0>(it->first);
                if (pipeline != nullptr && pipeline->m_shader == shader)
                {
                    vkDestroyPipeline(_vk_logic_device, it->second, nullptr);
                    it = _vk_pso_cache.erase(it);
                }
                else
                    ++it;
            }
        }

        VkPipeline get_or_create_pipeline(
            jegl_pipeline* pipeline, jevk12_vertex* vertex)
        {
            assert(pipeline != nullptr);
            assert(pipeline->m_shader != nullptr);
            assert(vertex != nullptr);
            assert(_vk_current_target_framebuffer != nullptr);
            assert(_vk_current_binded_shader != nullptr);

            auto* target_framebuffer = _vk_current_target_framebuffer;
            auto* shader_instance = static_cast<jevk12_shader*>(
                pipeline->m_shader->m_handle.m_ptr);
            assert(shader_instance != nullptr);
            auto* shader_blob = shader_instance->m_blob_data.get();
            assert(shader_blob != nullptr);

            const pso_key_t key{
                pipeline,
                target_framebuffer,
                (uint32_t)vertex->m_topology,
                (uint32_t)vertex->m_stride,
                (uint32_t)target_framebuffer->m_sample_count };

            auto found = _vk_pso_cache.find(key);
            if (found != _vk_pso_cache.end())
                return found->second;

            VkPrimitiveTopology vk_topology;
            switch (vertex->m_topology)
            {
            case jegl_vertex::type::LINESTRIP:
                vk_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
                break;
            case jegl_vertex::type::TRIANGLES:
                vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                break;
            case jegl_vertex::type::TRIANGLESTRIP:
                vk_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                break;
            default:
                jeecs::debug::logfatal("Unsupported vertex type.");
            }

            const auto& state = pipeline->m_state;

            VkVertexInputBindingDescription vertex_input_binding_description = {};
            vertex_input_binding_description.binding = 0;
            vertex_input_binding_description.stride = (uint32_t)vertex->m_stride;
            vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
            vertex_input_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state.pNext = nullptr;
            vertex_input_state.flags = 0;
            vertex_input_state.vertexBindingDescriptionCount = 1;
            vertex_input_state.pVertexBindingDescriptions =
                &vertex_input_binding_description;
            vertex_input_state.vertexAttributeDescriptionCount =
                (uint32_t)shader_blob->m_vertex_input_attribute_descriptions.size();
            vertex_input_state.pVertexAttributeDescriptions =
                shader_blob->m_vertex_input_attribute_descriptions.data();

            VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
            input_assembly_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_state.pNext = nullptr;
            input_assembly_state.flags = 0;
            input_assembly_state.topology = vk_topology;
            input_assembly_state.primitiveRestartEnable = VK_FALSE;

            VkPipelineRasterizationStateCreateInfo rasterization_state = {};
            rasterization_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_state.pNext = nullptr;
            rasterization_state.flags = 0;
            rasterization_state.depthClampEnable = VK_FALSE;
            rasterization_state.rasterizerDiscardEnable = VK_FALSE;
            rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;

            switch (state.m_cull)
            {
            case jegl_shader::cull_mode::FRONT:
                rasterization_state.cullMode = VK_CULL_MODE_FRONT_BIT;
                break;
            case jegl_shader::cull_mode::BACK:
                rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
                break;
            case jegl_shader::cull_mode::NONE:
            default:
                rasterization_state.cullMode = VK_CULL_MODE_NONE;
                break;
            }

            // 屏幕目标的 viewport 做了 Y 翻转；为了保持与 GL/DX11 相同的绕序语义，
            // 正面方向在屏幕/离屏目标之间取反。
            const bool state_ccw =
                state.m_front_face == jegl_front_face::COUNTER_CLOCKWISE;
            const bool is_screen = target_framebuffer->m_is_screen_framebuffer;
            rasterization_state.frontFace =
                (state_ccw == is_screen)
                ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                : VK_FRONT_FACE_CLOCKWISE;
            rasterization_state.depthBiasEnable = VK_FALSE;
            rasterization_state.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample_state = {};
            multisample_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_state.pNext = nullptr;
            multisample_state.flags = 0;
            multisample_state.rasterizationSamples = target_framebuffer->m_sample_count;
            multisample_state.sampleShadingEnable = VK_FALSE;
            multisample_state.minSampleShading = 1.0f;
            multisample_state.pSampleMask = nullptr;
            multisample_state.alphaToCoverageEnable = VK_FALSE;
            multisample_state.alphaToOneEnable = VK_FALSE;

            VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
            depth_stencil_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_state.pNext = nullptr;
            depth_stencil_state.flags = 0;
            depth_stencil_state.depthTestEnable =
                state.m_depth_test ? VK_TRUE : VK_FALSE;
            depth_stencil_state.depthWriteEnable =
                state.m_depth_write ? VK_TRUE : VK_FALSE;
            depth_stencil_state.depthCompareOp =
                vk_compare_op(state.m_depth_func);
            depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
            depth_stencil_state.stencilTestEnable = VK_FALSE;
            depth_stencil_state.front = {};
            depth_stencil_state.back = {};
            depth_stencil_state.minDepthBounds = 0.0f;
            depth_stencil_state.maxDepthBounds = 1.0f;

            const size_t framebuffer_color_attachment_count =
                target_framebuffer->m_color_attachments.size();
            const size_t enabled_color_attachment_count =
                std::min(
                    framebuffer_color_attachment_count,
                    shader_blob->m_shader_output_color_attachment_count);

            std::vector<VkPipelineColorBlendAttachmentState> attachment_states(
                framebuffer_color_attachment_count,
                VkPipelineColorBlendAttachmentState{});

            for (size_t i = 0; i < attachment_states.size(); ++i)
            {
                auto& attachment_state = attachment_states[i];
                if (i >= enabled_color_attachment_count)
                {
                    attachment_state.blendEnable = VK_FALSE;
                    attachment_state.colorWriteMask = 0;
                    continue;
                }

                attachment_state.blendEnable =
                    state.m_blend ? VK_TRUE : VK_FALSE;

                if (!state.m_blend)
                {
                    // 混合关闭时混合方程/因子不参与运算，DISABLED/未配置的
                    // shader 不应被当成非法枚举解析（与 dx11 行为一致）。
                    attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                    attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                    attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
                    attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
                    attachment_state.colorWriteMask =
                        vk_color_write_mask(state.m_color_write_mask);
                    continue;
                }

                attachment_state.srcColorBlendFactor =
                    vk_blend_factor(state.m_blend_src_color);
                attachment_state.dstColorBlendFactor =
                    vk_blend_factor(state.m_blend_dst_color);
                attachment_state.srcAlphaBlendFactor =
                    vk_blend_factor(state.m_blend_src_alpha);
                attachment_state.dstAlphaBlendFactor =
                    vk_blend_factor(state.m_blend_dst_alpha);
                attachment_state.colorBlendOp =
                    vk_blend_op(state.m_blend_equation);
                attachment_state.alphaBlendOp =
                    vk_blend_op(state.m_blend_equation);
                attachment_state.colorWriteMask =
                    vk_color_write_mask(state.m_color_write_mask);
            }

            VkPipelineColorBlendStateCreateInfo color_blend_state = {};
            color_blend_state.sType =
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state.pNext = nullptr;
            color_blend_state.flags = 0;
            color_blend_state.logicOpEnable = VK_FALSE;
            color_blend_state.logicOp = VK_LOGIC_OP_COPY;
            color_blend_state.attachmentCount =
                (uint32_t)attachment_states.size();
            color_blend_state.pAttachments =
                attachment_states.empty() ? nullptr : attachment_states.data();
            color_blend_state.blendConstants[0] = 0.0f;
            color_blend_state.blendConstants[1] = 0.0f;
            color_blend_state.blendConstants[2] = 0.0f;
            color_blend_state.blendConstants[3] = 0.0f;

            VkGraphicsPipelineCreateInfo pipeline_create_info = {};
            pipeline_create_info.sType =
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_create_info.pNext = nullptr;
            pipeline_create_info.flags = 0;
            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_blob->m_shader_stage_infos;
            pipeline_create_info.pVertexInputState = &vertex_input_state;
            pipeline_create_info.pInputAssemblyState = &input_assembly_state;
            pipeline_create_info.pViewportState =
                &shader_blob->m_viewport_state_create_info;
            pipeline_create_info.pRasterizationState = &rasterization_state;
            pipeline_create_info.pMultisampleState = &multisample_state;
            pipeline_create_info.pDepthStencilState = &depth_stencil_state;
            pipeline_create_info.pColorBlendState = &color_blend_state;
            pipeline_create_info.pDynamicState =
                &shader_blob->m_dynamic_state_create_info;
            pipeline_create_info.layout = shader_blob->m_pipeline_layout;
            pipeline_create_info.renderPass = target_framebuffer->m_rendpass;
            pipeline_create_info.subpass = 0;
            pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
            pipeline_create_info.basePipelineIndex = -1;

            VkPipeline new_pipeline = VK_NULL_HANDLE;
            JE_VK_CHECK(vkCreateGraphicsPipelines(
                _vk_logic_device,
                _vk_pipeline_cache,
                1,
                &pipeline_create_info,
                nullptr,
                &new_pipeline));

            _vk_pso_cache.emplace(key, new_pipeline);
            return new_pipeline;
        }

        //////////////////////////////////////////////////////////////////////////////////
        uint32_t prepare_draw_uniforms()
        {
            auto* shader = _vk_current_binded_shader;
            assert(shader != nullptr);

            if (shader->m_uniform_cpu_buffer_size == 0)
                return 0;

            auto& frame = current_frame();
            const VkDeviceSize offset = frame.allocate_uniform_block(
                shader->m_uniform_cpu_buffer_size,
                _vk_min_uniform_buffer_offset_alignment);

            memcpy(
                (uint8_t*)frame.m_ubo_ring_mapped + offset,
                shader->m_uniform_cpu_buffer,
                shader->m_uniform_cpu_buffer_size);

            return (uint32_t)offset;
        }

        void bind_draw_state(jevk12_vertex* vertex, uint32_t dynamic_uniform_offset)
        {
            auto* shader = _vk_current_binded_shader;
            assert(shader != nullptr);

            vkCmdBindPipeline(
                _vk_current_command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                get_or_create_pipeline(_vk_current_pipeline, vertex));

            auto* pipeline_layout = shader->m_blob_data->m_pipeline_layout;
            _vk_descriptor_set_allocator->bind_pipeline_layout(pipeline_layout);

            VkDescriptorSet uniform_vars =
                _vk_descriptor_set_allocator->uniform_vars_set(shader);
            _vk_descriptor_set_allocator->bind_all(
                _vk_current_command_buffer, uniform_vars, dynamic_uniform_offset);

            VkDeviceSize offsets = 0;
            vkCmdBindVertexBuffers(
                _vk_current_command_buffer, 0, 1, &vertex->m_vk_vertex_buffer, &offsets);
        }

        void cmd_draw_indexed_ex(
            jevk12_vertex* vertex,
            uint32_t index_count,
            uint32_t first_index,
            uint32_t instance_count)
        {
            assert(_vk_current_target_framebuffer != nullptr);
            assert(_vk_current_binded_shader != nullptr);
            assert(_vk_current_pipeline != nullptr);

            const uint32_t dynamic_uniform_offset = prepare_draw_uniforms();
            bind_draw_state(vertex, dynamic_uniform_offset);

            vkCmdBindIndexBuffer(
                _vk_current_command_buffer,
                vertex->m_vk_index_buffer,
                0,
                VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(
                _vk_current_command_buffer,
                index_count,
                instance_count,
                first_index,
                0,
                0);
        }

        void cmd_draw_ex(
            jevk12_vertex* vertex,
            uint32_t vertex_count,
            uint32_t first_vertex,
            uint32_t instance_count)
        {
            assert(_vk_current_target_framebuffer != nullptr);
            assert(_vk_current_binded_shader != nullptr);
            assert(_vk_current_pipeline != nullptr);

            const uint32_t dynamic_uniform_offset = prepare_draw_uniforms();
            bind_draw_state(vertex, dynamic_uniform_offset);

            vkCmdDraw(
                _vk_current_command_buffer,
                vertex_count,
                instance_count,
                first_vertex,
                0);
        }
        VkRenderPass imgui_target_render_pass() const
        {
            if (_vk_use_srgb_encode_pass
                && _vk_srgb_encode_screen_framebuffer != nullptr)
            {
                return _vk_srgb_encode_screen_framebuffer->m_rendpass;
            }
            return _vk_swapchain_images.empty()
                ? VK_NULL_HANDLE
                : _vk_swapchain_images.front()->m_framebuffer->m_rendpass;
        }

        void imgui_init()
        {
            assert(!_vk_swapchain_images.empty());

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = _vk_instance;
            init_info.PhysicalDevice = _vk_device;
            init_info.Device = _vk_logic_device;
            // ImGui 主视口由引擎的帧命令缓冲渲染；这里提供的队列用于多视口的 present。
            init_info.QueueFamily = _vk_device_queue_present_family_index;
            init_info.Queue = _vk_logic_device_present_queue;
            if (_vk_device_queue_graphic_family_index
                != _vk_device_queue_present_family_index)
            {
                jeecs::debug::logwarn(
                    "VK120: graphics and present queue families differ; "
                    "ImGui multi-viewport may be limited.");
            }
            init_info.PipelineCache = _vk_pipeline_cache;
            init_info.DescriptorPool = _vk_dear_imgui_descriptor_pool;
            init_info.Allocator = nullptr;
            init_info.MinImageCount = std::max<uint32_t>(
                2, _vk_surface_capabilities.minImageCount);
            init_info.ImageCount = (uint32_t)_vk_swapchain_images.size();
            init_info.MSAASamples = _vk_use_srgb_encode_pass
                ? VK_SAMPLE_COUNT_1_BIT
                : _vk_active_msaa_samples;
            init_info.CheckVkResultFn = nullptr;

            auto cmdbuf = begin_temp_command_buffer_records();

            jegui_init_vk120(
                _vk_jegl_context,
                [](jegl_context* ctx, jegl_texture_2d* res) -> uint64_t
                {
                    auto* this_context =
                        static_cast<jegl_vk120_context*>(ctx->m_graphic_impl_context);

                    auto* vk12_texture =
                        static_cast<jevk12_texture*>(res->m_handle.m_ptr);
                    if (vk12_texture == nullptr)
                        return 0;

                    auto found = this_context->_vk_imgui_texture_cache.find(
                        vk12_texture->m_vk_texture_image_view);
                    if (found != this_context->_vk_imgui_texture_cache.end())
                        return (uint64_t)found->second;

                    VkDescriptorSet desc_set = ImGui_ImplVulkan_AddTexture(
                        this_context->_vk_dear_imgui_sampler,
                        vk12_texture->m_vk_texture_image_view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                    this_context->_vk_imgui_texture_cache.emplace(
                        vk12_texture->m_vk_texture_image_view, desc_set);
                    return (uint64_t)desc_set;
                },
                [](jegl_context*, jegl_shader*) {},
                _vk_jegl_interface->interface_handle(),
                &init_info,
                imgui_target_render_pass(),
                cmdbuf,
#if JE4_VK_USE_DYNAMIC_VK_LIB
                [](const char* funcname, void* userdata)
                {
                    jegl_vk120_context* ctx = static_cast<jegl_vk120_context*>(userdata);
                    return ctx->vkGetInstanceProcAddr(ctx->_vk_instance, funcname);
                },
#else
                nullptr,
#endif
                this);

            end_temp_command_buffer_record(cmdbuf);
            _vk_imgui_initialized = true;
        }
    };

    jevk12_shader_blob::blob_data::~blob_data()
    {
        if (m_context == nullptr)
            return;

        if (m_shader_resource != nullptr)
            m_context->destroy_pipelines_for_shader(m_shader_resource);

        if (m_pipeline_layout != VK_NULL_HANDLE)
        {
            m_context->vkDestroyPipelineLayout(
                m_context->_vk_logic_device, m_pipeline_layout, nullptr);
        }
        if (m_vertex_shader_module != VK_NULL_HANDLE)
        {
            m_context->vkDestroyShaderModule(
                m_context->_vk_logic_device, m_vertex_shader_module, nullptr);
        }
        if (m_fragment_shader_module != VK_NULL_HANDLE)
        {
            m_context->vkDestroyShaderModule(
                m_context->_vk_logic_device, m_fragment_shader_module, nullptr);
        }

        for (auto& sampler : m_samplers)
        {
            if (sampler.m_vk_sampler != VK_NULL_HANDLE)
            {
                m_context->vkDestroySampler(
                    m_context->_vk_logic_device, sampler.m_vk_sampler, nullptr);
            }
        }
    }
    jevk12_shader::~jevk12_shader()
    {
        free(m_uniform_cpu_buffer);
    }
    je_GraphicImplContext
        startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan120) start!");

        jegl_vk120_context* context = new jegl_vk120_context(gthread, config, reboot);
        return context;
    }
    void pre_shutdown(jegl_context*, je_GraphicImplContext ctx, bool reboot)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        context->pre_shutdown();
    }
    void shutdown(jegl_context*, je_GraphicImplContext ctx, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan120) shutdown!");

        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        context->shutdown(reboot);
        context->_vk_jegl_interface->shutdown(reboot);
        delete context->_vk_jegl_interface;

        delete context;
    }

    jegl_update_action pre_update(je_GraphicImplContext ctx)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        context->_vk_current_binded_shader = nullptr;
        context->_vk_current_pipeline = nullptr;
        context->_vk_current_vertex = nullptr;
        context->_vk_current_shader_resource = nullptr;

        // 复用当前帧槽之前，必须先等待它上一次提交完成。
        context->wait_current_frame_slot();

        switch (context->_vk_jegl_interface->update())
        {
        case basic_interface::update_result::PAUSE:
            // 暂停帧不 acquire、不提交、不复用信号量；上一帧的 pending present
            // 留到恢复后的第一次 NORMAL 帧再处理。
            context->_vk_frame_recording = false;
            context->_vk_frame_has_swapchain_image = false;
            return jegl_update_action::JEGL_UPDATE_SKIP;
        case basic_interface::update_result::RESIZE:
            context->present();
            context->recreate_swapchain_and_reinit_imgui();
            break;
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_update_action::JEGL_UPDATE_STOP;
            [[fallthrough]];
        case basic_interface::update_result::NORMAL:
            context->present();
            if (context->_vk_swapchain_out_of_date)
            {
                context->_vk_swapchain_out_of_date = false;
                context->recreate_swapchain_and_reinit_imgui();
            }
            break;
        default:
            abort();
        }

        context->acquire_swapchain_image();
        context->begin_frame_recording(true);
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }
    jegl_update_action commit_update(
        je_GraphicImplContext ctx, jegl_update_action)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        context->late_update();

        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    jegl_resource_blob shader_create_resource_blob(
        je_GraphicImplContext ctx, jegl_shader* shader)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        return context->create_shader_blob(shader);
    }
    void shader_close_resource_blob(
        je_GraphicImplContext ctx, jegl_resource_blob blob)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        context->destroy_shader_blob(static_cast<jevk12_shader_blob*>(blob));
    }
    template<typename TexT>
    jegl_resource_blob texture_create_resource_blob(
        je_GraphicImplContext, TexT*)
    {
        return nullptr;
    }
    void texture_close_resource_blob(
        je_GraphicImplContext, jegl_resource_blob)
    {
    }
    jegl_resource_blob vertex_create_resource_blob(
        je_GraphicImplContext, jegl_vertex*)
    {
        return nullptr;
    }
    void vertex_close_resource_blob(
        je_GraphicImplContext, jegl_resource_blob)
    {
    }

    void init_shader(je_GraphicImplContext ctx, jegl_resource_blob blob, jegl_shader* shader)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        auto* shader_blob = static_cast<jevk12_shader_blob*>(blob);
        if (shader_blob != nullptr)
        {
            shader->m_handle.m_ptr = context->create_shader_with_blob(shader_blob);

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
            builtin_uniforms.m_builtin_uniform_shadow_ellipse =
                shader_blob->get_built_in_location("JE_SHADOW_ELLIPSE");

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
    }
    template<typename TexT>
    void init_texture(je_GraphicImplContext ctx, jegl_resource_blob, TexT* texture)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        texture->m_handle.m_ptr = context->create_texture_instance(texture);
    }
    void init_vertex(je_GraphicImplContext ctx, jegl_resource_blob, jegl_vertex* vertex)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        vertex->m_handle.m_ptr = context->create_vertex_instance(vertex);
    }
    void init_framebuffer(je_GraphicImplContext ctx, jegl_frame_buffer* fbuffer)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        std::vector<jevk12_texture*> color_attachments;
        jevk12_texture* depth_attachment = nullptr;

        jeecs::basic::resource<jeecs::graphic::texture2d>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture2d> *>(
                fbuffer->m_output_attachments);

        for (size_t i = 0; i < fbuffer->m_attachment_count; ++i)
        {
            auto& attachment = attachments[i];
            (void)_jegl_prepare_texture_2d_resource(attachment->resource());

            auto* attach_texture_instance =
                static_cast<jevk12_texture*>(attachment->resource()->m_handle.m_ptr);
            if (0 != (attachment->resource()->m_format & jegl_texture_common::format::DEPTH))
                depth_attachment = attach_texture_instance;
            else
                color_attachments.push_back(attach_texture_instance);
        }

        auto* frame_buffer_instance = context->create_frame_buffer(
            fbuffer->m_width,
            fbuffer->m_height,
            color_attachments,
            nullptr,
            depth_attachment,
            VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            false);

        fbuffer->m_handle.m_ptr = frame_buffer_instance;
    }
    void init_ubuffer(je_GraphicImplContext ctx, jegl_uniform_buffer* ubuffer)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        ubuffer->m_handle.m_ptr = context->create_uniform_buffer(ubuffer);
    }

    void update_shader(je_GraphicImplContext ctx, jegl_shader* shader)
    {
    }
    template<typename TexT>
    void update_texture(je_GraphicImplContext ctx, TexT* texture)
    {
        if constexpr (std::is_same_v<TexT, jegl_texture_msaa>)
        {
            (void)ctx;
            (void)texture;
            // NOTE: MSAA 纹理只存在于 GPU 侧，不支持 CPU 侧像素更新
            jeecs::debug::logerr("MSAA texture update is not supported.");
        }
        else
        {
            jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

            // Modified, free current resource id, reload one.
            context->trans_and_update_texture_pixels(
                static_cast<jevk12_texture*>(texture->m_handle.m_ptr),
                texture,
                true);
        }
    }
    void update_vertex(je_GraphicImplContext ctx, jegl_vertex* vertex)
    {
        (void)ctx;
        jevk12_vertex* vertex_instance =
            static_cast<jevk12_vertex*>(vertex->m_handle.m_ptr);

        if (vertex->m_vertex_length != 0 && vertex->m_vertexs != nullptr)
        {
            if (vertex->m_vertex_length > vertex_instance->m_size)
            {
                jeecs::debug::logerr(
                    "VK120: vertex update exceeds the allocated buffer size.");
                return;
            }
            if (vertex_instance->m_mapped == nullptr)
            {
                jeecs::debug::logerr(
                    "VK120: vertex buffer memory is not persistently mapped.");
                return;
            }

            memcpy(
                vertex_instance->m_mapped,
                vertex->m_vertexs,
                vertex->m_vertex_length);
        }

        vertex_instance->m_vertex_point_count =
            (uint32_t)vertex->m_index_count;
    }
    void update_framebuffer(je_GraphicImplContext ctx, jegl_frame_buffer* fbuffer)
    {
    }
    void update_ubuffer(je_GraphicImplContext ctx, jegl_uniform_buffer* ubuffer)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->update_uniform_buffer(ubuffer);
    }

    void close_shader(je_GraphicImplContext ctx, jegl_shader* shader)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->destroy_shader(static_cast<jevk12_shader*>(shader->m_handle.m_ptr));
    }
    template<typename TexT>
    void close_texture(je_GraphicImplContext ctx, TexT* texture)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->destroy_texture_instance(static_cast<jevk12_texture*>(texture->m_handle.m_ptr));
    }
    void close_vertex(je_GraphicImplContext ctx, jegl_vertex* vertex)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->destroy_vertex_instance(static_cast<jevk12_vertex*>(vertex->m_handle.m_ptr));
    }
    void close_framebuffer(je_GraphicImplContext ctx, jegl_frame_buffer* fbuffer)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->destroy_frame_buffer(static_cast<jevk12_framebuffer*>(fbuffer->m_handle.m_ptr));
    }
    void close_ubuffer(je_GraphicImplContext ctx, jegl_uniform_buffer* ubuffer)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        context->destroy_uniform_buffer(static_cast<jevk12_uniformbuf*>(ubuffer->m_handle.m_ptr));
    }

    void bind_uniform_buffer(je_GraphicImplContext ctx, jegl_uniform_buffer* uniformbuf)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);
        jevk12_uniformbuf* uniformbuf_instance =
            static_cast<jevk12_uniformbuf*>(uniformbuf->m_handle.m_ptr);

        context->_vk_descriptor_set_allocator->bind_uniform_buffer(
            uniformbuf_instance->m_real_binding_place, uniformbuf_instance->m_uniform_buffer);
    }

    template<typename TexT>
    void bind_texture(je_GraphicImplContext ctx, TexT* texture, size_t pass)
    {
        jegl_vk120_context* context = static_cast<jegl_vk120_context*>(ctx);

        auto* texture_instance = static_cast<jevk12_texture*>(texture->m_handle.m_ptr);
        assert(texture_instance != nullptr);

        if (texture_instance->m_current_layout
            != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            if (texture_instance->m_samples > 1)
            {
                jeecs::debug::logerr(
                    "VK120: cannot bind a multisampled texture as a sampled image.");
                return;
            }
            context->record_image_layout_transition(
                texture_instance, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        context->_vk_descriptor_set_allocator->bind_texture(
            (uint32_t)pass,
            texture_instance->m_vk_texture_image_view);
    }

    /////////////////////////////////////////////////////////////////////////
// Render API (v2)
/////////////////////////////////////////////////////////////////////////

    struct jegl_vk120_render_sampler
    {
        VkSampler m_sampler;
    };

    void render_pipeline_init(je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        assert(pipeline != nullptr);

        // shader 热重载或图形线程重启后，旧的 PSO 可能引用了失效的模块/布局。
        context->destroy_pipelines_for_pipeline(pipeline);
    }

    void render_pipeline_close(je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        if (pipeline != nullptr)
            context->destroy_pipelines_for_pipeline(pipeline);
    }

    void render_pipeline_bind(
        je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        auto* shader_instance = static_cast<jevk12_shader*>(
            pipeline->m_shader->m_handle.m_ptr);

        assert(shader_instance != nullptr);

        context->_vk_current_pipeline = pipeline;
        context->_vk_current_shader_resource = pipeline->m_shader;
        context->cmd_bind_shader_pipeline(shader_instance);
    }

    void render_sampler_init(je_GraphicImplContext ctx, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        auto* native = new jegl_vk120_render_sampler();

        auto cvt_filter = [](jegl_filter filter) -> jegl_shader::fliter_mode
            {
                return filter == jegl_filter::LINEAR
                    ? jegl_shader::fliter_mode::LINEAR
                    : jegl_shader::fliter_mode::NEAREST;
            };
        auto cvt_mip = [](jegl_mip_filter filter) -> jegl_shader::fliter_mode
            {
                return filter == jegl_mip_filter::LINEAR
                    ? jegl_shader::fliter_mode::LINEAR
                    : jegl_shader::fliter_mode::NEAREST;
            };
        auto cvt_wrap = [](jegl_wrap_mode wrap) -> jegl_shader::wrap_mode
            {
                switch (wrap)
                {
                case jegl_wrap_mode::CLAMP_TO_EDGE:
                    return jegl_shader::wrap_mode::CLAMP;
                case jegl_wrap_mode::REPEAT:
                case jegl_wrap_mode::MIRRORED_REPEAT:
                    return jegl_shader::wrap_mode::REPEAT;
                default:
                    assert(false);
                    return jegl_shader::wrap_mode::CLAMP;
                }
            };

        native->m_sampler = context->create_sampler(
            cvt_filter(sampler->m_desc.m_mag_filter),
            cvt_filter(sampler->m_desc.m_min_filter),
            cvt_mip(sampler->m_desc.m_mip_filter),
            cvt_wrap(sampler->m_desc.m_u_wrap),
            cvt_wrap(sampler->m_desc.m_v_wrap));

        sampler->m_backend_ptr = native;
    }

    void render_sampler_close(je_GraphicImplContext ctx, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        auto* native = static_cast<jegl_vk120_render_sampler*>(
            sampler->m_backend_ptr);

        if (native != nullptr)
        {
            context->vkDestroySampler(
                context->_vk_logic_device, native->m_sampler, nullptr);
            delete native;
            sampler->m_backend_ptr = nullptr;
        }
    }

    void render_sampler_bind(
        je_GraphicImplContext ctx, uint32_t sampler_id, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        auto* native = static_cast<jegl_vk120_render_sampler*>(
            sampler->m_backend_ptr);

        assert(native != nullptr);

        auto* shader = context->_vk_current_binded_shader;
        if (shader == nullptr)
        {
            jeecs::debug::logerr(
                "VK120: sampler_bind called before a pipeline was bound.");
            return;
        }

        bool bound = false;
        for (auto& binding : shader->m_blob_data->m_texture_sampler_bindings)
        {
            if (binding.m_sampler_id == sampler_id)
            {
                context->_vk_descriptor_set_allocator->bind_sampler(
                    binding.m_pass, native->m_sampler);
                bound = true;
            }
        }

        if (!bound)
        {
            jeecs::debug::logwarn(
                "VK120: sampler id %u is not declared by the current shader.",
                sampler_id);
        }
    }

    void render_begin_pass(
        je_GraphicImplContext ctx,
        /* OPTIONAL */ jegl_frame_buffer* target,
        const jegl_viewport* viewport)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);

        context->_vk_current_binded_shader = nullptr;
        context->_vk_current_pipeline = nullptr;
        context->_vk_current_shader_resource = nullptr;
        context->_vk_current_vertex = nullptr;

        auto* target_framebuffer = target != nullptr
            ? static_cast<jevk12_framebuffer*>(target->m_handle.m_ptr)
            : nullptr;

        context->cmd_begin_frame_buffer(
            target_framebuffer,
            viewport->x,
            viewport->y,
            (int32_t)viewport->width,
            (int32_t)viewport->height);
    }

    void render_clear_color(
        je_GraphicImplContext ctx, uint32_t attachment, const float rgba[4])
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        context->cmd_clear_frame_buffer_color(attachment, rgba);
    }

    void render_clear_depth(je_GraphicImplContext ctx, float depth)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);

        if (context->_vk_current_target_framebuffer == nullptr)
            return;
        if (context->_vk_current_target_framebuffer->m_depth_attachment == nullptr)
            return;

        context->cmd_clear_frame_buffer_depth(depth);
    }

    void render_end_pass(je_GraphicImplContext ctx)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        context->end_render_pass();
    }

    void render_bind_vertex(je_GraphicImplContext ctx, jegl_vertex* vertex)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);
        context->_vk_current_vertex =
            static_cast<jevk12_vertex*>(vertex->m_handle.m_ptr);
    }

    void render_bind_uniform_buffer(
        je_GraphicImplContext ctx, jegl_uniform_buffer* uniform_buffer)
    {
        bind_uniform_buffer(ctx, uniform_buffer);
    }

    void render_bind_texture_2d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_2d* texture)
    {
        bind_texture<jegl_texture_2d>(ctx, texture, pass);
    }

    void render_bind_texture_3d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_3d* texture)
    {
        bind_texture<jegl_texture_3d>(ctx, texture, pass);
    }

    void render_bind_texture_array(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_array* texture)
    {
        bind_texture<jegl_texture_array>(ctx, texture, pass);
    }

    void render_bind_texture_cube(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_cube* texture)
    {
        bind_texture<jegl_texture_cube>(ctx, texture, pass);
    }

    void render_prepare_uniform_block(
        jegl_vk120_context* context,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* shader_instance = context->_vk_current_binded_shader;
        assert(shader_instance != nullptr);

        if (shader_instance->m_uniform_cpu_buffer_size == 0)
        {
            assert(uniform_size == 0);
            return;
        }

        assert(uniform_data != nullptr);
        assert(uniform_size == shader_instance->m_uniform_cpu_buffer_size);

        memcpy(
            shader_instance->m_uniform_cpu_buffer,
            uniform_data,
            uniform_size);

        // Vulkan 的 NDC y 轴向下：屏幕目标已经用负高度 viewport 翻转，
        // 离屏目标用正 viewport 渲染时行序与 opengl3 一致（世界“上”落在
        // 图像末行，即引擎 V=1 的一侧），两者都不需要改 JE_NDC_SCALE。
        // 与 dx11/metal 不同，这里不能对离屏目标做 y 取反，否则 RT 内容
        // 在采样时会上下颠倒。
        //
        // 例外：sRGB 编码 pass 采样的是按屏幕方向渲染的离屏纹理，绘制到
        // 屏幕时需要翻转一次 NDC y 才能得到正确的采样方向。
        if (context->_vk_encoding_srgb_screen
            && context->_vk_current_shader_resource != nullptr)
        {
            const uint32_t ndc_offset =
                context->_vk_current_shader_resource->m_builtin_uniforms
                    .m_builtin_uniform_ndc_scale;
            if (ndc_offset != jeecs::graphic::INVALID_UNIFORM_LOCATION
                && ndc_offset + sizeof(float) * 4 <= uniform_size)
            {
                const float ndc_scale[4] = { 1.f, -1.f, 1.f, 1.f };
                memcpy(
                    shader_instance->m_uniform_cpu_buffer + ndc_offset,
                    ndc_scale,
                    sizeof(ndc_scale));
            }
        }
    }
    void render_draw_indexed(
        je_GraphicImplContext ctx,
        uint32_t index_count,
        uint32_t first_index,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);

        assert(context->_vk_current_vertex != nullptr);
        assert(index_count > 0);
        assert(instance_count > 0);

        render_prepare_uniform_block(context, uniform_data, uniform_size);
        context->cmd_draw_indexed_ex(
            context->_vk_current_vertex,
            index_count,
            first_index,
            instance_count);
    }

    void render_draw(
        je_GraphicImplContext ctx,
        uint32_t vertex_count,
        uint32_t first_vertex,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);

        assert(context->_vk_current_vertex != nullptr);
        assert(vertex_count > 0);
        assert(instance_count > 0);

        render_prepare_uniform_block(context, uniform_data, uniform_size);
        context->cmd_draw_ex(
            context->_vk_current_vertex,
            vertex_count,
            first_vertex,
            instance_count);
    }

    void render_commit_begin(je_GraphicImplContext ctx)
    {
        auto* context = static_cast<jegl_vk120_context*>(ctx);

        // 正常帧已经在 pre_update 中开始录制；暂停帧如果仍然提交了离屏命令，
        // 这里惰性开启一个不参与 present 的帧命令缓冲。
        if (!context->_vk_frame_recording)
            context->begin_frame_recording(false);
    }

    void render_commit_end(je_GraphicImplContext)
    {
        // 提交统一延迟到 update_draw_commit -> late_update，以合并同一帧的所有 pass。
    }
}

// 导出图形接口！
void jegl_using_vk120_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::vk120;

    write_to_apis->interface_startup = startup;
    write_to_apis->interface_shutdown_before_resource_release = pre_shutdown;
    write_to_apis->interface_shutdown = shutdown;

    write_to_apis->update_frame_ready = pre_update;
    write_to_apis->update_draw_commit = commit_update;

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

    write_to_apis->shader_init = init_shader;
    write_to_apis->texture_2d_init = init_texture<jegl_texture_2d>;
    write_to_apis->texture_3d_init = init_texture<jegl_texture_3d>;
    write_to_apis->texture_array_init = init_texture<jegl_texture_array>;
    write_to_apis->texture_cube_init = init_texture<jegl_texture_cube>;
    write_to_apis->texture_msaa_init = init_texture<jegl_texture_msaa>;
    write_to_apis->vertex_init = init_vertex;
    write_to_apis->framebuffer_init = init_framebuffer;
    write_to_apis->ubuffer_init = init_ubuffer;

    write_to_apis->shader_update = update_shader;
    write_to_apis->texture_2d_update = update_texture<jegl_texture_2d>;
    write_to_apis->texture_3d_update = update_texture<jegl_texture_3d>;
    write_to_apis->texture_array_update = update_texture<jegl_texture_array>;
    write_to_apis->texture_cube_update = update_texture<jegl_texture_cube>;
    write_to_apis->texture_msaa_update = update_texture<jegl_texture_msaa>;
    write_to_apis->vertex_update = update_vertex;
    write_to_apis->framebuffer_update = update_framebuffer;
    write_to_apis->ubuffer_update = update_ubuffer;

    write_to_apis->shader_close = close_shader;
    write_to_apis->texture_2d_close = close_texture<jegl_texture_2d>;
    write_to_apis->texture_3d_close = close_texture<jegl_texture_3d>;
    write_to_apis->texture_array_close = close_texture<jegl_texture_array>;
    write_to_apis->texture_cube_close = close_texture<jegl_texture_cube>;
    write_to_apis->texture_msaa_close = close_texture<jegl_texture_msaa>;
    write_to_apis->vertex_close = close_vertex;
    write_to_apis->framebuffer_close = close_framebuffer;
    write_to_apis->ubuffer_close = close_ubuffer;

    write_to_apis->pipeline_init = render_pipeline_init;
    write_to_apis->pipeline_close = render_pipeline_close;
    write_to_apis->pipeline_bind = render_pipeline_bind;

    write_to_apis->sampler_init = render_sampler_init;
    write_to_apis->sampler_close = render_sampler_close;
    write_to_apis->sampler_bind = render_sampler_bind;

    write_to_apis->begin_pass = render_begin_pass;
    write_to_apis->clear_color = render_clear_color;
    write_to_apis->clear_depth = render_clear_depth;
    write_to_apis->end_pass = render_end_pass;

    write_to_apis->bind_vertex = render_bind_vertex;
    write_to_apis->bind_uniform_buffer = render_bind_uniform_buffer;
    write_to_apis->bind_texture_2d = render_bind_texture_2d;
    write_to_apis->bind_texture_3d = render_bind_texture_3d;
    write_to_apis->bind_texture_array = render_bind_texture_array;
    write_to_apis->bind_texture_cube = render_bind_texture_cube;

    write_to_apis->draw_indexed = render_draw_indexed;
    write_to_apis->draw = render_draw;

    write_to_apis->commit_begin = render_commit_begin;
    write_to_apis->commit_end = render_commit_end;
}
#else
void jegl_using_vk120_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("vk120 not available.");
}
#endif
