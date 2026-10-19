/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
    uint32_t _je_test_align_up(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    void _je_test_uniform_layout(
        jegl_shader::uniform_type type, uint32_t* out_size, uint32_t* out_align)
    {
        switch (type)
        {
        case jegl_shader::uniform_type::INT:
        case jegl_shader::uniform_type::FLOAT:
            *out_size = 4;
            *out_align = 4;
            break;
        case jegl_shader::uniform_type::INT2:
        case jegl_shader::uniform_type::FLOAT2:
            *out_size = 8;
            *out_align = 8;
            break;
        case jegl_shader::uniform_type::INT3:
        case jegl_shader::uniform_type::FLOAT3:
            *out_size = 12;
            *out_align = 16;
            break;
        case jegl_shader::uniform_type::INT4:
        case jegl_shader::uniform_type::FLOAT4:
            *out_size = 16;
            *out_align = 16;
            break;
        case jegl_shader::uniform_type::FLOAT2X2:
            *out_size = 16;
            *out_align = 8;
            break;
        case jegl_shader::uniform_type::FLOAT3X3:
            *out_size = 48;
            *out_align = 16;
            break;
        case jegl_shader::uniform_type::FLOAT4X4:
            *out_size = 64;
            *out_align = 16;
            break;
        case jegl_shader::uniform_type::TEXTURE:
            *out_size = 0;
            *out_align = 1;
            break;
        default:
            *out_size = 0;
            *out_align = 0;
            break;
        }
    }

    const char* _je_test_uniform_type_name(jegl_shader::uniform_type type)
    {
        switch (type)
        {
        case jegl_shader::uniform_type::INT: return "int";
        case jegl_shader::uniform_type::INT2: return "int2";
        case jegl_shader::uniform_type::INT3: return "int3";
        case jegl_shader::uniform_type::INT4: return "int4";
        case jegl_shader::uniform_type::FLOAT: return "float";
        case jegl_shader::uniform_type::FLOAT2: return "float2";
        case jegl_shader::uniform_type::FLOAT3: return "float3";
        case jegl_shader::uniform_type::FLOAT4: return "float4";
        case jegl_shader::uniform_type::FLOAT2X2: return "float2x2";
        case jegl_shader::uniform_type::FLOAT3X3: return "float3x3";
        case jegl_shader::uniform_type::FLOAT4X4: return "float4x4";
        case jegl_shader::uniform_type::TEXTURE: return "texture";
        default: return "<unknown>";
        }
    }

    int _je_test_render_recording(jegl_shader* shader)
    {
        int failures = 0;
        auto fail = [&failures](const char* message, auto... args)
        {
            ++failures;
            std::fprintf(stderr, "[render-recording-test] FAIL: ");
            std::fprintf(stderr, message, args...);
            std::fprintf(stderr, "\n");
        };

        // The command buffer never commits in this test, so a zero-initialized
        // context is only used as an identity token.
        jegl_context fake_context{};

        jegl_pipeline_desc pipeline_desc{};
        pipeline_desc.m_shader = shader;
        pipeline_desc.m_state = nullptr;

        jegl_pipeline* pipeline =
            jegl_pipeline_create(&fake_context, &pipeline_desc);
        const auto* builtin_slots =
            jegl_pipeline_builtin_uniform_slots(pipeline);

        const jegl_uniform_slot color_slot =
            jegl_pipeline_uniform_slot(pipeline, "JE_COLOR");
        if (color_slot.m_offset != 0 || color_slot.m_size != 16)
            fail("JE_COLOR slot is %u/%u, expected 0/16",
                color_slot.m_offset, color_slot.m_size);
        if (builtin_slots->m_color.m_offset != color_slot.m_offset)
            fail("builtin JE_COLOR slot disagrees with name lookup");

        const jegl_uniform_slot missing_slot =
            jegl_pipeline_uniform_slot(pipeline, "TEST_MISSING_UNIFORM");
        if (missing_slot.m_offset != JEGL_UNIFORM_SLOT_NONE)
            fail("missing uniform did not return JEGL_UNIFORM_SLOT_NONE");

        jegl_sampler_desc sampler_desc{};
        sampler_desc.m_min_filter = jegl_filter::NEAREST;
        sampler_desc.m_mag_filter = jegl_filter::NEAREST;
        sampler_desc.m_mip_filter = jegl_mip_filter::NEAREST;
        sampler_desc.m_u_wrap = jegl_wrap_mode::CLAMP_TO_EDGE;
        sampler_desc.m_v_wrap = jegl_wrap_mode::CLAMP_TO_EDGE;
        sampler_desc.m_w_wrap = jegl_wrap_mode::CLAMP_TO_EDGE;
        jegl_sampler* sampler =
            jegl_sampler_create(&fake_context, &sampler_desc);

        const float vertices[] = {
            -1.f, -1.f, 0.f, 0.f, 0.f,
             1.f, -1.f, 0.f, 1.f, 0.f,
             0.f,  1.f, 0.f, 0.5f, 1.f,
        };
        const uint32_t indices[] = { 0, 1, 2 };
        const jegl_vertex::data_layout layouts[] = {
            { jegl_vertex::data_type::FLOAT32, 3 },
            { jegl_vertex::data_type::FLOAT32, 2 },
        };
        jegl_vertex* vertex = jegl_create_vertex(
            jegl_vertex::type::TRIANGLES,
            vertices,
            sizeof(vertices),
            indices,
            3,
            layouts,
            2);
        if (vertex == nullptr)
            fail("failed to create the test vertex");

        jegl_texture_2d* texture = jegl_create_texture_2d(
            1, 1, jegl_texture_common::format::RGBA);

        jegl_command_buffer* cmd = jegl_cmd_create(&fake_context);
        const jegl_viewport viewport{ 0, 0, 64, 64 };

        jegl_cmd_begin_pass(cmd, nullptr, &viewport);
        jegl_cmd_bind_pipeline(cmd, pipeline);
        jegl_cmd_bind_vertex(cmd, vertex);

        jegl_cmd_set_uniform_float4(cmd, color_slot, 1.f, 1.f, 1.f, 1.f);

        float mvp[4][4] = {
            { 1.f, 0.f, 0.f, 0.f },
            { 0.f, 1.f, 0.f, 0.f },
            { 0.f, 0.f, 1.f, 0.f },
            { 0.f, 0.f, 0.f, 1.f },
        };
        if (builtin_slots->m_model_view_projection.m_offset != JEGL_UNIFORM_SLOT_NONE)
            jegl_cmd_set_uniform_float4x4(
                cmd, builtin_slots->m_model_view_projection, mvp);

        jegl_cmd_bind_texture_2d(cmd, 0, texture);
        jegl_cmd_bind_sampler(cmd, 0, sampler);
        jegl_cmd_draw_indexed(cmd, 3, 0, 1);
        jegl_cmd_end_pass(cmd);

        jegl_cmd_reset(cmd);
        jegl_cmd_close(cmd);

        jegl_close_texture_2d(texture);
        jegl_close_vertex(vertex);
        jegl_close_sampler(sampler);
        jegl_close_pipeline(pipeline);

        std::printf(
            "[render-recording-test] %s\n",
            failures == 0 ? "OK" : "FAILED");
        return failures;
    }

    int _je_test_shader_layout()
    {
        jegl_shader* shader =
            jegl_load_shader(nullptr, "@/builtin/shader/Unlit.shader");

        if (shader == nullptr)
        {
            std::fprintf(stderr,
                "[shader-layout-test] failed to load builtin/shader/Unlit.shader\n");
            return 1;
        }

        int failures = 0;
        auto fail = [&failures](const char* message, auto... args)
        {
            ++failures;
            std::fprintf(stderr, "[shader-layout-test] FAIL: ");
            std::fprintf(stderr, message, args...);
            std::fprintf(stderr, "\n");
        };

        uint32_t expected_offset = 0;
        uint32_t max_align = 4;
        uint32_t last_align = 16;
        size_t uniform_count = 0;
        size_t texture_count = 0;

        std::printf("[shader-layout-test] shader uniforms:\n");
        for (auto* uniform = shader->m_custom_uniforms;
            uniform != nullptr;
            uniform = uniform->m_next)
        {
            if (uniform->m_uniform_type == jegl_shader::uniform_type::TEXTURE)
            {
                ++texture_count;
                if (uniform->m_offset != jeecs::graphic::INVALID_UNIFORM_LOCATION)
                    fail("texture uniform `%s` has a non-invalid offset", uniform->m_name);
                continue;
            }

            uint32_t size, align;
            _je_test_uniform_layout(uniform->m_uniform_type, &size, &align);
            if (size == 0)
            {
                fail("uniform `%s` has unknown type %d",
                    uniform->m_name, (int)uniform->m_uniform_type);
                continue;
            }

            if (align > last_align)
                fail("uniform `%s` is not sorted by alignment (%u > %u)",
                    uniform->m_name, align, last_align);
            last_align = align;

            expected_offset = _je_test_align_up(expected_offset, align);
            if (uniform->m_offset != expected_offset)
                fail("uniform `%s` offset %u, expected %u",
                    uniform->m_name, uniform->m_offset, expected_offset);
            if (uniform->m_size != size)
                fail("uniform `%s` size %u, expected %u",
                    uniform->m_name, uniform->m_size, size);

            std::printf("  %-24s %-8s offset=%u size=%u\n",
                uniform->m_name,
                _je_test_uniform_type_name(uniform->m_uniform_type),
                uniform->m_offset,
                uniform->m_size);

            expected_offset += size;
            max_align = std::max(max_align, align);
            ++uniform_count;
        }

        const uint32_t expected_block = _je_test_align_up(expected_offset, max_align);
        if (shader->m_uniform_block_size != expected_block)
            fail("uniform block size %u, expected %u",
                shader->m_uniform_block_size, expected_block);

        if (shader->m_texture_count != texture_count)
            fail("texture count %zu, expected %zu",
                shader->m_texture_count, texture_count);

        auto find_uniform = [shader](const char* name) -> uint32_t
        {
            for (auto* uniform = shader->m_custom_uniforms;
                uniform != nullptr;
                uniform = uniform->m_next)
            {
                if (uniform->m_uniform_type != jegl_shader::uniform_type::TEXTURE
                    && std::strcmp(uniform->m_name, name) == 0)
                    return uniform->m_offset;
            }
            return jeecs::graphic::INVALID_UNIFORM_LOCATION;
        };

        auto check_builtin = [&](const char* name, uint32_t slot)
        {
            const uint32_t expected = find_uniform(name);
            if (expected != slot)
                fail("builtin `%s` offset %u, expected %u", name, slot, expected);
        };

        check_builtin("JE_NDC_SCALE", shader->m_builtin_uniforms.m_builtin_uniform_ndc_scale);
        check_builtin("JE_M", shader->m_builtin_uniforms.m_builtin_uniform_m);
        check_builtin("JE_MV", shader->m_builtin_uniforms.m_builtin_uniform_mv);
        check_builtin("JE_MVP", shader->m_builtin_uniforms.m_builtin_uniform_mvp);
        check_builtin("JE_LOCAL_SCALE", shader->m_builtin_uniforms.m_builtin_uniform_local_scale);
        check_builtin("JE_UV_TILING", shader->m_builtin_uniforms.m_builtin_uniform_tiling);
        check_builtin("JE_UV_OFFSET", shader->m_builtin_uniforms.m_builtin_uniform_offset);
        check_builtin("JE_COLOR", shader->m_builtin_uniforms.m_builtin_uniform_color);
        check_builtin("JE_LIGHT2D_RESOLUTION", shader->m_builtin_uniforms.m_builtin_uniform_light2d_resolution);
        check_builtin("JE_LIGHT2D_DECAY", shader->m_builtin_uniforms.m_builtin_uniform_light2d_decay);
        check_builtin("JE_SHADOW_ELLIPSE", shader->m_builtin_uniforms.m_builtin_uniform_shadow_ellipse);

        std::printf("[shader-layout-test] textures:\n");
        for (size_t i = 0; i < shader->m_texture_count; ++i)
        {
            const auto& texture = shader->m_textures[i];
            if (texture.m_name == nullptr)
                fail("texture[%zu] has a null name", i);

            std::printf("  %-24s pass=%u sampler=%u kind=%d\n",
                texture.m_name != nullptr ? texture.m_name : "<null>",
                texture.m_pass,
                texture.m_sampler_id,
                (int)texture.m_kind);
        }

        failures += _je_test_render_recording(shader);

        std::printf(
            "[shader-layout-test] block=%u uniforms=%zu textures=%zu: %s\n",
            shader->m_uniform_block_size,
            uniform_count,
            texture_count,
            failures == 0 ? "OK" : "FAILED");

        jegl_close_shader(shader);
        return failures == 0 ? 0 : 1;
    }
}

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int main(int argc, char** argv)
{
    using namespace jeecs;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-test-shader-layout") == 0)
        {
            je_init(argc, argv);
            const int result = _je_test_shader_layout();
            je_finish();
            return result;
        }
    }

    game_engine_context context(argc, argv);
    context.loop();
}
