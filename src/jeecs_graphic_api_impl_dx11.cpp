#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_graphic_render_internal.hpp"

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

    constexpr size_t MAX_TEXTURE_SLOTS = 128;
    constexpr size_t MAX_SAMPLER_SLOTS = 16;
    constexpr size_t MAX_CONSTANT_BUFFER_SLOTS = 14;

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
        jedx11_vertex* m_current_vertex = nullptr;
    };

    struct jedx11_texture
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_texture);
        jedx11_texture() = default;
        ~jedx11_texture() = default;

        bool m_modifiable_texture_buffer;
        // 二维/数组/立方体/MSAA 纹理使用 m_texture；三维纹理使用 m_texture_3d，
        // 两者互斥（未使用的一者为空）
        jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> m_texture;
        jegl_dx11_context::MSWRLComPtr<ID3D11Texture3D> m_texture_3d;
        jegl_dx11_context::MSWRLComPtr<ID3D11ShaderResourceView> m_texture_view;

        // 当前持有的纹理资源（作为通用资源视图使用）
        ID3D11Resource* texture_resource() const noexcept
        {
            return m_texture_3d.Get() != nullptr
                ? static_cast<ID3D11Resource*>(m_texture_3d.Get())
                : static_cast<ID3D11Resource*>(m_texture.Get());
        }
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
        size_t m_uniform_buffer_size;

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

        bool m_modifiable_vertex_buffer;
        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_vbo;
        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_ebo;
        UINT m_count;
        UINT m_stride;
        D3D_PRIMITIVE_TOPOLOGY m_method;
    };
    struct jedx11_modifiable_vertex : public jedx11_vertex
    {
        JECS_DISABLE_MOVE_AND_COPY(jedx11_modifiable_vertex);
        jedx11_modifiable_vertex() = default;
        ~jedx11_modifiable_vertex() = default;

        jedx11_vertex* m_obsoluted_vertex;
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

        // sRGB swapchain: shaders render linear and the backbuffer write
        // performs the final linear-to-sRGB encode.
        JERCHECK(context->m_dx_swapchain->ResizeBuffers(
            1, (UINT)context->RESOLUTION_WIDTH, (UINT)context->RESOLUTION_HEIGHT,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0));
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
            context->m_dx_main_renderer_target_depth_view,
            "JoyEngineDx11TargetDepthView");

        // 将渲染目标视图和深度/模板缓冲区结合到管线
        context->m_dx_context->OMSetRenderTargets(1,
            context->m_dx_main_renderer_target_view.GetAddressOf(),
            context->m_dx_main_renderer_target_depth_view.Get());

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

    void dx11_create_rasterizer_states(jegl_dx11_context* context)
    {
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
                    jegl_shader::cull_mode::NONE)].ReleaseAndGetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_FRONT;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers[
                static_cast<size_t>(
                    jegl_shader::cull_mode::FRONT)].ReleaseAndGetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_BACK;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers[
                static_cast<size_t>(
                    jegl_shader::cull_mode::BACK)].ReleaseAndGetAddressOf()));

        rasterizer_describe.FrontCounterClockwise = FALSE;

        rasterizer_describe.CullMode = D3D11_CULL_NONE;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::NONE)].ReleaseAndGetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_FRONT;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::FRONT)].ReleaseAndGetAddressOf()));
        rasterizer_describe.CullMode = D3D11_CULL_BACK;
        JERCHECK(context->m_dx_device->CreateRasterizerState(
            &rasterizer_describe,
            context->m_rasterizers_r2b[
                static_cast<size_t>(
                    jegl_shader::cull_mode::BACK)].ReleaseAndGetAddressOf()));
    }

    je_GraphicImplContext dx11_startup(jegl_context* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_dx11_context* context = new jegl_dx11_context;

        context->m_interface = new glfw(reboot ? glfw::HOLD : glfw::DIRECTX11);
        context->m_interface->create_interface(config);

        context->WINDOWS_HANDLE =
            glfwGetWin32Window(
                static_cast<GLFWwindow*>(
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

        // 如果因为系统未安装 D3D11 SDK Layers（例如未开启 "图形工具" 可选组件的 Win10）
        // 导致 D3D11_CREATE_DEVICE_DEBUG 失败，则去掉该标志后重试一次。
        if (!SUCCEEDED(result) && (dx_device_flag & D3D11_CREATE_DEVICE_DEBUG))
        {
            dx_device_flag &= ~D3D11_CREATE_DEVICE_DEBUG;
            jeecs::debug::logwarn(
                "D3D11 debug layer is not available on this system. "
                "Retrying D3D11CreateDevice without D3D11_CREATE_DEVICE_DEBUG.");

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
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        // 是否开启多重采样？
        UINT msaa_quality = 0;
        if (context->MSAA_LEVEL != 0)
        {
            context->m_dx_device->CheckMultisampleQualityLevels(
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, (UINT)context->MSAA_LEVEL, &msaa_quality);
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

        // Rasterizer states do not depend on window size, create them only once.
        dx11_create_rasterizer_states(context);

        dx11_callback_windows_size_changed(context);

        // Fullscreen?
        if (context->m_lock_resolution_for_fullscreen)
            context->m_dx_swapchain->SetFullscreenState(true, nullptr);

        jegui_init_dx11(
            gthread,
            [](jegl_context*, jegl_texture_2d* res)
            {
                auto* resource = static_cast<jedx11_texture*>(res->m_handle.m_ptr);
                return (uint64_t)resource->m_texture_view.Get();
            },
            [](jegl_context* ctx, jegl_shader* res)
            {
                auto* context = static_cast<jegl_dx11_context*>(ctx->m_graphic_impl_context);
                auto* shader = static_cast<jedx11_shader*>(res->m_handle.m_ptr);

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
    void dx11_pre_shutdown(jegl_context*, je_GraphicImplContext, bool)
    {
    }
    void dx11_shutdown(jegl_context*, je_GraphicImplContext userdata, bool reboot)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(userdata);

        if (!reboot)
            jeecs::debug::log("Graphic thread (DX11) shutdown!");
       
        // 确保退出全屏模式
        if (context->m_dx_swapchain)
        {
            BOOL isFullscreen = FALSE;
            Microsoft::WRL::ComPtr<IDXGIOutput> ignoredOutput;
            if (SUCCEEDED(context->m_dx_swapchain->GetFullscreenState(&isFullscreen, ignoredOutput.GetAddressOf())) && isFullscreen)
                context->m_dx_swapchain->SetFullscreenState(FALSE, nullptr);
        }

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

    jegl_update_action dx11_pre_update(je_GraphicImplContext ctx)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

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
        je_GraphicImplContext ctx, jegl_update_action)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

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

            return jeecs::graphic::INVALID_UNIFORM_LOCATION;
        }
    };

    jegl_resource_blob dx11_shader_create_blob(
        je_GraphicImplContext ctx,
        jegl_shader* shader)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

        bool shader_load_failed = false;

        dx11_resource_shader_blob* blob = new dx11_resource_shader_blob;
        std::string error_informations;
        ID3DBlob* error_blob = nullptr;

        jegl_dx11_context::MSWRLComPtr<ID3DBlob> vertex_blob;
        jegl_dx11_context::MSWRLComPtr<ID3DBlob> fragment_blob;

        std::string string_path =
            shader->m_handle.m_path_may_null_if_builtin != nullptr
            ? shader->m_handle.m_path_may_null_if_builtin
            : "__joyengine_builtin_vshader" + std::to_string((intptr_t)shader) + "__";

        auto compile_result = D3DCompile(
            shader->m_vertex_hlsl_src,
            strlen(shader->m_vertex_hlsl_src),
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
                error_informations += static_cast<const char*>(error_blob->GetBufferPointer());
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
                shader->m_vertex_in_count);

            // VIN
            size_t INT_COUNT = 0;
            size_t FLOAT_COUNT = 0;
            size_t FLOAT2_COUNT = 0;
            size_t FLOAT3_4_COUNT = 0;

            for (size_t i = 0; i < shader->m_vertex_in_count; ++i)
            {
                auto& vlayout = vertex_in_layout[i];

                vlayout.InputSlot = 0;
                vlayout.AlignedByteOffset = layout_begin_offset;
                vlayout.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                vlayout.InstanceDataStepRate = 0;

                switch (shader->m_vertex_in[i])
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
                case jegl_shader::uniform_type::COLOR:
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
            shader->m_fragment_hlsl_src,
            strlen(shader->m_fragment_hlsl_src),
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
            fragment_blob.GetAddressOf(),
            &error_blob);

        if (FAILED(compile_result))
        {
            shader_load_failed = true;
            error_informations += "In fragment shader: \n";
            if (error_blob != nullptr)
            {
                error_informations += static_cast<const char*>(error_blob->GetBufferPointer());
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
                shader, error_informations.c_str());
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

            auto* uniforms = shader->m_custom_uniforms;
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
                case jegl_shader::uniform_type::COLOR:
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
                static_cast<size_t>(shader->m_cull_mode)];
            blob->m_rasterizer_r2b = context->m_rasterizers_r2b[
                static_cast<size_t>(shader->m_cull_mode)];

            D3D11_DEPTH_STENCIL_DESC depth_describe;
            depth_describe.DepthEnable = TRUE;
            switch (shader->m_depth_test)
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
            switch (shader->m_depth_mask)
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
            JERCHECK(context->m_dx_device->CreateDepthStencilState(
                &depth_describe, blob->m_depth.GetAddressOf()));

            JEDX11_TRACE_DEBUG_NAME(blob->m_depth,
                string_path + "_DepthStencilState");

            D3D11_BLEND_DESC blend_describe = {};
            blend_describe.AlphaToCoverageEnable = FALSE;
            // OpenGL咋没这么牛逼的选项呢…… 只能含泪关掉
            blend_describe.IndependentBlendEnable = FALSE;

            blend_describe.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (shader->m_blend_equation == jegl_shader::blend_equation::DISABLED)
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
                        shader->m_blend_equation);

                blend_describe.RenderTarget[0].SrcBlend
                    = blend_describe.RenderTarget[0].SrcBlendAlpha
                    = parse_dx11_enum_blend_method(shader->m_blend_src_mode);
                blend_describe.RenderTarget[0].DestBlend
                    = blend_describe.RenderTarget[0].DestBlendAlpha
                    = parse_dx11_enum_blend_method(shader->m_blend_dst_mode);
            }
            JERCHECK(context->m_dx_device->CreateBlendState(
                &blend_describe, blob->m_blend.GetAddressOf()));

            JEDX11_TRACE_DEBUG_NAME(blob->m_blend,
                string_path + "_BlendState");

            blob->m_samplers.resize(shader->m_sampler_count);
            for (size_t i = 0; i < shader->m_sampler_count; ++i)
            {
                auto& dxsampler = blob->m_samplers[i];
                auto& sampler = shader->m_sampler_methods[i];
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
                        if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
                            sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                        else
                            sampler_describe.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    }
                    else
                    {
                        if (sampler.m_mip == jegl_shader::fliter_mode::LINEAR)
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
            }
            return blob;
        }

        return nullptr;
    }
    void dx11_shader_close_blob(
        je_GraphicImplContext ctx,
        jegl_resource_blob blob)
    {
        delete static_cast<dx11_resource_shader_blob*>(blob);
    }

    template<typename TexT>
    jegl_resource_blob dx11_texture_create_blob(
        je_GraphicImplContext,
        TexT*)
    {
        return nullptr;
    }
    void dx11_texture_close_blob(
        je_GraphicImplContext,
        jegl_resource_blob)
    {
        // No need to do anything.
    }

    jegl_resource_blob dx11_vertex_create_blob(
        je_GraphicImplContext,
        jegl_vertex*)
    {
        return nullptr;
    }
    void dx11_vertex_close_blob(
        je_GraphicImplContext,
        jegl_resource_blob)
    {
        // No need to do anything.
    }

    void dx11_shader_init(
        je_GraphicImplContext ctx,
        jegl_resource_blob blob,
        jegl_shader* shader)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);
        if (blob != nullptr)
        {
            auto* shader_blob =
                static_cast<dx11_resource_shader_blob*>(blob);

            jedx11_shader* jedx11_shader_res = new jedx11_shader;

            auto& builtin_uniforms = shader->m_builtin_uniforms;

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
            builtin_uniforms.m_builtin_uniform_shadow_ellipse =
                shader_blob->get_built_in_location("JE_SHADOW_ELLIPSE");

            // ATTENTION: 注意，以下参数特殊shader可能挪作他用
            builtin_uniforms.m_builtin_uniform_local_scale =
                shader_blob->get_built_in_location("JE_LOCAL_SCALE");
            builtin_uniforms.m_builtin_uniform_color =
                shader_blob->get_built_in_location("JE_COLOR");

            auto* uniforms = shader->m_custom_uniforms;
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
            }

            jedx11_shader_res->m_vao = shader_blob->m_vao;
            jedx11_shader_res->m_vertex = shader_blob->m_vertex;
            jedx11_shader_res->m_fragment = shader_blob->m_fragment;

            jedx11_shader_res->m_rasterizer = shader_blob->m_rasterizer;
            jedx11_shader_res->m_rasterizer_r2b = shader_blob->m_rasterizer_r2b;
            jedx11_shader_res->m_depth = shader_blob->m_depth;
            jedx11_shader_res->m_blend = shader_blob->m_blend;

            jedx11_shader_res->m_samplers = shader_blob->m_samplers;

            shader->m_handle.m_ptr = jedx11_shader_res;
        }
        else
        {
            shader->m_handle.m_ptr = nullptr;
        }
    }
    void dx11_shader_update(
        je_GraphicImplContext,
        jegl_shader*)
    {
        // No need to do anything.
    }
    void dx11_shader_close(
        je_GraphicImplContext,
        jegl_shader* shader)
    {
        auto* dx11_shader = static_cast<jedx11_shader*>(shader->m_handle.m_ptr);
        if (dx11_shader != nullptr)
            delete dx11_shader;
    }

    template<typename TexT>
    static DXGI_FORMAT _dx11_pick_texture_format(const TexT* resource)
    {
        const bool float16 =
            0 != (resource->m_format & jegl_texture_common::format::FLOAT16);
        const bool srgb =
            0 != (resource->m_format & jegl_texture_common::format::SRGB);

        switch (resource->m_format & jegl_texture_common::format::COLOR_DEPTH_MASK)
        {
        case jegl_texture_common::format::MONO:
            return float16 ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R8_UNORM;
        case jegl_texture_common::format::RGBA:
            return float16
                ? DXGI_FORMAT_R16G16B16A16_FLOAT
                : (srgb
                    ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                    : DXGI_FORMAT_R8G8B8A8_UNORM);
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    template<typename TexT>
    jedx11_texture* dx11_create_texture_instance(jegl_dx11_context* context, TexT* resource, bool is_dynamic)
    {
        constexpr bool is_3d = std::is_same_v<TexT, jegl_texture_3d>;
        constexpr bool is_array = std::is_same_v<TexT, jegl_texture_array>;
        constexpr bool is_cube = std::is_same_v<TexT, jegl_texture_cube>;
        constexpr bool is_msaa = std::is_same_v<TexT, jegl_texture_msaa>;

        const bool is_framebuf =
            0 != (resource->m_format & jegl_texture_common::format::FRAMEBUF);
        const bool is_depth =
            0 != (resource->m_format & jegl_texture_common::format::DEPTH);

        DXGI_FORMAT texture_format = _dx11_pick_texture_format(resource);

        // MSAA 纹理只存在于 GPU 侧，且 FRAMEBUF 纹理必须是 DEFAULT usage
        // （不能是 DYNAMIC/IMMUTABLE），因为它们需要绑定为 render target 或
        // depth stencil；同时 CPUAccessFlags 必须为 0，否则与 DEPTH_STENCIL
        // 绑定标志冲突。
        const bool default_usage = is_msaa || is_framebuf;
        const D3D11_USAGE usage =
            default_usage ? D3D11_USAGE_DEFAULT
            : (is_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE);
        const UINT cpu_access = default_usage ? 0 : (is_dynamic ? D3D11_CPU_ACCESS_WRITE : 0);

        jedx11_texture* jedx11_texture_res =
            (is_dynamic && !default_usage) ? new jedx11_modifiable_texture : new jedx11_texture;

        // All texture is unmodifiable as default.
        jedx11_texture_res->m_modifiable_texture_buffer = (is_dynamic && !default_usage);

        UINT bind_flags = D3D11_BIND_SHADER_RESOURCE;
        UINT misc_flags = 0;
        UINT sample_count = 1;

        if constexpr (is_cube)
            misc_flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        if constexpr (is_msaa)
            sample_count = (UINT)resource->m_samples;

        if (is_depth)
        {
            texture_format = DXGI_FORMAT_R24G8_TYPELESS;
            bind_flags |= D3D11_BIND_DEPTH_STENCIL;
        }
        else if (is_framebuf)
        {
            bind_flags |= D3D11_BIND_RENDER_TARGET;
        }

        D3D11_SUBRESOURCE_DATA texture_sub_data;
        D3D11_SUBRESOURCE_DATA* texture_sub_data_ptr = nullptr;
        // 2D 数组/立方体贴图的初始数据要求每个数组切片一个条目
        //（子资源数 = MipLevels × ArraySize）；SysMemSlicePitch 仅适用
        // 于 3D 纹理。漏配会让运行时越界读取条目数组导致崩溃。
        std::vector<D3D11_SUBRESOURCE_DATA> texture_sub_data_array;

        if (!default_usage && resource->m_pixels != nullptr)
        {
            const UINT bytes_per_row =
                (UINT)resource->m_width
                * ((UINT)resource->m_format
                    & jegl_texture_common::format::COLOR_DEPTH_MASK)
                * ((resource->m_format & jegl_texture_common::format::FLOAT16) ? 2 : 1);
            const UINT bytes_per_slice = bytes_per_row * (UINT)resource->m_height;

            if constexpr (is_array || is_cube)
            {
                UINT slice_count = 0;
                if constexpr (is_array)
                    slice_count = (UINT)resource->m_layers;
                else
                    slice_count = 6;
                texture_sub_data_array.resize(slice_count);
                for (UINT s = 0; s < slice_count; ++s)
                {
                    texture_sub_data_array[s].pSysMem =
                        resource->m_pixels + (size_t)s * bytes_per_slice;
                    texture_sub_data_array[s].SysMemPitch = bytes_per_row;
                    texture_sub_data_array[s].SysMemSlicePitch = 0;
                }
                texture_sub_data_ptr = texture_sub_data_array.data();
            }
            else
            {
                texture_sub_data.pSysMem = resource->m_pixels;
                texture_sub_data.SysMemPitch = bytes_per_row;
                texture_sub_data.SysMemSlicePitch =
                    is_3d ? bytes_per_slice : 0u;
                texture_sub_data_ptr = &texture_sub_data;
            }
        }

        assert(DXGI_FORMAT_UNKNOWN != texture_format);

        if constexpr (is_3d)
        {
            D3D11_TEXTURE3D_DESC texture_describe;
            texture_describe.Width = (UINT)resource->m_width;
            texture_describe.Height = (UINT)resource->m_height;
            texture_describe.Depth = (UINT)resource->m_depth;
            texture_describe.MipLevels = 1;
            texture_describe.Format = texture_format;
            texture_describe.Usage = usage;
            texture_describe.BindFlags = bind_flags;
            texture_describe.CPUAccessFlags = cpu_access;
            texture_describe.MiscFlags = misc_flags;

            JERCHECK(context->m_dx_device->CreateTexture3D(
                &texture_describe, texture_sub_data_ptr,
                jedx11_texture_res->m_texture_3d.GetAddressOf()));
        }
        else
        {
            D3D11_TEXTURE2D_DESC texture_describe;
            texture_describe.Width = (UINT)resource->m_width;
            texture_describe.Height = (UINT)resource->m_height;
            texture_describe.MipLevels = 1;

            if constexpr (is_array)
                texture_describe.ArraySize = (UINT)resource->m_layers;
            else if constexpr (is_cube)
                texture_describe.ArraySize = 6;
            else
                texture_describe.ArraySize = 1;

            texture_describe.Format = texture_format;
            texture_describe.SampleDesc.Count = sample_count;
            texture_describe.SampleDesc.Quality = 0;
            texture_describe.Usage = usage;
            texture_describe.BindFlags = bind_flags;
            texture_describe.CPUAccessFlags = cpu_access;
            texture_describe.MiscFlags = misc_flags;

            JERCHECK(context->m_dx_device->CreateTexture2D(
                &texture_describe, texture_sub_data_ptr,
                jedx11_texture_res->m_texture.GetAddressOf()));
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC texture_shader_view_describe;
        texture_shader_view_describe.Format = is_depth
            ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS
            : texture_format;

        if constexpr (is_3d)
        {
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            texture_shader_view_describe.Texture3D.MostDetailedMip = 0;
            texture_shader_view_describe.Texture3D.MipLevels = 1;
        }
        else if constexpr (is_array)
        {
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            texture_shader_view_describe.Texture2DArray.MostDetailedMip = 0;
            texture_shader_view_describe.Texture2DArray.MipLevels = 1;
            texture_shader_view_describe.Texture2DArray.FirstArraySlice = 0;
            texture_shader_view_describe.Texture2DArray.ArraySize = (UINT)resource->m_layers;
        }
        else if constexpr (is_cube)
        {
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            texture_shader_view_describe.TextureCube.MostDetailedMip = 0;
            texture_shader_view_describe.TextureCube.MipLevels = 1;
        }
        else if constexpr (is_msaa)
        {
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        }
        else
        {
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            texture_shader_view_describe.Texture2D.MipLevels = 1;
            texture_shader_view_describe.Texture2D.MostDetailedMip = 0;
        }

        assert(DXGI_FORMAT_UNKNOWN != texture_shader_view_describe.Format);

        JERCHECK(context->m_dx_device->CreateShaderResourceView(
            jedx11_texture_res->texture_resource(),
            &texture_shader_view_describe,
            jedx11_texture_res->m_texture_view.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(jedx11_texture_res->m_texture_view,
            std::string(
                resource->m_handle.m_path_may_null_if_builtin != nullptr
                ? resource->m_handle.m_path_may_null_if_builtin
                : "_builtin_texture_") + "_View");

        return jedx11_texture_res;
    }

    template<typename TexT>
    void dx11_texture_init(
        je_GraphicImplContext ctx,
        jegl_resource_blob,
        TexT* resource)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);
        resource->m_handle.m_ptr = dx11_create_texture_instance(
            context, resource, false /* Immutable as default */);
    }

    template<typename TexT>
    void dx11_texture_update(
        je_GraphicImplContext ctx,
        TexT* resource)
    {
        if constexpr (std::is_same_v<TexT, jegl_texture_msaa>)
        {
            (void)resource;
            // NOTE: MSAA 纹理只存在于 GPU 侧，不支持 CPU 侧像素更新
            jeecs::debug::logerr("MSAA texture update is not supported.");
        }
        else
        {
            jegl_dx11_context* context =
                static_cast<jegl_dx11_context*>(ctx);
            jedx11_texture* texture_instance =
                static_cast<jedx11_texture*>(resource->m_handle.m_ptr);

            if (texture_instance->m_modifiable_texture_buffer)
            {
                const bool is_float16 =
                    0 != (resource->m_format & jegl_texture_common::format::FLOAT16);

                const size_t row_byte_size =
                    resource->m_width
                    * (resource->m_format & jegl_texture_common::format::COLOR_DEPTH_MASK)
                    * (is_float16 ? 2 : 1);

                constexpr bool is_3d = std::is_same_v<TexT, jegl_texture_3d>;

                if constexpr (is_3d)
                {
                    // 3D 纹理：单一子资源，映射后经 DepthPitch 寻址深度
                    D3D11_MAPPED_SUBRESOURCE mappedData;
                    JERCHECK(context->m_dx_context->Map(
                        texture_instance->texture_resource(), 0,
                        D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

                    for (size_t z = 0; z < resource->m_depth; ++z)
                    {
                        for (size_t i = 0; i < resource->m_height; ++i)
                        {
                            void* dst_row_data = (void*)(
                                (intptr_t)mappedData.pData
                                + z * mappedData.DepthPitch
                                + i * mappedData.RowPitch);
                            const void* src_row_data =
                                resource->m_pixels
                                + (z * resource->m_height + i) * row_byte_size;

                            memcpy(dst_row_data, src_row_data, row_byte_size);
                        }
                    }
                    context->m_dx_context->Unmap(
                        texture_instance->texture_resource(), 0);
                }
                else
                {
                    // 2D 数组/立方体贴图：每个数组切片是独立子资源，
                    // 需逐层 Map（与创建时逐切片初始数据同一子资源语义）
                    size_t slice_count = 1;
                    if constexpr (std::is_same_v<TexT, jegl_texture_array>)
                        slice_count = resource->m_layers;
                    else if constexpr (std::is_same_v<TexT, jegl_texture_cube>)
                        slice_count = 6;

                    // mappedData is not byte-aligned, we need to copy it byte by row
                    for (size_t slice = 0; slice < slice_count; ++slice)
                    {
                        D3D11_MAPPED_SUBRESOURCE mappedData;
                        JERCHECK(context->m_dx_context->Map(
                            texture_instance->texture_resource(), (UINT)slice,
                            D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

                        for (size_t i = 0; i < resource->m_height; ++i)
                        {
                            void* dst_row_data = (void*)(
                                (intptr_t)mappedData.pData
                                + i * mappedData.RowPitch);
                            const void* src_row_data =
                                resource->m_pixels
                                + (slice * resource->m_height + i) * row_byte_size;

                            memcpy(dst_row_data, src_row_data, row_byte_size);
                        }
                        context->m_dx_context->Unmap(
                            texture_instance->texture_resource(), (UINT)slice);
                    }
                }
            }
            else
            {
                // This texture is immutable, we need to recreate it as dynamic
                jedx11_modifiable_texture* modifiable_texture_instance =
                    static_cast<jedx11_modifiable_texture*>(
                        dx11_create_texture_instance(
                            context, resource, true /* Regenerate it as dynamic */));

                modifiable_texture_instance->m_obsoluted_texture = texture_instance;
                resource->m_handle.m_ptr = modifiable_texture_instance;
            }
        }
    }

    template<typename TexT>
    void dx11_texture_close(
        je_GraphicImplContext,
        TexT* resource)
    {
        jedx11_texture* texture_instance =
            static_cast<jedx11_texture*>(resource->m_handle.m_ptr);

        if (texture_instance->m_modifiable_texture_buffer)
        {
            auto* dynamic_texture_instance = static_cast<jedx11_modifiable_texture*>(texture_instance);
            delete dynamic_texture_instance->m_obsoluted_texture;
            delete dynamic_texture_instance;
        }
        else
            delete texture_instance;
    }

    jedx11_vertex* dx11_create_vertex_instance(
        jegl_dx11_context* context, jegl_vertex* resource, bool is_dynamic)
    {
        jedx11_vertex* vertex =
            is_dynamic ? new jedx11_modifiable_vertex : new jedx11_vertex;

        const static D3D_PRIMITIVE_TOPOLOGY DRAW_METHODS[] = {
            D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        };
        vertex->m_method =
            DRAW_METHODS[resource->m_type];

        vertex->m_count = (UINT)resource->m_index_count;
        vertex->m_stride = resource->m_data_size_per_point;

        // 与纹理相同策略：默认 IMMUTABLE，首次更新时重建为 DYNAMIC，
        // 之后通过 Map/WRITE_DISCARD 重写，避免 UpdateSubresource 中转拷贝
        vertex->m_modifiable_vertex_buffer = is_dynamic;

        // 新建顶点缓冲区
        D3D11_BUFFER_DESC vertex_buffer_describe;
        vertex_buffer_describe.ByteWidth =
            (UINT)resource->m_vertex_length;

        vertex_buffer_describe.Usage =
            is_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
        vertex_buffer_describe.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buffer_describe.CPUAccessFlags =
            is_dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        vertex_buffer_describe.MiscFlags = 0;
        vertex_buffer_describe.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vertex_buffer_data;
        vertex_buffer_data.pSysMem = resource->m_vertexs;
        vertex_buffer_data.SysMemPitch = 0;
        vertex_buffer_data.SysMemSlicePitch = 0;
        JERCHECK(context->m_dx_device->CreateBuffer(
            &vertex_buffer_describe,
            &vertex_buffer_data,
            vertex->m_vbo.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(vertex->m_vbo,
            std::string(
                resource->m_handle.m_path_may_null_if_builtin != nullptr
                ? resource->m_handle.m_path_may_null_if_builtin
                : "_builtin_vertex_") + "_Vbo");

        // 新建索引缓冲区
        static_assert(sizeof(uint32_t) == sizeof(UINT));

        // 按分配容量创建（m_index_count 可能已被 jegl_update_vertex_buffer
        // 收缩为激活数量——D3D11 不允许 ByteWidth 为 0；绘制数量另存于
        // vertex->m_count）
        D3D11_BUFFER_DESC index_buffer_describe;
        index_buffer_describe.ByteWidth =
            (UINT)resource->m_index_capacity * sizeof(uint32_t);
        index_buffer_describe.Usage = D3D11_USAGE_IMMUTABLE;
        index_buffer_describe.BindFlags = D3D11_BIND_INDEX_BUFFER;
        index_buffer_describe.CPUAccessFlags = 0;
        index_buffer_describe.MiscFlags = 0;
        index_buffer_describe.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA index_buffer_data;
        index_buffer_data.pSysMem = resource->m_indices;
        index_buffer_data.SysMemPitch = 0;
        index_buffer_data.SysMemSlicePitch = 0;
        JERCHECK(context->m_dx_device->CreateBuffer(
            &index_buffer_describe,
            &index_buffer_data,
            vertex->m_ebo.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(vertex->m_ebo,
            std::string(
                resource->m_handle.m_path_may_null_if_builtin != nullptr
                ? resource->m_handle.m_path_may_null_if_builtin
                : "_builtin_vertex_") + "_Ebo");

        return vertex;
    }
    void dx11_vertex_init(
        je_GraphicImplContext ctx,
        jegl_resource_blob,
        jegl_vertex* resource)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);
        resource->m_handle.m_ptr = dx11_create_vertex_instance(
            context, resource, false /* Immutable as default */);
    }
    void dx11_vertex_update(
        je_GraphicImplContext ctx,
        jegl_vertex* resource)
    {
        jegl_dx11_context* context =
            static_cast<jegl_dx11_context*>(ctx);
        jedx11_vertex* vertex_instance =
            static_cast<jedx11_vertex*>(resource->m_handle.m_ptr);

        if (vertex_instance->m_modifiable_vertex_buffer)
        {
            // 整体重写顶点数据（索引缓冲内容不变），并同步激活索引数量
            D3D11_MAPPED_SUBRESOURCE mappedData;
            JERCHECK(context->m_dx_context->Map(
                vertex_instance->m_vbo.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

            memcpy(mappedData.pData, resource->m_vertexs, resource->m_vertex_length);
            context->m_dx_context->Unmap(vertex_instance->m_vbo.Get(), 0);

            vertex_instance->m_count = (UINT)resource->m_index_count;
        }
        else
        {
            // This vertex buffer is immutable, we need to recreate it as dynamic
            jedx11_modifiable_vertex* modifiable_vertex_instance =
                static_cast<jedx11_modifiable_vertex*>(
                    dx11_create_vertex_instance(
                        context, resource, true /* Regenerate it as dynamic */));

            modifiable_vertex_instance->m_obsoluted_vertex = vertex_instance;
            resource->m_handle.m_ptr = modifiable_vertex_instance;
        }
    }
    void dx11_vertex_close(
        je_GraphicImplContext,
        jegl_vertex* resource)
    {
        jedx11_vertex* vertex_instance =
            static_cast<jedx11_vertex*>(resource->m_handle.m_ptr);

        if (vertex_instance->m_modifiable_vertex_buffer)
        {
            auto* dynamic_vertex_instance =
                static_cast<jedx11_modifiable_vertex*>(vertex_instance);
            delete dynamic_vertex_instance->m_obsoluted_vertex;
            delete dynamic_vertex_instance;
        }
        else
            delete vertex_instance;
    }

    void dx11_framebuf_init(
        je_GraphicImplContext ctx,
        jegl_frame_buffer* resource)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);
        jedx11_framebuffer* jedx11_framebuffer_res =
            new jedx11_framebuffer(
                resource->m_width,
                resource->m_height);

        jeecs::basic::resource<jeecs::graphic::texture2d>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture2d> *>(
                resource->m_output_attachments);

        size_t color_attachment_count = 0;
        for (size_t i = 0; i < resource->m_attachment_count; ++i)
        {
            auto& attachment = attachments[i];
            if (0 == (
                attachment->resource()->m_format
                & jegl_texture_common::format::DEPTH))
                ++color_attachment_count;
        }

        jedx11_framebuffer_res->m_rend_views.resize(color_attachment_count);
        color_attachment_count = 0;

        for (size_t i = 0; i < resource->m_attachment_count; ++i)
        {
            auto& attachment = attachments[i];
            (void)_jegl_prepare_texture_2d_resource(attachment->resource());

            if (0 != (
                attachment->resource()->m_format
                & jegl_texture_common::format::DEPTH))
            {
                if (jedx11_framebuffer_res->m_depth_view.Get() == nullptr)
                {
                    D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_describe;
                    depth_view_describe.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                    depth_view_describe.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                    depth_view_describe.Texture2D.MipSlice = 0;
                    depth_view_describe.Flags = 0;

                    JERCHECK(context->m_dx_device->CreateDepthStencilView(
                            static_cast<jedx11_texture*>(
                                attachment->resource()->m_handle.m_ptr)->m_texture.Get(),
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
                        static_cast<jedx11_texture*>(
                            attachment->resource()->m_handle.m_ptr)->m_texture.Get(),
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
    }
    void dx11_framebuf_update(
        je_GraphicImplContext,
        jegl_frame_buffer*)
    {
        // No need to do anything.
    }
    void dx11_framebuf_close(
        je_GraphicImplContext,
        jegl_frame_buffer* resource)
    {
        delete static_cast<jedx11_framebuffer*>(resource->m_handle.m_ptr);
    }

    void dx11_ubuffer_init(
        je_GraphicImplContext ctx,
        jegl_uniform_buffer* resource)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);
        auto* jedx11_uniformbuf_res = new jedx11_uniformbuf;

        D3D11_BUFFER_DESC const_buffer_describe;

        const_buffer_describe.ByteWidth =
            (UINT)resource->m_buffer_size;
        const_buffer_describe.Usage = D3D11_USAGE_DYNAMIC;
        const_buffer_describe.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        const_buffer_describe.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        const_buffer_describe.MiscFlags = 0;
        const_buffer_describe.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA const_buffer_resource_describe;
        const_buffer_resource_describe.pSysMem =
            resource->m_buffer;
        const_buffer_resource_describe.SysMemPitch = 0;
        const_buffer_resource_describe.SysMemSlicePitch = 0;

        // 新建常量缓冲区，不使用初始数据
        JERCHECK(context->m_dx_device->CreateBuffer(
            &const_buffer_describe, &const_buffer_resource_describe,
            jedx11_uniformbuf_res->m_uniformbuf.GetAddressOf()));

        jedx11_uniformbuf_res->m_binding_place =
            // 0 has been used as shader uniforms
            (UINT)resource->m_buffer_binding_place + 1;

        resource->m_handle.m_ptr = jedx11_uniformbuf_res;
    }
    void dx11_ubuffer_update(
        je_GraphicImplContext ctx,
        jegl_uniform_buffer* resource)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

        auto* uniformbuf_instance = static_cast<jedx11_uniformbuf*>(resource->m_handle.m_ptr);
        assert(resource->m_update_length != 0);
        D3D11_MAPPED_SUBRESOURCE mappedData;
        JERCHECK(context->m_dx_context->Map(
            uniformbuf_instance->m_uniformbuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

        // DX11 Donot support partial update, update all.
        memcpy((void*)((intptr_t)mappedData.pData),
            resource->m_buffer,
            resource->m_buffer_size);

        context->m_dx_context->Unmap(uniformbuf_instance->m_uniformbuf.Get(), 0);
    }
    void dx11_ubuffer_close(
        je_GraphicImplContext,
        jegl_uniform_buffer* resource)
    {
        delete static_cast<jedx11_uniformbuf*>(resource->m_handle.m_ptr);
    }

                void dx11_bind_uniform_buffer(je_GraphicImplContext ctx, jegl_uniform_buffer* uniformbuf)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

        auto* uniformbuf_instance = static_cast<jedx11_uniformbuf*>(uniformbuf->m_handle.m_ptr);
        context->m_dx_context->VSSetConstantBuffers(
            uniformbuf_instance->m_binding_place, 1, uniformbuf_instance->m_uniformbuf.GetAddressOf());
        context->m_dx_context->PSSetConstantBuffers(
            uniformbuf_instance->m_binding_place, 1, uniformbuf_instance->m_uniformbuf.GetAddressOf());
    }

    template<typename TexT>
    void dx11_bind_texture(je_GraphicImplContext ctx, TexT* texture, size_t pass)
    {
        jegl_dx11_context* context = static_cast<jegl_dx11_context*>(ctx);

        auto* texture_instance = static_cast<jedx11_texture*>(texture->m_handle.m_ptr);
        if (texture_instance->m_texture_view.Get() != nullptr)
        {
            context->m_dx_context->VSSetShaderResources(
                (UINT)pass, 1, texture_instance->m_texture_view.GetAddressOf());
            context->m_dx_context->PSSetShaderResources(
                (UINT)pass, 1, texture_instance->m_texture_view.GetAddressOf());
        }
    }

            /////////////////////////////////////////////////////////////////////////
    // Render API (v2)
    /////////////////////////////////////////////////////////////////////////

    struct jegl_dx11_render_pipeline
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilState> m_depth;
        jegl_dx11_context::MSWRLComPtr<ID3D11BlendState> m_blend;
    };

    struct jegl_dx11_render_sampler
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11SamplerState> m_sampler;
    };

    D3D11_COMPARISON_FUNC dx11_render_depth_func(
        jegl_shader::depth_test_method method)
    {
        switch (method)
        {
        case jegl_shader::depth_test_method::NEVER: return D3D11_COMPARISON_NEVER;
        case jegl_shader::depth_test_method::LESS: return D3D11_COMPARISON_LESS;
        case jegl_shader::depth_test_method::EQUAL: return D3D11_COMPARISON_EQUAL;
        case jegl_shader::depth_test_method::LESS_EQUAL: return D3D11_COMPARISON_LESS_EQUAL;
        case jegl_shader::depth_test_method::GREATER: return D3D11_COMPARISON_GREATER;
        case jegl_shader::depth_test_method::NOT_EQUAL: return D3D11_COMPARISON_NOT_EQUAL;
        case jegl_shader::depth_test_method::GREATER_EQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
        case jegl_shader::depth_test_method::ALWAYS: return D3D11_COMPARISON_ALWAYS;
        default:
            assert(false);
            return D3D11_COMPARISON_LESS;
        }
    }

    D3D11_BLEND_OP dx11_render_blend_op(
        jegl_shader::blend_equation equation)
    {
        switch (equation)
        {
        case jegl_shader::blend_equation::ADD: return D3D11_BLEND_OP_ADD;
        case jegl_shader::blend_equation::SUBTRACT: return D3D11_BLEND_OP_SUBTRACT;
        case jegl_shader::blend_equation::REVERSE_SUBTRACT: return D3D11_BLEND_OP_REV_SUBTRACT;
        case jegl_shader::blend_equation::MIN: return D3D11_BLEND_OP_MIN;
        case jegl_shader::blend_equation::MAX: return D3D11_BLEND_OP_MAX;
        default:
            assert(false);
            return D3D11_BLEND_OP_ADD;
        }
    }

    D3D11_BLEND dx11_render_blend_factor(
        jegl_shader::blend_method method)
    {
        switch (method)
        {
        case jegl_shader::blend_method::ZERO: return D3D11_BLEND_ZERO;
        case jegl_shader::blend_method::ONE: return D3D11_BLEND_ONE;
        case jegl_shader::blend_method::SRC_COLOR: return D3D11_BLEND_SRC_COLOR;
        case jegl_shader::blend_method::SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
        case jegl_shader::blend_method::DST_COLOR: return D3D11_BLEND_DEST_COLOR;
        case jegl_shader::blend_method::DST_ALPHA: return D3D11_BLEND_DEST_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_DST_ALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
        case jegl_shader::blend_method::ONE_MINUS_DST_COLOR: return D3D11_BLEND_INV_DEST_COLOR;
        default:
            assert(false);
            return D3D11_BLEND_ONE;
        }
    }

    D3D11_CULL_MODE dx11_render_cull_mode(jegl_shader::cull_mode mode)
    {
        switch (mode)
        {
        case jegl_shader::cull_mode::NONE: return D3D11_CULL_NONE;
        case jegl_shader::cull_mode::FRONT: return D3D11_CULL_FRONT;
        case jegl_shader::cull_mode::BACK: return D3D11_CULL_BACK;
        default:
            assert(false);
            return D3D11_CULL_NONE;
        }
    }

    void dx11_render_pipeline_init(je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* shader_instance = static_cast<jedx11_shader*>(
            pipeline->m_shader->m_handle.m_ptr);

        assert(shader_instance != nullptr);

        auto* dxpipeline = new jegl_dx11_render_pipeline();

        D3D11_DEPTH_STENCIL_DESC depth_desc = {};
        depth_desc.DepthEnable = pipeline->m_state.m_depth_test ? TRUE : FALSE;
        depth_desc.DepthWriteMask = pipeline->m_state.m_depth_write
            ? D3D11_DEPTH_WRITE_MASK_ALL
            : D3D11_DEPTH_WRITE_MASK_ZERO;
        depth_desc.DepthFunc = dx11_render_depth_func(pipeline->m_state.m_depth_func);
        depth_desc.StencilEnable = FALSE;
        depth_desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
        depth_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
        depth_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depth_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depth_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        depth_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        depth_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depth_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depth_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depth_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

        JERCHECK(context->m_dx_device->CreateDepthStencilState(
            &depth_desc, dxpipeline->m_depth.GetAddressOf()));

        D3D11_BLEND_DESC blend_desc = {};
        blend_desc.AlphaToCoverageEnable = FALSE;
        blend_desc.IndependentBlendEnable = FALSE;

        auto& render_target = blend_desc.RenderTarget[0];
        render_target.RenderTargetWriteMask =
            pipeline->m_state.m_color_write_mask & 0x0F;
        render_target.BlendEnable = pipeline->m_state.m_blend ? TRUE : FALSE;
        render_target.BlendOp = pipeline->m_state.m_blend
            ? dx11_render_blend_op(pipeline->m_state.m_blend_equation)
            : D3D11_BLEND_OP_ADD;
        render_target.BlendOpAlpha = render_target.BlendOp;
        render_target.SrcBlend = dx11_render_blend_factor(pipeline->m_state.m_blend_src_color);
        render_target.DestBlend = dx11_render_blend_factor(pipeline->m_state.m_blend_dst_color);
        render_target.SrcBlendAlpha = dx11_render_blend_factor(pipeline->m_state.m_blend_src_alpha);
        render_target.DestBlendAlpha = dx11_render_blend_factor(pipeline->m_state.m_blend_dst_alpha);

        JERCHECK(context->m_dx_device->CreateBlendState(
            &blend_desc, dxpipeline->m_blend.GetAddressOf()));

        pipeline->m_backend_ptr = dxpipeline;
    }

    void dx11_render_pipeline_close(je_GraphicImplContext, jegl_pipeline* pipeline)
    {
        delete static_cast<jegl_dx11_render_pipeline*>(pipeline->m_backend_ptr);
        pipeline->m_backend_ptr = nullptr;
    }

    void dx11_render_pipeline_bind(je_GraphicImplContext ctx, jegl_pipeline* pipeline)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* shader_instance = static_cast<jedx11_shader*>(
            pipeline->m_shader->m_handle.m_ptr);
        auto* dxpipeline = static_cast<jegl_dx11_render_pipeline*>(
            pipeline->m_backend_ptr);

        assert(shader_instance != nullptr);
        assert(dxpipeline != nullptr);

        context->m_current_target_shader = shader_instance;

        context->m_dx_context->VSSetShader(shader_instance->m_vertex.Get(), nullptr, 0);
        context->m_dx_context->PSSetShader(shader_instance->m_fragment.Get(), nullptr, 0);
        context->m_dx_context->IASetInputLayout(shader_instance->m_vao.Get());

        const size_t cull_index =
            static_cast<size_t>(pipeline->m_state.m_cull);
        assert(cull_index < 3);

        context->m_dx_context->RSSetState(
            (context->m_current_target_framebuffer == nullptr
                ? context->m_rasterizers[cull_index]
                : context->m_rasterizers_r2b[cull_index]).Get());

        float blend_factor[4] = {};
        context->m_dx_context->OMSetBlendState(
            dxpipeline->m_blend.Get(), blend_factor, 0xFFFFFFFF);
        context->m_dx_context->OMSetDepthStencilState(
            dxpipeline->m_depth.Get(), 0);

        for (auto& sampler : shader_instance->m_samplers)
        {
            context->m_dx_context->VSSetSamplers(
                sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
            context->m_dx_context->PSSetSamplers(
                sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
        }
    }

    void dx11_render_sampler_init(je_GraphicImplContext ctx, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* native = new jegl_dx11_render_sampler();

        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = (sampler->m_desc.m_min_filter == jegl_filter::LINEAR
            ? (sampler->m_desc.m_mag_filter == jegl_filter::LINEAR
                ? (sampler->m_desc.m_mip_filter == jegl_mip_filter::LINEAR
                    ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
                    : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT)
                : (sampler->m_desc.m_mip_filter == jegl_mip_filter::LINEAR
                    ? D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                    : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT))
            : (sampler->m_desc.m_mag_filter == jegl_filter::LINEAR
                ? (sampler->m_desc.m_mip_filter == jegl_mip_filter::LINEAR
                    ? D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR
                    : D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT)
                : (sampler->m_desc.m_mip_filter == jegl_mip_filter::LINEAR
                    ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR
                    : D3D11_FILTER_MIN_MAG_MIP_POINT)));

        auto cmp_wrap = [](jegl_wrap_mode wrap) -> D3D11_TEXTURE_ADDRESS_MODE
        {
            switch (wrap)
            {
            case jegl_wrap_mode::CLAMP_TO_EDGE: return D3D11_TEXTURE_ADDRESS_CLAMP;
            case jegl_wrap_mode::REPEAT: return D3D11_TEXTURE_ADDRESS_WRAP;
            case jegl_wrap_mode::MIRRORED_REPEAT: return D3D11_TEXTURE_ADDRESS_MIRROR;
            default:
                assert(false);
                return D3D11_TEXTURE_ADDRESS_CLAMP;
            }
        };

        desc.AddressU = cmp_wrap(sampler->m_desc.m_u_wrap);
        desc.AddressV = cmp_wrap(sampler->m_desc.m_v_wrap);
        desc.AddressW = cmp_wrap(sampler->m_desc.m_w_wrap);
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 0;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.BorderColor[0] = 0.0f;
        desc.BorderColor[1] = 0.0f;
        desc.BorderColor[2] = 0.0f;
        desc.BorderColor[3] = 0.0f;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        JERCHECK(context->m_dx_device->CreateSamplerState(
            &desc, native->m_sampler.GetAddressOf()));

        sampler->m_backend_ptr = native;
    }

    void dx11_render_sampler_close(je_GraphicImplContext, jegl_sampler* sampler)
    {
        delete static_cast<jegl_dx11_render_sampler*>(sampler->m_backend_ptr);
        sampler->m_backend_ptr = nullptr;
    }

    void dx11_render_sampler_bind(
        je_GraphicImplContext ctx, uint32_t sampler_id, jegl_sampler* sampler)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* native = static_cast<jegl_dx11_render_sampler*>(
            sampler->m_backend_ptr);

        assert(native != nullptr);
        context->m_dx_context->VSSetSamplers(
            sampler_id, 1, native->m_sampler.GetAddressOf());
        context->m_dx_context->PSSetSamplers(
            sampler_id, 1, native->m_sampler.GetAddressOf());
    }

    void dx11_render_begin_pass(
        je_GraphicImplContext ctx,
        /* OPTIONAL */ jegl_frame_buffer* target,
        const jegl_viewport* viewport)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);

        context->m_current_target_shader = nullptr;
        context->m_current_vertex = nullptr;

        if (target == nullptr)
        {
            context->m_current_target_framebuffer = nullptr;
            context->m_dx_context->OMSetRenderTargets(
                1,
                context->m_dx_main_renderer_target_view.GetAddressOf(),
                context->m_dx_main_renderer_target_depth_view.Get());
        }
        else
        {
            context->m_current_target_framebuffer =
                static_cast<jedx11_framebuffer*>(target->m_handle.m_ptr);

            context->m_dx_context->OMSetRenderTargets(
                context->m_current_target_framebuffer->m_color_target_count,
                context->m_current_target_framebuffer->m_target_views.data(),
                context->m_current_target_framebuffer->m_target_depth_view_may_null);
        }

        const int32_t buf_h = static_cast<int32_t>(
            context->m_current_target_framebuffer != nullptr
            ? context->m_current_target_framebuffer->m_frame_height
            : context->RESOLUTION_HEIGHT);

        D3D11_VIEWPORT dx_viewport = {};
        dx_viewport.MinDepth = 0.0f;
        dx_viewport.MaxDepth = 1.0f;
        dx_viewport.Width = (float)viewport->width;
        dx_viewport.TopLeftX = (float)viewport->x;
        dx_viewport.Height = (float)viewport->height;
        dx_viewport.TopLeftY =
            (float)(buf_h - (int32_t)viewport->y - (int32_t)viewport->height);

        context->m_dx_context->RSSetViewports(1, &dx_viewport);
    }

    void dx11_render_clear_color(
        je_GraphicImplContext ctx, uint32_t attachment, const float rgba[4])
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);

        if (context->m_current_target_framebuffer == nullptr)
        {
            if (attachment == 0)
                context->m_dx_context->ClearRenderTargetView(
                    context->m_dx_main_renderer_target_view.Get(), rgba);
        }
        else if (attachment
            < context->m_current_target_framebuffer->m_target_views.size())
        {
            context->m_dx_context->ClearRenderTargetView(
                context->m_current_target_framebuffer->m_target_views[attachment],
                rgba);
        }
    }

    void dx11_render_clear_depth(je_GraphicImplContext ctx, float depth)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);

        if (context->m_current_target_framebuffer == nullptr)
            context->m_dx_context->ClearDepthStencilView(
                context->m_dx_main_renderer_target_depth_view.Get(),
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                depth, 0);
        else if (context->m_current_target_framebuffer->m_target_depth_view_may_null != nullptr)
            context->m_dx_context->ClearDepthStencilView(
                context->m_current_target_framebuffer->m_target_depth_view_may_null,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                depth, 0);
    }

    void dx11_render_end_pass(je_GraphicImplContext)
    {}

    void dx11_render_bind_vertex(je_GraphicImplContext ctx, jegl_vertex* vertex)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* vertex_instance = static_cast<jedx11_vertex*>(
            vertex->m_handle.m_ptr);

        assert(vertex_instance != nullptr);
        context->m_current_vertex = vertex_instance;
    }

    void dx11_render_bind_uniform_buffer(
        je_GraphicImplContext ctx, jegl_uniform_buffer* uniform_buffer)
    {
        dx11_bind_uniform_buffer(ctx, uniform_buffer);
    }

    void dx11_render_bind_texture_2d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_2d* texture)
    {
        dx11_bind_texture<jegl_texture_2d>(ctx, texture, pass);
    }

    void dx11_render_bind_texture_3d(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_3d* texture)
    {
        dx11_bind_texture<jegl_texture_3d>(ctx, texture, pass);
    }

    void dx11_render_bind_texture_array(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_array* texture)
    {
        dx11_bind_texture<jegl_texture_array>(ctx, texture, pass);
    }

    void dx11_render_bind_texture_cube(
        je_GraphicImplContext ctx, uint32_t pass, jegl_texture_cube* texture)
    {
        dx11_bind_texture<jegl_texture_cube>(ctx, texture, pass);
    }

    void dx11_render_upload_uniform_block(
        jegl_dx11_context* context,
        jedx11_shader* shader_instance,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        assert(uniform_data != nullptr);
        assert(shader_instance->m_uniform_buffer_size != 0);
        assert(uniform_size == shader_instance->m_uniform_buffer_size);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        JERCHECK(context->m_dx_context->Map(
            shader_instance->m_uniforms.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped));

        memcpy(mapped.pData, uniform_data, uniform_size);

        const uint32_t ndc_offset = shader_instance->m_ndc_scale_uniform_id;
        if (ndc_offset != jeecs::graphic::INVALID_UNIFORM_LOCATION
            && ndc_offset + sizeof(float) * 4 <= uniform_size)
        {
            const float ndc_scale[4] = {
                1.f,
                context->m_current_target_framebuffer == nullptr ? 1.f : -1.f,
                1.f,
                1.f };
            memcpy(
                static_cast<char*>(mapped.pData) + ndc_offset,
                ndc_scale,
                sizeof(ndc_scale));
        }

        context->m_dx_context->Unmap(shader_instance->m_uniforms.Get(), 0);

        context->m_dx_context->VSSetConstantBuffers(
            0, 1, shader_instance->m_uniforms.GetAddressOf());
        context->m_dx_context->PSSetConstantBuffers(
            0, 1, shader_instance->m_uniforms.GetAddressOf());
    }

    void dx11_render_draw_indexed(
        je_GraphicImplContext ctx,
        uint32_t index_count,
        uint32_t first_index,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* shader_instance = context->m_current_target_shader;
        auto* vertex_instance = context->m_current_vertex;

        assert(shader_instance != nullptr);
        assert(vertex_instance != nullptr);

        if (shader_instance->m_uniform_buffer_size != 0)
            dx11_render_upload_uniform_block(
                context, shader_instance, uniform_data, uniform_size);
        else
            assert(uniform_size == 0);

        const UINT offset = 0;
        const UINT stride = vertex_instance->m_stride;

        context->m_dx_context->IASetIndexBuffer(
            vertex_instance->m_ebo.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->m_dx_context->IASetVertexBuffers(
            0, 1, vertex_instance->m_vbo.GetAddressOf(), &stride, &offset);
        context->m_dx_context->IASetPrimitiveTopology(vertex_instance->m_method);

        if (instance_count > 1)
            context->m_dx_context->DrawIndexedInstanced(
                index_count, instance_count, first_index, 0, 0);
        else
            context->m_dx_context->DrawIndexed(
                index_count, first_index, 0);
    }

    void dx11_render_draw(
        je_GraphicImplContext ctx,
        uint32_t vertex_count,
        uint32_t first_vertex,
        uint32_t instance_count,
        const void* uniform_data,
        uint32_t uniform_size)
    {
        auto* context = static_cast<jegl_dx11_context*>(ctx);
        auto* shader_instance = context->m_current_target_shader;
        auto* vertex_instance = context->m_current_vertex;

        assert(shader_instance != nullptr);
        assert(vertex_instance != nullptr);

        if (shader_instance->m_uniform_buffer_size != 0)
            dx11_render_upload_uniform_block(
                context, shader_instance, uniform_data, uniform_size);
        else
            assert(uniform_size == 0);

        const UINT offset = 0;
        const UINT stride = vertex_instance->m_stride;

        context->m_dx_context->IASetVertexBuffers(
            0, 1, vertex_instance->m_vbo.GetAddressOf(), &stride, &offset);
        context->m_dx_context->IASetPrimitiveTopology(vertex_instance->m_method);

        if (instance_count > 1)
            context->m_dx_context->DrawInstanced(
                vertex_count, instance_count, first_vertex, 0);
        else
            context->m_dx_context->Draw(vertex_count, first_vertex);
    }

    void dx11_render_commit_begin(je_GraphicImplContext)
    {}

    void dx11_render_commit_end(je_GraphicImplContext)
    {}
}

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::dx11;

    write_to_apis->interface_startup = dx11_startup;
    write_to_apis->interface_shutdown_before_resource_release = dx11_pre_shutdown;
    write_to_apis->interface_shutdown = dx11_shutdown;

    write_to_apis->update_frame_ready = dx11_pre_update;
    write_to_apis->update_draw_commit = dx11_commit_update;

    write_to_apis->shader_create_blob = dx11_shader_create_blob;
    write_to_apis->texture_2d_create_blob = dx11_texture_create_blob<jegl_texture_2d>;
    write_to_apis->texture_3d_create_blob = dx11_texture_create_blob<jegl_texture_3d>;
    write_to_apis->texture_array_create_blob = dx11_texture_create_blob<jegl_texture_array>;
    write_to_apis->texture_cube_create_blob = dx11_texture_create_blob<jegl_texture_cube>;
    write_to_apis->texture_msaa_create_blob = dx11_texture_create_blob<jegl_texture_msaa>;
    write_to_apis->vertex_create_blob = dx11_vertex_create_blob;

    write_to_apis->shader_close_blob = dx11_shader_close_blob;
    write_to_apis->texture_close_blob = dx11_texture_close_blob;
    write_to_apis->vertex_close_blob = dx11_vertex_close_blob;

    write_to_apis->shader_init = dx11_shader_init;
    write_to_apis->texture_2d_init = dx11_texture_init<jegl_texture_2d>;
    write_to_apis->texture_3d_init = dx11_texture_init<jegl_texture_3d>;
    write_to_apis->texture_array_init = dx11_texture_init<jegl_texture_array>;
    write_to_apis->texture_cube_init = dx11_texture_init<jegl_texture_cube>;
    write_to_apis->texture_msaa_init = dx11_texture_init<jegl_texture_msaa>;
    write_to_apis->vertex_init = dx11_vertex_init;
    write_to_apis->framebuffer_init = dx11_framebuf_init;
    write_to_apis->ubuffer_init = dx11_ubuffer_init;

    write_to_apis->shader_update = dx11_shader_update;
    write_to_apis->texture_2d_update = dx11_texture_update<jegl_texture_2d>;
    write_to_apis->texture_3d_update = dx11_texture_update<jegl_texture_3d>;
    write_to_apis->texture_array_update = dx11_texture_update<jegl_texture_array>;
    write_to_apis->texture_cube_update = dx11_texture_update<jegl_texture_cube>;
    write_to_apis->texture_msaa_update = dx11_texture_update<jegl_texture_msaa>;
    write_to_apis->vertex_update = dx11_vertex_update;
    write_to_apis->framebuffer_update = dx11_framebuf_update;
    write_to_apis->ubuffer_update = dx11_ubuffer_update;

    write_to_apis->shader_close = dx11_shader_close;
    write_to_apis->texture_2d_close = dx11_texture_close<jegl_texture_2d>;
    write_to_apis->texture_3d_close = dx11_texture_close<jegl_texture_3d>;
    write_to_apis->texture_array_close = dx11_texture_close<jegl_texture_array>;
    write_to_apis->texture_cube_close = dx11_texture_close<jegl_texture_cube>;
    write_to_apis->texture_msaa_close = dx11_texture_close<jegl_texture_msaa>;
    write_to_apis->vertex_close = dx11_vertex_close;
    write_to_apis->framebuffer_close = dx11_framebuf_close;
    write_to_apis->ubuffer_close = dx11_ubuffer_close;

    write_to_apis->pipeline_init = dx11_render_pipeline_init;
    write_to_apis->pipeline_close = dx11_render_pipeline_close;
    write_to_apis->pipeline_bind = dx11_render_pipeline_bind;

    write_to_apis->sampler_init = dx11_render_sampler_init;
    write_to_apis->sampler_close = dx11_render_sampler_close;
    write_to_apis->sampler_bind = dx11_render_sampler_bind;

    write_to_apis->begin_pass = dx11_render_begin_pass;
    write_to_apis->clear_color = dx11_render_clear_color;
    write_to_apis->clear_depth = dx11_render_clear_depth;
    write_to_apis->end_pass = dx11_render_end_pass;

    write_to_apis->bind_vertex = dx11_render_bind_vertex;
    write_to_apis->bind_uniform_buffer = dx11_render_bind_uniform_buffer;
    write_to_apis->bind_texture_2d = dx11_render_bind_texture_2d;
    write_to_apis->bind_texture_3d = dx11_render_bind_texture_3d;
    write_to_apis->bind_texture_array = dx11_render_bind_texture_array;
    write_to_apis->bind_texture_cube = dx11_render_bind_texture_cube;

    write_to_apis->draw_indexed = dx11_render_draw_indexed;
    write_to_apis->draw = dx11_render_draw;

    write_to_apis->commit_begin = dx11_render_commit_begin;
    write_to_apis->commit_end = dx11_render_commit_end;
}

#else
void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("DX11 not available.");
}
#endif
