#define JE_IMPL
#include "jeecs.hpp"
#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "jeecs_cache_version.hpp"
#include "jeecs_graphic_shader_wrapper.hpp"

// From jeecs_graphic_api_basic.cpp
jegl_resource* _create_resource();

#ifdef JE4_ENABLE_SHADER_WRAP_GENERATOR

#include <glslang_c_interface.h>
#include <resource_limits_c.h>
#include <spirv_cross_c.h>

void _jegl_create_shader_cache(jegl_resource* shader_resource, wo_integer_t virtual_file_crc64)
{
    assert(shader_resource->m_path != nullptr && shader_resource->m_raw_shader_data && shader_resource->m_type == jegl_resource::type::SHADER);

    if (auto* cachefile = jeecs_create_cache_file(
        shader_resource->m_path,
        SHADER_CACHE_VERSION,
        virtual_file_crc64))
    {
        auto* raw_shader_data = shader_resource->m_raw_shader_data;

        // 1. write shader generated source to cache
        // 1.1. HLSL
        const uint64_t vertex_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_hlsl_src);
        const uint64_t fragment_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_hlsl_src);

        jeecs_write_cache_file(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_hlsl_src, sizeof(char), (size_t)vertex_hlsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_hlsl_src, sizeof(char), (size_t)fragment_hlsl_src_len, cachefile);

        // 1.2. GLSL
        const uint64_t vertex_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_glsl_src);
        const uint64_t fragment_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_glsl_src);

        jeecs_write_cache_file(&vertex_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_glsl_src, sizeof(char), (size_t)vertex_glsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_glsl_src, sizeof(char), (size_t)fragment_glsl_src_len, cachefile);

        // 1.3. GLSLES
        const uint64_t vertex_glsles_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_glsles_src);
        const uint64_t fragment_glsles_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_glsles_src);

        jeecs_write_cache_file(&vertex_glsles_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_glsles_src, sizeof(char), (size_t)vertex_glsles_src_len, cachefile);
        jeecs_write_cache_file(&fragment_glsles_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_glsles_src, sizeof(char), (size_t)fragment_glsles_src_len, cachefile);

        // 1.4. MSL MACOS
        const uint64_t vertex_msl_mac_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_msl_mac_src);
        const uint64_t fragment_msl_mac_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_msl_mac_src);

        jeecs_write_cache_file(&vertex_msl_mac_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_msl_mac_src, sizeof(char), (size_t)vertex_msl_mac_src_len, cachefile);
        jeecs_write_cache_file(&fragment_msl_mac_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_msl_mac_src, sizeof(char), (size_t)fragment_msl_mac_src_len, cachefile);

        // 1.5. SPIR-V

        const uint64_t vertex_spirv_src_len = (uint64_t)raw_shader_data->m_vertex_spirv_count * sizeof(jegl_shader::spir_v_code_t);
        const uint64_t fragment_spirv_src_len = (uint64_t)raw_shader_data->m_fragment_spirv_count * sizeof(jegl_shader::spir_v_code_t);

        jeecs_write_cache_file(&vertex_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_spirv_codes, sizeof(char), (size_t)vertex_spirv_src_len, cachefile);
        jeecs_write_cache_file(&fragment_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_spirv_codes, sizeof(char), (size_t)fragment_spirv_src_len, cachefile);

        // 2. write shader config to cache
        /*
            depth_test_method   m_depth_test;
            depth_mask_method   m_depth_mask;
            blend_method        m_blend_src_mode, m_blend_dst_mode;
            cull_mode           m_cull_mode;
        */
        jeecs_write_cache_file(&raw_shader_data->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_equation, sizeof(jegl_shader::blend_equation), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cachefile);

        // 3. write if shader is enable to shared?
        jeecs_write_cache_file(&raw_shader_data->m_enable_to_shared, sizeof(bool), 1, cachefile);

        // 4. write shader custom variable & uniform block informs.
        uint64_t count_for_uniform = 0;
        uint64_t count_for_uniform_block = 0;

        auto* custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            ++count_for_uniform;
            custom_uniform = custom_uniform->m_next;
        }
        auto* custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            ++count_for_uniform_block;
            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.1 write shader custom variable
        jeecs_write_cache_file(&count_for_uniform, sizeof(uint64_t), 1, cachefile);
        custom_uniform = raw_shader_data->m_custom_uniforms;
        while (custom_uniform)
        {
            // 4.1.1 write name
            uint64_t uniform_name_len = (uint64_t)strlen(custom_uniform->m_name);
            jeecs_write_cache_file(&uniform_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform->m_name, sizeof(char), (size_t)uniform_name_len, cachefile);

            // 4.1.2 write type
            jeecs_write_cache_file(&custom_uniform->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cachefile);

            // 4.1.3 write data
            static_assert(sizeof(custom_uniform->m_value.mat4x4) == sizeof(float[4][4]));
            jeecs_write_cache_file(&custom_uniform->m_value.mat4x4, sizeof(float[4][4]), 1, cachefile);

            custom_uniform = custom_uniform->m_next;
        }

        // 4.2 write shader custom uniform block informs
        jeecs_write_cache_file(&count_for_uniform_block, sizeof(uint64_t), 1, cachefile);
        custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            // 4.2.1 write name
            uint64_t uniform_block_name_len = (uint64_t)strlen(custom_uniform_block->m_name);
            jeecs_write_cache_file(&uniform_block_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform_block->m_name, sizeof(char), (size_t)uniform_block_name_len, cachefile);

            // 4.2.2 write place
            static_assert(sizeof(custom_uniform_block->m_specify_binding_place) == sizeof(uint32_t));
            jeecs_write_cache_file(&custom_uniform_block->m_specify_binding_place, sizeof(uint32_t), 1, cachefile);

            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.3 write shader vertex layout
        uint64_t vertex_in_count = (uint64_t)raw_shader_data->m_vertex_in_count;
        jeecs_write_cache_file(&vertex_in_count, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_in, sizeof(jegl_shader::uniform_type),
            raw_shader_data->m_vertex_in_count, cachefile);

        uint64_t fragment_out_count = (uint64_t)raw_shader_data->m_fragment_out_count;
        jeecs_write_cache_file(&fragment_out_count, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_out, sizeof(jegl_shader::uniform_type),
            raw_shader_data->m_fragment_out_count, cachefile);

        // 4.4 write sampler informations;
        uint64_t sampler_count = (uint64_t)raw_shader_data->m_sampler_count;
        jeecs_write_cache_file(&sampler_count, sizeof(uint64_t), 1, cachefile);
        for (uint64_t i = 0; i < sampler_count; ++i)
        {
            auto& sampler = raw_shader_data->m_sampler_methods[i];
            jeecs_write_cache_file(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_sampler_id, sizeof(uint32_t), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cachefile);
        }

        jeecs_close_cache_file(cachefile);
    }
}

jegl_shader::spir_v_code_t* _jegl_parse_spir_v_from_hlsl(const char* hlsl_src, bool is_fragment, size_t* out_codelen)
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
        jeecs::debug::logfatal("Failed to preprocess hlsl shader: %s.",
            glslang_shader_get_info_log(hlsl_shader));
    }
    if (!glslang_shader_parse(hlsl_shader, &hlsl_shader_input))
    {
        jeecs::debug::logfatal("Failed to preprocess hlsl shader: %s.",
            glslang_shader_get_info_log(hlsl_shader));
    }

    glslang_program_add_shader(program, hlsl_shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT))
    {
        jeecs::debug::logfatal("Failed to preprocess hlsl program: %s.",
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
        jeecs::debug::logfatal("Failed to generate code hlsl vertex program: %s.",
            glslang_program_SPIRV_get_messages(program));
    }

    auto spir_v_code_len = glslang_program_SPIRV_get_size(program);
    auto spir_v_codes = glslang_program_SPIRV_get_ptr(program);

    *out_codelen = spir_v_code_len;
    jegl_shader::spir_v_code_t* codes =
        (jegl_shader::spir_v_code_t*)je_mem_alloc(
            spir_v_code_len * sizeof(jegl_shader::spir_v_code_t));

    memcpy(codes, spir_v_codes,
        spir_v_code_len * sizeof(jegl_shader::spir_v_code_t));

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
    std::unordered_map<uint32_t, uint32_t> vertex_sampler_bindings;
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
    std::unordered_map<uint32_t, uint32_t> fragment_sampler_bindings;
    for (size_t i = 0; i < fragment_sampler_count; i++) {
        const spvc_reflected_resource& sampler = fragment_samplers[i];
        uint32_t binding = spvc_compiler_get_decoration(fragment_compiler, sampler.id, SpvDecorationBinding);
        fragment_sampler_bindings[sampler.id] = binding;
    }

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
    std::unordered_map<uint32_t, std::string> location_to_name;

    // 首先收集顶点着色器的输出位置和名称
    for (size_t i = 0; i < vertex_output_count; i++) {
        const spvc_reflected_resource& output = vertex_outputs[i];
        uint32_t location = spvc_compiler_get_decoration(vertex_compiler, output.id, SpvDecorationLocation);

        // 标准化名称：使用 v_out_{location} 作为统一命名格式
        std::string standardized_name = "v_out_" + std::to_string(location);
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
    *out_vertex_glsl = jeecs::basic::make_new_string(vertex_src);
    *out_fragment_glsl = jeecs::basic::make_new_string(fragment_src);

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
    spvc_resources resources = NULL;
    const char* vertex_msl = NULL;
    const char* fragment_msl = NULL;
    spvc_context_create(&context);
    spvc_context_parse_spirv(context, vertex_spir_v_code, vertex_spir_v_ir_count, &vertex_ir);
    spvc_context_create_compiler(context, SPVC_BACKEND_MSL, vertex_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &vertex_compiler);
    spvc_context_parse_spirv(context, fragment_spir_v_code, fragment_spir_v_ir_count, &fragment_ir);
    spvc_context_create_compiler(context, SPVC_BACKEND_MSL, fragment_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &fragment_compiler);
    spvc_compiler_options options;
    spvc_compiler_create_compiler_options(vertex_compiler, &options);
    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(2, 0, 0));
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE);
    spvc_compiler_install_compiler_options(vertex_compiler, options);

    spvc_compiler_create_compiler_options(fragment_compiler, &options);
    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(2, 0, 0));
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE);
    spvc_compiler_install_compiler_options(fragment_compiler, options);

    spvc_compiler_compile(vertex_compiler, &vertex_msl);
    spvc_compiler_compile(fragment_compiler, &fragment_msl);

    *out_vertex_msl = jeecs::basic::make_new_string(vertex_msl);
    *out_fragment_msl = jeecs::basic::make_new_string(fragment_msl);

    spvc_context_destroy(context);
}

void jegl_shader_generate_shader_source(shader_wrapper* shader_generator, jegl_shader* write_to_shader)
{
    shader_wrapper* shader_wrapper_ptr = (shader_wrapper*)shader_generator;

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
        jeecs::basic::make_new_string(shader_wrapper_ptr->m_vertex_hlsl_source);
    write_to_shader->m_fragment_hlsl_src =
        jeecs::basic::make_new_string(shader_wrapper_ptr->m_fragment_hlsl_source);

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
    write_to_shader->m_vertex_in = new jegl_shader::uniform_type[write_to_shader->m_vertex_in_count];
    for (size_t i = 0; i < write_to_shader->m_vertex_in_count; ++i)
        write_to_shader->m_vertex_in[i] = shader_wrapper_ptr->m_vin_layout.at(i);

    write_to_shader->m_fragment_out_count = shader_wrapper_ptr->m_fout_layout.size();
    write_to_shader->m_fragment_out = new jegl_shader::uniform_type[write_to_shader->m_fragment_out_count];
    for (size_t i = 0; i < write_to_shader->m_fragment_out_count; ++i)
        write_to_shader->m_fragment_out[i] = shader_wrapper_ptr->m_fout_layout.at(i);

    std::unordered_map<std::string, const shader_uniform_block_info*> _uniform_blocks;
    for (auto& uniform_block : shader_wrapper_ptr->m_uniform_blocks)
    {
        auto result = _uniform_blocks.insert(
            std::make_pair(uniform_block.m_name, &uniform_block));

        (void)result;
        assert(result.second);
    }

    std::map<uint32_t, std::set<uint32_t>> sampler_binded_texture_passes;
    do
    {
        jegl_shader::unifrom_variables** last = &write_to_shader->m_custom_uniforms;
        for (auto& uniform_info : shader_wrapper_ptr->m_uniform_vars)
        {
            jegl_shader::unifrom_variables* variable = new jegl_shader::unifrom_variables();
            variable->m_next = nullptr;

            variable->m_name = jeecs::basic::make_new_string(uniform_info.m_name);
            variable->m_uniform_type = uniform_info.m_type;
            variable->m_index = jeecs::typing::INVALID_UINT32;
            variable->m_value = uniform_info.m_init_value;
            variable->m_updated = true;

            *last = variable;
            last = &variable->m_next;
        }

        for (auto& texture : shader_wrapper_ptr->m_textures)
        {
            (void)sampler_binded_texture_passes[texture.m_sampler_id].insert(texture.m_pass);

            jegl_shader::unifrom_variables* variable = new jegl_shader::unifrom_variables();
            variable->m_next = nullptr;
            variable->m_name = jeecs::basic::make_new_string(texture.m_name);
            variable->m_uniform_type = jegl_shader::uniform_type::TEXTURE;
            variable->m_index = jeecs::typing::INVALID_UINT32;
            variable->m_value.ix = texture.m_pass;
            variable->m_updated = true;
            *last = variable;
            last = &variable->m_next;
        }
    } while (false);

    do
    {
        jegl_shader::uniform_blocks** last = &write_to_shader->m_custom_uniform_blocks;
        for (auto& [_ /*useless*/, uniform_block_info] : _uniform_blocks)
        {
            jegl_shader::uniform_blocks* block = new jegl_shader::uniform_blocks();
            block->m_next = nullptr;

            assert(uniform_block_info->m_binding_place != jeecs::typing::INVALID_UINT32);

            block->m_name = jeecs::basic::make_new_string(uniform_block_info->m_name);
            block->m_specify_binding_place = uniform_block_info->m_binding_place;

            *last = block;
            last = &block->m_next;
        }
    } while (false);

    write_to_shader->m_sampler_count = shader_wrapper_ptr->m_samplers.size();
    auto* sampler_methods = new jegl_shader::sampler_method[write_to_shader->m_sampler_count];
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void jegl_shader_generator_init()
{
    glslang_initialize_process();
}
void jegl_shader_generator_shutdown()
{
    glslang_finalize_process();
}

#else
void jegl_shader_generator_init()
{
}
void jegl_shader_generator_shutdown()
{
}
#endif

jegl_resource* _jegl_load_shader_cache(jeecs_file* cache_file, const char* path)
{
    assert(cache_file != nullptr);

    jegl_shader* _shader = new jegl_shader();

    jegl_resource* shader = _create_resource();
    shader->m_type = jegl_resource::SHADER;
    shader->m_raw_shader_data = _shader;

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
    jeecs_file_read(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_hlsl_src = (const char*)je_mem_alloc((size_t)vertex_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_hlsl_src), sizeof(char), (size_t)vertex_hlsl_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_hlsl_src)[(size_t)vertex_hlsl_src_len] = 0;

    jeecs_file_read(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_hlsl_src = (const char*)je_mem_alloc((size_t)fragment_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_hlsl_src), sizeof(char), (size_t)fragment_hlsl_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_hlsl_src)[(size_t)fragment_hlsl_src_len] = 0;

    // 1.2. GLSL
    jeecs_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_glsl_src = (const char*)je_mem_alloc((size_t)vertex_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_glsl_src), sizeof(char), (size_t)vertex_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_glsl_src)[(size_t)vertex_glsl_src_len] = 0;

    jeecs_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_glsl_src = (const char*)je_mem_alloc((size_t)fragment_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_glsl_src), sizeof(char), (size_t)fragment_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_glsl_src)[(size_t)fragment_glsl_src_len] = 0;

    // 1.3. GLSL ES

    jeecs_file_read(&vertex_glsles_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_glsles_src = (const char*)je_mem_alloc((size_t)vertex_glsles_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_glsles_src), sizeof(char), (size_t)vertex_glsles_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_glsles_src)[(size_t)vertex_glsles_src_len] = 0;

    jeecs_file_read(&fragment_glsles_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_glsles_src = (const char*)je_mem_alloc((size_t)fragment_glsles_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_glsles_src), sizeof(char), (size_t)fragment_glsles_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_glsles_src)[(size_t)fragment_glsles_src_len] = 0;

    // 1.4. MSL (mac)
    jeecs_file_read(&vertex_msl_mac_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_msl_mac_src = (const char*)je_mem_alloc((size_t)vertex_msl_mac_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_msl_mac_src), sizeof(char), (size_t)vertex_msl_mac_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_msl_mac_src)[(size_t)vertex_msl_mac_src_len] = 0;

    jeecs_file_read(&fragment_msl_mac_src_len, sizeof(uint64_t), 1, cache_file);

    _shader->m_fragment_msl_mac_src = (const char*)je_mem_alloc((size_t)fragment_msl_mac_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_msl_mac_src), sizeof(char), (size_t)fragment_msl_mac_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_msl_mac_src)[(size_t)fragment_msl_mac_src_len] = 0;

    // 1.5. SPIR-V
    jeecs_file_read(&vertex_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)vertex_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);

    _shader->m_vertex_spirv_count = (size_t)vertex_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_vertex_spirv_codes = (const jegl_shader::spir_v_code_t*)je_mem_alloc((size_t)vertex_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t*>(_shader->m_vertex_spirv_codes), sizeof(char), (size_t)vertex_spirv_src_len, cache_file);

    jeecs_file_read(&fragment_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)fragment_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);

    _shader->m_fragment_spirv_count = (size_t)fragment_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_fragment_spirv_codes = (const jegl_shader::spir_v_code_t*)je_mem_alloc((size_t)fragment_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t*>(_shader->m_fragment_spirv_codes), sizeof(char), (size_t)fragment_spirv_src_len, cache_file);

    // 2. read shader config
    jeecs_file_read(&_shader->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cache_file);
    jeecs_file_read(&_shader->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_equation, sizeof(jegl_shader::blend_equation), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cache_file);

    // 3. read if shader is enable to shared?
    jeecs_file_read(&_shader->m_enable_to_shared, sizeof(bool), 1, cache_file);

    // 4. read and generate custom variable & uniform block informs

    // 4.1 read and generate custom variable
    uint64_t custom_uniform_count;
    jeecs_file_read(&custom_uniform_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniforms = nullptr;

    jegl_shader::unifrom_variables* last_create_variable = nullptr;
    for (uint64_t i = 0; i < custom_uniform_count; ++i)
    {
        jegl_shader::unifrom_variables* current_variable = new jegl_shader::unifrom_variables();
        if (_shader->m_custom_uniforms == nullptr)
            _shader->m_custom_uniforms = current_variable;

        if (last_create_variable != nullptr)
            last_create_variable->m_next = current_variable;

        // 4.1.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_variable->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_variable->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_variable->m_name)[(size_t)uniform_name_len] = 0;

        // 4.1.2 read type
        jeecs_file_read(&current_variable->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cache_file);

        // 4.1.3 read data
        static_assert(sizeof(current_variable->m_value.mat4x4) == sizeof(float[4][4]));
        jeecs_file_read(&current_variable->m_value.mat4x4, sizeof(float[4][4]), 1, cache_file);

        current_variable->m_index = jeecs::typing::INVALID_UINT32;
        current_variable->m_updated = false;

        last_create_variable = current_variable;
        current_variable->m_next = nullptr;
    }

    // 4.2 read uniform block informs
    uint64_t custom_uniform_block_count;
    jeecs_file_read(&custom_uniform_block_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniform_blocks = nullptr;

    jegl_shader::uniform_blocks* last_create_block = nullptr;
    for (uint64_t i = 0; i < custom_uniform_block_count; ++i)
    {
        jegl_shader::uniform_blocks* current_block = new jegl_shader::uniform_blocks();
        if (_shader->m_custom_uniform_blocks == nullptr)
            _shader->m_custom_uniform_blocks = current_block;

        if (last_create_block != nullptr)
            last_create_block->m_next = current_block;

        // 4.2.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_block->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_block->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_block->m_name)[(size_t)uniform_name_len] = 0;

        // 4.2.2 read binding place
        static_assert(sizeof(current_block->m_specify_binding_place) == sizeof(uint32_t));
        jeecs_file_read(&current_block->m_specify_binding_place, sizeof(uint32_t), 1, cache_file);

        last_create_block = current_block;
        current_block->m_next = nullptr;
    }

    // 4.3 read shader vertex layout
    uint64_t vertex_in_count;
    jeecs_file_read(&vertex_in_count, sizeof(uint64_t), 1, cache_file);
    _shader->m_vertex_in_count = (size_t)vertex_in_count;
    _shader->m_vertex_in = new jegl_shader::uniform_type[_shader->m_vertex_in_count];
    jeecs_file_read(_shader->m_vertex_in, sizeof(jegl_shader::uniform_type),
        _shader->m_vertex_in_count, cache_file);

    uint64_t fragment_out_count;
    jeecs_file_read(&fragment_out_count, sizeof(uint64_t), 1, cache_file);
    _shader->m_fragment_out_count = (size_t)fragment_out_count;
    _shader->m_fragment_out = new jegl_shader::uniform_type[_shader->m_fragment_out_count];
    jeecs_file_read(_shader->m_fragment_out, sizeof(jegl_shader::uniform_type),
        _shader->m_fragment_out_count, cache_file);

    // 4.4 read sampler informations;
    uint64_t sampler_count;
    jeecs_file_read(&sampler_count, sizeof(uint64_t), 1, cache_file);
    _shader->m_sampler_count = (size_t)sampler_count;
    _shader->m_sampler_methods = new jegl_shader::sampler_method[(size_t)sampler_count];
    for (uint64_t i = 0; i < sampler_count; ++i)
    {
        auto& sampler = _shader->m_sampler_methods[i];
        jeecs_file_read(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_sampler_id, sizeof(uint32_t), 1, cache_file);
        jeecs_file_read(&sampler.m_pass_id_count, sizeof(uint64_t), 1, cache_file);

        sampler.m_pass_ids = new uint32_t[(size_t)sampler.m_pass_id_count];
        jeecs_file_read(sampler.m_pass_ids, sizeof(uint32_t), (size_t)sampler.m_pass_id_count, cache_file);
    }

    jeecs_file_close(cache_file);

    shader->m_path = jeecs::basic::make_new_string(path);
    return shader;
}
void jegl_shader_free_generated_shader_source(jegl_shader* write_to_shader)
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
}

WO_API wo_api jeecs_shader_wrap_result_pack(wo_vm vm, wo_value args)
{
    /*
    func _wraped_shader(
                vertex_source: string,
                fragment_source: string,
                vertex_in_layout: array<woshader::Type>,
                fragment_out_layout: array<woshader::Type>,
                samplers: array<woshader::Sampler2D>,
                texture_passes: array<(string, int, int)>,
                uniform_variables: array<(string, woshader::Type, woshader::ShaderValueImm)>,
                uniform_blocks: array<(string, int)>,
                config: ShaderConfig)=> gchandle;
    using ShaderConfig = struct{
        shared: mut bool,
        ztest     : mut ZConfig,
        zwrite    : mut GConfig,
        blend_equation: mut BlendEquation,
        blend_src : mut BlendConfig,
        blend_dst : mut BlendConfig,
        cull      : mut CullConfig,
    };
    */
    enum WoshaderType
    {
        Float,
        Float2,
        Float3,
        Float4,
        Float2x2,
        Float3x3,
        Float4x4,
        Integer,
        Integer2,
        Integer3,
        Integer4,
        Texture2D,
        Structure, // User defined structure.
    };
    enum WoshaderFilter
    {
        NEAREST,
        LINEAR,
    };
    enum WoshaderWrap
    {
        CLAMP,
        REPEAT,
    };
    auto parse_woshader_type_to_je_uniform_type =
        [](WoshaderType t)
        {
            switch (t)
            {
            case Float: return jegl_shader::uniform_type::FLOAT;
            case Float2: return jegl_shader::uniform_type::FLOAT2;
            case Float3: return jegl_shader::uniform_type::FLOAT3;
            case Float4: return jegl_shader::uniform_type::FLOAT4;
            case Float2x2: return jegl_shader::uniform_type::FLOAT2X2;
            case Float3x3: return jegl_shader::uniform_type::FLOAT3X3;
            case Float4x4: return jegl_shader::uniform_type::FLOAT4X4;
            case Integer: return jegl_shader::uniform_type::INT;
            case Integer2: return jegl_shader::uniform_type::INT2;
            case Integer3: return jegl_shader::uniform_type::INT3;
            case Integer4: return jegl_shader::uniform_type::INT4;
            default:
                jeecs::debug::logfatal("Unsupport woshader type: %d.", (int)t);
                return jegl_shader::uniform_type::FLOAT;
            }

        };
    auto parse_woshader_filter_to_je_filter =
        [](WoshaderFilter f)
        {
            switch (f)
            {
            case WoshaderFilter::LINEAR:
                return jegl_shader::fliter_mode::LINEAR;
            case WoshaderFilter::NEAREST:
                return jegl_shader::fliter_mode::NEAREST;
            default:
                jeecs::debug::logfatal("Unsupport woshader filter type: %d.", (int)f);
                return jegl_shader::fliter_mode::LINEAR;
            }
        };
    auto parse_woshader_wrap_to_je_wrap =
        [](WoshaderWrap w)
        {
            switch (w)
            {
            case WoshaderWrap::CLAMP:
                return jegl_shader::wrap_mode::CLAMP;
            case WoshaderWrap::REPEAT:
                return jegl_shader::wrap_mode::REPEAT;
            default:
                jeecs::debug::logfatal("Unsupport woshader wrap type: %d.", (int)w);
                return jegl_shader::wrap_mode::CLAMP;
            }
        };

    wo_value s = wo_reserve_stack(vm, 3, &args);
    wo_value tmp = s + 0;
    wo_value tmp2 = s + 1;
    wo_value tmp3 = s + 2;

    shader_wrapper* wrapper = new shader_wrapper();

    wrapper->m_vertex_hlsl_source = wo_string(args + 0);
    wrapper->m_fragment_hlsl_source = wo_string(args + 1);

    wo_value vertex_in_layout = args + 2;
    wo_value fragment_out_layout = args + 3;
    wo_value samplers = args + 4;
    wo_value texture_passes = args + 5;
    wo_value uniform_variables = args + 6;
    wo_value uniform_blocks = args + 7;
    wo_value configs = args + 8;

    const auto vin_layout_len = wo_arr_len(vertex_in_layout);
    wrapper->m_vin_layout.resize(vin_layout_len);
    for (size_t i = 0; i < vin_layout_len; ++i)
    {
        wo_arr_get(tmp, vertex_in_layout, i);
        wrapper->m_vin_layout.at(i) =
            parse_woshader_type_to_je_uniform_type((WoshaderType)wo_int(tmp));
    }

    const auto fout_layout_len = wo_arr_len(fragment_out_layout);
    wrapper->m_fout_layout.resize(fout_layout_len);
    for (size_t i = 0; i < fout_layout_len; ++i)
    {
        wo_arr_get(tmp, fragment_out_layout, i);
        wrapper->m_fout_layout.at(i) =
            parse_woshader_type_to_je_uniform_type((WoshaderType)wo_int(tmp));
    }

    const auto sampler_count = wo_arr_len(samplers);
    wrapper->m_samplers.resize(sampler_count);
    for (size_t i = 0; i < sampler_count; ++i)
    {
        auto& sampler = wrapper->m_samplers.at(i);
        wo_arr_get(tmp, samplers, i);

        wo_struct_get(tmp2, tmp, 0);
        sampler.m_min = parse_woshader_filter_to_je_filter((WoshaderFilter)wo_int(tmp2));
        wo_struct_get(tmp2, tmp, 1);
        sampler.m_mag = parse_woshader_filter_to_je_filter((WoshaderFilter)wo_int(tmp2));
        wo_struct_get(tmp2, tmp, 2);
        sampler.m_mip = parse_woshader_filter_to_je_filter((WoshaderFilter)wo_int(tmp2));

        wo_struct_get(tmp2, tmp, 3);
        sampler.m_uwrap = parse_woshader_wrap_to_je_wrap((WoshaderWrap)wo_int(tmp2));
        wo_struct_get(tmp2, tmp, 4);
        sampler.m_vwrap = parse_woshader_wrap_to_je_wrap((WoshaderWrap)wo_int(tmp2));
    }

    const auto texture_passes_len = wo_arr_len(texture_passes);
    wrapper->m_textures.resize(texture_passes_len);
    for (size_t i = 0; i < texture_passes_len; ++i)
    {
        auto& texture_pass = wrapper->m_textures.at(i);
        wo_arr_get(tmp, texture_passes, i);

        wo_struct_get(tmp2, tmp, 0);
        texture_pass.m_name = wo_string(tmp2);
        wo_struct_get(tmp2, tmp, 1);
        texture_pass.m_pass = (uint32_t)wo_int(tmp2);
        wo_struct_get(tmp2, tmp, 2);
        texture_pass.m_sampler_id = (uint32_t)wo_int(tmp2);
    }

    const auto uniform_variables_count = wo_arr_len(uniform_variables);
    wrapper->m_uniform_vars.resize(uniform_variables_count);
    for (size_t i = 0; i < uniform_variables_count; ++i)
    {
        auto& uniform_variable = wrapper->m_uniform_vars.at(i);
        wo_arr_get(tmp, uniform_variables, i);

        wo_struct_get(tmp2, tmp, 0);
        uniform_variable.m_name = wo_string(tmp2);

        wo_struct_get(tmp2, tmp, 1);
        uniform_variable.m_type =
            parse_woshader_type_to_je_uniform_type((WoshaderType)wo_int(tmp2));

        wo_struct_get(tmp2, tmp, 2);

        // Get imm value from ShaderValueImm.
        wo_struct_get(tmp, tmp2, 1);
        switch (uniform_variable.m_type)
        {
        case jegl_shader::uniform_type::INT:
            uniform_variable.m_init_value.ix = (int)wo_int(tmp);
            break;
        case jegl_shader::uniform_type::INT2:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.ix = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.iy = (int)wo_int(tmp2);
            break;
        case jegl_shader::uniform_type::INT3:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.ix = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.iy = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.iz = (int)wo_int(tmp2);
            break;
        case jegl_shader::uniform_type::INT4:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.ix = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.iy = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.iz = (int)wo_int(tmp2);
            wo_struct_get(tmp2, tmp, 3);
            uniform_variable.m_init_value.iw = (int)wo_int(tmp2);
            break;
        case jegl_shader::uniform_type::FLOAT:
            uniform_variable.m_init_value.x = wo_float(tmp);
            break;
        case jegl_shader::uniform_type::FLOAT2:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.x = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.y = wo_float(tmp2);
            break;
        case jegl_shader::uniform_type::FLOAT3:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.x = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.y = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.z = wo_float(tmp2);
            break;
        case jegl_shader::uniform_type::FLOAT4:
            wo_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.x = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.y = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.z = wo_float(tmp2);
            wo_struct_get(tmp2, tmp, 3);
            uniform_variable.m_init_value.w = wo_float(tmp2);
            break;
        case jegl_shader::uniform_type::FLOAT2X2:
            for (size_t x = 0; x < 2; ++x)
            {
                wo_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 2; ++y)
                {
                    wo_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.mat2x2[x][y] = wo_float(tmp3);
                }
            }
            break;
        case jegl_shader::uniform_type::FLOAT3X3:
            for (size_t x = 0; x < 3; ++x)
            {
                wo_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 3; ++y)
                {
                    wo_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.mat3x3[x][y] = wo_float(tmp3);
                }
            }
            break;
        case jegl_shader::uniform_type::FLOAT4X4:
            for (size_t x = 0; x < 4; ++x)
            {
                wo_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 4; ++y)
                {
                    wo_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.mat4x4[x][y] = wo_float(tmp3);
                }
            }
            break;
        default:
            jeecs::debug::logfatal("Unsupported uniform variable type: %d", (int)uniform_variable.m_type);
        }
    }

    const auto uniform_block_count = wo_arr_len(uniform_blocks);
    wrapper->m_uniform_blocks.resize(uniform_block_count);
    for (size_t i = 0; i < uniform_block_count; ++i)
    {
        auto& uniform_block = wrapper->m_uniform_blocks.at(i);
        wo_arr_get(tmp, uniform_blocks, i);

        wo_struct_get(tmp2, tmp, 0);
        uniform_block.m_name = wo_string(tmp2);

        wo_struct_get(tmp2, tmp, 1);
        uniform_block.m_binding_place = (uint32_t)wo_int(tmp2);
    }

    wo_struct_get(tmp, configs, 0);
    wrapper->m_shader_config.m_enable_shared = wo_bool(tmp);
    wo_struct_get(tmp, configs, 1);
    wrapper->m_shader_config.m_depth_test = (jegl_shader::depth_test_method)wo_int(tmp);
    wo_struct_get(tmp, configs, 2);
    wrapper->m_shader_config.m_depth_mask = (jegl_shader::depth_mask_method)wo_int(tmp);
    wo_struct_get(tmp, configs, 3);
    wrapper->m_shader_config.m_blend_equation = (jegl_shader::blend_equation)wo_int(tmp);
    wo_struct_get(tmp, configs, 4);
    wrapper->m_shader_config.m_blend_src = (jegl_shader::blend_method)wo_int(tmp);
    wo_struct_get(tmp, configs, 5);
    wrapper->m_shader_config.m_blend_dst = (jegl_shader::blend_method)wo_int(tmp);
    wo_struct_get(tmp, configs, 6);
    wrapper->m_shader_config.m_cull_mode = (jegl_shader::cull_mode)wo_int(tmp);

    return wo_ret_gchandle(
        vm,
        wrapper,
        nullptr,
        [](wo_ptr_t p)
        {
            delete reinterpret_cast<shader_wrapper*>(p);
        });
}
