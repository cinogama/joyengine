#pragma once

#ifndef JE_IMPL
#define JE_IMPL
#endif
#include "jeecs.hpp"

#include <functional>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace jeecs::graphic::shader_wrapper
{
    struct ShaderConfigs
    {
        bool m_enable_shared;
        je_gl_shader_depth_test_method_t m_depth_test;
        je_gl_shader_depth_mask_method_t m_depth_mask;
        je_gl_shader_blend_equation_t m_blend_equation;
        je_gl_shader_blend_method_t m_blend_src, m_blend_dst;
        je_gl_shader_cull_mode_t m_cull_mode;
    };

    struct ShaderSampler
    {
        je_gl_shader_filter_mode_t m_min;
        je_gl_shader_filter_mode_t m_mag;
        je_gl_shader_filter_mode_t m_mip;
        je_gl_shader_wrap_mode_t m_uwrap;
        je_gl_shader_wrap_mode_t m_vwrap;
        uint32_t m_sampler_id;
    };

    struct ShaderUniformVariable
    {
        std::string m_name;
        je_gl_shader_uniform_type_t m_type;
        je_gl_shader_uniform_mask_t m_uniform_mask;
        je_gl_shader_uniform_value_t m_init_value;
    };

    struct ShaderTexture
    {
        std::string m_name;
        size_t m_pass;
        size_t m_sampler_id;
        je_gl_texture_kind_t m_kind;
    };

    struct ShaderUniformBlockInfo
    {
        std::string m_name;
        uint32_t m_binding_place;
    };

    struct shader_wrapper
    {
        ShaderConfigs m_shader_config;

        std::string m_vertex_hlsl_source;
        std::string m_fragment_hlsl_source;

        std::vector<je_gl_shader_uniform_type_t> m_vin_layout;
        std::vector<je_gl_shader_uniform_type_t> m_fout_layout;
        std::vector<ShaderUniformBlockInfo> m_uniform_blocks;

        std::vector<ShaderSampler> m_samplers;
        std::vector<ShaderTexture> m_textures;
        std::vector<ShaderUniformVariable> m_uniform_vars;
    };
}