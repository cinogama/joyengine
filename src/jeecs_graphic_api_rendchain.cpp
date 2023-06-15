#define JE_IMPL
#include "jeecs.hpp"

// NOTE:
// Rendchain 是 JoyEngine 的并行渲染接口，其目标本身并非为了并行渲染，而是
// 为了保证渲染系统、渲染Job 和渲染线程之间的同步和数据一致性。

struct jegl_rendchain;
struct jegl_uniform_data_node
{
    jegl_shader::uniform_type m_type;
    uint32_t m_binding_place;
    uint32_t* m_binding_place_addr;
    union
    {
        int m_int;
        float m_float;
        float m_float2[2];
        float m_float3[3];
        float m_float4[4];
        float m_float4x4[4][4];
    };
};
struct jegl_binding_texture_data_node
{
    size_t m_texture_index;
    std::unordered_map<size_t, jegl_resource*> m_binding_textures;
};
struct jegl_rendchain_rend_action
{
    jegl_rendchain* m_chain;
    jegl_resource* m_vertex;
    jegl_resource* m_shader;
    std::vector<size_t> m_binding_uniforms;
    size_t m_binding_textures;
};
struct jegl_rendchain
{
    JECS_DISABLE_MOVE_AND_COPY(jegl_rendchain);

    jegl_rendchain() = default;

    bool m_clear_target_frame_color_buffer;
    bool m_clear_target_frame_depth_buffer;
    jegl_resource* m_target_frame_buffer;
    size_t m_target_frame_buffer_viewport[4];

    std::unordered_set<jegl_resource*> m_used_resource;

    size_t m_used_uniform_count;
    std::vector<jegl_uniform_data_node> m_used_uniforms;

    size_t m_rend_action_count;
    std::vector<jegl_rendchain_rend_action> m_rend_actions;

    size_t m_binding_textures_count;
    std::vector<jegl_binding_texture_data_node> m_binding_textures;

    std::vector<jegl_resource*> m_binding_uniform_buffer;
    std::vector<size_t> m_pre_bind_tetuxre_group;
};

jegl_rendchain* jegl_rchain_create()
{
    jegl_rendchain* chain = new jegl_rendchain;
    return chain;
}
void jegl_rchain_close(jegl_rendchain* chain)
{
    delete chain;
}
void jegl_rchain_begin(jegl_rendchain* chain, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    assert(framebuffer == nullptr || framebuffer->m_type == jegl_resource::type::FRAMEBUF);

    chain->m_target_frame_buffer = framebuffer;
    chain->m_target_frame_buffer_viewport[0] = x;
    chain->m_target_frame_buffer_viewport[1] = y;
    chain->m_target_frame_buffer_viewport[2] = w;
    chain->m_target_frame_buffer_viewport[3] = h;

    chain->m_clear_target_frame_color_buffer = false;
    chain->m_clear_target_frame_depth_buffer = false;
    chain->m_used_resource.clear();
    chain->m_binding_uniform_buffer.clear();
    chain->m_pre_bind_tetuxre_group.clear();
    chain->m_used_uniform_count = 0;
    chain->m_rend_action_count = 0;
    chain->m_binding_textures_count = 0;

    chain->m_used_resource.insert(framebuffer);
}
void jegl_rchain_bind_uniform_buffer(jegl_rendchain* chain, jegl_resource* uniformbuffer)
{
    assert(uniformbuffer->m_type == jegl_resource::type::UNIFORMBUF);
    chain->m_binding_uniform_buffer.push_back(uniformbuffer);
}
void jegl_rchain_clear_color_buffer(jegl_rendchain* chain)
{
    chain->m_clear_target_frame_color_buffer = true;
}
void jegl_rchain_clear_depth_buffer(jegl_rendchain* chain)
{
    chain->m_clear_target_frame_depth_buffer = true;
}
size_t jegl_rchain_allocate_texture_group(jegl_rendchain* chain)
{
    size_t texture_index = chain->m_binding_textures_count++;
    if (texture_index >= chain->m_binding_textures.size())
    {
        assert(chain->m_binding_textures.size() == texture_index);
        chain->m_binding_textures.push_back(jegl_binding_texture_data_node{
                texture_index
            });
    }
    chain->m_binding_textures[texture_index].m_binding_textures.clear();
    return texture_index;
}
jegl_rendchain_rend_action* jegl_rchain_draw(jegl_rendchain* chain, jegl_resource* shader, jegl_resource* vertex, size_t texture_group)
{
    assert(shader->m_type == jegl_resource::type::SHADER);
    assert(vertex->m_type == jegl_resource::type::VERTEX);

    chain->m_used_resource.insert(shader);
    chain->m_used_resource.insert(vertex);

    size_t current_id = chain->m_rend_action_count++;

    if (current_id >= chain->m_rend_actions.size())
    {
        assert(chain->m_rend_actions.size() == current_id);
        chain->m_rend_actions.emplace_back(jegl_rendchain_rend_action{ chain });
    }
    auto& action = chain->m_rend_actions[current_id];

    assert(texture_group == SIZE_MAX || texture_group < chain->m_binding_textures_count);

    action.m_binding_textures = texture_group;
    action.m_binding_uniforms.clear();
    action.m_shader = shader;
    action.m_vertex = vertex;
    return &action;
}

jegl_uniform_data_node* _jegl_rchain_get_uniform_node(jegl_rendchain_rend_action* act, int binding_place)
{
    size_t uniform_index = act->m_chain->m_used_uniform_count++;

    if (uniform_index >= act->m_chain->m_used_uniforms.size())
    {
        assert(act->m_chain->m_used_uniforms.size() == uniform_index);
        act->m_chain->m_used_uniforms.push_back({});
    }
    auto* uniform_addr = &act->m_chain->m_used_uniforms[uniform_index];
    act->m_binding_uniforms.push_back(uniform_index);
    uniform_addr->m_binding_place = binding_place;
    uniform_addr->m_binding_place_addr = &uniform_addr->m_binding_place;
    return uniform_addr;
}
void jegl_rchain_set_uniform_int(jegl_rendchain_rend_action* act, uint32_t binding_place, int val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT;
    uniform->m_int = val;
}
void jegl_rchain_set_uniform_float(jegl_rendchain_rend_action* act, uint32_t binding_place, float val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float = val;
}
void jegl_rchain_set_uniform_float2(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT2;
    uniform->m_float2[0] = x;
    uniform->m_float2[1] = y;
}
void jegl_rchain_set_uniform_float3(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y, float z)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT3;
    uniform->m_float3[0] = x;
    uniform->m_float3[1] = y;
    uniform->m_float3[2] = z;
}
void jegl_rchain_set_uniform_float4(jegl_rendchain_rend_action* act, uint32_t binding_place, float x, float y, float z, float w)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT4;
    uniform->m_float4[0] = x;
    uniform->m_float4[1] = y;
    uniform->m_float4[2] = z;
    uniform->m_float4[3] = w;
}
void jegl_rchain_set_uniform_float4x4(jegl_rendchain_rend_action* act, uint32_t binding_place, const float(*mat)[4])
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT4X4;

    static_assert(sizeof(uniform->m_float4x4) == sizeof(float) * 4 * 4);
    memcpy(uniform->m_float4x4, mat, sizeof(uniform->m_float4x4));
}

void jegl_rchain_set_builtin_uniform_int(jegl_rendchain_rend_action* act, uint32_t* binding_place, int val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::INT;
    uniform->m_int = val;
}
void jegl_rchain_set_builtin_uniform_float(jegl_rendchain_rend_action* act, uint32_t* binding_place, float val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float = val;
}
void jegl_rchain_set_builtin_uniform_float2(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::FLOAT2;
    uniform->m_float2[0] = x;
    uniform->m_float2[1] = y;
}
void jegl_rchain_set_builtin_uniform_float3(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y, float z)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::FLOAT3;
    uniform->m_float3[0] = x;
    uniform->m_float3[1] = y;
    uniform->m_float3[2] = z;
}
void jegl_rchain_set_builtin_uniform_float4(jegl_rendchain_rend_action* act, uint32_t* binding_place, float x, float y, float z, float w)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::FLOAT4;
    uniform->m_float4[0] = x;
    uniform->m_float4[1] = y;
    uniform->m_float4[2] = z;
    uniform->m_float4[3] = w;
}
void jegl_rchain_set_builtin_uniform_float4x4(jegl_rendchain_rend_action* act, uint32_t* binding_place, const float(*mat)[4])
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, *binding_place);
    uniform->m_binding_place_addr = binding_place;
    uniform->m_type = jegl_shader::uniform_type::FLOAT4X4;

    static_assert(sizeof(uniform->m_float4x4) == sizeof(float) * 4 * 4);
    memcpy(uniform->m_float4x4, mat, sizeof(uniform->m_float4x4));
}

void jegl_rchain_bind_texture(jegl_rendchain* chain, size_t texture_group, size_t binding_pass, jegl_resource* texture)
{
    assert(texture->m_type == jegl_resource::type::TEXTURE);
    assert(texture_group < chain->m_binding_textures_count);

    chain->m_binding_textures[texture_group].m_binding_textures[binding_pass] = texture;
    chain->m_used_resource.insert(texture);
}
void jegl_rchain_bind_pre_texture_group(jegl_rendchain* chain, size_t texture_group)
{
    assert(texture_group < chain->m_binding_textures_count);
    chain->m_pre_bind_tetuxre_group.push_back(texture_group);
}

void _jegl_commit_rendchain(jegl_thread* glthread, jegl_rendchain* chain);
bool _jegl_rchain_resource_used_by_chain(jegl_rendchain* chain, jegl_resource* resource)
{
    return chain->m_used_resource.find(resource) != chain->m_used_resource.end();
}
void jegl_rchain_commit(jegl_rendchain* chain, jegl_thread* glthread)
{
    _jegl_commit_rendchain(glthread, chain);

    // 遍历所有绘制命令，开始提交！
    jegl_rend_to_framebuffer(chain->m_target_frame_buffer, 
        chain->m_target_frame_buffer_viewport[0], 
        chain->m_target_frame_buffer_viewport[1],
        chain->m_target_frame_buffer_viewport[2],
        chain->m_target_frame_buffer_viewport[3]);

    if (chain->m_clear_target_frame_color_buffer)
        jegl_clear_framebuffer_color(chain->m_target_frame_buffer);
    if (chain->m_clear_target_frame_depth_buffer)
        jegl_clear_framebuffer_depth(chain->m_target_frame_buffer);

    for (auto* uniform_buffer : chain->m_binding_uniform_buffer)
        jegl_using_resource(uniform_buffer);

    for (auto pre_bind_texture_group_index : chain->m_pre_bind_tetuxre_group)
        for (auto& [pass, texture] : chain->m_binding_textures[pre_bind_texture_group_index].m_binding_textures)
            jegl_using_texture(texture, pass);

    size_t last_used_texture = SIZE_MAX;

    for (size_t aidx = 0; aidx < chain->m_rend_action_count; ++aidx)
    {
        auto& action = chain->m_rend_actions[aidx];

        if (last_used_texture != action.m_binding_textures && action.m_binding_textures != SIZE_MAX)
            for (auto& [pass, texture] : chain->m_binding_textures[action.m_binding_textures].m_binding_textures)
                jegl_using_texture(texture, pass);

        last_used_texture = action.m_binding_textures;

        jegl_using_resource(action.m_shader);
        for (auto& uniform_index : action.m_binding_uniforms)
        {
            auto& uniform_data = chain->m_used_uniforms[uniform_index];

            if (*uniform_data.m_binding_place_addr == jeecs::typing::INVALID_UINT32)
                continue;

            switch (uniform_data.m_type)
            {
            case jegl_shader::uniform_type::INT:
                jegl_uniform_int(action.m_shader, *uniform_data.m_binding_place_addr, uniform_data.m_int); break;
            case jegl_shader::uniform_type::FLOAT:
                jegl_uniform_float(action.m_shader, *uniform_data.m_binding_place_addr, uniform_data.m_float); break;
            case jegl_shader::uniform_type::FLOAT2:
                jegl_uniform_float2(action.m_shader, *uniform_data.m_binding_place_addr,
                    uniform_data.m_float2[0], uniform_data.m_float2[1]); 
                break;
            case jegl_shader::uniform_type::FLOAT3:
                jegl_uniform_float3(action.m_shader, *uniform_data.m_binding_place_addr,
                    uniform_data.m_float3[0], uniform_data.m_float3[1], uniform_data.m_float3[2]);
                break;
            case jegl_shader::uniform_type::FLOAT4:
                jegl_uniform_float4(action.m_shader, *uniform_data.m_binding_place_addr,
                    uniform_data.m_float4[0], uniform_data.m_float4[1], uniform_data.m_float4[2], uniform_data.m_float4[3]);
                break;
            case jegl_shader::uniform_type::FLOAT4X4:
                jegl_uniform_float4x4(action.m_shader, *uniform_data.m_binding_place_addr, uniform_data.m_float4x4); break;
            default:
                break;
            }
        }

        jegl_draw_vertex(action.m_vertex);
    }
}