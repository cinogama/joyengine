#define JE_IMPL
#include "jeecs.hpp"

#ifdef JE_ENABLE_DX11_GAPI

#include "jeecs_imgui_backend_api.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#ifndef NDEBUG
#   include <dxgidebug.h>
#endif

#undef max
#undef min

#define JERCHECK(RC) if (FAILED(RC)){jeecs::debug::logfatal("JoyEngine DX11 Failed: " #RC);}

namespace jeecs::graphic::api::dx11
{
    struct jedx11_texture;
    struct jedx11_shader;
    struct jedx11_vertex;
    struct jedx11_uniformbuf;
    struct jedx11_framebuffer;

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

        size_t RESOLUTION_WIDTH;
        size_t RESOLUTION_HEIGHT;

        size_t MSAA_LEVEL;
        size_t MSAA_QUALITY;

        size_t FPS;

        MSWRLComPtr<ID3D11DepthStencilView> m_dx_main_renderer_target_depth_view;   // 深度模板视图
        MSWRLComPtr<ID3D11Texture2D> m_dx_main_renderer_target_depth_buffer;        // 深度模板缓冲区
        MSWRLComPtr<ID3D11RenderTargetView> m_dx_main_renderer_target_view;         // 渲染目标视图

        WNDCLASS m_win32_window_class;
        HICON m_win32_window_icon;

        bool m_dx_context_finished;
        bool m_window_should_close;
        bool m_lock_resolution_for_fullscreen;

        bool m_windows_changing;
        size_t m_windows_changing_width;
        size_t m_windows_changing_height;

        UINT m_next_binding_texture_place;

        jedx11_shader* m_current_target_shader;
        jedx11_framebuffer* m_current_target_framebuffer;
    };

    struct jedx11_texture
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11Texture2D> m_texture;
        jegl_dx11_context::MSWRLComPtr<ID3D11ShaderResourceView> m_texture_view;
    };
    struct jedx11_shader
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11InputLayout> m_vao; // WTF?
        jegl_dx11_context::MSWRLComPtr<ID3D11VertexShader> m_vertex;
        jegl_dx11_context::MSWRLComPtr<ID3D11PixelShader> m_fragment;

        bool m_uniform_updated;
        jegl_dx11_context::MSWRLComPtr<ID3D11Buffer> m_uniforms;
        void* m_uniform_cpu_buffers;
        size_t m_uniform_buffer_size;

        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer;
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

    thread_local jegl_dx11_context* _je_dx_current_thread_context;

    template<typename T>
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

    void dx11_callback_windows_size_changed(jegl_dx11_context* context, size_t w, size_t h)
    {
        je_io_update_windowsize((int)w, (int)h);
        if (context->m_dx_context_finished == false)
            return;

        if (w == 0 || h == 0)
            return;

        context->WINDOWS_SIZE_WIDTH = w;
        context->WINDOWS_SIZE_HEIGHT = h;

        if (context->m_lock_resolution_for_fullscreen == false)
        {
            context->RESOLUTION_WIDTH = context->WINDOWS_SIZE_WIDTH;
            context->RESOLUTION_HEIGHT = context->WINDOWS_SIZE_HEIGHT;
        }

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
            &depthStencilDesc, nullptr,
            context->m_dx_main_renderer_target_depth_buffer.GetAddressOf()));
        JERCHECK(context->m_dx_device->CreateDepthStencilView(
            context->m_dx_main_renderer_target_depth_buffer.Get(), nullptr,
            context->m_dx_main_renderer_target_depth_view.GetAddressOf()));

        JEDX11_TRACE_DEBUG_NAME(context->m_dx_main_renderer_target_depth_buffer, "JoyEngineDx11TargetDepthBuffer");
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_main_renderer_target_depth_buffer, "JoyEngineDx11TargetDepthBuffer");
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_main_renderer_target_depth_view, "JoyEngineDx11TargetDepthView");

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

    void dx11_handle_key_state(WPARAM wParam, bool down)
    {
        jeecs::input::keycode code = jeecs::input::keycode::UNKNOWN;
        switch (wParam)
        {
        case VK_SHIFT:
            code = jeecs::input::keycode::L_SHIFT;
            break;
        case VK_CONTROL:
            code = jeecs::input::keycode::L_CTRL;
            break;
        case VK_MENU:
            code = jeecs::input::keycode::L_ALT;
            break;
        case VK_TAB:
            code = jeecs::input::keycode::TAB;
            break;
        case VK_RETURN:
            code = jeecs::input::keycode::ENTER;
            break;
        case VK_ESCAPE:
            code = jeecs::input::keycode::ESC;
            break;
        case VK_BACK:
            code = jeecs::input::keycode::BACKSPACE;
            break;
        default:
            if (wParam >= 'A' && wParam <= 'Z' || wParam >= '0' && wParam <= '9')
                code = (jeecs::input::keycode)wParam;
        }
        je_io_update_keystate(code, down);
    }

    LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_CHAR)
        {
            // NOTE: In WIN32, Only support U16.
            static char multi_byte_record[2];
            static size_t multi_byte_record_index = 0;

            wchar_t wch = 0;

            multi_byte_record[multi_byte_record_index++] = (char)wParam;
            if (multi_byte_record_index >= 2)
            {
                multi_byte_record_index = 0;
                MultiByteToWideChar(CP_ACP, 0, multi_byte_record, 2, &wch, 1);
            }
            else
            {
                if (0 == (multi_byte_record[0] & (char)0x80))
                {
                    multi_byte_record_index = 0;
                    wch = (wchar_t)multi_byte_record[0];
                }
            }
            jegui_win32_append_unicode16_char(wch);
        }
        else
        {
            if (jegui_win32_proc_handler(hwnd, msg, wParam, lParam))
                return 0;
        }


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
            _je_dx_current_thread_context->m_windows_changing_width = (size_t)LOWORD(lParam);
            _je_dx_current_thread_context->m_windows_changing_height = (size_t)HIWORD(lParam);
            if (!_je_dx_current_thread_context->m_windows_changing)
                goto JE_DX11_APPLY_SIZE;
            return 0;
        case WM_ENTERSIZEMOVE:
            _je_dx_current_thread_context->m_windows_changing = true;
            return 0;
        case WM_EXITSIZEMOVE:
        {
            _je_dx_current_thread_context->m_windows_changing = false;
        JE_DX11_APPLY_SIZE:
            dx11_callback_windows_size_changed(
                _je_dx_current_thread_context,
                _je_dx_current_thread_context->m_windows_changing_width,
                _je_dx_current_thread_context->m_windows_changing_height);
            return 0;
        }
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
            je_io_update_mouse_state(0, jeecs::input::mousecode::LEFT, true);
            return 0;
        case WM_MBUTTONDOWN:
            je_io_update_mouse_state(0, jeecs::input::mousecode::MID, true);
            return 0;
        case WM_RBUTTONDOWN:
            je_io_update_mouse_state(0, jeecs::input::mousecode::RIGHT, true);
            return 0;
        case WM_LBUTTONUP:
            je_io_update_mouse_state(0, jeecs::input::mousecode::LEFT, false);
            return 0;
        case WM_MBUTTONUP:
            je_io_update_mouse_state(0, jeecs::input::mousecode::MID, false);
            return 0;
        case WM_RBUTTONUP:
            je_io_update_mouse_state(0, jeecs::input::mousecode::RIGHT, false);
            return 0;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            dx11_handle_key_state(wParam, true);
            break;
        case WM_SYSKEYUP:
        case WM_KEYUP:
            dx11_handle_key_state(wParam, false);
            break;
        case WM_MOUSEWHEEL:
        {
            float wx, wy;
            je_io_get_wheel(0, &wx, &wy);
            je_io_update_wheel(0, wx, wy + GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
            return 0;
        }
        case WM_MOUSEMOVE:
            return 0;
        default:
            break;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // Creates an RGBA icon or cursor
    // (C)GLFW, a little modified.
    static HICON _dx11_CreateIcon(
        unsigned char* image_data,
        LONG width,
        LONG height)
    {
        int i;
        HDC dc;
        HICON handle;
        HBITMAP color, mask;
        BITMAPV5HEADER bi;
        ICONINFO ii;
        unsigned char* target = NULL;
        unsigned char* source = image_data;

        ZeroMemory(&bi, sizeof(bi));
        bi.bV5Size = sizeof(bi);
        bi.bV5Width = width;
        bi.bV5Height = -height;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask = 0x00ff0000;
        bi.bV5GreenMask = 0x0000ff00;
        bi.bV5BlueMask = 0x000000ff;
        bi.bV5AlphaMask = 0xff000000;

        dc = GetDC(NULL);
        color = CreateDIBSection(dc,
            (BITMAPINFO*)&bi,
            DIB_RGB_COLORS,
            (void**)&target,
            NULL,
            (DWORD)0);
        ReleaseDC(NULL, dc);

        if (!color)
        {
            jeecs::debug::logwarn("Failed to create dib section when create icon for dx11 interface window.");
            return NULL;
        }

        mask = CreateBitmap(width, height, 1, 1, NULL);
        if (!mask)
        {
            jeecs::debug::logwarn("Failed to create bitmap when create icon for dx11 interface window.");
            DeleteObject(color);
            return NULL;
        }

        for (i = 0; i < width * height; i++)
        {
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            target[3] = source[3];
            target += 4;
            source += 4;
        }

        ZeroMemory(&ii, sizeof(ii));
        ii.fIcon = TRUE;
        ii.xHotspot = 0;
        ii.yHotspot = 0;
        ii.hbmMask = mask;
        ii.hbmColor = color;

        handle = CreateIconIndirect(&ii);

        DeleteObject(color);
        DeleteObject(mask);

        if (!handle)
            jeecs::debug::logwarn("Failed to create icon for dx11 interface window.");

        return handle;
    }

    jegl_thread::custom_thread_data_t dx11_startup(jegl_thread* gthread, const jegl_interface_config* config, bool reboot)
    {
        jegl_dx11_context* context = new jegl_dx11_context;
        _je_dx_current_thread_context = context;

        context->m_current_target_shader = nullptr;
        context->m_current_target_framebuffer = nullptr;
        context->m_dx_context_finished = false;
        context->m_window_should_close = false;
        context->WINDOWS_SIZE_WIDTH = config->m_width;
        context->WINDOWS_SIZE_HEIGHT = config->m_height;
        context->RESOLUTION_WIDTH = context->WINDOWS_SIZE_WIDTH;
        context->RESOLUTION_HEIGHT = context->WINDOWS_SIZE_HEIGHT;
        context->m_lock_resolution_for_fullscreen = false;
        context->MSAA_LEVEL = config->m_msaa;
        context->FPS = config->m_fps;
        context->m_next_binding_texture_place = 0;
        context->m_win32_window_icon = nullptr;
        context->m_windows_changing = false;
        context->m_windows_changing_width = context->WINDOWS_SIZE_WIDTH;
        context->m_windows_changing_height = context->WINDOWS_SIZE_HEIGHT;

        je_io_update_windowsize(
            (int)context->WINDOWS_SIZE_WIDTH,
            (int)context->WINDOWS_SIZE_HEIGHT);

        if (!reboot)
        {
            jeecs::debug::log("Graphic thread (DX11) start!");
            // ...
        }

        jeecs::basic::resource<jeecs::graphic::texture> icon = jeecs::graphic::texture::load("@/icon.png");
        if (icon == nullptr)
            icon = jeecs::graphic::texture::load("!/builtin/icon/icon.png");

        if (icon != nullptr)
        {
            LONG width = (LONG)icon->width();
            LONG height = (LONG)icon->height();
            unsigned char* pixels = (unsigned char*)malloc((size_t)(width * height * 4));
            // Here need a y-direct flip.
            auto* image_pixels = icon->resouce()->m_raw_texture_data->m_pixels;
            assert(pixels != nullptr);

            for (size_t iy = 0; iy < (size_t)height; ++iy)
            {
                size_t target_place_offset = iy * (size_t)width * 4;
                size_t origin_place_offset = ((size_t)height - iy - 1) * (size_t)width * 4;
                memcpy(pixels + target_place_offset, image_pixels + origin_place_offset, (size_t)width * 4);
            }

            context->m_win32_window_icon = _dx11_CreateIcon(pixels, width, height);
            je_mem_free(pixels);
        }

        context->m_win32_window_class.style = CS_HREDRAW | CS_VREDRAW;
        context->m_win32_window_class.lpfnWndProc = MainWndProc;
        context->m_win32_window_class.cbClsExtra = 0;
        context->m_win32_window_class.cbWndExtra = 0;
        context->m_win32_window_class.hInstance = GetModuleHandle(NULL);
        context->m_win32_window_class.hIcon =
            context->m_win32_window_icon == nullptr
            ? LoadIcon(0, IDI_APPLICATION)
            : context->m_win32_window_icon;
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

        DWORD window_style = WS_OVERLAPPEDWINDOW;

        if (config->m_enable_resize == false)
            window_style &= ~WS_THICKFRAME;

        switch (config->m_display_mode)
        {
        case jegl_interface_config::display_mode::BOARDLESS:
            window_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
            break;
        case jegl_interface_config::display_mode::FULLSCREEN:
            context->m_lock_resolution_for_fullscreen = true;
            break;
        case jegl_interface_config::display_mode::WINDOWED:
            break;
        }

        context->m_window_handle = CreateWindowA("JoyEngineDX11Form", config->m_title,
            window_style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0,
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
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_context, "JoyEngineDx11Context");
        JEDX11_TRACE_DEBUG_NAME(context->m_dx_swapchain, "JoyEngineDx11SwapChain");

        context->m_dx_context_finished = true;

        dx11_callback_windows_size_changed(context,
            context->WINDOWS_SIZE_WIDTH, context->WINDOWS_SIZE_HEIGHT);

        jegui_init_dx11(
            [](auto* res) {
                auto* resource = std::launder(reinterpret_cast<jedx11_texture*>(res->m_handle.m_ptr));
                return (void*)(intptr_t)resource->m_texture_view.Get();
            },
            [](auto* res)
            {
                auto* shader = std::launder(reinterpret_cast<jedx11_shader*>(res->m_handle.m_ptr));
                for (auto& sampler : shader->m_samplers)
                {
                    _je_dx_current_thread_context->m_dx_context->VSSetSamplers(
                        sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
                    _je_dx_current_thread_context->m_dx_context->PSSetSamplers(
                        sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
                }
            },
            context->m_window_handle,
            context->m_dx_device.Get(),
            context->m_dx_context.Get(),
            reboot);

        // Fullscreen?
        if (context->m_lock_resolution_for_fullscreen)
            context->m_dx_swapchain->SetFullscreenState(true, nullptr);

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
        DestroyIcon(context->m_win32_window_icon);

        if (!UnregisterClassA(
            context->m_win32_window_class.lpszClassName,
            context->m_win32_window_class.hInstance))
        {
            jeecs::debug::logfatal("Failed to shutdown windows for dx11: UnregisterClass failed.");
        }
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

    bool dx11_update(jegl_thread::custom_thread_data_t ctx)
    {
        jegl_dx11_context* context =
            std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        MSG msg = { 0 };
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (context->m_window_should_close)
        {
            if (jegui_shutdown_callback())
                return false;
            context->m_window_should_close = false;
        }

        RECT rect;
        GetClientRect(context->m_window_handle, &rect);
        je_io_update_windowsize(rect.right - rect.left, rect.bottom - rect.top);

        POINT point;
        GetCursorPos(&point);
        ScreenToClient(context->m_window_handle, &point);
        je_io_update_mousepos(0, point.x, point.y);

        int mouse_lock_x, mouse_lock_y;
        if (je_io_get_lock_mouse(&mouse_lock_x, &mouse_lock_y))
        {
            point.x = mouse_lock_x;
            point.y = mouse_lock_y;
            ClientToScreen(context->m_window_handle, &point);
            SetCursorPos(point.x, point.y);
        }

        int window_width, window_height;
        if (je_io_fetch_update_windowsize(&window_width, &window_height))
        {
            RECT rect = { 0, 0, window_width, window_height };
            SetWindowPos(context->m_window_handle, HWND_TOP,
                0, 0, rect.right - rect.left, rect.bottom - rect.top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
        }

        const char* title;
        if (je_io_fetch_update_windowtitle(&title))
            SetWindowTextA(context->m_window_handle, title);

        return true;
    }

    bool dx11_pre_update(jegl_thread::custom_thread_data_t ctx)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        JERCHECK(context->m_dx_swapchain->Present(
            context->FPS == 0 ? 1 : 0, 0));
        return true;
    }
    bool dx11_lateupdate(jegl_thread::custom_thread_data_t ctx)
    {
        jegui_update_dx11();
        return true;
    }

    struct dx11_resource_blob
    {
        jegl_dx11_context::MSWRLComPtr<ID3D11InputLayout> m_vao;
        jegl_dx11_context::MSWRLComPtr<ID3DBlob> m_vertex_blob;
        jegl_dx11_context::MSWRLComPtr<ID3DBlob> m_fragment_blob;

        jegl_dx11_context::MSWRLComPtr<ID3D11RasterizerState> m_rasterizer;
        jegl_dx11_context::MSWRLComPtr<ID3D11DepthStencilState> m_depth;
        jegl_dx11_context::MSWRLComPtr<ID3D11BlendState> m_blend;

        std::vector<jedx11_shader::sampler_structs> m_samplers;
        std::unordered_map<std::string, uint32_t> m_ulocations;

        size_t m_uniform_size;

        uint32_t get_built_in_location(const std::string& name)const
        {
            auto fnd = m_ulocations.find(name);
            if (fnd == m_ulocations.end())
                return jeecs::typing::INVALID_UINT32;
            return fnd->second;
        }
    };

    jegl_resource_blob dx11_create_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            bool shader_load_failed = false;

            dx11_resource_blob* blob = new dx11_resource_blob;
            std::string error_informations;
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
                blob->m_vertex_blob.GetAddressOf(),
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
                    blob->m_vertex_blob->GetBufferPointer(),
                    blob->m_vertex_blob->GetBufferSize(),
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
                blob->m_fragment_blob.ReleaseAndGetAddressOf(),
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
                        auto next_edge = last_elem_end_place / DX11_ALLIGN_BASE * DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

                        if (last_elem_end_place + std::min((size_t)16, unit_size) > next_edge)
                            last_elem_end_place = next_edge;

                        blob->m_ulocations[uniforms->m_name] = last_elem_end_place;

                        last_elem_end_place += unit_size;
                    }
                    uniforms = uniforms->m_next;
                }

                if (last_elem_end_place % DX11_ALLIGN_BASE != 0)
                    last_elem_end_place = last_elem_end_place / DX11_ALLIGN_BASE * DX11_ALLIGN_BASE + DX11_ALLIGN_BASE;

                blob->m_uniform_size = last_elem_end_place;

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
                    &rasterizer_describe, blob->m_rasterizer.GetAddressOf()));

                JEDX11_TRACE_DEBUG_NAME(blob->m_rasterizer,
                    string_path + "_RasterizerState");

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
                    &depth_describe, blob->m_depth.GetAddressOf()));

                JEDX11_TRACE_DEBUG_NAME(blob->m_depth,
                    string_path + "_DepthStencilState");

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

    void dx11_close_resource_blob(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob)
    {
        if (blob != nullptr)
            delete (dx11_resource_blob*)blob;
    }

    void dx11_init_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource_blob blob, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            if (blob != nullptr)
            {
                auto* shader_blob = std::launder(reinterpret_cast<dx11_resource_blob*>(blob));

                jedx11_shader* jedx11_shader_res = new jedx11_shader;
                jedx11_shader_res->m_uniform_updated = false;

                std::string string_path = resource->m_path == nullptr
                    ? "__joyengine_builtin_vshader" + std::to_string((intptr_t)resource) + "__"
                    : resource->m_path;

                JERCHECK(context->m_dx_device->CreateVertexShader(
                    shader_blob->m_vertex_blob->GetBufferPointer(),
                    shader_blob->m_vertex_blob->GetBufferSize(),
                    nullptr,
                    jedx11_shader_res->m_vertex.GetAddressOf()));

                JERCHECK(context->m_dx_device->CreatePixelShader(
                    shader_blob->m_fragment_blob->GetBufferPointer(),
                    shader_blob->m_fragment_blob->GetBufferSize(),
                    nullptr,
                    jedx11_shader_res->m_fragment.GetAddressOf()));

                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_m = shader_blob->get_built_in_location("JOYENGINE_TRANS_M");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_v = shader_blob->get_built_in_location("JOYENGINE_TRANS_V");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_p = shader_blob->get_built_in_location("JOYENGINE_TRANS_P");

                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_mvp = shader_blob->get_built_in_location("JOYENGINE_TRANS_MVP");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_mv = shader_blob->get_built_in_location("JOYENGINE_TRANS_MV");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_vp = shader_blob->get_built_in_location("JOYENGINE_TRANS_VP");

                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_tiling = shader_blob->get_built_in_location("JOYENGINE_TEXTURE_TILING");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_offset = shader_blob->get_built_in_location("JOYENGINE_TEXTURE_OFFSET");

                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_light2d_resolution = shader_blob->get_built_in_location("JOYENGINE_LIGHT2D_RESOLUTION");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_light2d_decay = shader_blob->get_built_in_location("JOYENGINE_LIGHT2D_DECAY");

                // ATTENTION: 注意，以下参数特殊shader可能挪作他用
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_local_scale = shader_blob->get_built_in_location("JOYENGINE_LOCAL_SCALE");
                resource->m_raw_shader_data->m_builtin_uniforms.m_builtin_uniform_color = shader_blob->get_built_in_location("JOYENGINE_MAIN_COLOR");

                auto* uniforms = resource->m_raw_shader_data->m_custom_uniforms;
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
                }
                else
                {
                    jedx11_shader_res->m_uniform_cpu_buffers = nullptr;
                }

                jedx11_shader_res->m_vao = shader_blob->m_vao;

                jedx11_shader_res->m_rasterizer = shader_blob->m_rasterizer;
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

            D3D11_TEXTURE2D_DESC texture_describe;
            texture_describe.Width = (UINT)resource->m_raw_texture_data->m_width;
            texture_describe.Height = (UINT)resource->m_raw_texture_data->m_height;
            texture_describe.MipLevels = 1;
            texture_describe.ArraySize = 1;

            bool depth16f = 0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR16);

            switch (resource->m_raw_texture_data->m_format & jegl_texture::format::COLOR_DEPTH_MASK)
            {
            case jegl_texture::format::MONO:
                texture_describe.Format = depth16f ? DXGI_FORMAT_R16_FLOAT : DXGI_FORMAT_R8_UNORM;
                break;
            case jegl_texture::format::RGBA:
                texture_describe.Format = depth16f ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            }

            // 不使用MSAA
            texture_describe.SampleDesc.Count = 1;
            texture_describe.SampleDesc.Quality = 0;

            texture_describe.Usage = D3D11_USAGE_IMMUTABLE;
            texture_describe.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            texture_describe.CPUAccessFlags = 0;
            texture_describe.MiscFlags = 0;

            jedx11_texture* jedx11_texture_res = new jedx11_texture;

            if ((resource->m_raw_texture_data->m_format & jegl_texture::format::FRAMEBUF)
                == jegl_texture::format::FRAMEBUF)
            {
                if (resource->m_raw_texture_data->m_format & jegl_texture::format::DEPTH)
                {
                    texture_describe.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
                    texture_describe.Usage = D3D11_USAGE_DEFAULT;

                    // 不使用MSAA
                    texture_describe.SampleDesc.Count = 1;
                    texture_describe.SampleDesc.Quality = 0;

                    texture_describe.BindFlags = D3D11_BIND_DEPTH_STENCIL;

                    // 创建深度缓冲区以及深度模板视图
                    JERCHECK(context->m_dx_device->CreateTexture2D(
                        &texture_describe, nullptr,
                        jedx11_texture_res->m_texture.GetAddressOf()));
                    // JERCHECK(context->m_dx_device->CreateShaderResourceView(
                    //     jedx11_texture_res->m_texture.Get(), nullptr,
                    //     jedx11_texture_res->m_texture_view.GetAddressOf()));
                }
                else
                {
                    texture_describe.BindFlags |= D3D11_BIND_RENDER_TARGET;
                    texture_describe.Usage = D3D11_USAGE_DEFAULT;

                    JERCHECK(context->m_dx_device->CreateTexture2D(
                        &texture_describe, nullptr, jedx11_texture_res->m_texture.GetAddressOf()));

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

            JEDX11_TRACE_DEBUG_NAME(jedx11_texture_res->m_texture,
                std::string(resource->m_path == nullptr ? "_builtin_texture_" : resource->m_path) + "_Texture");
            JEDX11_TRACE_DEBUG_NAME(jedx11_texture_res->m_texture_view,
                std::string(resource->m_path == nullptr ? "_builtin_texture_" : resource->m_path) + "_View");

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

            JEDX11_TRACE_DEBUG_NAME(vertex->m_vbo,
                std::string(resource->m_path == nullptr ? "_builtin_vertex_" : resource->m_path) + "_Vbo");

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
                jegl_using_resource(attachment->resouce());
                if (0 != (attachment->resouce()->m_raw_texture_data->m_format & jegl_texture::format::DEPTH))
                {
                    if (jedx11_framebuffer_res->m_depth_view.Get() == nullptr)
                    {
                        JERCHECK(context->m_dx_device->CreateDepthStencilView(
                            std::launder(reinterpret_cast<jedx11_texture*>(attachment->resouce()->m_handle.m_ptr))->m_texture.Get(),
                            nullptr, jedx11_framebuffer_res->m_depth_view.GetAddressOf()));

                        JEDX11_TRACE_DEBUG_NAME(jedx11_framebuffer_res->m_depth_view, "Framebuffer_DepthView");
                    }
                    else
                        jeecs::debug::logerr("Framebuffer(%p) attach depth buffer repeatedly.", resource);
                }
                else
                {
                    JERCHECK(context->m_dx_device->CreateRenderTargetView(
                        std::launder(reinterpret_cast<jedx11_texture*>(attachment->resouce()->m_handle.m_ptr))->m_texture.Get(),
                        nullptr, jedx11_framebuffer_res->m_rend_views[color_attachment_count].GetAddressOf()));

                    JEDX11_TRACE_DEBUG_NAME(jedx11_framebuffer_res->m_rend_views[color_attachment_count], "Framebuffer_Color");

                    ++color_attachment_count;
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
    void dx11_close_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource);
    void dx11_using_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            auto* shader = std::launder(reinterpret_cast<jedx11_shader*>(resource->m_handle.m_ptr));
            context->m_current_target_shader = shader;

            context->m_dx_context->VSSetShader(shader->m_vertex.Get(), nullptr, 0);
            context->m_dx_context->PSSetShader(shader->m_fragment.Get(), nullptr, 0);
            context->m_dx_context->IASetInputLayout(shader->m_vao.Get());

            context->m_dx_context->RSSetState(shader->m_rasterizer.Get());
            float _useless[4] = {};
            context->m_dx_context->OMSetBlendState(shader->m_blend.Get(), _useless, UINT_MAX);
            context->m_dx_context->OMSetDepthStencilState(shader->m_depth.Get(), 0);

            for (auto& sampler : shader->m_samplers)
            {
                context->m_dx_context->VSSetSamplers(
                    sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
                context->m_dx_context->PSSetSamplers(
                    sampler.m_sampler_id, 1, sampler.m_sampler.GetAddressOf());
            }
            break;
        }
        case jegl_resource::type::TEXTURE:
        {
            if (resource->m_raw_texture_data != nullptr)
            {
                if (resource->m_raw_texture_data->m_modified)
                {
                    resource->m_raw_texture_data->m_modified = false;
                    // Modified, free current resource id, reload one.
                    dx11_close_resource(ctx, resource);
                    resource->m_handle.m_ptr = nullptr;
                    dx11_init_resource(ctx, nullptr, resource);
                }
            }
            auto* texture = std::launder(reinterpret_cast<jedx11_texture*>(resource->m_handle.m_ptr));
            if (texture->m_texture_view.Get() != nullptr)
            {
                context->m_dx_context->VSSetShaderResources(
                    context->m_next_binding_texture_place, 1, texture->m_texture_view.GetAddressOf());
                context->m_dx_context->PSSetShaderResources(
                    context->m_next_binding_texture_place, 1, texture->m_texture_view.GetAddressOf());
            }
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
    void dx11_close_resource(jegl_thread::custom_thread_data_t ctx, jegl_resource* resource)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        switch (resource->m_type)
        {
        case jegl_resource::type::SHADER:
        {
            auto* shader = std::launder(reinterpret_cast<jedx11_shader*>(resource->m_handle.m_ptr));
            if (shader->m_uniform_buffer_size != 0)
            {
                assert(shader->m_uniform_cpu_buffers != nullptr);
                free(shader->m_uniform_cpu_buffers);
            }
            delete shader;
            break;
        }
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

    void dx11_draw_vertex_with_shader(jegl_thread::custom_thread_data_t ctx, jegl_resource* vert)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        assert(vert->m_type == jegl_resource::type::VERTEX);

        if (context->m_current_target_shader != nullptr
            && context->m_current_target_shader->m_uniform_buffer_size != 0)
        {
            if (context->m_current_target_shader->m_uniform_updated)
            {
                context->m_current_target_shader->m_uniform_updated = false;
                D3D11_MAPPED_SUBRESOURCE mappedData;
                JERCHECK(context->m_dx_context->Map(
                    context->m_current_target_shader->m_uniforms.Get(), 0,
                    D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

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

        jegl_using_resource(vert);

        auto* vertex = std::launder(reinterpret_cast<jedx11_vertex*>(vert->m_handle.m_ptr));
        context->m_dx_context->Draw(vertex->m_count, 0);
    }

    void dx11_bind_texture(jegl_thread::custom_thread_data_t ctx, jegl_resource* texture, size_t pass)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));
        context->m_next_binding_texture_place = pass;
        jegl_using_resource(texture);
    }

    void dx11_set_rend_to_framebuffer(jegl_thread::custom_thread_data_t ctx, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        if (framebuffer == nullptr)
        {
            context->m_dx_context->OMSetRenderTargets(1,
                context->m_dx_main_renderer_target_view.GetAddressOf(),
                context->m_dx_main_renderer_target_depth_view.Get());
            context->m_current_target_framebuffer = nullptr;
        }
        else
        {
            jegl_using_resource(framebuffer);
            context->m_current_target_framebuffer =
                (jedx11_framebuffer*)framebuffer->m_handle.m_ptr;
        }

        auto* framw_buffer_raw = framebuffer != nullptr ? framebuffer->m_raw_framebuf_data : nullptr;
        size_t buf_w, buf_h;
        buf_w = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_width : context->WINDOWS_SIZE_WIDTH;
        buf_h = framw_buffer_raw != nullptr ? framebuffer->m_raw_framebuf_data->m_height : context->WINDOWS_SIZE_HEIGHT;

        if (w == 0)
            w = buf_w;
        if (h == 0)
            h = buf_h;

        y = buf_h - y - h;

        D3D11_VIEWPORT viewport;
        viewport.Width = (float)w;
        viewport.Height = (float)h;
        viewport.TopLeftX = (float)x;
        viewport.TopLeftY = (float)y;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        context->m_dx_context->RSSetViewports(1, &viewport);
    }

    void dx11_clear_framebuffer_color(jegl_thread::custom_thread_data_t ctx, float clear_color[4])
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        if (context->m_current_target_framebuffer == nullptr)
            context->m_dx_context->ClearRenderTargetView(
                context->m_dx_main_renderer_target_view.Get(), clear_color);
        else
        {
            for (auto& target : context->m_current_target_framebuffer->m_rend_views)
                context->m_dx_context->ClearRenderTargetView(
                    target.Get(), clear_color);
        }
    }
    void dx11_clear_framebuffer_depth(jegl_thread::custom_thread_data_t ctx)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        if (context->m_current_target_framebuffer == nullptr)
            context->m_dx_context->ClearDepthStencilView(
                context->m_dx_main_renderer_target_depth_view.Get(),
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        else
        {
            if (context->m_current_target_framebuffer->m_depth_view.Get() != nullptr)
                context->m_dx_context->ClearDepthStencilView(
                    context->m_current_target_framebuffer->m_depth_view.Get(),
                    D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        }
    }
    void dx11_set_uniform(jegl_thread::custom_thread_data_t ctx, uint32_t location, jegl_shader::uniform_type type, const void* val)
    {
        jegl_dx11_context* context = std::launder(reinterpret_cast<jegl_dx11_context*>(ctx));

        assert(context->m_current_target_shader != nullptr);

        if (location == jeecs::typing::INVALID_UINT32)
            return;

        context->m_current_target_shader->m_uniform_updated = true;
        assert(context->m_current_target_shader->m_uniform_buffer_size != 0);
        assert(context->m_current_target_shader->m_uniform_cpu_buffers != nullptr);

        size_t data_size_byte_length = 0;
        switch (type)
        {
        case jegl_shader::INT:
        case jegl_shader::FLOAT:
            memcpy(reinterpret_cast<void*>((intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location), val, 4);
            break;
        case jegl_shader::FLOAT2:
            memcpy(reinterpret_cast<void*>((intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location), val, 8);
            break;
        case jegl_shader::FLOAT3:
            memcpy(reinterpret_cast<void*>((intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location), val, 12);
            break;
        case jegl_shader::FLOAT4:
            memcpy(reinterpret_cast<void*>((intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location), val, 16);
            break;
        case jegl_shader::FLOAT4X4:
            memcpy(reinterpret_cast<void*>((intptr_t)context->m_current_target_shader->m_uniform_cpu_buffers + location), val, 64);
            break;
        default:
            jeecs::debug::logerr("Unknown uniform variable type to set."); break;
            break;
        }
    }
}

void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    using namespace jeecs::graphic::api::dx11;

    write_to_apis->init_interface = dx11_startup;
    write_to_apis->shutdown_interface = dx11_shutdown;

    write_to_apis->pre_update_interface = dx11_pre_update;
    write_to_apis->update_interface = dx11_update;
    write_to_apis->late_update_interface = dx11_lateupdate;

    write_to_apis->create_resource_blob = dx11_create_resource_blob;
    write_to_apis->close_resource_blob = dx11_close_resource_blob;

    write_to_apis->init_resource = dx11_init_resource;
    write_to_apis->using_resource = dx11_using_resource;
    write_to_apis->close_resource = dx11_close_resource;

    write_to_apis->draw_vertex = dx11_draw_vertex_with_shader;
    write_to_apis->bind_texture = dx11_bind_texture;

    write_to_apis->set_rend_buffer = dx11_set_rend_to_framebuffer;
    write_to_apis->clear_rend_buffer_color = dx11_clear_framebuffer_color;
    write_to_apis->clear_rend_buffer_depth = dx11_clear_framebuffer_depth;

    write_to_apis->set_uniform = dx11_set_uniform;
}

#else
void jegl_using_dx11_apis(jegl_graphic_api* write_to_apis)
{
    jeecs::debug::logfatal("DX11 not available.");
}
#endif