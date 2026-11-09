#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_render_helpers.hpp"

namespace jeecs::graphic
{
    using namespace slice_requirement;

    namespace
    {
        inline const float default_shape_quad_data[] = {
            /* vpos, vuv, vnormal, vtangent */
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        };
    }

    DefaultResources::DefaultResources()
        : default_shape_quad{
              graphic::vertex::create(
                  JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                  default_shape_quad_data,
                  sizeof(default_shape_quad_data),
                  {0, 1, 2, 3},
                  {
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                  })
                  .value() },
        default_shader{ jeecs::graphic::shader::load(
                            nullptr,
                            "!/builtin/shader/internal/builtin_default.shader")
                            .value() }
, default_texture{ graphic::texture2d::create(2, 2, JE_GL_TEXTURE_FORMAT_RGBA) }
, default_shaders_list{ default_shader }
    {
        default_texture->pix(0, 0).set({ 1.f, 0.25f, 1.f, 1.f });
        default_texture->pix(1, 1).set({ 1.f, 0.25f, 1.f, 1.f });
        default_texture->pix(0, 1).set({ 0.25f, 0.25f, 0.25f, 1.f });
        default_texture->pix(1, 0).set({ 0.25f, 0.25f, 0.25f, 1.f });
    }

    BaseImpledGraphicPipeline::BaseImpledGraphicPipeline(game_world w)
        : BasePipelineInterface(w, nullptr)
    {
    }

    void BaseImpledGraphicPipeline::PrepareCameras()
    {
        for (auto&& [
            translation,
            projection,
            ortho,
            perspec,
            viewport,
            rendbuf,
            frustumCulling
        ] :
        query<
            view typesof(
                Translation&,
                Projection&,
                OrthoProjection*,
                PerspectiveProjection*,
                Viewport*,
                RendToFramebuffer*,
                FrustumCulling*
            ),
            anyof typesof(
                OrthoProjection,
                PerspectiveProjection
            )
        >())
        {
            float mat_inv_rotation[4][4];
            translation.world_rotation.create_inv_matrix(mat_inv_rotation);

            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation.world_position.x;
            mat_inv_position[3][1] = -translation.world_position.y;
            mat_inv_position[3][2] = -translation.world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            graphic::framebuffer* rend_aim_buffer =
                rendbuf && rendbuf->framebuffer.has_value()
                ? rendbuf->framebuffer.value().get()
                : nullptr;

            const size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT));

            if (ortho)
            {
                graphic::ortho_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, projection.znear, projection.zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, projection.znear, projection.zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, projection.znear, projection.zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, projection.znear, projection.zfar);
            }

            projection.default_uniform_buffer->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                sizeof(projection.view),
                projection.view);
            projection.default_uniform_buffer->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                sizeof(projection.projection),
                projection.projection);

            math::mat4xmat4(projection.view_projection, projection.projection, projection.view);

            if (frustumCulling != nullptr)
            {
                // Left clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    0,
                    projection.view_projection[0][3] + projection.view_projection[0][0],
                    projection.view_projection[1][3] + projection.view_projection[1][0],
                    projection.view_projection[2][3] + projection.view_projection[2][0],
                    projection.view_projection[3][3] + projection.view_projection[3][0]);

                // Right clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    1,
                    projection.view_projection[0][3] - projection.view_projection[0][0],
                    projection.view_projection[1][3] - projection.view_projection[1][0],
                    projection.view_projection[2][3] - projection.view_projection[2][0],
                    projection.view_projection[3][3] - projection.view_projection[3][0]);

                // Top clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    2,
                    projection.view_projection[0][3] - projection.view_projection[0][1],
                    projection.view_projection[1][3] - projection.view_projection[1][1],
                    projection.view_projection[2][3] - projection.view_projection[2][1],
                    projection.view_projection[3][3] - projection.view_projection[3][1]);

                // Bottom clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    3,
                    projection.view_projection[0][3] + projection.view_projection[0][1],
                    projection.view_projection[1][3] + projection.view_projection[1][1],
                    projection.view_projection[2][3] + projection.view_projection[2][1],
                    projection.view_projection[3][3] + projection.view_projection[3][1]);

                // Near clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    4,
                    projection.view_projection[0][3] + projection.view_projection[0][2],
                    projection.view_projection[1][3] + projection.view_projection[1][2],
                    projection.view_projection[2][3] + projection.view_projection[2][2],
                    projection.view_projection[3][3] + projection.view_projection[3][2]);

                // Far clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    5,
                    projection.view_projection[0][3] - projection.view_projection[0][2],
                    projection.view_projection[1][3] - projection.view_projection[1][2],
                    projection.view_projection[2][3] - projection.view_projection[2][2],
                    projection.view_projection[3][3] - projection.view_projection[3][2]);
            }
        }
    }

    math::vec3 BaseImpledGraphicPipeline::get_entity_size(
        const Transform::Translation& trans,
        const basic::optional<basic::resource<graphic::vertex>>& mesh)
    {
        math::vec3 size = trans.local_scale;

        const auto* light_shape =
            mesh.has_value() ? mesh->get() : m_default_resources.default_shape_quad.get();

        const auto* const raw_vertex_data = light_shape->resource();
        size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
        size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
        size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));

        return size;
    }

    void BaseImpledGraphicPipeline::update_default_uniform_buffer(
        const Projection& projection,
        const float(&view)[4][4],
        const float(&proj)[4][4],
        const float(&vp)[4][4],
        const math::vec4& shader_time)
    {
        auto* ub = projection.default_uniform_buffer.get();
        ub->update_buffer(
            offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
            sizeof(view), view);
        ub->update_buffer(
            offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
            sizeof(proj), proj);
        ub->update_buffer(
            offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
            sizeof(vp), vp);
        ub->update_buffer(
            offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
            sizeof(shader_time), &shader_time);
    }

    BaseImpledGraphicPipeline::~BaseImpledGraphicPipeline()
    {
        for (auto& [key, pipeline] : m_pipeline_cache)
            je_graphic_pipeline_close(pipeline);
        m_pipeline_cache.clear();
    }

    je_gl_context_t* BaseImpledGraphicPipeline::graphic_context() const
    {
        je_gl_context_t* context = je_graphic_uhost_get_context(_m_graphic_host);
        assert(context != nullptr);
        return context;
    }

    je_gl_topology_t BaseImpledGraphicPipeline::topology_of(const je_gl_vertex_t* vertex)
    {
        switch (vertex->m_type)
        {
        case JE_GL_VERTEX_TYPE_LINESTRIP:
            return je_gl_topology_t::LINE_STRIP;
        case JE_GL_VERTEX_TYPE_TRIANGLES:
            return je_gl_topology_t::TRIANGLES;
        case JE_GL_VERTEX_TYPE_TRIANGLESTRIP:
            return je_gl_topology_t::TRIANGLE_STRIP;
        default:
            assert(false);
            return je_gl_topology_t::TRIANGLES;
        }
    }

    je_gl_pipeline_t* BaseImpledGraphicPipeline::pipeline_for(
        graphic::shader* shader, je_gl_vertex_t* vertex)
    {
        assert(shader != nullptr);
        assert(vertex != nullptr);

        const auto topology = topology_of(vertex);
        const auto key = std::make_pair(
            shader->resource(), (uint8_t)topology);

        auto found = m_pipeline_cache.find(key);
        if (found != m_pipeline_cache.end())
            return found->second;

        je_gl_pipeline_state_t state =
            je_graphic_pipeline_state_from_shader(shader->resource());
        state.m_topology = topology;

        je_gl_pipeline_desc_t desc{};
        desc.m_shader = shader->resource();
        desc.m_state = &state;

        je_gl_pipeline_t* pipeline =
            je_graphic_pipeline_create(graphic_context(), &desc);
        m_pipeline_cache.emplace(key, pipeline);
        return pipeline;
    }

    je_gl_command_buffer_t* BaseImpledGraphicPipeline::new_rend_chain_for_camera(
        je_render_branch_t* branch,
        const Viewport* viewport,
        graphic::framebuffer* rend_aim_buffer,
        size_t buffer_w, size_t buffer_h)
    {
        assert(buffer_w > 0 && buffer_h > 0);

        je_gl_frame_buffer_t* fb =
            rend_aim_buffer ? rend_aim_buffer->resource() : nullptr;

        je_gl_viewport_t vp{};
        if (viewport != nullptr)
        {
            vp.x = (int32_t)(viewport->viewport.x * (float)buffer_w);
            vp.y = (int32_t)(viewport->viewport.y * (float)buffer_h);
            vp.width = (uint32_t)(viewport->viewport.z * (float)buffer_w);
            vp.height = (uint32_t)(viewport->viewport.w * (float)buffer_h);
        }
        else
        {
            vp.x = 0;
            vp.y = 0;
            vp.width = (uint32_t)buffer_w;
            vp.height = (uint32_t)buffer_h;
        }

        assert(vp.width > 0 && vp.height > 0);
        return je_graphic_branch_new_command_buffer(branch, fb, &vp);
    }

    void BaseImpledGraphicPipeline::clear_color_if_any(
        je_gl_command_buffer_t* cmd, const Camera::Clear* clear)
    {
        if (clear != nullptr)
        {
            const math::vec4 clear_color = clear->color.to_linear();
            const float clear_buffer_color[] = {
                clear_color.x, clear_color.y, clear_color.z, clear_color.w };
            je_graphic_cmd_clear_color(cmd, 0, clear_buffer_color);
        }
    }

    BaseImpledGraphicPipeline::bound_textures_t BaseImpledGraphicPipeline::bind_entity_textures(je_gl_command_buffer_t* cmd, const Textures2D* textures,
        const Textures2DArray* textures_array)
    {
        bound_textures_t result{
            math::vec2(1.f, 1.f), math::vec2(0.f, 0.f) };

        je_graphic_cmd_bind_texture_2d(
            cmd, 0, m_default_resources.default_texture->resource());

        if (textures != nullptr)
        {
            result.tiling = textures->tiling;
            result.offset = textures->offset;
            for (auto& texture : textures->textures)
                je_graphic_cmd_bind_texture_2d(
                    cmd, texture.m_pass_id, texture.m_texture->resource());
        }

        // 数组纹理在 2D 绑定之后写入同通道（后写覆盖，各后端语义一致）
        if (textures_array != nullptr)
            for (auto& texture : textures_array->textures)
                je_graphic_cmd_bind_texture_array(
                    cmd, texture.m_pass_id, texture.m_texture->resource());
        return result;
    }

    void BaseImpledGraphicPipeline::draw_shader_passes(
        je_gl_command_buffer_t* cmd,
        const basic::resource<graphic::vertex>& shape,
        const basic::vector<basic::resource<graphic::shader>>& shaders,
        const pass_uniforms_t& u)
    {
        auto* vertex_resource = shape->resource();

        for (auto& shader_pass : shaders)
        {
            auto* using_shader = &shader_pass;
            if (!shader_pass->m_builtin)
                using_shader = &m_default_resources.default_shader;

            auto* pipeline = pipeline_for(
                (*using_shader).get(), vertex_resource);
            je_graphic_cmd_bind_pipeline(cmd, pipeline);
            je_graphic_cmd_bind_vertex(cmd, vertex_resource);
            render_builtin_uniforms uniforms(cmd, pipeline);

            if (u.m != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model, float4x4, u.m);
            if (u.mv != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view, float4x4, u.mv);
            if (u.mvp != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view_projection, float4x4, u.mvp);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_local_scale, float3,
                u.local_scale.x, u.local_scale.y, u.local_scale.z);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2, u.tiling.x, u.tiling.y);
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2, u.offset.x, u.offset.y);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                u.color.x, u.color.y, u.color.z, u.color.w);

            if (u.light2d_resolution != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_light2d_resolution, float2,
                    u.light2d_resolution->x, u.light2d_resolution->y);
            if (u.light2d_decay != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_light2d_decay, float, *u.light2d_decay);

            je_graphic_cmd_draw_indexed(
                cmd, (uint32_t)vertex_resource->m_index_count, 0, 1);
        }
    }

    void BaseImpledGraphicPipeline::draw_world_renderers(
        je_gl_command_buffer_t* cmd,
        const FrustumCulling* frustum_culling,
        const float(&view)[4][4],
        const float(&vp)[4][4])
    {
        for (auto& rendentity : m_renderer_list)
        {
            assert(rendentity.translation != nullptr
                && rendentity.shaders != nullptr
                && rendentity.shape != nullptr);

            const float entity_range = 0.5f *
                get_entity_size(*rendentity.translation, rendentity.shape->vertex).length();

            if (frustum_culling != nullptr)
            {
                if (false == frustum_culling->test_circle(
                    rendentity.translation->world_position, entity_range))
                    continue;
            }

            auto& drawing_shape =
                rendentity.shape->vertex.has_value()
                ? rendentity.shape->vertex.value()
                : m_default_resources.default_shape_quad;
            auto& drawing_shaders =
                rendentity.shaders->shaders.empty() == false
                ? rendentity.shaders->shaders
                : m_default_resources.default_shaders_list;

            const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
            float MAT4_MV[4][4], MAT4_MVP[4][4];
            math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);
            math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);

            auto bound = bind_entity_textures(cmd, rendentity.textures, rendentity.textures_array);

            pass_uniforms_t u;
            u.m = MAT4_MODEL;
            u.mv = MAT4_MV;
            u.mvp = MAT4_MVP;
            u.local_scale = rendentity.translation->local_scale;
            u.tiling = bound.tiling;
            u.offset = bound.offset;
            if (rendentity.color != nullptr)
                u.color = rendentity.color->color.to_linear();

            draw_shader_passes(cmd, drawing_shape, drawing_shaders, u);
        }
    }
}
