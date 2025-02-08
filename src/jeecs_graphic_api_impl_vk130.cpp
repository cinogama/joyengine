#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_VK130_GAPI

#include "jeecs_imgui_backend_api.hpp"
#include <vulkan/vulkan.h>

#if defined(JE_OS_WINDOWS)
#   include <vulkan/vulkan_win32.h>
#elif defined(JE_OS_ANDROID)
#   include <vulkan/vulkan_android.h>
#elif defined(JE_OS_LINUX)
#   include <vulkan/vulkan_xlib.h>
#else
#   error Unsupport platform.
#endif

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   include "jeecs_graphic_api_interface_egl.hpp"
#else
#   include "jeecs_graphic_api_interface_glfw.hpp"
#endif // JE_GL_USE_EGL_INSTEAD_GLFW

#include <optional>
#include <array>

#undef max
#undef min

namespace jeecs::graphic::api::vk130
{
#if defined(JE_OS_WINDOWS)

#   define VK_API_PLATFORM_API_LIST \
VK_API_DECL(vkCreateWin32SurfaceKHR)

#elif defined(JE_OS_ANDROID)

#   define VK_API_PLATFORM_API_LIST \
VK_API_DECL(vkCreateAndroidSurfaceKHR)

#elif defined(JE_OS_LINUX)

#   define VK_API_PLATFORM_API_LIST \
VK_API_DECL(vkCreateXlibSurfaceKHR)

#else
#   error Unsupport platform.
#endif

#define VK_API_LIST \
VK_API_DECL(vkEnumeratePhysicalDevices);\
VK_API_DECL(vkGetPhysicalDeviceProperties);\
VK_API_DECL(vkEnumerateDeviceExtensionProperties);\
\
VK_API_DECL(vkGetPhysicalDeviceQueueFamilyProperties);\
VK_API_DECL(vkGetPhysicalDeviceFormatProperties);\
VK_API_DECL(vkCreateDevice);\
VK_API_DECL(vkDestroyDevice);\
VK_API_DECL(vkGetDeviceQueue);\
VK_API_DECL(vkQueueSubmit);\
VK_API_DECL(vkQueueWaitIdle);\
VK_API_DECL(vkDeviceWaitIdle);\
\
VK_API_DECL(vkAcquireNextImageKHR);\
VK_API_DECL(vkQueuePresentKHR);\
\
VK_API_DECL(vkDestroySurfaceKHR);\
\
VK_API_DECL(vkCreateSwapchainKHR);\
VK_API_DECL(vkDestroySwapchainKHR);\
VK_API_DECL(vkGetSwapchainImagesKHR);\
\
VK_API_DECL(vkCreateSampler);\
VK_API_DECL(vkDestroySampler);\
\
VK_API_DECL(vkCreateImage);\
VK_API_DECL(vkDestroyImage);\
VK_API_DECL(vkBindImageMemory);\
VK_API_DECL(vkGetImageMemoryRequirements);\
\
VK_API_DECL(vkCreateImageView);\
VK_API_DECL(vkDestroyImageView);\
\
VK_API_DECL(vkAllocateMemory);\
VK_API_DECL(vkFreeMemory);\
VK_API_DECL(vkMapMemory);\
VK_API_DECL(vkUnmapMemory);\
VK_API_DECL(vkBindBufferMemory);\
VK_API_DECL(vkGetBufferMemoryRequirements);\
VK_API_DECL(vkCreateBuffer);\
VK_API_DECL(vkDestroyBuffer);\
VK_API_DECL(vkFlushMappedMemoryRanges);\
\
VK_API_DECL(vkCreateShaderModule);\
VK_API_DECL(vkDestroyShaderModule);\
\
VK_API_DECL(vkCreatePipelineLayout);\
VK_API_DECL(vkDestroyPipelineLayout);\
\
VK_API_DECL(vkCreateGraphicsPipelines);\
VK_API_DECL(vkDestroyPipeline);\
\
VK_API_DECL(vkCreateFramebuffer);\
VK_API_DECL(vkDestroyFramebuffer);\
\
VK_API_DECL(vkCreateCommandPool);\
VK_API_DECL(vkDestroyCommandPool);\
VK_API_DECL(vkResetCommandPool);\
VK_API_DECL(vkAllocateCommandBuffers);\
VK_API_DECL(vkFreeCommandBuffers);\
VK_API_DECL(vkBeginCommandBuffer);\
VK_API_DECL(vkResetCommandBuffer);\
VK_API_DECL(vkEndCommandBuffer);\
VK_API_DECL(vkCmdBeginRenderPass);\
VK_API_DECL(vkCmdEndRenderPass);\
VK_API_DECL(vkCmdBindPipeline);\
VK_API_DECL(vkCmdBindVertexBuffers2);\
VK_API_DECL(vkCmdDraw);\
VK_API_DECL(vkCmdSetViewport);\
VK_API_DECL(vkCmdSetScissor);\
VK_API_DECL(vkCmdClearAttachments);\
VK_API_DECL(vkCmdSetPrimitiveTopology);\
VK_API_DECL(vkCmdSetPrimitiveRestartEnable);\
VK_API_DECL(vkCmdBindDescriptorSets);\
VK_API_DECL(vkCmdCopyBufferToImage);\
VK_API_DECL(vkCmdPipelineBarrier);\
VK_API_DECL(vkCmdPushConstants);\
VK_API_DECL(vkCmdDrawIndexed);\
VK_API_DECL(vkCmdBindVertexBuffers);\
VK_API_DECL(vkCmdBindIndexBuffer);\
\
VK_API_DECL(vkCreateSemaphore);\
VK_API_DECL(vkDestroySemaphore);\
VK_API_DECL(vkWaitSemaphores);\
\
VK_API_DECL(vkCreateFence);\
VK_API_DECL(vkDestroyFence);\
VK_API_DECL(vkWaitForFences);\
VK_API_DECL(vkResetFences);\
\
VK_API_DECL(vkCreateDescriptorSetLayout);\
VK_API_DECL(vkDestroyDescriptorSetLayout);\
\
VK_API_DECL(vkCreateRenderPass);\
VK_API_DECL(vkDestroyRenderPass);\
\
VK_API_DECL(vkCreateDescriptorPool);\
VK_API_DECL(vkDestroyDescriptorPool);\
VK_API_DECL(vkAllocateDescriptorSets);\
VK_API_DECL(vkFreeDescriptorSets);\
VK_API_DECL(vkUpdateDescriptorSets);\
\
VK_API_DECL(vkGetPhysicalDeviceSurfaceSupportKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceFormatsKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfacePresentModesKHR);\
\
VK_API_DECL(vkCreateDebugUtilsMessengerEXT);\
VK_API_DECL(vkDestroyDebugUtilsMessengerEXT);\
\
VK_API_DECL(vkGetPhysicalDeviceMemoryProperties);\
VK_API_DECL(vkGetPhysicalDeviceProperties2);\
\
VK_API_PLATFORM_API_LIST

    struct jegl_vk130_context;

    struct jevk11_shader_blob
    {
        struct blob_data
        {
            JECS_DISABLE_MOVE_AND_COPY(blob_data);

            blob_data() = default;
            ~blob_data();

            inline static const VkDynamicState m_dynamic_states[] =
            {
                //VkDynamicState::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT,
                VkDynamicState::VK_DYNAMIC_STATE_SCISSOR,
                VkDynamicState::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                VkDynamicState::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
                VkDynamicState::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
            };

            jegl_vk130_context* m_context;

            VkShaderModule m_vertex_shader_module;
            VkShaderModule m_fragment_shader_module;
            VkPipelineShaderStageCreateInfo m_shader_stage_infos[2];

            std::vector<VkVertexInputAttributeDescription>  m_vertex_input_attribute_descriptions;
            VkVertexInputBindingDescription                 m_vertex_input_binding_description;
            VkPipelineVertexInputStateCreateInfo            m_vertex_input_state_create_info;
            VkPipelineInputAssemblyStateCreateInfo          m_input_assembly_state_create_info;
            VkViewport                                      m_viewport;
            VkRect2D                                        m_scissor;
            VkPipelineViewportStateCreateInfo               m_viewport_state_create_info;
            VkPipelineRasterizationStateCreateInfo          m_rasterization_state_create_info;
            VkPipelineDepthStencilStateCreateInfo           m_depth_stencil_state_create_info;
            VkPipelineMultisampleStateCreateInfo            m_multi_sample_state_create_info;
            VkPipelineColorBlendAttachmentState             m_color_blend_attachment_state;
            VkPipelineDynamicStateCreateInfo                m_dynamic_state_create_info;
            VkPipelineLayout                                m_pipeline_layout;

            struct sampler_data
            {
                VkSampler   m_vk_sampler;
                uint32_t    m_sampler_id;
            };
            std::vector<sampler_data>                       m_samplers;
            std::unordered_map<std::string, uint32_t>       m_ulocations;
            size_t                                          m_uniform_size;
        };

        jeecs::basic::resource<blob_data> m_blob_data;

        uint32_t get_built_in_location(const std::string& name)const
        {
            auto fnd = m_blob_data->m_ulocations.find(name);
            if (fnd == m_blob_data->m_ulocations.end())
                return jeecs::typing::INVALID_UINT32;
            return fnd->second;
        }
    };
    struct jevk11_texture
    {
        //VkBuffer m_vk_texture_buffer;
        //VkDeviceMemory m_vk_texture_buffer_memory;

        VkImage m_vk_texture_image;
        VkDeviceMemory m_vk_texture_image_memory;

        VkImageView m_vk_texture_image_view;
        VkFormat m_vk_texture_format;
    };
    struct jevk11_uniformbuf
    {
        VkBuffer m_uniform_buffer;
        VkDeviceMemory m_uniform_buffer_memory;

        uint32_t m_real_binding_place;
    };
    struct jevk11_shader
    {
        jeecs::basic::resource<jevk11_shader_blob::blob_data>
            m_blob_data;

        // Vulkan的这个设计真的让人很想吐槽，因为Pipeline和Shader与Pass/Rendbuffer
        // 捆绑在一起了，想尽办法最后就得靠目标渲染缓冲区的格式作为索引，根据shader
        // 的不同渲染目标(格式)创建不同的渲染管线
        // TODO: 考虑什么情况下要释放Pipeline，如果Rendbuffer被释放了，那么Pipeline
        //      也应该被释放。但是得考虑释放的时机
        std::unordered_map<VkRenderPass, VkPipeline>
            m_target_pass_pipelines;

        std::vector<jevk11_uniformbuf*> m_uniform_variables;
        size_t m_next_allocate_ubos_for_uniform_variable;
        size_t m_command_commit_round;

        void* m_uniform_cpu_buffer;
        size_t              m_uniform_cpu_buffer_size;
        bool                m_uniform_cpu_buffer_updated;

        VkPipeline prepare_pipeline(jegl_vk130_context* context);
        jevk11_uniformbuf* allocate_ubo_for_vars(jegl_vk130_context* context);
        jevk11_uniformbuf* get_last_usable_variable(jegl_vk130_context* context);
    };
    struct jevk11_framebuffer
    {
        VkRenderPass    m_rendpass;
        std::vector<jevk11_texture*>
            m_color_attachments;
        jevk11_texture* m_depth_attachment;
        VkSemaphore     m_render_finished_semaphore;
        // VkFence         m_render_finished_fence;
        VkFramebuffer   m_framebuffer;

        size_t          m_width;
        size_t          m_height;

        size_t          m_rend_rounds;
    };
    struct jevk11_vertex
    {
        VkBuffer m_vk_vertex_buffer;
        VkDeviceMemory m_vk_vertex_buffer_memory;

        VkBuffer m_vk_index_buffer;
        VkDeviceMemory m_vk_index_buffer_memory;

        uint32_t m_vertex_point_count;
        VkDeviceSize m_size;
        VkDeviceSize m_stride;

        VkPrimitiveTopology m_topology;
    };

    struct jegl_vk130_context
    {
        struct vklibrary_instance_proxy
        {
            void* _instance;

            vklibrary_instance_proxy()
            {
#ifdef JE_OS_WINDOWS
                _instance = wo_load_lib("je/graphiclib/vulkan-1", "vulkan-1.dll", nullptr, false);
#else
                _instance = wo_load_lib("je/graphiclib/vulkan", "libvulkan.so.1", nullptr, false);
                if (_instance == nullptr)
                    _instance = wo_load_lib("je/graphiclib/vulkan", "libvulkan.so", nullptr, false);
#endif
                if (_instance == nullptr)
                {
                    jeecs::debug::logfatal("Failed to get vulkan library instance.");
                }
            }
            ~vklibrary_instance_proxy()
            {
                assert(_instance != nullptr);
                wo_unload_lib(_instance, WO_DYLIB_UNREF);
            }

            JECS_DISABLE_MOVE_AND_COPY(vklibrary_instance_proxy);

            template<typename FT>
            FT api(const char* name)
            {
                auto* result_ft = reinterpret_cast<FT>(wo_load_func(_instance, name));
                if (result_ft == nullptr)
                {
                    jeecs::debug::logfatal("Failed to get vulkan library api: '%s'.", name);
                }
                return result_ft;
            }
        };

        vklibrary_instance_proxy vk_proxy;

#define VK_API_DECL(name) PFN_##name name = vk_proxy.api<PFN_##name>(#name);

        VK_API_DECL(vkCreateInstance);
        VK_API_DECL(vkDestroyInstance);

        VK_API_DECL(vkGetInstanceProcAddr);

        VK_API_DECL(vkEnumerateInstanceVersion);
        VK_API_DECL(vkEnumerateInstanceLayerProperties);
        VK_API_DECL(vkEnumerateInstanceExtensionProperties);

#undef VK_API_DECL

        inline thread_local static jegl_vk130_context* _vk_this_context = nullptr;

        // 一些配置项
        size_t                      _vk_msaa_config;
        bool                        _vk_vsync_config;

        // Vk的全局实例
        VkInstance          _vk_instance;
        basic_interface*    _vk_jegl_interface;

        VkPhysicalDevice    _vk_device;
        uint32_t            _vk_device_queue_graphic_family_index;
        uint32_t            _vk_device_queue_present_family_index;
        VkDevice            _vk_logic_device;
        VkQueue             _vk_logic_device_graphic_queue;
        VkQueue             _vk_logic_device_present_queue;

        VkSurfaceKHR                _vk_surface;
        VkSurfaceCapabilitiesKHR    _vk_surface_capabilities;
        VkSurfaceFormatKHR          _vk_surface_format;
        VkPresentModeKHR            _vk_surface_present_mode;
        VkFormat                    _vk_depth_format;

        VkSwapchainKHR                      _vk_swapchain;
        std::vector<jevk11_framebuffer*>    _vk_swapchain_framebuffer;
        std::vector<jevk11_texture*>        _vk_swapchain_textures_for_free;

        template<VkDescriptorType DESC_SET_TYPE, uint32_t MAX_BINDING, VkShaderStageFlags STAGE>
        struct single_descriptor_set_allocator
        {
            VkDescriptorSetLayout          m_layout;
            std::vector<VkDescriptorPool>  m_pools;
            size_t                         m_next;
            std::vector<VkDescriptorSet>   m_sets;

            jegl_vk130_context* m_context;

            JECS_DISABLE_MOVE_AND_COPY(single_descriptor_set_allocator);

            single_descriptor_set_allocator(jegl_vk130_context* ctx)
                : m_context(ctx)
            {
                m_next = 0;

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

                if (VK_SUCCESS != m_context->vkCreateDescriptorSetLayout(
                    m_context->_vk_logic_device,
                    &ubo_layout_create_info,
                    nullptr,
                    &m_layout))
                {
                    jeecs::debug::logfatal("Failed to create vk130 uniform variables descriptor set layout.");
                }
            }
            ~single_descriptor_set_allocator()
            {
                for (auto& pool : m_pools)
                    _vk_this_context->vkDestroyDescriptorPool(_vk_this_context->_vk_logic_device, pool, nullptr);

                _vk_this_context->vkDestroyDescriptorSetLayout(_vk_this_context->_vk_logic_device, m_layout, nullptr);
            }

            void flush_descriptor_set_allocator()
            {
                m_next = 0;
            }
            VkDescriptorSet allocate_descriptor_set()
            {
                if (m_next < m_sets.size())
                    return m_sets[m_next++];

                VkDescriptorSet result;
                for (;;)
                {
                    if (!m_pools.empty())
                    {
                        VkDescriptorSetAllocateInfo ubo_set_alloc_info = {};
                        ubo_set_alloc_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                        ubo_set_alloc_info.pNext = nullptr;
                        ubo_set_alloc_info.descriptorPool = m_pools.back();
                        ubo_set_alloc_info.descriptorSetCount = 1;
                        ubo_set_alloc_info.pSetLayouts = &m_layout;

                        auto alloc_result = _vk_this_context->vkAllocateDescriptorSets(
                            _vk_this_context->_vk_logic_device, &ubo_set_alloc_info, &result);

                        if (VK_SUCCESS == alloc_result)
                        {
                            m_sets.push_back(result);
                            m_next++;
                            return result;
                        }
                    }

                    VkDescriptorPool new_created_pool;

                    VkDescriptorPoolSize pool_sizes[1] = {};
                    pool_sizes[0].type = DESC_SET_TYPE;
                    pool_sizes[0].descriptorCount = 128;

                    VkDescriptorPoolCreateInfo pool_create_info = {};
                    pool_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                    pool_create_info.pNext = nullptr;
                    pool_create_info.flags = 0;
                    pool_create_info.maxSets = 128;
                    pool_create_info.poolSizeCount = 1;
                    pool_create_info.pPoolSizes = pool_sizes;

                    if (VK_SUCCESS != _vk_this_context->vkCreateDescriptorPool(
                        _vk_this_context->_vk_logic_device, &pool_create_info, nullptr, &new_created_pool))
                    {
                        jeecs::debug::logfatal("Failed to create vk130 descriptor pool.");
                    }
                    m_pools.push_back(new_created_pool);
                }
            }
        };

        struct descriptor_set_allocator
        {
            constexpr static size_t MAX_LAYOUT_COUNT = 16;
            constexpr static size_t MAX_UBO_LAYOUT = MAX_LAYOUT_COUNT;
            constexpr static size_t MAX_TEXTURE_LAYOUT = MAX_LAYOUT_COUNT;
            constexpr static size_t MAX_SAMPLER_LAYOUT = MAX_LAYOUT_COUNT;

            jegl_vk130_context* m_context;

            enum DescriptorSetType : uint32_t
            {
                UNIFORM_VARIABLES = 0,
                UNIFORM_BUFFER,
                TEXTURE,
                SAMPLER,

                COUNT,
                BEGIN = UNIFORM_VARIABLES,
            };
            VkDescriptorSetLayout m_vk_global_descriptor_set_layouts[DescriptorSetType::COUNT];

            single_descriptor_set_allocator<VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL>
                m_vk_shader_uniform_variable_sets;
            single_descriptor_set_allocator<VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_UBO_LAYOUT, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL>
                m_vk_shader_uniform_block_sets;
            single_descriptor_set_allocator<VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_LAYOUT, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL>
                m_vk_shader_uniform_texture_sets;
            single_descriptor_set_allocator<VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLER_LAYOUT, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL>
                m_vk_shader_uniform_sampler_sets;

            // Binding update context;
            VkDescriptorBufferInfo  m_binded_uvar;
            VkDescriptorBufferInfo  m_binded_ubos[MAX_UBO_LAYOUT];
            VkDescriptorImageInfo   m_binded_textures[MAX_TEXTURE_LAYOUT];
            VkDescriptorImageInfo   m_binded_samplers[MAX_SAMPLER_LAYOUT];
            VkPipelineLayout        m_binded_pipeline_layout;

            VkWriteDescriptorSet m_updated_writes_reusing[MAX_LAYOUT_COUNT];
            VkDescriptorSet m_updated_sets[DescriptorSetType::COUNT];

            void flush_descriptor_set_allocator()
            {
                m_binded_pipeline_layout = VK_NULL_HANDLE;

                m_vk_shader_uniform_variable_sets.flush_descriptor_set_allocator();
                m_vk_shader_uniform_block_sets.flush_descriptor_set_allocator();
                m_vk_shader_uniform_texture_sets.flush_descriptor_set_allocator();
                m_vk_shader_uniform_sampler_sets.flush_descriptor_set_allocator();

                m_binded_uvar.buffer = VK_NULL_HANDLE;
                for (auto& ubos : m_binded_ubos)
                    ubos.buffer = VK_NULL_HANDLE;
                for (auto& textures : m_binded_textures)
                    textures.imageView = VK_NULL_HANDLE;
                for (auto& samplers : m_binded_samplers)
                    samplers.sampler = VK_NULL_HANDLE;

                for (auto& set : m_updated_sets)
                    set = VK_NULL_HANDLE;

            }
            void bind_uniform_vars(VkBuffer buffer)
            {
                if (m_binded_uvar.buffer != buffer)
                {
                    m_binded_uvar.buffer = buffer;
                    m_updated_sets[DescriptorSetType::UNIFORM_VARIABLES] = VK_NULL_HANDLE;
                }
            }
            void bind_uniform_buffer(uint32_t binding, VkBuffer buffer)
            {
                if (m_binded_ubos[binding].buffer != buffer)
                {
                    m_binded_ubos[binding].buffer = buffer;
                    m_updated_sets[DescriptorSetType::UNIFORM_BUFFER] = VK_NULL_HANDLE;
                }
            }
            void bind_texture(uint32_t binding, VkImageView image_view)
            {
                if (m_binded_textures[binding].imageView != image_view)
                {
                    m_binded_textures[binding].imageView = image_view;
                    m_updated_sets[DescriptorSetType::TEXTURE] = VK_NULL_HANDLE;
                }
            }
            void bind_sampler(uint32_t binding, VkSampler sampler)
            {
                if (m_binded_samplers[binding].sampler != sampler)
                {
                    m_binded_samplers[binding].sampler = sampler;
                    m_updated_sets[DescriptorSetType::SAMPLER] = VK_NULL_HANDLE;
                }
            }
            void bind_pipeline_layout(VkPipelineLayout pipeline_layout)
            {
                m_binded_pipeline_layout = pipeline_layout;
            }
            void apply_binding_work()
            {
                assert(m_binded_pipeline_layout != VK_NULL_HANDLE);

                // 更新Uniform variables
                if (VK_NULL_HANDLE == m_updated_sets[DescriptorSetType::UNIFORM_VARIABLES])
                {
                    auto set = m_vk_shader_uniform_variable_sets.allocate_descriptor_set();
                    m_updated_sets[DescriptorSetType::UNIFORM_VARIABLES] = set;

                    // Update layouts
                    m_updated_writes_reusing[0].dstBinding = 0;
                    m_updated_writes_reusing[0].dstSet = set;
                    m_updated_writes_reusing[0].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    m_updated_writes_reusing[0].pBufferInfo = &m_binded_uvar;

                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device,
                        1,
                        &m_updated_writes_reusing[0],
                        0,
                        nullptr);
                }
                // 更新Uniform buffer
                if (VK_NULL_HANDLE == m_updated_sets[DescriptorSetType::UNIFORM_BUFFER])
                {
                    auto set = m_vk_shader_uniform_block_sets.allocate_descriptor_set();
                    m_updated_sets[DescriptorSetType::UNIFORM_BUFFER] = set;

                    // Update layouts
                    for (size_t i = 0; i < MAX_UBO_LAYOUT; ++i)
                    {
                        auto& write = m_updated_writes_reusing[i];
                        write.dstBinding = (uint32_t)i;
                        write.dstSet = set;
                        write.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        write.pBufferInfo = &m_binded_ubos[i];
                    }

                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device,
                        MAX_UBO_LAYOUT,
                        m_updated_writes_reusing,
                        0,
                        nullptr);
                }
                // 更新Texture
                if (VK_NULL_HANDLE == m_updated_sets[DescriptorSetType::TEXTURE])
                {
                    auto set = m_vk_shader_uniform_texture_sets.allocate_descriptor_set();
                    m_updated_sets[DescriptorSetType::TEXTURE] = set;

                    // Update layouts
                    for (size_t i = 0; i < MAX_TEXTURE_LAYOUT; ++i)
                    {
                        auto& write = m_updated_writes_reusing[i];
                        write.dstBinding = (uint32_t)i;
                        write.dstSet = set;
                        write.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        write.pImageInfo = &m_binded_textures[i];
                    }

                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device,
                        MAX_TEXTURE_LAYOUT,
                        m_updated_writes_reusing,
                        0,
                        nullptr);
                }
                // 更新Sampler
                if (VK_NULL_HANDLE == m_updated_sets[DescriptorSetType::SAMPLER])
                {
                    auto set = m_vk_shader_uniform_sampler_sets.allocate_descriptor_set();
                    m_updated_sets[DescriptorSetType::SAMPLER] = set;

                    // Update layouts
                    size_t update_count = 0;
                    for (size_t i = 0; i < MAX_SAMPLER_LAYOUT; ++i)
                    {
                        if (m_binded_samplers[i].sampler != VK_NULL_HANDLE)
                        {
                            auto& write = m_updated_writes_reusing[update_count++];
                            write.dstBinding = (uint32_t)i;
                            write.dstSet = set;
                            write.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
                            write.pImageInfo = &m_binded_samplers[i];
                        }
                    }

                    m_context->vkUpdateDescriptorSets(
                        m_context->_vk_logic_device,
                        MAX_SAMPLER_LAYOUT,
                        m_updated_writes_reusing,
                        0,
                        nullptr);
                }

                m_context->vkCmdBindDescriptorSets(
                    m_context->_vk_current_command_buffer,
                    VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_binded_pipeline_layout,
                    0,
                    DescriptorSetType::COUNT,
                    m_updated_sets,
                    0,
                    nullptr);
            }

            JECS_DISABLE_MOVE_AND_COPY(descriptor_set_allocator);

            descriptor_set_allocator(jegl_vk130_context* context)
                : m_context(context)
                , m_vk_shader_uniform_variable_sets(context)
                , m_vk_shader_uniform_block_sets(context)
                , m_vk_shader_uniform_texture_sets(context)
                , m_vk_shader_uniform_sampler_sets(context)
            {
                static_assert(MAX_LAYOUT_COUNT >= MAX_SAMPLER_LAYOUT);
                static_assert(MAX_LAYOUT_COUNT >= MAX_TEXTURE_LAYOUT);
                static_assert(MAX_LAYOUT_COUNT >= MAX_UBO_LAYOUT);

                assert(m_context != nullptr);

                for (uint32_t i = 0; i < MAX_LAYOUT_COUNT; ++i)
                {
                    auto& write = m_updated_writes_reusing[i];

                    write.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.pNext = nullptr;
                    write.dstBinding = i;
                    write.dstArrayElement = 0;
                    write.descriptorCount = 1;

                    write.dstSet = VK_NULL_HANDLE;
                    write.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    write.pImageInfo = nullptr;
                    write.pBufferInfo = nullptr;
                    write.pTexelBufferView = nullptr;
                }

                m_binded_uvar.buffer = VK_NULL_HANDLE;
                m_binded_uvar.offset = 0;
                m_binded_uvar.range = VK_WHOLE_SIZE;
                for (auto& ubos : m_binded_ubos)
                {
                    ubos.buffer = VK_NULL_HANDLE;
                    ubos.offset = 0;
                    ubos.range = VK_WHOLE_SIZE;
                }
                for (auto& textures : m_binded_textures)
                {
                    textures.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                    textures.imageView = VK_NULL_HANDLE;
                    textures.sampler = VK_NULL_HANDLE;
                }
                for (auto& samplers : m_binded_samplers)
                {
                    samplers.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                    samplers.imageView = VK_NULL_HANDLE;
                    samplers.sampler = VK_NULL_HANDLE;
                }

                flush_descriptor_set_allocator();

                m_vk_global_descriptor_set_layouts[DescriptorSetType::UNIFORM_VARIABLES] =
                    m_vk_shader_uniform_variable_sets.m_layout;

                m_vk_global_descriptor_set_layouts[DescriptorSetType::UNIFORM_BUFFER] =
                    m_vk_shader_uniform_block_sets.m_layout;

                m_vk_global_descriptor_set_layouts[DescriptorSetType::TEXTURE] =
                    m_vk_shader_uniform_texture_sets.m_layout;

                m_vk_global_descriptor_set_layouts[DescriptorSetType::SAMPLER] =
                    m_vk_shader_uniform_sampler_sets.m_layout;

            }
            ~descriptor_set_allocator()
            {
            }
        };
        struct command_buffer_allocator
        {
            jegl_vk130_context* m_context;

            VkCommandPool                   m_command_pool;

            std::vector<VkCommandBuffer>    m_cmd_buffers;
            size_t                          m_next_cmd_buffer;

            std::vector<VkSemaphore>        m_cmd_semaphores;
            size_t                          m_next_cmd_semaphore;

            VkSemaphore allocate_semaphore()
            {
                if (m_next_cmd_semaphore < m_cmd_semaphores.size())
                    return m_cmd_semaphores[m_next_cmd_semaphore++];

                VkSemaphore result = m_context->create_semaphore();
                m_cmd_semaphores.push_back(result);
                m_next_cmd_semaphore++;
                return result;
            }
            VkCommandBuffer allocate_command_buffer()
            {
                if (m_next_cmd_buffer < m_cmd_buffers.size())
                    return m_cmd_buffers[m_next_cmd_buffer++];

                VkCommandBufferAllocateInfo command_buffer_alloc_info = {};
                command_buffer_alloc_info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                command_buffer_alloc_info.pNext = nullptr;
                command_buffer_alloc_info.commandPool = m_command_pool;
                command_buffer_alloc_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                command_buffer_alloc_info.commandBufferCount = 1;

                VkCommandBuffer result = nullptr;
                if (m_context->vkAllocateCommandBuffers(m_context->_vk_logic_device, &command_buffer_alloc_info, &result) != VK_SUCCESS)
                {
                    jeecs::debug::logfatal("Failed to allocate command buffers!");
                }

                m_cmd_buffers.push_back(result);
                m_next_cmd_buffer++;
                return result;
            }

            void flush_command_buffer_allocator()
            {
                m_next_cmd_buffer = 0;
                m_next_cmd_semaphore = 0;
            }

            JECS_DISABLE_MOVE_AND_COPY(command_buffer_allocator);

            command_buffer_allocator(jegl_vk130_context* context)
            {
                m_context = context;
                assert(m_context != nullptr);

                VkCommandPoolCreateInfo default_command_pool_create_info = {};
                default_command_pool_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                default_command_pool_create_info.pNext = nullptr;
                default_command_pool_create_info.flags =
                    VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                    VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                default_command_pool_create_info.queueFamilyIndex = m_context->_vk_device_queue_graphic_family_index;

                if (VK_SUCCESS != m_context->vkCreateCommandPool(
                    m_context->_vk_logic_device,
                    &default_command_pool_create_info,
                    nullptr,
                    &m_command_pool))
                {
                    jeecs::debug::logfatal("Failed to create vk130 default command pool.");
                }
            }
            ~command_buffer_allocator()
            {
                for (auto& cmd_buffer : m_cmd_buffers)
                    m_context->vkFreeCommandBuffers(m_context->_vk_logic_device, m_command_pool, 1, &cmd_buffer);

                for (auto& semaphore : m_cmd_semaphores)
                    m_context->destroy_semaphore(semaphore);

                m_context->vkDestroyCommandPool(m_context->_vk_logic_device, m_command_pool, nullptr);
            }

        };

        descriptor_set_allocator* _vk_descriptor_set_allocator;
        single_descriptor_set_allocator<VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT>*
            _vk_dear_imgui_descriptor_set_allocator;

        command_buffer_allocator* _vk_command_buffer_allocator;

        VkSampler _vk_dear_imgui_sampler;
        VkDescriptorPool _vk_dear_imgui_descriptor_pool;

        // Following is runtime vk states.
        uint32_t            _vk_presenting_swapchain_image_index;
        jevk11_framebuffer* _vk_current_swapchain_framebuffer;
        jevk11_framebuffer* _vk_current_target_framebuffer;
        VkCommandBuffer     _vk_current_command_buffer;
        jevk11_shader*      _vk_current_binded_shader;
        size_t              _vk_command_commit_round;

        VkSemaphore             _vk_last_command_buffer_semaphore;
        VkPipelineStageFlags    _vk_wait_for_last_command_buffer_stage;

        /////////////////////////////////////////////////////////////////////
        VkCommandBuffer begin_temp_command_buffer_records()
        {
            VkCommandBuffer result = _vk_command_buffer_allocator->allocate_command_buffer();

            VkCommandBufferBeginInfo command_buffer_begin_info = {};
            command_buffer_begin_info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            command_buffer_begin_info.pNext = nullptr;
            command_buffer_begin_info.flags = 0;
            command_buffer_begin_info.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(result, &command_buffer_begin_info) != VK_SUCCESS)
            {
                jeecs::debug::logfatal("Failed to begin recording command buffer!");
            }

            return result;
        }
        void end_temp_command_buffer_record(VkCommandBuffer cmd_buffer)
        {
            if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS)
            {
                jeecs::debug::logfatal("Failed to record command buffer!");
            }

            VkSubmitInfo submit_info = {};
            submit_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &cmd_buffer;

            vkQueueSubmit(_vk_logic_device_graphic_queue, 1, &submit_info, VK_NULL_HANDLE);
            vkQueueWaitIdle(_vk_logic_device_graphic_queue);
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
                jeecs::debug::logfatal("Failed to create vk130 semaphore.");
            }
            return result;
        }
        void destroy_semaphore(VkSemaphore semaphore)
        {
            vkDestroySemaphore(_vk_logic_device, semaphore, nullptr);
        }

        VkFence create_fence()
        {
            VkFenceCreateInfo fence_create_info = {};
            fence_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_create_info.pNext = nullptr;
            fence_create_info.flags = 0; // 创建时必须被设置为空就绪状态

            VkFence result = VK_NULL_HANDLE;
            if (VK_SUCCESS != vkCreateFence(_vk_logic_device, &fence_create_info, nullptr, &result))
            {
                jeecs::debug::logfatal("Failed to create vk130 fence.");
            }
            return result;
        }
        void destroy_fence(VkFence fence)
        {
            vkDestroyFence(_vk_logic_device, fence, nullptr);
        }

        // TODO: 这里的参数应该还包含fb的附件配置信息
        jevk11_framebuffer* create_frame_buffer(
            size_t w,
            size_t h,
            const std::vector<jevk11_texture*>& attachment_colors,
            jevk11_texture* attachment_depth,
            VkImageLayout final_layout)
        {
            jevk11_framebuffer* result = new jevk11_framebuffer{};

            result->m_rend_rounds = 0;

            result->m_width = w;
            result->m_height = h;

            std::vector<VkAttachmentDescription> color_and_depth_attachments(
                attachment_colors.size() + (attachment_depth == nullptr ? 0 : 1));
            std::vector<VkAttachmentReference> color_attachment_refs(attachment_colors.size());

            VkAttachmentReference depth_attachment_ref = {};

            for (size_t attachment_i = 0; attachment_i < attachment_colors.size(); ++attachment_i)
            {
                // TODO: 这边的目标格式暂时没动，之后真正的framebuffer是需要调整的
                auto& attachment = color_and_depth_attachments[attachment_i];
                attachment.flags = 0;
                attachment.format = attachment_colors[attachment_i]->m_vk_texture_format;
                attachment.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = final_layout;
                attachment.finalLayout = final_layout;

                VkAttachmentReference& attachment_ref = color_attachment_refs[attachment_i];
                attachment_ref.attachment = (uint32_t)attachment_i;
                attachment_ref.layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            if (attachment_depth != nullptr)
            {
                assert(!color_and_depth_attachments.empty());
                VkAttachmentDescription& depth_attachment = color_and_depth_attachments.back();

                depth_attachment.flags = 0;
                depth_attachment.format = attachment_depth->m_vk_texture_format;
                depth_attachment.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
                depth_attachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                depth_attachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
                depth_attachment.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depth_attachment.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth_attachment.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depth_attachment.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                depth_attachment_ref.attachment = (uint32_t)color_and_depth_attachments.size() - 1;
                depth_attachment_ref.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            VkSubpassDescription default_render_subpass = {};
            default_render_subpass.flags = 0;
            default_render_subpass.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
            default_render_subpass.inputAttachmentCount = 0;
            default_render_subpass.pInputAttachments = nullptr;
            default_render_subpass.colorAttachmentCount = (uint32_t)color_attachment_refs.size();
            default_render_subpass.pColorAttachments = color_attachment_refs.data();
            default_render_subpass.pResolveAttachments = nullptr;
            default_render_subpass.pDepthStencilAttachment =
                attachment_depth != nullptr ? &depth_attachment_ref : nullptr;
            default_render_subpass.preserveAttachmentCount = 0;
            default_render_subpass.pPreserveAttachments = nullptr;

            VkSubpassDependency default_render_subpass_dependency = {};
            default_render_subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            default_render_subpass_dependency.dstSubpass = 0;
            default_render_subpass_dependency.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            default_render_subpass_dependency.srcAccessMask = 0;
            default_render_subpass_dependency.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            default_render_subpass_dependency.dstAccessMask = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo default_render_pass_create_info = {};
            default_render_pass_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            default_render_pass_create_info.flags = 0;
            default_render_pass_create_info.pNext = nullptr;
            default_render_pass_create_info.attachmentCount = (uint32_t)color_and_depth_attachments.size();
            default_render_pass_create_info.pAttachments = color_and_depth_attachments.data();
            default_render_pass_create_info.subpassCount = 1;
            default_render_pass_create_info.pSubpasses = &default_render_subpass;
            default_render_pass_create_info.dependencyCount = 1;
            default_render_pass_create_info.pDependencies = &default_render_subpass_dependency;

            if (VK_SUCCESS != vkCreateRenderPass(
                _vk_logic_device,
                &default_render_pass_create_info,
                nullptr,
                &result->m_rendpass))
            {
                jeecs::debug::logfatal("Failed to create vk130 default render pass.");
            }

            result->m_color_attachments = attachment_colors;
            result->m_depth_attachment = attachment_depth;

            std::vector<VkImageView> attachment_image_views(
                result->m_color_attachments.size() + (attachment_depth == nullptr ? 0 : 1));
            for (size_t i = 0; i < result->m_color_attachments.size(); ++i)
            {
                attachment_image_views[i] = result->m_color_attachments[i]->m_vk_texture_image_view;
            }
            if (result->m_depth_attachment != nullptr)
                attachment_image_views.back() = result->m_depth_attachment->m_vk_texture_image_view;

            VkFramebufferCreateInfo framebuffer_create_info = {};
            framebuffer_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.pNext = nullptr;
            framebuffer_create_info.flags = 0;
            framebuffer_create_info.renderPass = result->m_rendpass;
            framebuffer_create_info.attachmentCount = (uint32_t)attachment_image_views.size();
            framebuffer_create_info.pAttachments = attachment_image_views.data();
            framebuffer_create_info.width = (uint32_t)w;
            framebuffer_create_info.height = (uint32_t)h;
            framebuffer_create_info.layers = 1;

            if (VK_SUCCESS != vkCreateFramebuffer(_vk_logic_device, &framebuffer_create_info, nullptr, &result->m_framebuffer))
            {
                jeecs::debug::logfatal("Failed to create vk130 swapchain framebuffer.");
            }

            result->m_render_finished_semaphore = create_semaphore();
            // result->m_render_finished_fence = create_fence();

            return result;
        }
        void destroy_frame_buffer(jevk11_framebuffer* fb)
        {
            destroy_semaphore(fb->m_render_finished_semaphore);
            // destroy_fence(fb->m_render_finished_fence);
            vkDestroyRenderPass(_vk_logic_device, fb->m_rendpass, nullptr);
            vkDestroyFramebuffer(_vk_logic_device, fb->m_framebuffer, nullptr);
        }

        void destroy_swap_chain()
        {
            for (auto* fb : _vk_swapchain_framebuffer)
                destroy_frame_buffer(fb);

            for (auto* tex : _vk_swapchain_textures_for_free)
                destroy_texture_instance(tex);

            _vk_swapchain_textures_for_free.clear();
            _vk_swapchain_framebuffer.clear();

            vkDestroySwapchainKHR(_vk_logic_device, _vk_swapchain, nullptr);
        }

        void recreate_swap_chain_for_current_surface(size_t w, size_t h)
        {
            vkDeviceWaitIdle(_vk_logic_device);

            // 在此处开始创建交换链
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vk_device, _vk_surface, &_vk_surface_capabilities);

            VkExtent2D used_extent = { (uint32_t)w, (uint32_t)h };
            if (_vk_surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                used_extent = _vk_surface_capabilities.currentExtent;
            }
            else {
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
            assert(!vk_surface_formats.empty());

            // 获取交换链支持的呈现模式
            uint32_t vk_present_mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(_vk_device, _vk_surface, &vk_present_mode_count, nullptr);

            std::vector<VkPresentModeKHR> vk_present_modes(vk_present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(_vk_device, _vk_surface, &vk_present_mode_count, vk_present_modes.data());
            assert(!vk_present_modes.empty());

            // 获取受支持的颜色格式，优先选择VK_FORMAT_B8G8R8A8_UNORM 和 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            _vk_surface_format = {};
            if (vk_surface_formats.size() == 1 && vk_surface_formats.front().format == VK_FORMAT_UNDEFINED)
            {
                _vk_surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
                _vk_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }
            else
            {
                bool found_support_format = false;;
                for (auto& format : vk_surface_formats)
                {
                    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    {
                        found_support_format = true;
                        _vk_surface_format = format;
                        break;
                    }
                }
                if (!found_support_format)
                    _vk_surface_format = vk_surface_formats.front();
            }

            // 获取受支持的深度数据格式
            _vk_depth_format = VK_FORMAT_UNDEFINED;
            std::vector<VkFormat> vk_depth_formats = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
            };

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

            // 设置交换链呈现模式，优先选择VK_PRESENT_MODE_MAILBOX_KHR，其次是VK_PRESENT_MODE_FIFO_KHR；
            // 如果都不支持，则回滚到VK_PRESENT_MODE_IMMEDIATE_KHR；如果以上模式均不支持，则直接终止
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

            if (vk_present_mode_mailbox_supported && !_vk_vsync_config)
                _vk_surface_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (vk_present_mode_fifo_supported && _vk_vsync_config)
                _vk_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            else if (vk_present_mode_immediate_supported)
                _vk_surface_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            else
            {
                jeecs::debug::logfatal("Failed to create vk130 swapchain, unsupported present mode.");
            }

            _vk_presenting_swapchain_image_index = typing::INVALID_UINT32;

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
                 _vk_device_queue_present_family_index
            };
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
            swapchain_create_info.compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapchain_create_info.presentMode = _vk_surface_present_mode;
            swapchain_create_info.clipped = VK_TRUE;

            swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

            if (_vk_swapchain != VK_NULL_HANDLE)
                destroy_swap_chain();

            if (VK_SUCCESS != vkCreateSwapchainKHR(_vk_logic_device, &swapchain_create_info, nullptr, &_vk_swapchain))
            {
                jeecs::debug::logfatal("Failed to create vk130 swapchain.");
            }

            uint32_t swapchain_real_image_count = 0;
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, nullptr);

            assert(swapchain_real_image_count == swapchain_image_count);

            std::vector<VkImage> swapchain_images(swapchain_real_image_count);
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, swapchain_images.data());

            _vk_swapchain_framebuffer.resize(swapchain_real_image_count);
            for (uint32_t i = 0; i < swapchain_real_image_count; ++i)
            {
                transition_image_layout(
                    swapchain_images[i],
                    _vk_surface_format.format,
                    VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                    VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                jevk11_texture* color_attachment = create_framebuf_texture_with_swapchain_image(
                    swapchain_images[i],
                    _vk_surface_format.format,
                    VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
                );

                jevk11_texture* depth_attachment = create_framebuf_texture(
                    (jegl_texture::format)(jegl_texture::format::DEPTH | jegl_texture::format::FRAMEBUF), w, h);

                _vk_swapchain_textures_for_free.push_back(color_attachment);
                _vk_swapchain_textures_for_free.push_back(depth_attachment);

                _vk_swapchain_framebuffer[i] = create_frame_buffer(
                    (size_t)used_extent.width,
                    (size_t)used_extent.height,
                    { color_attachment },
                    depth_attachment,
                    VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                );
            }
        }

#define VK_API_DECL(name) PFN_##name name
        VK_API_LIST;
#undef VK_API_DECL

#ifndef NDEBUG
        VkDebugUtilsMessengerEXT _vk_debug_manager;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT _0,
            VkDebugUtilsMessageTypeFlagsEXT _1,
            const VkDebugUtilsMessengerCallbackDataEXT* info,
            void* userdata)
        {
            debug::logwarn("[Vulkan] %s", info->pMessage);
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
                        vkGetPhysicalDeviceSurfaceSupportKHR(
                            device, family_index, _vk_surface, &present_support);

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
                vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, nullptr);

                std::vector<VkExtensionProperties> vk_device_extensions(vk_device_extension_count);
                vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, vk_device_extensions.data());

                std::unordered_set<std::string> required_extensions(
                    required_device_extensions.begin(), required_device_extensions.end());

                for (auto& contained_extension : vk_device_extensions) {
                    required_extensions.erase(contained_extension.extensionName);
                }

                if (required_extensions.empty())
                {
                    physics_device_info result;
                    result.queue_graphic_family_index = queue_graphic_family_index;
                    result.queue_present_family_index = queue_present_family_index;

                    jeecs::debug::logwarn("Use the first suitable device: '%s' enumerated by Vulkan, consider to "
                        "choose the most appropriate device, todo.", prop.deviceName);

                    return std::optional(result);
                }
            }
            return std::nullopt;
        }

        void init_vulkan(const jegl_interface_config* config)
        {
            _vk_this_context = this;

            _vk_last_command_buffer_semaphore = VK_NULL_HANDLE;
            _vk_wait_for_last_command_buffer_stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

            _vk_current_target_framebuffer = nullptr;
            _vk_current_command_buffer = nullptr;
            _vk_current_binded_shader = nullptr;
            _vk_command_commit_round = 1;

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
            vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, vk_available_extensions.data());


            bool vk_validation_layer_supported = false;
#ifndef NDEBUG
            if (vk_available_layers.end() == std::find_if(vk_available_layers.begin(), vk_available_layers.end(),
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
            application_info.apiVersion = VK_API_VERSION_1_3;

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

            unsigned int glfw_extension_count = 0;
            const char** glfw_extensions = nullptr;
            glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

            std::vector<const char*> required_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
            if (vk_validation_layer_supported)
                required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            instance_create_info.enabledExtensionCount = (uint32_t)required_extensions.size();
            instance_create_info.ppEnabledExtensionNames = required_extensions.data();

            if (VK_SUCCESS != vkCreateInstance(&instance_create_info, nullptr, &_vk_instance))
            {
                jeecs::debug::logfatal("Failed to create vk130 instance.");
            }

            // 在此初始化vkAPI
#define VK_API_DECL(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(_vk_instance, #name));
            VK_API_LIST;
#undef VK_API_DECL

            for (const char* required_layer : required_layers) {
                if (vk_available_layers.end() == std::find_if(vk_available_layers.begin(), vk_available_layers.end(),
                    [required_layer](const VkLayerProperties& prop)
                    {
                        return strcmp(required_layer, prop.layerName) == 0;
                    }))
                {
                    jeecs::debug::logfatal("Required layer '%s', but current instance is not supported.", required_layer);
                }
            }


            // 创建Surface，并且绑定窗口句柄
#if 0
#   if defined(JE_OS_WINDOWS)
            VkWin32SurfaceCreateInfoKHR surface_create_info = {};
            surface_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = nullptr;
            surface_create_info.flags = 0;
            surface_create_info.hinstance = GetModuleHandle(nullptr);
            surface_create_info.hwnd = (HWND)_vk_jegl_interface->interface_handle();

            assert(vkCreateWin32SurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateWin32SurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                jeecs::debug::logfatal("Failed to create vk130 win32 surface.");
            }
#   elif defined(JE_OS_ANDROID)
            VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
            surface_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = nullptr;
            surface_create_info.flags = 0;
            surface_create_info.window = (ANativeWindow*)_vk_jegl_interface->interface_handle();

            assert(vkCreateAndroidSurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateAndroidSurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                jeecs::debug::logfatal("Failed to create vk130 android surface.");
            }

#   elif defined(JE_OS_LINUX)
            VkXlibSurfaceCreateInfoKHR surface_create_info = {};
            surface_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = nullptr;
            surface_create_info.flags = 0;
            surface_create_info.dpy = (Display*)_vk_jegl_interface->interface_handle();
            surface_create_info.window = (Window)_vk_jegl_interface->interface_handle();

            assert(vkCreateXlibSurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateXlibSurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                jeecs::debug::logfatal("Failed to create vk130 xlib surface.");
            }
#   endif
#else
            if (VK_SUCCESS != glfwCreateWindowSurface(
                _vk_instance, (GLFWwindow*)_vk_jegl_interface->interface_handle(), nullptr, &_vk_surface))
            {
                jeecs::debug::logfatal("Failed to create vk130 glfw surface.");
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
                "VK_EXT_extended_dynamic_state3", // Emmm... Vulkan 真是令人讨厌……
                "VK_EXT_robustness2",
            };

            size_t SKIP_DEVICE = 0;

            _vk_device = nullptr;
            for (auto& device : all_physics_devices)
            {
                if (SKIP_DEVICE)
                {
                    --SKIP_DEVICE;
                    continue;
                }

                auto info = check_physics_device_is_suatable(
                    device,
                    vk_validation_layer_supported,
                    required_device_layers,
                    required_device_extensions
                );

                if (info)
                {
                    _vk_device = device;
                    _vk_device_queue_graphic_family_index = info->queue_graphic_family_index;
                    _vk_device_queue_present_family_index = info->queue_present_family_index;

                    break;
                }
            }

            if (_vk_device == nullptr)
            {
                jeecs::debug::logfatal("Failed to get vk130 device.");
            }

#ifndef NDEBUG
            _vk_debug_manager = VK_NULL_HANDLE;

            if (vk_validation_layer_supported)
            {
                VkDebugUtilsMessengerCreateInfoEXT debug_layer_config = {};
                debug_layer_config.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debug_layer_config.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debug_layer_config.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debug_layer_config.pfnUserCallback = &debug_callback;

                assert(vkCreateDebugUtilsMessengerEXT != nullptr);
                if (VK_SUCCESS != vkCreateDebugUtilsMessengerEXT(_vk_instance, &debug_layer_config, nullptr, &_vk_debug_manager))
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
            std::set<uint32_t> unique_queue_family_indexs = {
                _vk_device_queue_graphic_family_index,
                _vk_device_queue_present_family_index
            };

            float queue_priority = 1.0f;

            for (uint32_t queue_family_index : unique_queue_family_indexs)
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

            VkPhysicalDeviceFeatures device_features = {};

            VkPhysicalDeviceRobustness2FeaturesEXT robustness2_features = {};
            robustness2_features.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
            robustness2_features.pNext = nullptr;
            robustness2_features.robustBufferAccess2 = VK_FALSE;
            robustness2_features.robustImageAccess2 = VK_FALSE;
            robustness2_features.nullDescriptor = VK_TRUE;

            VkDeviceCreateInfo device_create_info = {};
            device_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pNext = &robustness2_features;
            device_create_info.flags = 0;
            device_create_info.queueCreateInfoCount = (uint32_t)queue_create_infos.size();
            device_create_info.pQueueCreateInfos = queue_create_infos.data();

            device_create_info.enabledLayerCount = (uint32_t)required_device_layers.size();
            device_create_info.ppEnabledLayerNames = required_device_layers.data();

            device_create_info.enabledExtensionCount = (uint32_t)required_device_extensions.size();
            device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
            device_create_info.pEnabledFeatures = &device_features;

            if (VK_SUCCESS != vkCreateDevice(_vk_device, &device_create_info, nullptr, &_vk_logic_device))
            {
                jeecs::debug::logfatal("Failed to create vk130 logic device.");
            }

            vkGetDeviceQueue(_vk_logic_device, _vk_device_queue_graphic_family_index, 0, &_vk_logic_device_graphic_queue);
            vkGetDeviceQueue(_vk_logic_device, _vk_device_queue_present_family_index, 0, &_vk_logic_device_present_queue);
            assert(_vk_logic_device_graphic_queue != nullptr);
            assert(_vk_logic_device_present_queue != nullptr);

            _vk_descriptor_set_allocator = new descriptor_set_allocator(this);
            _vk_dear_imgui_descriptor_set_allocator =
                new single_descriptor_set_allocator<
                VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT>(this);
            _vk_command_buffer_allocator = new command_buffer_allocator(this);

            // 为imgui创建描述符集池
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

            VkDescriptorPoolCreateInfo imgui_descriptor_pool_info = {};
            imgui_descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            imgui_descriptor_pool_info.pNext = nullptr;
            imgui_descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            imgui_descriptor_pool_info.maxSets = (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize) * 1000);
            imgui_descriptor_pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize));
            imgui_descriptor_pool_info.pPoolSizes = pool_sizes;

            if (VK_SUCCESS != vkCreateDescriptorPool(
                _vk_logic_device, &imgui_descriptor_pool_info, nullptr, &_vk_dear_imgui_descriptor_pool))
            {
                jeecs::debug::logfatal("Failed to create vk130 imgui descriptor pool.");
            }

            recreate_swap_chain_for_current_surface(
                _vk_jegl_interface->m_interface_width,
                _vk_jegl_interface->m_interface_height);

            _vk_dear_imgui_sampler = create_sampler(
                jegl_shader::fliter_mode::NEAREST,
                jegl_shader::fliter_mode::NEAREST,
                jegl_shader::fliter_mode::NEAREST,
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
            jegui_shutdown_vk130(reboot);

            delete _vk_dear_imgui_descriptor_set_allocator;
            delete _vk_descriptor_set_allocator;
            delete _vk_command_buffer_allocator;

            destroy_swap_chain();

            vkDestroySampler(_vk_logic_device, _vk_dear_imgui_sampler, nullptr);

            vkDestroyDescriptorPool(_vk_logic_device, _vk_dear_imgui_descriptor_pool, nullptr);

            vkDestroyDevice(_vk_logic_device, nullptr);
#ifndef NDEBUG
            if (_vk_debug_manager != VK_NULL_HANDLE)
            {
                assert(vkDestroyDebugUtilsMessengerEXT != nullptr);
                vkDestroyDebugUtilsMessengerEXT(_vk_instance, _vk_debug_manager, nullptr);
            }
#endif
            vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
            vkDestroyInstance(_vk_instance, nullptr);

            _vk_this_context = nullptr;
        }
        /////////////////////////////////////////////////////
        void begin_command_buffer_record(VkCommandBuffer cmdbuf)
        {
            VkCommandBufferBeginInfo command_buffer_begin_info = {};
            command_buffer_begin_info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            command_buffer_begin_info.pNext = nullptr;
            // TODO: 我们的命令缓冲区稍后会设置为执行完毕后重用
            command_buffer_begin_info.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; 
            command_buffer_begin_info.pInheritanceInfo = nullptr;

            if (VK_SUCCESS != vkBeginCommandBuffer(cmdbuf, &command_buffer_begin_info))
            {
                jeecs::debug::logfatal("Failed to begin vk130 command buffer record.");
            }
        }
        void end_command_buffer_record(VkCommandBuffer cmdbuf)
        {
            if (VK_SUCCESS != vkEndCommandBuffer(cmdbuf))
            {
                jeecs::debug::logfatal("Failed to end vk130 command buffer record.");
            }
        }
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

        VkDeviceMemory alloc_vk_device_memory(const VkMemoryRequirements& requirement, VkMemoryPropertyFlags properties)
        {
            VkDeviceMemory result;

            VkMemoryAllocateInfo vertex_buffer_memory_allocate_info = {};
            vertex_buffer_memory_allocate_info.sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            vertex_buffer_memory_allocate_info.pNext = nullptr;
            vertex_buffer_memory_allocate_info.allocationSize = requirement.size;
            vertex_buffer_memory_allocate_info.memoryTypeIndex = find_memory_type(
                requirement.memoryTypeBits, properties);

            if (VK_SUCCESS != vkAllocateMemory(
                _vk_logic_device,
                &vertex_buffer_memory_allocate_info,
                nullptr,
                &result))
            {
                jeecs::debug::logfatal("Failed to allocate vk130 device buffer memory.");
            }

            return result;
        }

        void alloc_vk_device_buffer_memory(
            size_t buffer_size,
            VkBufferUsageFlagBits buffer_usage,
            VkMemoryPropertyFlags properties,
            VkBuffer* out_device_buffer,
            VkDeviceMemory* out_device_memory)
        {
            VkBufferCreateInfo vertex_buffer_create_info = {};
            vertex_buffer_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertex_buffer_create_info.pNext = nullptr;
            vertex_buffer_create_info.flags = 0;
            vertex_buffer_create_info.size = (uint32_t)buffer_size;
            vertex_buffer_create_info.usage = buffer_usage;
            vertex_buffer_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;

            if (VK_SUCCESS != vkCreateBuffer(
                _vk_logic_device,
                &vertex_buffer_create_info,
                nullptr,
                out_device_buffer))
            {
                jeecs::debug::logfatal("Failed to create vk130 device buffer.");
            }

            VkMemoryRequirements vertex_buffer_memory_requirements;
            vkGetBufferMemoryRequirements(
                _vk_logic_device,
                *out_device_buffer,
                &vertex_buffer_memory_requirements);

            *out_device_memory = alloc_vk_device_memory(vertex_buffer_memory_requirements, properties);

            vkBindBufferMemory(_vk_logic_device, *out_device_buffer, *out_device_memory, 0);
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
            sampler_create_info.maxLod = 0.0f;

            VkSampler result;
            if (VK_SUCCESS != vkCreateSampler(
                _vk_logic_device,
                &sampler_create_info,
                nullptr,
                &result))
            {
                jeecs::debug::logfatal("Failed to create vk130 sampler.");
            }
            return result;
        }
        jevk11_shader_blob* create_shader_blob(jegl_resource* resource)
        {
            jevk11_shader_blob::blob_data* shader_blob = new jevk11_shader_blob::blob_data{};
            assert(resource != nullptr
                && resource->m_type == jegl_resource::type::SHADER
                && resource->m_raw_shader_data != nullptr);

            shader_blob->m_context = this;

            // 此处创建采样器
            shader_blob->m_samplers.resize(resource->m_raw_shader_data->m_sampler_count);
            for (size_t i = 0; i < resource->m_raw_shader_data->m_sampler_count; ++i)
            {
                const jegl_shader::sampler_method& method = resource->m_raw_shader_data->m_sampler_methods[i];

                shader_blob->m_samplers[i].m_vk_sampler =
                    create_sampler(
                        method.m_mag,
                        method.m_min,
                        method.m_mip,
                        method.m_uwrap,
                        method.m_vwrap);
                shader_blob->m_samplers[i].m_sampler_id = method.m_sampler_id;
            }

            VkShaderModuleCreateInfo vertex_shader_module_create_info = {};
            vertex_shader_module_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vertex_shader_module_create_info.pNext = nullptr;
            vertex_shader_module_create_info.flags = 0;
            vertex_shader_module_create_info.codeSize =
                resource->m_raw_shader_data->m_vertex_spirv_count * sizeof(jegl_shader::spir_v_code_t);
            vertex_shader_module_create_info.pCode = resource->m_raw_shader_data->m_vertex_spirv_codes;

            if (VK_SUCCESS != vkCreateShaderModule(
                _vk_logic_device,
                &vertex_shader_module_create_info,
                nullptr,
                &shader_blob->m_vertex_shader_module))
            {
                jeecs::debug::logfatal("Failed to create vk130 vertex shader module.");
            }

            VkShaderModuleCreateInfo fragment_shader_module_create_info = {};
            fragment_shader_module_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            fragment_shader_module_create_info.pNext = nullptr;
            fragment_shader_module_create_info.flags = 0;
            fragment_shader_module_create_info.codeSize =
                resource->m_raw_shader_data->m_fragment_spirv_count * sizeof(jegl_shader::spir_v_code_t);
            fragment_shader_module_create_info.pCode = resource->m_raw_shader_data->m_fragment_spirv_codes;

            if (VK_SUCCESS != vkCreateShaderModule(
                _vk_logic_device,
                &fragment_shader_module_create_info,
                nullptr,
                &shader_blob->m_fragment_shader_module))
            {
                jeecs::debug::logfatal("Failed to create vk130 fragment shader module.");
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
                resource->m_raw_shader_data->m_vertex_in_count);

            size_t vertex_point_data_size = 0;
            for (size_t i = 0; i < resource->m_raw_shader_data->m_vertex_in_count; ++i)
            {
                shader_blob->m_vertex_input_attribute_descriptions[i].binding = 0;
                shader_blob->m_vertex_input_attribute_descriptions[i].location = (uint32_t)i;
                shader_blob->m_vertex_input_attribute_descriptions[i].offset = (uint32_t)vertex_point_data_size;
                switch (resource->m_raw_shader_data->m_vertex_in[i].m_type)
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
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 2;
                    break;
                case jegl_shader::uniform_type::FLOAT3:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 3;
                    break;
                case jegl_shader::uniform_type::FLOAT4:
                    shader_blob->m_vertex_input_attribute_descriptions[i].format = VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
                    vertex_point_data_size += sizeof(float) * 4;
                    break;
                }
            }

            shader_blob->m_vertex_input_binding_description = {};
            shader_blob->m_vertex_input_binding_description.binding = 0;
            shader_blob->m_vertex_input_binding_description.stride = (uint32_t)vertex_point_data_size;
            shader_blob->m_vertex_input_binding_description.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

            shader_blob->m_vertex_input_state_create_info = {};
            shader_blob->m_vertex_input_state_create_info.sType = 
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            shader_blob->m_vertex_input_state_create_info.pNext = nullptr;
            shader_blob->m_vertex_input_state_create_info.flags = 0;
            shader_blob->m_vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
            shader_blob->m_vertex_input_state_create_info.pVertexBindingDescriptions = 
                &shader_blob->m_vertex_input_binding_description;
            shader_blob->m_vertex_input_state_create_info.vertexAttributeDescriptionCount = 
                (uint32_t)shader_blob->m_vertex_input_attribute_descriptions.size();
            shader_blob->m_vertex_input_state_create_info.pVertexAttributeDescriptions = 
                shader_blob->m_vertex_input_attribute_descriptions.data();

            // TODO: 根据JoyEngine的绘制设计，此处需要允许动态调整或者创建多个图形管线以匹配不同的绘制需求
            //       这里暂时先挂个最常见的绘制图元模式
            shader_blob->m_input_assembly_state_create_info = {};
            shader_blob->m_input_assembly_state_create_info.sType = 
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            shader_blob->m_input_assembly_state_create_info.pNext = nullptr;
            shader_blob->m_input_assembly_state_create_info.flags = 0;
            shader_blob->m_input_assembly_state_create_info.topology = 
                VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            shader_blob->m_input_assembly_state_create_info.primitiveRestartEnable = VK_TRUE;

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

            shader_blob->m_rasterization_state_create_info = {};
            shader_blob->m_rasterization_state_create_info.sType = 
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            shader_blob->m_rasterization_state_create_info.pNext = nullptr;
            shader_blob->m_rasterization_state_create_info.flags = 0;
            shader_blob->m_rasterization_state_create_info.depthClampEnable = VK_FALSE;
            shader_blob->m_rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
            shader_blob->m_rasterization_state_create_info.polygonMode = 
                VkPolygonMode::VK_POLYGON_MODE_FILL;

            switch (resource->m_raw_shader_data->m_cull_mode)
            {
            case jegl_shader::cull_mode::FRONT:
                shader_blob->m_rasterization_state_create_info.cullMode = VkCullModeFlagBits::VK_CULL_MODE_FRONT_BIT;
                break;
            case jegl_shader::cull_mode::BACK:
                shader_blob->m_rasterization_state_create_info.cullMode = VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;
                break;
            case jegl_shader::cull_mode::NONE:
                shader_blob->m_rasterization_state_create_info.cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
                break;
            }
            shader_blob->m_rasterization_state_create_info.frontFace = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
            shader_blob->m_rasterization_state_create_info.lineWidth = 1;

            // TODO: 配置多重采样，不过JoyEngine的多重采样应该是配置在渲染目标上的，这里暂时不知道怎么处理比较合适
            //      先挂个默认的，并且也应该允许动态调整
            shader_blob->m_multi_sample_state_create_info = {};
            shader_blob->m_multi_sample_state_create_info.sType = 
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            shader_blob->m_multi_sample_state_create_info.pNext = nullptr;
            shader_blob->m_multi_sample_state_create_info.flags = 0;
            shader_blob->m_multi_sample_state_create_info.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            shader_blob->m_multi_sample_state_create_info.sampleShadingEnable = VK_FALSE;
            shader_blob->m_multi_sample_state_create_info.minSampleShading = 1.0f;
            shader_blob->m_multi_sample_state_create_info.pSampleMask = nullptr;
            shader_blob->m_multi_sample_state_create_info.alphaToCoverageEnable = VK_FALSE;
            shader_blob->m_multi_sample_state_create_info.alphaToOneEnable = VK_FALSE;

            // 深度缓冲区配置
            shader_blob->m_depth_stencil_state_create_info = {};
            shader_blob->m_depth_stencil_state_create_info.sType = 
                VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            shader_blob->m_depth_stencil_state_create_info.pNext = nullptr;
            shader_blob->m_depth_stencil_state_create_info.flags = 0;

            switch (resource->m_raw_shader_data->m_depth_mask)
            {
            case jegl_shader::depth_mask_method::DISABLE:
                shader_blob->m_depth_stencil_state_create_info.depthWriteEnable = VK_FALSE;
                break;
            case jegl_shader::depth_mask_method::ENABLE:
                shader_blob->m_depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
                break;
            }

            shader_blob->m_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
            switch (resource->m_raw_shader_data->m_depth_test)
            {
            case jegl_shader::depth_test_method::OFF:
                shader_blob->m_depth_stencil_state_create_info.depthTestEnable = VK_FALSE;
                break;
            case jegl_shader::depth_test_method::NEVER:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
                break;
            case jegl_shader::depth_test_method::LESS:       /* DEFAULT */
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS;
                break;
            case jegl_shader::depth_test_method::EQUAL:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_EQUAL;
                break;
            case jegl_shader::depth_test_method::LESS_EQUAL:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
                break;
            case jegl_shader::depth_test_method::GREATER:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_GREATER;
                break;
            case jegl_shader::depth_test_method::NOT_EQUAL:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_NOT_EQUAL;
                break;
            case jegl_shader::depth_test_method::GREATER_EQUAL:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_GREATER_OR_EQUAL;
                break;
            case jegl_shader::depth_test_method::ALWAYS:
                shader_blob->m_depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
                break;
            }
            shader_blob->m_depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
            shader_blob->m_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
            shader_blob->m_depth_stencil_state_create_info.front = {};
            shader_blob->m_depth_stencil_state_create_info.back = {};
            shader_blob->m_depth_stencil_state_create_info.minDepthBounds = 0;
            shader_blob->m_depth_stencil_state_create_info.maxDepthBounds = 1;

            // 混色方法，这个不需要动态调整，直接从shader配置中读取混合模式
            shader_blob->m_color_blend_attachment_state = {};
            if (resource->m_raw_shader_data->m_blend_src_mode == jegl_shader::blend_method::ONE &&
                resource->m_raw_shader_data->m_blend_dst_mode == jegl_shader::blend_method::ZERO)
            {
                shader_blob->m_color_blend_attachment_state.blendEnable = VK_FALSE;
            }
            else
            {
                shader_blob->m_color_blend_attachment_state.blendEnable = VK_TRUE;
            }

            switch (resource->m_raw_shader_data->m_blend_src_mode)
            {
            case jegl_shader::blend_method::ZERO:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
                break;
            case jegl_shader::blend_method::ONE:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
                break;
            case jegl_shader::blend_method::SRC_COLOR:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_COLOR;
                break;
            case jegl_shader::blend_method::SRC_ALPHA:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
                break;
            case jegl_shader::blend_method::DST_COLOR:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_DST_COLOR;
                break;
            case jegl_shader::blend_method::DST_ALPHA:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_DST_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                break;
            }
            switch (resource->m_raw_shader_data->m_blend_dst_mode)
            {
            case jegl_shader::blend_method::ZERO:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
                break;
            case jegl_shader::blend_method::ONE:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
                break;
            case jegl_shader::blend_method::SRC_COLOR:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_COLOR;
                break;
            case jegl_shader::blend_method::SRC_ALPHA:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
                break;
            case jegl_shader::blend_method::DST_COLOR:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_DST_COLOR;
                break;
            case jegl_shader::blend_method::DST_ALPHA:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_DST_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                break;
            case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                break;
            }

            shader_blob->m_color_blend_attachment_state.srcAlphaBlendFactor =
                shader_blob->m_color_blend_attachment_state.srcColorBlendFactor;
            shader_blob->m_color_blend_attachment_state.dstAlphaBlendFactor =
                shader_blob->m_color_blend_attachment_state.dstColorBlendFactor;

            shader_blob->m_color_blend_attachment_state.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
            shader_blob->m_color_blend_attachment_state.alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD;

            shader_blob->m_color_blend_attachment_state.colorWriteMask =
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT;

            // 此处处理UBO布局，类似DX11，我们的Uniform变量实际上是放在location=0的UBO中的
            // 遍历Uniform，按照vulkan对于ubo的大小和对齐规则，计算实际偏移量；最后得到整个uniform的大小
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
                case  jegl_shader::uniform_type::TEXTURE:
                    break;
                }

                if (unit_size != 0)
                {
                    max_allign = std::max(max_allign, allign_base);

                    last_elem_end_place = jeecs::basic::allign_size(last_elem_end_place, allign_base);
                    shader_blob->m_ulocations[uniforms->m_name] = last_elem_end_place;
                    last_elem_end_place += unit_size;
                }
                uniforms = uniforms->m_next;
            }
            shader_blob->m_uniform_size = jeecs::basic::allign_size(last_elem_end_place, max_allign);

            // 动态配置~！
            // InputAssemblyState 和 Viewport 都应当允许动态配置
            shader_blob->m_dynamic_state_create_info = {};
            shader_blob->m_dynamic_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            shader_blob->m_dynamic_state_create_info.pNext = nullptr;
            shader_blob->m_dynamic_state_create_info.flags = 0;
            shader_blob->m_dynamic_state_create_info.dynamicStateCount = 
                sizeof(jevk11_shader_blob::blob_data::m_dynamic_states) / sizeof(VkDynamicState);
            shader_blob->m_dynamic_state_create_info.pDynamicStates = jevk11_shader_blob::blob_data::m_dynamic_states;

            // 创建管道布局
            VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
            pipeline_layout_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.pNext = nullptr;
            pipeline_layout_create_info.flags = 0;
            pipeline_layout_create_info.setLayoutCount = (uint32_t)descriptor_set_allocator::DescriptorSetType::COUNT;
            pipeline_layout_create_info.pSetLayouts = _vk_descriptor_set_allocator->m_vk_global_descriptor_set_layouts;
            pipeline_layout_create_info.pushConstantRangeCount = 0;
            pipeline_layout_create_info.pPushConstantRanges = nullptr;

            if (VK_SUCCESS != vkCreatePipelineLayout(
                _vk_logic_device,
                &pipeline_layout_create_info,
                nullptr,
                &shader_blob->m_pipeline_layout))
            {
                jeecs::debug::logfatal("Failed to create vk130 pipeline layout.");
            }

            auto* result = new jevk11_shader_blob;
            result->m_blob_data = shader_blob;
            return result;
        }
        void destroy_shader_blob(jevk11_shader_blob* blob)
        {
            delete blob;
        }

        jevk11_shader* create_shader_with_blob(jevk11_shader_blob* blob)
        {
            assert(blob != nullptr);

            jevk11_shader* shader = new jevk11_shader{};

            shader->m_blob_data = blob->m_blob_data;
            shader->m_uniform_cpu_buffer_size = shader->m_blob_data->m_uniform_size;

            shader->m_uniform_cpu_buffer_updated = true;
            shader->m_next_allocate_ubos_for_uniform_variable = 0;
            shader->m_command_commit_round = 0;

            if (shader->m_blob_data->m_uniform_size == 0)
                shader->m_uniform_cpu_buffer = nullptr;
            else
                shader->m_uniform_cpu_buffer = malloc(shader->m_uniform_cpu_buffer_size);

            return shader;
        }
        void destroy_shader(jevk11_shader* shader)
        {
            for (auto& [_, pipeline] : shader->m_target_pass_pipelines)
            {
                vkDestroyPipeline(_vk_logic_device, pipeline, nullptr);
            }
            for (auto* ubo : shader->m_uniform_variables)
            {
                destroy_uniform_buffer(ubo);
            }
            if (shader->m_uniform_cpu_buffer != nullptr)
                free(shader->m_uniform_cpu_buffer);

            delete shader;
        }

        jevk11_vertex* create_vertex_instance(jegl_resource* resource)
        {
            jevk11_vertex* vertex = new jevk11_vertex();

            switch (resource->m_raw_vertex_data->m_type)
            {
            case jegl_vertex::type::LINES:
                vertex->m_topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                break;
            case jegl_vertex::type::LINESTRIP:
                vertex->m_topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
                break;
            case jegl_vertex::type::TRIANGLES:
                vertex->m_topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                break;
            case jegl_vertex::type::TRIANGLESTRIP:
                vertex->m_topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                break;
            default:
                jeecs::debug::logfatal("Unsupported vertex type.");
                break;
            }

            size_t vertex_buffer_size = resource->m_raw_vertex_data->m_vertex_length;

            // 获取所需分配的内存类型
            alloc_vk_device_buffer_memory(
                vertex_buffer_size,
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &vertex->m_vk_vertex_buffer,
                &vertex->m_vk_vertex_buffer_memory
            );

            void* vertex_buffer_memory_ptr = nullptr;
            vkMapMemory(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer_memory,
                0,
                vertex_buffer_size,
                0,
                &vertex_buffer_memory_ptr);
            memcpy(
                vertex_buffer_memory_ptr,
                resource->m_raw_vertex_data->m_vertexs,
                vertex_buffer_size);
            vkUnmapMemory(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer_memory);

            // 创建索引缓冲区
            alloc_vk_device_buffer_memory(
                resource->m_raw_vertex_data->m_index_count * sizeof(uint32_t),
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &vertex->m_vk_index_buffer,
                &vertex->m_vk_index_buffer_memory);

            void* index_buffer_memory_ptr = nullptr;
            vkMapMemory(
                _vk_logic_device,
                vertex->m_vk_index_buffer_memory,
                0,
                resource->m_raw_vertex_data->m_index_count * sizeof(uint32_t),
                0,
                &index_buffer_memory_ptr);
            memcpy(
                index_buffer_memory_ptr,
                resource->m_raw_vertex_data->m_indexs,
                resource->m_raw_vertex_data->m_index_count * sizeof(uint32_t));
            vkUnmapMemory(
                _vk_logic_device,
                vertex->m_vk_index_buffer_memory);

            vertex->m_vertex_point_count =
                (uint32_t)resource->m_raw_vertex_data->m_index_count;
            vertex->m_size = (VkDeviceSize)vertex_buffer_size;
            vertex->m_stride = (VkDeviceSize)resource->m_raw_vertex_data->m_data_size_per_point;

            return vertex;
        }
        void destroy_vertex_instance(jevk11_vertex* vertex)
        {
            vkDestroyBuffer(_vk_logic_device, vertex->m_vk_vertex_buffer, nullptr);
            vkFreeMemory(_vk_logic_device, vertex->m_vk_vertex_buffer_memory, nullptr);

            vkDestroyBuffer(_vk_logic_device, vertex->m_vk_index_buffer, nullptr);
            vkFreeMemory(_vk_logic_device, vertex->m_vk_index_buffer_memory, nullptr);

            delete vertex;
        }

        VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
        {
            VkImageViewCreateInfo image_view_create_info = {};
            image_view_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.pNext = nullptr;
            image_view_create_info.flags = 0;
            image_view_create_info.image = image;
            image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
            image_view_create_info.format = format;
            image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.subresourceRange.aspectMask = aspect_flags;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;

            VkImageView result;
            if (VK_SUCCESS != vkCreateImageView(
                _vk_logic_device,
                &image_view_create_info,
                nullptr,
                &result))
            {
                jeecs::debug::logfatal("Failed to create vk130 image view.");
            }

            return result;
        }
        void create_image(
            size_t width, size_t height,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkImageAspectFlags aspect_flags,
            VkImage* out_image,
            VkImageView* out_image_view,
            VkDeviceMemory* out_memory)
        {
            VkImageCreateInfo image_create_info = {};
            image_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_create_info.pNext = nullptr;
            image_create_info.flags = 0;
            image_create_info.imageType = VkImageType::VK_IMAGE_TYPE_2D;
            image_create_info.format = format;
            image_create_info.extent.width = (uint32_t)width;
            image_create_info.extent.height = (uint32_t)height;
            image_create_info.extent.depth = 1;
            image_create_info.mipLevels = 1;
            image_create_info.arrayLayers = 1;
            image_create_info.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            image_create_info.tiling = tiling;
            image_create_info.usage = usage;
            image_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
            image_create_info.queueFamilyIndexCount = 0;
            image_create_info.pQueueFamilyIndices = nullptr;
            image_create_info.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;

            if (VK_SUCCESS != vkCreateImage(
                _vk_logic_device,
                &image_create_info,
                nullptr,
                out_image))
            {
                jeecs::debug::logfatal("Failed to create vk130 image.");
            }

            VkMemoryRequirements image_memory_requirements;
            vkGetImageMemoryRequirements(
                _vk_logic_device,
                *out_image,
                &image_memory_requirements);

            *out_memory = alloc_vk_device_memory(image_memory_requirements, properties);

            vkBindImageMemory(
                _vk_logic_device,
                *out_image,
                *out_memory,
                0);

            *out_image_view = create_image_view(*out_image, format, aspect_flags);
        }
        jevk11_texture* create_framebuf_texture_with_swapchain_image(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
        {
            jevk11_texture* texture = new jevk11_texture{};

            texture->m_vk_texture_image = VK_NULL_HANDLE; // 不设置此项，不需要释放
            texture->m_vk_texture_image_view = create_image_view(image, format, aspect_flags);
            texture->m_vk_texture_image_memory = VK_NULL_HANDLE;
            texture->m_vk_texture_format = format;

            return texture;
        }
        jevk11_texture* create_framebuf_texture(jegl_texture::format format, size_t w, size_t h)
        {
            assert(format & jegl_texture::format::FRAMEBUF);

            jevk11_texture* texture = new jevk11_texture{};

            if (format & jegl_texture::format::DEPTH)
            {
                create_image(
                    w, h,
                    _vk_depth_format,
                    VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                    VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
                    &texture->m_vk_texture_image,
                    &texture->m_vk_texture_image_view,
                    &texture->m_vk_texture_image_memory);
                texture->m_vk_texture_format = _vk_depth_format;

                transition_image_layout(
                    texture->m_vk_texture_image,
                    _vk_depth_format,
                    VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                    VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
            else
            {
                bool is_float16 = 0 != (format & jegl_texture::format::FLOAT16);
                VkFormat vk_attachment_format;
                if (format & jegl_texture::format::RGBA)
                {
                    vk_attachment_format = is_float16
                        ? VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT
                        : VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
                }
                else
                {
                    vk_attachment_format = is_float16
                        ? VkFormat::VK_FORMAT_R16_SFLOAT
                        : VkFormat::VK_FORMAT_R8_UNORM;
                }

                create_image(
                    w, h,
                    vk_attachment_format,
                    VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                    VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                    &texture->m_vk_texture_image,
                    &texture->m_vk_texture_image_view,
                    &texture->m_vk_texture_image_memory);
                texture->m_vk_texture_format = vk_attachment_format;

                transition_image_layout(
                    texture->m_vk_texture_image,
                    vk_attachment_format,
                    VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                    VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            return texture;
        }

        void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
        {
            VkCommandBuffer command_buffer = begin_temp_command_buffer_records();
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = 0;
                barrier.oldLayout = old_layout;
                barrier.newLayout = new_layout;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                {
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
                        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                else
                {
                    barrier.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                }

                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;

                VkPipelineStageFlags source_stage;
                VkPipelineStageFlags destination_stage;

                if (old_layout == VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED &&
                    new_layout == VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;

                    source_stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destination_stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT;
                }
                else if (old_layout == VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                    new_layout == VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                {
                    barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;

                    source_stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT;
                    destination_stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                    && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = 
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

                    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
                    && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                {
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

                    source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
                    && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                {
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
                    && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                {
                    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }
                else
                {
                    jeecs::debug::logfatal("Unsupported layout transition.");
                }

                vkCmdPipelineBarrier(
                    command_buffer,
                    source_stage, destination_stage,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }
            end_temp_command_buffer_record(command_buffer);
        }
        jevk11_texture* create_texture_instance(jegl_resource* resource)
        {
            jegl_texture* texture_raw_data = resource->m_raw_texture_data;
            bool is_cube = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::CUBE);

            if (texture_raw_data->m_format & jegl_texture::format::FRAMEBUF)
            {
                return create_framebuf_texture(
                    texture_raw_data->m_format,
                    texture_raw_data->m_width,
                    texture_raw_data->m_height);
            }
            else
            {
                jevk11_texture* texture = new jevk11_texture{};

                VkFormat image_format;
                switch (texture_raw_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
                {
                case jegl_texture::format::RGBA:
                    image_format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM; break;
                case jegl_texture::format::MONO:
                    image_format = VkFormat::VK_FORMAT_R8_UNORM; break;
                }

                create_image(
                    texture_raw_data->m_width,
                    texture_raw_data->m_height,
                    image_format,
                    VkImageTiling::VK_IMAGE_TILING_OPTIMAL,
                    VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                    &texture->m_vk_texture_image,
                    &texture->m_vk_texture_image_view,
                    &texture->m_vk_texture_image_memory);
                texture->m_vk_texture_format = image_format;

                VkBuffer texture_data_buffer;
                VkDeviceMemory texture_data_buffer_memory;

                size_t texture_buffer_size = texture_raw_data->m_width * texture_raw_data->m_height *
                    (texture_raw_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK);

                alloc_vk_device_buffer_memory(
                    texture_buffer_size,
                    VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &texture_data_buffer,
                    &texture_data_buffer_memory);

                void* data;
                vkMapMemory(
                    _vk_logic_device,
                    texture_data_buffer_memory,
                    0,
                    texture_buffer_size,
                    0,
                    &data);
                memcpy(data, texture_raw_data->m_pixels, texture_buffer_size);
                vkUnmapMemory(_vk_logic_device, texture_data_buffer_memory);

                // 开始传输纹理数据
                transition_image_layout(
                    texture->m_vk_texture_image,
                    image_format,
                    VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                    VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkCommandBuffer command_buffer = begin_temp_command_buffer_records();
                {
                    VkBufferImageCopy region = {};
                    region.bufferOffset = 0;
                    region.bufferRowLength = 0;
                    region.bufferImageHeight = 0;
                    region.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.mipLevel = 0;
                    region.imageSubresource.baseArrayLayer = 0;
                    region.imageSubresource.layerCount = 1;
                    region.imageOffset = { 0, 0, 0 };
                    region.imageExtent = {
                        (uint32_t)texture_raw_data->m_width,
                        (uint32_t)texture_raw_data->m_height,
                        1
                    };

                    vkCmdCopyBufferToImage(
                        command_buffer,
                        texture_data_buffer,
                        texture->m_vk_texture_image,
                        VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &region);
                }
                end_temp_command_buffer_record(command_buffer);
                transition_image_layout(
                    texture->m_vk_texture_image,
                    image_format,
                    VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // 释放临时缓冲区
                vkDestroyBuffer(_vk_logic_device, texture_data_buffer, nullptr);
                vkFreeMemory(_vk_logic_device, texture_data_buffer_memory, nullptr);

                return texture;
            }
        }
        void destroy_texture_instance(jevk11_texture* texture)
        {
            vkDestroyImageView(_vk_logic_device, texture->m_vk_texture_image_view, nullptr);

            if (texture->m_vk_texture_image != VK_NULL_HANDLE)
                vkDestroyImage(_vk_logic_device, texture->m_vk_texture_image, nullptr);
            if (texture->m_vk_texture_image_memory != VK_NULL_HANDLE)
                vkFreeMemory(_vk_logic_device, texture->m_vk_texture_image_memory, nullptr);
            //vkDestroyBuffer(_vk_logic_device, texture->m_vk_texture_buffer, nullptr);
            //vkFreeMemory(_vk_logic_device, texture->m_vk_texture_buffer_memory, nullptr);
            delete texture;
        }

        jevk11_uniformbuf* create_uniform_buffer_with_size(uint32_t real_binding_place, size_t size)
        {
            jevk11_uniformbuf* uniformbuf = new jevk11_uniformbuf{};

            uniformbuf->m_real_binding_place = real_binding_place;

            // 获取所需分配的内存类型
            alloc_vk_device_buffer_memory(
                size,
                VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &uniformbuf->m_uniform_buffer,
                &uniformbuf->m_uniform_buffer_memory
            );

            return uniformbuf;
        }
        jevk11_uniformbuf* create_uniform_buffer(jegl_resource* resource)
        {
            return create_uniform_buffer_with_size(
                (uint32_t)resource->m_raw_uniformbuf_data->m_buffer_binding_place,
                resource->m_raw_uniformbuf_data->m_buffer_size);
        }
        void destroy_uniform_buffer(jevk11_uniformbuf* uniformbuf)
        {
            vkDestroyBuffer(_vk_logic_device, uniformbuf->m_uniform_buffer, nullptr);
            vkFreeMemory(_vk_logic_device, uniformbuf->m_uniform_buffer_memory, nullptr);
            delete uniformbuf;
        }
        void update_uniform_buffer_with_range(jevk11_uniformbuf* ubuf, void* data, size_t offset, size_t size)
        {
            void* mdata;

            vkMapMemory(
                _vk_logic_device,
                ubuf->m_uniform_buffer_memory,
                (VkDeviceSize)offset,
                (VkDeviceSize)size,
                0,
                &mdata);
            memcpy(mdata, data, size);
            vkUnmapMemory(_vk_logic_device, ubuf->m_uniform_buffer_memory);
        }
        void update_uniform_buffer(jegl_resource* resource)
        {
            jevk11_uniformbuf* uniformbuf = std::launder(
                reinterpret_cast<jevk11_uniformbuf*>(resource->m_handle.m_ptr));

            auto* raw_uniformbuf_data = resource->m_raw_uniformbuf_data;
            if (raw_uniformbuf_data != nullptr && raw_uniformbuf_data->m_update_length > 0)
            {
                update_uniform_buffer_with_range(
                    uniformbuf,
                    raw_uniformbuf_data->m_buffer + raw_uniformbuf_data->m_update_begin_offset,
                    raw_uniformbuf_data->m_update_begin_offset,
                    raw_uniformbuf_data->m_buffer_size);

                resource->m_raw_uniformbuf_data->m_update_begin_offset = 0;
                resource->m_raw_uniformbuf_data->m_update_length = 0;
            }
        }

        /////////////////////////////////////////////////////
        void finish_frame_buffer()
        {
            vkCmdEndRenderPass(_vk_current_command_buffer);
            end_command_buffer_record(_vk_current_command_buffer);

            // OK 提交页面
            assert(_vk_last_command_buffer_semaphore != VK_NULL_HANDLE);

            VkSubmitInfo submit_info = {};
            submit_info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            // TODO: 未来应当支持等待多个信号量，例如延迟管线
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &_vk_last_command_buffer_semaphore; 
            submit_info.pWaitDstStageMask = &_vk_wait_for_last_command_buffer_stage;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &_vk_current_command_buffer;

            auto new_semphore = _vk_command_buffer_allocator->allocate_semaphore();

            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &new_semphore;

            if (vkQueueSubmit(
                _vk_logic_device_graphic_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
            {
                jeecs::debug::logfatal("Failed to submit draw command buffer!");
            }

            _vk_last_command_buffer_semaphore = new_semphore;
            if (_vk_current_target_framebuffer == _vk_current_swapchain_framebuffer)
                _vk_wait_for_last_command_buffer_stage =
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            else
                _vk_wait_for_last_command_buffer_stage =
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            _vk_current_target_framebuffer = nullptr;
            _vk_current_command_buffer = nullptr;
        }
        void update()
        {
            if (_vk_presenting_swapchain_image_index != typing::INVALID_UINT32)
            {
                vkQueueWaitIdle(_vk_logic_device_present_queue);

                assert(_vk_current_swapchain_framebuffer ==
                    _vk_swapchain_framebuffer[_vk_presenting_swapchain_image_index]);

                VkPresentInfoKHR present_info = {};
                present_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present_info.pNext = nullptr;
                present_info.waitSemaphoreCount = 1;
                present_info.pWaitSemaphores = &_vk_last_command_buffer_semaphore;

                VkSwapchainKHR swapchains[] = { _vk_swapchain };
                present_info.swapchainCount = 1;
                present_info.pSwapchains = swapchains;
                present_info.pImageIndices = &_vk_presenting_swapchain_image_index;
                present_info.pResults = nullptr;

                vkQueuePresentKHR(_vk_logic_device_present_queue, &present_info);

                _vk_dear_imgui_descriptor_set_allocator->flush_descriptor_set_allocator();
                _vk_descriptor_set_allocator->flush_descriptor_set_allocator();
                _vk_command_buffer_allocator->flush_command_buffer_allocator();

                ++_vk_command_commit_round;
            }

            // 开始录制！
            _vk_last_command_buffer_semaphore = _vk_command_buffer_allocator->allocate_semaphore();
            _vk_wait_for_last_command_buffer_stage = 
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if (VkResult::VK_SUCCESS != vkAcquireNextImageKHR(
                _vk_logic_device,
                _vk_swapchain,
                UINT64_MAX,
                _vk_last_command_buffer_semaphore,
                VK_NULL_HANDLE,
                &_vk_presenting_swapchain_image_index))
            {
                jeecs::debug::logfatal("Failed to acquire swap chain image.");
            }

            _vk_current_swapchain_framebuffer =
                _vk_swapchain_framebuffer[_vk_presenting_swapchain_image_index];

            assert(_vk_current_target_framebuffer == nullptr);
            assert(_vk_current_command_buffer == nullptr);

            cmd_begin_frame_buffer(_vk_current_swapchain_framebuffer, 0, 0, 0, 0);
        }
        void late_update()
        {
            assert(_vk_current_swapchain_framebuffer ==
                _vk_swapchain_framebuffer[_vk_presenting_swapchain_image_index]);


            cmd_begin_frame_buffer(_vk_current_swapchain_framebuffer, 0, 0, 0, 0);

            jegui_update_vk130(_vk_current_command_buffer);

            finish_frame_buffer();
        }
        /////////////////////////////////////////////////////
        void cmd_begin_frame_buffer(jevk11_framebuffer* framebuf, size_t x, size_t y, size_t w, size_t h)
        {
            if (_vk_current_target_framebuffer != nullptr)
                finish_frame_buffer();

            assert(_vk_current_target_framebuffer == nullptr);
            assert(_vk_current_command_buffer == nullptr);

            _vk_current_target_framebuffer = framebuf;
            _vk_current_command_buffer = _vk_command_buffer_allocator->allocate_command_buffer();

            begin_command_buffer_record(_vk_current_command_buffer);

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.pNext = nullptr;
            render_pass_begin_info.renderPass = _vk_current_target_framebuffer->m_rendpass;
            render_pass_begin_info.framebuffer = _vk_current_target_framebuffer->m_framebuffer;
            render_pass_begin_info.renderArea.offset = { 0, 0 };
            render_pass_begin_info.renderArea.extent.width = (uint32_t)framebuf->m_width;
            render_pass_begin_info.renderArea.extent.height = (uint32_t)framebuf->m_height;

            render_pass_begin_info.clearValueCount = 0;
            render_pass_begin_info.pClearValues = nullptr;

            vkCmdBeginRenderPass(
                _vk_current_command_buffer,
                &render_pass_begin_info,
                VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

            // NOTE: Pipeline的viewport的纵向坐标应当翻转以兼容opengl3和dx11实现
            if (w == 0) w = framebuf->m_width;
            if (h == 0) h = framebuf->m_height;

            VkViewport viewport = {};
            viewport.x = (float)x;
            viewport.y = (float)framebuf->m_height - (float)y;
            viewport.width = (float)w;
            viewport.height = -(float)h;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(_vk_current_command_buffer, 0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.offset = { (int32_t)0, (int32_t)0 };
            scissor.extent = { (uint32_t)framebuf->m_width, (uint32_t)framebuf->m_height };
            vkCmdSetScissor(_vk_current_command_buffer, 0, 1, &scissor);

            vkCmdSetPrimitiveRestartEnable(_vk_current_command_buffer, VK_TRUE);
        }
        void cmd_clear_frame_buffer_color(float color[4])
        {
            assert(_vk_current_target_framebuffer != nullptr);

            for (size_t i = 0; i < _vk_current_target_framebuffer->m_color_attachments.size(); ++i)
            {
                VkClearValue clear_color = { color[0], color[1], color[2], color[3] };
                VkClearAttachment clear_attachment = {};
                clear_attachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                clear_attachment.colorAttachment = (uint32_t)i;
                clear_attachment.clearValue = clear_color;

                VkClearRect clear_rect = {};
                clear_rect.baseArrayLayer = 0;
                clear_rect.layerCount = 1;
                clear_rect.rect.offset = { 0, 0 };
                clear_rect.rect.extent = {
                    (uint32_t)_vk_current_target_framebuffer->m_width,
                    (uint32_t)_vk_current_target_framebuffer->m_height
                };

                vkCmdClearAttachments(
                    _vk_current_command_buffer,
                    1,
                    &clear_attachment,
                    1,
                    &clear_rect);
            }
        }
        void cmd_clear_frame_buffer_depth()
        {
            assert(_vk_current_target_framebuffer != nullptr);

            VkClearAttachment clear_attachment = {};
            clear_attachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            clear_attachment.colorAttachment = 0;
            clear_attachment.clearValue.depthStencil.depth = 1.0f;
            clear_attachment.clearValue.depthStencil.stencil = 0;

            VkClearRect clear_rect = {};
            clear_rect.baseArrayLayer = 0;
            clear_rect.layerCount = 1;
            clear_rect.rect.offset = { 0, 0 };
            clear_rect.rect.extent = {
                (uint32_t)_vk_current_target_framebuffer->m_width,
                (uint32_t)_vk_current_target_framebuffer->m_height
            };

            vkCmdClearAttachments(
                _vk_current_command_buffer,
                1,
                &clear_attachment,
                1,
                &clear_rect);
        }
        void cmd_bind_shader_pipeline(jevk11_shader* shader)
        {
            assert(_vk_current_target_framebuffer != nullptr);

            _vk_current_binded_shader = shader;

            for (auto& sampler : shader->m_blob_data->m_samplers)
            {
                _vk_descriptor_set_allocator->bind_sampler(
                    (size_t)sampler.m_sampler_id, sampler.m_vk_sampler);
            }

            vkCmdBindPipeline(
                _vk_current_command_buffer,
                VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                shader->prepare_pipeline(this));

            _vk_descriptor_set_allocator->bind_pipeline_layout(
                shader->m_blob_data->m_pipeline_layout);
        }
        void cmd_draw_vertex(jevk11_vertex* vertex)
        {
            assert(_vk_current_target_framebuffer != nullptr);
            assert(_vk_current_binded_shader != nullptr);
            VkDeviceSize offsets = 0;

            if (_vk_current_binded_shader->m_uniform_cpu_buffer_size != 0)
            {
                if (_vk_current_binded_shader->m_uniform_cpu_buffer_updated)
                {
                    _vk_current_binded_shader->m_uniform_cpu_buffer_updated = false;

                    auto* new_ubo = _vk_current_binded_shader->allocate_ubo_for_vars(this);

                    update_uniform_buffer_with_range(
                        new_ubo,
                        _vk_current_binded_shader->m_uniform_cpu_buffer,
                        0,
                        _vk_current_binded_shader->m_uniform_cpu_buffer_size);
                }

                _vk_descriptor_set_allocator->bind_uniform_vars(
                    _vk_current_binded_shader->get_last_usable_variable(this)->m_uniform_buffer);
            }

            _vk_descriptor_set_allocator->apply_binding_work();

            vkCmdSetPrimitiveTopology(
                _vk_current_command_buffer,
                vertex->m_topology);

            vkCmdBindVertexBuffers2(
                _vk_current_command_buffer,
                0,
                1,
                &vertex->m_vk_vertex_buffer,
                &offsets,
                &vertex->m_size,
                &vertex->m_stride);

            vkCmdBindIndexBuffer(
                _vk_current_command_buffer,
                vertex->m_vk_index_buffer,
                0,
                VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(
                _vk_current_command_buffer,
                (uint32_t)vertex->m_vertex_point_count,
                1,
                0,
                0,
                0);
        }
        //////////////////////////////////////////////////////////////////////////////////
        void imgui_init()
        {
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = _vk_instance;
            init_info.PhysicalDevice = _vk_device;
            init_info.Device = _vk_logic_device;
            init_info.QueueFamily = _vk_device_queue_graphic_family_index;
            init_info.Queue = _vk_logic_device_graphic_queue;
            init_info.PipelineCache = VK_NULL_HANDLE;
            init_info.DescriptorPool = _vk_dear_imgui_descriptor_pool;
            init_info.Allocator = nullptr;
            init_info.MinImageCount = (uint32_t)_vk_swapchain_framebuffer.size();
            init_info.ImageCount = (uint32_t)_vk_swapchain_framebuffer.size();
            init_info.CheckVkResultFn = nullptr;

            auto cmdbuf = begin_temp_command_buffer_records();

            jegui_init_vk130(
                [](jegl_resource* res)
                {
                    auto vk11_texture = reinterpret_cast<jevk11_texture*>(res->m_handle.m_ptr);
                    auto desc_set = jegl_vk130_context::_vk_this_context
                        ->_vk_dear_imgui_descriptor_set_allocator
                        ->allocate_descriptor_set();

                    // 更新set
                    VkWriteDescriptorSet write_desc_set = {};
                    write_desc_set.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write_desc_set.pNext = nullptr;
                    write_desc_set.dstSet = desc_set;
                    write_desc_set.dstBinding = 0;
                    write_desc_set.dstArrayElement = 0;
                    write_desc_set.descriptorCount = 1;
                    write_desc_set.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                    VkDescriptorImageInfo image_info = {};
                    image_info.sampler = jegl_vk130_context::_vk_this_context->_vk_dear_imgui_sampler;
                    image_info.imageView = vk11_texture->m_vk_texture_image_view;
                    image_info.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    write_desc_set.pImageInfo = &image_info;
                    write_desc_set.pBufferInfo = nullptr;
                    write_desc_set.pTexelBufferView = nullptr;

                    jegl_vk130_context::_vk_this_context->vkUpdateDescriptorSets(
                        jegl_vk130_context::_vk_this_context->_vk_logic_device,
                        1,
                        &write_desc_set,
                        0,
                        nullptr);

                    return (uint64_t)desc_set;
                },
                [](auto* res)
                {
                },
                _vk_jegl_interface->interface_handle(),
                &init_info,
                _vk_swapchain_framebuffer.front()->m_rendpass,
                cmdbuf);

            end_temp_command_buffer_record(cmdbuf);
        }
    };

    jevk11_shader_blob::blob_data::~blob_data()
    {
        m_context->vkDestroyPipelineLayout(m_context->_vk_logic_device, m_pipeline_layout, nullptr);
        m_context->vkDestroyShaderModule(m_context->_vk_logic_device, m_vertex_shader_module, nullptr);
        m_context->vkDestroyShaderModule(m_context->_vk_logic_device, m_fragment_shader_module, nullptr);

        for (auto& sampler : m_samplers)
            m_context->vkDestroySampler(m_context->_vk_logic_device, sampler.m_vk_sampler, nullptr);
    }
    VkPipeline jevk11_shader::prepare_pipeline(jegl_vk130_context* context)
    {
        assert(context->_vk_current_target_framebuffer != nullptr);

        VkRenderPass target_pass = context->_vk_current_target_framebuffer->m_rendpass;
        assert(target_pass != VK_NULL_HANDLE);

        auto fnd = m_target_pass_pipelines.find(target_pass);
        if (fnd != m_target_pass_pipelines.end())
            return fnd->second;

        VkGraphicsPipelineCreateInfo pipeline_create_info = {};
        pipeline_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.pNext = nullptr;
        pipeline_create_info.flags = 0;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = m_blob_data->m_shader_stage_infos;
        pipeline_create_info.pVertexInputState = &m_blob_data->m_vertex_input_state_create_info;
        pipeline_create_info.pInputAssemblyState = &m_blob_data->m_input_assembly_state_create_info;
        pipeline_create_info.pViewportState = &m_blob_data->m_viewport_state_create_info;
        pipeline_create_info.pRasterizationState = &m_blob_data->m_rasterization_state_create_info;
        pipeline_create_info.pMultisampleState = &m_blob_data->m_multi_sample_state_create_info;
        pipeline_create_info.pDepthStencilState = &m_blob_data->m_depth_stencil_state_create_info;

        std::vector<VkPipelineColorBlendAttachmentState> attachment_states(
            context->_vk_current_target_framebuffer->m_color_attachments.size());

        for (auto& state : attachment_states)
            state = m_blob_data->m_color_blend_attachment_state;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.pNext = nullptr;
        color_blend_state_create_info.flags = 0;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VkLogicOp::VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount = (uint32_t)attachment_states.size();
        color_blend_state_create_info.pAttachments = attachment_states.data();
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
        pipeline_create_info.pDynamicState = &m_blob_data->m_dynamic_state_create_info;
        pipeline_create_info.layout = m_blob_data->m_pipeline_layout;
        pipeline_create_info.renderPass = target_pass;
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;  // TODO: 看着我们可以使用子渲染管线？稍后考虑
        pipeline_create_info.basePipelineIndex = -1;

        VkPipeline pipeline;
        if (VK_SUCCESS != context->vkCreateGraphicsPipelines(
            context->_vk_logic_device,
            VK_NULL_HANDLE,
            1,
            &pipeline_create_info,
            nullptr,
            &pipeline))
        {
            jeecs::debug::logfatal("Failed to create vk130 graphics pipeline.");
        }

        m_target_pass_pipelines[target_pass] = pipeline;
        return pipeline;
    }
    jevk11_uniformbuf* jevk11_shader::allocate_ubo_for_vars(jegl_vk130_context* context)
    {
        if (m_next_allocate_ubos_for_uniform_variable >= m_uniform_variables.size())
        {
            auto* ubo = context->create_uniform_buffer_with_size(
                0, m_uniform_cpu_buffer_size);

            m_uniform_variables.push_back(ubo);
            ++m_next_allocate_ubos_for_uniform_variable;
            return ubo;
        }
        return m_uniform_variables[m_next_allocate_ubos_for_uniform_variable++];
    }

    jevk11_uniformbuf* jevk11_shader::get_last_usable_variable(jegl_vk130_context* context)
    {
        if (m_next_allocate_ubos_for_uniform_variable == 0)
        {
            return allocate_ubo_for_vars(context);
        }
        else if (m_command_commit_round != context->_vk_command_commit_round)
        {
            if (m_next_allocate_ubos_for_uniform_variable != 0)
                std::swap(m_uniform_variables[0], m_uniform_variables[m_next_allocate_ubos_for_uniform_variable - 1]);

            m_command_commit_round = context->_vk_command_commit_round;
            m_next_allocate_ubos_for_uniform_variable = 1;
        }
        return m_uniform_variables[m_next_allocate_ubos_for_uniform_variable - 1];
    }

    jegl_context::userdata_t
        startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan130) start!");

        jegl_vk130_context* context = new jegl_vk130_context;

        context->_vk_jegl_interface = new glfw(reboot ? glfw::HOLD : glfw::VULKAN130);
        context->_vk_jegl_interface->create_interface(gthread, config);

        context->init_vulkan(config);

        return context;
    }
    void pre_shutdown(jegl_context*, jegl_context::userdata_t ctx, bool reboot)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->pre_shutdown();
    }
    void shutdown(jegl_context*, jegl_context::userdata_t ctx, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan130) shutdown!");

        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->shutdown(reboot);
        context->_vk_jegl_interface->shutdown(reboot);
        delete context->_vk_jegl_interface;

        delete context;
    }

    jegl_graphic_api::update_action pre_update(jegl_context::userdata_t ctx)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        switch (context->_vk_jegl_interface->update())
        {
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_graphic_api::update_action::STOP;
            /*fallthrough*/
            [[fallthrough]];
        case basic_interface::update_result::PAUSE:
            return jegl_graphic_api::update_action::SKIP;
        case basic_interface::update_result::RESIZE:
            jegui_shutdown_vk130(true);
            context->recreate_swap_chain_for_current_surface(
                context->_vk_jegl_interface->m_interface_width,
                context->_vk_jegl_interface->m_interface_height);
            context->imgui_init();
            /*fallthrough*/
            [[fallthrough]];
        case basic_interface::update_result::NORMAL:
            context->update();
            return jegl_graphic_api::update_action::CONTINUE;
        default:
            abort();
        }
    }
    jegl_graphic_api::update_action commit_update(jegl_context::userdata_t ctx)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->late_update();

        return jegl_graphic_api::update_action::CONTINUE;
    }

    jegl_resource_blob create_resource_blob(jegl_context::userdata_t ctx, jegl_resource* resource)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            return context->create_shader_blob(resource);
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
    void close_resource_blob(jegl_context::userdata_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
        {
            jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

            auto* shader_blob = std::launder(reinterpret_cast<jevk11_shader_blob*>(blob));
            context->destroy_shader_blob(shader_blob);
        }

    }

    void create_resource(jegl_context::userdata_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            assert(blob != nullptr);
            auto* shader_blob = std::launder(reinterpret_cast<jevk11_shader_blob*>(blob));
            resource->m_handle.m_ptr = context->create_shader_with_blob(shader_blob);

            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_m = shader_blob->get_built_in_location("JOYENGINE_TRANS_M");
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_mv = shader_blob->get_built_in_location("JOYENGINE_TRANS_MV");
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_mvp = shader_blob->get_built_in_location("JOYENGINE_TRANS_MVP");

            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_tiling = shader_blob->get_built_in_location("JOYENGINE_TEXTURE_TILING");
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_offset = shader_blob->get_built_in_location("JOYENGINE_TEXTURE_OFFSET");

            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_light2d_resolution =
                shader_blob->get_built_in_location("JOYENGINE_LIGHT2D_RESOLUTION");
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_light2d_decay =
                shader_blob->get_built_in_location("JOYENGINE_LIGHT2D_DECAY");

            // ATTENTION: 注意，以下参数特殊shader可能挪作他用
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_local_scale = shader_blob->get_built_in_location("JOYENGINE_LOCAL_SCALE");
            resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_color = shader_blob->get_built_in_location("JOYENGINE_MAIN_COLOR");

            auto* uniforms = resource->m_raw_shader_data->m_custom_uniforms;
            while (uniforms != nullptr)
            {
                uniforms->m_index = shader_blob->get_built_in_location(uniforms->m_name);
                uniforms = uniforms->m_next;
            }

            break;
        }
        case jegl_resource::type::TEXTURE:
            resource->m_handle.m_ptr = context->create_texture_instance(resource);
            break;
        case jegl_resource::type::VERTEX:
            resource->m_handle.m_ptr = context->create_vertex_instance(resource);
            break;
        case jegl_resource::type::FRAMEBUF:
        {
            std::vector<jevk11_texture*> color_attachments;
            jevk11_texture* depth_attachment = nullptr;

            jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture>*>(
                    resource->m_raw_framebuf_data->m_output_attachments));

            for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
            {
                auto& attachment = attachments[i];
                jegl_using_resource(attachment->resouce());

                auto* attach_texture_instance = std::launder(reinterpret_cast<jevk11_texture*>(attachment->resouce()->m_handle.m_ptr));
                if (0 != (attachment->resouce()->m_raw_texture_data->m_format & jegl_texture::format::DEPTH))
                    depth_attachment = attach_texture_instance;
                else
                    color_attachments.push_back(attach_texture_instance);
            }

            resource->m_handle.m_ptr = context->create_frame_buffer(
                resource->m_raw_framebuf_data->m_width,
                resource->m_raw_framebuf_data->m_height,
                color_attachments,
                depth_attachment,
                VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            break;
        }
        case jegl_resource::type::UNIFORMBUF:
            resource->m_handle.m_ptr = context->create_uniform_buffer(resource);
            break;
        default:
            break;
        }
    }
    void close_resource(jegl_context::userdata_t ctx, jegl_resource* resource);
    void using_resource(jegl_context::userdata_t ctx, jegl_resource* resource)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            break;
        case jegl_resource::type::TEXTURE:
        {
            if (resource->m_raw_texture_data != nullptr)
            {
                if (resource->m_raw_texture_data->m_modified)
                {
                    resource->m_raw_texture_data->m_modified = false;
                    // Modified, free current resource id, reload one.

                    close_resource(ctx, resource);
                    resource->m_handle.m_ptr = nullptr;
                    create_resource(ctx, nullptr, resource);
                }
            }
            break;
        }
        case jegl_resource::type::VERTEX:
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
            context->update_uniform_buffer(resource);
            break;
        default:
            break;
        }
    }
    void close_resource(jegl_context::userdata_t ctx, jegl_resource* resource)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->vkDeviceWaitIdle(context->_vk_logic_device);

        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            context->destroy_shader(std::launder(reinterpret_cast<jevk11_shader*>(resource->m_handle.m_ptr)));
            break;
        case jegl_resource::type::TEXTURE:
            context->destroy_texture_instance(std::launder(reinterpret_cast<jevk11_texture*>(resource->m_handle.m_ptr)));
            break;
        case jegl_resource::type::VERTEX:
            context->destroy_vertex_instance(std::launder(reinterpret_cast<jevk11_vertex*>(resource->m_handle.m_ptr)));
            break;
        case jegl_resource::type::FRAMEBUF:
            context->destroy_frame_buffer(std::launder(reinterpret_cast<jevk11_framebuffer*>(resource->m_handle.m_ptr)));
            break;
        case jegl_resource::type::UNIFORMBUF:
            context->destroy_uniform_buffer(std::launder(reinterpret_cast<jevk11_uniformbuf*>(resource->m_handle.m_ptr)));
            break;
        default:
            break;
        }
    }

    void draw_vertex_with_shader(jegl_context::userdata_t ctx, jegl_resource* vertex)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->cmd_draw_vertex(std::launder(reinterpret_cast<jevk11_vertex*>(vertex->m_handle.m_ptr)));
    }

    void bind_shader(jegl_context::userdata_t ctx, jegl_resource* shader)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        context->cmd_bind_shader_pipeline(std::launder(reinterpret_cast<jevk11_shader*>(shader->m_handle.m_ptr)));
    }
    void bind_uniform_buffer(jegl_context::userdata_t ctx, jegl_resource* uniformbuf)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        jevk11_uniformbuf* uniformbuf_instance = std::launder(
            reinterpret_cast<jevk11_uniformbuf*>(uniformbuf->m_handle.m_ptr));

        context->_vk_descriptor_set_allocator->bind_uniform_buffer(
            uniformbuf_instance->m_real_binding_place, uniformbuf_instance->m_uniform_buffer);
    }

    void bind_texture(jegl_context::userdata_t ctx, jegl_resource* texture, size_t pass)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        auto* texture_instance = std::launder(reinterpret_cast<jevk11_texture*>(texture->m_handle.m_ptr));
        context->_vk_descriptor_set_allocator->bind_texture(
            (uint32_t)pass,
            texture_instance->m_vk_texture_image_view);
    }

    void set_rend_to_framebuffer(jegl_context::userdata_t ctx, jegl_resource* framebuf, size_t x, size_t y, size_t w, size_t h)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        jevk11_framebuffer* target_framebuf;
        if (framebuf == nullptr)
            target_framebuf = context->_vk_current_swapchain_framebuffer;
        else
        {
            target_framebuf = std::launder(reinterpret_cast<jevk11_framebuffer*>(framebuf->m_handle.m_ptr));
        }
        context->cmd_begin_frame_buffer(target_framebuf, x, y, w, h);
    }
    void clear_framebuffer_color(jegl_context::userdata_t ctx, float color[4])
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        context->cmd_clear_frame_buffer_color(color);
    }
    void clear_framebuffer_depth(jegl_context::userdata_t ctx)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));
        context->cmd_clear_frame_buffer_depth();
    }

    void set_uniform(jegl_context::userdata_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val)
    {
        jegl_vk130_context* context = std::launder(reinterpret_cast<jegl_vk130_context*>(ctx));

        assert(context->_vk_current_binded_shader != nullptr);

        if (location == jeecs::typing::INVALID_UINT32)
            return;

        context->_vk_current_binded_shader->m_uniform_cpu_buffer_updated = true;
        assert(context->_vk_current_binded_shader->m_uniform_cpu_buffer_size != 0);
        assert(context->_vk_current_binded_shader->m_uniform_cpu_buffer != nullptr);

        size_t data_size_byte_length = 0;
        switch (type)
        {
        case jegl_shader::INT:
        case jegl_shader::FLOAT:
            memcpy(reinterpret_cast<void*>((intptr_t)context->_vk_current_binded_shader->m_uniform_cpu_buffer + location), val, 4);
            break;
        case jegl_shader::INT2:
        case jegl_shader::FLOAT2:
            memcpy(reinterpret_cast<void*>((intptr_t)context->_vk_current_binded_shader->m_uniform_cpu_buffer + location), val, 8);
            break;
        case jegl_shader::INT3:
        case jegl_shader::FLOAT3:
            memcpy(reinterpret_cast<void*>((intptr_t)context->_vk_current_binded_shader->m_uniform_cpu_buffer + location), val, 12);
            break;
        case jegl_shader::INT4:
        case jegl_shader::FLOAT4:
            memcpy(reinterpret_cast<void*>((intptr_t)context->_vk_current_binded_shader->m_uniform_cpu_buffer + location), val, 16);
            break;
        case jegl_shader::FLOAT4X4:
            memcpy(reinterpret_cast<void*>((intptr_t)context->_vk_current_binded_shader->m_uniform_cpu_buffer + location), val, 64);
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set."); break;
            break;
        }
    }
}

// 在此处为imgui提供vulkan接口:
VkResult VKAPI_CALL vkWaitForFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence* pFences,
    VkBool32                                    waitAll,
    uint64_t                                    timeout)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

VkResult VKAPI_CALL vkResetFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence* pFences)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkResetFences(device, fenceCount, pFences);
}

void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet* pDescriptorWrites,
    uint32_t                                    descriptorCopyCount,
    const VkCopyDescriptorSet* pDescriptorCopies)
{
    using namespace jeecs::graphic::api::vk130;
    jegl_vk130_context::_vk_this_context->vkUpdateDescriptorSets(
        device,
        descriptorWriteCount,
        pDescriptorWrites,
        descriptorCopyCount,
        pDescriptorCopies);
}

VkResult VKAPI_CALL vkMapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize                                offset,
    VkDeviceSize                                size,
    VkMemoryMapFlags                            flags,
    void** ppData)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkMapMemory(
        device,
        memory,
        offset,
        size,
        flags,
        ppData);
}

void VKAPI_CALL vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory)
{
    using namespace jeecs::graphic::api::vk130;
    jegl_vk130_context::_vk_this_context->vkUnmapMemory(
        device,
        memory);
}

VkResult VKAPI_CALL vkResetCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    VkCommandBufferResetFlags                   flags)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkResetCommandBuffer(
        commandBuffer,
        flags);
}

VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence                                     fence)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkQueueSubmit(
        queue,
        submitCount,
        pSubmits,
        fence);
}

VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue                                     queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkQueuePresentKHR(
        queue,
        pPresentInfo);
}

VkResult VKAPI_CALL vkGetSwapchainImagesKHR(
    VkDevice                                    device,
    VkSwapchainKHR                              swapchain,
    uint32_t* pSwapchainImageCount,
    VkImage* pSwapchainImages)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetSwapchainImagesKHR(
        device,
        swapchain,
        pSwapchainImageCount,
        pSwapchainImages);
}

VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    queueFamilyIndex,
    VkSurfaceKHR                                surface,
    VkBool32* pSupported)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice,
        queueFamilyIndex,
        surface,
        pSupported);
}

VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t* pPresentModeCount,
    VkPresentModeKHR* pPresentModes)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice,
        surface,
        pPresentModeCount,
        pPresentModes);
}

VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t* pSurfaceFormatCount,
    VkSurfaceFormatKHR* pSurfaceFormats)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice,
        surface,
        pSurfaceFormatCount,
        pSurfaceFormats);
}

VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice,
        surface,
        pSurfaceCapabilities);
}

void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetPhysicalDeviceMemoryProperties(
        physicalDevice,
        pMemoryProperties);
}

PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char* pName)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetInstanceProcAddr(
        instance,
        pName);
}

void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements* pMemoryRequirements)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetImageMemoryRequirements(
        device,
        image,
        pMemoryRequirements);
}

void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements* pMemoryRequirements)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkGetBufferMemoryRequirements(
        device,
        buffer,
        pMemoryRequirements);
}

void VKAPI_CALL vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkFreeMemory(
        device,
        memory,
        pAllocator);
}

VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkFreeDescriptorSets(
        device,
        descriptorPool,
        descriptorSetCount,
        pDescriptorSets);
}

void VKAPI_CALL vkFreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer* pCommandBuffers)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkFreeCommandBuffers(
        device,
        commandPool,
        commandBufferCount,
        pCommandBuffers);
}

VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange* pMemoryRanges)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkFlushMappedMemoryRanges(
        device,
        memoryRangeCount,
        pMemoryRanges);
}

VkResult VKAPI_CALL vkEndCommandBuffer(
    VkCommandBuffer                             commandBuffer)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkEndCommandBuffer(
        commandBuffer);
}

VkResult VKAPI_CALL vkDeviceWaitIdle(
    VkDevice                                    device)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDeviceWaitIdle(
        device);
}

void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice                                    device,
    VkSwapchainKHR                              swapchain,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroySwapchainKHR(
        device,
        swapchain,
        pAllocator);
}

void VKAPI_CALL vkDestroySurfaceKHR(
    VkInstance                                  instance,
    VkSurfaceKHR                                surface,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroySurfaceKHR(
        instance,
        surface,
        pAllocator);
}

void VKAPI_CALL vkDestroyShaderModule(
    VkDevice                                    device,
    VkShaderModule                              shaderModule,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyShaderModule(
        device,
        shaderModule,
        pAllocator);
}

VkResult VKAPI_CALL vkResetCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    VkCommandPoolResetFlags                     flags)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkResetCommandPool(
        device,
        commandPool,
        flags);
}

void VKAPI_CALL vkDestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroySemaphore(
        device,
        semaphore,
        pAllocator);
}

void VKAPI_CALL vkDestroySampler(
    VkDevice                                    device,
    VkSampler                                   sampler,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroySampler(
        device,
        sampler,
        pAllocator);
}

void VKAPI_CALL vkDestroyRenderPass(
    VkDevice                                    device,
    VkRenderPass                                renderPass,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyRenderPass(
        device,
        renderPass,
        pAllocator);
}

void VKAPI_CALL vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyPipeline(
        device,
        pipeline,
        pAllocator);
}

void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyPipelineLayout(
        device,
        pipelineLayout,
        pAllocator);
}

void VKAPI_CALL vkDestroyImage(
    VkDevice                                    device,
    VkImage                                     image,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyImage(
        device,
        image,
        pAllocator);
}

void VKAPI_CALL vkDestroyImageView(
    VkDevice                                    device,
    VkImageView                                 imageView,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyImageView(
        device,
        imageView,
        pAllocator);
}

void VKAPI_CALL vkDestroyFramebuffer(
    VkDevice                                    device,
    VkFramebuffer                               framebuffer,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyFramebuffer(
        device,
        framebuffer,
        pAllocator);
}

void VKAPI_CALL vkDestroyFence(
    VkDevice                                    device,
    VkFence                                     fence,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyFence(
        device,
        fence,
        pAllocator);
}

void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice                                    device,
    VkDescriptorSetLayout                       descriptorSetLayout,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyDescriptorSetLayout(
        device,
        descriptorSetLayout,
        pAllocator);
}

void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyDescriptorPool(
        device,
        descriptorPool,
        pAllocator);
}

void VKAPI_CALL vkDestroyBuffer(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyBuffer(
        device,
        buffer,
        pAllocator);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice                                    device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSwapchainKHR* pSwapchain)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateSwapchainKHR(
        device,
        pCreateInfo,
        pAllocator,
        pSwapchain);
}

VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkShaderModule* pShaderModule)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateShaderModule(
        device,
        pCreateInfo,
        pAllocator,
        pShaderModule);
}

VkResult VKAPI_CALL vkCreateSemaphore(
    VkDevice                                    device,
    const VkSemaphoreCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSemaphore* pSemaphore)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateSemaphore(
        device,
        pCreateInfo,
        pAllocator,
        pSemaphore);
}

VkResult VKAPI_CALL vkCreateSampler(
    VkDevice                                    device,
    const VkSamplerCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSampler* pSampler)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateSampler(
        device,
        pCreateInfo,
        pAllocator,
        pSampler);
}

VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice                                    device,
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkRenderPass* pRenderPass)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateRenderPass(
        device,
        pCreateInfo,
        pAllocator,
        pRenderPass);
}

VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice                                    device,
    const VkPipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkPipelineLayout* pPipelineLayout)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreatePipelineLayout(
        device,
        pCreateInfo,
        pAllocator,
        pPipelineLayout);
}

VkResult VKAPI_CALL vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkImageView* pView)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateImageView(
        device,
        pCreateInfo,
        pAllocator,
        pView);
}

VkResult VKAPI_CALL vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkImage* pImage)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateImage(
        device,
        pCreateInfo,
        pAllocator,
        pImage);
}

VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateGraphicsPipelines(
        device,
        pipelineCache,
        createInfoCount,
        pCreateInfos,
        pAllocator,
        pPipelines);
}

VkResult VKAPI_CALL vkCreateFramebuffer(
    VkDevice                                    device,
    const VkFramebufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkFramebuffer* pFramebuffer)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateFramebuffer(
        device,
        pCreateInfo,
        pAllocator,
        pFramebuffer);
}

VkResult VKAPI_CALL vkCreateFence(
    VkDevice                                    device,
    const VkFenceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkFence* pFence)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateFence(
        device,
        pCreateInfo,
        pAllocator,
        pFence);
}

VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDescriptorSetLayout* pSetLayout)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateDescriptorSetLayout(
        device,
        pCreateInfo,
        pAllocator,
        pSetLayout);
}

VkResult VKAPI_CALL vkCreateCommandPool(
    VkDevice                                    device,
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkCommandPool* pCommandPool)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateCommandPool(
        device,
        pCreateInfo,
        pAllocator,
        pCommandPool);
}

VkResult VKAPI_CALL vkCreateBuffer(
    VkDevice                                    device,
    const VkBufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBuffer* pBuffer)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCreateBuffer(
        device,
        pCreateInfo,
        pAllocator,
        pBuffer);
}

void VKAPI_CALL vkDestroyCommandPool(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    const VkAllocationCallbacks* pAllocator)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkDestroyCommandPool(
        device,
        commandPool,
        pAllocator);
}

void VKAPI_CALL vkCmdSetViewport(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstViewport,
    uint32_t                                    viewportCount,
    const VkViewport* pViewports)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdSetViewport(
        commandBuffer,
        firstViewport,
        viewportCount,
        pViewports);
}

void VKAPI_CALL vkCmdSetScissor(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D* pScissors)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdSetScissor(
        commandBuffer,
        firstScissor,
        scissorCount,
        pScissors);
}

void VKAPI_CALL vkCmdPushConstants(
    VkCommandBuffer                             commandBuffer,
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    offset,
    uint32_t                                    size,
    const void* pValues)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdPushConstants(
        commandBuffer,
        layout,
        stageFlags,
        offset,
        size,
        pValues);
}

void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    VkDependencyFlags                           dependencyFlags,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        dependencyFlags,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             commandBuffer)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdEndRenderPass(
        commandBuffer);
}

void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdDrawIndexed(
        commandBuffer,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);
}

void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkBufferImageCopy* pRegions)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdCopyBufferToImage(
        commandBuffer,
        srcBuffer,
        dstImage,
        dstImageLayout,
        regionCount,
        pRegions);
}

void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer* pBuffers,
    const VkDeviceSize* pOffsets)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdBindVertexBuffers(
        commandBuffer,
        firstBinding,
        bindingCount,
        pBuffers,
        pOffsets);
}

void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdBindPipeline(
        commandBuffer,
        pipelineBindPoint,
        pipeline);
}

void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdBindIndexBuffer(
        commandBuffer,
        buffer,
        offset,
        indexType);
}

void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t* pDynamicOffsets)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdBindDescriptorSets(
        commandBuffer,
        pipelineBindPoint,
        layout,
        firstSet,
        descriptorSetCount,
        pDescriptorSets,
        dynamicOffsetCount,
        pDynamicOffsets);
}

void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents                           contents)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkCmdBeginRenderPass(
        commandBuffer,
        pRenderPassBegin,
        contents);
}

VkResult VKAPI_CALL vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkBindImageMemory(
        device,
        image,
        memory,
        memoryOffset);
}

VkResult VKAPI_CALL vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkBindBufferMemory(
        device,
        buffer,
        memory,
        memoryOffset);
}

VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkBeginCommandBuffer(
        commandBuffer,
        pBeginInfo);
}

VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice                                    device,
    const VkMemoryAllocateInfo* pAllocateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDeviceMemory* pMemory)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkAllocateMemory(
        device,
        pAllocateInfo,
        pAllocator,
        pMemory);
}

VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice                                    device,
    const VkDescriptorSetAllocateInfo* pAllocateInfo,
    VkDescriptorSet* pDescriptorSets)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkAllocateDescriptorSets(
        device,
        pAllocateInfo,
        pDescriptorSets);
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice                                    device,
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer* pCommandBuffers)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkAllocateCommandBuffers(
        device,
        pAllocateInfo,
        pCommandBuffers);
}

VkResult VKAPI_CALL vkAcquireNextImageKHR(
    VkDevice                                    device,
    VkSwapchainKHR                              swapchain,
    uint64_t                                    timeout,
    VkSemaphore                                 semaphore,
    VkFence                                     fence,
    uint32_t* pImageIndex)
{
    using namespace jeecs::graphic::api::vk130;
    return jegl_vk130_context::_vk_this_context->vkAcquireNextImageKHR(
        device,
        swapchain,
        timeout,
        semaphore,
        fence,
        pImageIndex);
}

// 导出图形接口！ 
void jegl_using_vk130_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::vk130;

    write_to_apis->interface_startup = startup;
    write_to_apis->interface_shutdown_before_resource_release = pre_shutdown;
    write_to_apis->interface_shutdown = shutdown;

    write_to_apis->update_frame_ready = pre_update;
    write_to_apis->update_draw_commit = commit_update;

    write_to_apis->create_resource_blob_cache = create_resource_blob;
    write_to_apis->close_resource_blob_cache = close_resource_blob;

    write_to_apis->create_resource = create_resource;
    write_to_apis->using_resource = using_resource;
    write_to_apis->close_resource = close_resource;

    write_to_apis->bind_uniform_buffer = bind_uniform_buffer;
    write_to_apis->bind_texture = bind_texture;
    write_to_apis->bind_shader = bind_shader;
    write_to_apis->draw_vertex = draw_vertex_with_shader;

    write_to_apis->bind_framebuf = set_rend_to_framebuffer;
    write_to_apis->clear_frame_color = clear_framebuffer_color;
    write_to_apis->clear_frame_depth = clear_framebuffer_depth;

    write_to_apis->set_uniform = set_uniform;
}
#else
void jegl_using_vk130_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("vk130 not available.");
}
#endif