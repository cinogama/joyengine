#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_VK110_GAPI

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

#undef max
#undef min

namespace jeecs::graphic::api::vk110
{
    struct vklibrary_instance_proxy
    {
        void* _instance;

        vklibrary_instance_proxy()
        {
#ifdef JE_OS_WINDOWS
            _instance = wo_load_lib("je/graphiclib/vulkan-1", "vulkan-1.dll", false);
#else
            _instance = wo_load_lib("je/graphiclib/vulkan", "libvulkan.so.1", false);
            if (_instance == nullptr)
                _instance = wo_load_lib("je/graphiclib/vulkan", "libvulkan.so", false);
#endif
            if (_instance == nullptr)
            {
                jeecs::debug::logfatal("Failed to get vulkan library instance.");
            }
        }
        ~vklibrary_instance_proxy()
        {
            assert(_instance != nullptr);
            wo_unload_lib(_instance);
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

#define VK_API_DECL(name) auto name = vk_proxy.api<PFN_##name>(#name);

    VK_API_DECL(vkCreateInstance);
    VK_API_DECL(vkDestroyInstance);

    VK_API_DECL(vkGetInstanceProcAddr);

    VK_API_DECL(vkEnumerateInstanceVersion);
    VK_API_DECL(vkEnumerateInstanceLayerProperties);
    VK_API_DECL(vkEnumerateInstanceExtensionProperties);

#undef VK_API_DECL

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
VK_API_DECL(vkCreateDevice);\
VK_API_DECL(vkDestroyDevice);\
VK_API_DECL(vkGetDeviceQueue);\
\
VK_API_DECL(vkDestroySurfaceKHR);\
\
VK_API_DECL(vkCreateSwapchainKHR);\
VK_API_DECL(vkDestroySwapchainKHR);\
VK_API_DECL(vkGetSwapchainImagesKHR);\
\
VK_API_DECL(vkCreateImageView);\
VK_API_DECL(vkDestroyImageView);\
\
VK_API_DECL(vkGetPhysicalDeviceMemoryProperties);\
VK_API_DECL(vkAllocateMemory);\
VK_API_DECL(vkFreeMemory);\
VK_API_DECL(vkMapMemory);\
VK_API_DECL(vkUnmapMemory);\
VK_API_DECL(vkBindBufferMemory);\
VK_API_DECL(vkGetBufferMemoryRequirements);\
VK_API_DECL(vkCreateBuffer);\
VK_API_DECL(vkDestroyBuffer);\
\
VK_API_DECL(vkCreateShaderModule);\
VK_API_DECL(vkDestroyShaderModule);\
\
VK_API_DECL(vkCreatePipelineLayout);\
VK_API_DECL(vkDestroyPipelineLayout);\
\
VK_API_DECL(vkGetPhysicalDeviceSurfaceSupportKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceFormatsKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfacePresentModesKHR);\
\
VK_API_DECL(vkCreateDebugUtilsMessengerEXT);\
VK_API_DECL(vkDestroyDebugUtilsMessengerEXT);\
\
VK_API_PLATFORM_API_LIST


    struct jevk11_shader_blob
    {
        inline static const VkDynamicState m_dynamic_states[] =
        {
            VkDynamicState::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
            VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT
        };

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
        VkPipelineColorBlendAttachmentState             m_color_blend_attachment_state;
        VkPipelineColorBlendStateCreateInfo             m_color_blend_state_create_info;
        VkPipelineDynamicStateCreateInfo                m_dynamic_state_create_info;

        VkPipelineLayout                                m_pipeline_layout;
    };
    struct jevk11_texture
    {
        VkBuffer m_vk_texture_buffer;
        VkDeviceMemory m_vk_texture_buffer_memory;
    };
    struct jevk11_shader
    {
        VkShaderModule m_vk_shader_module;
    };
    struct jevk111_vertex
    {
        VkBuffer m_vk_vertex_buffer;
        VkDeviceMemory m_vk_vertex_buffer_memory;
    };

    struct jegl_vk110_context
    {
        VkInstance          _vk_instance;
        basic_interface* _vk_jegl_interface;

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

        VkSwapchainKHR              _vk_swapchain;
        std::vector<VkImage>        _vk_swapchain_images;
        std::vector<VkImageView>    _vk_swapchain_image_views;

        void recreate_swap_chain_for_current_surface(size_t w, size_t h)
        {
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

            swapchain_create_info.oldSwapchain = _vk_swapchain;
            // TODO: Old swapchain may need be send to vkDestroySwapchainKHR?
            // Close old swapchain's view
            for (auto& view : _vk_swapchain_image_views)
                vkDestroyImageView(_vk_logic_device, view, nullptr);

            if (VK_SUCCESS != vkCreateSwapchainKHR(_vk_logic_device, &swapchain_create_info, nullptr, &_vk_swapchain))
            {
                jeecs::debug::logfatal("Failed to create vk110 swapchain.");
            }

            uint32_t swapchain_real_image_count = 0;
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, nullptr);

            assert(swapchain_real_image_count == swapchain_image_count);

            _vk_swapchain_images.resize(swapchain_real_image_count);
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, _vk_swapchain_images.data());

            _vk_swapchain_image_views.resize(swapchain_real_image_count);
            for (uint32_t i = 0; i < swapchain_real_image_count; ++i)
            {
                VkImageViewCreateInfo image_view_create_info = {};
                image_view_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                image_view_create_info.pNext = nullptr;
                image_view_create_info.flags = 0;
                image_view_create_info.image = _vk_swapchain_images[i];
                image_view_create_info.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
                image_view_create_info.format = _vk_surface_format.format;
                image_view_create_info.components.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                image_view_create_info.subresourceRange.baseMipLevel = 0;
                image_view_create_info.subresourceRange.levelCount = 1;
                image_view_create_info.subresourceRange.baseArrayLayer = 0;
                image_view_create_info.subresourceRange.layerCount = 1;

                if (VK_SUCCESS != vkCreateImageView(_vk_logic_device, &image_view_create_info, nullptr, &_vk_swapchain_image_views[i]))
                {
                    jeecs::debug::logfatal("Failed to create vk110 swapchain image view.");
                }
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
            jeecs::debug::logerr("[Vulkan] %s", info->pMessage);
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
            assert(_vk_surface != nullptr);

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

                    return std::make_optional(result);
                }
            }
            return std::nullopt;
        }

        void init_vulkan(const jegl_interface_config* config)
        {
            _vk_swapchain = nullptr;

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
            application_info.engineVersion = VK_MAKE_API_VERSION(0, 4, 0, 0);
            application_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

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
                jeecs::debug::logfatal("Failed to create vk110 instance.");
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
            surface_create_info.hwnd = (HWND)_vk_jegl_interface->native_handle();

            assert(vkCreateWin32SurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateWin32SurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 win32 surface.");
            }
#   elif defined(JE_OS_ANDROID)
            VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
            surface_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = nullptr;
            surface_create_info.flags = 0;
            surface_create_info.window = (ANativeWindow*)_vk_jegl_interface->native_handle();

            assert(vkCreateAndroidSurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateAndroidSurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 android surface.");
            }

#   elif defined(JE_OS_LINUX)
            VkXlibSurfaceCreateInfoKHR surface_create_info = {};
            surface_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = nullptr;
            surface_create_info.flags = 0;
            surface_create_info.dpy = (Display*)_vk_jegl_interface->native_handle();
            surface_create_info.window = (Window)_vk_jegl_interface->native_handle();

            assert(vkCreateXlibSurfaceKHR != nullptr);
            if (VK_SUCCESS != vkCreateXlibSurfaceKHR(_vk_instance, &surface_create_info, nullptr, &_vk_surface))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 xlib surface.");
            }
#   endif
#else
            if (VK_SUCCESS != glfwCreateWindowSurface(
                _vk_instance, (GLFWwindow*)_vk_jegl_interface->native_handle(), nullptr, &_vk_surface))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 glfw surface.");
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
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

            _vk_device = nullptr;
            for (auto& device : all_physics_devices)
            {
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
                vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to get vk110 device.");
            }

#ifndef NDEBUG
            _vk_debug_manager = nullptr;

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

            VkDeviceCreateInfo device_create_info = {};
            device_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pNext = nullptr;
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
                vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 logic device.");
            }

            vkGetDeviceQueue(_vk_logic_device, _vk_device_queue_graphic_family_index, 0, &_vk_logic_device_graphic_queue);
            vkGetDeviceQueue(_vk_logic_device, _vk_device_queue_present_family_index, 0, &_vk_logic_device_present_queue);
            assert(_vk_logic_device_graphic_queue != nullptr);
            assert(_vk_logic_device_present_queue != nullptr);

            // 在此处开始创建交换链
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_vk_device, _vk_surface, &_vk_surface_capabilities);

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

            if (vk_present_mode_mailbox_supported)
                _vk_surface_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            else if (vk_present_mode_fifo_supported)
                _vk_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            else if (vk_present_mode_immediate_supported)
                _vk_surface_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            else
            {
                vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
                vkDestroyInstance(_vk_instance, nullptr);
                jeecs::debug::logfatal("Failed to create vk110 swapchain, unsupported present mode.");
            }

            recreate_swap_chain_for_current_surface(
                _vk_jegl_interface->m_interface_width,
                _vk_jegl_interface->m_interface_height);
        }

        void shutdown()
        {
            for (auto& view : _vk_swapchain_image_views)
                vkDestroyImageView(_vk_logic_device, view, nullptr);

            vkDestroySwapchainKHR(_vk_logic_device, _vk_swapchain, nullptr);
            vkDestroyDevice(_vk_logic_device, nullptr);

            if (_vk_debug_manager != nullptr)
            {
                assert(vkDestroyDebugUtilsMessengerEXT != nullptr);
                vkDestroyDebugUtilsMessengerEXT(_vk_instance, _vk_debug_manager, nullptr);
            }
            vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
            vkDestroyInstance(_vk_instance, nullptr);
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
                jeecs::debug::logfatal("Failed to allocate vk110 device buffer memory.");
            }

            return result;
        }

        /////////////////////////////////////////////////////

        jevk11_shader_blob* create_shader_blob(jegl_resource* resource)
        {
            jevk11_shader_blob* shader_blob = new jevk11_shader_blob{};
            assert(resource != nullptr
                && resource->m_type == jegl_resource::type::SHADER
                && resource->m_raw_shader_data != nullptr);

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
                jeecs::debug::logfatal("Failed to create vk110 vertex shader module.");
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
                jeecs::debug::logfatal("Failed to create vk110 fragment shader module.");
            }

            VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
            vertex_shader_stage_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertex_shader_stage_info.pNext = nullptr;
            vertex_shader_stage_info.flags = 0;
            vertex_shader_stage_info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
            vertex_shader_stage_info.module = shader_blob->m_vertex_shader_module;
            vertex_shader_stage_info.pName = "main";

            VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {};
            fragment_shader_stage_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_shader_stage_info.pNext = nullptr;
            fragment_shader_stage_info.flags = 0;
            fragment_shader_stage_info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_shader_stage_info.module = shader_blob->m_fragment_shader_module;
            fragment_shader_stage_info.pName = "main";

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
            shader_blob->m_vertex_input_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            shader_blob->m_vertex_input_state_create_info.pNext = nullptr;
            shader_blob->m_vertex_input_state_create_info.flags = 0;
            shader_blob->m_vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
            shader_blob->m_vertex_input_state_create_info.pVertexBindingDescriptions = &shader_blob->m_vertex_input_binding_description;
            shader_blob->m_vertex_input_state_create_info.vertexAttributeDescriptionCount = (uint32_t)shader_blob->m_vertex_input_attribute_descriptions.size();
            shader_blob->m_vertex_input_state_create_info.pVertexAttributeDescriptions = shader_blob->m_vertex_input_attribute_descriptions.data();

            // TODO: 根据JoyEngine的绘制设计，此处需要允许动态调整或者创建多个图形管线以匹配不同的绘制需求
            //       这里暂时先挂个最常见的绘制图元模式
            shader_blob->m_input_assembly_state_create_info = {};
            shader_blob->m_input_assembly_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            shader_blob->m_input_assembly_state_create_info.pNext = nullptr;
            shader_blob->m_input_assembly_state_create_info.flags = 0;
            shader_blob->m_input_assembly_state_create_info.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            shader_blob->m_input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

            // TODO: 此属性也应当允许自定义，不然玩个p
            shader_blob->m_viewport = {};
            shader_blob->m_viewport.x = 0.0f;
            shader_blob->m_viewport.y = 0.0f;
            shader_blob->m_viewport.width = (float)_vk_surface_capabilities.currentExtent.width;
            shader_blob->m_viewport.height = (float)_vk_surface_capabilities.currentExtent.height;
            shader_blob->m_viewport.minDepth = 0.0f;
            shader_blob->m_viewport.maxDepth = 1.0f;

            shader_blob->m_scissor = {};
            shader_blob->m_scissor.offset = { 0, 0 };
            shader_blob->m_scissor.extent = _vk_surface_capabilities.currentExtent;

            shader_blob->m_viewport_state_create_info = {};
            shader_blob->m_viewport_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            shader_blob->m_viewport_state_create_info.pNext = nullptr;
            shader_blob->m_viewport_state_create_info.flags = 0;
            shader_blob->m_viewport_state_create_info.viewportCount = 1;
            shader_blob->m_viewport_state_create_info.pViewports = &shader_blob->m_viewport;
            shader_blob->m_viewport_state_create_info.scissorCount = 1;
            shader_blob->m_viewport_state_create_info.pScissors = &shader_blob->m_scissor;

            shader_blob->m_rasterization_state_create_info = {};
            shader_blob->m_rasterization_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            shader_blob->m_rasterization_state_create_info.pNext = nullptr;
            shader_blob->m_rasterization_state_create_info.flags = 0;
            shader_blob->m_rasterization_state_create_info.depthClampEnable = VK_FALSE;
            shader_blob->m_rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
            shader_blob->m_rasterization_state_create_info.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL;

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

            // TODO: 配置多重采样，不过JoyEngine的多重采样应该是配置在渲染目标上的，这里暂时不知道怎么处理比较合适
            //      先挂个默认的，并且也应该允许动态调整
            //VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
            //multisample_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            //multisample_state_create_info.pNext = nullptr;
            //multisample_state_create_info.flags = 0;
            //multisample_state_create_info.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            //multisample_state_create_info.sampleShadingEnable = VK_FALSE;
            //multisample_state_create_info.minSampleShading = 1.0f;
            //multisample_state_create_info.pSampleMask = nullptr;
            //multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
            //multisample_state_create_info.alphaToOneEnable = VK_FALSE;

            // TODO: 深度缓冲区配置

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

            shader_blob->m_color_blend_attachment_state.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
            shader_blob->m_color_blend_attachment_state.srcAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
            shader_blob->m_color_blend_attachment_state.dstAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ZERO;
            shader_blob->m_color_blend_attachment_state.alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
            shader_blob->m_color_blend_attachment_state.colorWriteMask = VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT |
                VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT;

            shader_blob->m_color_blend_state_create_info = {};
            shader_blob->m_color_blend_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            shader_blob->m_color_blend_state_create_info.pNext = nullptr;
            shader_blob->m_color_blend_state_create_info.flags = 0;
            shader_blob->m_color_blend_state_create_info.logicOpEnable = VK_FALSE;
            shader_blob->m_color_blend_state_create_info.logicOp = VkLogicOp::VK_LOGIC_OP_COPY;
            shader_blob->m_color_blend_state_create_info.attachmentCount = 1;
            shader_blob->m_color_blend_state_create_info.pAttachments = &shader_blob->m_color_blend_attachment_state;
            shader_blob->m_color_blend_state_create_info.blendConstants[0] = 0.0f;
            shader_blob->m_color_blend_state_create_info.blendConstants[1] = 0.0f;
            shader_blob->m_color_blend_state_create_info.blendConstants[2] = 0.0f;
            shader_blob->m_color_blend_state_create_info.blendConstants[3] = 0.0f;

            // 动态配置~！
            // InputAssemblyState 和 Viewport 都应当允许动态配置
            shader_blob->m_dynamic_state_create_info = {};
            shader_blob->m_dynamic_state_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            shader_blob->m_dynamic_state_create_info.pNext = nullptr;
            shader_blob->m_dynamic_state_create_info.flags = 0;
            shader_blob->m_dynamic_state_create_info.dynamicStateCount = 2;
            shader_blob->m_dynamic_state_create_info.pDynamicStates = jevk11_shader_blob::m_dynamic_states;

            // 创建管道布局
            VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
            pipeline_layout_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_create_info.pNext = nullptr;
            pipeline_layout_create_info.flags = 0;
            pipeline_layout_create_info.setLayoutCount = 0;
            pipeline_layout_create_info.pSetLayouts = nullptr;
            pipeline_layout_create_info.pushConstantRangeCount = 0;
            pipeline_layout_create_info.pPushConstantRanges = nullptr;

            if (VK_SUCCESS != vkCreatePipelineLayout(
                _vk_logic_device,
                &pipeline_layout_create_info,
                nullptr,
                &shader_blob->m_pipeline_layout))
            {
                jeecs::debug::logfatal("Failed to create vk110 pipeline layout.");
            }

            return shader_blob;
        }
        void destroy_shader_blob(jevk11_shader_blob* blob)
        {
            vkDestroyPipelineLayout(_vk_logic_device, blob->m_pipeline_layout, nullptr);
            vkDestroyShaderModule(_vk_logic_device, blob->m_vertex_shader_module, nullptr);
            vkDestroyShaderModule(_vk_logic_device, blob->m_fragment_shader_module, nullptr);

            delete blob;
        }

        jevk11_shader* create_shader_with_blob(jevk11_shader_blob* blob)
        {
            
        }

        jevk111_vertex* create_vertex_instance(jegl_resource* resource)
        {
            jevk111_vertex* vertex = new jevk111_vertex();

            VkBufferCreateInfo vertex_buffer_create_info = {};
            vertex_buffer_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertex_buffer_create_info.pNext = nullptr;
            vertex_buffer_create_info.flags = 0;
            vertex_buffer_create_info.size =
                resource->m_raw_vertex_data->m_point_count *
                resource->m_raw_vertex_data->m_data_count_per_point *
                sizeof(float);
            vertex_buffer_create_info.usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            vertex_buffer_create_info.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;

            if (VK_SUCCESS != vkCreateBuffer(
                _vk_logic_device,
                &vertex_buffer_create_info,
                nullptr,
                &vertex->m_vk_vertex_buffer))
            {
                jeecs::debug::logfatal("Failed to create vk110 vertex buffer.");
            }

            // 获取所需分配的内存类型
            VkMemoryRequirements vertex_buffer_memory_requirements;
            vkGetBufferMemoryRequirements(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer,
                &vertex_buffer_memory_requirements);

            vertex->m_vk_vertex_buffer_memory = alloc_vk_device_memory(
                vertex_buffer_memory_requirements, 
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );

            if (VK_SUCCESS != vkBindBufferMemory(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer,
                vertex->m_vk_vertex_buffer_memory,
                0))
            {
                jeecs::debug::logfatal("Failed to bind vk110 vertex buffer memory.");
            }

            void* vertex_buffer_memory_ptr = nullptr;
            vkMapMemory(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer_memory,
                0,
                vertex_buffer_memory_requirements.size,
                0,
                &vertex_buffer_memory_ptr);
            memcpy(
                vertex_buffer_memory_ptr,
                resource->m_raw_vertex_data->m_vertex_datas,
                vertex_buffer_create_info.size);
            vkUnmapMemory(
                _vk_logic_device,
                vertex->m_vk_vertex_buffer_memory);

            return vertex;
        }
        void destroy_vertex_instance(jevk111_vertex* vertex)
        {
            vkDestroyBuffer(_vk_logic_device, vertex->m_vk_vertex_buffer, nullptr);
            vkFreeMemory(_vk_logic_device, vertex->m_vk_vertex_buffer_memory, nullptr);
            delete vertex;
        }

        jevk11_texture* create_texture_instance(jegl_resource* resource)
        {
            jevk11_texture* texture = new jevk11_texture;

            TODO;

            return texture;
        }
        void destroy_texture_instance(jevk11_texture* texture)
        {
            TODO;
        }
    };

    jegl_thread::custom_thread_data_t
        startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan110) start!");

        jegl_vk110_context* context = new jegl_vk110_context;

        context->_vk_jegl_interface = new glfw(reboot ? glfw::HOLD : glfw::VULKAN110);
        context->_vk_jegl_interface->create_interface(gthread, config);

        context->init_vulkan(config);

        jegui_init_none(
            [](auto* res)->void* {return nullptr; },
            [](auto* res) {});

        return context;
    }
    void shutdown(jegl_thread*, jegl_thread::custom_thread_data_t ctx, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan110) shutdown!");

        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));

        context->shutdown();
        context->_vk_jegl_interface->shutdown(reboot);
        delete context->_vk_jegl_interface;

        delete context;

        jegui_shutdown_none(reboot);
    }

    bool pre_update(jegl_thread::custom_thread_data_t ctx)
    {
        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));

        // context->_vk_jegl_interface->swap();
        return true;
    }
    bool update(jegl_thread::custom_thread_data_t ctx)
    {
        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));

        if (!context->_vk_jegl_interface->update())
        {
            if (jegui_shutdown_callback())
                return false;
        }
        return true;
    }
    bool late_update(jegl_thread::custom_thread_data_t ctx)
    {
        jegui_update_none();
        return true;
    }

    jegl_resource_blob create_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));
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
    void close_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
        {
            jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));

            auto* shader_blob = std::launder(reinterpret_cast<jevk11_shader_blob*>(blob));
            context->destroy_shader_blob(shader_blob);
        }

    }

    void init_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            assert(blob != nullptr);
            auto* shader_blob = std::launder(reinterpret_cast<jevk11_shader_blob*>(blob));
            // OK, 终于要开始创建管线了，嘻嘻嘻嘻！从blob里读取管线配置，开始图形管线！

            break;
        }
        case jegl_resource::type::TEXTURE:
            break;
        case jegl_resource::type::VERTEX:
            resource->m_handle.m_ptr = context->create_vertex_instance(resource);
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
            break;
        default:
            break;
        }
    }
    void using_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
    }
    void close_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        jegl_vk110_context* context = std::launder(reinterpret_cast<jegl_vk110_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            break;
        case jegl_resource::type::TEXTURE:
            break;
        case jegl_resource::type::VERTEX:
        {
            context->destroy_vertex_instance(std::launder(reinterpret_cast<jevk111_vertex*>(resource->m_handle.m_ptr)));
            break;
        }
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
            break;
        default:
            break;
        }
    }

    void draw_vertex_with_shader(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
    }

    void bind_texture(jegl_thread::custom_thread_data_t, jegl_resource*, size_t)
    {
    }

    void set_rend_to_framebuffer(jegl_thread::custom_thread_data_t, jegl_resource*, size_t, size_t, size_t, size_t)
    {
    }
    void clear_framebuffer_color(jegl_thread::custom_thread_data_t, float[4])
    {
    }
    void clear_framebuffer_depth(jegl_thread::custom_thread_data_t)
    {
    }

    void set_uniform(jegl_thread::custom_thread_data_t, uint32_t, jegl_shader::uniform_type, const void*)
    {
    }
}

void jegl_using_vulkan110_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::vk110;

    write_to_apis->init_interface = startup;
    write_to_apis->shutdown_interface = shutdown;

    write_to_apis->pre_update_interface = pre_update;
    write_to_apis->update_interface = update;
    write_to_apis->late_update_interface = late_update;

    write_to_apis->create_resource_blob = create_resource_blob;
    write_to_apis->close_resource_blob = close_resource_blob;

    write_to_apis->init_resource = init_resource;
    write_to_apis->using_resource = using_resource;
    write_to_apis->close_resource = close_resource;

    write_to_apis->draw_vertex = draw_vertex_with_shader;
    write_to_apis->bind_texture = bind_texture;

    write_to_apis->set_rend_buffer = set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer_color = clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = clear_framebuffer_depth;

    write_to_apis->set_uniform = set_uniform;
}
#else
void jegl_using_vulkan110_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("VK110 not available.");
}
#endif