#pragma once

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

namespace jeecs::graphic::metal
{
    struct metal_interface_context
    {
        JECS_DISABLE_MOVE_AND_COPY(metal_interface_context);

        MTL::Device* const m_retained_device;

        // NOTE: 从外部 GUI 同步得到的上下文，每帧开始前同步一次，保证在绘制回调期间始终有效
        struct gui_context_t
        {
            MTK::View* m_draw_target_view;
        };
        gui_context_t m_gui_context;

        metal_interface_context(MTL::Device* device)
            : m_retained_device(device->retain())
            , m_gui_context{}
        {
        }
        ~metal_interface_context()
        {
            m_retained_device->release();
        }
    };
}