#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_VK110_GAPI

#include "jeecs_imgui_backend_api.hpp"
#include <vulkan/vulkan.h>

#ifdef JE_GL_USE_EGL_INSTEAD_GLFW
#   include "jeecs_graphic_api_interface_egl.hpp"
#else
#   include "jeecs_graphic_api_interface_glfw.hpp"
#endif // JE_GL_USE_EGL_INSTEAD_GLFW

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
VK_API_DECL(vkCreateDebugUtilsMessengerEXT);\
VK_API_DECL(vkDestroyDebugUtilsMessengerEXT);

    struct jegl_vk110_context
    {
        VkInstance          _vk_instance;
        basic_interface* _vk_jegl_interface;

        VkPhysicalDevice    _vk_device;
        uint32_t            _vk_device_queue_family_index;
        VkDevice            _vk_logic_device;
        VkQueue             _vk_logic_device_queue;

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

        void init_vulkan(const jegl_interface_config* config)
        {
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

            uint32_t enum_deveice_count = 0;
            vkEnumeratePhysicalDevices(_vk_instance, &enum_deveice_count, nullptr);

            std::vector<VkPhysicalDevice> all_physics_devices((size_t)enum_deveice_count);
            vkEnumeratePhysicalDevices(_vk_instance, &enum_deveice_count, all_physics_devices.data());

            std::vector<const char*> required_device_layers = {
            };
            if (vk_validation_layer_supported)
                required_layers.push_back("VK_LAYER_KHRONOS_validation");

            std::vector<const char*> required_device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

            _vk_device = nullptr;
            for (auto& device : all_physics_devices)
            {
                VkPhysicalDeviceProperties prop = {};
                vkGetPhysicalDeviceProperties(device, &prop);

                uint32_t vk_queue_family_count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device, &vk_queue_family_count, nullptr);

                std::vector<VkQueueFamilyProperties> vk_queue_families(vk_queue_family_count);
                vkGetPhysicalDeviceQueueFamilyProperties(device, &vk_queue_family_count, vk_queue_families.data());

                bool is_graphic_device = false;
                uint32_t queue_family_index = 0;

                for (auto& queue_family : vk_queue_families)
                {
                    if (queue_family.queueCount > 0 && 0 != (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    {
                        is_graphic_device = true;
                        queue_family_index = (uint32_t)(&queue_family - vk_queue_families.data());

                        break;
                    }
                }

                if (is_graphic_device)
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
                        _vk_device = device;
                        _vk_device_queue_family_index = queue_family_index;
                        jeecs::debug::logwarn("Use the first device: '%s' enumerated by Vulkan, consider implementing "
                            "it and choose the most appropriate device, todo.", prop.deviceName);
                    }
                }
                if (_vk_device != nullptr)
                    break;
            }

            if (_vk_device == nullptr)
            {
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

            VkDeviceQueueCreateInfo queue_create_info = {};
            queue_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.pNext = nullptr;
            queue_create_info.flags = 0;
            queue_create_info.queueFamilyIndex = _vk_device_queue_family_index;
            queue_create_info.queueCount = 1;

            float queue_priority = 1.0f;
            queue_create_info.pQueuePriorities = &queue_priority;

            VkPhysicalDeviceFeatures device_features = {};

            VkDeviceCreateInfo device_create_info = {};
            device_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pNext = nullptr;
            device_create_info.flags = 0;
            device_create_info.queueCreateInfoCount = 1;
            device_create_info.pQueueCreateInfos = &queue_create_info;

            device_create_info.enabledLayerCount = (uint32_t)required_device_layers.size();
            device_create_info.ppEnabledLayerNames = required_device_layers.data();

            device_create_info.enabledExtensionCount = (uint32_t)required_device_extensions.size();
            device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
            device_create_info.pEnabledFeatures = &device_features;

            if (VK_SUCCESS != vkCreateDevice(_vk_device, &device_create_info, nullptr, &_vk_logic_device))
            {
                vkDestroyInstance(_vk_instance, nullptr);

                jeecs::debug::logfatal("Failed to create vk110 logic device.");
                je_clock_sleep_for(1.0);
                abort();
            }

            vkGetDeviceQueue(_vk_logic_device, _vk_device_queue_family_index, 0, &_vk_logic_device_queue);
            assert(_vk_logic_device_queue != nullptr);
        }

        void shutdown()
        {
            vkDestroyDevice(_vk_logic_device, nullptr);

            if (_vk_debug_manager != nullptr)
            {
                assert(vkDestroyDebugUtilsMessengerEXT != nullptr);
                vkDestroyDebugUtilsMessengerEXT(_vk_instance, _vk_debug_manager, nullptr);
            }
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