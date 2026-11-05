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

struct shader_configs
{
    bool m_enable_shared;
    je_gl_shader_depth_test_method_t m_depth_test;
    je_gl_shader_depth_mask_method_t m_depth_mask;
    je_gl_shader_blend_equation_t m_blend_equation;
    je_gl_shader_blend_method_t m_blend_src, m_blend_dst;
    je_gl_shader_cull_mode_t m_cull_mode;
};

struct shader_sampler
{
    je_gl_shader_filter_mode_t m_min;
    je_gl_shader_filter_mode_t m_mag;
    je_gl_shader_filter_mode_t m_mip;
    je_gl_shader_wrap_mode_t m_uwrap;
    je_gl_shader_wrap_mode_t m_vwrap;
    uint32_t m_sampler_id;
};

struct shader_uniform_variable
{
    std::string m_name;
    je_gl_shader_uniform_type_t m_type;
    je_gl_shader_uniform_mask_t m_uniform_mask;
    je_gl_shader_uniform_value_t m_init_value;
};

struct shader_texture
{
    std::string m_name;
    size_t m_pass;
    size_t m_sampler_id;
    je_gl_texture_kind_t m_kind;
};

struct shader_uniform_block_info
{
    std::string m_name;
    uint32_t m_binding_place;
};

struct shader_wrapper
{
    shader_configs m_shader_config;

    std::string m_vertex_hlsl_source;
    std::string m_fragment_hlsl_source;

    std::vector<je_gl_shader_uniform_type_t> m_vin_layout;
    std::vector<je_gl_shader_uniform_type_t> m_fout_layout;
    std::vector<shader_uniform_block_info> m_uniform_blocks;

    std::vector<shader_sampler> m_samplers;
    std::vector<shader_texture> m_textures;
    std::vector<shader_uniform_variable> m_uniform_vars;

};
