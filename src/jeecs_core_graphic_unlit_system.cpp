#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_graphic_system.hpp"

namespace jeecs::graphic
{
    using namespace slice_requirement;

    UnlitGraphicPipelineSystem::UnlitGraphicPipelineSystem(game_world w)
        : BaseImpledGraphicPipeline(w)
    {
    }

    UnlitGraphicPipelineSystem::~UnlitGraphicPipelineSystem()
    {
    }

    void UnlitGraphicPipelineSystem::GraphicUpdate()
    {
        auto WINDOWS_SIZE = jeecs::input::windowsize();
        WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
        WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

        m_camera_list.clear();
        m_renderer_list.clear();

        this->branch_allocate_begin();

        UnlitGraphicPipelineSystem::PrepareCameras();

        for (auto&& [
            projection,
            rendqueue,
            cameraviewport,
            rendbuf,
            frustumCulling,
            clear
        ] :
        query<
            view typesof(
                Projection&,
                Rendqueue*,
                Viewport*,
                RendToFramebuffer*,
                FrustumCulling*,
                Clear*
            )
        >())
        {
            auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
            m_camera_list.emplace(
                camera_arch{
                    branch, rendqueue, &projection, cameraviewport, rendbuf, frustumCulling, clear
                });
        }

        for (auto&& [trans, shads, texs, texarr, shape, rendqueue, color] : query<
            view typesof(
                Translation&,
                Shaders&,
                Textures2D*,
                Textures2DArray*,
                Shape&,
                Rendqueue*,
                Color*
            ),
            except typesof(
                Point,
                Parallel,
                Element
            )
            >())
        {
            // RendOb will be input to a chain and used for swap
            m_renderer_list.emplace(
                renderer_arch{
                    color, rendqueue, &trans, &shape, &shads, texs,
                    nullptr, nullptr, nullptr, nullptr, texarr
                });
        }

        this->branch_allocate_end();
        DrawFrame();
    }

    void UnlitGraphicPipelineSystem::DrawFrame()
    {
        const double current_time = je_clock_time();

        math::vec4 shader_time =
        {
            (float)current_time,
            (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
            (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
            (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

        for (auto& current_camera : m_camera_list)
        {
            graphic::framebuffer* rend_aim_buffer = nullptr;
            if (current_camera.rendToFramebuffer)
            {
                if (current_camera.rendToFramebuffer->framebuffer.has_value())
                    rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->get();
                else
                    continue;
            }

            const size_t
                RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

            const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
            const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
            const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

            update_default_uniform_buffer(
                *current_camera.projection, MAT4_VIEW, MAT4_PROJECTION, MAT4_VP, shader_time);

            je_gl_command_buffer_t* rend_chain = new_rend_chain_for_camera(
                current_camera.branchPipeline, current_camera.viewport,
                rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

            clear_color_if_any(rend_chain, current_camera.clear);
            je_graphic_cmd_clear_depth(rend_chain, 1.0);
            je_graphic_cmd_bind_uniform_buffer(rend_chain,
                current_camera.projection->default_uniform_buffer->resource());

            draw_world_renderers(rend_chain, current_camera.frustumCulling, MAT4_VIEW, MAT4_VP);
        }
    }
}
