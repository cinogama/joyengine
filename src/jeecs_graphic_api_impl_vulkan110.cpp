#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_VK110_GAPI

#include "jeecs_imgui_backend_api.hpp"
#include <vulkan/vulkan.h>

namespace jeecs::graphic::api::vk110
{
    struct vklibrary_instance_proxy
    {
        void* _instance;

        vklibrary_instance_proxy()
        {
#ifdef JE_OS_WINDOWS
            _instance = wo_load_lib("je/graphiclib/vulkan", "vulkan-1.dll", false);
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
    VK_API_DECL(vkDestoryInstance);
    VK_API_DECL(vkEnumerateInstanceVersion);

    VK_API_DECL(vkEnumeratePhysicalDevices);
    VK_API_DECL(vkGetPhysicalDeviceProperties);

    struct jegl_vk110_context
    {
        VkInstance _vk_instance;
    };

    jegl_thread::custom_thread_data_t
        startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
    {
        if (!reboot)
            jeecs::debug::log("Graphic thread (Vulkan110) start!");

        jegl_vk110_context* context = new jegl_vk110_context;

        VkApplicationInfo application_info = {};
        application_info.sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = nullptr;
        application_info.pApplicationName = nullptr;
        application_info.applicationVersion = 0;
        application_info.pEngineName = nullptr;
        application_info.engineVersion = 0;
        application_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

        VkInstanceCreateInfo instance_create_info = {};
        instance_create_info.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pNext = nullptr;
        instance_create_info.flags = 0;
        instance_create_info.pApplicationInfo = &application_info;
        instance_create_info.enabledLayerCount = 0;
        instance_create_info.ppEnabledLayerNames = nullptr;
        instance_create_info.enabledExtensionCount = 0;
        instance_create_info.ppEnabledExtensionNames = nullptr;

        if (VK_SUCCESS != vkCreateInstance(&instance_create_info, nullptr, &context->_vk_instance))
        {
            delete context;

            jeecs::debug::logfatal("Failed to create vk110 instance.");
            je_clock_sleep_for(1.0);
            abort();
        }

        uint32_t enum_deveice_count = 0;
        vkEnumeratePhysicalDevices(context->_vk_instance, &enum_deveice_count, nullptr);

        std::vector<VkPhysicalDevice> all_physics_devices((size_t)enum_deveice_count);
        vkEnumeratePhysicalDevices(context->_vk_instance, &enum_deveice_count, all_physics_devices.data());

        if (enum_deveice_count == 0)
        {
            vkDestoryInstance(context->_vk_instance);
            delete context;

            jeecs::debug::logfatal("Failed to get vk110 device.");
            je_clock_sleep_for(1.0);
            abort();
        }

        for (auto& device : all_physics_devices)
        {
            VkPhysicalDeviceProperties prop = {};
            vkGetPhysicalDeviceProperties(device, &prop);

            jeecs::debug::log("Vulkan device: %s.", prop.deviceName);
        }
        
        // TODO:

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
        vkDestoryInstance(context->_vk_instance);

        delete context;

        jegui_shutdown_none(reboot);
    }

    bool pre_update(jegl_thread::custom_thread_data_t)
    {
        return true;
    }
    bool update(jegl_thread::custom_thread_data_t)
    {
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