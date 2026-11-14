#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/animation.hpp"
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/core.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/io.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"

#include "wo.h"

#include "jeecs_graphic_render_internal.hpp"
#include "jeecs_graphic_shader_wrapper.hpp"

#include <cstdlib>
#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;
using namespace jeecs::graphic::shader_wrapper;

// Editor graphic helpers
WOORT_API woort_api wojeapi_mark_shared_glresource_outdated(void)
{
    auto* uhost = je_graphic_uhost_get_or_create_for_universe(static_cast<je_game_universe_t*>(woort_pointer(0)), nullptr);
    return woort_ret_bool(
        je_graphic_shared_resources_mark_outdated(
            je_graphic_uhost_get_context(uhost),
            woort_string(1)));
}

WOORT_API woort_api wojeapi_init_graphic_pipeline_for_editor(void)
{
    auto* uhost = je_graphic_uhost_get_or_create_for_universe(static_cast<je_game_universe_t*>(woort_pointer(0)), nullptr);
    je_graphic_uhost_set_skip_behavior(uhost, false);

    return woort_ret_void();
}

///////////////////////////////////////////////////////////////////////

WOORT_API woort_api wojeapi_texture2d_open(void)
{
    const woort_value universe_ptr = WOORT_RETURN_SLOT;

    je_game_universe_t* universe_addr = nullptr;
    if (woort_option_get(universe_ptr, 0))
        universe_addr = static_cast<je_game_universe_t*>(woort_pointer(universe_ptr));

    optional<basic::resource<graphic::texture2d>> loaded_texture;

    const woort_U8CString path = woort_string(1);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        je_gl_context_t* gcontext = nullptr;
        if (universe_addr != nullptr)
        {
            gcontext = je_graphic_uhost_get_context(
                je_graphic_uhost_get_or_create_for_universe(universe_addr, nullptr));
        }
        loaded_texture = graphic::texture2d::load(gcontext, path);
    }
    (void)woort_vm_swap(last);

    if (loaded_texture.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::texture2d>(loaded_texture.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::texture2d> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_texture2d_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);

    auto loaded_texture = graphic::texture2d::create(
        w, h, JE_GL_TEXTURE_FORMAT_RGBA);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture2d>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture2d> *)ptr;
        },
        nullptr);
}

// 创建带初始像素的 RGBA 纹理：pixels 为完整像素缓冲（width*height*4 字节，
// 自底向上行序）
WOORT_API woort_api wojeapi_texture2d_create_with_pixels(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);

    size_t buffer_length = 0;
    const void* pixels = woort_buffer(2, &buffer_length);

    assert(pixels != nullptr);
    assert(buffer_length == w * h * 4);

    auto loaded_texture = graphic::texture2d::create(
        w, h, JE_GL_TEXTURE_FORMAT_RGBA, pixels);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture2d>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture2d> *)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texture2d_bind_path(void)
{
    auto* loaded_texture =
        (basic::resource<graphic::texture2d> *)woort_gcpointer(0);

    auto& path = (*loaded_texture)->resource()->m_handle.m_path_may_null_if_builtin;
    if (path != nullptr)
        je_mem_free((void*)path);

    path = basic::make_new_string(woort_string(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_texture3d_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);
    const size_t d = (size_t)woort_int(2);

    auto loaded_texture = graphic::texture3d::create(
        w, h, d, JE_GL_TEXTURE_FORMAT_RGBA);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture3d>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture3d> *)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texturearray_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);
    const size_t layers = (size_t)woort_int(2);

    auto loaded_texture = graphic::texture_array::create(
        w, h, layers, JE_GL_TEXTURE_FORMAT_RGBA);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture_array>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture_array> *)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texturearray_layers(void)
{
    auto* texture =
        (basic::resource<graphic::texture_array> *)woort_gcpointer(0);
    return woort_ret_int((woort_Int)(*texture)->layers());
}

WOORT_API woort_api wojeapi_texturecube_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);

    auto loaded_texture = graphic::texture_cube::create(
        w, h, JE_GL_TEXTURE_FORMAT_RGBA);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture_cube>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture_cube> *)ptr;
        },
        nullptr);
}

WOORT_API woort_api wojeapi_texturemsaa_create(void)
{
    const size_t w = (size_t)woort_int(0);
    const size_t h = (size_t)woort_int(1);
    const size_t samples = (size_t)woort_int(2);

    auto loaded_texture = graphic::texture_msaa::create(
        w, h, samples, JE_GL_TEXTURE_FORMAT_RGBA);

    return woort_ret_gchandle(
        new basic::resource<graphic::texture_msaa>(loaded_texture),
        WOORT_IGNORE,
        [](void* ptr)
        {
            delete (basic::resource<graphic::texture_msaa> *)ptr;
        },
        nullptr);
}

// 读取任意纹理 gchandle 载荷的实际类别；所有 resource<textureN> 的
// 共享指针布局一致（首成员即包装对象指针），因此可以安全地按 texture2d
// 视图读取其内部 jegl 纹理的 m_kind 字段。
static je_gl_texture_kind_t woje_texture_kind_of(void* texture_gchandle_payload)
{
    auto* res = static_cast<
        basic::resource<graphic::texture2d> *>(
            texture_gchandle_payload);
    return (*res)->resource()->m_kind;
}

// 将二维纹理绑定到 Renderer::Textures2D 组件的指定通道；
// 组件只接受二维纹理，其他类别将记录错误并忽略
static void woje_bind_texture_to_component(
    Renderer::Textures2D* textures, size_t pass, void* texture_gchandle_payload)
{
    if (woje_texture_kind_of(texture_gchandle_payload) != je_gl_texture_kind_t::TEXTURE_2D)
    {
        debug::logerr(
            "Renderer::Textures2D component only accepts 2d textures.");
        return;
    }
    textures->bind_texture(
        pass,
        *(basic::resource<graphic::texture2d> *)texture_gchandle_payload);
}

/////////////////////////////////////////////////////////////
WOORT_API woort_api wojeapi_font_open(void)
{
    optional<basic::resource<graphic::font>> loaded_font;

    const woort_U8CString font_path = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_font = graphic::font::load(font_path);
    }
    (void)woort_vm_swap(last);

    if (loaded_font.has_value())
    {
        return woort_ret_option_gchandle(
            new basic::resource<graphic::font>(loaded_font.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::font> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_font_load_char(void)
{
    auto* loaded_font =
        static_cast<basic::resource<graphic::font> *>(
            woort_gcpointer(0));
    assert(loaded_font != nullptr);

    const graphic::character* ch =
        loaded_font->get()->get_character(
            (float)woort_int(1),
            static_cast<woort_Char>(woort_int(2)));

    /*
    public using character = struct{
            m_character: char,
            m_width: int,
            m_height: int,
            m_advise_width: int,
            m_advise_height: int,
            m_baseline_offset_x: int,
            m_baseline_offset_y: int,
        };
    */

    woort_value s;

    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;

    woort_set_struct(result, 7);

    woort_struct_set_int(result, 0, (woort_Int)ch->m_char);

    woort_struct_set_int(result, 1, ch->m_width);
    woort_struct_set_int(result, 2, ch->m_height);

    woort_struct_set_int(result, 3, ch->m_advance_x);
    woort_struct_set_int(result, 4, ch->m_advance_y);

    woort_struct_set_int(result, 5, ch->m_baseline_offset_x);
    woort_struct_set_int(result, 6, ch->m_baseline_offset_y);

    return woort_ret_value(result);
}

/////////////////////////////////////////////////////////////
WOORT_API woort_api wojeapi_shader_open(void)
{
    je_gl_context_t* gcontext = nullptr;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    if (woort_option_get(WOORT_RETURN_SLOT, 0))
    {
        gcontext = je_graphic_uhost_get_context(
            je_graphic_uhost_get_or_create_for_universe(
                static_cast<je_game_universe_t*>(woort_pointer(universe_ptr)), nullptr));
    }

    optional<basic::resource<graphic::shader>> loaded_shader;

    const woort_U8CString path = woort_string(1);
    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_shader = graphic::shader::load(gcontext, path);
    }
    (void)woort_vm_swap(last);

    if (loaded_shader.has_value())
    {
        return woort_ret_option_gchandle(
            new basic::resource<graphic::shader>(loaded_shader.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::shader> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_shader_create(void)
{
    je_gl_context_t* gcontext = nullptr;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    if (woort_option_get(universe_ptr, 0))
    {
        gcontext = je_graphic_uhost_get_context(
            je_graphic_uhost_get_or_create_for_universe(
                static_cast<je_game_universe_t*>(woort_pointer(universe_ptr)), nullptr));
    }

    optional<basic::resource<graphic::shader>> loaded_shader;
    const woort_U8CString shader_path = woort_string(1);
    const woort_U8CString shader_content = woort_string(2);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_shader = graphic::shader::create(
            gcontext, shader_path, shader_content);
    }
    (void)woort_vm_swap(last);

    if (loaded_shader.has_value())
    {
        return woort_ret_option_gchandle(
            new basic::resource<graphic::shader>(loaded_shader.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::shader> *)ptr;
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_textures_of_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_map = s + 0;
    const woort_value val = s + 2;

    woort_set_map(out_map);

    if (Renderer::Textures2D* textures = (Renderer::Textures2D*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Textures2D>()))
    {
        for (auto& texture : textures->textures)
        {
            woort_set_gchandle(
                val,
                new basic::resource<graphic::texture2d>(texture.m_texture),
                WOORT_IGNORE,
                [](void* ptr)
                {
                    delete (basic::resource<graphic::texture2d> *)ptr;
                },
                nullptr);
            (void)woort_map_set_by_int(out_map, (woort_Int)texture.m_pass_id, val);
        }
    }

    return woort_ret_value(out_map);
}

WOORT_API woort_api wojeapi_bind_texture_for_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    if (Renderer::Textures2D* textures = (Renderer::Textures2D*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Textures2D>()))
    {
        if (woort_option_get(WOORT_RETURN_SLOT, 2))
        {
            woje_bind_texture_to_component(
                textures,
                (size_t)woort_int(1),
                woort_gcpointer(WOORT_RETURN_SLOT));
        }
        else
        {
            textures->remove_texture((size_t)woort_int(1));
        }
    }

    // TODO: 如果当前实体不包含Renderer::Textures2D组件，在此panic?

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_shape_for_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    if (Renderer::Shape* shape = (Renderer::Shape*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Shape>()))
    {
        if (woort_option_get(WOORT_RETURN_SLOT, 1))
            shape->vertex.emplace(*(basic::resource<graphic::vertex> *)woort_gcpointer(WOORT_RETURN_SLOT));
        else
            shape->vertex.reset();
    }

    // TODO: 如果当前实体不包含Renderer::Textures2D组件，在此panic?

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_shape_of_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    if (Renderer::Shape* shape = (Renderer::Shape*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Shape>()))
    {
        if (shape->vertex.has_value())
            return woort_ret_option_gchandle(
                new basic::resource<graphic::vertex>(shape->vertex.value()),
                WOORT_IGNORE,
                [](void* ptr)
                {
                    delete (basic::resource<graphic::vertex> *)ptr;
                },
                nullptr);
    }

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_load(void)
{
    optional<basic::resource<graphic::vertex>> loaded_vertex;

    const woort_value universe_ptr = WOORT_RETURN_SLOT;
    je_gl_context_t* gcontext = nullptr;
    if (woort_option_get(universe_ptr, 0))
    {
        gcontext = je_graphic_uhost_get_context(
            je_graphic_uhost_get_or_create_for_universe(
                static_cast<je_game_universe_t*>(woort_pointer(universe_ptr)), nullptr));
    }

    const woort_U8CString path = woort_string(1);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_vertex = graphic::vertex::load(gcontext, path);
    }
    (void)woort_vm_swap(last);

    if (loaded_vertex.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::vertex>(loaded_vertex.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::vertex> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_create(void)
{
    // vertices: array<real>, indices: array<int>
    je_gl_vertex_type_t vertex_type = (je_gl_vertex_type_t)woort_int(0);
    vector<float> vertices(woort_vec_len(1));
    vector<uint32_t> indices(woort_vec_len(2));
    vector<je_gl_vertex_data_layout_t> formats(woort_vec_len(3));

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 1, i);
        vertices[i] = woort_unbox_float(WOORT_RETURN_SLOT);
    }
    for (size_t i = 0; i < indices.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 2, i);
        indices[i] = (uint32_t)woort_unbox_int(WOORT_RETURN_SLOT);
    }
    for (size_t i = 0; i < formats.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 3, i);
        formats[i] = {
            JE_GL_VERTEX_DATA_TYPE_FLOAT32,
            (size_t)woort_unbox_int(WOORT_RETURN_SLOT) };
    }

    optional<basic::resource<graphic::vertex>> loaded_vertex;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_vertex = graphic::vertex::create(
            vertex_type,
            vertices.data(),
            vertices.size() * sizeof(float),
            indices,
            formats);
    }
    (void)woort_vm_swap(last);

    if (loaded_vertex.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::vertex>(loaded_vertex.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::vertex> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_path(void)
{
    auto* loaded_vertex =
        (basic::resource<graphic::vertex> *)woort_gcpointer(0);

    if (auto path = (*loaded_vertex)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_option_string(path);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_vertex_aabb(void)
{
    auto* loaded_vertex =
        (basic::resource<graphic::vertex> *)woort_gcpointer(0);
    auto* raw_vertex = (*loaded_vertex)->resource();

    woort_set_struct(WOORT_RETURN_SLOT, 6);

    woort_struct_set_float(WOORT_RETURN_SLOT, 0, raw_vertex->m_x_min);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, raw_vertex->m_y_min);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, raw_vertex->m_z_min);
    woort_struct_set_float(WOORT_RETURN_SLOT, 3, raw_vertex->m_x_max);
    woort_struct_set_float(WOORT_RETURN_SLOT, 4, raw_vertex->m_y_max);
    woort_struct_set_float(WOORT_RETURN_SLOT, 5, raw_vertex->m_z_max);

    return woort_ret();
}

// 录制本帧的顶点内容并收缩激活索引数量（语义见
// je_graphic_cmd_update_vertex）。索引缓冲保持创建时的内容不变，因此
// 调用方应按"最坏容量创建、静态角点索引"的方式使用（文本批处理与
// 粒子系统在 C++ 侧的同款消费者）。注意包围盒不会被重算：需要视锥
// 剔除正确时应以覆盖完整范围的初始数据创建缓冲。
WOORT_API woort_api wojeapi_vertex_update(void)
{
    auto* loaded_vertex =
        (basic::resource<graphic::vertex> *)woort_gcpointer(0);
    auto* cmd =
        (je_gl_command_buffer_t*)woort_pointer(1);
    const size_t active_index_count = (size_t)woort_int(3);

    vector<float> vertices(woort_vec_len(2));
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, 2, i);
        vertices[i] = woort_unbox_float(WOORT_RETURN_SLOT);
    }

    (*loaded_vertex)->update_buffer(
        cmd,
        vertices.data(),
        vertices.size() * sizeof(float),
        active_index_count);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_shaders_of_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    if (Renderer::Shaders* shaders = (Renderer::Shaders*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Shaders>()))
    {
        for (auto& shader : shaders->shaders)
        {
            woort_set_gchandle(elem, new basic::resource<graphic::shader>(shader), WOORT_IGNORE, [](void* ptr)
                { delete (basic::resource<graphic::shader> *)ptr; }, nullptr);

            woort_vec_push(out_array, elem);
        }
    }

    // TODO: 如果当前实体不包含Renderer::Shaders组件，在此panic?
    return woort_ret_value(out_array);
}

WOORT_API woort_api wojeapi_set_shaders_of_entity(void)
{
    je_game_entity_t* entity = (je_game_entity_t*)woort_gcpointer(0);
    const woort_value shader_array = 1;

    if (Renderer::Shaders* shaders = (Renderer::Shaders*)je_ecs_world_entity_component_get(entity, typing::id<Renderer::Shaders>()))
    {
        shaders->shaders.clear();
        const size_t arrsize = woort_vec_len(shader_array);
        for (size_t i = 0; i < arrsize; ++i)
        {
            (void)woort_vec_get(WOORT_RETURN_SLOT, shader_array, i);

            basic::resource<graphic::shader>* shader =
                (basic::resource<graphic::shader> *)woort_gcpointer(
                    WOORT_RETURN_SLOT);
            shaders->shaders.push_back(*shader);
        }
    }

    // TODO: 如果当前实体不包含Renderer::Shaders组件，在此panic?
    return woort_ret_void();
}

static je_gl_shader_script_uniform_variant_t _woje_uniform_variant(
    const je_gl_shader_uniform_variables_t* uniform)
{
    if ((uniform->m_uniform_mask & JE_GL_SHADER_UNIFORM_MASK_COLOR) != 0)
        return JE_GL_SHADER_SCRIPT_UNIFORM_COLOR;

    switch (uniform->m_uniform_type)
    {
    case JE_GL_SHADER_UNIFORM_TYPE_INT:
        return JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER;
    case JE_GL_SHADER_UNIFORM_TYPE_INT2:
        return JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER2;
    case JE_GL_SHADER_UNIFORM_TYPE_INT3:
        return JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER3;
    case JE_GL_SHADER_UNIFORM_TYPE_INT4:
        return JE_GL_SHADER_SCRIPT_UNIFORM_INTEGER4;
    case JE_GL_SHADER_UNIFORM_TYPE_FLOAT:
        return JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT;
    case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2:
        return JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT2;
    case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3:
        return JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT3;
    case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4:
        return JE_GL_SHADER_SCRIPT_UNIFORM_FLOAT4;
    case JE_GL_SHADER_UNIFORM_TYPE_TEXTURE:
        return JE_GL_SHADER_SCRIPT_UNIFORM_TEXTURE;
    default:
        return JE_GL_SHADER_SCRIPT_UNIFORM_OTHERS;
    }
}

WOORT_API woort_api wojeapi_get_uniforms_from_shader(void)
{
    /*
    extern("libjoyecs", "wojeapi_get_uniforms_from_shader")
                func _get_uniforms_from_shader(
                    shad: shader
                )=> map<string, uniform_variable>;
    */
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_map = s + 0;
    const woort_value val = s + 1;
    const woort_value elem = s + 2;

    woort_set_map(out_map);

    auto* uniforms = (*shader)->resource()->m_custom_uniforms;

    while (uniforms)
    {
        woort_set_struct(val, 2);
        (void)woort_map_set_by_string(out_map, uniforms->m_name, val);

        woort_set_int(elem, (int)_woje_uniform_variant(uniforms));

        woort_struct_set(val, 0, elem);

        switch (uniforms->m_uniform_type)
        {
        case JE_GL_SHADER_UNIFORM_TYPE_INT:
        case JE_GL_SHADER_UNIFORM_TYPE_TEXTURE:
            woort_set_int(elem, uniforms->m_value.m_int);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT2:
            woort_set_struct(elem, 2);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int2[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int2[1]);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT3:
            woort_set_struct(elem, 3);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int3[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int3[1]);
            woort_struct_set_int(elem, 2, uniforms->m_value.m_int3[2]);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT4:
            woort_set_struct(elem, 4);
            woort_struct_set_int(elem, 0, uniforms->m_value.m_int4[0]);
            woort_struct_set_int(elem, 1, uniforms->m_value.m_int4[1]);
            woort_struct_set_int(elem, 2, uniforms->m_value.m_int4[2]);
            woort_struct_set_int(elem, 3, uniforms->m_value.m_int4[3]);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT:
            woort_set_float(elem, uniforms->m_value.m_float);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2:
            woort_set_struct(elem, 2);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float2[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float2[1]);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3:
            woort_set_struct(elem, 3);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float3[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float3[1]);
            woort_struct_set_float(elem, 2, uniforms->m_value.m_float3[2]);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4:
            woort_set_struct(elem, 4);
            woort_struct_set_float(elem, 0, uniforms->m_value.m_float4[0]);
            woort_struct_set_float(elem, 1, uniforms->m_value.m_float4[1]);
            woort_struct_set_float(elem, 2, uniforms->m_value.m_float4[2]);
            woort_struct_set_float(elem, 3, uniforms->m_value.m_float4[3]);
            break;
        default:
            // Unsupport type
            woort_set_nil(elem);
        }

        woort_struct_set(val, 1, elem);

        uniforms = uniforms->m_next;
    }

    return woort_ret_value(out_map);
}

WOORT_API woort_api wojeapi_set_uniforms_int(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_int2(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_int3(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3), (int)woort_int(4));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_int4(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), (int)woort_int(2), (int)woort_int(3), (int)woort_int(4), (int)woort_int(5));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_float(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1), woort_float(2));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_float2(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        math::vec2(woort_float(2), woort_float(3)));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_float3(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        math::vec3(woort_float(2), woort_float(3), woort_float(4)));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_float4(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    (*shader)->set_uniform(woort_string(1),
        math::vec4(woort_float(2), woort_float(3), woort_float(4), woort_float(5)));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_set_uniforms_color(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);
    // The caller supplies linear components; math::color stores linear too,
    // so the color overload is an explicit type check plus a float4 write.
    (*shader)->set_uniform(woort_string(1),
        math::color(woort_float(2), woort_float(3), woort_float(4), woort_float(5)));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_shader_path(void)
{
    auto* shader = (basic::resource<graphic::shader> *)woort_gcpointer(0);

    if (auto str = (*shader)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_option_string(str);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_texture2d_size(void)
{
    auto* texture = (basic::resource<graphic::texture2d> *)woort_gcpointer(0);
    auto sz = texture->get()->size();

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)sz.x);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)sz.y);

    return woort_ret();
}

WOORT_API woort_api wojeapi_texture2d_path(void)
{
    auto* texture = (basic::resource<graphic::texture2d> *)woort_gcpointer(0);

    if (auto str = (*texture)->resource()->m_handle.m_path_may_null_if_builtin)
        return woort_ret_option_string(str);
    return woort_ret_option_none();
}

///////////////////////////////////////////////////////////////
WOORT_API woort_api wojeapi_framebuffer_create(void)
{
    const size_t reso_w = (size_t)woort_int(0);
    const size_t reso_h = (size_t)woort_int(1);
    const woort_value formats_arr = 2;
    const bool contain_depth = woort_bool(3);

    const size_t fmt_count = woort_vec_len(formats_arr);
    vector<je_gl_texture_format_t> formats(fmt_count);

    for (size_t i = 0; i < fmt_count; ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, formats_arr, i);
        formats[i] = (je_gl_texture_format_t)woort_unbox_int(WOORT_RETURN_SLOT);
    }

    optional<basic::resource<graphic::framebuffer>> loaded_framebuffer;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        loaded_framebuffer = graphic::framebuffer::create(
            reso_w, reso_h, formats, contain_depth);
    }
    (void)woort_vm_swap(last);

    if (loaded_framebuffer.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::framebuffer>(loaded_framebuffer.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::framebuffer> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_framebuffer_get_attachment(void)
{
    auto* loaded_framebuffer =
        (basic::resource<graphic::framebuffer> *)woort_gcpointer(0);

    auto attachment = (*loaded_framebuffer)->get_attachment((size_t)woort_int(1));

    if (attachment.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::texture2d>(attachment.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::texture2d> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_framebuffer_get_size(void)
{
    auto* loaded_framebuffer =
        (basic::resource<graphic::framebuffer> *)woort_gcpointer(0);
    auto sz = loaded_framebuffer->get()->size();

    woort_set_struct(WOORT_RETURN_SLOT, 2);

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)sz.x);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)sz.y);

    return woort_ret();
}

/////////////////////////// RENDCHAIN API ///////////////////////////

WOORT_API woort_api wojeapi_uniformbuffer_create(void)
{
    const size_t binding_place = (size_t)woort_int(0);
    const size_t buffer_size = (size_t)woort_int(1);

    optional<basic::resource<graphic::uniformbuffer>> ub;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        ub = graphic::uniformbuffer::create(binding_place, buffer_size);
    }
    (void)woort_vm_swap(last);

    if (ub.has_value())
        return woort_ret_option_gchandle(
            new basic::resource<graphic::uniformbuffer>(ub.value()),
            WOORT_IGNORE,
            [](void* ptr)
            {
                delete (basic::resource<graphic::uniformbuffer> *)ptr;
            },
            nullptr);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_uniformbuffer_update(void)
{
    auto* ub =
        (basic::resource<graphic::uniformbuffer> *)woort_gcpointer(0);
    auto* cmd =
        (je_gl_command_buffer_t*)woort_pointer(1);

    size_t buflen = 0;
    const void* buf = woort_buffer(2, &buflen);

    (*ub)->update_buffer(cmd, buf, buflen);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_uhost_get_or_create_for_universe(void)
{
    je_game_universe_t* universe = static_cast<je_game_universe_t*>(woort_pointer(0));

    je_graphic_uhost_t* host;

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        host = je_graphic_uhost_get_or_create_for_universe(universe, nullptr);
    }
    (void)woort_vm_swap(last);

    return woort_ret_pointer(host);
}

WOORT_API woort_api wojeapi_uhost_get_context(void)
{
    auto* host = (je_graphic_uhost_t*)woort_pointer(0);
    return woort_ret_pointer(je_graphic_uhost_get_context(host));
}

WOORT_API woort_api wojeapi_uhost_set_skip_behavior(void)
{
    auto* host = (je_graphic_uhost_t*)woort_pointer(0);
    bool skip_all_draw = woort_bool(1);

    je_graphic_uhost_set_skip_behavior(host, skip_all_draw);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_uhost_alloc_branch(void)
{
    auto* host = (je_graphic_uhost_t*)woort_pointer(0);
    return woort_ret_pointer(je_graphic_uhost_alloc_branch(host));
}

WOORT_API woort_api wojeapi_uhost_free_branch(void)
{
    auto* host = (je_graphic_uhost_t*)woort_pointer(0);
    auto* branch = (je_render_branch_t*)woort_pointer(1);

    je_graphic_uhost_free_branch(host, branch);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_branch_new_frame(void)
{
    auto* branch = (je_render_branch_t*)woort_pointer(0);
    int priority = (int)woort_int(1);

    je_graphic_branch_new_frame(branch, priority);

    return woort_ret_void();
}

/////////////////////////////////////////////////////////////////////////////
// Render API (v2) bindings
/////////////////////////////////////////////////////////////////////////////

namespace
{
    // A shader+vertex pair bound to a command buffer by
    // wojeapi_command_buffer_bind_shader. Scripts set per-draw uniforms on it
    // by name and finish with the explicit wojeapi_bound_shader_draw.
    struct woje_bound_shader
    {
        je_gl_command_buffer_t* m_command;
        je_gl_pipeline_t* m_pipeline;
        uint32_t m_index_count;
    };

    unordered_map<uint64_t, je_gl_pipeline_t*> _woje_pipeline_cache;

    je_gl_topology_t _woje_topology_of(const je_gl_vertex_t* vertex)
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

    je_gl_pipeline_t* _woje_get_pipeline(
        je_gl_context_t* context, je_gl_shader_t* shader, const je_gl_vertex_t* vertex)
    {
        const auto topology = _woje_topology_of(vertex);
        const uint64_t key =
            (uint64_t)(uintptr_t)shader ^ ((uint64_t)topology << 48);

        auto found = _woje_pipeline_cache.find(key);
        if (found != _woje_pipeline_cache.end())
            return found->second;

        je_gl_pipeline_state_t state = je_graphic_pipeline_state_from_shader(shader);
        state.m_topology = topology;

        je_gl_pipeline_desc_t desc{};
        desc.m_shader = shader;
        desc.m_state = &state;

        je_gl_pipeline_t* pipeline = je_graphic_pipeline_create(context, &desc);
        _woje_pipeline_cache.emplace(key, pipeline);
        return pipeline;
    }

    thread_local woje_bound_shader _woje_bound_shaders[64];
    thread_local size_t _woje_bound_shader_index = 0;

    je_gl_uniform_slot_t _woje_bound_shader_uniform_slot(
        woje_bound_shader* bound, const char* name)
    {
        // Editor shaders do not necessarily consume every builtin uniform
        // (e.g. a gizmo shader may use only JE_MVP). Absent uniforms are a
        // normal optional case here; the typed setters below skip them.
        return je_graphic_pipeline_uniform_slot(bound->m_pipeline, name);
    }
}

WOORT_API woort_api wojeapi_branch_new_command_buffer(void)
{
    auto* branch = (je_render_branch_t*)woort_pointer(0);

    je_gl_frame_buffer_t* fb = nullptr;
    basic::resource<graphic::framebuffer>* fb_res = nullptr;

    if (woort_option_get(WOORT_RETURN_SLOT, 1))
    {
        fb_res =
            (basic::resource<graphic::framebuffer> *)
                woort_gcpointer(WOORT_RETURN_SLOT);
        fb = (*fb_res)->resource();
    }

    int32_t x = (int32_t)woort_int(2);
    int32_t y = (int32_t)woort_int(3);
    uint32_t w = (uint32_t)woort_int(4);
    uint32_t h = (uint32_t)woort_int(5);

    if (w == 0 || h == 0)
    {
        if (fb_res != nullptr)
        {
            w = (uint32_t)(*fb_res)->width();
            h = (uint32_t)(*fb_res)->height();
        }
        else
        {
            int window_w = 0, window_h = 0;
            je_io_window_size_get(&window_w, &window_h);
            w = (uint32_t)window_w;
            h = (uint32_t)window_h;
        }
    }

    assert(w > 0 && h > 0);
    const je_gl_viewport_t viewport{ x, y, w, h };

    return woort_ret_pointer(
        je_graphic_branch_new_command_buffer(branch, fb, &viewport));
}

WOORT_API woort_api wojeapi_command_buffer_bind_uniform_buffer(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    auto* ub =
        (basic::resource<graphic::uniformbuffer> *)woort_gcpointer(1);

    je_graphic_cmd_bind_uniform_buffer(cmd, (*ub)->resource());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_command_buffer_clear_color_buffer(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    const uint32_t attachment = (uint32_t)woort_int(1);

    const woort_value color_tuple = 2;
    float clear_color[4];
    clear_color[0] = (float)woort_struct_get_real(color_tuple, 0);
    clear_color[1] = (float)woort_struct_get_real(color_tuple, 1);
    clear_color[2] = (float)woort_struct_get_real(color_tuple, 2);
    clear_color[3] = (float)woort_struct_get_real(color_tuple, 3);

    je_graphic_cmd_clear_color(cmd, attachment, clear_color);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_command_buffer_clear_depth_buffer(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    je_graphic_cmd_clear_depth(cmd, woort_float(1));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_command_buffer_bind_texture(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    const uint32_t pass = (uint32_t)woort_int(1);
    auto* tex =
        (basic::resource<graphic::texture2d> *)woort_gcpointer(2);

    switch (woje_texture_kind_of(tex))
    {
    case je_gl_texture_kind_t::TEXTURE_2D:
        je_graphic_cmd_bind_texture_2d(cmd, pass, (*tex)->resource());
        break;
    case je_gl_texture_kind_t::TEXTURE_3D:
        je_graphic_cmd_bind_texture_3d(
            cmd, pass,
            (*(basic::resource<graphic::texture3d> *)tex)->resource());
        break;
    case je_gl_texture_kind_t::TEXTURE_ARRAY:
        je_graphic_cmd_bind_texture_array(
            cmd, pass,
            (*(basic::resource<graphic::texture_array> *)tex)->resource());
        break;
    case je_gl_texture_kind_t::TEXTURE_CUBE:
        je_graphic_cmd_bind_texture_cube(
            cmd, pass,
            (*(basic::resource<graphic::texture_cube> *)tex)->resource());
        break;
    default:
        debug::logerr(
            "Unknown texture kind when binding texture to command buffer.");
        break;
    }

    return woort_ret_void();
}

// 持久区段上传（见 je_graphic_cmd_upload_texture_2d）：pixels 为
// width*height*4 字节的 RGBA 缓冲，行序自底向上
WOORT_API woort_api wojeapi_command_buffer_upload_texture_2d(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    auto* tex =
        (basic::resource<graphic::texture2d> *)woort_gcpointer(1);

    const size_t x = (size_t)woort_int(2);
    const size_t y = (size_t)woort_int(3);
    const size_t w = (size_t)woort_int(4);
    const size_t h = (size_t)woort_int(5);

    size_t buffer_length = 0;
    const void* pixels = woort_buffer(6, &buffer_length);

    assert(pixels != nullptr);
    assert(buffer_length == w * h * 4);

    je_graphic_cmd_upload_texture_2d(
        cmd, (*tex)->resource(), x, y, w, h,
        static_cast<const je_gl_texture_pixel_data_t*>(pixels));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_command_buffer_bind_shader(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    auto* shad =
        (basic::resource<graphic::shader> *)woort_gcpointer(1);
    auto* vert =
        (basic::resource<graphic::vertex> *)woort_gcpointer(2);

    auto* shader_resource = (*shad)->resource();
    auto* vertex_resource = (*vert)->resource();

    auto* pipeline = _woje_get_pipeline(
        je_graphic_cmd_get_context(cmd), shader_resource, vertex_resource);

    je_graphic_cmd_bind_pipeline(cmd, pipeline);
    je_graphic_cmd_bind_vertex(cmd, vertex_resource);

    auto& bound =
        _woje_bound_shaders[_woje_bound_shader_index++ % 64];
    bound.m_command = cmd;
    bound.m_pipeline = pipeline;
    bound.m_index_count = (uint32_t)vertex_resource->m_index_count;

    return woort_ret_pointer(&bound);
}

WOORT_API woort_api wojeapi_bound_shader_draw(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    je_graphic_cmd_draw_indexed(
        bound->m_command, bound->m_index_count, 0, 1);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_command_buffer_commit(void)
{
    auto* cmd = (je_gl_command_buffer_t*)woort_pointer(0);
    je_graphic_cmd_commit(cmd);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_int(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_int(bound->m_command, slot, (int)woort_int(2));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_int2(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_int2(
        bound->m_command, slot, (int)woort_int(2), (int)woort_int(3));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_int3(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_int3(bound->m_command, slot,
        (int)woort_int(2), (int)woort_int(3), (int)woort_int(4));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_int4(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_int4(bound->m_command, slot,
        (int)woort_int(2), (int)woort_int(3), (int)woort_int(4), (int)woort_int(5));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_float(bound->m_command, slot, woort_float(2));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float2(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_float2(
        bound->m_command, slot, woort_float(2), woort_float(3));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float3(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_float3(bound->m_command, slot,
        woort_float(2), woort_float(3), woort_float(4));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float4(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    je_graphic_cmd_set_uniform_float4(bound->m_command, slot,
        woort_float(2), woort_float(3), woort_float(4), woort_float(5));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_color(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    // Do not reinterpret the components as the final pipeline values: route
    // them through math::color so its storage-to-linear conversion stays the
    // only place that knows how a color is represented.
    const math::vec4 linear = math::color(
        woort_float(2), woort_float(3), woort_float(4), woort_float(5))
        .to_linear();
    je_graphic_cmd_set_uniform_float4(bound->m_command, slot,
        linear.x, linear.y, linear.z, linear.w);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float2x2(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    float mat[2][2];
    for (int i = 0; i < 4; ++i)
        (&mat[0][0])[i] = woort_float((woort_value)(2 + i));

    je_graphic_cmd_set_uniform_float2x2(bound->m_command, slot, mat);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float3x3(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    float mat[3][3];
    for (int i = 0; i < 9; ++i)
        (&mat[0][0])[i] = woort_float((woort_value)(2 + i));

    je_graphic_cmd_set_uniform_float3x3(bound->m_command, slot, mat);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_bound_shader_set_uniform_float4x4(void)
{
    auto* bound = (woje_bound_shader*)woort_pointer(0);
    const char* name = woort_string(1);
    auto slot = _woje_bound_shader_uniform_slot(bound, name);
    if (slot.m_offset == JE_GL_UNIFORM_SLOT_NONE)
        return woort_ret_void();

    float mat[4][4];
    for (int i = 0; i < 16; ++i)
        (&mat[0][0])[i] = woort_float((woort_value)(2 + i));

    je_graphic_cmd_set_uniform_float4x4(bound->m_command, slot, mat);
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_image_texture_cache_ensure(void)
{
    const string path = woort_string(0);

    je_file_t* cache = je_image_texture_cache_open_or_create(path.c_str());
    if (cache != nullptr)
    {
        je_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

// 为 RGBA8 像素数据选择体积最小的 BC 格式：全部不透明时 BC1（返回 1），
// 否则 BC3（返回 2）。pixels 为 width * height * 4 字节、自底向上行序。
WOORT_API woort_api wojeapi_texture_bc_format_choose(void)
{
    size_t buffer_length = 0;
    const void* pixels = woort_buffer(0, &buffer_length);

    const size_t width = (size_t)woort_int(1);
    const size_t height = (size_t)woort_int(2);

    assert(pixels != nullptr);
    assert(buffer_length == width * height * 4);

    return woort_ret_int((woort_Int)jeecs::graphic::texture_bc_format_for_rgba(
        width, height,
        static_cast<const je_gl_texture_pixel_data_t*>(pixels)));
}

// 把 RGBA8 像素数据压缩为 BC1/BC3 块数据（format：1 = BC1，2 = BC3），
// 返回可直接交给图形库上传的压缩缓冲；内存不足时返回 none。
WOORT_API woort_api wojeapi_texture_bc_compress(void)
{
    size_t buffer_length = 0;
    const void* pixels = woort_buffer(0, &buffer_length);

    const size_t width = (size_t)woort_int(1);
    const size_t height = (size_t)woort_int(2);
    const je_gl_texture_format_t format =
        (je_gl_texture_format_t)woort_int(3);

    assert(pixels != nullptr);
    assert(buffer_length == width * height * 4);
    assert(0 != (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK));

    auto* block_data = jeecs::graphic::compress_texture_bc(
        format, width, height,
        static_cast<const je_gl_texture_pixel_data_t*>(pixels));

    if (block_data == nullptr)
        return woort_ret_option_none();

    const size_t block_data_size =
        jeecs::graphic::texture_format_pixel_data_size(
            format, width, height);

    const woort_api result = woort_ret_option_buffer(block_data, block_data_size);
    free(block_data);

    return result;
}


// Shader wrap result pack (prebuild script)
WOORT_API woort_api jeecs_shader_wrap_result_pack(void)
{
    /*
    func _wraped_shader(
                vertex_source: string,
                fragment_source: string,
                vertex_in_layout: array<woshader::Type>,
                fragment_out_layout: array<woshader::Type>,
                samplers: array<woshader::Sampler2D>,
                texture_passes: array<(string, int, int, int)>,
                uniform_variables: array<(string, woshader::Type, woshader::ShaderValueImm)>,
                uniform_blocks: array<(string, int)>,
                config: ShaderConfig)=> gchandle;
    using ShaderConfig = struct{
        shared    : mut bool,
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
        Texture2DArray,
        Texture3D,
        TextureCube,
        Texture2DMS,
        Structure, // User defined structure.
        Color,     // float4 storage, tagged by woshader::Type::Color.
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
            case Float: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT;
            case Float2: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT2;
            case Float3: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT3;
            case Float4: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT4;
            case Float2x2: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT2X2;
            case Float3x3: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT3X3;
            case Float4x4: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT4X4;
            case Integer: return JE_GL_SHADER_UNIFORM_TYPE_INT;
            case Integer2: return JE_GL_SHADER_UNIFORM_TYPE_INT2;
            case Integer3: return JE_GL_SHADER_UNIFORM_TYPE_INT3;
            case Integer4: return JE_GL_SHADER_UNIFORM_TYPE_INT4;
            // Color uses FLOAT4 storage; the color semantics are carried by
            // the uniform mask recorded next to the type.
            case Color: return JE_GL_SHADER_UNIFORM_TYPE_FLOAT4;
            default:
                debug::logfatal("Unsupport woshader type: %d.", (int)t);
                return JE_GL_SHADER_UNIFORM_TYPE_FLOAT;
            }

        };
    auto parse_woshader_filter_to_je_filter =
        [](WoshaderFilter f)
        {
            switch (f)
            {
            case WoshaderFilter::LINEAR:
                return JE_GL_SHADER_FILTER_MODE_LINEAR;
            case WoshaderFilter::NEAREST:
                return JE_GL_SHADER_FILTER_MODE_NEAREST;
            default:
                debug::logfatal("Unsupport woshader filter type: %d.", (int)f);
                return JE_GL_SHADER_FILTER_MODE_LINEAR;
            }
        };
    auto parse_woshader_wrap_to_je_wrap =
        [](WoshaderWrap w)
        {
            switch (w)
            {
            case WoshaderWrap::CLAMP:
                return JE_GL_SHADER_WRAP_MODE_CLAMP;
            case WoshaderWrap::REPEAT:
                return JE_GL_SHADER_WRAP_MODE_REPEAT;
            default:
                debug::logfatal("Unsupport woshader wrap type: %d.", (int)w);
                return JE_GL_SHADER_WRAP_MODE_CLAMP;
            }
        };
    auto parse_woshader_texture_type_to_je_kind =
        [](WoshaderType t) -> je_gl_texture_kind_t
        {
            switch (t)
            {
            case WoshaderType::Texture2D:
                return je_gl_texture_kind_t::TEXTURE_2D;
            case WoshaderType::Texture2DArray:
                return je_gl_texture_kind_t::TEXTURE_ARRAY;
            case WoshaderType::Texture3D:
                return je_gl_texture_kind_t::TEXTURE_3D;
            case WoshaderType::TextureCube:
                return je_gl_texture_kind_t::TEXTURE_CUBE;
            case WoshaderType::Texture2DMS:
                return je_gl_texture_kind_t::TEXTURE_MSAA;
            default:
                debug::logfatal("Unsupport woshader texture type: %d.", (int)t);
                return je_gl_texture_kind_t::TEXTURE_2D;
            }
        };

    woort_value s;
    if (!woort_push_reserve(3, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value tmp = s + 0;
    const woort_value tmp2 = s + 1;
    const woort_value tmp3 = s + 2;

    ShaderWrapper* wrapper = new ShaderWrapper();

    wrapper->m_vertex_hlsl_source = woort_string(0);
    wrapper->m_fragment_hlsl_source = woort_string(1);

    const woort_value vertex_in_layout = 2;
    const woort_value fragment_out_layout = 3;
    const woort_value samplers = 4;
    const woort_value texture_passes = 5;
    const woort_value uniform_variables = 6;
    const woort_value uniform_blocks = 7;
    const woort_value configs = 8;

    const auto vin_layout_len = woort_vec_len(vertex_in_layout);
    wrapper->m_vin_layout.resize(vin_layout_len);
    for (size_t i = 0; i < vin_layout_len; ++i)
    {
        (void)woort_vec_get(tmp, vertex_in_layout, i);
        wrapper->m_vin_layout.at(i) =
            parse_woshader_type_to_je_uniform_type(
                static_cast<WoshaderType>(woort_unbox_int(tmp)));
    }

    const auto fout_layout_len = woort_vec_len(fragment_out_layout);
    wrapper->m_fout_layout.resize(fout_layout_len);
    for (size_t i = 0; i < fout_layout_len; ++i)
    {
        (void)woort_vec_get(tmp, fragment_out_layout, i);
        wrapper->m_fout_layout.at(i) =
            parse_woshader_type_to_je_uniform_type(
                static_cast<WoshaderType>(woort_unbox_int(tmp)));
    }

    const auto sampler_count = woort_vec_len(samplers);
    wrapper->m_samplers.resize(sampler_count);
    for (size_t i = 0; i < sampler_count; ++i)
    {
        auto& sampler = wrapper->m_samplers.at(i);

        sampler.m_sampler_id = static_cast<uint32_t>(i);

        (void)woort_vec_get(tmp, samplers, i);

        woort_struct_get(tmp2, tmp, 0);
        sampler.m_min = parse_woshader_filter_to_je_filter((WoshaderFilter)woort_int(tmp2));
        woort_struct_get(tmp2, tmp, 1);
        sampler.m_mag = parse_woshader_filter_to_je_filter((WoshaderFilter)woort_int(tmp2));
        woort_struct_get(tmp2, tmp, 2);
        sampler.m_mip = parse_woshader_filter_to_je_filter((WoshaderFilter)woort_int(tmp2));

        woort_struct_get(tmp2, tmp, 3);
        sampler.m_uwrap = parse_woshader_wrap_to_je_wrap((WoshaderWrap)woort_int(tmp2));
        woort_struct_get(tmp2, tmp, 4);
        sampler.m_vwrap = parse_woshader_wrap_to_je_wrap((WoshaderWrap)woort_int(tmp2));
    }

    const auto texture_passes_len = woort_vec_len(texture_passes);
    wrapper->m_textures.resize(texture_passes_len);
    for (size_t i = 0; i < texture_passes_len; ++i)
    {
        auto& texture_pass = wrapper->m_textures.at(i);
        (void)woort_vec_get(tmp, texture_passes, i);

        woort_struct_get(tmp2, tmp, 0);
        texture_pass.m_name = woort_string(tmp2);
        woort_struct_get(tmp2, tmp, 1);
        texture_pass.m_pass = (uint32_t)woort_int(tmp2);
        woort_struct_get(tmp2, tmp, 2);
        texture_pass.m_sampler_id = (uint32_t)woort_int(tmp2);
        woort_struct_get(tmp2, tmp, 3);
        texture_pass.m_kind =
            parse_woshader_texture_type_to_je_kind((WoshaderType)woort_int(tmp2));
    }

    const auto uniform_variables_count = woort_vec_len(uniform_variables);
    wrapper->m_uniform_vars.resize(uniform_variables_count);
    for (size_t i = 0; i < uniform_variables_count; ++i)
    {
        auto& uniform_variable = wrapper->m_uniform_vars.at(i);
        (void)woort_vec_get(tmp, uniform_variables, i);

        woort_struct_get(tmp2, tmp, 0);
        uniform_variable.m_name = woort_string(tmp2);

        woort_struct_get(tmp2, tmp, 1);
        const auto woshader_type = (WoshaderType)woort_int(tmp2);
        uniform_variable.m_type =
            parse_woshader_type_to_je_uniform_type(woshader_type);
        uniform_variable.m_uniform_mask = woshader_type == WoshaderType::Color
            ? JE_GL_SHADER_UNIFORM_MASK_COLOR
            : JE_GL_SHADER_UNIFORM_MASK_NONE;

        woort_struct_get(tmp2, tmp, 2);

        // Get imm value from ShaderValueImm.
        woort_struct_get(tmp, tmp2, 1);
        switch (uniform_variable.m_type)
        {
        case JE_GL_SHADER_UNIFORM_TYPE_INT:
            uniform_variable.m_init_value.m_int = (int)woort_int(tmp);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT2:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_int2[0] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_int2[1] = (int)woort_int(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT3:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_int3[0] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_int3[1] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.m_int3[2] = (int)woort_int(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_INT4:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_int4[0] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_int4[1] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.m_int4[2] = (int)woort_int(tmp2);
            woort_struct_get(tmp2, tmp, 3);
            uniform_variable.m_init_value.m_int4[3] = (int)woort_int(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT:
            uniform_variable.m_init_value.m_float = woort_float(tmp);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_float2[0] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_float2[1] = woort_float(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_float3[0] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_float3[1] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.m_float3[2] = woort_float(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4:
            woort_struct_get(tmp2, tmp, 0);
            uniform_variable.m_init_value.m_float4[0] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 1);
            uniform_variable.m_init_value.m_float4[1] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 2);
            uniform_variable.m_init_value.m_float4[2] = woort_float(tmp2);
            woort_struct_get(tmp2, tmp, 3);
            uniform_variable.m_init_value.m_float4[3] = woort_float(tmp2);
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2X2:
            for (size_t x = 0; x < 2; ++x)
            {
                woort_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 2; ++y)
                {
                    woort_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.m_float2x2[x][y] = woort_float(tmp3);
                }
            }
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3X3:
            for (size_t x = 0; x < 3; ++x)
            {
                woort_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 3; ++y)
                {
                    woort_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.m_float3x3[x][y] = woort_float(tmp3);
                }
            }
            break;
        case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4X4:
            for (size_t x = 0; x < 4; ++x)
            {
                woort_struct_get(tmp2, tmp, x);
                for (size_t y = 0; y < 4; ++y)
                {
                    woort_struct_get(tmp3, tmp2, y);
                    uniform_variable.m_init_value.m_float4x4[x][y] = woort_float(tmp3);
                }
            }
            break;
        default:
            debug::logfatal("Unsupported uniform variable type: %d", (int)uniform_variable.m_type);
        }
    }

    const auto uniform_block_count = woort_vec_len(uniform_blocks);
    wrapper->m_uniform_blocks.resize(uniform_block_count);
    for (size_t i = 0; i < uniform_block_count; ++i)
    {
        auto& uniform_block = wrapper->m_uniform_blocks.at(i);
        (void)woort_vec_get(tmp, uniform_blocks, i);

        woort_struct_get(tmp2, tmp, 0);
        uniform_block.m_name = woort_string(tmp2);

        woort_struct_get(tmp2, tmp, 1);
        uniform_block.m_binding_place = (uint32_t)woort_int(tmp2);
    }

    woort_struct_get(tmp, configs, 0);
    wrapper->m_shader_config.m_enable_shared = woort_bool(tmp);
    woort_struct_get(tmp, configs, 1);
    wrapper->m_shader_config.m_depth_test = (je_gl_shader_depth_test_method_t)woort_int(tmp);
    woort_struct_get(tmp, configs, 2);
    wrapper->m_shader_config.m_depth_mask = (je_gl_shader_depth_mask_method_t)woort_int(tmp);
    woort_struct_get(tmp, configs, 3);
    wrapper->m_shader_config.m_blend_equation = (je_gl_shader_blend_equation_t)woort_int(tmp);
    woort_struct_get(tmp, configs, 4);
    wrapper->m_shader_config.m_blend_src = (je_gl_shader_blend_method_t)woort_int(tmp);
    woort_struct_get(tmp, configs, 5);
    wrapper->m_shader_config.m_blend_dst = (je_gl_shader_blend_method_t)woort_int(tmp);
    woort_struct_get(tmp, configs, 6);
    wrapper->m_shader_config.m_cull_mode = (je_gl_shader_cull_mode_t)woort_int(tmp);

    return woort_ret_gchandle(
        wrapper,
        WOORT_IGNORE,
        [](void* p)
        {
            delete static_cast<ShaderWrapper*>(p);
        },
        nullptr);
}


// Animation cache ensure
WOORT_API woort_api wojeapi_animation_cache_ensure(void)
{
    const string path = woort_string(0);

    je_file_t* cache = je_animation_cache_open_or_create(path.c_str());
    if (nullptr != cache)
    {
        je_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}
