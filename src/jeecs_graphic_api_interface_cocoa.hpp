#pragma once

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

namespace jeecs::graphic::metal
{
    struct metal_interface_context
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_interface_context);

        MTL::Device* m_device;
        MTK::View* m_draw_target_view;

        // NOTE: 从外部 GUI 同步得到的上下文，每帧开始前同步一次，保证在绘制回调期间始终有效
        struct gui_context_t
        {
        };
        gui_context_t m_gui_context;

        metal_interface_context(
            NS::Window* graphic_window,
            MTK::ViewDelegate* view_delegate)
            : m_gui_context{}
        {
            m_device = MTL::CreateSystemDefaultDevice();
            m_draw_target_view = MTK::View::alloc()->init(frame, m_device);

            m_mtk_view->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
            m_mtk_view->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));

            m_mtk_view->setDelegate(view_delegate);
            graphic_window->setContentView(m_mtk_view);
        }
        ~metal_interface_context()
        {
            m_draw_target_view->release();
            m_device->release();
        }
    };

    class metal_view_delegate : public MTK::ViewDelegate
    {
        bool m_graphic_request_to_close;
        graphic_syncer_host* m_je_graphic_host;

        metal_interface_context* m_metal_interface_context;

    public:
        metal_view_delegate(
            NS::Window* graphic_window,
            graphic_syncer_host* graphic_host)
            : MTK::ViewDelegate()
            , m_graphic_request_to_close(false)
            , m_je_graphic_host(graphic_host)
            , m_metal_interface_context(nullptr)
        {
            auto* engine_raw_graphic_context =
                m_je_graphic_host->get_graphic_context_after_context_ready();

            m_metal_interface_context =
                new metal_interface_context(
                    graphic_window,
                    this);

            engine_raw_graphic_context->m_config.m_userdata =
                m_metal_interface_context;
        }
        virtual ~metal_view_delegate() override
        {
            if (!m_graphic_request_to_close)
            {
                // NOTE: view delegate has been destructed, but graphic host
                //  has not been requested to close.
                // This may happen when the window is closed by user in macos.

                jegl_sync_shutdown(
                    m_je_graphic_host->get_graphic_context_after_context_ready(),
                    false);
            }
            delete m_metal_interface_context;
        }

        virtual void drawInMTKView(MTK::View* pView) override
        {
            if (m_graphic_request_to_close)
                // Graphic has been requested to close.
                return;

            // TODO: Update gui context here.
            // m_metal_interface_context->m_gui_context.xxx = ...;

            if (!m_je_graphic_host->frame())
            {
                // TODO: Graphic requested to close, close windows and exit app.
                m_graphic_request_to_close = true;
            }
        }
    };
}