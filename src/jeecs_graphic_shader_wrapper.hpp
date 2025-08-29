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
    jegl_shader::depth_test_method m_depth_test;
    jegl_shader::depth_mask_method m_depth_mask;
    jegl_shader::blend_equation m_blend_equation;
    jegl_shader::blend_method m_blend_src, m_blend_dst;
    jegl_shader::cull_mode m_cull_mode;
};

struct shader_sampler
{
    jegl_shader::fliter_mode m_min;
    jegl_shader::fliter_mode m_mag;
    jegl_shader::fliter_mode m_mip;
    jegl_shader::wrap_mode m_uwrap;
    jegl_shader::wrap_mode m_vwrap;
    uint32_t m_sampler_id;

    std::vector<uint32_t> m_binded_texture_passid;
};

struct shader_uniform_variable
{
    std::string m_name;
    jegl_shader::uniform_type m_type;
};

struct shader_texture
{
    size_t m_pass;
    size_t m_sampler_id;
};

struct shader_uniform_block_info
{
    std::string name;
    uint32_t binding_place;
};

struct shader_wrapper
{
    shader_configs m_shader_config;

    std::string m_vertex_hlsl_source;
    std::string m_fragment_hlsl_source;

    std::vector<jegl_shader::uniform_type> m_vin_layout;
    std::vector<shader_uniform_block_info> m_uniform_blocks;

    std::vector<shader_sampler> m_samplers;
    std::vector<shader_texture> m_textures;
};
