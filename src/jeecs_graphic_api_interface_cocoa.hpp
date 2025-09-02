#pragma once

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

namespace jeecs::graphic::metal
{
    struct metal_interface_context
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_interface_context);

        MTL::Device* const m_device;
        MTK::View* const m_draw_target_view;

        // NOTE: 从外部 GUI 同步得到的上下文，每帧开始前同步一次，保证在绘制回调期间始终有效
        struct gui_context_t
        {
        };
        gui_context_t m_gui_context;

        metal_interface_context(
            NS::Window* graphic_window, 
            MTK::ViewDelegate* view_delegate)
            : m_device(MTL::CreateSystemDefaultDevice())
            , m_draw_target_view(MTK::View::alloc()->init(frame, m_device))
            , m_gui_context{}
        {
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
}