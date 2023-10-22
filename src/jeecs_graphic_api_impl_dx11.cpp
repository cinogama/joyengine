#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_OS_WINDOWS

#include "jeecs_imgui_api.hpp"

#include <D3D11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "winmm.lib")

#define JERCHECK(RC) if (FAILED(RC)){jeecs::debug::logfatal("JoyEngine DX11 Failed: " #RC);}

struct jegl_dx11_context
{
    template <class T>
    using MSWRLComPtr = Microsoft::WRL::ComPtr<T>;

    // Direct3D 11
    MSWRLComPtr<ID3D11Device> m_dx_device;
    MSWRLComPtr<ID3D11DeviceContext> m_dx_context;
    MSWRLComPtr<IDXGISwapChain> m_dx_swapchain;

    HWND    m_window_handle;

    size_t WINDOWS_SIZE_WIDTH;
    size_t WINDOWS_SIZE_HEIGHT;

    size_t MSAA_LEVEL;
    size_t MSAA_QUALITY;

    MSWRLComPtr<ID3D11DepthStencilView> m_dx_main_renderer_target_depth_view;   // 深度模板视图
    MSWRLComPtr<ID3D11Texture2D> m_dx_main_renderer_target_depth_buffer;        // 深度模板缓冲区
    MSWRLComPtr<ID3D11RenderTargetView> m_dx_main_renderer_target_view;         // 渲染目标视图

    WNDCLASS m_win32_window_class;

    bool m_dx_context_finished;
    bool m_window_should_close;

    UINT m_next_binding_texture_place;
};

thread_local jegl_dx11_context* _je_dx_current_thread_context;

void dx11_callback_windows_size_changed(jegl_dx11_context* context, size_t w, size_t h)
{
    if (context->m_dx_context_finished == false)
        return;

    if (w == 0 || h == 0)
        return;

    context->WINDOWS_SIZE_WIDTH = w;
    context->WINDOWS_SIZE_HEIGHT = h;

    je_io_set_windowsize((int)w, (int)h);

    // 释放渲染管线输出用到的相关资源
    context->m_dx_main_renderer_target_depth_view.Reset();
    context->m_dx_main_renderer_target_depth_buffer.Reset();
    context->m_dx_main_renderer_target_view.Reset();

    // 重设交换链并且重新创建渲染目标视图
    jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> back_buffer;
    JERCHECK(context->m_dx_swapchain->ResizeBuffers(
        1, (UINT)context->WINDOWS_SIZE_WIDTH, (UINT)context->WINDOWS_SIZE_HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
    JERCHECK(context->m_dx_swapchain->GetBuffer(
        0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back_buffer.GetAddressOf())));
    JERCHECK(context->m_dx_device->CreateRenderTargetView(
        back_buffer.Get(), nullptr, context->m_dx_main_renderer_target_view.GetAddressOf()));

    // 设置调试对象名
#ifndef _NDEBUG
    const char debug_back_buffer_name[] = "JoyEngineDx11BackBuffer";
    back_buffer->SetPrivateData(WKPDID_D3DDebugObjectName,
        strlen(debug_back_buffer_name), debug_back_buffer_name);
#endif
    back_buffer.Reset();

    D3D11_TEXTURE2D_DESC depthStencilDesc;

    depthStencilDesc.Width = (UINT)context->WINDOWS_SIZE_WIDTH;
    depthStencilDesc.Height = (UINT)context->WINDOWS_SIZE_HEIGHT;
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
        &depthStencilDesc, nullptr,
        context->m_dx_main_renderer_target_depth_buffer.GetAddressOf()));
    JERCHECK(context->m_dx_device->CreateDepthStencilView(
        context->m_dx_main_renderer_target_depth_buffer.Get(), nullptr,
        context->m_dx_main_renderer_target_depth_view.GetAddressOf()));

    // 将渲染目标视图和深度/模板缓冲区结合到管线
    context->m_dx_context->OMSetRenderTargets(1,
        context->m_dx_main_renderer_target_view.GetAddressOf(),
        context->m_dx_main_renderer_target_depth_view.Get());

    // 设置视口变换
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)context->WINDOWS_SIZE_WIDTH;
    viewport.Height = (float)context->WINDOWS_SIZE_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    context->m_dx_context->RSSetViewports(1, &viewport);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (jegui_win32_proc_handler(hwnd, msg, wParam, lParam))
        return 0;

    switch (msg)
    {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.
    case WM_CLOSE:
        _je_dx_current_thread_context->m_window_should_close = true;
        return 0;
    case WM_ACTIVATE:
        return 0;
        // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
    {
        // Save the new client area dimensions.
        auto width = LOWORD(lParam);
        auto height = HIWORD(lParam);
        dx11_callback_windows_size_changed(_je_dx_current_thread_context, (size_t)width, (size_t)height);
        return 0;
    }
    case WM_ENTERSIZEMOVE:
        return 0;
    case WM_EXITSIZEMOVE:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);
    case WM_GETMINMAXINFO:
        /*((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;*/
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        return 0;
    case WM_MOUSEMOVE:
        return 0;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

jegl_thread::custom_thread_data_t dx11_startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
{
    jegl_dx11_context* context = new jegl_dx11_context;
    _je_dx_current_thread_context = context;

    context->m_dx_context_finished = false;
    context->m_window_should_close = false;
    context->WINDOWS_SIZE_WIDTH = config->m_width;
    context->WINDOWS_SIZE_HEIGHT = config->m_height;
    context->MSAA_LEVEL = config->m_msaa;
    context->m_next_binding_texture_place = 0;
    if (!reboot)
    {
        jeecs::debug::log("Graphic thread (DX11) start!");
        // ...
    }

    context->m_win32_window_class.style = CS_HREDRAW | CS_VREDRAW;
    context->m_win32_window_class.lpfnWndProc = MainWndProc;
    context->m_win32_window_class.cbClsExtra = 0;
    context->m_win32_window_class.cbWndExtra = 0;
    context->m_win32_window_class.hInstance = GetModuleHandle(NULL);
    context->m_win32_window_class.hIcon = LoadIcon(0, IDI_APPLICATION);
    context->m_win32_window_class.hCursor = LoadCursor(0, IDC_ARROW);
    context->m_win32_window_class.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    context->m_win32_window_class.lpszMenuName = 0;
    context->m_win32_window_class.lpszClassName = "JoyEngineDX11Form";

    if (!RegisterClassA(&context->m_win32_window_class))
    {
        jeecs::debug::logfatal("Failed to create windows for dx11: RegisterClass failed.");
        return nullptr;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = { 0, 0, (LONG)context->WINDOWS_SIZE_WIDTH, (LONG)context->WINDOWS_SIZE_HEIGHT };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    LONG width = R.right - R.left;
    LONG height = R.bottom - R.top;

    context->m_window_handle = CreateWindowA("JoyEngineDX11Form", config->m_title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0,
        context->m_win32_window_class.hInstance, 0);
    if (!context->m_window_handle)
    {
        jeecs::debug::logfatal("Failed to create windows for dx11.");
        return nullptr;
    }

    ShowWindow(context->m_window_handle, SW_SHOW);
    UpdateWindow(context->m_window_handle);

    D3D_DRIVER_TYPE dx_driver_types[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    D3D_FEATURE_LEVEL dx_feature_levels[] =
    {
        // D3D_FEATURE_LEVEL_11_1,// Only support dx11
        D3D_FEATURE_LEVEL_11_0
    };

    UINT dx_device_flag =
#ifdef _NDEBUG
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
    jegl_dx11_context::MSWRLComPtr<IDXGIFactory1> dxgiFactory1 = nullptr;   // D3D11.0(包含DXGI1.1)的接口类

    // 为了正确创建 DXGI交换链，首先我们需要获取创建 D3D设备 的 DXGI工厂，否则会引发报错：
    // "IDXGIFactory::CreateSwapChain: This function is being called with a device from a different IDXGIFactory."
    JERCHECK(context->m_dx_device.As(&dxgiDevice));
    JERCHECK(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));
    JERCHECK(dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory1.GetAddressOf())));

    // 填充DXGI_SWAP_CHAIN_DESC用以描述交换链
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width = (UINT)context->WINDOWS_SIZE_WIDTH;
    sd.BufferDesc.Height = (UINT)context->WINDOWS_SIZE_HEIGHT;
    sd.BufferDesc.RefreshRate.Numerator = (UINT)config->m_fps;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    if (config->m_fps == 0)
        je_ecs_universe_set_frame_deltatime(gthread->_m_universe_instance, 0.0);
    else
        je_ecs_universe_set_frame_deltatime(gthread->_m_universe_instance, 1.0 / (double)config->m_fps);

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
    sd.BufferCount = 1;
    sd.OutputWindow = context->m_window_handle;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;
    JERCHECK(dxgiFactory1->CreateSwapChain(
        context->m_dx_device.Get(), &sd, context->m_dx_swapchain.GetAddressOf()));


    // 可以禁止alt+enter全屏
    dxgiFactory1->MakeWindowAssociation(context->m_window_handle,
        DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

    // 设置调试对象名

#ifndef _NDEBUG
    const char debug_context_name[] = "JoyEngineDx11Context";
    context->m_dx_context->SetPrivateData(WKPDID_D3DDebugObjectName,
        strlen(debug_context_name), debug_context_name);

    const char debug_swapchain_name[] = "JoyEngineDx11SwapChain";
    context->m_dx_swapchain->SetPrivateData(WKPDID_D3DDebugObjectName,
        strlen(debug_swapchain_name), debug_swapchain_name);

#endif
    context->m_dx_context_finished = true;

    dx11_callback_windows_size_changed(context,
        context->WINDOWS_SIZE_WIDTH, context->WINDOWS_SIZE_HEIGHT);

    jegui_init_dx11(
        context->m_window_handle,
        context->m_dx_device.Get(),
        context->m_dx_context.Get(),
        reboot);
    return context;
}

void dx11_shutdown(jegl_thread*, jegl_thread::custom_thread_data_t userdata, bool reboot)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(userdata));

    if (!reboot)
        jeecs::debug::log("Graphic thread (DX11) shutdown!");

    jegui_shutdown_dx11(reboot);

    if (context->m_dx_context)
        context->m_dx_context->ClearState();

    DestroyWindow(context->m_window_handle);

    if (!UnregisterClassA(
        context->m_win32_window_class.lpszClassName,
        context->m_win32_window_class.hInstance))
    {
        jeecs::debug::logfatal("Failed to shutdown windows for dx11: UnregisterClass failed.");
    }
    delete context;
}

bool dx11_update(jegl_thread* ctx)
{
    jegl_dx11_context* context =
        std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));

    MSG msg = { 0 };
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (context->m_window_should_close)
    {
        jeecs::debug::loginfo("Graphic interface has been requested to close.");

        if (jegui_shutdown_callback())
            return false;
        context->m_window_should_close = false;
    }
    return true;
}

bool dx11_pre_update(jegl_thread* ctx)
{
    static auto quad = jeecs::graphic::vertex::create(
        jegl_vertex::TRIANGLESTRIP,
        {
            -0.5f, 0.5f, 0.5f,    0.0f, 1.0f, // 0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, 0.5f,   0.0f, 0.0f,  //0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, 0.5f,     1.0f, 1.0f,  //0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, 0.5f,    1.0f, 0.0f,  //0.0f, 0.0f, -1.0f,
        },
        { 3, 2, /*3*/ });
    static auto shad = jeecs::graphic::shader::create("dx11_test.wo", R"(
import je::shader;

SHARED  (false);
ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (NONE);

VAO_STRUCT! vin {
    vertex  : float3,
    uv      : float2,
};

using v2f = struct {
    pos     : float4,
    uv      : float2,
};

using fout = struct {
    color   : float4
};

public func vert(v: vin)
{
    return v2f{
        pos = float4::create(v.vertex, 1.0),
        uv = v.uv, //uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let Albedo = uniform_texture:<texture2d>("Albedo", 0);
    let color = texture(Albedo, vf.uv);
    return fout{
        color = float4::create(color->xyz, color->w * vf.uv->x),
    };
}
    )");
    static auto texture = jeecs::graphic::texture::load("@/builtin/icon/icon.png");

    jegl_using_texture(texture->resouce(), 0);
    jegl_using_resource(shad->resouce());
    jegl_draw_vertex(quad->resouce());

    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    JERCHECK(context->m_dx_swapchain->Present(ctx->m_config.m_fps == 0 ? 1 : 0, 0));
    return true;
}
bool dx11_lateupdate(jegl_thread* ctx)
{
    jegui_update_dx11(ctx);
    return true;
}

struct jedx11_texture
{
    jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> m_texture;
    jegl_dx11_context::MSWRLComPtr<ID3D11ShaderResourceView> m_texture_view;
    jegl_dx11_context::MSWRLComPtr<ID3D11SamplerState> m_texture_sampler;
};
struct jedx11_shader
{
    jegl_dx11_context::MSWRLComPtr<ID3D11InputLayout> m_vao; // WTF?
    jegl_dx11_context::MSWRLComPtr<ID3D11VertexShader> m_vertex;
    jegl_dx11_context::MSWRLComPtr<ID3D11PixelShader> m_fragment;

    bool m_have_uniforms;
    void* m_uniform_updating_buffer;
    jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_uniforms;

    jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer;
    jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilState> m_depth;
    jegl_dx11_context::MSWRLComPtr<ID3D11BlendState> m_blend;
};
struct jedx11_vertex
{
    jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_vbo;
    UINT m_count;
    UINT m_strides;
    D3D_PRIMITIVE_TOPOLOGY m_method;
};
struct jedx11_uniformbuf
{
    jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_uniformbuf;
    UINT m_binding_place;
};
struct jedx11_framebuffer
{
    std::vector<jegl_dx11_context::MSWRLComPtr<ID3D11RenderTargetView>>m_rend_views;
    jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilView> m_depth_view;
    std::vector<ID3D11RenderTargetView*>m_target_views;
    UINT m_color_target_count;
};

void dx11_init_resource(jegl_thread* ctx, jegl_resource* resource)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    switch (resource->m_type)
    {
    case jegl_resource::type::SHADER:
    {
        jedx11_shader* jedx11_shader_res = new jedx11_shader;
        jedx11_shader_res->m_uniform_updating_buffer = nullptr;

        bool shader_load_failed = false;

        std::string error_informations;

        jegl_dx11_context::MSWRLComPtr<ID3DBlob> blob;
        ID3DBlob* error_blob = nullptr;

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
            blob.GetAddressOf(),
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
            JERCHECK(context->m_dx_device->CreateVertexShader(
                blob->GetBufferPointer(),
                blob->GetBufferSize(),
                nullptr,
                jedx11_shader_res->m_vertex.GetAddressOf()));

            UINT layout_begin_offset = 0;

            std::vector<D3D11_INPUT_ELEMENT_DESC> vertex_in_layout(resource->m_raw_shader_data->m_vertex_in_count);

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

                switch (resource->m_raw_shader_data->m_vertex_in[i].m_type)
                {
                case jegl_shader::uniform_type::INT:
                    vlayout.SemanticIndex = INT_COUNT++;
                    vlayout.SemanticName = "BLENDINDICES";
                    vlayout.Format = DXGI_FORMAT_R32_SINT;
                    layout_begin_offset += 4;
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
                    else
                    {
                        vlayout.SemanticIndex -= 2;
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
                    else
                    {
                        vlayout.SemanticIndex -= 2;
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
                blob->GetBufferPointer(),
                blob->GetBufferSize(),
                jedx11_shader_res->m_vao.GetAddressOf()));
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
            blob.ReleaseAndGetAddressOf(),
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
        else
        {
            JERCHECK(context->m_dx_device->CreatePixelShader(
                blob->GetBufferPointer(),
                blob->GetBufferSize(),
                nullptr,
                jedx11_shader_res->m_fragment.GetAddressOf()));
        }

        if (shader_load_failed)
        {
            jeecs::debug::logerr("Some error happend when tring compile shader %p, please check.\n %s",
                resource, error_informations.c_str());
            resource->m_handle.m_ptr = nullptr;
        }
        else
        {
            // Generate uniform locations here.
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
                case jegl_shader::uniform_type::FLOAT2:
                    unit_size = 8;
                    break;
                case jegl_shader::uniform_type::FLOAT3:
                    unit_size = 12;
                    break;
                case jegl_shader::uniform_type::FLOAT4:
                    unit_size = 16;
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
                    auto next_edge = last_elem_end_place / DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

                    if ((last_elem_end_place + unit_size) > next_edge)
                        last_elem_end_place = next_edge;

                    uniforms->m_index = last_elem_end_place;
                    last_elem_end_place += unit_size;
                }

                uniforms = uniforms->m_next;
            }

            if (last_elem_end_place % DX11_ALLIGN_BASE != 0)
                last_elem_end_place = last_elem_end_place / DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

            // Alloc constant buffer for this shader!
            jedx11_shader_res->m_have_uniforms = (last_elem_end_place != 0);
            if (jedx11_shader_res->m_have_uniforms)
            {
                D3D11_BUFFER_DESC const_buffer_describe;

                const_buffer_describe.ByteWidth = (UINT)last_elem_end_place;
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

            D3D11_RASTERIZER_DESC rasterizer_describe;
            switch (resource->m_raw_shader_data->m_cull_mode)
            {
            case jegl_shader::cull_mode::NONE:
                rasterizer_describe.CullMode = D3D11_CULL_NONE;
                break;
            case jegl_shader::cull_mode::FRONT:
                rasterizer_describe.CullMode = D3D11_CULL_FRONT;
                break;
            case jegl_shader::cull_mode::BACK:
                rasterizer_describe.CullMode = D3D11_CULL_BACK;
                break;
            }
            rasterizer_describe.FillMode = D3D11_FILL_SOLID;
            rasterizer_describe.FrontCounterClockwise = TRUE;
            rasterizer_describe.DepthBias = 0;
            rasterizer_describe.DepthBiasClamp = 0.0f;
            rasterizer_describe.SlopeScaledDepthBias = 0.0f;
            rasterizer_describe.DepthClipEnable = TRUE;
            rasterizer_describe.ScissorEnable = FALSE;
            rasterizer_describe.MultisampleEnable = FALSE;
            rasterizer_describe.AntialiasedLineEnable = FALSE;
            JERCHECK(context->m_dx_device->CreateRasterizerState(
                &rasterizer_describe, jedx11_shader_res->m_rasterizer.GetAddressOf()));

            D3D11_DEPTH_STENCIL_DESC depth_describe;
            depth_describe.DepthEnable = TRUE;
            switch (resource->m_raw_shader_data->m_depth_test)
            {
            case jegl_shader::depth_test_method::OFF:
                depth_describe.DepthEnable = FALSE;
                break;
            case jegl_shader::depth_test_method::NEVER:
                depth_describe.DepthFunc = D3D11_COMPARISON_NEVER;
                break;
            case jegl_shader::depth_test_method::LESS:       /* DEFAULT */
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
                &depth_describe, jedx11_shader_res->m_depth.GetAddressOf()));

            D3D11_BLEND_DESC blend_describe;
            blend_describe.AlphaToCoverageEnable = FALSE;
            // OpenGL咋没这么牛逼的选项呢…… 只能含泪关掉
            blend_describe.IndependentBlendEnable = FALSE;
            blend_describe.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blend_describe.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend_describe.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (resource->m_raw_shader_data->m_blend_src_mode == jegl_shader::blend_method::ONE &&
                resource->m_raw_shader_data->m_blend_dst_mode == jegl_shader::blend_method::ZERO)
                blend_describe.RenderTarget[0].BlendEnable = FALSE;
            else
                blend_describe.RenderTarget[0].BlendEnable = TRUE;
            auto parse_dx11_enum = [](jegl_shader::blend_method method)
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
            blend_describe.RenderTarget[0].SrcBlend
                = blend_describe.RenderTarget[0].SrcBlendAlpha
                = parse_dx11_enum(resource->m_raw_shader_data->m_blend_src_mode);
            blend_describe.RenderTarget[0].DestBlend
                = blend_describe.RenderTarget[0].DestBlendAlpha
                = parse_dx11_enum(resource->m_raw_shader_data->m_blend_dst_mode);
            JERCHECK(context->m_dx_device->CreateBlendState(
                &blend_describe, jedx11_shader_res->m_blend.GetAddressOf()));

            resource->m_handle.m_ptr = jedx11_shader_res;
        }

        break;
    }
    case jegl_resource::type::TEXTURE:
    {
        assert((resource->m_raw_texture_data->m_format & jegl_texture::format::CUBE) == 0);

        D3D11_TEXTURE2D_DESC texture_describe;
        texture_describe.Width = (UINT)resource->m_raw_texture_data->m_width;
        texture_describe.Height = (UINT)resource->m_raw_texture_data->m_height;
        texture_describe.MipLevels = 1;
        texture_describe.ArraySize = 1;

        bool depth16f = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR16);

        switch (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
        {
        case jegl_texture::format::MONO:
            texture_describe.Format = depth16f ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R16_FLOAT;
            break;
        case jegl_texture::format::RGBA:
            texture_describe.Format = depth16f ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT;
            break;
        }
        size_t msaa_level = (size_t)(resource->m_raw_texture_data->m_format
            & jegl_texture::format::MSAA_MASK) >> (size_t)8;
        UINT msaa_quality = 0;
        if (msaa_level != 0)
        {
            context->m_dx_device->CheckMultisampleQualityLevels(
                texture_describe.Format, (UINT)msaa_level, &msaa_quality);
        }
        if (msaa_quality > 0)
        {
            texture_describe.SampleDesc.Count = (UINT)msaa_level;
            texture_describe.SampleDesc.Quality = msaa_quality - 1;
        }
        else
        {
            texture_describe.SampleDesc.Count = 1;
            texture_describe.SampleDesc.Quality = 0;
        }
        texture_describe.Usage = D3D11_USAGE_DEFAULT;
        texture_describe.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_describe.CPUAccessFlags = 0;
        texture_describe.MiscFlags = 0;

        jedx11_texture* jedx11_texture_res = new jedx11_texture;

        D3D11_SAMPLER_DESC sampler_describe;

        sampler_describe.MipLODBias = 0.0f;
        sampler_describe.MaxAnisotropy = 0;
        sampler_describe.BorderColor[0] = 0.0f;
        sampler_describe.BorderColor[1] = 0.0f;
        sampler_describe.BorderColor[2] = 0.0f;
        sampler_describe.BorderColor[3] = 0.0f;

        // Apply fliter setting
        switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MIN_FILTER_MASK)
        {
        case jegl_texture::sampling::MIN_LINEAR:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        case jegl_texture::sampling::MIN_NEAREST:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        case jegl_texture::sampling::MIN_LINEAR_LINEAR_MIP:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        case jegl_texture::sampling::MIN_NEAREST_LINEAR_MIP:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        case jegl_texture::sampling::MIN_LINEAR_NEAREST_MIP:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        case jegl_texture::sampling::MIN_NEAREST_NEAREST_MIP:
            switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::MAG_FILTER_MASK)
            {
            case jegl_texture::sampling::MAG_LINEAR:
                sampler_describe.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
            case jegl_texture::sampling::MAG_NEAREST:
                sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
            default:
                jeecs::debug::logerr("Unknown texture mag filter method(%04x)",
                    resource->m_raw_texture_data->m_sampling);
            }
            break;
        default:
            jeecs::debug::logerr("Unknown texture min filter method(%04x)",
                resource->m_raw_texture_data->m_sampling);
        }

        if (resource->m_raw_texture_data->m_sampling)
            sampler_describe.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

        switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::WRAP_X_METHOD_MASK)
        {
        case jegl_texture::sampling::CLAMP_EDGE_X:
            sampler_describe.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; break;
        case jegl_texture::sampling::REPEAT_X:
            sampler_describe.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; break;
        default:
            jeecs::debug::logerr("Unknown texture wrap method in x(%04x)",
                resource->m_raw_texture_data->m_sampling);
        }
        switch (resource->m_raw_texture_data->m_sampling & jegl_texture::sampling::WRAP_Y_METHOD_MASK)
        {
        case jegl_texture::sampling::CLAMP_EDGE_Y:
            sampler_describe.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; break;
        case jegl_texture::sampling::REPEAT_Y:
            sampler_describe.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; break;
        default:
            jeecs::debug::logerr("Unknown texture wrap method in y(%04x)",
                resource->m_raw_texture_data->m_sampling);
        }
        sampler_describe.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_describe.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_describe.MinLOD = 0;
        sampler_describe.MaxLOD = D3D11_FLOAT32_MAX;
        JERCHECK(context->m_dx_device->CreateSamplerState(
            &sampler_describe,
            jedx11_texture_res->m_texture_sampler.GetAddressOf()));

        if ((resource->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF)
            == jegl_texture::format::FRAMEBUF)
        {
            if (resource->m_raw_texture_data->m_format & jegl_texture::format::DEPTH)
            {
                texture_describe.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                size_t msaa_level = (size_t)(resource->m_raw_texture_data->m_format
                    & jegl_texture::format::MSAA_MASK) >> (size_t)8;
                UINT msaa_quality = 0;
                if (msaa_level != 0)
                {
                    context->m_dx_device->CheckMultisampleQualityLevels(
                        texture_describe.Format, (UINT)msaa_level, &msaa_quality);
                }
                if (msaa_quality > 0)
                {
                    texture_describe.SampleDesc.Count = (UINT)msaa_level;
                    texture_describe.SampleDesc.Quality = msaa_quality - 1;
                }
                else
                {
                    texture_describe.SampleDesc.Count = 1;
                    texture_describe.SampleDesc.Quality = 0;
                }

                texture_describe.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

                // 创建深度缓冲区以及深度模板视图
                JERCHECK(context->m_dx_device->CreateTexture2D(
                    &texture_describe, nullptr,
                    jedx11_texture_res->m_texture.GetAddressOf()));
                JERCHECK(context->m_dx_device->CreateShaderResourceView(
                    jedx11_texture_res->m_texture.Get(), nullptr,
                    jedx11_texture_res->m_texture_view.GetAddressOf()));
            }
            else
            {
                texture_describe.BindFlags |= D3D11_BIND_RENDER_TARGET;

                JERCHECK(context->m_dx_device->CreateTexture2D(
                    &texture_describe, nullptr, jedx11_texture_res->m_texture.GetAddressOf()));
            }
        }
        else
        {
            D3D11_SUBRESOURCE_DATA texture_sub_data;
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

            JERCHECK(context->m_dx_device->CreateTexture2D(
                &texture_describe, &texture_sub_data,
                jedx11_texture_res->m_texture.GetAddressOf()));

            D3D11_SHADER_RESOURCE_VIEW_DESC texture_shader_view_describe;
            texture_shader_view_describe.Format = texture_describe.Format;
            texture_shader_view_describe.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            texture_shader_view_describe.Texture2D.MipLevels = 1;
            texture_shader_view_describe.Texture2D.MostDetailedMip = 0;

            JERCHECK(context->m_dx_device->CreateShaderResourceView(
                jedx11_texture_res->m_texture.Get(),
                &texture_shader_view_describe,
                jedx11_texture_res->m_texture_view.GetAddressOf()));
        }
        resource->m_handle.m_ptr = jedx11_texture_res;
        break;
    }
    case jegl_resource::type::VERTEX:
    {
        jedx11_vertex* vertex = new jedx11_vertex;

        const static D3D_PRIMITIVE_TOPOLOGY DRAW_METHODS[] = {
            D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        };
        vertex->m_method =
            DRAW_METHODS[resource->m_raw_vertex_data->m_type];

        D3D11_BUFFER_DESC vertex_buffer_describe;

        UINT ByteWidth;
        D3D11_USAGE Usage;
        UINT BindFlags;
        UINT CPUAccessFlags;
        UINT MiscFlags;
        UINT StructureByteStride;

        vertex->m_count = (UINT)resource->m_raw_vertex_data->m_point_count;
        vertex->m_strides = resource->m_raw_vertex_data->m_data_count_per_point;

        vertex_buffer_describe.ByteWidth =
            (UINT)(resource->m_raw_vertex_data->m_point_count
                * resource->m_raw_vertex_data->m_data_count_per_point
                * sizeof(float));

        vertex_buffer_describe.Usage = D3D11_USAGE_IMMUTABLE;
        vertex_buffer_describe.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buffer_describe.CPUAccessFlags = 0;
        vertex_buffer_describe.MiscFlags = 0;
        vertex_buffer_describe.StructureByteStride = 0;

        // 新建顶点缓冲区
        D3D11_SUBRESOURCE_DATA vertex_buffer_data;
        vertex_buffer_data.pSysMem = resource->m_raw_vertex_data->m_vertex_datas;
        vertex_buffer_data.SysMemPitch = 0;
        vertex_buffer_data.SysMemSlicePitch = 0;
        JERCHECK(context->m_dx_device->CreateBuffer(
            &vertex_buffer_describe,
            &vertex_buffer_data,
            vertex->m_vbo.GetAddressOf()));

        resource->m_handle.m_ptr = vertex;

        break;
    }
    case jegl_resource::type::FRAMEBUF:
    {
        jedx11_framebuffer* jedx11_framebuffer_res = new jedx11_framebuffer;

        jeecs::basic::resource<jeecs::graphic::texture>* attachments =
            std::launder(reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture>*>(
                resource->m_raw_framebuf_data->m_output_attachments));

        size_t color_attachment_count = 0;
        for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
        {
            auto& attachment = attachments[i];
            if (0 == (attachment->resouce()->m_raw_texture_data->m_format & jegl_texture::format::DEPTH))
                ++color_attachment_count;
        }

        jedx11_framebuffer_res->m_rend_views.resize(color_attachment_count);
        color_attachment_count = 0;

        for (size_t i = 0; i < resource->m_raw_framebuf_data->m_attachment_count; ++i)
        {
            auto& attachment = attachments[i];
            if (0 != (attachment->resouce()->m_raw_texture_data->m_format & jegl_texture::format::DEPTH))
            {
                if (jedx11_framebuffer_res->m_depth_view.Get() == nullptr)
                {
                    JERCHECK(context->m_dx_device->CreateDepthStencilView(
                        std::launder(reinterpret_cast<jedx11_texture*>(attachment->resouce()->m_handle.m_ptr))->m_texture.Get(),
                        nullptr, jedx11_framebuffer_res->m_depth_view.GetAddressOf()));
                }
                else
                    jeecs::debug::logerr("Framebuffer(%p) attach depth buffer repeatedly.", resource);
            }
            else
            {
                JERCHECK(context->m_dx_device->CreateRenderTargetView(
                    std::launder(reinterpret_cast<jedx11_texture*>(attachment->resouce()->m_handle.m_ptr))->m_texture.Get(),
                    nullptr, jedx11_framebuffer_res->m_rend_views[color_attachment_count++].GetAddressOf()));

            }
        }
        for (auto& v : jedx11_framebuffer_res->m_rend_views)
            jedx11_framebuffer_res->m_target_views.push_back(v.Get());

        jedx11_framebuffer_res->m_color_target_count = (UINT)color_attachment_count;
        assert(jedx11_framebuffer_res->m_target_views.size() == jedx11_framebuffer_res->m_rend_views.size());
        assert(jedx11_framebuffer_res->m_target_views.size() == color_attachment_count);

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
void dx11_using_resource(jegl_thread* ctx, jegl_resource* resource)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    switch (resource->m_type)
    {
    case jegl_resource::type::SHADER:
    {
        auto* shader = std::launder(reinterpret_cast<jedx11_shader*>(resource->m_handle.m_ptr));
        if (shader->m_have_uniforms)
        {
            if (shader->m_uniform_updating_buffer != nullptr)
            {
                context->m_dx_context->Unmap(shader->m_uniforms.Get(), 0);
                shader->m_uniform_updating_buffer = nullptr;
            }
            context->m_dx_context->VSSetConstantBuffers(0, 1, shader->m_uniforms.GetAddressOf());
            context->m_dx_context->PSSetConstantBuffers(0, 1, shader->m_uniforms.GetAddressOf());
        }
        context->m_dx_context->VSSetShader(shader->m_vertex.Get(), nullptr, 0);
        context->m_dx_context->PSSetShader(shader->m_fragment.Get(), nullptr, 0);
        context->m_dx_context->IASetInputLayout(shader->m_vao.Get());

        context->m_dx_context->RSSetState(shader->m_rasterizer.Get());
        float _useless[4] = {};
        context->m_dx_context->OMSetBlendState(shader->m_blend.Get(), _useless, UINT_MAX);
        context->m_dx_context->OMSetDepthStencilState(shader->m_depth.Get(), 0);

        break;
    }
    case jegl_resource::type::TEXTURE:
    {
        auto* texture = std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));
        context->m_dx_context->VSSetSamplers(
            context->m_next_binding_texture_place, 1, texture->m_texture_sampler.GetAddressOf());
        context->m_dx_context->VSSetShaderResources(
            context->m_next_binding_texture_place, 1, texture->m_texture_view.GetAddressOf());
        context->m_dx_context->PSSetSamplers(
            context->m_next_binding_texture_place, 1, texture->m_texture_sampler.GetAddressOf());
        context->m_dx_context->PSSetShaderResources(
            context->m_next_binding_texture_place, 1, texture->m_texture_view.GetAddressOf());
        break;
    }
    case jegl_resource::type::VERTEX:
    {
        auto* vertex = std::launder(reinterpret_cast<jedx11_vertex*>(resource->m_handle.m_ptr));
        const UINT offset = 0;
        const UINT strides = vertex->m_strides * sizeof(float);
        context->m_dx_context->IASetVertexBuffers(
            0, 1, vertex->m_vbo.GetAddressOf(), &strides, &offset);
        context->m_dx_context->IASetPrimitiveTopology(vertex->m_method);
        break;
    }
    case jegl_resource::type::FRAMEBUF:
    {
        auto* framebuffer = std::launder(reinterpret_cast<jedx11_framebuffer*>(resource->m_handle.m_ptr));
        context->m_dx_context->OMSetRenderTargets(
            framebuffer->m_color_target_count,
            framebuffer->m_target_views.data(),
            framebuffer->m_depth_view.Get());
        break;
    }
    case jegl_resource::type::UNIFORMBUF:
    {
        auto* uniformbuf = std::launder(reinterpret_cast<jedx11_uniformbuf*>(resource->m_handle.m_ptr));
        if (resource->m_raw_uniformbuf_data != nullptr)
        {
            if (resource->m_raw_uniformbuf_data->m_update_length != 0)
            {
                D3D11_MAPPED_SUBRESOURCE mappedData;
                JERCHECK(context->m_dx_context->Map(
                    uniformbuf->m_uniformbuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

                memcpy((void*)((intptr_t)mappedData.pData + resource->m_raw_uniformbuf_data->m_update_begin_offset),
                    resource->m_raw_uniformbuf_data->m_buffer + resource->m_raw_uniformbuf_data->m_update_begin_offset,
                    resource->m_raw_uniformbuf_data->m_update_length);

                context->m_dx_context->Unmap(uniformbuf->m_uniformbuf.Get(), 0);

                resource->m_raw_uniformbuf_data->m_update_begin_offset = 0;
                resource->m_raw_uniformbuf_data->m_update_length = 0;
            }
            context->m_dx_context->VSSetConstantBuffers(
                uniformbuf->m_binding_place, 1, uniformbuf->m_uniformbuf.GetAddressOf());
            context->m_dx_context->PSSetConstantBuffers(
                uniformbuf->m_binding_place, 1, uniformbuf->m_uniformbuf.GetAddressOf());
        }
        break;
    }
    default:
        break;
    }
}
void dx11_close_resource(jegl_thread* ctx, jegl_resource* resource)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    switch (resource->m_type)
    {
    case jegl_resource::type::SHADER:
        delete std::launder(reinterpret_cast<jedx11_shader*>(resource->m_handle.m_ptr));
        break;
    case jegl_resource::type::TEXTURE:
        delete std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));
        break;
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
}
void* dx11_native_resource(jegl_thread* gthread, jegl_resource* resource)
{
    switch (resource->m_type)
    {
    case jegl_resource::type::SHADER:
        return nullptr;
    case jegl_resource::type::TEXTURE:
    {
        auto* res = std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));
        return (void*)(intptr_t)res->m_texture_view.Get();
    }
    case jegl_resource::type::VERTEX:
        return nullptr;
    case jegl_resource::type::FRAMEBUF:
        return nullptr;
    case jegl_resource::type::UNIFORMBUF:
        return nullptr;
    default:
        jeecs::debug::logerr("Unknown resource type when closing resource %p, please check.", resource);
        return nullptr;
    }
}

void dx11_draw_vertex_with_shader(jegl_thread* ctx, jegl_resource* vert)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));

    assert(vert->m_type == jegl_resource::type::VERTEX);

    jegl_using_resource(vert);

    auto* vertex = std::launder(reinterpret_cast<jedx11_vertex*>(vert->m_handle.m_ptr));
    context->m_dx_context->Draw(vertex->m_count, 0);
}

void dx11_bind_texture(jegl_thread* ctx, jegl_resource* texture, size_t pass)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    context->m_next_binding_texture_place = pass;
    jegl_using_resource(texture);
}

void dx11_set_rend_to_framebuffer(jegl_thread* ctx, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));
    if (framebuffer == nullptr)
    {
        context->m_dx_context->OMSetRenderTargets(1,
            context->m_dx_main_renderer_target_view.GetAddressOf(),
            context->m_dx_main_renderer_target_depth_view.Get());
    }
    else
        jegl_using_resource(framebuffer);

    auto* framw_buffer_raw = framebuffer != nullptr ? framebuffer->m_raw_framebuf_data : nullptr;
    if (w == 0)
        w = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_width : context->WINDOWS_SIZE_WIDTH;
    if (h == 0)
        h = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_height : context->WINDOWS_SIZE_HEIGHT;

    y = context->WINDOWS_SIZE_HEIGHT - y;

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = (float)x;
    viewport.TopLeftY = (float)y;
    viewport.Width = (float)context->WINDOWS_SIZE_WIDTH;
    viewport.Height = (float)context->WINDOWS_SIZE_HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    context->m_dx_context->RSSetViewports(1, &viewport);
}

void dx11_clear_framebuffer_color(jegl_thread* ctx, jegl_resource* framebuffer)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));

    static float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // RGBA = (0,0,255,255)

    if (framebuffer == nullptr)
        context->m_dx_context->ClearRenderTargetView(
            context->m_dx_main_renderer_target_view.Get(), clear_color);
    else
    {
        auto* frambuf = std::launder(reinterpret_cast<jedx11_framebuffer*>(framebuffer->m_handle.m_ptr));
        for (auto& target : frambuf->m_rend_views)
            context->m_dx_context->ClearRenderTargetView(
                target.Get(), clear_color);
    }
}
void dx11_clear_framebuffer_depth(jegl_thread* ctx, jegl_resource* framebuffer)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));

    if (framebuffer == nullptr)
        context->m_dx_context->ClearDepthStencilView(
            context->m_dx_main_renderer_target_depth_view.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    else
    {
        auto* frambuf = std::launder(reinterpret_cast<jedx11_framebuffer*>(framebuffer->m_handle.m_ptr));
        context->m_dx_context->ClearDepthStencilView(
            frambuf->m_depth_view.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}
void dx11_clear_framebuffer(jegl_thread* ctx, jegl_resource* framebuffer)
{
    dx11_clear_framebuffer_color(ctx, framebuffer);
    dx11_clear_framebuffer_depth(ctx, framebuffer);
}

void dx11_set_uniform_todo(jegl_thread* ctx, jegl_resource* res, uint32_t location, jegl_shader::uniform_type type, const void* val)
{
    jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx->m_userdata));

    assert(res->m_type == jegl_resource::type::SHADER);
    auto* shader = std::launder(reinterpret_cast<jedx11_shader*>(res->m_handle.m_ptr));

    if (location == jeecs::typing::INVALID_UINT32 || !shader->m_have_uniforms)
        return;

    if (shader->m_uniform_updating_buffer == nullptr)
    {
        D3D11_MAPPED_SUBRESOURCE mappedData;
        JERCHECK(context->m_dx_context->Map(shader->m_uniforms.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
        shader->m_uniform_updating_buffer = mappedData.pData;
    }

    size_t data_size_byte_length = 0;
    switch (type)
    {
    case jegl_shader::INT:
    case jegl_shader::FLOAT:
        memcpy(reinterpret_cast<void*>((intptr_t)shader->m_uniform_updating_buffer + 1), val, 4);
        break;
    case jegl_shader::FLOAT2:
        memcpy(reinterpret_cast<void*>((intptr_t)shader->m_uniform_updating_buffer + 1), val, 8);
        break;
    case jegl_shader::FLOAT3:
        memcpy(reinterpret_cast<void*>((intptr_t)shader->m_uniform_updating_buffer + 1), val, 12);
        break;
    case jegl_shader::FLOAT4:
        memcpy(reinterpret_cast<void*>((intptr_t)shader->m_uniform_updating_buffer + 1), val, 16);
        break;
    case jegl_shader::FLOAT4X4:
        memcpy(reinterpret_cast<void*>((intptr_t)shader->m_uniform_updating_buffer + 1), val, 64);
        break;
    default:
        jeecs::debug::logerr("Unknown uniform variable type to set."); break;
        break;
    }
}

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    write_to_apis->init_interface = dx11_startup;
    write_to_apis->shutdown_interface = dx11_shutdown;

    write_to_apis->pre_update_interface = dx11_pre_update;
    write_to_apis->update_interface = dx11_update;
    write_to_apis->late_update_interface = dx11_lateupdate;

    write_to_apis->init_resource = dx11_init_resource;
    write_to_apis->using_resource = dx11_using_resource;
    write_to_apis->close_resource = dx11_close_resource;
    write_to_apis->native_resource = dx11_native_resource;

    write_to_apis->draw_vertex = dx11_draw_vertex_with_shader;
    write_to_apis->bind_texture = dx11_bind_texture;

    write_to_apis->set_rend_buffer = dx11_set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer = dx11_clear_framebuffer;
    write_to_apis->clear_rend_buffer_color = dx11_clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = dx11_clear_framebuffer_depth;

    write_to_apis->set_uniform = dx11_set_uniform_todo;
}

#else

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("Current platform not support dx11, try using opengl3 instead.");
}

#endif