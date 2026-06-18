#define JE_IMPL
#include "jeecs.hpp"

#include <cstring>
#include <deque>
#include <utility>
#include <vector>

// NOTE:
// Rendchain 是 JoyEngine 的并行渲染接口，其目标本身并非为了并行渲染，而是
// 为了保证渲染系统、渲染Job 和渲染线程之间的同步和数据一致性。

struct jegl_rendchain;

// 录制期间缓存单个 uniform 的类型和值，
// 提交时按 m_binding_place_addr 解析最终 location 并写入。
// m_binding_place_addr 指向 shader 内的 location 字段，
// 因为 shader uniform 表可能异步解析（PENDING_UNIFORM_LOCATION），
// 所以必须保留指针到 commit 时再读取。
struct jegl_uniform_data_node
{
    jegl_shader::uniform_type m_type;
    const uint32_t* m_binding_place_addr;

    jegl_shader::unifrom_variables::value m_value;
};

// 纹理组：一组 (texture_pass, texture) 绑定。
// 槽位数量通常 <8，使用扁平 vector + 线性查找，
// 比 unordered_map 更快且无 per-entry 堆分配。
struct jegl_rchain_texture_group
{
    std::vector<std::pair<size_t, jegl_texture*>> m_binding_textures;
};

// 单次绘制操作。
// m_binding_uniforms 存储的是 chain->m_created_uniforms 内的索引（非指针），
// 避免 vector 扩容导致悬挂指针。
struct jegl_rendchain_rend_action
{
    jegl_rendchain* m_chain;
    jegl_vertex* m_vertex;
    jegl_shader* m_shader;
    std::vector<size_t> m_binding_uniforms;
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

            // 泛型构造：通过 if constexpr 在编译期选择释放函数，
            // 替代原先 5 个重载构造，便于扩展新的 basic_graphic_resource 类型。
            template<jeecs::graphic::requirements::basic_graphic_resource T>
            resource_release_method(T* resource)
                : m_release_method(nullptr)
                , m_graphic_resource_instance(resource)
            {
                if constexpr (std::is_same_v<T, jegl_shader>)
                    m_release_method = &release_shader;
                else if constexpr (std::is_same_v<T, jegl_texture>)
                    m_release_method = &release_texture;
                else if constexpr (std::is_same_v<T, jegl_vertex>)
                    m_release_method = &release_vertex;
                else if constexpr (std::is_same_v<T, jegl_frame_buffer>)
                    m_release_method = &release_frame_buffer;
                else if constexpr (std::is_same_v<T, jegl_uniform_buffer>)
                    m_release_method = &release_uniform_buffer;
                else
                    static_assert(sizeof(T*) == 0, "Unsupported graphic resource type.");
            }

            void release()
            {
                m_release_method(m_graphic_resource_instance);
            }
        };

        // 扁平存储：单帧内唯一资源条目通常 <50，
        // 线性查找比 unordered_map 更快且无 per-entry 堆分配。
        struct entry
        {
            jegl_resource_handle* handle;
            resource_release_method release;
        };
        std::vector<entry> m_used_resources;

        template<jeecs::graphic::requirements::basic_graphic_resource T>
        void use_resource(T* resource)
        {
            jegl_resource_handle* const h = &resource->m_handle;
            for (const auto& e : m_used_resources)
            {
                if (e.handle == h)
                    return; // 已记录，引用计数只需提升一次
            }
            jegl_share_resource(resource);
            m_used_resources.push_back(entry{ h, resource_release_method(resource) });
        }
        void release_all()
        {
            for (auto& e : m_used_resources)
                e.release.release();
            m_used_resources.clear();
        }
    };
    used_resources m_used_resource;

    jegl_frame_buffer_clear_operation*
        m_clear_operation;

    size_t m_used_clear_operation_count;
    // deque 保证元素地址稳定（m_next 链表依赖），且 push_back 不失效已有地址；
    // 消除原先每节点一次 new/delete 的开销。
    std::deque<jegl_frame_buffer_clear_operation>
        m_created_clear_operations;

    size_t m_used_uniform_count;
    // 值存储 + 索引访问：消除 per-node 堆分配。
    // 索引在 vector 扩容后仍有效，避免悬挂指针。
    std::vector<jegl_uniform_data_node>
        m_created_uniforms;

    size_t m_binding_textures_count;
    // deque 保证 texture_group 地址稳定（公开 API 返回裸指针给外部回传）。
    std::deque<jegl_rchain_texture_group>
        m_binding_textures;

    size_t m_used_uniform_buffer_count;
    std::vector<jegl_uniform_buffer*>
        m_binding_uniform_buffer;

    size_t m_rend_action_count;
    std::vector<jegl_rendchain_rend_action>
        m_rend_actions;
};

// 内部辅助函数：匿名命名空间提供内部链接，
// 避免下划线前缀的全局保留标识符与外部 TU 冲突。
namespace
{
    jegl_frame_buffer_clear_operation* _jegl_rchain_prepare_clear_operation(
        jegl_rendchain* chain, jegl_frame_buffer_clear_operation::clear_type kind)
    {
        const size_t current_id = chain->m_used_clear_operation_count++;
        if (current_id >= chain->m_created_clear_operations.size())
        {
            assert(chain->m_created_clear_operations.size() == current_id);
            chain->m_created_clear_operations.push_back(jegl_frame_buffer_clear_operation{});
        }
        auto& op = chain->m_created_clear_operations[current_id];
        op.m_type = kind;
        // 头插法构建链表：m_next 指向前一个 op。
        // 注意：jegl_rend_to_framebuffer 接收的链表顺序为 LIFO（倒序于调用顺序），
        // 后端实现需保证对不同附件的清除操作幂等，不依赖调用顺序。
        op.m_next = chain->m_clear_operation;

        chain->m_clear_operation = &op;
        return chain->m_clear_operation;
    }

    // 返回 m_created_uniforms 内的索引（非指针），避免后续 push_back 导致悬挂。
    size_t _jegl_rchain_get_uniform_node(
        jegl_rendchain_rend_action* act, const uint32_t* binding_place_addr)
    {
        const size_t uniform_index = act->m_chain->m_used_uniform_count++;
        if (uniform_index >= act->m_chain->m_created_uniforms.size())
        {
            assert(act->m_chain->m_created_uniforms.size() == uniform_index);
            act->m_chain->m_created_uniforms.push_back(jegl_uniform_data_node{});
        }
        act->m_binding_uniforms.push_back(uniform_index);
        act->m_chain->m_created_uniforms[uniform_index].m_binding_place_addr = binding_place_addr;
        return uniform_index;
    }
}

jegl_rendchain* jegl_rchain_create()
{
    jegl_rendchain* chain = new jegl_rendchain{};
    return chain;
}
// NOTE: 调用后 chain 指针悬空，调用方需自行置空。
void jegl_rchain_close(jegl_rendchain* chain)
{
    assert(chain != nullptr);
    chain->m_used_resource.release_all();
    // 资源均以值存储，无需手动 delete；deque/vector 析构自动回收。
    delete chain;
}
void jegl_rchain_begin(
    jegl_rendchain* chain,
    jegl_frame_buffer* framebuffer,
    int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    assert(chain != nullptr);
    // viewport 字段为 int32_t，强制转换前确认无截断。
    assert(w <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    assert(h <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));

    chain->m_target_frame_buffer = framebuffer;
    chain->m_target_frame_buffer_viewport[0] = x;
    chain->m_target_frame_buffer_viewport[1] = y;
    chain->m_target_frame_buffer_viewport[2] = static_cast<int32_t>(w);
    chain->m_target_frame_buffer_viewport[3] = static_cast<int32_t>(h);

    chain->m_clear_operation = nullptr;
    chain->m_used_resource.release_all();
    chain->m_used_uniform_buffer_count = 0;
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
    assert(chain != nullptr);
    chain->m_used_resource.use_resource(uniformbuffer);

    // 池化复用：与其它池字段一致，按计数器重置，保留 vector 容量。
    const size_t current_id = chain->m_used_uniform_buffer_count++;
    if (current_id >= chain->m_binding_uniform_buffer.size())
    {
        assert(chain->m_binding_uniform_buffer.size() == current_id);
        chain->m_binding_uniform_buffer.push_back(uniformbuffer);
    }
    else
    {
        chain->m_binding_uniform_buffer[current_id] = uniformbuffer;
    }
}

void jegl_rchain_clear_color_buffer(
    jegl_rendchain* chain,
    size_t attachment_idx,
    const float clear_color_rgba[4])
{
    assert(chain != nullptr);
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
    assert(chain != nullptr);
    auto* op = _jegl_rchain_prepare_clear_operation(
        chain, jegl_frame_buffer_clear_operation::clear_type::DEPTH);

    op->m_depth = jegl_frame_buffer_clear_operation::clear_depth_attachment
    {
        clear_depth,
    };
}
jegl_rchain_texture_group* jegl_rchain_allocate_texture_group(jegl_rendchain* chain)
{
    assert(chain != nullptr);
    size_t texture_index = chain->m_binding_textures_count++;
    if (texture_index >= chain->m_binding_textures.size())
    {
        assert(chain->m_binding_textures.size() == texture_index);
        // deque 末尾插入不会失效已分配给外部的指针
        chain->m_binding_textures.push_back(jegl_rchain_texture_group{});
    }

    auto& texture_group = chain->m_binding_textures[texture_index];
    texture_group.m_binding_textures.clear();
    return &texture_group;
}
jegl_rendchain_rend_action* jegl_rchain_draw(
    jegl_rendchain* chain, jegl_shader* shader, jegl_vertex* vertex, jegl_rchain_texture_group* texture_group)
{
    assert(chain != nullptr);
    chain->m_used_resource.use_resource(shader);
    chain->m_used_resource.use_resource(vertex);

    const size_t current_id = chain->m_rend_action_count++;
    if (current_id >= chain->m_rend_actions.size())
    {
        assert(chain->m_rend_actions.size() == current_id);
        chain->m_rend_actions.push_back(jegl_rendchain_rend_action{ chain });
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
    assert(act != nullptr);
    act->m_chain->m_used_resource.use_resource(uniform_buffer);
    act->m_uniform_buffers.push_back(uniform_buffer);
}

void jegl_rchain_set_uniform_int(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int val)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::INT;
    uniform.m_value.m_int = val;
}
void jegl_rchain_set_uniform_int2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::INT2;
    uniform.m_value.m_int2[0] = x;
    uniform.m_value.m_int2[1] = y;
}
void jegl_rchain_set_uniform_int3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y,
    int z)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::INT3;
    uniform.m_value.m_int3[0] = x;
    uniform.m_value.m_int3[1] = y;
    uniform.m_value.m_int3[2] = z;
}
void jegl_rchain_set_uniform_int4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    int x,
    int y,
    int z,
    int w)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::INT4;
    uniform.m_value.m_int4[0] = x;
    uniform.m_value.m_int4[1] = y;
    uniform.m_value.m_int4[2] = z;
    uniform.m_value.m_int4[3] = w;
}
void jegl_rchain_set_uniform_float(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float val)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT;
    uniform.m_value.m_float = val;
}
void jegl_rchain_set_uniform_float2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT2;
    uniform.m_value.m_float2[0] = x;
    uniform.m_value.m_float2[1] = y;
}
void jegl_rchain_set_uniform_float3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y,
    float z)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT3;
    uniform.m_value.m_float3[0] = x;
    uniform.m_value.m_float3[1] = y;
    uniform.m_value.m_float3[2] = z;
}
void jegl_rchain_set_uniform_float4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    float x,
    float y,
    float z,
    float w)
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT4;
    uniform.m_value.m_float4[0] = x;
    uniform.m_value.m_float4[1] = y;
    uniform.m_value.m_float4[2] = z;
    uniform.m_value.m_float4[3] = w;
}
void jegl_rchain_set_uniform_float2x2(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[2])
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT2X2;

    static_assert(sizeof(uniform.m_value.m_float2x2) == sizeof(float) * 2 * 2);
    memcpy(uniform.m_value.m_float2x2, mat, sizeof(uniform.m_value.m_float2x2));
}
void jegl_rchain_set_uniform_float3x3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[3])
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT3X3;

    static_assert(sizeof(uniform.m_value.m_float3x3) == sizeof(float) * 3 * 3);
    memcpy(uniform.m_value.m_float3x3, mat, sizeof(uniform.m_value.m_float3x3));
}
void jegl_rchain_set_uniform_float4x4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place,
    const float (*mat)[4])
{
    assert(act != nullptr);
    if (binding_place == nullptr
        || *binding_place == jeecs::graphic::INVALID_UNIFORM_LOCATION)
        return;

    const size_t idx = _jegl_rchain_get_uniform_node(act, binding_place);
    auto& uniform = act->m_chain->m_created_uniforms[idx];
    uniform.m_type = jegl_shader::uniform_type::FLOAT4X4;

    static_assert(sizeof(uniform.m_value.m_float4x4) == sizeof(float) * 4 * 4);
    memcpy(uniform.m_value.m_float4x4, mat, sizeof(uniform.m_value.m_float4x4));
}

void jegl_rchain_bind_texture(
    jegl_rendchain* chain,
    jegl_rchain_texture_group* texture_group,
    size_t binding_pass,
    jegl_texture* texture)
{
    assert(chain != nullptr);
    assert(texture_group != nullptr);

    // 替换已存在 pass 的绑定；否则追加。槽位通常 <8，线性查找即可。
    bool updated = false;
    for (auto& [pass, tex] : texture_group->m_binding_textures)
    {
        if (pass == binding_pass)
        {
            tex = texture;
            updated = true;
            break;
        }
    }
    if (!updated)
        texture_group->m_binding_textures.push_back({ binding_pass, texture });

    chain->m_used_resource.use_resource(texture);
}

jegl_frame_buffer* /* MAY NULL */ jegl_rchain_get_target_framebuf(jegl_rendchain* chain)
{
    assert(chain != nullptr);
    return chain->m_target_frame_buffer;
}
void jegl_rchain_commit(jegl_rendchain* chain, jegl_context* glthread)
{
    assert(chain != nullptr);
    // 遍历所有绘制命令，开始提交！
    jegl_rend_to_framebuffer(
        chain->m_target_frame_buffer,
        &chain->m_target_frame_buffer_viewport,
        chain->m_clear_operation);

    for (size_t i = 0; i < chain->m_used_uniform_buffer_count; ++i)
        jegl_bind_uniform_buffer(chain->m_binding_uniform_buffer[i]);

    jegl_rchain_texture_group* last_used_texture_group = nullptr;
    for (size_t aidx = 0; aidx < chain->m_rend_action_count; ++aidx)
    {
        auto& action = chain->m_rend_actions[aidx];

        // 按设计契约，两次绘制使用的纹理组相互独立；
        // 这里不主动解绑上一次的纹理，是出于性能考虑，
        // 依赖后端在 shader/framebuffer 切换时重置纹理绑定状态。
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
            for (auto uniform_idx : action.m_binding_uniforms)
            {
                auto& uniform_data = chain->m_created_uniforms[uniform_idx];

                if (jeecs::graphic::PENDING_UNIFORM_LOCATION == *uniform_data.m_binding_place_addr)
                    continue;

                assert(uniform_data.m_type != jegl_shader::uniform_type::TEXTURE);

                jegl_set_uniform_value(
                    *uniform_data.m_binding_place_addr,
                    uniform_data.m_type,
                    &uniform_data.m_value);
            }
            jegl_draw_vertex(action.m_vertex);
        }
    }
}
