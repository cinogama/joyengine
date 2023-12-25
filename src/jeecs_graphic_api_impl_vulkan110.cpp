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

    VK_API_DECL(vkEnumeratePhysicalDevices);
    VK_API_DECL(vkGetPhysicalDeviceProperties);
    VK_API_DECL(vkEnumerateDeviceExtensionProperties);

    VK_API_DECL(vkGetPhysicalDeviceQueueFamilyProperties);

    struct jegl_vk110_context
    {
        VkInstance _vk_instance;
        basic_interface* _vk_jegl_interface;

        VkPhysicalDevice _vk_device;

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
                for (auto& queue_family : vk_queue_families)
                {
                    if (queue_family.queueCount > 0 && 0 != (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    {
                        is_graphic_device = true;
                        break;
                    }
                }

                if (is_graphic_device)
                {
                    uint32_t vk_device_extension_count = 0;
                    vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, nullptr);

                    std::vector<VkExtensionProperties> vk_device_extensions(vk_device_extension_count);
                    vkEnumerateDeviceExtensionProperties(device, nullptr, &vk_device_extension_count, vk_device_extensions.data());

                    std::unordered_set<std::string> required_extensions = {
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    };

                    for (auto& contained_extension : vk_device_extensions) {
                        required_extensions.erase(contained_extension.extensionName);
                    }

                    if (required_extensions.empty())
                    {
                        _vk_device = device;
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

            if (vk_available_layers.end() == std::find_if(vk_available_layers.begin(), vk_available_layers.end(),
                [](const VkLayerProperties& prop)
                {
                    return strcmp("VK_LAYER_KHRONOS_validation", prop.layerName) == 0;
                }))
            {
                jeecs::debug::logwarn("'VK_LAYER_KHRONOS_validation' not supported, skip.");
            }
            else
            {
                VkDebugUtilsMessengerCreateInfoEXT debug_layer_config = {};
                debug_layer_config.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debug_layer_config.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debug_layer_config.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debug_layer_config.pfnUserCallback = &debug_callback;

                auto _vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
                    vkGetInstanceProcAddr(_vk_instance, "vkCreateDebugUtilsMessengerEXT");

                assert(_vkCreateDebugUtilsMessengerEXT != nullptr);
                if (VK_SUCCESS != _vkCreateDebugUtilsMessengerEXT(_vk_instance, &debug_layer_config, nullptr, &_vk_debug_manager)) {
                    jeecs::debug::logfatal("Failed to set up debug layer callback.");

                    je_clock_sleep_for(1.0);
                    abort();
                }
            }
#endif

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

        if (context->_vk_debug_manager != nullptr)
        {
            auto _vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(context->_vk_instance, "vkDestroyDebugUtilsMessengerEXT");

            _vkDestroyDebugUtilsMessengerEXT(context->_vk_instance, context->_vk_debug_manager, nullptr);
        }

        vkDestroyInstance(context->_vk_instance, nullptr);

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