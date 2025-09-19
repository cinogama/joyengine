#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI

#include "jeecs_imgui_backend_api.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#ifndef NDEBUG
#include <dxgidebug.h>
#endif

#include "jeecs_graphic_api_interface_glfw.hpp"

#undef max
#undef min

#define JERCHECK(RC)                                           \
    if (FAILED(RC))                                            \
    {                                                          \
        jeecs::debug::logfatal("JoyEngine DX11 Failed: " #RC); \
    }

namespace jeecs::graphic::api::dx11
{
    struct jedx11_texture;
    struct jedx11_shader;
    struct jedx11_vertex;
    struct jedx11_uniformbuf;
    struct jedx11_framebuffer;

    struct jegl_dx11_context
    {
        JECS_DISABLE_MOVE_AND_COPY(jegl_dx11_context);
        jegl_dx11_context() = default;
        ~jegl_dx11_context() = default;

        graphic::glfw* m_interface;

        template <class T>
        using MSWRLComPtr = Microsoft::WRL::ComPtr<T>;

        // Direct3D 11
        MSWRLComPtr<ID3D11Device> m_dx_device;
        MSWRLComPtr<ID3D11DeviceContext> m_dx_context;
        MSWRLComPtr<IDXGISwapChain> m_dx_swapchain;

        size_t RESOLUTION_WIDTH;
        size_t RESOLUTION_HEIGHT;

        size_t MSAA_LEVEL;
        size_t MSAA_QUALITY;

        size_t FPS;

        HWND WINDOWS_HANDLE;

        MSWRLComPtr<ID3D11DepthStencilView> m_dx_main_renderer_target_depth_view; // 深度模板视图
        MSWRLComPtr<ID3D11Texture2D> m_dx_main_renderer_target_depth_buffer;      // 深度模板缓冲区
        MSWRLComPtr<ID3D11RenderTargetView> m_dx_main_renderer_target_view;       // 渲染目标视图

        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizers[3];
        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizers_r2b[3];

        bool m_dx_context_finished;
        bool m_lock_resolution_for_fullscreen;

        jedx11_shader* m_current_target_shader;
        jedx11_framebuffer* m_current_target_framebuffer;
    };

    struct jedx11_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_texture);
        jedx11_texture() = default;
        ~jedx11_texture() = default;

        bool m_modifiable_texture_buffer;
        jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> m_texture;
        jegl_dx11_context::MSWRLComPtr<ID3D11ShaderResourceView> m_texture_view;
    };
    struct jedx11_modifiable_texture : public jedx11_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_modifiable_texture);
        jedx11_modifiable_texture() = default;
        ~jedx11_modifiable_texture() = default;

        jedx11_texture* m_obsoluted_texture;
    };
    struct jedx11_shader
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_shader);
        jedx11_shader() = default;
        ~jedx11_shader() = default;

        jegl_dx11_context::MSWRLComPtr<ID3D11InputLayout> m_vao; // WTF?
        jegl_dx11_context::MSWRLComPtr<ID3D11VertexShader> m_vertex;
        jegl_dx11_context::MSWRLComPtr<ID3D11PixelShader> m_fragment;

        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_uniforms;
        void* m_uniform_cpu_buffers;
        size_t m_uniform_buffer_size;
        bool m_uniform_updated;

        bool m_draw_for_r2b;
        uint32_t m_ndc_scale_uniform_id;

        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer;
        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer_r2b;
        jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilState> m_depth;
        jegl_dx11_context::MSWRLComPtr<ID3D11BlendState> m_blend;
        struct sampler_structs
        {
            uint32_t m_sampler_id;
            jegl_dx11_context::MSWRLComPtr<ID3D11SamplerState> m_sampler;
        };
        std::vector<sampler_structs> m_samplers;
    };
    struct jedx11_vertex
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_vertex);
        jedx11_vertex() = default;
        ~jedx11_vertex() = default;

        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_vbo;
        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_ebo;
        UINT m_count;
        UINT m_stride;
        D3D_PRIMITIVE_TOPOLOGY m_method;
    };
    struct jedx11_uniformbuf
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_uniformbuf);
        jedx11_uniformbuf() = default;
        ~jedx11_uniformbuf() = default;

        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_uniformbuf;
        UINT m_binding_place;
    };
    struct jedx11_framebuffer
    {
        size_t m_frame_width;
        size_t m_frame_height;

        JECS_DISABLE_MOVE_AND_COPY(jedx11_framebuffer);
        jedx11_framebuffer(size_t w, size_t h)
            : m_frame_width(w)
            , m_frame_height(h)
        {
        }
        ~jedx11_framebuffer() = default;

        std::vector<jegl_dx11_context::MSWRLComPtr<ID3D11RenderTargetView>> m_rend_views;
        jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilView> m_depth_view;

        std::vector<ID3D11RenderTargetView*> m_target_views;
        ID3D11DepthStencilView* m_target_depth_view_may_null;
        UINT m_color_target_count;
    };

    template <typename T>
    void _jedx11_trace_debug(T& ob, const std::string& obname)
    {
        if (ob.Get() != nullptr)
            ob->SetPrivateData(WKPDID_D3DDebugObjectName,
                obname.size(), obname.c_str());
    }

#ifdef NDEBUG
#define JEDX11_TRACE_DEBUG_NAME(OB, NAME) ((void)0)
#else
#define JEDX11_TRACE_DEBUG_NAME(OB, NAME) _jedx11_trace_debug(OB, NAME)
#endif

    void dx11_callback_windows_size_changed(jegl_dx11_context* context)
    {
        if (context->m_dx_context_finished == false)
            return;

        int w, h;
        je_io_get_window_size(&w, &h);

        if (w == 0 || h == 0)
            return;

        context->RESOLUTION_WIDTH = (size_t)w;
        context->RESOLUTION_HEIGHT = (size_t)h;

        // 释放渲染管线输出用到的相关资源
        context->m_dx_main_renderer_target_depth_view.Reset();
        context->m_dx_main_renderer_target_depth_buffer.Reset();
        context->m_dx_main_renderer_target_view.Reset();

        // 重设交换链并且重新创建渲染目标视图
        jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> back_buffer;
        JERCHECK(context->m_dx_swapchain->ResizeBuffers(
            1, (UINT)context->RESOLUTION_WIDTH, (UINT)context->RESOLUTION_HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
        JERCHECK(context->m_dx_swapchain->GetBuffer(
            0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back_buffer.GetAddressOf())));
        JERCHECK(context->m_dx_device->CreateRenderTargetView(
            back_buffer.Get(), nullptr, context->m_dx_main_renderer_target_view.GetAddressOf()));

        // 设置调试对象名
        JEDX11_TRACE_DEBUG_NAME(back_buffer, "JoyEngineDx11TargetBuffer");
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_main_renderer_target_view, "JoyEngineDx11TargetView");
        back_buffer.Reset();

        D3D11_TEXTURE2D_DESC depthStencilDesc;

        depthStencilDesc.Width = (UINT)context->RESOLUTION_WIDTH;
        depthStencilDesc.Height = (UINT)context->RESOLUTION_HEIGHT;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.ArraySize = 1;
        depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 要使用 MSAA? --需要给交换链设置MASS参数
        if (context->MSAA_QUALITY > 0)
        {
            depthStencilDesc.SampleDesc.Count = (UINT)context->MSAA_LEVEL;
            depthStencilDesc.SampleDesc.Quality = (UINT)context->MSAA_QUALITY - 1;
        }
        else
        {
            depthStencilDesc.SampleDesc.Count = 1;
            depthStencilDesc.SampleDesc.Quality = 0;
        }

        depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
        depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthStencilDesc.CPUAccessFlags = 0;
        depthStencilDesc.MiscFlags = 0;

        // 创建深度缓冲区以及深度模板视图
        JERCHECK(context->m_dx_device->CreateTexture2D(
            &depthStencilDesc,
            nullptr,
            context->m_dx_main_renderer_target_depth_buffer.GetAddressOf()));

        JERCHECK(context->m_dx_device->CreateDepthStencilView(
            context->m_dx_main_renderer_target_depth_buffer.Get(),
            nullptr,
            context->m_dx_main_renderer_target_depth_view.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(
            context->m_dx_main_renderer_target_depth_buffer,
            "JoyEngineDx11TargetDepthBuffer");
        JEDX11_TRACE_DEBUG_NAME(
            context->m_dx_main_renderer_target_depth_buffer,
            "JoyEngineDx11TargetDepthBuffer");
        JEDX11_TRACE_DEBUG_NAME(
            context->m_dx_main_renderer_target_depth_view,
            "JoyEngineDx11TargetDepthView");

        // 将渲染目标视图和深度/模板缓冲区结合到管线
        context->m_dx_context->OMSetRenderTargets(1,
            context->m_dx_main_renderer_target_view.GetAddressOf(),
            context->m_dx_main_renderer_target_depth_view.Get());

        D3D11_RASTERIZER_DESC rasterizer_describe;
        rasterizer_describe.FillMode = D3D11_FILL_SOLID;
        rasterizer_describe.DepthBias = 0;
        rasterizer_describe.DepthBiasClamp = 0.0f;
        rasterizer_describe.SlopeScaledDepthBias = 0.0f;
        rasterizer_describe.DepthClipEnable = TRUE;
        rasterizer_describe.ScissorEnable = FALSE;
        rasterizer_describe.MultisampleEnable = FALSE;
        rasterizer_describe.AntialiasedLineEnable = FALSE;

        rasterizer_describe.FrontCounterClockwise = TRUE;

        rasterizer_describe.CullMode = D3D11_CULL_NONE;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers[
                static_cast<size_t>(
                    jegl_shader::cull_mode::NONE)].GetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_FRONT;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers[
                static_cast<size_t>(
                    jegl_shader::cull_mode::FRONT)].GetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_BACK;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers[
                static_cast<size_t>(
                    jegl_shader::cull_mode::BACK)].GetAddressOf()));

        rasterizer_describe.FrontCounterClockwise = FALSE;

        rasterizer_describe.CullMode = D3D11_CULL_NONE;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::NONE)].GetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_FRONT;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::FRONT)].GetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_BACK;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::BACK)].GetAddressOf()));

        // 设置视口变换
        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = (float)w;
        viewport.Height = (float)h;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        context->m_dx_context->RSSetViewports(1, &viewport);
    }

    jegl_context::graphic_impl_context_t dx11_startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_dx11_context* context = new jegl_dx11_context;

        context->m_interface = new glfw(reboot ? glfw::HOLD : glfw::DIRECTX11);
        context->m_interface->create_interface(config);

        context->WINDOWS_HANDLE =
            glfwGetWin32Window(
                reinterpret_cast<GLFWwindow*>(
                    context->m_interface->interface_handle()));

        context->m_current_target_shader = nullptr;
        context->m_current_target_framebuffer = nullptr;
        context->m_dx_context_finished = false;
        context->m_lock_resolution_for_fullscreen = false;
        context->MSAA_LEVEL = config->m_msaa;
        context->FPS = config->m_fps;

        if (!reboot)
        {
            jeecs::debug::log("Graphic thread (DX11) start!");
            // ...
        }

        D3D_DRIVER_TYPE dx_driver_types[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };
        D3D_FEATURE_LEVEL dx_feature_levels[] =
        {
            // D3D_FEATURE_LEVEL_11_1,// Only support dx11
            D3D_FEATURE_LEVEL_11_0 };

        UINT dx_device_flag =
#ifdef NDEBUG
            0;
#else
            D3D11_CREATE_DEVICE_DEBUG;
#endif
        const size_t dx_driver_type_count = ARRAYSIZE(dx_driver_types);
        const size_t dx_feature_level_count = ARRAYSIZE(dx_feature_levels);

        D3D_FEATURE_LEVEL dx_used_feature_level;
        D3D_DRIVER_TYPE dx_used_driver_type;

        HRESULT result;

        for (size_t i = 0; i < dx_driver_type_count; i++)
        {
            dx_used_driver_type = dx_driver_types[i];
            result = D3D11CreateDevice(
                nullptr,
                dx_used_driver_type,
                nullptr,
                dx_device_flag,
                dx_feature_levels,
                dx_feature_level_count,
                D3D11_SDK_VERSION,
                context->m_dx_device.GetAddressOf(),
                &dx_used_feature_level,
                context->m_dx_context.GetAddressOf());

            if (SUCCEEDED(result))
                break;
        }

        if (!SUCCEEDED(result) || dx_used_feature_level != D3D_FEATURE_LEVEL_11_0)
        {
            jeecs::debug::logfatal("Failed to create dx11 device.");
            return nullptr;
        }

        //
        jegl_dx11_context::MSWRLComPtr<IDXGIDevice> dxgiDevice = nullptr;
        jegl_dx11_context::MSWRLComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
        jegl_dx11_context::MSWRLComPtr<IDXGIFactory1> dxgiFactory1 = nullptr; // D3D11.0(包含DXGI1.1)的接口类

        // 为了正确创建 DXGI交换链，首先我们需要获取创建 D3D设备 的 DXGI工厂，否则会引发报错：
        // "IDXGIFactory::CreateSwapChain: This function is being called with a device from a different IDXGIFactory."
        JERCHECK(context->m_dx_device.As(&dxgiDevice));
        JERCHECK(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));
        JERCHECK(dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory1.GetAddressOf())));

        // 填充DXGI_SWAP_CHAIN_DESC用以描述交换链
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferDesc.Width = (UINT)config->m_width;
        sd.BufferDesc.Height = (UINT)config->m_height;
        sd.BufferDesc.RefreshRate.Numerator = (UINT)context->FPS;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        // 是否开启多重采样？
        UINT msaa_quality = 0;
        if (context->MSAA_LEVEL != 0)
        {
            context->m_dx_device->CheckMultisampleQualityLevels(
                DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)context->MSAA_LEVEL, &msaa_quality);
        }

        if (msaa_quality > 0)
        {
            sd.SampleDesc.Count = (UINT)context->MSAA_LEVEL;
            sd.SampleDesc.Quality = msaa_quality - 1;
        }
        else
        {
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
        }
        context->MSAA_LEVEL = (size_t)sd.SampleDesc.Count;
        context->MSAA_QUALITY = (size_t)msaa_quality;

        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.OutputWindow = context->WINDOWS_HANDLE;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = 0;
        JERCHECK(dxgiFactory1->CreateSwapChain(
            context->m_dx_device.Get(), &sd, context->m_dx_swapchain.GetAddressOf()));

        // 可以禁止alt+enter全屏
        dxgiFactory1->MakeWindowAssociation(sd.OutputWindow,
            DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

        // 设置调试对象名
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_context, "JoyEngineDx11Context");
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_swapchain, "JoyEngineDx11SwapChain");

        context->m_dx_context_finished = true;

        dx11_callback_windows_size_changed(context);

        // Fullscreen?
        if (context->m_lock_resolution_for_fullscreen)
            context->m_dx_swapchain->SetFullscreenState(true, nullptr);

        jegui_init_dx11(
            gthread,
            [](jegl_context*, jegl_resource* res)
            {
                auto* resource = reinterpret_cast<jedx11_texture*>(res->m_handle.m_ptr);
                return (uint64_t)resource->m_texture_view.Get();
            },
            [](jegl_context* ctx, jegl_resource* res)
            {
                auto* context = reinterpret_cast<jegl_dx11_context*>(ctx->m_graphic_impl_context);
                auto* shader = reinterpret_cast<jedx11_shader*>(res->m_handle.m_ptr);
                for (auto& sampler : shader->m_samplers)
                {
                    context->m_dx_context->VSSetSamplers(
                        sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
                    context->m_dx_context->PSSetSamplers(
                        sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
                }
            },
            context->m_interface->interface_handle(),
            context->m_dx_device.Get(),
            context->m_dx_context.Get(),
            reboot);

        return context;
    }
    void dx11_pre_shutdown(jegl_context*, jegl_context::graphic_impl_context_t, bool)
    {
    }
    void dx11_shutdown(jegl_context*, jegl_context::graphic_impl_context_t userdata, bool reboot)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(userdata));

        if (!reboot)
            jeecs::debug::log("Graphic thread (DX11) shutdown!");

        jegui_shutdown_dx11(reboot);

        if (context->m_dx_context)
        {
            context->m_dx_context->ClearState();
            context->m_dx_context->Flush(); // 确保所有命令完成
        }

        context->m_interface->shutdown(reboot);
        delete context->m_interface;
        delete context;

#ifndef NDEBUG
        if (!reboot)
        {
            if (auto* debug_module = GetModuleHandle("Dxgidebug.dll"))
            {
                if (auto* get_debug_interface_f =
                    reinterpret_cast<decltype(&DXGIGetDebugInterface)>(
                        GetProcAddress(debug_module, "DXGIGetDebugInterface")))
                {
                    IDXGIDebug* pDxgiDebug;
                    get_debug_interface_f(__uuidof(IDXGIDebug), (void**)&pDxgiDebug);
                    pDxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
                }
            }
        }
#endif
    }

    jegl_update_action dx11_pre_update(jegl_context::graphic_impl_context_t ctx)
    {
        jegl_dx11_context* context =
            std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        context->m_current_target_framebuffer = nullptr;
        context->m_current_target_shader = nullptr;

        switch (context->m_interface->update())
        {
        case basic_interface::update_result::CLOSE:
            if (jegui_shutdown_callback())
                return jegl_update_action::JEGL_UPDATE_STOP;
            goto _label_jegl_dx11_normal_job;
        case basic_interface::update_result::PAUSE:
            return jegl_update_action::JEGL_UPDATE_SKIP;
        case basic_interface::update_result::RESIZE:
            dx11_callback_windows_size_changed(context);
            /*fallthrough*/
            [[fallthrough]];
        case basic_interface::update_result::NORMAL:
        _label_jegl_dx11_normal_job:
            JERCHECK(context->m_dx_swapchain->Present(context->FPS == 0 ? 1 : 0, 0));
            return jegl_update_action::JEGL_UPDATE_CONTINUE;
        default:
            abort();
        }
    }

    jegl_update_action dx11_commit_update(
        jegl_context::graphic_impl_context_t ctx, jegl_update_action)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        // 回到默认帧缓冲区
        context->m_dx_context->OMSetRenderTargets(1,
            context->m_dx_main_renderer_target_view.GetAddressOf(),
            context->m_dx_main_renderer_target_depth_view.Get());
        context->m_current_target_framebuffer = nullptr;

        jegui_update_dx11();
        return jegl_update_action::JEGL_UPDATE_CONTINUE;
    }

    struct dx11_resource_shader_blob
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11InputLayout> m_vao;
        jegl_dx11_context::MSWRLComPtr<ID3D11VertexShader> m_vertex;
        jegl_dx11_context::MSWRLComPtr<ID3D11PixelShader> m_fragment;

        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer;
        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer_r2b;
        jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilState> m_depth;
        jegl_dx11_context::MSWRLComPtr<ID3D11BlendState> m_blend;

        std::vector<jedx11_shader::sampler_structs> m_samplers;
        std::unordered_map<std::string, uint32_t> m_uniform_locations;

        size_t m_uniform_size;

        uint32_t get_built_in_location(const std::string& name) const
        {
            auto fnd = m_uniform_locations.find(name);
            if (fnd != m_uniform_locations.end())
                return fnd->second;

            return jeecs::typing::INVALID_UINT32;
        }
    };

    jegl_resource_blob dx11_create_resource_blob(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            bool shader_load_failed = false;

            dx11_resource_shader_blob* blob = new dx11_resource_shader_blob;
            std::string error_informations;
            ID3DBlob* error_blob = nullptr;

            jegl_dx11_context::MSWRLComPtr<ID3DBlob> vertex_blob;
            jegl_dx11_context::MSWRLComPtr<ID3DBlob> fragment_blob;

            std::string string_path = resource->m_path == nullptr
                ? "__joyengine_builtin_vshader" + std::to_string((intptr_t)resource) + "__"
                : resource->m_path;

            auto compile_result = D3DCompile(
                resource->m_raw_shader_data->m_vertex_hlsl_src,
                strlen(resource->m_raw_shader_data->m_vertex_hlsl_src),
                (string_path + ".vhlsl").c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "vertex_main",
                "vs_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS
#ifdef _DEBUG
                // 设置 D3DCOMPILE_DEBUG 标志用于获取着色器调试信息。该标志可以提升调试体验，
                // 但仍然允许着色器进行优化操作
                | D3DCOMPILE_DEBUG
                // 在Debug环境下禁用优化以避免出现一些不合理的情况
                | D3DCOMPILE_SKIP_OPTIMIZATION
#endif
                ,
                0,
                vertex_blob.GetAddressOf(),
                &error_blob);

            if (FAILED(compile_result))
            {
                shader_load_failed = true;
                error_informations += "In vertex shader: \n";
                if (error_blob != nullptr)
                {
                    error_informations += reinterpret_cast<const char*>(error_blob->GetBufferPointer());
                    error_blob->Release();
                    error_blob = nullptr;
                }
                else
                    error_informations += "Unknown vertex shader failed.";
            }
            else
            {
                UINT layout_begin_offset = 0;

                std::vector<D3D11_INPUT_ELEMENT_DESC> vertex_in_layout(
                    resource->m_raw_shader_data->m_vertex_in_count);

                // VIN
                size_t INT_COUNT = 0;
                size_t FLOAT_COUNT = 0;
                size_t FLOAT2_COUNT = 0;
                size_t FLOAT3_4_COUNT = 0;

                for (size_t i = 0; i < resource->m_raw_shader_data->m_vertex_in_count; ++i)
                {
                    auto& vlayout = vertex_in_layout[i];

                    vlayout.InputSlot = 0;
                    vlayout.AlignedByteOffset = layout_begin_offset;
                    vlayout.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                    vlayout.InstanceDataStepRate = 0;

                    switch (resource->m_raw_shader_data->m_vertex_in[i])
                    {
                    case jegl_shader::uniform_type::INT:
                        vlayout.SemanticIndex = INT_COUNT++;
                        vlayout.SemanticName = "BLENDINDICES";
                        vlayout.Format = DXGI_FORMAT_R32_SINT;
                        layout_begin_offset += 4;
                        break;
                    case jegl_shader::uniform_type::INT2:
                        vlayout.SemanticIndex = INT_COUNT++;
                        vlayout.SemanticName = "BLENDINDICES";
                        vlayout.Format = DXGI_FORMAT_R32G32_SINT;
                        layout_begin_offset += 8;
                        break;
                    case jegl_shader::uniform_type::INT3:
                        vlayout.SemanticIndex = INT_COUNT++;
                        vlayout.SemanticName = "BLENDINDICES";
                        vlayout.Format = DXGI_FORMAT_R32G32B32_SINT;
                        layout_begin_offset += 12;
                        break;
                    case jegl_shader::uniform_type::INT4:
                        vlayout.SemanticIndex = INT_COUNT++;
                        vlayout.SemanticName = "BLENDINDICES";
                        vlayout.Format = DXGI_FORMAT_R32G32B32A32_SINT;
                        layout_begin_offset += 16;
                        break;
                    case jegl_shader::uniform_type::FLOAT:
                        vlayout.SemanticIndex = FLOAT_COUNT++;
                        vlayout.SemanticName = "BLENDWEIGHT";
                        vlayout.Format = DXGI_FORMAT_R32_FLOAT;
                        layout_begin_offset += 4;
                        break;
                    case jegl_shader::uniform_type::FLOAT2:
                        vlayout.SemanticIndex = FLOAT2_COUNT++;
                        vlayout.SemanticName = "TEXCOORD";
                        vlayout.Format = DXGI_FORMAT_R32G32_FLOAT;
                        layout_begin_offset += 8;
                        break;
                    case jegl_shader::uniform_type::FLOAT3:
                    {
                        vlayout.SemanticIndex = FLOAT3_4_COUNT++;
                        if (vlayout.SemanticIndex == 0)
                            vlayout.SemanticName = "POSITION";
                        else if (vlayout.SemanticIndex == 1)
                        {
                            vlayout.SemanticIndex = 0;
                            vlayout.SemanticName = "NORMAL";
                        }
                        else if (vlayout.SemanticIndex == 2)
                        {
                            vlayout.SemanticIndex = 0;
                            vlayout.SemanticName = "TANGENT";
                        }
                        else
                        {
                            vlayout.SemanticIndex -= 3;
                            vlayout.SemanticName = "COLOR";
                        }
                        vlayout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
                        layout_begin_offset += 12;
                        break;
                    }
                    case jegl_shader::uniform_type::FLOAT4:
                    {
                        vlayout.SemanticIndex = FLOAT3_4_COUNT++;
                        if (vlayout.SemanticIndex == 0)
                            vlayout.SemanticName = "POSITION";
                        else if (vlayout.SemanticIndex == 1)
                        {
                            vlayout.SemanticIndex = 0;
                            vlayout.SemanticName = "NORMAL";
                        }
                        else if (vlayout.SemanticIndex == 2)
                        {
                            vlayout.SemanticIndex = 0;
                            vlayout.SemanticName = "TANGENT";
                        }
                        else
                        {
                            vlayout.SemanticIndex -= 3;
                            vlayout.SemanticName = "COLOR";
                        }
                        vlayout.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                        layout_begin_offset += 16;
                        break;
                    }
                    default:
                        abort();
                    }
                }
                JERCHECK(context->m_dx_device->CreateInputLayout(
                    vertex_in_layout.data(),
                    vertex_in_layout.size(),
                    vertex_blob->GetBufferPointer(),
                    vertex_blob->GetBufferSize(),
                    blob->m_vao.GetAddressOf()));
                JEDX11_TRACE_DEBUG_NAME(blob->m_vao, string_path + "_Vao");
            }

            compile_result = D3DCompile(
                resource->m_raw_shader_data->m_fragment_hlsl_src,
                strlen(resource->m_raw_shader_data->m_fragment_hlsl_src),
                (string_path + ".fhlsl").c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "fragment_main",
                "ps_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS
#ifdef _DEBUG
                // 设置 D3DCOMPILE_DEBUG 标志用于获取着色器调试信息。该标志可以提升调试体验，
                // 但仍然允许着色器进行优化操作
                | D3DCOMPILE_DEBUG
                // 在Debug环境下禁用优化以避免出现一些不合理的情况
                | D3DCOMPILE_SKIP_OPTIMIZATION
#endif
                ,
                0,
                fragment_blob.ReleaseAndGetAddressOf(),
                &error_blob);

            if (FAILED(compile_result))
            {
                shader_load_failed = true;
                error_informations += "In fragment shader: \n";
                if (error_blob != nullptr)
                {
                    error_informations += reinterpret_cast<const char*>(error_blob->GetBufferPointer());
                    error_blob->Release();
                    error_blob = nullptr;
                }
                else
                    error_informations += "Unknown fragment shader failed.";
            }

            // Generate uniform locations here.
            if (shader_load_failed)
            {
                delete blob;
                jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                    resource, error_informations.c_str());
                return nullptr;
            }
            else
            {
                JERCHECK(context->m_dx_device->CreateVertexShader(
                    vertex_blob->GetBufferPointer(),
                    vertex_blob->GetBufferSize(),
                    nullptr,
                    blob->m_vertex.GetAddressOf()));

                JERCHECK(context->m_dx_device->CreatePixelShader(
                    fragment_blob->GetBufferPointer(),
                    fragment_blob->GetBufferSize(),
                    nullptr,
                    blob->m_fragment.GetAddressOf()));

                uint32_t last_elem_end_place = 0;
                constexpr size_t DX11_ALLIGN_BASE = 16; // 128bit allign in dx11

                auto* uniforms = resource->m_raw_shader_data->m_custom_uniforms;
                while (uniforms != nullptr)
                {
                    size_t unit_size = 0;
                    switch (uniforms->m_uniform_type)
                    {
                    case jegl_shader::uniform_type::INT:
                    case jegl_shader::uniform_type::FLOAT:
                        unit_size = 4;
                        break;
                    case jegl_shader::uniform_type::INT2:
                    case jegl_shader::uniform_type::FLOAT2:
                        unit_size = 8;
                        break;
                    case jegl_shader::uniform_type::INT3:
                    case jegl_shader::uniform_type::FLOAT3:
                        unit_size = 12;
                        break;
                    case jegl_shader::uniform_type::INT4:
                    case jegl_shader::uniform_type::FLOAT4:
                        unit_size = 16;
                        break;
                    case jegl_shader::uniform_type::FLOAT2X2:
                        unit_size = 16;
                        break;
                    case jegl_shader::uniform_type::FLOAT3X3:
                        unit_size = 48;
                        break;
                    case jegl_shader::uniform_type::FLOAT4X4:
                        unit_size = 64;
                        break;
                    default:
                        unit_size = 0;
                        break;
                    }

                    if (unit_size != 0)
                    {
                        auto next_edge = last_elem_end_place / DX11_ALLIGN_BASE * DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

                        if (last_elem_end_place + std::min((size_t)16, unit_size) > next_edge)
                            last_elem_end_place = next_edge;

                        blob->m_uniform_locations[uniforms->m_name] = last_elem_end_place;

                        last_elem_end_place += unit_size;
                    }
                    uniforms = uniforms->m_next;
                }

                if (last_elem_end_place % DX11_ALLIGN_BASE != 0)
                    last_elem_end_place = last_elem_end_place / DX11_ALLIGN_BASE * DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

                blob->m_uniform_size = last_elem_end_place;

                blob->m_rasterizer = context->m_rasterizers[
                    static_cast<size_t>(resource->m_raw_shader_data->m_cull_mode)];
                blob->m_rasterizer_r2b = context->m_rasterizers_r2b[
                    static_cast<size_t>(resource->m_raw_shader_data->m_cull_mode)];

                D3D11_DEPTH_STENCIL_DESC depth_describe;
                depth_describe.DepthEnable = TRUE;
                switch (resource->m_raw_shader_data->m_depth_test)
                {
                case jegl_shader::depth_test_method::NEVER:
                    depth_describe.DepthFunc = D3D11_COMPARISON_NEVER;
                    break;
                case jegl_shader::depth_test_method::LESS: /* DEFAULT */
                    depth_describe.DepthFunc = D3D11_COMPARISON_LESS;
                    break;
                case jegl_shader::depth_test_method::EQUAL:
                    depth_describe.DepthFunc = D3D11_COMPARISON_EQUAL;
                    break;
                case jegl_shader::depth_test_method::LESS_EQUAL:
                    depth_describe.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
                    break;
                case jegl_shader::depth_test_method::GREATER:
                    depth_describe.DepthFunc = D3D11_COMPARISON_GREATER;
                    break;
                case jegl_shader::depth_test_method::NOT_EQUAL:
                    depth_describe.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
                    break;
                case jegl_shader::depth_test_method::GREATER_EQUAL:
                    depth_describe.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
                    break;
                case jegl_shader::depth_test_method::ALWAYS:
                    depth_describe.DepthFunc = D3D11_COMPARISON_ALWAYS;
                    break;
                }
                switch (resource->m_raw_shader_data->m_depth_mask)
                {
                case jegl_shader::depth_mask_method::ENABLE:
                    depth_describe.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                    break;
                case jegl_shader::depth_mask_method::DISABLE:
                    depth_describe.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
                    break;
                }
                depth_describe.StencilEnable = FALSE;
                depth_describe.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
                depth_describe.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
                depth_describe.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                depth_describe.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                depth_describe.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                depth_describe.BackFace;
                JERCHECK(context->m_dx_device->CreateDepthStencilState(
                    &depth_describe, blob->m_depth.GetAddressOf()));

                JEDX11_TRACE_DEBUG_NAME(blob->m_depth,
                    string_path + "_DepthStencilState");

                D3D11_BLEND_DESC blend_describe = {};
                blend_describe.AlphaToCoverageEnable = FALSE;
                // OpenGL咋没这么牛逼的选项呢…… 只能含泪关掉
                blend_describe.IndependentBlendEnable = FALSE;

                blend_describe.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                if (resource->m_raw_shader_data->m_blend_equation == jegl_shader::blend_equation::DISABLED)
                    blend_describe.RenderTarget[0].BlendEnable = FALSE;
                else
                {
                    blend_describe.RenderTarget[0].BlendEnable = TRUE;

                    auto parse_dx11_enum_blend_method = [](jegl_shader::blend_method method)
                        {
                            switch (method)
                            {
                            case jegl_shader::blend_method::ZERO:
                                return D3D11_BLEND_ZERO;
                            case jegl_shader::blend_method::ONE:
                                return D3D11_BLEND_ONE;
                            case jegl_shader::blend_method::SRC_COLOR:
                                return D3D11_BLEND_SRC_COLOR;
                            case jegl_shader::blend_method::SRC_ALPHA:
                                return D3D11_BLEND_SRC_ALPHA;
                            case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA:
                                return D3D11_BLEND_INV_SRC_ALPHA;
                            case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR:
                                return D3D11_BLEND_INV_SRC_COLOR;
                            case jegl_shader::blend_method::DST_COLOR:
                                return D3D11_BLEND_DEST_COLOR;
                            case jegl_shader::blend_method::DST_ALPHA:
                                return D3D11_BLEND_DEST_ALPHA;
                            case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA:
                                return D3D11_BLEND_INV_DEST_ALPHA;
                            case jegl_shader::blend_method::ONE_MINUS_DST_COLOR:
                                return D3D11_BLEND_INV_DEST_COLOR;
                            default:
                                jeecs::debug::logerr("Invalid blend src method.");
                                return D3D11_BLEND_ONE;
                            }
                        };
                    auto parse_dx11_enum_blend_equation = [](jegl_shader::blend_equation eq)
                        {
                            switch (eq)
                            {
                            case jegl_shader::blend_equation::ADD:
                                return D3D11_BLEND_OP_ADD;
                            case jegl_shader::blend_equation::SUBTRACT:
                                return D3D11_BLEND_OP_SUBTRACT;
                            case jegl_shader::blend_equation::REVERSE_SUBTRACT:
                                return D3D11_BLEND_OP_REV_SUBTRACT;
                            case jegl_shader::blend_equation::MIN:
                                return D3D11_BLEND_OP_MIN;
                            case jegl_shader::blend_equation::MAX:
                                return D3D11_BLEND_OP_MAX;
                            default:
                                jeecs::debug::logerr("Invalid blend equation.");
                                return D3D11_BLEND_OP_ADD;
                            }
                        };

                    blend_describe.RenderTarget[0].BlendOp
                        = blend_describe.RenderTarget[0].BlendOpAlpha
                        = parse_dx11_enum_blend_equation(
                            resource->m_raw_shader_data->m_blend_equation);

                    blend_describe.RenderTarget[0].SrcBlend
                        = blend_describe.RenderTarget[0].SrcBlendAlpha
                        = parse_dx11_enum_blend_method(resource->m_raw_shader_data->m_blend_src_mode);
                    blend_describe.RenderTarget[0].DestBlend
                        = blend_describe.RenderTarget[0].DestBlendAlpha
                        = parse_dx11_enum_blend_method(resource->m_raw_shader_data->m_blend_dst_mode);
                }
                JERCHECK(context->m_dx_device->CreateBlendState(
                    &blend_describe, blob->m_blend.GetAddressOf()));

                JEDX11_TRACE_DEBUG_NAME(blob->m_blend,
                    string_path + "_BlendState");

                blob->m_samplers.resize(resource->m_raw_shader_data->m_sampler_count);
                for (size_t i = 0; i < resource->m_raw_shader_data->m_sampler_count; ++i)
                {
                    auto& dxsampler = blob->m_samplers[i];
                    auto& sampler = resource->m_raw_shader_data->m_sampler_methods[i];
                    dxsampler.m_sampler_id = sampler.m_sampler_id;

                    D3D11_SAMPLER_DESC sampler_describe;

                    sampler_describe.MipLODBias = 0.0f;
                    sampler_describe.MaxAnisotropy = 0;
                    sampler_describe.BorderColor[0] = 0.0f;
                    sampler_describe.BorderColor[1] = 0.0f;
                    sampler_describe.BorderColor[2] = 0.0f;
                    sampler_describe.BorderColor[3] = 0.0f;

                    // Apply fliter setting
                    if (sampler.m_min == jegl_shader::fliter_mode::LINEAR)
                    {
                        if (sampler.m_mag == jegl_shader::fliter_mode::LINEAR)
                        {
                            if (sampler.m_mag == jegl_shader::fliter_mode::LINEAR)
                                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                            else
                                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                        }
                        else
                        {
                            if (sampler.m_mag == jegl_shader::fliter_mode::LINEAR)
                                sampler_describe.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                            else
                                sampler_describe.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
                        }
                    }
                    else
                    {
                        if (sampler.m_mag == jegl_shader::fliter_mode::LINEAR)
                        {
                            if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
                                sampler_describe.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                            else
                                sampler_describe.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
                        }
                        else
                        {
                            if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
                                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                            else
                                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                        }
                    }

                    if (sampler.m_uwrap == jegl_shader::wrap_mode::CLAMP)
                        sampler_describe.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                    else
                        sampler_describe.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;

                    if (sampler.m_vwrap == jegl_shader::wrap_mode::CLAMP)
                        sampler_describe.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                    else
                        sampler_describe.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;

                    sampler_describe.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
                    sampler_describe.ComparisonFunc = D3D11_COMPARISON_NEVER;
                    sampler_describe.MinLOD = 0;
                    sampler_describe.MaxLOD = D3D11_FLOAT32_MAX;
                    JERCHECK(context->m_dx_device->CreateSamplerState(
                        &sampler_describe,
                        dxsampler.m_sampler.GetAddressOf()));

                    dxsampler.m_sampler;
                }
                return blob;
            }
        }
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

    void dx11_close_resource_blob(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
            delete reinterpret_cast<dx11_resource_shader_blob*>(blob);
    }

    jedx11_texture* dx11_create_texture_instance(jegl_dx11_context* context, jegl_resource* resource, bool is_dynamic)
    {
        D3D11_TEXTURE2D_DESC texture_describe;
        texture_describe.Width = (UINT)resource->m_raw_texture_data->m_width;
        texture_describe.Height = (UINT)resource->m_raw_texture_data->m_height;
        texture_describe.MipLevels = 1;
        texture_describe.ArraySize = 1;

        bool float16 = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::FLOAT16);
        bool is_cube = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::CUBE);

        switch (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
        {
        case jegl_texture::format::MONO:
            texture_describe.Format = float16
                ? DXGI_FORMAT_R16_FLOAT
                : DXGI_FORMAT_R8_UNORM;
            break;
        case jegl_texture::format::RGBA:
            texture_describe.Format = float16
                ? DXGI_FORMAT_R16G16B16A16_FLOAT
                : DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            texture_describe.Format = DXGI_FORMAT_UNKNOWN;
        }

        // 不使用MSAA
        texture_describe.SampleDesc.Count = 1;
        texture_describe.SampleDesc.Quality = 0;

        texture_describe.Usage = is_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
        texture_describe.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_describe.CPUAccessFlags = is_dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        texture_describe.MiscFlags = 0;

        jedx11_texture* jedx11_texture_res = is_dynamic ? new jedx11_modifiable_texture : new jedx11_texture;

        // All texture is unmodifiable as default.
        jedx11_texture_res->m_modifiable_texture_buffer = is_dynamic;

        D3D11_SUBRESOURCE_DATA texture_sub_data;
        D3D11_SUBRESOURCE_DATA* texture_sub_data_ptr = nullptr;

        D3D11_SHADER_RESOURCE_VIEW_DESC texture_shader_view_describe;

        texture_shader_view_describe.Format = texture_describe.Format;
        texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        texture_shader_view_describe.Texture2D.MipLevels = 1;
        texture_shader_view_describe.Texture2D.MostDetailedMip = 0;

        if ((resource->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF) == jegl_texture::format::FRAMEBUF)
        {
            if (resource->m_raw_texture_data->m_format & jegl_texture::format::DEPTH)
            {
                texture_describe.Format = DXGI_FORMAT_R24G8_TYPELESS;
                texture_describe.Usage = D3D11_USAGE_DEFAULT;

                // 不使用MSAA
                texture_describe.SampleDesc.Count = 1;
                texture_describe.SampleDesc.Quality = 0;

                texture_describe.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

                texture_shader_view_describe.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            }
            else
            {
                texture_describe.BindFlags |= D3D11_BIND_RENDER_TARGET;
                texture_describe.Usage = D3D11_USAGE_DEFAULT;
            }
        }
        else
        {
            texture_sub_data.pSysMem = resource->m_raw_texture_data->m_pixels;
            texture_sub_data.SysMemPitch =
                (UINT)resource->m_raw_texture_data->m_width *
                ((UINT)resource->m_raw_texture_data->m_format &
                    jegl_texture::format::COLOR_DEPTH_MASK);
            texture_sub_data.SysMemSlicePitch =
                (UINT)resource->m_raw_texture_data->m_width *
                (UINT)resource->m_raw_texture_data->m_height *
                ((UINT)resource->m_raw_texture_data->m_format &
                    jegl_texture::format::COLOR_DEPTH_MASK);

            texture_sub_data_ptr = &texture_sub_data;
        }

        assert(DXGI_FORMAT_UNKNOWN != texture_describe.Format);
        assert(DXGI_FORMAT_UNKNOWN != texture_shader_view_describe.Format);

        JERCHECK(context->m_dx_device->CreateTexture2D(
            &texture_describe, texture_sub_data_ptr,
            jedx11_texture_res->m_texture.GetAddressOf()));

        JERCHECK(context->m_dx_device->CreateShaderResourceView(
            jedx11_texture_res->m_texture.Get(),
            &texture_shader_view_describe,
            jedx11_texture_res->m_texture_view.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(jedx11_texture_res->m_texture,
            std::string(resource->m_path == nullptr ? "_builtin_texture_" : resource->m_path) + "_Texture");
        JEDX11_TRACE_DEBUG_NAME(jedx11_texture_res->m_texture_view,
            std::string(resource->m_path == nullptr ? "_builtin_texture_" : resource->m_path) + "_View");

        return jedx11_texture_res;
    }
    void dx11_init_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            if (blob != nullptr)
            {
                auto* shader_blob =
                    reinterpret_cast<dx11_resource_shader_blob*>(blob);

                jedx11_shader* jedx11_shader_res = new jedx11_shader;
                jedx11_shader_res->m_draw_for_r2b = false;
                jedx11_shader_res->m_uniform_updated = false;

                std::string string_path = resource->m_path == nullptr
                    ? "__joyengine_builtin_vshader" + std::to_string((intptr_t)resource) + "__"
                    : resource->m_path;

                auto* raw_shader_data = resource->m_raw_shader_data;
                auto& builtin_uniforms = raw_shader_data->m_builtin_uniforms;

                builtin_uniforms.m_builtin_uniform_ndc_scale =
                    shader_blob->get_built_in_location("JE_NDC_SCALE");
                jedx11_shader_res->m_ndc_scale_uniform_id =
                    builtin_uniforms.m_builtin_uniform_ndc_scale;

                builtin_uniforms.m_builtin_uniform_m =
                    shader_blob->get_built_in_location("JE_M");
                builtin_uniforms.m_builtin_uniform_mv =
                    shader_blob->get_built_in_location("JE_MV");
                builtin_uniforms.m_builtin_uniform_mvp =
                    shader_blob->get_built_in_location("JE_MVP");

                builtin_uniforms.m_builtin_uniform_tiling =
                    shader_blob->get_built_in_location("JE_UV_TILING");
                builtin_uniforms.m_builtin_uniform_offset =
                    shader_blob->get_built_in_location("JE_UV_OFFSET");

                builtin_uniforms.m_builtin_uniform_light2d_resolution =
                    shader_blob->get_built_in_location("JE_LIGHT2D_RESOLUTION");
                builtin_uniforms.m_builtin_uniform_light2d_decay =
                    shader_blob->get_built_in_location("JE_LIGHT2D_DECAY");

                // ATTENTION: 注意，以下参数特殊shader可能挪作他用
                builtin_uniforms.m_builtin_uniform_local_scale =
                    shader_blob->get_built_in_location("JE_LOCAL_SCALE");
                builtin_uniforms.m_builtin_uniform_color =
                    shader_blob->get_built_in_location("JE_COLOR");

                auto* uniforms = raw_shader_data->m_custom_uniforms;
                while (uniforms != nullptr)
                {
                    uniforms->m_index = shader_blob->get_built_in_location(uniforms->m_name);
                    uniforms = uniforms->m_next;
                }

                jedx11_shader_res->m_uniform_buffer_size = shader_blob->m_uniform_size;
                if (jedx11_shader_res->m_uniform_buffer_size != 0)
                {
                    D3D11_BUFFER_DESC const_buffer_describe;

                    const_buffer_describe.ByteWidth = (UINT)jedx11_shader_res->m_uniform_buffer_size;
                    const_buffer_describe.Usage = D3D11_USAGE_DYNAMIC;
                    const_buffer_describe.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                    const_buffer_describe.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                    const_buffer_describe.MiscFlags = 0;
                    const_buffer_describe.StructureByteStride = 0;

                    // 新建常量缓冲区，不使用初始数据
                    JERCHECK(context->m_dx_device->CreateBuffer(
                        &const_buffer_describe, nullptr,
                        jedx11_shader_res->m_uniforms.GetAddressOf()));

                    jedx11_shader_res->m_uniform_cpu_buffers =
                        malloc(shader_blob->m_uniform_size);

                    memset(
                        jedx11_shader_res->m_uniform_cpu_buffers,
                        0,
                        shader_blob->m_uniform_size);
                }
                else
                    jedx11_shader_res->m_uniform_cpu_buffers = nullptr;

                jedx11_shader_res->m_vao = shader_blob->m_vao;
                jedx11_shader_res->m_vertex = shader_blob->m_vertex;
                jedx11_shader_res->m_fragment = shader_blob->m_fragment;

                jedx11_shader_res->m_rasterizer = shader_blob->m_rasterizer;
                jedx11_shader_res->m_rasterizer_r2b = shader_blob->m_rasterizer_r2b;
                jedx11_shader_res->m_depth = shader_blob->m_depth;
                jedx11_shader_res->m_blend = shader_blob->m_blend;

                jedx11_shader_res->m_samplers = shader_blob->m_samplers;

                resource->m_handle.m_ptr = jedx11_shader_res;
            }
            else
            {
                resource->m_handle.m_ptr = nullptr;
            }
            break;
        }
        case jegl_resource::type::TEXTURE:
        {
            assert((resource->m_raw_texture_data->m_format & jegl_texture::format::CUBE) == 0);
            resource->m_handle.m_ptr = dx11_create_texture_instance(
                context, resource, false /* Immutable as default */);
            break;
        }
        case jegl_resource::type::VERTEX:
        {
            jedx11_vertex* vertex = new jedx11_vertex;

            const static D3D_PRIMITIVE_TOPOLOGY DRAW_METHODS[] = {
                D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
                D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
            };
            vertex->m_method =
                DRAW_METHODS[resource->m_raw_vertex_data->m_type];

            vertex->m_count = (UINT)resource->m_raw_vertex_data->m_index_count;
            vertex->m_stride = resource->m_raw_vertex_data->m_data_size_per_point;

            // 新建顶点缓冲区
            D3D11_BUFFER_DESC vertex_buffer_describe;
            vertex_buffer_describe.ByteWidth =
                (UINT)resource->m_raw_vertex_data->m_vertex_length;

            vertex_buffer_describe.Usage = D3D11_USAGE_IMMUTABLE;
            vertex_buffer_describe.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertex_buffer_describe.CPUAccessFlags = 0;
            vertex_buffer_describe.MiscFlags = 0;
            vertex_buffer_describe.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA vertex_buffer_data;
            vertex_buffer_data.pSysMem = resource->m_raw_vertex_data->m_vertexs;
            vertex_buffer_data.SysMemPitch = 0;
            vertex_buffer_data.SysMemSlicePitch = 0;
            JERCHECK(context->m_dx_device->CreateBuffer(
                &vertex_buffer_describe,
                &vertex_buffer_data,
                vertex->m_vbo.GetAddressOf()));

            JEDX11_TRACE_DEBUG_NAME(vertex->m_vbo,
                std::string(resource->m_path == nullptr ? "_builtin_vertex_" : resource->m_path) + "_Vbo");

            // 新建索引缓冲区
            static_assert(sizeof(uint32_t) == sizeof(UINT));

            D3D11_BUFFER_DESC index_buffer_describe;
            index_buffer_describe.ByteWidth =
                (UINT)resource->m_raw_vertex_data->m_index_count * sizeof(uint32_t);
            index_buffer_describe.Usage = D3D11_USAGE_IMMUTABLE;
            index_buffer_describe.BindFlags = D3D11_BIND_INDEX_BUFFER;
            index_buffer_describe.CPUAccessFlags = 0;
            index_buffer_describe.MiscFlags = 0;
            index_buffer_describe.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA index_buffer_data;
            index_buffer_data.pSysMem = resource->m_raw_vertex_data->m_indices;
            index_buffer_data.SysMemPitch = 0;
            index_buffer_data.SysMemSlicePitch = 0;
            JERCHECK(context->m_dx_device->CreateBuffer(
                &index_buffer_describe,
                &index_buffer_data,
                vertex->m_ebo.GetAddressOf()));

            JEDX11_TRACE_DEBUG_NAME(vertex->m_ebo,
                std::string(resource->m_path == nullptr ? "_builtin_vertex_" : resource->m_path) + "_Ebo");

            resource->m_handle.m_ptr = vertex;

            break;
        }
        case jegl_resource::type::FRAMEBUF:
        {
            jedx11_framebuffer* jedx11_framebuffer_res =
                new jedx11_framebuffer(
                    resource->m_raw_framebuf_data->m_width,
                    resource->m_raw_framebuf_data->m_height);

            jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
                    resource->m_raw_framebuf_data->m_output_attachments));

            size_t color_attachment_count = 0;
            for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
            {
                auto& attachment = attachments[i];
                if (0 == (
                    attachment->resource()->m_raw_texture_data->m_format
                    & jegl_texture::format::DEPTH))
                    ++color_attachment_count;
            }

            jedx11_framebuffer_res->m_rend_views.resize(color_attachment_count);
            color_attachment_count = 0;

            for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
            {
                auto& attachment = attachments[i];
                jegl_using_resource(attachment->resource());
                if (0 != (
                    attachment->resource()->m_raw_texture_data->m_format
                    & jegl_texture::format::DEPTH))
                {
                    if (jedx11_framebuffer_res->m_depth_view.Get() == nullptr)
                    {
                        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_describe;
                        depth_view_describe.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                        depth_view_describe.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                        depth_view_describe.Texture2D.MipSlice = 0;
                        depth_view_describe.Flags = 0;

                        JERCHECK(context->m_dx_device->CreateDepthStencilView(
                            std::launder(
                                reinterpret_cast<jedx11_texture*>(
                                    attachment->resource()->m_handle.m_ptr))->m_texture.Get(),
                            &depth_view_describe,
                            jedx11_framebuffer_res->m_depth_view.GetAddressOf()));

                        JEDX11_TRACE_DEBUG_NAME(jedx11_framebuffer_res->m_depth_view, "Framebuffer_DepthView");
                    }
                    else
                        jeecs::debug::logerr("Framebuffer(%p) attach depth buffer repeatedly.", resource);
                }
                else
                {
                    JERCHECK(context->m_dx_device->CreateRenderTargetView(
                        std::launder(
                            reinterpret_cast<jedx11_texture*>(
                                attachment->resource()->m_handle.m_ptr))->m_texture.Get(),
                        nullptr,
                        jedx11_framebuffer_res->m_rend_views[color_attachment_count].GetAddressOf()));

                    JEDX11_TRACE_DEBUG_NAME(
                        jedx11_framebuffer_res->m_rend_views[color_attachment_count],
                        "Framebuffer_Color");

                    ++color_attachment_count;
                }
            }

            for (auto& v : jedx11_framebuffer_res->m_rend_views)
                jedx11_framebuffer_res->m_target_views.push_back(v.Get());

            jedx11_framebuffer_res->m_target_depth_view_may_null =
                jedx11_framebuffer_res->m_depth_view.Get();

            jedx11_framebuffer_res->m_color_target_count = (UINT)color_attachment_count;
            assert(
                jedx11_framebuffer_res->m_target_views.size()
                == jedx11_framebuffer_res->m_rend_views.size());
            assert(
                jedx11_framebuffer_res->m_target_views.size()
                == color_attachment_count);

            resource->m_handle.m_ptr = jedx11_framebuffer_res;
            break;
        }
        case jegl_resource::type::UNIFORMBUF:
        {
            auto* jedx11_uniformbuf_res = new jedx11_uniformbuf;

            D3D11_BUFFER_DESC const_buffer_describe;

            const_buffer_describe.ByteWidth =
                (UINT)resource->m_raw_uniformbuf_data->m_buffer_size;
            const_buffer_describe.Usage = D3D11_USAGE_DYNAMIC;
            const_buffer_describe.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            const_buffer_describe.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            const_buffer_describe.MiscFlags = 0;
            const_buffer_describe.StructureByteStride = 0;

            D3D11_SUBRESOURCE_DATA const_buffer_resource_describe;
            const_buffer_resource_describe.pSysMem =
                resource->m_raw_uniformbuf_data->m_buffer;
            const_buffer_resource_describe.SysMemPitch = 0;
            const_buffer_resource_describe.SysMemSlicePitch = 0;

            // 新建常量缓冲区，不使用初始数据
            JERCHECK(context->m_dx_device->CreateBuffer(
                &const_buffer_describe, &const_buffer_resource_describe,
                jedx11_uniformbuf_res->m_uniformbuf.GetAddressOf()));

            jedx11_uniformbuf_res->m_binding_place =
                // 0 has been used as shader uniforms
                (UINT)resource->m_raw_uniformbuf_data->m_buffer_binding_place + 1;

            resource->m_handle.m_ptr = jedx11_uniformbuf_res;
            break;
        }
        default:
            break;
        }
    }
    void dx11_close_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource);
    void dx11_using_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
            break;
        case jegl_resource::type::TEXTURE:
            if (resource->m_modified)
            {
                resource->m_modified = false;
                if (resource->m_raw_texture_data != nullptr)
                {
                    jedx11_texture* texture_instance =
                        std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));

                    if (texture_instance->m_modifiable_texture_buffer)
                    {
                        // We map the teure buffer to CPU memory, and copy the new data to it.
                        D3D11_MAPPED_SUBRESOURCE mappedData;

                        JERCHECK(context->m_dx_context->Map(
                            texture_instance->m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

                        // mappedData is not byte-aligned, we need to copy it byte by row
                        for (size_t i = 0; i < resource->m_raw_texture_data->m_height; ++i)
                        {
                            const size_t row_byte_size = resource->m_raw_texture_data->m_width * (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK);
                            void* dst_row_data = (void*)((intptr_t)mappedData.pData + i * mappedData.RowPitch);
                            const void* src_row_data = resource->m_raw_texture_data->m_pixels + i * row_byte_size;

                            memcpy(dst_row_data, src_row_data, row_byte_size);
                        }
                        context->m_dx_context->Unmap(texture_instance->m_texture.Get(), 0);
                    }
                    else
                    {
                        // This texture is immutable, we need to recreate it as dynamic
                        jedx11_modifiable_texture* modifiable_texture_instance =
                            static_cast<jedx11_modifiable_texture*>(dx11_create_texture_instance(
                                context, resource, true /* Regenerate it as dynamic */));

                        modifiable_texture_instance->m_obsoluted_texture = texture_instance;
                        resource->m_handle.m_ptr = modifiable_texture_instance;
                    }
                }
            }
            break;
        case jegl_resource::type::VERTEX:
            break;
        case jegl_resource::type::FRAMEBUF:
            break;
        case jegl_resource::type::UNIFORMBUF:
        {
            if (resource->m_modified)
            {
                resource->m_modified = false;

                auto* uniformbuf_instance = std::launder(reinterpret_cast<jedx11_uniformbuf*>(resource->m_handle.m_ptr));
                if (resource->m_raw_uniformbuf_data != nullptr)
                {
                    assert(resource->m_raw_uniformbuf_data->m_update_length != 0);
                    D3D11_MAPPED_SUBRESOURCE mappedData;
                    JERCHECK(context->m_dx_context->Map(
                        uniformbuf_instance->m_uniformbuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

                    // DX11 Donot support partial update, update all.
                    memcpy((void*)((intptr_t)mappedData.pData),
                        resource->m_raw_uniformbuf_data->m_buffer,
                        resource->m_raw_uniformbuf_data->m_buffer_size);

                    context->m_dx_context->Unmap(uniformbuf_instance->m_uniformbuf.Get(), 0);
                }
            }
            break;
        }
        default:
            break;
        }
    }
    void dx11_close_resource(jegl_context::graphic_impl_context_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            auto* shader = reinterpret_cast<jedx11_shader*>(resource->m_handle.m_ptr);
            if (shader != nullptr)
            {
                if (shader->m_uniform_buffer_size != 0)
                {
                    assert(shader->m_uniform_cpu_buffers != nullptr);
                    free(shader->m_uniform_cpu_buffers);
                }
                delete shader;
            }
            break;
        }
        case jegl_resource::type::TEXTURE:
        {
            jedx11_texture* texture_instance =
                std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));

            if (texture_instance->m_modifiable_texture_buffer)
            {
                auto* dynamic_texture_instance = static_cast<jedx11_modifiable_texture*>(texture_instance);
                delete dynamic_texture_instance->m_obsoluted_texture;
                delete dynamic_texture_instance;
            }
            else
                delete texture_instance;

            break;
        }
        case jegl_resource::type::VERTEX:
            delete std::launder(reinterpret_cast<jedx11_vertex*>(resource->m_handle.m_ptr));
            break;
        case jegl_resource::type::FRAMEBUF:
            delete std::launder(reinterpret_cast<jedx11_framebuffer*>(resource->m_handle.m_ptr));
            break;
        case jegl_resource::type::UNIFORMBUF:
            delete std::launder(reinterpret_cast<jedx11_uniformbuf*>(resource->m_handle.m_ptr));
            break;
        default:
            break;
        }

        // 添加：清理资源句柄
        resource->m_handle.m_ptr = nullptr;
    }
    void dx11_set_uniform(jegl_context::graphic_impl_context_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val);
    void dx11_draw_vertex_with_shader(jegl_context::graphic_impl_context_t ctx, jegl_resource* vert)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        assert(vert->m_type == jegl_resource::type::VERTEX);

        auto* current_shader_instance = context->m_current_target_shader;
        assert(current_shader_instance != nullptr);

        if (current_shader_instance->m_uniform_buffer_size != 0)
        {
            if (context->m_current_target_framebuffer == nullptr)
            {
                if (current_shader_instance->m_draw_for_r2b)
                {
                    const float ndc_scale[4] = { 1.f, 1.f, 1.f, 1.f };

                    current_shader_instance->m_draw_for_r2b = false;
                    dx11_set_uniform(
                        ctx,
                        current_shader_instance->m_ndc_scale_uniform_id,
                        jegl_shader::uniform_type::FLOAT4,
                        ndc_scale);
                }
            }
            else
            {
                if (!current_shader_instance->m_draw_for_r2b)
                {
                    const float ndc_scale_r2b[4] = { 1.f, -1.f, 1.f, 1.f };

                    current_shader_instance->m_draw_for_r2b = true;
                    dx11_set_uniform(
                        ctx,
                        current_shader_instance->m_ndc_scale_uniform_id,
                        jegl_shader::uniform_type::FLOAT4,
                        ndc_scale_r2b);
                }
            }

            if (context->m_current_target_shader->m_uniform_updated)
            {
                context->m_current_target_shader->m_uniform_updated = false;
                D3D11_MAPPED_SUBRESOURCE mappedData;
                JERCHECK(context->m_dx_context->Map(
                    context->m_current_target_shader->m_uniforms.Get(),
                    0,
                    D3D11_MAP_WRITE_DISCARD,
                    0,
                    &mappedData));

                memcpy(mappedData.pData,
                    context->m_current_target_shader->m_uniform_cpu_buffers,
                    context->m_current_target_shader->m_uniform_buffer_size);

                context->m_dx_context->Unmap(context->m_current_target_shader->m_uniforms.Get(), 0);
            }
            context->m_dx_context->VSSetConstantBuffers(0, 1,
                context->m_current_target_shader->m_uniforms.GetAddressOf());
            context->m_dx_context->PSSetConstantBuffers(0, 1,
                context->m_current_target_shader->m_uniforms.GetAddressOf());
        }

        auto* vertex = std::launder(reinterpret_cast<jedx11_vertex*>(vert->m_handle.m_ptr));

        const UINT offset = 0;
        const UINT strides = vertex->m_stride;
        context->m_dx_context->IASetIndexBuffer(
            vertex->m_ebo.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->m_dx_context->IASetVertexBuffers(
            0, 1, vertex->m_vbo.GetAddressOf(), &strides, &offset);
        context->m_dx_context->IASetPrimitiveTopology(vertex->m_method);
        context->m_dx_context->DrawIndexed(vertex->m_count, 0, 0);
    }
    bool dx11_bind_shader(jegl_context::graphic_impl_context_t ctx, jegl_resource* shader)
    {
        jegl_dx11_context* context = reinterpret_cast<jegl_dx11_context*>(ctx);

        auto* shader_instance = reinterpret_cast<jedx11_shader*>(shader->m_handle.m_ptr);

        if (context->m_current_target_shader == shader_instance)
            return shader_instance != nullptr;

        context->m_current_target_shader = shader_instance;

        if (shader_instance == nullptr)
            return false;

        context->m_dx_context->VSSetShader(shader_instance->m_vertex.Get(), nullptr, 0);
        context->m_dx_context->PSSetShader(shader_instance->m_fragment.Get(), nullptr, 0);
        context->m_dx_context->IASetInputLayout(shader_instance->m_vao.Get());

        if (context->m_current_target_framebuffer == nullptr)
            context->m_dx_context->RSSetState(shader_instance->m_rasterizer.Get());
        else
            context->m_dx_context->RSSetState(shader_instance->m_rasterizer_r2b.Get());

        float _useless[4] = {};
        context->m_dx_context->OMSetBlendState(shader_instance->m_blend.Get(), _useless, UINT_MAX);
        context->m_dx_context->OMSetDepthStencilState(shader_instance->m_depth.Get(), 0);

        for (auto& sampler : shader_instance->m_samplers)
        {
            context->m_dx_context->VSSetSamplers(
                sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
            context->m_dx_context->PSSetSamplers(
                sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
        }
        return true;
    }

    void dx11_bind_uniform_buffer(jegl_context::graphic_impl_context_t ctx, jegl_resource* uniformbuf)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        auto* uniformbuf_instance = std::launder(reinterpret_cast<jedx11_uniformbuf*>(uniformbuf->m_handle.m_ptr));
        context->m_dx_context->VSSetConstantBuffers(
            uniformbuf_instance->m_binding_place, 1, uniformbuf_instance->m_uniformbuf.GetAddressOf());
        context->m_dx_context->PSSetConstantBuffers(
            uniformbuf_instance->m_binding_place, 1, uniformbuf_instance->m_uniformbuf.GetAddressOf());
    }

    void dx11_bind_texture(jegl_context::graphic_impl_context_t ctx, jegl_resource* texture, size_t pass)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        auto* texture_instance = std::launder(reinterpret_cast<jedx11_texture*>(texture->m_handle.m_ptr));
        if (texture_instance->m_texture_view.Get() != nullptr)
        {
            context->m_dx_context->VSSetShaderResources(
                (UINT)pass, 1, texture_instance->m_texture_view.GetAddressOf());
            context->m_dx_context->PSSetShaderResources(
                (UINT)pass, 1, texture_instance->m_texture_view.GetAddressOf());
        }
    }

    void dx11_set_rend_to_framebuffer(
        jegl_context::graphic_impl_context_t ctx,
        jegl_resource* framebuffer,
        const int32_t(*viewport_xywh)[4],
        const jegl_frame_buffer_clear_operation* clear_operations)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        // Reset current binded shader.
        context->m_current_target_shader = nullptr;

        if (framebuffer == nullptr)
        {
            context->m_current_target_framebuffer = nullptr;

            context->m_dx_context->OMSetRenderTargets(1,
                context->m_dx_main_renderer_target_view.GetAddressOf(),
                context->m_dx_main_renderer_target_depth_view.Get());
        }
        else
        {
            context->m_current_target_framebuffer =
                reinterpret_cast<jedx11_framebuffer*>(
                    framebuffer->m_handle.m_ptr);

            context->m_dx_context->OMSetRenderTargets(
                context->m_current_target_framebuffer->m_color_target_count,
                context->m_current_target_framebuffer->m_target_views.data(),
                context->m_current_target_framebuffer->m_target_depth_view_may_null);
        }

        int32_t x = 0, y = 0, w = 0, h = 0;
        if (viewport_xywh != nullptr)
        {
            auto& v = *viewport_xywh;
            x = v[0];
            y = v[1];
            w = v[2];
            h = v[3];
        }

        const int32_t buf_h =
            static_cast<int32_t>(
                context->m_current_target_framebuffer != nullptr
                ? context->m_current_target_framebuffer->m_frame_height
                : context->RESOLUTION_HEIGHT);

        if (w == 0)
        {
            w = static_cast<int32_t>(
                context->m_current_target_framebuffer != nullptr
                ? context->m_current_target_framebuffer->m_frame_width
                : context->RESOLUTION_WIDTH);
        }
        if (h == 0)
            h = buf_h;

        D3D11_VIEWPORT viewport;

        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        viewport.Width = (float)w;
        viewport.TopLeftX = (float)x;

        viewport.Height = (float)h;
        viewport.TopLeftY = (float)(buf_h - y - h);

        context->m_dx_context->RSSetViewports(1, &viewport);

        while (clear_operations != nullptr)
        {
            switch (clear_operations->m_type)
            {
            case jegl_frame_buffer_clear_operation::clear_type::COLOR:
                if (context->m_current_target_framebuffer == nullptr)
                {
                    if (clear_operations->m_color.m_color_attachment_idx == 0)
                        context->m_dx_context->ClearRenderTargetView(
                            context->m_dx_main_renderer_target_view.Get(),
                            clear_operations->m_color.m_clear_color_rgba);
                }
                else if (clear_operations->m_color.m_color_attachment_idx <
                    context->m_current_target_framebuffer->m_target_views.size())
                {
                    context->m_dx_context->ClearRenderTargetView(
                        context->m_current_target_framebuffer->m_target_views.at(
                            clear_operations->m_color.m_color_attachment_idx),
                        clear_operations->m_color.m_clear_color_rgba);
                }
                break;
            case jegl_frame_buffer_clear_operation::clear_type::DEPTH:
                if (context->m_current_target_framebuffer == nullptr)
                    context->m_dx_context->ClearDepthStencilView(
                        context->m_dx_main_renderer_target_depth_view.Get(),
                        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                        clear_operations->m_depth.m_clear_depth,
                        0);
                else
                {
                    if (context->m_current_target_framebuffer->m_target_depth_view_may_null != nullptr)
                        context->m_dx_context->ClearDepthStencilView(
                            context->m_current_target_framebuffer->m_target_depth_view_may_null,
                            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                            clear_operations->m_depth.m_clear_depth,
                            0);
                }
                break;
            default:
                jeecs::debug::logfatal("Unknown framebuffer clear operation.");
                abort();
                break;
            }

            clear_operations = clear_operations->m_next;
        }
    }
    void dx11_set_uniform(
        jegl_context::graphic_impl_context_t ctx,
        uint32_t location,
        jegl_shader::uniform_type type,
        const void* val)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        if (location == jeecs::typing::INVALID_UINT32
            || context->m_current_target_shader == nullptr)
            return;

        context->m_current_target_shader->m_uniform_updated = true;
        assert(context->m_current_target_shader->m_uniform_buffer_size != 0);
        assert(context->m_current_target_shader->m_uniform_cpu_buffers != nullptr);

        auto* target_buffer = reinterpret_cast<void*>(
            (intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location);

        size_t data_size_byte_length = 0;
        switch (type)
        {
        case jegl_shader::INT:
        case jegl_shader::FLOAT:
            data_size_byte_length = 4;
            break;
        case jegl_shader::INT2:
        case jegl_shader::FLOAT2:
            data_size_byte_length = 8;
            break;
        case jegl_shader::INT3:
        case jegl_shader::FLOAT3:
            data_size_byte_length = 12;
            break;
        case jegl_shader::INT4:
        case jegl_shader::FLOAT4:
            data_size_byte_length = 16;
            break;
        case jegl_shader::FLOAT2X2:
            data_size_byte_length = 16;
            break;
        case jegl_shader::FLOAT3X3:
        {
            float* target_storage = reinterpret_cast<float*>(target_buffer);
            const float* source_storage = reinterpret_cast<const float*>(val);

            memcpy(target_storage, source_storage, 12);
            memcpy(target_storage + 4, source_storage + 3, 12);
            memcpy(target_storage + 8, source_storage + 6, 12);

            return;
        }
        case jegl_shader::FLOAT4X4:
            data_size_byte_length = 64;
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set.");
            break;
        }
        memcpy(target_buffer, val, data_size_byte_length);
    }
}

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::dx11;

    write_to_apis->interface_startup = dx11_startup;
    write_to_apis->interface_shutdown_before_resource_release = dx11_pre_shutdown;
    write_to_apis->interface_shutdown = dx11_shutdown;

    write_to_apis->update_frame_ready = dx11_pre_update;
    write_to_apis->update_draw_commit = dx11_commit_update;

    write_to_apis->create_resource_blob_cache = dx11_create_resource_blob;
    write_to_apis->close_resource_blob_cache = dx11_close_resource_blob;

    write_to_apis->create_resource = dx11_init_resource;
    write_to_apis->using_resource = dx11_using_resource;
    write_to_apis->close_resource = dx11_close_resource;

    write_to_apis->bind_uniform_buffer = dx11_bind_uniform_buffer;
    write_to_apis->bind_texture = dx11_bind_texture;
    write_to_apis->bind_shader = dx11_bind_shader;
    write_to_apis->draw_vertex = dx11_draw_vertex_with_shader;

    write_to_apis->bind_framebuf = dx11_set_rend_to_framebuffer;

    write_to_apis->set_uniform = dx11_set_uniform;
}

#else
void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("DX11 not available.");
}
#endif