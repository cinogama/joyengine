#define JE_IMPL
#include "jeecs.hpp"

// NOTE:
// Rendchain 是 JoyEngine 的并行渲染接口，其目标本身并非为了并行渲染，而是
// 为了保证渲染系统、渲染Job 和渲染线程之间的同步和数据一致性。

struct jegl_rendchain;
struct jegl_uniform_data_node
{
    jegl_shader::uniform_type m_type;
    int m_binding_place;
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
    const jegl_binding_texture_data_node* m_last_textures;
    std::unordered_map<size_t, jegl_resource*> m_binding_textures;
};
struct jegl_rendchain_rend_action
{
    jegl_rendchain* m_chain;
    jegl_resource* m_vertex;
    jegl_resource* m_shader;
    std::vector<const jegl_uniform_data_node*> m_binding_uniforms;
    jegl_binding_texture_data_node*            m_binding_textures;
};
struct jegl_rendchain
{
    JECS_DISABLE_MOVE_AND_COPY(jegl_rendchain);

    jegl_rendchain() = default;

    jegl_resource* m_target_frame_buffer;
    jeecs::math::vec4 m_target_frame_buffer_viewport;

    std::unordered_set<jegl_resource*> m_used_resource;

    size_t m_used_uniform_count;
    std::vector<jegl_uniform_data_node> m_used_uniforms;

    size_t m_rend_action_count;
    std::vector<jegl_rendchain_rend_action> m_rend_actions;

    size_t m_binding_textures_count;
    std::vector<jegl_binding_texture_data_node> m_binding_textures;
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
void jegl_rchain_begin(jegl_rendchain* chain, jegl_resource* framebuffer, float x, float y, float w, float h)
{
    assert(framebuffer->m_type == jegl_resource::type::FRAMEBUF);

    chain->m_target_frame_buffer = framebuffer;
    chain->m_target_frame_buffer_viewport = jeecs::math::vec4(x, y, w, h);

    chain->m_used_resource.clear();
    chain->m_used_uniform_count = 0;
    chain->m_rend_action_count = 0;
    chain->m_binding_textures_count = 0;

    chain->m_used_resource.insert(framebuffer);
}

jegl_binding_texture_data_node* _jegl_rchain_get_texture_node(jegl_rendchain* chain)
{
    size_t texture_index = chain->m_binding_textures_count++;
    if (texture_index <= chain->m_binding_textures.size())
    {
        assert(chain->m_binding_textures.size() == texture_index);
        chain->m_binding_textures.push_back(jegl_binding_texture_data_node{
            texture_index == 0? nullptr : &chain->m_binding_textures[texture_index - 1]
            });
    }
    auto* textures_addr = &chain->m_binding_textures[texture_index];
    textures_addr->m_binding_textures.clear();
    return textures_addr;
}
jegl_rendchain_rend_action* jegl_rchain_rend(jegl_rendchain* chain, jegl_resource* shader, jegl_resource* vertex)
{
    assert(shader->m_type == jegl_resource::type::SHADER);
    assert(vertex->m_type == jegl_resource::type::VERTEX);

    chain->m_used_resource.insert(shader);
    chain->m_used_resource.insert(vertex);

    size_t current_id = chain->m_rend_action_count++;

    if (current_id <= chain->m_rend_actions.size())
    {
        assert(chain->m_rend_actions.size() == current_id);
        chain->m_rend_actions.emplace_back(jegl_rendchain_rend_action{ chain });
    }
    auto& action = chain->m_rend_actions[current_id];
    action.m_binding_textures = _jegl_rchain_get_texture_node(chain);
    action.m_binding_uniforms.clear();
    action.m_shader = shader;
    action.m_vertex = vertex;
    return &action;
}

jegl_uniform_data_node* _jegl_rchain_get_uniform_node(jegl_rendchain_rend_action* act, int binding_place)
{
    size_t uniform_index = act->m_chain->m_used_uniform_count++;

    if (uniform_index <= act->m_chain->m_used_uniforms.size())
    {
        assert(chain->m_used_uniforms.size() == uniform_index);
        act->m_chain->m_used_uniforms.push_back({});
    }
    auto* uniform_addr = &act->m_chain->m_used_uniforms[uniform_index];
    act->m_binding_uniforms.push_back(uniform_addr);

    return uniform_addr;
}
void jegl_rchain_set_uniform_int(jegl_rendchain_rend_action* act, int binding_place, int val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT;
    uniform->m_int = val;
}
void jegl_rchain_set_uniform_float(jegl_rendchain_rend_action* act, int binding_place, float val)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float = val;
}
void jegl_rchain_set_uniform_float2(jegl_rendchain_rend_action* act, int binding_place, float x, float y)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float2[0] = x;
    uniform->m_float2[1] = y;
}
void jegl_rchain_set_uniform_float3(jegl_rendchain_rend_action* act, int binding_place, float x, float y, float z)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float3[0] = x;
    uniform->m_float3[1] = y;
    uniform->m_float3[2] = z;
}
void jegl_rchain_set_uniform_float4(jegl_rendchain_rend_action* act, int binding_place, float x, float y, float z, float w)
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_float4[0] = x;
    uniform->m_float4[1] = y;
    uniform->m_float4[2] = z;
    uniform->m_float4[3] = w;
}
void jegl_rchain_set_uniform_float4x4(jegl_rendchain_rend_action* act, int binding_place, float(*mat)[4])
{
    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;

    static_assert(sizeof(uniform->m_float4x4) == sizeof(float) * 4 * 4);
    memcpy(uniform->m_float4x4, mat, sizeof(uniform->m_float4x4));
}
void jegl_rchain_bind_texture(jegl_rendchain_rend_action* act, size_t binding_pass, jegl_resource* texture)
{
    assert(texture->m_type == jegl_resource::type::TEXTURE);
    if (act->m_binding_textures->m_last_textures != nullptr)
    {
        auto fnd = act->m_binding_textures->m_last_textures->m_binding_textures.find(binding_pass);
        if (fnd != act->m_binding_textures->m_last_textures->m_binding_textures.end() && fnd->second == texture)
            return;
    }
    act->m_binding_textures->m_binding_textures[binding_pass] = texture;
    act->m_chain->m_used_resource.insert(texture);
}

void jegl_rchain_commit(jegl_rendchain* chain)
{
    // 遍历所有绘制命令，开始提交！
    jegl_clear_framebuffer(chain->m_target_frame_buffer);
    jegl_rend_to_framebuffer(chain->m_target_frame_buffer, 
        chain->m_target_frame_buffer_viewport.x, 
        chain->m_target_frame_buffer_viewport.y,
        chain->m_target_frame_buffer_viewport.z,
        chain->m_target_frame_buffer_viewport.w);

    for (size_t aidx = 0; aidx < chain->m_rend_action_count; ++aidx)
    {
        auto& action = chain->m_rend_actions[aidx];
        for (auto& [pass, texture] : action.m_binding_textures->m_binding_textures)
            jegl_using_texture(texture, pass);

        jegl_using_resource(action.m_shader);
        for (auto& uniform_data : action.m_binding_uniforms)
        {
            switch (uniform_data->m_type)
            {
            case jegl_shader::uniform_type::INT:
                jegl_uniform_int(action.m_shader, uniform_data->m_binding_place, uniform_data->m_int); break;
            case jegl_shader::uniform_type::FLOAT:
                jegl_uniform_float(action.m_shader, uniform_data->m_binding_place, uniform_data->m_float); break;
            case jegl_shader::uniform_type::FLOAT2:
                jegl_uniform_float2(action.m_shader, uniform_data->m_binding_place, 
                    uniform_data->m_float2[0], uniform_data->m_float2[1]); 
                break;
            case jegl_shader::uniform_type::FLOAT3:
                jegl_uniform_float3(action.m_shader, uniform_data->m_binding_place, 
                    uniform_data->m_float3[0], uniform_data->m_float3[1], uniform_data->m_float3[2]);
                break;
            case jegl_shader::uniform_type::FLOAT4:
                jegl_uniform_float4(action.m_shader, uniform_data->m_binding_place, 
                    uniform_data->m_float4[0], uniform_data->m_float4[1], uniform_data->m_float4[2], uniform_data->m_float4[3]);
                break;
            case jegl_shader::uniform_type::FLOAT4X4:
                jegl_uniform_float4x4(action.m_shader, uniform_data->m_binding_place, uniform_data->m_float4x4); break;
            default:
                break;
            }
        }

        jegl_draw_vertex(action.m_vertex);
    }
}