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

                je_clock_sleep_for(1.0);
                abort();
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
                je_clock_sleep_for(1.0);
                abort();
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
VK_API_PLATFORM_API_LIST;\
VK_API_DECL(vkDestroySurfaceKHR);\
\
VK_API_DECL(vkCreateSwapchainKHR);\
VK_API_DECL(vkDestroySwapchainKHR);\
VK_API_DECL(vkGetSwapchainImagesKHR);\
\
VK_API_DECL(vkGetPhysicalDeviceSurfaceSupportKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfaceFormatsKHR);\
VK_API_DECL(vkGetPhysicalDeviceSurfacePresentModesKHR);\
\
VK_API_DECL(vkCreateDebugUtilsMessengerEXT);\
VK_API_DECL(vkDestroyDebugUtilsMessengerEXT)

    struct jegl_vk110_context
    {
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

        VkSwapchainKHR              _vk_swapchain;
        std::vector<VkImage>        _vk_swapchain_images;

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

            if (VK_SUCCESS != vkCreateSwapchainKHR(_vk_logic_device, &swapchain_create_info, nullptr, &_vk_swapchain))
            {
                jeecs::debug::logfatal("Failed to create vk110 swapchain.");
                je_clock_sleep_for(1.0);
                abort();
            }

            uint32_t swapchain_real_image_count = 0;
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, nullptr);

            assert(swapchain_real_image_count == swapchain_image_count);

            _vk_swapchain_images.resize(swapchain_real_image_count);
            vkGetSwapchainImagesKHR(_vk_logic_device, _vk_swapchain, &swapchain_real_image_count, _vk_swapchain_images.data());
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
                je_clock_sleep_for(1.0);
                abort();
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
                    je_clock_sleep_for(1.0);
                    abort();
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
                je_clock_sleep_for(1.0);
                abort();
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
                je_clock_sleep_for(1.0);
                abort();
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
                je_clock_sleep_for(1.0);
                abort();
            }
#   endif
#else
            if (VK_SUCCESS != glfwCreateWindowSurface(
                _vk_instance, (GLFWwindow*)_vk_jegl_interface->native_handle(), nullptr, &_vk_surface))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 glfw surface.");
                je_clock_sleep_for(1.0);
                abort();
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
                je_clock_sleep_for(1.0);
                abort();
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
                if (VK_SUCCESS != vkCreateDebugUtilsMessengerEXT(_vk_instance, &debug_layer_config, nullptr, &_vk_debug_manager)) {
                    jeecs::debug::logfatal("Failed to set up debug layer callback.");

                    je_clock_sleep_for(1.0);
                    abort();
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
                je_clock_sleep_for(1.0);
                abort();
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
                je_clock_sleep_for(1.0);
                abort();
            }
        }

        void shutdown()
        {
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

    jegl_resource_blob create_resource_blob(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
        return nullptr;
    }
    void close_resource_blob(jegl_thread::custom_thread_data_t, jegl_resource_blob)
    {
    }

    void init_resource(jegl_thread::custom_thread_data_t, jegl_resource_blob, jegl_resource*)
    {
    }
    void using_resource(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
    }
    void close_resource(jegl_thread::custom_thread_data_t, jegl_resource*)
    {
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