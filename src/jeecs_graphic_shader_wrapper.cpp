#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/cache.hpp"
#include "jeecs/core.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/logging.hpp"
#include "jeecs_basic_bootup.hpp"
#include "jeecs_basic_cache_version.hpp"
#include "jeecs_graphic_shader_wrapper.hpp"
#include "jeecs_graphic_render_internal.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace jeecs::graphic::shader_wrapper {}

using namespace std;
using namespace jeecs;
using namespace jeecs::graphic::shader_wrapper;

#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR
#   include <glslang_c_interface.h>
#   include <resource_limits_c.h>
#   include <spirv_cross_c.h>
#endif

namespace jeecs::graphic::shader_wrapper
{
    namespace
    {
        void _jegl_get_uniform_layout(
            je_gl_shader_uniform_type_t type, uint32_t* out_size, uint32_t* out_align)
        {
            switch (type)
            {
            case JE_GL_SHADER_UNIFORM_TYPE_INT:
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT:
                *out_size = 4;
                *out_align = 4;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT2:
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2:
                *out_size = 8;
                *out_align = 8;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT3:
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3:
                *out_size = 12;
                *out_align = 16;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT4:
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4:
                *out_size = 16;
                *out_align = 16;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2X2:
                *out_size = 16;
                *out_align = 8;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3X3:
                *out_size = 48;
                *out_align = 16;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4X4:
                *out_size = 64;
                *out_align = 16;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_TEXTURE:
                *out_size = 0;
                *out_align = 1;
                break;
            default:
                assert(false);
                *out_size = 0;
                *out_align = 1;
                break;
            }
        }

        uint32_t _jegl_align_up(uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) / alignment * alignment;
        }

        // Computes the canonical std140 layout of the shader's per-draw uniform
        // block. Every backend and the command-buffer recorder use these offsets,
        // so the layout must stay stable across backends; the shader generator
        // sorts uniforms by packing rank to keep HLSL cbuffer packing identical to
        // std140 (see je/shader/shader.wo).
        void _jegl_resolve_canonical_uniform_layout(je_gl_shader_t* shader)
        {
            uint32_t offset = 0;
            uint32_t max_align = 4;

            for (auto* uniform = shader->m_custom_uniforms;
                uniform != nullptr;
                uniform = uniform->m_next)
            {
                uint32_t size, align;
                _jegl_get_uniform_layout(uniform->m_uniform_type, &size, &align);

                if (size == 0)
                {
                    uniform->m_offset = INVALID_UNIFORM_LOCATION;
                    uniform->m_size = 0;
                    continue;
                }

                offset = _jegl_align_up(offset, align);
                uniform->m_offset = offset;
                uniform->m_size = size;
                offset += size;
                max_align = max(max_align, align);
            }

            shader->m_uniform_block_size = _jegl_align_up(offset, max_align);

            auto resolve = [shader](const char* name) -> uint32_t
                {
                    for (auto* uniform = shader->m_custom_uniforms;
                        uniform != nullptr;
                        uniform = uniform->m_next)
                    {
                        if (uniform->m_uniform_type != JE_GL_SHADER_UNIFORM_TYPE_TEXTURE
                            && strcmp(uniform->m_name, name) == 0)
                            return uniform->m_offset;
                    }
                    return INVALID_UNIFORM_LOCATION;
                };

            shader->m_builtin_uniforms.m_builtin_uniform_ndc_scale = resolve("JE_NDC_SCALE");
            shader->m_builtin_uniforms.m_builtin_uniform_m = resolve("JE_M");
            shader->m_builtin_uniforms.m_builtin_uniform_mv = resolve("JE_MV");
            shader->m_builtin_uniforms.m_builtin_uniform_mvp = resolve("JE_MVP");
            shader->m_builtin_uniforms.m_builtin_uniform_local_scale = resolve("JE_LOCAL_SCALE");
            shader->m_builtin_uniforms.m_builtin_uniform_tiling = resolve("JE_UV_TILING");
            shader->m_builtin_uniforms.m_builtin_uniform_offset = resolve("JE_UV_OFFSET");
            shader->m_builtin_uniforms.m_builtin_uniform_color = resolve("JE_COLOR");
            shader->m_builtin_uniforms.m_builtin_uniform_light2d_resolution = resolve("JE_LIGHT2D_RESOLUTION");
            shader->m_builtin_uniforms.m_builtin_uniform_light2d_decay = resolve("JE_LIGHT2D_DECAY");
            shader->m_builtin_uniforms.m_builtin_uniform_shadow_ellipse = resolve("JE_SHADOW_ELLIPSE");
        }

#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR
        je_gl_shader_spirv_code_t* _jegl_parse_spir_v_from_hlsl(const char* hlsl_src, bool is_fragment, size_t* out_codelen)
        {
            glslang_input_t hlsl_shader_input;
            hlsl_shader_input.language = glslang_source_t::GLSLANG_SOURCE_HLSL;
            hlsl_shader_input.stage = is_fragment
                ? glslang_stage_t::GLSLANG_STAGE_FRAGMENT
                : glslang_stage_t::GLSLANG_STAGE_VERTEX;
            hlsl_shader_input.client = glslang_client_t::GLSLANG_CLIENT_VULKAN;
            hlsl_shader_input.client_version = glslang_target_client_version_t::GLSLANG_TARGET_VULKAN_1_1;
            hlsl_shader_input.target_language = glslang_target_language_t::GLSLANG_TARGET_SPV;
            hlsl_shader_input.target_language_version = glslang_target_language_version_t::GLSLANG_TARGET_SPV_1_1;

            hlsl_shader_input.code = hlsl_src;
            hlsl_shader_input.default_version = 50; // HLSL VERSION?
            hlsl_shader_input.default_profile = GLSLANG_NO_PROFILE;
            hlsl_shader_input.force_default_version_and_profile = 0; // ?
            hlsl_shader_input.forward_compatible = 0;                // ?
            hlsl_shader_input.messages = glslang_messages_t::GLSLANG_MSG_DEFAULT_BIT;
            hlsl_shader_input.resource = glslang_default_resource();
            hlsl_shader_input.callbacks = {};
            hlsl_shader_input.callbacks_ctx = nullptr;

            glslang_program_t* program = glslang_program_create();
            glslang_shader_t* hlsl_shader = glslang_shader_create(&hlsl_shader_input);

            glslang_shader_set_entry_point(hlsl_shader, is_fragment ? "fragment_main" : "vertex_main");
            glslang_shader_set_preamble(hlsl_shader, "#define GLSLANG_HLSL_TO_SPIRV 1");

            if (!glslang_shader_preprocess(hlsl_shader, &hlsl_shader_input))
            {
                debug::logfatal("Failed to preprocess hlsl shader: %s.",
                    glslang_shader_get_info_log(hlsl_shader));
            }
            if (!glslang_shader_parse(hlsl_shader, &hlsl_shader_input))
            {
                debug::logfatal("Failed to preprocess hlsl shader: %s.",
                    glslang_shader_get_info_log(hlsl_shader));
            }

            glslang_program_add_shader(program, hlsl_shader);

            if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT))
            {
                debug::logfatal("Failed to preprocess hlsl program: %s.",
                    glslang_program_get_info_log(program));
            }

            glslang_spv_options_t spv_options{};
            spv_options.generate_debug_info = false;
            spv_options.strip_debug_info = false;
            spv_options.disable_optimizer = false;
            spv_options.optimize_size = true;
            spv_options.disassemble = false;
            spv_options.validate = true;
            spv_options.emit_nonsemantic_shader_debug_info = false;
            spv_options.emit_nonsemantic_shader_debug_source = false;
            spv_options.compile_only = false;

            glslang_program_SPIRV_generate_with_options(program, hlsl_shader_input.stage, &spv_options);
            if (glslang_program_SPIRV_get_messages(program))
            {
                debug::logfatal("Failed to generate code hlsl vertex program: %s.",
                    glslang_program_SPIRV_get_messages(program));
            }

            auto spir_v_code_len = glslang_program_SPIRV_get_size(program);
            auto spir_v_codes = glslang_program_SPIRV_get_ptr(program);

            *out_codelen = spir_v_code_len;
            je_gl_shader_spirv_code_t* codes =
                (je_gl_shader_spirv_code_t*)je_mem_alloc(
                    spir_v_code_len * sizeof(je_gl_shader_spirv_code_t));

            memcpy(codes, spir_v_codes,
                spir_v_code_len * sizeof(je_gl_shader_spirv_code_t));

            glslang_shader_delete(hlsl_shader);
            glslang_program_delete(program);

            return codes;
        }

        void _jegl_regenerate_and_alloc_glsl_from_spir_v_combined(
            const uint32_t* vertex_spir_v_code, size_t vertex_spir_v_ir_count,
            const uint32_t* fragment_spir_v_code, size_t fragment_spir_v_ir_count,
            const char** out_vertex_glsl,
            const char** out_fragment_glsl,
            bool generate_for_es)
        {
            spvc_context spir_v_cross_context = nullptr;
            spvc_context_create(&spir_v_cross_context);

            // 解析顶点着色器SPIR-V
            spvc_parsed_ir vertex_ir = nullptr;
            spvc_context_parse_spirv(spir_v_cross_context, vertex_spir_v_code, vertex_spir_v_ir_count, &vertex_ir);

            // 解析片元着色器SPIR-V
            spvc_parsed_ir fragment_ir = nullptr;
            spvc_context_parse_spirv(spir_v_cross_context, fragment_spir_v_code, fragment_spir_v_ir_count, &fragment_ir);

            // 创建顶点着色器编译器
            spvc_compiler vertex_compiler = nullptr;
            spvc_context_create_compiler(spir_v_cross_context, SPVC_BACKEND_GLSL, vertex_ir, SPVC_CAPTURE_MODE_COPY, &vertex_compiler);

            // 创建片元着色器编译器
            spvc_compiler fragment_compiler = nullptr;
            spvc_context_create_compiler(spir_v_cross_context, SPVC_BACKEND_GLSL, fragment_ir, SPVC_CAPTURE_MODE_COPY, &fragment_compiler);

            // 设置通用编译选项
            spvc_compiler_options vertex_options = nullptr, fragment_options = nullptr;
            spvc_compiler_create_compiler_options(vertex_compiler, &vertex_options);
            spvc_compiler_create_compiler_options(fragment_compiler, &fragment_options);

            if (generate_for_es)
            {
                spvc_compiler_options_set_uint(vertex_options, SPVC_COMPILER_OPTION_GLSL_VERSION, 300);
                spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
                spvc_compiler_options_set_uint(fragment_options, SPVC_COMPILER_OPTION_GLSL_VERSION, 300);
                spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
            }
            else
            {
                spvc_compiler_options_set_uint(vertex_options, SPVC_COMPILER_OPTION_GLSL_VERSION, 330);
                spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
                spvc_compiler_options_set_uint(fragment_options, SPVC_COMPILER_OPTION_GLSL_VERSION, 330);
                spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
            }

            spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_FIXUP_DEPTH_CONVENTION, SPVC_TRUE);
            spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_FALSE);
            spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
            spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_GLSL_SEPARATE_SHADER_OBJECTS, SPVC_FALSE);

            spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_FALSE);
            spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
            spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_GLSL_SEPARATE_SHADER_OBJECTS, SPVC_FALSE);

            // 禁用重命名所有变量的行为，保留原始命名
            spvc_compiler_options_set_bool(vertex_options, SPVC_COMPILER_OPTION_FORCE_TEMPORARY, SPVC_FALSE);
            spvc_compiler_options_set_bool(fragment_options, SPVC_COMPILER_OPTION_FORCE_TEMPORARY, SPVC_FALSE);

            spvc_compiler_install_compiler_options(vertex_compiler, vertex_options);
            spvc_compiler_install_compiler_options(fragment_compiler, fragment_options);

            // 在构建组合图像采样器之前，先保存原始sampler名称和绑定点
            spvc_resources vertex_resources_pre = nullptr;
            spvc_compiler_create_shader_resources(vertex_compiler, &vertex_resources_pre);

            const spvc_reflected_resource* vertex_samplers = nullptr;
            size_t vertex_sampler_count = 0;
            spvc_resources_get_resource_list_for_type(
                vertex_resources_pre, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &vertex_samplers, &vertex_sampler_count);

            // 存储sampler的ID到binding的映射
            unordered_map<uint32_t, uint32_t> vertex_sampler_bindings;
            for (size_t i = 0; i < vertex_sampler_count; i++) {
                const spvc_reflected_resource& sampler = vertex_samplers[i];
                uint32_t binding = spvc_compiler_get_decoration(vertex_compiler, sampler.id, SpvDecorationBinding);
                vertex_sampler_bindings[sampler.id] = binding;
            }

            spvc_resources fragment_resources_pre = nullptr;
            spvc_compiler_create_shader_resources(fragment_compiler, &fragment_resources_pre);

            const spvc_reflected_resource* fragment_samplers = nullptr;
            size_t fragment_sampler_count = 0;
            spvc_resources_get_resource_list_for_type(
                fragment_resources_pre, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &fragment_samplers, &fragment_sampler_count);

            // 存储sampler的ID到binding的映射
            unordered_map<uint32_t, uint32_t> fragment_sampler_bindings;
            for (size_t i = 0; i < fragment_sampler_count; i++) {
                const spvc_reflected_resource& sampler = fragment_samplers[i];
                uint32_t binding = spvc_compiler_get_decoration(fragment_compiler, sampler.id, SpvDecorationBinding);
                fragment_sampler_bindings[sampler.id] = binding;
            }

            // 预备哑采样器：spirv-cross 要求在构建组合图像采样器之前调用，
            // 否则遇到无采样器的 texelFetch（如 Texture2DMS.Load）会抛异常；
            // 仅在确实存在此类 fetch 时才会实际创建（对现有 shader 无影响）
            spvc_variable_id dummy_sampler_id = 0;
            spvc_compiler_build_dummy_sampler_for_combined_images(
                vertex_compiler, &dummy_sampler_id);
            spvc_compiler_build_dummy_sampler_for_combined_images(
                fragment_compiler, &dummy_sampler_id);

            // 构建组合图像采样器
            spvc_compiler_build_combined_image_samplers(vertex_compiler);
            spvc_compiler_build_combined_image_samplers(fragment_compiler);

            // 处理顶点着色器中的组合图像采样器，使用通道号作为命名的一部分
            const spvc_combined_image_sampler* vertex_combined_samplers = nullptr;
            size_t vertex_combined_count = 0;
            spvc_compiler_get_combined_image_samplers(vertex_compiler, &vertex_combined_samplers, &vertex_combined_count);

            char gl_sampler_name[64];
            for (size_t i = 0; i < vertex_combined_count; i++) {
                const auto& combined = vertex_combined_samplers[i];
                uint32_t binding = 0;

                // 尝试获取原始sampler的binding
                auto it = vertex_sampler_bindings.find(combined.image_id);
                if (it != vertex_sampler_bindings.end()) {
                    binding = it->second;
                }

                // 使用binding作为命名的一部分
                auto count = snprintf(
                    gl_sampler_name, sizeof(gl_sampler_name), "je4_gl_sampler_%u", binding);
                (void)count;
                assert(count > 0 && (size_t)count < sizeof(gl_sampler_name));

                spvc_compiler_set_name(vertex_compiler, combined.combined_id, gl_sampler_name);
            }

            // 处理片元着色器中的组合图像采样器，使用通道号作为命名的一部分
            const spvc_combined_image_sampler* fragment_combined_samplers = nullptr;
            size_t fragment_combined_count = 0;
            spvc_compiler_get_combined_image_samplers(fragment_compiler, &fragment_combined_samplers, &fragment_combined_count);

            for (size_t i = 0; i < fragment_combined_count; i++) {
                const auto& combined = fragment_combined_samplers[i];
                uint32_t binding = 0;

                // 尝试获取原始sampler的binding
                auto it = fragment_sampler_bindings.find(combined.image_id);
                if (it != fragment_sampler_bindings.end()) {
                    binding = it->second;
                }

                // 使用binding作为命名的一部分
                auto count = snprintf(
                    gl_sampler_name, sizeof(gl_sampler_name), "je4_gl_sampler_%u", binding);
                (void)count;
                assert(count > 0 && (size_t)count < sizeof(gl_sampler_name));

                spvc_compiler_set_name(fragment_compiler, combined.combined_id, gl_sampler_name);
            }

            // 获取顶点着色器输出变量
            spvc_resources vertex_resources = nullptr;
            spvc_compiler_create_shader_resources(vertex_compiler, &vertex_resources);

            const spvc_reflected_resource* vertex_outputs = nullptr;
            size_t vertex_output_count = 0;
            spvc_resources_get_resource_list_for_type(vertex_resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &vertex_outputs, &vertex_output_count);

            // 获取片元着色器输入变量
            spvc_resources fragment_resources = nullptr;
            spvc_compiler_create_shader_resources(fragment_compiler, &fragment_resources);

            const spvc_reflected_resource* fragment_inputs = nullptr;
            size_t fragment_input_count = 0;
            spvc_resources_get_resource_list_for_type(fragment_resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &fragment_inputs, &fragment_input_count);

            // 确保接口变量名称一致
            // 创建映射表跟踪需要重命名的变量
            unordered_map<uint32_t, string> location_to_name;

            // 首先收集顶点着色器的输出位置和名称
            for (size_t i = 0; i < vertex_output_count; i++) {
                const spvc_reflected_resource& output = vertex_outputs[i];
                uint32_t location = spvc_compiler_get_decoration(vertex_compiler, output.id, SpvDecorationLocation);

                // 标准化名称：使用 v_out_{location} 作为统一命名格式
                string standardized_name = "v_out_" + to_string(location);
                location_to_name[location] = standardized_name;

                // 重命名顶点着色器输出变量
                spvc_compiler_set_name(vertex_compiler, output.id, standardized_name.c_str());
            }

            // 然后处理片元着色器的输入变量，使其与顶点着色器输出匹配
            for (size_t i = 0; i < fragment_input_count; i++) {
                const spvc_reflected_resource& input = fragment_inputs[i];
                uint32_t location = spvc_compiler_get_decoration(fragment_compiler, input.id, SpvDecorationLocation);

                // 查找对应位置的标准化名称
                auto it = location_to_name.find(location);
                if (it != location_to_name.end()) {
                    // 重命名片元着色器输入变量，使其与顶点着色器输出匹配
                    spvc_compiler_set_name(fragment_compiler, input.id, it->second.c_str());
                }
            }

            // 编译生成GLSL源码
            const char* vertex_src = nullptr;
            spvc_compiler_compile(vertex_compiler, &vertex_src);

            const char* fragment_src = nullptr;
            spvc_compiler_compile(fragment_compiler, &fragment_src);

            // 复制结果
            *out_vertex_glsl = basic::make_new_string(vertex_src);
            *out_fragment_glsl = basic::make_new_string(fragment_src);

            // 清理资源
            spvc_context_destroy(spir_v_cross_context);
        }

        void _jegl_regenerate_and_alloc_msl_from_spir_v(
            const uint32_t* vertex_spir_v_code, size_t vertex_spir_v_ir_count,
            const uint32_t* fragment_spir_v_code, size_t fragment_spir_v_ir_count,
            const char** out_vertex_msl,
            const char** out_fragment_msl)
        {
            spvc_context context = NULL;
            spvc_parsed_ir vertex_ir = NULL;
            spvc_parsed_ir fragment_ir = NULL;
            spvc_compiler vertex_compiler = NULL;
            spvc_compiler fragment_compiler = NULL;

            const char* vertex_msl = NULL;
            const char* fragment_msl = NULL;
            spvc_context_create(&context);
            spvc_context_parse_spirv(context, vertex_spir_v_code, vertex_spir_v_ir_count, &vertex_ir);
            spvc_context_create_compiler(context, SPVC_BACKEND_MSL, vertex_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &vertex_compiler);
            spvc_context_parse_spirv(context, fragment_spir_v_code, fragment_spir_v_ir_count, &fragment_ir);
            spvc_context_create_compiler(context, SPVC_BACKEND_MSL, fragment_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &fragment_compiler);

            // 设置编译选项
            spvc_compiler_options options;
            spvc_compiler_create_compiler_options(vertex_compiler, &options);
            spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(2, 0, 0));
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE);
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, SPVC_TRUE);
            spvc_compiler_install_compiler_options(vertex_compiler, options);

            spvc_compiler_create_compiler_options(fragment_compiler, &options);
            spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(2, 0, 0));
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE);
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ENABLE_DECORATION_BINDING, SPVC_TRUE);
            spvc_compiler_install_compiler_options(fragment_compiler, options);

            // 修改顶点着色器的buffer绑定位置
            spvc_resources vertex_resources = NULL;
            spvc_compiler_create_shader_resources(vertex_compiler, &vertex_resources);

            const spvc_reflected_resource* vertex_uniform_buffers = NULL;
            size_t vertex_uniform_buffer_count = 0;
            spvc_resources_get_resource_list_for_type(
                vertex_resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &vertex_uniform_buffers, &vertex_uniform_buffer_count);

            for (size_t i = 0; i < vertex_uniform_buffer_count; i++) {
                // Get SpvDecorationDescriptorSet as SpvDecorationBinding
                uint32_t current_binding = spvc_compiler_get_decoration(vertex_compiler, vertex_uniform_buffers[i].id, SpvDecorationDescriptorSet);
                spvc_compiler_set_decoration(vertex_compiler, vertex_uniform_buffers[i].id, SpvDecorationBinding, current_binding + 1);
            }

            // 修改片元着色器的buffer绑定位置
            spvc_resources fragment_resources = NULL;
            spvc_compiler_create_shader_resources(fragment_compiler, &fragment_resources);

            const spvc_reflected_resource* fragment_uniform_buffers = NULL;
            size_t fragment_uniform_buffer_count = 0;
            spvc_resources_get_resource_list_for_type(
                fragment_resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &fragment_uniform_buffers, &fragment_uniform_buffer_count);

            for (size_t i = 0; i < fragment_uniform_buffer_count; i++) {
                // Get SpvDecorationDescriptorSet as SpvDecorationBinding
                uint32_t current_binding = spvc_compiler_get_decoration(fragment_compiler, fragment_uniform_buffers[i].id, SpvDecorationDescriptorSet);
                spvc_compiler_set_decoration(fragment_compiler, fragment_uniform_buffers[i].id, SpvDecorationBinding, current_binding + 1);
            }

            spvc_compiler_compile(vertex_compiler, &vertex_msl);
            spvc_compiler_compile(fragment_compiler, &fragment_msl);

            *out_vertex_msl = basic::make_new_string(vertex_msl);
            *out_fragment_msl = basic::make_new_string(fragment_msl);

            spvc_context_destroy(context);
        }
#endif
    }

#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR
    void _jegl_create_shader_cache(je_gl_shader_t* shader_resource, uint64_t virtual_file_crc64)
    {
        assert(shader_resource->m_handle.m_path_may_null_if_builtin != nullptr);

        if (auto* const cachefile = je_cache_file_create(
            shader_resource->m_handle.m_path_may_null_if_builtin,
            SHADER_CACHE_VERSION,
            virtual_file_crc64))
        {
            // 1. write shader generated source to cache
            // 1.1. HLSL
            const uint64_t vertex_hlsl_src_len = (uint64_t)strlen(shader_resource->m_vertex_hlsl_src);
            const uint64_t fragment_hlsl_src_len = (uint64_t)strlen(shader_resource->m_fragment_hlsl_src);

            je_cache_file_write(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_hlsl_src, sizeof(char), (size_t)vertex_hlsl_src_len, cachefile);
            je_cache_file_write(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_hlsl_src, sizeof(char), (size_t)fragment_hlsl_src_len, cachefile);

            // 1.2. GLSL
            const uint64_t vertex_glsl_src_len = (uint64_t)strlen(shader_resource->m_vertex_glsl_src);
            const uint64_t fragment_glsl_src_len = (uint64_t)strlen(shader_resource->m_fragment_glsl_src);

            je_cache_file_write(&vertex_glsl_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_glsl_src, sizeof(char), (size_t)vertex_glsl_src_len, cachefile);
            je_cache_file_write(&fragment_glsl_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_glsl_src, sizeof(char), (size_t)fragment_glsl_src_len, cachefile);

            // 1.3. GLSLES
            const uint64_t vertex_glsles_src_len = (uint64_t)strlen(shader_resource->m_vertex_glsles_src);
            const uint64_t fragment_glsles_src_len = (uint64_t)strlen(shader_resource->m_fragment_glsles_src);

            je_cache_file_write(&vertex_glsles_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_glsles_src, sizeof(char), (size_t)vertex_glsles_src_len, cachefile);
            je_cache_file_write(&fragment_glsles_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_glsles_src, sizeof(char), (size_t)fragment_glsles_src_len, cachefile);

            // 1.4. MSL MACOS
            const uint64_t vertex_msl_mac_src_len = (uint64_t)strlen(shader_resource->m_vertex_msl_mac_src);
            const uint64_t fragment_msl_mac_src_len = (uint64_t)strlen(shader_resource->m_fragment_msl_mac_src);

            je_cache_file_write(&vertex_msl_mac_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_msl_mac_src, sizeof(char), (size_t)vertex_msl_mac_src_len, cachefile);
            je_cache_file_write(&fragment_msl_mac_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_msl_mac_src, sizeof(char), (size_t)fragment_msl_mac_src_len, cachefile);

            // 1.5. SPIR-V
            const uint64_t vertex_spirv_src_len = (uint64_t)shader_resource->m_vertex_spirv_count * sizeof(je_gl_shader_spirv_code_t);
            const uint64_t fragment_spirv_src_len = (uint64_t)shader_resource->m_fragment_spirv_count * sizeof(je_gl_shader_spirv_code_t);

            je_cache_file_write(&vertex_spirv_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_spirv_codes, sizeof(char), (size_t)vertex_spirv_src_len, cachefile);
            je_cache_file_write(&fragment_spirv_src_len, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_spirv_codes, sizeof(char), (size_t)fragment_spirv_src_len, cachefile);

            // 2. write shader config to cache
            /*
                je_gl_shader_depth_test_method_t   m_depth_test;
                je_gl_shader_depth_mask_method_t   m_depth_mask;
                je_gl_shader_blend_method_t        m_blend_src_mode, m_blend_dst_mode;
                je_gl_shader_cull_mode_t           m_cull_mode;
            */
            je_cache_file_write(&shader_resource->m_depth_test, sizeof(je_gl_shader_depth_test_method_t), 1, cachefile);
            je_cache_file_write(&shader_resource->m_depth_mask, sizeof(je_gl_shader_depth_mask_method_t), 1, cachefile);
            je_cache_file_write(&shader_resource->m_blend_equation, sizeof(je_gl_shader_blend_equation_t), 1, cachefile);
            je_cache_file_write(&shader_resource->m_blend_src_mode, sizeof(je_gl_shader_blend_method_t), 1, cachefile);
            je_cache_file_write(&shader_resource->m_blend_dst_mode, sizeof(je_gl_shader_blend_method_t), 1, cachefile);
            je_cache_file_write(&shader_resource->m_cull_mode, sizeof(je_gl_shader_cull_mode_t), 1, cachefile);

            // 3. write if shader is enable to shared?
            je_cache_file_write(&shader_resource->m_enable_to_shared, sizeof(bool), 1, cachefile);

            // 4. write shader custom variable & uniform block informs.
            uint64_t count_for_uniform = 0;
            uint64_t count_for_uniform_block = 0;

            auto* custom_uniform = shader_resource->m_custom_uniforms;
            while (custom_uniform)
            {
                ++count_for_uniform;
                custom_uniform = custom_uniform->m_next;
            }
            auto* custom_uniform_block = shader_resource->m_custom_uniform_blocks;
            while (custom_uniform_block)
            {
                ++count_for_uniform_block;
                custom_uniform_block = custom_uniform_block->m_next;
            }

            // 4.1 write shader custom variable
            je_cache_file_write(&count_for_uniform, sizeof(uint64_t), 1, cachefile);
            custom_uniform = shader_resource->m_custom_uniforms;
            while (custom_uniform)
            {
                // 4.1.1 write name
                uint64_t uniform_name_len = (uint64_t)strlen(custom_uniform->m_name);
                je_cache_file_write(&uniform_name_len, sizeof(uint64_t), 1, cachefile);
                je_cache_file_write(custom_uniform->m_name, sizeof(char), (size_t)uniform_name_len, cachefile);

                // 4.1.2 write type & tags
                je_cache_file_write(&custom_uniform->m_uniform_type, sizeof(je_gl_shader_uniform_type_t), 1, cachefile);
                je_cache_file_write(&custom_uniform->m_uniform_mask, sizeof(je_gl_shader_uniform_mask_t), 1, cachefile);

                // 4.1.3 write data
                static_assert(sizeof(custom_uniform->m_value.m_float4x4) == sizeof(float[4][4]));
                je_cache_file_write(&custom_uniform->m_value.m_float4x4, sizeof(float[4][4]), 1, cachefile);

                custom_uniform = custom_uniform->m_next;
            }

            // 4.2 write shader custom uniform block informs
            je_cache_file_write(&count_for_uniform_block, sizeof(uint64_t), 1, cachefile);
            custom_uniform_block = shader_resource->m_custom_uniform_blocks;
            while (custom_uniform_block)
            {
                // 4.2.1 write name
                uint64_t uniform_block_name_len = (uint64_t)strlen(custom_uniform_block->m_name);
                je_cache_file_write(&uniform_block_name_len, sizeof(uint64_t), 1, cachefile);
                je_cache_file_write(custom_uniform_block->m_name, sizeof(char), (size_t)uniform_block_name_len, cachefile);

                // 4.2.2 write place
                static_assert(sizeof(custom_uniform_block->m_specify_binding_place) == sizeof(uint32_t));
                je_cache_file_write(&custom_uniform_block->m_specify_binding_place, sizeof(uint32_t), 1, cachefile);

                custom_uniform_block = custom_uniform_block->m_next;
            }

            // 4.3 write shader vertex layout
            uint64_t vertex_in_count = (uint64_t)shader_resource->m_vertex_in_count;
            je_cache_file_write(&vertex_in_count, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_vertex_in, sizeof(je_gl_shader_uniform_type_t),
                shader_resource->m_vertex_in_count, cachefile);

            uint64_t fragment_out_count = (uint64_t)shader_resource->m_fragment_out_count;
            je_cache_file_write(&fragment_out_count, sizeof(uint64_t), 1, cachefile);
            je_cache_file_write(shader_resource->m_fragment_out, sizeof(je_gl_shader_uniform_type_t),
                shader_resource->m_fragment_out_count, cachefile);

            // 4.4 write sampler informations;
            uint64_t sampler_count = (uint64_t)shader_resource->m_sampler_count;
            je_cache_file_write(&sampler_count, sizeof(uint64_t), 1, cachefile);
            for (uint64_t i = 0; i < sampler_count; ++i)
            {
                auto& sampler = shader_resource->m_sampler_methods[i];
                je_cache_file_write(&sampler.m_min, sizeof(je_gl_shader_filter_mode_t), 1, cachefile);
                je_cache_file_write(&sampler.m_mag, sizeof(je_gl_shader_filter_mode_t), 1, cachefile);
                je_cache_file_write(&sampler.m_mip, sizeof(je_gl_shader_filter_mode_t), 1, cachefile);
                je_cache_file_write(&sampler.m_uwrap, sizeof(je_gl_shader_wrap_mode_t), 1, cachefile);
                je_cache_file_write(&sampler.m_vwrap, sizeof(je_gl_shader_wrap_mode_t), 1, cachefile);
                je_cache_file_write(&sampler.m_sampler_id, sizeof(uint32_t), 1, cachefile);
                je_cache_file_write(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cachefile);
                je_cache_file_write(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cachefile);
            }

            // 4.5 write texture binding reflection
            uint64_t texture_count = (uint64_t)shader_resource->m_texture_count;
            je_cache_file_write(&texture_count, sizeof(uint64_t), 1, cachefile);
            for (uint64_t i = 0; i < texture_count; ++i)
            {
                auto& texture = shader_resource->m_textures[i];

                uint64_t texture_name_len = (uint64_t)strlen(texture.m_name);
                je_cache_file_write(&texture_name_len, sizeof(uint64_t), 1, cachefile);
                je_cache_file_write(texture.m_name, sizeof(char), (size_t)texture_name_len, cachefile);

                je_cache_file_write(&texture.m_pass, sizeof(uint32_t), 1, cachefile);
                je_cache_file_write(&texture.m_sampler_id, sizeof(uint32_t), 1, cachefile);
                je_cache_file_write(&texture.m_kind, sizeof(je_gl_texture_kind_t), 1, cachefile);
            }

            je_cache_file_close(cachefile, true);
        }
    }

    void jegl_shader_generate_shader_source(ShaderWrapper* shader_generator, je_gl_shader_t* write_to_shader)
    {
        ShaderWrapper* shader_wrapper_ptr = (ShaderWrapper*)shader_generator;

        const auto& shader_config = shader_wrapper_ptr->m_shader_config;
        write_to_shader->m_enable_to_shared = shader_config.m_enable_shared;
        write_to_shader->m_depth_test = shader_config.m_depth_test;
        write_to_shader->m_depth_mask = shader_config.m_depth_mask;
        write_to_shader->m_blend_equation = shader_config.m_blend_equation;
        write_to_shader->m_blend_src_mode = shader_config.m_blend_src;
        write_to_shader->m_blend_dst_mode = shader_config.m_blend_dst;
        write_to_shader->m_cull_mode = shader_config.m_cull_mode;

        // 1. Generate original hlsl source.
        write_to_shader->m_vertex_hlsl_src =
            basic::make_new_string(shader_wrapper_ptr->m_vertex_hlsl_source);
        write_to_shader->m_fragment_hlsl_src =
            basic::make_new_string(shader_wrapper_ptr->m_fragment_hlsl_source);

        // 2. Generate spir-v by original hlsl source.
        write_to_shader->m_vertex_spirv_codes =
            _jegl_parse_spir_v_from_hlsl(
                write_to_shader->m_vertex_hlsl_src,
                false,
                &write_to_shader->m_vertex_spirv_count);
        write_to_shader->m_fragment_spirv_codes =
            _jegl_parse_spir_v_from_hlsl(
                write_to_shader->m_fragment_hlsl_src,
                true,
                &write_to_shader->m_fragment_spirv_count);

        // 3. Generate glsl by spir-v
        _jegl_regenerate_and_alloc_glsl_from_spir_v_combined(
            write_to_shader->m_vertex_spirv_codes,
            write_to_shader->m_vertex_spirv_count,
            write_to_shader->m_fragment_spirv_codes,
            write_to_shader->m_fragment_spirv_count,
            &write_to_shader->m_vertex_glsl_src,
            &write_to_shader->m_fragment_glsl_src,
            false);

        _jegl_regenerate_and_alloc_glsl_from_spir_v_combined(
            write_to_shader->m_vertex_spirv_codes,
            write_to_shader->m_vertex_spirv_count,
            write_to_shader->m_fragment_spirv_codes,
            write_to_shader->m_fragment_spirv_count,
            &write_to_shader->m_vertex_glsles_src,
            &write_to_shader->m_fragment_glsles_src,
            true);

        _jegl_regenerate_and_alloc_msl_from_spir_v(
            write_to_shader->m_vertex_spirv_codes,
            write_to_shader->m_vertex_spirv_count,
            write_to_shader->m_fragment_spirv_codes,
            write_to_shader->m_fragment_spirv_count,
            &write_to_shader->m_vertex_msl_mac_src,
            &write_to_shader->m_fragment_msl_mac_src);

        // 4. Generate other info.
        write_to_shader->m_vertex_in_count = shader_wrapper_ptr->m_vin_layout.size();
        write_to_shader->m_vertex_in = new je_gl_shader_uniform_type_t[write_to_shader->m_vertex_in_count];
        for (size_t i = 0; i < write_to_shader->m_vertex_in_count; ++i)
            write_to_shader->m_vertex_in[i] = shader_wrapper_ptr->m_vin_layout.at(i);

        write_to_shader->m_fragment_out_count = shader_wrapper_ptr->m_fout_layout.size();
        write_to_shader->m_fragment_out = new je_gl_shader_uniform_type_t[write_to_shader->m_fragment_out_count];
        for (size_t i = 0; i < write_to_shader->m_fragment_out_count; ++i)
            write_to_shader->m_fragment_out[i] = shader_wrapper_ptr->m_fout_layout.at(i);

        unordered_map<string, const ShaderUniformBlockInfo*> _uniform_blocks;
        for (auto& uniform_block : shader_wrapper_ptr->m_uniform_blocks)
        {
            auto result = _uniform_blocks.emplace(
                uniform_block.m_name, &uniform_block);

            (void)result;
            assert(result.second);
        }

        map<uint32_t, set<uint32_t>> sampler_binded_texture_passes;
        do
        {
            je_gl_shader_uniform_variables_t** last = &write_to_shader->m_custom_uniforms;
            for (auto& uniform_info : shader_wrapper_ptr->m_uniform_vars)
            {
                je_gl_shader_uniform_variables_t* variable = new je_gl_shader_uniform_variables_t();
                variable->m_next = nullptr;

                variable->m_name = basic::make_new_string(uniform_info.m_name);
                variable->m_uniform_type = uniform_info.m_type;
                variable->m_uniform_mask = uniform_info.m_uniform_mask;
                variable->m_index = INVALID_UNIFORM_LOCATION;
                variable->m_offset = 0;
                variable->m_size = 0;
                variable->m_value = uniform_info.m_init_value;
                variable->m_updated = true;

                *last = variable;
                last = &variable->m_next;
            }

            for (auto& texture : shader_wrapper_ptr->m_textures)
            {
                (void)sampler_binded_texture_passes[texture.m_sampler_id].insert(texture.m_pass);

                je_gl_shader_uniform_variables_t* variable = new je_gl_shader_uniform_variables_t();
                variable->m_next = nullptr;
                variable->m_name = basic::make_new_string(texture.m_name);
                variable->m_uniform_type = JE_GL_SHADER_UNIFORM_TYPE_TEXTURE;
                variable->m_uniform_mask = JE_GL_SHADER_UNIFORM_MASK_NONE;
                variable->m_index = INVALID_UNIFORM_LOCATION;
                variable->m_offset = INVALID_UNIFORM_LOCATION;
                variable->m_size = 0;
                variable->m_value.m_int = (int)texture.m_pass;
                variable->m_updated = true;
                *last = variable;
                last = &variable->m_next;
            }
        } while (false);

        // Canonical layout of the per-draw uniform block, shared by all backends.
        _jegl_resolve_canonical_uniform_layout(write_to_shader);

        // Texture binding reflection used by the render API.
        write_to_shader->m_texture_count = shader_wrapper_ptr->m_textures.size();
        write_to_shader->m_textures = new je_gl_shader_texture_decl_t[write_to_shader->m_texture_count];
        for (size_t i = 0; i < write_to_shader->m_texture_count; ++i)
        {
            const auto& texture = shader_wrapper_ptr->m_textures.at(i);
            auto& decl = write_to_shader->m_textures[i];
            decl.m_name = basic::make_new_string(texture.m_name);
            decl.m_pass = (uint32_t)texture.m_pass;
            decl.m_sampler_id = (uint32_t)texture.m_sampler_id;
            decl.m_kind = texture.m_kind;
        }

        do
        {
            je_gl_shader_uniform_block_t** last = &write_to_shader->m_custom_uniform_blocks;
            for (auto& [_ /*useless*/, uniform_block_info] : _uniform_blocks)
            {
                je_gl_shader_uniform_block_t* block = new je_gl_shader_uniform_block_t();
                block->m_next = nullptr;

                assert(uniform_block_info->m_binding_place != INVALID_UNIFORM_LOCATION);

                block->m_name = basic::make_new_string(uniform_block_info->m_name);
                block->m_specify_binding_place = uniform_block_info->m_binding_place;

                *last = block;
                last = &block->m_next;
            }
        } while (false);

        write_to_shader->m_sampler_count = shader_wrapper_ptr->m_samplers.size();
        auto* sampler_methods = new je_gl_shader_sampler_method_t[write_to_shader->m_sampler_count];
        for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
        {
            const auto& sampler = shader_wrapper_ptr->m_samplers.at(i);
            sampler_methods[i].m_min = sampler.m_min;
            sampler_methods[i].m_mag = sampler.m_mag;
            sampler_methods[i].m_mip = sampler.m_mip;
            sampler_methods[i].m_uwrap = sampler.m_uwrap;
            sampler_methods[i].m_vwrap = sampler.m_vwrap;

            sampler_methods[i].m_sampler_id = sampler.m_sampler_id;

            auto& sampled_pass = sampler_binded_texture_passes[sampler.m_sampler_id];
            sampler_methods[i].m_pass_id_count = (uint64_t)sampled_pass.size();
            auto* passids = new uint32_t[sampled_pass.size()];

            size_t pass_idx = 0;
            for (auto passid : sampled_pass)
                passids[pass_idx++] = passid;

            sampler_methods[i].m_pass_ids = passids;
        }
        write_to_shader->m_sampler_methods = sampler_methods;
    }
#endif

    void bootup(int, char**)
    {
#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR
        glslang_initialize_process();
#endif
    }
    void shutdown()
    {
#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR
        glslang_finalize_process();
#endif
    }

    je_gl_shader_t* _jegl_create_shader_instance_and_init(void)
    {
        je_gl_shader_t* shader_instance = static_cast<je_gl_shader_t*>(
            malloc(sizeof(je_gl_shader_t)));

        assert(shader_instance != nullptr);
        static_assert(
            sizeof(shader_instance->m_builtin_uniforms) == 11 * sizeof(uint32_t));

        shader_instance->m_builtin_uniforms.m_builtin_uniform_ndc_scale =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_m =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_mv =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_mvp =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_local_scale =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_tiling =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_offset =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_color =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_light2d_resolution =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_light2d_decay =
            shader_instance->m_builtin_uniforms.m_builtin_uniform_shadow_ellipse =
        {
            PENDING_UNIFORM_LOCATION
        };

        shader_instance->m_uniform_block_size = 0;
        shader_instance->m_textures = nullptr;
        shader_instance->m_texture_count = 0;

        return shader_instance;
    }
    je_gl_shader_t* _jegl_load_shader_cache(je_file_t* cache_file, const char* path)
    {
        assert(cache_file != nullptr);

        je_gl_shader_t* shader_instance = _jegl_create_shader_instance_and_init();

        uint64_t
            vertex_hlsl_src_len,
            fragment_hlsl_src_len,
            vertex_glsl_src_len,
            fragment_glsl_src_len,
            vertex_glsles_src_len,
            fragment_glsles_src_len,
            vertex_msl_mac_src_len,
            fragment_msl_mac_src_len,
            vertex_spirv_src_len,
            fragment_spirv_src_len;

        // 1. Read generated source

        // 1.1. HLSL
        je_file_read(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_vertex_hlsl_src = (const char*)je_mem_alloc((size_t)vertex_hlsl_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_vertex_hlsl_src), sizeof(char), (size_t)vertex_hlsl_src_len, cache_file);
        const_cast<char*>(shader_instance->m_vertex_hlsl_src)[(size_t)vertex_hlsl_src_len] = 0;

        je_file_read(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_fragment_hlsl_src = (const char*)je_mem_alloc((size_t)fragment_hlsl_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_fragment_hlsl_src), sizeof(char), (size_t)fragment_hlsl_src_len, cache_file);
        const_cast<char*>(shader_instance->m_fragment_hlsl_src)[(size_t)fragment_hlsl_src_len] = 0;

        // 1.2. GLSL
        je_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_vertex_glsl_src = (const char*)je_mem_alloc((size_t)vertex_glsl_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_vertex_glsl_src), sizeof(char), (size_t)vertex_glsl_src_len, cache_file);
        const_cast<char*>(shader_instance->m_vertex_glsl_src)[(size_t)vertex_glsl_src_len] = 0;

        je_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_fragment_glsl_src = (const char*)je_mem_alloc((size_t)fragment_glsl_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_fragment_glsl_src), sizeof(char), (size_t)fragment_glsl_src_len, cache_file);
        const_cast<char*>(shader_instance->m_fragment_glsl_src)[(size_t)fragment_glsl_src_len] = 0;

        // 1.3. GLSL ES

        je_file_read(&vertex_glsles_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_vertex_glsles_src = (const char*)je_mem_alloc((size_t)vertex_glsles_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_vertex_glsles_src), sizeof(char), (size_t)vertex_glsles_src_len, cache_file);
        const_cast<char*>(shader_instance->m_vertex_glsles_src)[(size_t)vertex_glsles_src_len] = 0;

        je_file_read(&fragment_glsles_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_fragment_glsles_src = (const char*)je_mem_alloc((size_t)fragment_glsles_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_fragment_glsles_src), sizeof(char), (size_t)fragment_glsles_src_len, cache_file);
        const_cast<char*>(shader_instance->m_fragment_glsles_src)[(size_t)fragment_glsles_src_len] = 0;

        // 1.4. MSL (mac)
        je_file_read(&vertex_msl_mac_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_vertex_msl_mac_src = (const char*)je_mem_alloc((size_t)vertex_msl_mac_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_vertex_msl_mac_src), sizeof(char), (size_t)vertex_msl_mac_src_len, cache_file);
        const_cast<char*>(shader_instance->m_vertex_msl_mac_src)[(size_t)vertex_msl_mac_src_len] = 0;

        je_file_read(&fragment_msl_mac_src_len, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_fragment_msl_mac_src = (const char*)je_mem_alloc((size_t)fragment_msl_mac_src_len + 1);
        je_file_read(const_cast<char*>(shader_instance->m_fragment_msl_mac_src), sizeof(char), (size_t)fragment_msl_mac_src_len, cache_file);
        const_cast<char*>(shader_instance->m_fragment_msl_mac_src)[(size_t)fragment_msl_mac_src_len] = 0;

        // 1.5. SPIR-V
        je_file_read(&vertex_spirv_src_len, sizeof(uint64_t), 1, cache_file);
        assert((size_t)vertex_spirv_src_len % sizeof(je_gl_shader_spirv_code_t) == 0);

        shader_instance->m_vertex_spirv_count = (size_t)vertex_spirv_src_len / sizeof(je_gl_shader_spirv_code_t);
        shader_instance->m_vertex_spirv_codes = (const je_gl_shader_spirv_code_t*)je_mem_alloc((size_t)vertex_spirv_src_len);
        je_file_read(const_cast<je_gl_shader_spirv_code_t*>(shader_instance->m_vertex_spirv_codes), sizeof(char), (size_t)vertex_spirv_src_len, cache_file);

        je_file_read(&fragment_spirv_src_len, sizeof(uint64_t), 1, cache_file);
        assert((size_t)fragment_spirv_src_len % sizeof(je_gl_shader_spirv_code_t) == 0);

        shader_instance->m_fragment_spirv_count = (size_t)fragment_spirv_src_len / sizeof(je_gl_shader_spirv_code_t);
        shader_instance->m_fragment_spirv_codes = (const je_gl_shader_spirv_code_t*)je_mem_alloc((size_t)fragment_spirv_src_len);
        je_file_read(const_cast<je_gl_shader_spirv_code_t*>(shader_instance->m_fragment_spirv_codes), sizeof(char), (size_t)fragment_spirv_src_len, cache_file);

        // 2. read shader config
        je_file_read(&shader_instance->m_depth_test, sizeof(je_gl_shader_depth_test_method_t), 1, cache_file);
        je_file_read(&shader_instance->m_depth_mask, sizeof(je_gl_shader_depth_mask_method_t), 1, cache_file);
        je_file_read(&shader_instance->m_blend_equation, sizeof(je_gl_shader_blend_equation_t), 1, cache_file);
        je_file_read(&shader_instance->m_blend_src_mode, sizeof(je_gl_shader_blend_method_t), 1, cache_file);
        je_file_read(&shader_instance->m_blend_dst_mode, sizeof(je_gl_shader_blend_method_t), 1, cache_file);
        je_file_read(&shader_instance->m_cull_mode, sizeof(je_gl_shader_cull_mode_t), 1, cache_file);

        // 3. read if shader is enable to shared?
        je_file_read(&shader_instance->m_enable_to_shared, sizeof(bool), 1, cache_file);

        // 4. read and generate custom variable & uniform block informs

        // 4.1 read and generate custom variable
        uint64_t custom_uniform_count;
        je_file_read(&custom_uniform_count, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_custom_uniforms = nullptr;

        je_gl_shader_uniform_variables_t* last_create_variable = nullptr;
        for (uint64_t i = 0; i < custom_uniform_count; ++i)
        {
            je_gl_shader_uniform_variables_t* current_variable = new je_gl_shader_uniform_variables_t();

            if (shader_instance->m_custom_uniforms == nullptr)
                shader_instance->m_custom_uniforms = current_variable;

            if (last_create_variable != nullptr)
                last_create_variable->m_next = current_variable;

            // 4.1.1 read name
            uint64_t uniform_name_len;
            je_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
            current_variable->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
            je_file_read(const_cast<char*>(current_variable->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
            const_cast<char*>(current_variable->m_name)[(size_t)uniform_name_len] = 0;

            // 4.1.2 read type & tags
            je_file_read(&current_variable->m_uniform_type, sizeof(je_gl_shader_uniform_type_t), 1, cache_file);
            je_file_read(&current_variable->m_uniform_mask, sizeof(je_gl_shader_uniform_mask_t), 1, cache_file);

            // 4.1.3 read data
            static_assert(sizeof(current_variable->m_value.m_float4x4) == sizeof(float[4][4]));
            je_file_read(&current_variable->m_value.m_float4x4, sizeof(float[4][4]), 1, cache_file);

            current_variable->m_index = INVALID_UNIFORM_LOCATION;

            // For all variable, update it at first time.
            current_variable->m_updated = true;

            last_create_variable = current_variable;
            current_variable->m_next = nullptr;
        }

        // The uniform list is stored in canonical packing order; recompute the
        // shared std140 offsets for this shader instance.
        _jegl_resolve_canonical_uniform_layout(shader_instance);

        // 4.2 read uniform block informs
        uint64_t custom_uniform_block_count;
        je_file_read(&custom_uniform_block_count, sizeof(uint64_t), 1, cache_file);

        shader_instance->m_custom_uniform_blocks = nullptr;

        je_gl_shader_uniform_block_t* last_create_block = nullptr;
        for (uint64_t i = 0; i < custom_uniform_block_count; ++i)
        {
            je_gl_shader_uniform_block_t* current_block = new je_gl_shader_uniform_block_t();
            if (shader_instance->m_custom_uniform_blocks == nullptr)
                shader_instance->m_custom_uniform_blocks = current_block;

            if (last_create_block != nullptr)
                last_create_block->m_next = current_block;

            // 4.2.1 read name
            uint64_t uniform_name_len;
            je_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
            current_block->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
            je_file_read(const_cast<char*>(current_block->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
            const_cast<char*>(current_block->m_name)[(size_t)uniform_name_len] = 0;

            // 4.2.2 read binding place
            static_assert(sizeof(current_block->m_specify_binding_place) == sizeof(uint32_t));
            je_file_read(&current_block->m_specify_binding_place, sizeof(uint32_t), 1, cache_file);

            last_create_block = current_block;
            current_block->m_next = nullptr;
        }

        // 4.3 read shader vertex layout
        uint64_t vertex_in_count;
        je_file_read(&vertex_in_count, sizeof(uint64_t), 1, cache_file);
        shader_instance->m_vertex_in_count = (size_t)vertex_in_count;
        shader_instance->m_vertex_in = new je_gl_shader_uniform_type_t[shader_instance->m_vertex_in_count];
        je_file_read(shader_instance->m_vertex_in, sizeof(je_gl_shader_uniform_type_t),
            shader_instance->m_vertex_in_count, cache_file);

        uint64_t fragment_out_count;
        je_file_read(&fragment_out_count, sizeof(uint64_t), 1, cache_file);
        shader_instance->m_fragment_out_count = (size_t)fragment_out_count;
        shader_instance->m_fragment_out = new je_gl_shader_uniform_type_t[shader_instance->m_fragment_out_count];
        je_file_read(shader_instance->m_fragment_out, sizeof(je_gl_shader_uniform_type_t),
            shader_instance->m_fragment_out_count, cache_file);

        // 4.4 read sampler informations;
        uint64_t sampler_count;
        je_file_read(&sampler_count, sizeof(uint64_t), 1, cache_file);
        shader_instance->m_sampler_count = (size_t)sampler_count;
        shader_instance->m_sampler_methods = new je_gl_shader_sampler_method_t[(size_t)sampler_count];
        for (uint64_t i = 0; i < sampler_count; ++i)
        {
            auto& sampler = shader_instance->m_sampler_methods[i];
            je_file_read(&sampler.m_min, sizeof(je_gl_shader_filter_mode_t), 1, cache_file);
            je_file_read(&sampler.m_mag, sizeof(je_gl_shader_filter_mode_t), 1, cache_file);
            je_file_read(&sampler.m_mip, sizeof(je_gl_shader_filter_mode_t), 1, cache_file);
            je_file_read(&sampler.m_uwrap, sizeof(je_gl_shader_wrap_mode_t), 1, cache_file);
            je_file_read(&sampler.m_vwrap, sizeof(je_gl_shader_wrap_mode_t), 1, cache_file);
            je_file_read(&sampler.m_sampler_id, sizeof(uint32_t), 1, cache_file);
            je_file_read(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cache_file);

            sampler.m_pass_ids = new uint32_t[(size_t)sampler.m_pass_id_count];
            je_file_read(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cache_file);
        }

        // 4.5 read texture binding reflection
        uint64_t texture_count;
        je_file_read(&texture_count, sizeof(uint64_t), 1, cache_file);
        shader_instance->m_texture_count = (size_t)texture_count;
        shader_instance->m_textures = new je_gl_shader_texture_decl_t[shader_instance->m_texture_count];
        for (uint64_t i = 0; i < texture_count; ++i)
        {
            auto& texture = shader_instance->m_textures[i];

            uint64_t texture_name_len;
            je_file_read(&texture_name_len, sizeof(uint64_t), 1, cache_file);
            texture.m_name = (const char*)je_mem_alloc((size_t)texture_name_len + 1);
            je_file_read(const_cast<char*>(texture.m_name), sizeof(char), (size_t)texture_name_len, cache_file);
            const_cast<char*>(texture.m_name)[(size_t)texture_name_len] = 0;

            je_file_read(&texture.m_pass, sizeof(uint32_t), 1, cache_file);
            je_file_read(&texture.m_sampler_id, sizeof(uint32_t), 1, cache_file);
            je_file_read(&texture.m_kind, sizeof(je_gl_texture_kind_t), 1, cache_file);
        }

        je_file_close(cache_file);

        init_resource_handle(&shader_instance->m_handle, path);
        return shader_instance;
    }
    void jegl_shader_free_generated_shader_source(je_gl_shader_t* write_to_shader)
    {
        je_mem_free((void*)write_to_shader->m_vertex_hlsl_src);
        je_mem_free((void*)write_to_shader->m_fragment_hlsl_src);

        je_mem_free((void*)write_to_shader->m_vertex_glsl_src);
        je_mem_free((void*)write_to_shader->m_fragment_glsl_src);

        je_mem_free((void*)write_to_shader->m_vertex_glsles_src);
        je_mem_free((void*)write_to_shader->m_fragment_glsles_src);

        je_mem_free((void*)write_to_shader->m_vertex_spirv_codes);
        je_mem_free((void*)write_to_shader->m_fragment_spirv_codes);

        je_mem_free((void*)write_to_shader->m_vertex_msl_mac_src);
        je_mem_free((void*)write_to_shader->m_fragment_msl_mac_src);

        delete[] write_to_shader->m_vertex_in;

        auto* uniform_variable_info = write_to_shader->m_custom_uniforms;
        while (uniform_variable_info)
        {
            auto* current_uniform_variable = uniform_variable_info;
            uniform_variable_info = uniform_variable_info->m_next;

            je_mem_free((void*)current_uniform_variable->m_name);

            delete current_uniform_variable;
        }
        auto* uniform_block_info = write_to_shader->m_custom_uniform_blocks;
        while (uniform_block_info)
        {
            auto* cur_uniform_block = uniform_block_info;
            uniform_block_info = uniform_block_info->m_next;

            je_mem_free((void*)cur_uniform_block->m_name);

            delete cur_uniform_block;
        }

        for (size_t i = 0; i < write_to_shader->m_sampler_count; ++i)
            delete[] write_to_shader->m_sampler_methods[i].m_pass_ids;
        delete[] write_to_shader->m_sampler_methods;

        for (size_t i = 0; i < write_to_shader->m_texture_count; ++i)
            je_mem_free((void*)write_to_shader->m_textures[i].m_name);
        delete[] write_to_shader->m_textures;
    }

}
