#define JE_IMPL
#include "jeecs.hpp"

#include <list>

// NOTE:
// Rendchain 是 JoyEngine 的并行渲染接口，其目标本身并非为了并行渲染，而是
// 为了保证渲染系统、渲染Job 和渲染线程之间的同步和数据一致性。

struct jegl_rendchain;
struct jegl_uniform_data_node
{
    jegl_shader::uniform_type m_type;
    const uint32_t* m_binding_place_addr;

    jegl_shader::unifrom_variables::value m_value;
};
struct jegl_rchain_texture_group
{
    std::unordered_map<size_t, jegl_texture*> m_binding_textures;
};
struct jegl_rendchain_rend_action
{
    jegl_rendchain* m_chain;
    jegl_vertex* m_vertex;
    jegl_shader* m_shader;
    std::vector<jegl_uniform_data_node*> m_binding_uniforms;
    std::vector<jegl_uniform_buffer*> m_uniform_buffers;
    jegl_rchain_texture_group* m_binding_texture_group;
};
struct jegl_rendchain
{
    JECS_DISABLE_MOVE_AND_COPY(jegl_rendchain);

    jegl_rendchain() = default;

    jegl_frame_buffer* m_target_frame_buffer;
    int32_t m_target_frame_buffer_viewport[4];

    struct used_resources
    {
        struct resource_release_method
        {
            using release_method_t = void(*)(void*);

            release_method_t m_release_method;
            void* m_graphic_resource_instance;

            static void release_shader(void* shader_ptr)
            {
                jegl_close_shader(reinterpret_cast<jegl_shader*>(shader_ptr));
            }
            static void release_texture(void* texture_ptr)
            {
                jegl_close_texture(reinterpret_cast<jegl_texture*>(texture_ptr));
            }
            static void release_vertex(void* vertex_ptr)
            {
                jegl_close_vertex(reinterpret_cast<jegl_vertex*>(vertex_ptr));
            }
            static void release_uniform_buffer(void* uniform_buffer_ptr)
            {
                jegl_close_uniformbuf(reinterpret_cast<jegl_uniform_buffer*>(uniform_buffer_ptr));
            }
            static void release_frame_buffer(void* frame_buffer_ptr)
            {
                jegl_close_framebuf(reinterpret_cast<jegl_frame_buffer*>(frame_buffer_ptr));
            }

            resource_release_method(const resource_release_method&) = default;
            resource_release_method(resource_release_method&&) = default;
            resource_release_method& operator=(const resource_release_method&) = default;
            resource_release_method& operator=(resource_release_method&&) = default;

            resource_release_method(jegl_shader* shader)
                : m_release_method(&release_shader)
                , m_graphic_resource_instance(shader)
            {
            }
            resource_release_method(jegl_texture* texture)
                : m_release_method(&release_texture)
                , m_graphic_resource_instance(texture)
            {
            }
            resource_release_method(jegl_vertex* vertex)
                : m_release_method(&release_vertex)
                , m_graphic_resource_instance(vertex)
            {
            }
            resource_release_method(jegl_uniform_buffer* uniform_buffer)
                : m_release_method(&release_uniform_buffer)
                , m_graphic_resource_instance(uniform_buffer)
            {
            }
            resource_release_method(jegl_frame_buffer* frame_buffer)
                : m_release_method(&release_frame_buffer)
                , m_graphic_resource_instance(frame_buffer)
            {
            }

            void release()
            {
                m_release_method(m_graphic_resource_instance);
            }
        };
        std::unordered_map<jegl_resource_handle*, resource_release_method> m_used_shader;

        template<jeecs::graphic::requirements::basic_graphic_resource T>
        void use_resource(T* resource)
        {
            auto it = m_used_shader.find(&resource->m_handle);
            if (it == m_used_shader.end())
            {
                jegl_share_resource(resource);
                m_used_shader.emplace(
                    &resource->m_handle,
                    resource_release_method(resource));
            }
        }
        void release_all()
        {
            for (auto& [_, release_method] : m_used_shader)
                release_method.release();
            m_used_shader.clear();
        }
    };
    used_resources m_used_resource;

    jegl_frame_buffer_clear_operation*
        m_clear_operation;

    size_t m_used_clear_operation_count;
    std::vector<jegl_frame_buffer_clear_operation*>
        m_created_clear_operations;

    size_t m_used_uniform_count;
    std::vector<jegl_uniform_data_node*> m_created_uniforms;

    size_t m_binding_textures_count;
    std::vector<jegl_rchain_texture_group*> m_binding_textures;

    std::vector<jegl_uniform_buffer*> m_binding_uniform_buffer;

    size_t m_rend_action_count;
    std::vector<jegl_rendchain_rend_action> m_rend_actions;
};

jegl_rendchain* jegl_rchain_create()
{
    jegl_rendchain* chain = new jegl_rendchain{};
    return chain;
}
void jegl_rchain_close(jegl_rendchain* chain)
{
    chain->m_used_resource.release_all();

    for (auto* op : chain->m_created_clear_operations)
        delete op;
    for (auto* u : chain->m_created_uniforms)
        delete u;
    for (auto* t : chain->m_binding_textures)
        delete t;

    delete chain;
}
void jegl_rchain_begin(
    jegl_rendchain* chain,
    jegl_frame_buffer* framebuffer,
    int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    chain->m_target_frame_buffer = framebuffer;
    chain->m_target_frame_buffer_viewport[0] = x;
    chain->m_target_frame_buffer_viewport[1] = y;
    chain->m_target_frame_buffer_viewport[2] = static_cast<int32_t>(w);
    chain->m_target_frame_buffer_viewport[3] = static_cast<int32_t>(h);

    chain->m_clear_operation = nullptr;
    chain->m_used_resource.release_all();
    chain->m_binding_uniform_buffer.clear();
    chain->m_used_clear_operation_count = 0;
    chain->m_used_uniform_count = 0;
    chain->m_rend_action_count = 0;
    chain->m_binding_textures_count = 0;

    if (framebuffer != nullptr)
        chain->m_used_resource.use_resource(framebuffer);
}
void jegl_rchain_bind_uniform_buffer(
    jegl_rendchain* chain, jegl_uniform_buffer* uniformbuffer)
{
    chain->m_used_resource.use_resource(uniformbuffer);
    chain->m_binding_uniform_buffer.push_back(uniformbuffer);
}

jegl_frame_buffer_clear_operation* _jegl_rchain_prepare_clear_operation(
    jegl_rendchain* chain, jegl_frame_buffer_clear_operation::clear_type kind)
{
    const size_t current_id = chain->m_used_clear_operation_count++;
    if (current_id >= chain->m_created_clear_operations.size())
    {
        assert(chain->m_created_clear_operations.size() == current_id);
        chain->m_created_clear_operations.push_back(new jegl_frame_buffer_clear_operation{});
    }
    auto* op = chain->m_created_clear_operations[current_id];
    op->m_type = kind;
    op->m_next = chain->m_clear_operation;

    chain->m_clear_operation = op;
    return op;
}

void jegl_rchain_clear_color_buffer(
    jegl_rendchain* chain,
    size_t attachment_idx,
    const float clear_color_rgba[4])
{
    auto* op = _jegl_rchain_prepare_clear_operation(
        chain, jegl_frame_buffer_clear_operation::clear_type::COLOR);

    op->m_color = jegl_frame_buffer_clear_operation::clear_color_attachment
    {
        attachment_idx,
        {
            clear_color_rgba[0],
            clear_color_rgba[1],
            clear_color_rgba[2],
            clear_color_rgba[3],
        },
    };
}
void jegl_rchain_clear_depth_buffer(jegl_rendchain* chain, float clear_depth)
{
    auto* op = _jegl_rchain_prepare_clear_operation(
        chain, jegl_frame_buffer_clear_operation::clear_type::DEPTH);

    op->m_depth = jegl_frame_buffer_clear_operation::clear_depth_attachment
    {
        clear_depth,
    };
}
jegl_rchain_texture_group* jegl_rchain_allocate_texture_group(jegl_rendchain* chain)
{
    size_t texture_index = chain->m_binding_textures_count++;
    if (texture_index >= chain->m_binding_textures.size())
    {
        assert(chain->m_binding_textures.size() == texture_index);
        chain->m_binding_textures.push_back(new jegl_rchain_texture_group());
    }

    auto* texture_group = chain->m_binding_textures[texture_index];
    texture_group->m_binding_textures.clear();
    return texture_group;
}
jegl_rendchain_rend_action* jegl_rchain_draw(
    jegl_rendchain* chain, jegl_shader* shader, jegl_vertex* vertex, jegl_rchain_texture_group* texture_group)
{
    chain->m_used_resource.use_resource(shader);
    chain->m_used_resource.use_resource(vertex);

    const size_t current_id = chain->m_rend_action_count++;
    if (current_id >= chain->m_rend_actions.size())
    {
        assert(chain->m_rend_actions.size() == current_id);
        chain->m_rend_actions.emplace_back(jegl_rendchain_rend_action{ chain });
    }
    auto& action = chain->m_rend_actions[current_id];

    action.m_binding_texture_group = texture_group;
    action.m_binding_uniforms.clear();
    action.m_uniform_buffers.clear();
    action.m_shader = shader;
    action.m_vertex = vertex;
    return &action;
}

void jegl_rchain_set_uniform_buffer(
    jegl_rendchain_rend_action* act,
    jegl_uniform_buffer* uniform_buffer)
{
    act->m_chain->m_used_resource.use_resource(uniform_buffer);
    act->m_uniform_buffers.push_back(uniform_buffer);
}

jegl_uniform_data_node* _jegl_rchain_get_uniform_node(
    jegl_rendchain_rend_action* act, const uint32_t* binding_place_addr)
{
    const size_t uniform_index = act->m_chain->m_used_uniform_count++;
    if (uniform_index >= act->m_chain->m_created_uniforms.size())
    {
        assert(act->m_chain->m_created_uniforms.size() == uniform_index);
        act->m_chain->m_created_uniforms.push_back(new jegl_uniform_data_node{});
    }
    auto* uniform_addr = act->m_chain->m_created_uniforms[uniform_index];
    act->m_binding_uniforms.push_back(uniform_addr);
    uniform_addr->m_binding_place_addr = binding_place_addr;
    return uniform_addr;
}

void jegl_rchain_set_uniform_int(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int val)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT;
    uniform->m_value.m_int = val;
}
void jegl_rchain_set_uniform_int2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT2;
    uniform->m_value.m_int2[0] = x;
    uniform->m_value.m_int2[1] = y;
}
void jegl_rchain_set_uniform_int3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y,
    int z)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT3;
    uniform->m_value.m_int3[0] = x;
    uniform->m_value.m_int3[1] = y;
    uniform->m_value.m_int3[2] = z;
}
void jegl_rchain_set_uniform_int4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y,
    int z,
    int w)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::INT4;
    uniform->m_value.m_int4[0] = x;
    uniform->m_value.m_int4[1] = y;
    uniform->m_value.m_int4[2] = z;
    uniform->m_value.m_int4[3] = w;
}
void jegl_rchain_set_uniform_float(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float val)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT;
    uniform->m_value.m_float = val;
}
void jegl_rchain_set_uniform_float2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT2;
    uniform->m_value.m_float2[0] = x;
    uniform->m_value.m_float2[1] = y;
}
void jegl_rchain_set_uniform_float3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y,
    float z)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT3;
    uniform->m_value.m_float3[0] = x;
    uniform->m_value.m_float3[1] = y;
    uniform->m_value.m_float3[2] = z;
}
void jegl_rchain_set_uniform_float4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y,
    float z,
    float w)
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT4;
    uniform->m_value.m_float4[0] = x;
    uniform->m_value.m_float4[1] = y;
    uniform->m_value.m_float4[2] = z;
    uniform->m_value.m_float4[3] = w;
}
void jegl_rchain_set_uniform_float2x2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[2])
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT2X2;

    static_assert(sizeof(uniform->m_value.m_float2x2) == sizeof(float) * 2 * 2);
    memcpy(uniform->m_value.m_float2x2, mat, sizeof(uniform->m_value.m_float2x2));
}
void jegl_rchain_set_uniform_float3x3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[3])
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT3X3;

    static_assert(sizeof(uniform->m_value.m_float3x3) == sizeof(float) * 3 * 3);
    memcpy(uniform->m_value.m_float3x3, mat, sizeof(uniform->m_value.m_float3x3));
}
void jegl_rchain_set_uniform_float4x4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[4])
{
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    auto* uniform = _jegl_rchain_get_uniform_node(act, binding_place);
    uniform->m_type = jegl_shader::uniform_type::FLOAT4X4;

    static_assert(sizeof(uniform->m_value.m_float4x4) == sizeof(float) * 4 * 4);
    memcpy(uniform->m_value.m_float4x4, mat, sizeof(uniform->m_value.m_float4x4));
}

void jegl_rchain_bind_texture(
    jegl_rendchain* chain,
    jegl_rchain_texture_group* texture_group,
    size_t binding_pass,
    jegl_texture* texture)
{
    assert(texture_group != nullptr);

    texture_group->m_binding_textures[binding_pass] = texture;
    chain->m_used_resource.use_resource(texture);
}

jegl_frame_buffer* /* MAY NULL */ jegl_rchain_get_target_framebuf(jegl_rendchain* chain)
{
    return chain->m_target_frame_buffer;
}
void jegl_rchain_commit(jegl_rendchain* chain, jegl_context* glthread)
{
    // 遍历所有绘制命令，开始提交！
    jegl_rend_to_framebuffer(
        chain->m_target_frame_buffer,
        &chain->m_target_frame_buffer_viewport,
        chain->m_clear_operation);

    for (auto* uniform_buffer : chain->m_binding_uniform_buffer)
        jegl_bind_uniform_buffer(uniform_buffer);

    jegl_rchain_texture_group* last_used_texture_group = nullptr;
    for (size_t aidx = 0; aidx < chain->m_rend_action_count; ++aidx)
    {
        auto& action = chain->m_rend_actions[aidx];

        if (last_used_texture_group != action.m_binding_texture_group
            && action.m_binding_texture_group != nullptr)
        {
            for (auto& [pass, texture] :
                action.m_binding_texture_group->m_binding_textures)
            {
                jegl_bind_texture(texture, pass);
            }
        }
        last_used_texture_group = action.m_binding_texture_group;

        for (auto* uniform_buffer : action.m_uniform_buffers)
            jegl_bind_uniform_buffer(uniform_buffer);

        if (jegl_bind_shader(action.m_shader))
        {
            for (auto* uniform_data : action.m_binding_uniforms)
            {
                if (jeecs::graphic::PENDING_UNIFORM_LOCATION == *uniform_data->m_binding_place_addr)
                    continue;

                assert(uniform_data->m_type != jegl_shader::uniform_type::TEXTURE);

                jegl_set_uniform_value(
                    *uniform_data->m_binding_place_addr,
                    uniform_data->m_type,
                    &uniform_data->m_value);
            }
            jegl_draw_vertex(action.m_vertex);
        }
    }
}
