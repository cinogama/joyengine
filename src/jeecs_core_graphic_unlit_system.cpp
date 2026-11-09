#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_graphic_system.hpp"

namespace jeecs::graphic
{
    using namespace slice_requirement;

    UnlitGraphicPipelineSystem::UnlitGraphicPipelineSystem(game_world w)
        : GraphicPipelineBase(w)
    {
    }

    UnlitGraphicPipelineSystem::~UnlitGraphicPipelineSystem()
    {
    }

    void UnlitGraphicPipelineSystem::GraphicUpdate()
    {
        begin_frame();

        collect_cameras(m_cameras);

        m_renderers.clear();
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
                Range,
                Element
            )
            >())
        {
            m_renderers.push_back(
                world_renderer_arch{
                    color, rendqueue, &trans, &shape, &shads, texs, texarr
                });
        }
        sort_by_rend_queue(m_renderers);

        end_frame();
        DrawFrame();
    }

    void UnlitGraphicPipelineSystem::DrawFrame()
    {
        const math::vec4 shader_time = current_shader_time();

        for (auto& current_camera : m_cameras)
        {
            const auto target = resolve_camera_target(current_camera);
            if (!target.has_value())
                continue;

            const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
            const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
            const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

            update_default_uniform_buffer(
                *current_camera.projection, MAT4_VIEW, MAT4_PROJECTION, MAT4_VP, shader_time);

            je_gl_command_buffer_t* rend_chain = begin_camera_pass(
                current_camera, current_camera.viewport,
                target->framebuffer, target->width, target->height);

            draw_world_renderers(
                rend_chain, m_renderers, current_camera.frustumCulling, MAT4_VIEW, MAT4_VP);
        }
    }
}
