#define JE_IMPL
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_graphic_render_internal.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

// Render API (v2): backend-agnostic command recording.
//
// Recording happens on any thread and only touches CPU memory. Commit runs on
// the owning graphic thread, prepares resources lazily and dispatches a compact
// command stream to the backend render vtable. All state validation is done
// with asserts; invalid usage is a programming error, not a recoverable case.

using namespace jeecs::graphic;

namespace
{
    enum class jegl_cmd_op : uint32_t
    {
        BEGIN_PASS,
        CLEAR_COLOR,
        CLEAR_DEPTH,
        END_PASS,

        BIND_PIPELINE,
        BIND_VERTEX,
        BIND_UNIFORM_BUFFER,
        BIND_TEXTURE_2D,
        BIND_TEXTURE_3D,
        BIND_TEXTURE_ARRAY,
        BIND_TEXTURE_CUBE,
        BIND_SAMPLER,

        // 资源更新：提交期落实，数据由命令缓冲的 m_data_arena 持有
        UPDATE_VERTEX,
        UPDATE_UNIFORM_BUFFER,
        UPDATE_TEXTURE_2D,
        UPDATE_TEXTURE_3D,
        UPDATE_TEXTURE_ARRAY,
        UPDATE_TEXTURE_CUBE,

        DRAW_INDEXED,
        DRAW,
    };

    struct jegl_cmd
    {
        jegl_cmd_op m_op;
        uint32_t m_a, m_b, m_c, m_d;
        // UPDATE_TEXTURE_* 的额外参数（高度/层数/深度等）
        uint32_t m_e, m_g;
        // 更新命令的数据在命令缓冲 m_data_arena 中的偏移与长度
        uint32_t m_data_offset;
        uint32_t m_data_size;
        uint32_t m_uniform_offset;
        uint32_t m_uniform_size;
        uintptr_t m_ptr0;
        float m_f[4];
    };

    struct jegl_resource_entry
    {
        void* m_ptr;
        void (*m_release)(void*);
    };

    template<typename T>
    void _jegl_release_shared_resource(void* ptr)
    {
        T* resource = static_cast<T*>(ptr);

        if constexpr (std::is_same_v<T, je_gl_shader_t>)
            je_graphic_shader_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_texture_2d_t>)
            je_graphic_texture_2d_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_texture_3d_t>)
            je_graphic_texture_3d_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_texture_array_t>)
            je_graphic_texture_array_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_texture_cube_t>)
            je_graphic_texture_cube_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_texture_msaa_t>)
            je_graphic_texture_msaa_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_vertex_t>)
            je_graphic_vertex_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_frame_buffer_t>)
            je_graphic_framebuf_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_uniform_buffer_t>)
            je_graphic_uniformbuf_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_pipeline_t>)
            je_graphic_pipeline_close(resource);
        else if constexpr (std::is_same_v<T, je_gl_sampler_t>)
            je_graphic_sampler_close(resource);
        else
            static_assert(!sizeof(T*), "Unsupported graphic resource type.");
    }

    inline size_t _jegl_resource_hash(void* ptr)
    {
        return (reinterpret_cast<uintptr_t>(ptr) >> 4);
    }

    inline uint32_t _jegl_align_up(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    bool _jegl_topology_of_vertex(
        const je_gl_vertex_t* vertex, je_gl_topology_t* out_topology)
    {
        switch (vertex->m_type)
        {
        case JE_GL_VERTEX_TYPE_LINESTRIP:
            *out_topology = je_gl_topology_t::LINE_STRIP;
            return true;
        case JE_GL_VERTEX_TYPE_TRIANGLES:
            *out_topology = je_gl_topology_t::TRIANGLES;
            return true;
        case JE_GL_VERTEX_TYPE_TRIANGLESTRIP:
            *out_topology = je_gl_topology_t::TRIANGLE_STRIP;
            return true;
        default:
            return false;
        }
    }

    bool _jegl_vertex_layout_matches(
        const je_gl_vertex_t* vertex, const je_gl_shader_t* shader)
    {
        // A shader consumes a prefix of the vertex attributes. Models may
        // carry extra attributes (normal/tangent/...) that the shader ignores;
        // the pipeline input layout is built from the shader's vin, so only
        // that prefix has to match.
        if (shader->m_vertex_in_count > vertex->m_format_count)
            return false;

        for (size_t i = 0; i < shader->m_vertex_in_count; ++i)
        {
            const auto type = shader->m_vertex_in[i];
            size_t count = 0;
            je_gl_vertex_data_type_t base_type;

            switch (type)
            {
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT:
                count = 1;
                base_type = JE_GL_VERTEX_DATA_TYPE_FLOAT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT2:
                count = 2;
                base_type = JE_GL_VERTEX_DATA_TYPE_FLOAT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT3:
                count = 3;
                base_type = JE_GL_VERTEX_DATA_TYPE_FLOAT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_FLOAT4:
                count = 4;
                base_type = JE_GL_VERTEX_DATA_TYPE_FLOAT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT:
                count = 1;
                base_type = JE_GL_VERTEX_DATA_TYPE_INT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT2:
                count = 2;
                base_type = JE_GL_VERTEX_DATA_TYPE_INT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT3:
                count = 3;
                base_type = JE_GL_VERTEX_DATA_TYPE_INT32;
                break;
            case JE_GL_SHADER_UNIFORM_TYPE_INT4:
                count = 4;
                base_type = JE_GL_VERTEX_DATA_TYPE_INT32;
                break;
            default:
                return false;
            }

            if (vertex->m_formats[i].m_count != count
                || vertex->m_formats[i].m_type != base_type)
                return false;
        }
        return true;
    }
}

struct je_gl_command_buffer_t
{
    JECS_DISABLE_MOVE_AND_COPY(je_gl_command_buffer_t);

    struct texture_binding
    {
        uint32_t m_pass;
        je_gl_texture_kind_t m_kind;
        void* m_texture;
    };

    je_gl_context_t* m_context;

    std::vector<jegl_cmd> m_commands;
    std::vector<uint8_t> m_uniform_arena;
    // 资源更新命令的数据（顶点/UBO 内容、纹理区段像素），按 16 字节对齐追加
    std::vector<uint8_t> m_data_arena;

    // Open-addressing set of resources that must stay alive until reset/close.
    std::vector<jegl_resource_entry> m_resources;
    size_t m_resource_count;

    bool m_in_pass;
    bool m_pass_has_draw;

    bool m_has_pipeline;
    bool m_has_vertex;
    je_gl_pipeline_t* m_pipeline;
    je_gl_vertex_t* m_vertex;

    je_gl_frame_buffer_t* m_target;
    uint32_t m_target_attachment_count;

    std::vector<uint8_t> m_uniform_staging;
    uint32_t m_uniform_staging_size;
    bool m_uniform_staging_dirty;
    bool m_has_last_uniform_block;
    uint32_t m_last_uniform_offset;
    uint32_t m_last_uniform_size;

    texture_binding m_texture_bindings[32];
    size_t m_texture_binding_count;

    explicit je_gl_command_buffer_t(je_gl_context_t* context)
        : m_context(context)
        , m_resource_count(0)
        , m_in_pass(false)
        , m_pass_has_draw(false)
        , m_has_pipeline(false)
        , m_has_vertex(false)
        , m_pipeline(nullptr)
        , m_vertex(nullptr)
        , m_target(nullptr)
        , m_target_attachment_count(1)
        , m_uniform_staging_size(0)
        , m_uniform_staging_dirty(false)
        , m_has_last_uniform_block(false)
        , m_last_uniform_offset(0)
        , m_last_uniform_size(0)
        , m_texture_binding_count(0)
    {
        assert(context != nullptr);
    }

    ~je_gl_command_buffer_t()
    {
        release_all_resources();
    }

    void ensure_resource_capacity(size_t needed)
    {
        size_t capacity = m_resources.size();
        if (capacity >= needed)
            return;

        if (capacity == 0)
            capacity = 64;
        while (capacity < needed)
            capacity *= 2;

        std::vector<jegl_resource_entry> old =
            std::move(m_resources);
        m_resources.assign(capacity, jegl_resource_entry{ nullptr, nullptr });
        m_resource_count = 0;

        for (auto& entry : old)
        {
            if (entry.m_ptr != nullptr)
                insert_raw(entry);
        }
    }

    size_t find_slot(void* ptr) const
    {
        assert(!m_resources.empty());
        const size_t mask = m_resources.size() - 1;
        size_t index = _jegl_resource_hash(ptr) & mask;

        while (m_resources[index].m_ptr != nullptr)
        {
            if (m_resources[index].m_ptr == ptr)
                return index;
            index = (index + 1) & mask;
        }
        return index;
    }

    bool contains(void* ptr) const
    {
        if (m_resources.empty())
            return false;
        return m_resources[find_slot(ptr)].m_ptr == ptr;
    }

    void insert_raw(const jegl_resource_entry& entry)
    {
        const size_t mask = m_resources.size() - 1;
        size_t index = _jegl_resource_hash(entry.m_ptr) & mask;

        while (m_resources[index].m_ptr != nullptr)
            index = (index + 1) & mask;

        m_resources[index] = entry;
        ++m_resource_count;
    }

    void retain_raw(void* ptr, void (*release)(void*))
    {
        if (m_resources.empty() || (m_resource_count + 1) * 10 >= m_resources.size() * 7)
            ensure_resource_capacity(m_resource_count + 1);

        if (contains(ptr))
            return;

        insert_raw(jegl_resource_entry{ ptr, release });
    }

    template<typename T>
    void retain(T* resource)
    {
        if (resource == nullptr)
            return;
        if (contains(resource))
            return;

        jegl_share_resource(resource);
        retain_raw(resource, &_jegl_release_shared_resource<T>);
    }

    void release_all_resources()
    {
        for (auto& entry : m_resources)
        {
            if (entry.m_ptr != nullptr)
            {
                entry.m_release(entry.m_ptr);
                entry.m_ptr = nullptr;
                entry.m_release = nullptr;
            }
        }
        m_resource_count = 0;
    }

    void reset()
    {
        // 帧可能因图形实现请求 SKIP/STOP（窗口最小化/关闭）而从未提交，
        // 此时缓冲区仍停留在 pass 内。录制只发生在本侧 CPU 内存，尚未提交
        // 到后端，直接丢弃即可，复用前不应因此报错。
        m_in_pass = false;

        release_all_resources();
        m_commands.clear();
        m_uniform_arena.clear();
        m_data_arena.clear();

        m_pass_has_draw = false;
        m_has_pipeline = false;
        m_has_vertex = false;
        m_pipeline = nullptr;
        m_vertex = nullptr;
        m_target = nullptr;
        m_target_attachment_count = 1;

        m_uniform_staging.clear();
        m_uniform_staging_size = 0;
        m_uniform_staging_dirty = false;
        m_has_last_uniform_block = false;
        m_last_uniform_offset = 0;
        m_last_uniform_size = 0;

        m_texture_binding_count = 0;
    }

    void push_command(const jegl_cmd& command)
    {
        m_commands.push_back(command);
    }

    // 追加一段资源更新数据，返回其在 m_data_arena 中的偏移
    uint32_t append_data(const void* data, size_t size)
    {
        assert(data != nullptr);
        assert(size != 0);

        const size_t aligned = (m_data_arena.size() + 15) & ~size_t(15);
        assert(aligned + size <= 0xFFFFFFFFu);

        m_data_arena.resize(aligned + size);
        memcpy(m_data_arena.data() + aligned, data, size);

        return (uint32_t)aligned;
    }

    void reset_uniform_staging(je_gl_pipeline_t* pipeline)
    {
        const auto* shader = pipeline->m_shader;
        const uint32_t size = shader->m_uniform_block_size;

        m_uniform_staging.resize(size);
        m_uniform_staging_size = size;

        if (size != 0)
        {
            uint8_t* staging = m_uniform_staging.data();
            memset(staging, 0, size);

            for (auto* uniform = shader->m_custom_uniforms;
                uniform != nullptr;
                uniform = uniform->m_next)
            {
                if (uniform->m_size == 0)
                    continue;

                if (uniform->m_uniform_type == JE_GL_SHADER_UNIFORM_TYPE_FLOAT3X3)
                {
                    for (size_t row = 0; row < 3; ++row)
                    {
                        memcpy(
                            staging + uniform->m_offset + row * 16,
                            uniform->m_value.m_float3x3[row],
                            sizeof(float) * 3);
                    }
                }
                else
                {
                    memcpy(
                        staging + uniform->m_offset,
                        &uniform->m_value,
                        uniform->m_size);
                }
            }
        }

        m_uniform_staging_dirty = true;
        m_has_last_uniform_block = false;
    }

    void validate_draw_state() const
    {
        assert(m_has_pipeline);
        assert(m_has_vertex);

        const auto* shader = m_pipeline->m_shader;
        je_gl_topology_t topology;
        assert(_jegl_topology_of_vertex(m_vertex, &topology));
        assert(topology == m_pipeline->m_state.m_topology);
        assert(_jegl_vertex_layout_matches(m_vertex, shader));

        for (size_t i = 0; i < m_texture_binding_count; ++i)
        {
            const auto& binding = m_texture_bindings[i];
            for (size_t j = 0; j < shader->m_texture_count; ++j)
            {
                if (shader->m_textures[j].m_pass == binding.m_pass
                    && shader->m_textures[j].m_kind != binding.m_kind)
                {
                    assert(!"Texture kind does not match the shader declaration.");
                }
            }
        }
    }

    void emit_draw(jegl_cmd_op op, uint32_t a, uint32_t b, uint32_t c)
    {
        validate_draw_state();

        uint32_t uniform_offset = 0;
        uint32_t uniform_size = m_uniform_staging_size;

        if (uniform_size != 0)
        {
            if (!m_uniform_staging_dirty && m_has_last_uniform_block)
            {
                uniform_offset = m_last_uniform_offset;
                uniform_size = m_last_uniform_size;
            }
            else
            {
                const size_t aligned = (m_uniform_arena.size() + 15) & ~size_t(15);
                assert(aligned + uniform_size <= 0xFFFFFFFFu);

                m_uniform_arena.resize(aligned + uniform_size);
                memcpy(
                    m_uniform_arena.data() + aligned,
                    m_uniform_staging.data(),
                    uniform_size);

                uniform_offset = (uint32_t)aligned;
                m_last_uniform_offset = uniform_offset;
                m_last_uniform_size = uniform_size;
                m_has_last_uniform_block = true;
                m_uniform_staging_dirty = false;
            }
        }

        jegl_cmd command{};
        command.m_op = op;
        command.m_a = a;
        command.m_b = b;
        command.m_c = c;
        command.m_uniform_offset = uniform_offset;
        command.m_uniform_size = uniform_size;
        push_command(command);

        m_pass_has_draw = true;
    }
};

/////////////////////////////////////////////////////////////////////////////
// Pipeline / uniform reflection
/////////////////////////////////////////////////////////////////////////////

je_gl_pipeline_state_t je_graphic_pipeline_state_from_shader(je_gl_shader_t* shader)
{
    assert(shader != nullptr);

    je_gl_pipeline_state_t state{};
    state.m_topology = je_gl_topology_t::TRIANGLES;

    state.m_depth_test = true;
    state.m_depth_func = shader->m_depth_test;
    state.m_depth_write =
        shader->m_depth_mask == je_gl_shader_depth_mask_method_t::ENABLE;

    state.m_blend =
        shader->m_blend_equation != je_gl_shader_blend_equation_t::DISABLED;
    state.m_blend_equation = shader->m_blend_equation;
    state.m_blend_src_color = shader->m_blend_src_mode;
    state.m_blend_dst_color = shader->m_blend_dst_mode;
    state.m_blend_src_alpha = shader->m_blend_src_mode;
    state.m_blend_dst_alpha = shader->m_blend_dst_mode;

    state.m_cull = shader->m_cull_mode;
    state.m_front_face = je_gl_front_face_t::COUNTER_CLOCKWISE;
    state.m_color_write_mask = 0x0F;

    if (state.m_depth_func == je_gl_shader_depth_test_method_t::INVALID)
    {
        state.m_depth_test = false;
        state.m_depth_func = je_gl_shader_depth_test_method_t::LESS;
    }
    if (state.m_blend_equation == je_gl_shader_blend_equation_t::INVALID)
    {
        state.m_blend = false;
        state.m_blend_equation = je_gl_shader_blend_equation_t::DISABLED;
    }
    if (state.m_blend_src_color == je_gl_shader_blend_method_t::INVALID)
        state.m_blend_src_color = je_gl_shader_blend_method_t::ONE;
    if (state.m_blend_dst_color == je_gl_shader_blend_method_t::INVALID)
        state.m_blend_dst_color = je_gl_shader_blend_method_t::ZERO;
    if (state.m_blend_src_alpha == je_gl_shader_blend_method_t::INVALID)
        state.m_blend_src_alpha = je_gl_shader_blend_method_t::ONE;
    if (state.m_blend_dst_alpha == je_gl_shader_blend_method_t::INVALID)
        state.m_blend_dst_alpha = je_gl_shader_blend_method_t::ZERO;
    if (state.m_cull == je_gl_shader_cull_mode_t::INVALID)
        state.m_cull = je_gl_shader_cull_mode_t::NONE;

    return state;
}

je_gl_uniform_slot_t je_graphic_pipeline_uniform_slot(
    je_gl_pipeline_t* pipeline, const char* name)
{
    assert(pipeline != nullptr);
    assert(name != nullptr);

    for (auto* uniform = pipeline->m_shader->m_custom_uniforms;
        uniform != nullptr;
        uniform = uniform->m_next)
    {
        if (uniform->m_uniform_type != JE_GL_SHADER_UNIFORM_TYPE_TEXTURE
            && std::strcmp(uniform->m_name, name) == 0)
            return je_gl_uniform_slot_t{ uniform->m_offset, uniform->m_size };
    }
    return je_gl_uniform_slot_t{ JE_GL_UNIFORM_SLOT_NONE, 0 };
}

const je_gl_builtin_uniform_slots_t* je_graphic_pipeline_builtin_uniform_slots(
    je_gl_pipeline_t* pipeline)
{
    assert(pipeline != nullptr);
    return &pipeline->m_builtin_slots;
}

/////////////////////////////////////////////////////////////////////////////
// Command buffer
/////////////////////////////////////////////////////////////////////////////

je_gl_command_buffer_t* je_graphic_cmd_create(je_gl_context_t* context)
{
    assert(context != nullptr);
    return new je_gl_command_buffer_t(context);
}

void je_graphic_cmd_close(je_gl_command_buffer_t* cmd)
{
    assert(cmd != nullptr);
    delete cmd;
}

void je_graphic_cmd_reset(je_gl_command_buffer_t* cmd)
{
    assert(cmd != nullptr);
    cmd->reset();
}

je_gl_context_t* je_graphic_cmd_get_context(je_gl_command_buffer_t* cmd)
{
    assert(cmd != nullptr);
    return cmd->m_context;
}

void je_graphic_cmd_begin_pass(
    je_gl_command_buffer_t* cmd,
    /* OPTIONAL: nullptr means screen */ je_gl_frame_buffer_t* target,
    const je_gl_viewport_t* viewport)
{
    assert(cmd != nullptr);
    assert(viewport != nullptr);
    assert(viewport->width > 0);
    assert(viewport->height > 0);
    assert(!cmd->m_in_pass);

    cmd->retain(target);

    cmd->m_target = target;
    cmd->m_target_attachment_count =
        target != nullptr ? (uint32_t)target->m_attachment_count : 1;

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BEGIN_PASS;
    command.m_a = (uint32_t)viewport->x;
    command.m_b = (uint32_t)viewport->y;
    command.m_c = viewport->width;
    command.m_d = viewport->height;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(target);

    cmd->m_in_pass = true;
    cmd->m_pass_has_draw = false;
    cmd->m_has_pipeline = false;
    cmd->m_has_vertex = false;
    cmd->m_pipeline = nullptr;
    cmd->m_vertex = nullptr;
    cmd->m_texture_binding_count = 0;
    cmd->m_uniform_staging_dirty = false;
    cmd->m_has_last_uniform_block = false;

    cmd->push_command(command);
}

void je_graphic_cmd_clear_color(
    je_gl_command_buffer_t* cmd, uint32_t attachment, const float rgba[4])
{
    assert(cmd != nullptr);
    assert(rgba != nullptr);
    assert(cmd->m_in_pass);
    assert(!cmd->m_pass_has_draw);
    assert(attachment < cmd->m_target_attachment_count);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::CLEAR_COLOR;
    command.m_a = attachment;
    command.m_f[0] = rgba[0];
    command.m_f[1] = rgba[1];
    command.m_f[2] = rgba[2];
    command.m_f[3] = rgba[3];
    cmd->push_command(command);
}

void je_graphic_cmd_clear_depth(je_gl_command_buffer_t* cmd, float depth)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    assert(!cmd->m_pass_has_draw);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::CLEAR_DEPTH;
    command.m_f[0] = depth;
    cmd->push_command(command);
}

void je_graphic_cmd_end_pass(je_gl_command_buffer_t* cmd)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::END_PASS;
    cmd->push_command(command);

    cmd->m_in_pass = false;
    cmd->m_target = nullptr;
}

void je_graphic_cmd_bind_pipeline(
    je_gl_command_buffer_t* cmd, je_gl_pipeline_t* pipeline)
{
    assert(cmd != nullptr);
    assert(pipeline != nullptr);
    assert(cmd->m_in_pass);
    assert(pipeline->m_context == cmd->m_context);

    cmd->retain(pipeline);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_PIPELINE;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(pipeline);
    cmd->push_command(command);

    cmd->m_has_pipeline = true;
    cmd->m_pipeline = pipeline;
    cmd->reset_uniform_staging(pipeline);
}

void je_graphic_cmd_bind_vertex(
    je_gl_command_buffer_t* cmd, je_gl_vertex_t* vertex)
{
    assert(cmd != nullptr);
    assert(vertex != nullptr);
    assert(cmd->m_in_pass);

    cmd->retain(vertex);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_VERTEX;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(vertex);
    cmd->push_command(command);

    cmd->m_has_vertex = true;
    cmd->m_vertex = vertex;
}

void je_graphic_cmd_bind_uniform_buffer(
    je_gl_command_buffer_t* cmd, je_gl_uniform_buffer_t* buffer)
{
    assert(cmd != nullptr);
    assert(buffer != nullptr);
    assert(cmd->m_in_pass);

    cmd->retain(buffer);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_UNIFORM_BUFFER;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(buffer);
    cmd->push_command(command);
}

template<typename TexT>
static void _jegl_cmd_bind_texture_impl(
    je_gl_command_buffer_t* cmd,
    jegl_cmd_op op,
    je_gl_texture_kind_t kind,
    uint32_t pass,
    TexT* texture)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(cmd->m_in_pass);
    assert(pass < 32);

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = op;
    command.m_a = pass;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    cmd->push_command(command);

    bool updated = false;
    for (size_t i = 0; i < cmd->m_texture_binding_count; ++i)
    {
        if (cmd->m_texture_bindings[i].m_pass == pass)
        {
            cmd->m_texture_bindings[i].m_kind = kind;
            cmd->m_texture_bindings[i].m_texture = texture;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        auto& binding = cmd->m_texture_bindings[cmd->m_texture_binding_count++];
        binding.m_pass = pass;
        binding.m_kind = kind;
        binding.m_texture = texture;
    }
}

void je_graphic_cmd_bind_texture_2d(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_2d_t* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_2D, je_gl_texture_kind_t::TEXTURE_2D, pass, texture);
}

void je_graphic_cmd_bind_texture_3d(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_3d_t* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_3D, je_gl_texture_kind_t::TEXTURE_3D, pass, texture);
}

void je_graphic_cmd_bind_texture_array(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_array_t* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_ARRAY, je_gl_texture_kind_t::TEXTURE_ARRAY, pass, texture);
}

void je_graphic_cmd_bind_texture_cube(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_cube_t* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_CUBE, je_gl_texture_kind_t::TEXTURE_CUBE, pass, texture);
}

void je_graphic_cmd_bind_sampler(
    je_gl_command_buffer_t* cmd, uint32_t sampler_id, je_gl_sampler_t* sampler)
{
    assert(cmd != nullptr);
    assert(sampler != nullptr);
    assert(cmd->m_in_pass);
    assert(cmd->m_has_pipeline);
    assert(sampler_id < 16);

    cmd->retain(sampler);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_SAMPLER;
    command.m_a = sampler_id;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(sampler);
    cmd->push_command(command);
}

static size_t _jegl_upload_byte_size(
    const je_gl_texture_common_t* texture, size_t pixel_count)
{
    assert(0 == (texture->m_format & JE_GL_TEXTURE_FORMAT_NON_PIXEL_FORMAT_MASK));

    const size_t pixel_size =
        (size_t)(texture->m_format & JE_GL_TEXTURE_FORMAT_COLOR_DEPTH_MASK);
    assert(pixel_size != 0);

    return pixel_count * pixel_size;
}

void je_graphic_cmd_update_vertex(
    je_gl_command_buffer_t* cmd,
    je_gl_vertex_t* vertex,
    /* OPTIONAL: nullptr 时只调整激活索引数量 */ const void* vertices,
    size_t vertex_length,
    size_t active_index_count)
{
    assert(cmd != nullptr);
    assert(vertex != nullptr);
    assert(vertex_length <= vertex->m_vertex_length);
    assert(active_index_count <= vertex->m_index_capacity);
    if (vertex_length != 0)
        assert(vertices != nullptr);
    else
        assert(vertices == nullptr);

    // 激活数量保持至少 1：0 长度的绘制调用在部分后端上非法；1 个索引的
    // TRIANGLES 绘制不产生任何图元，视觉上等价于不绘制
    if (active_index_count == 0)
        active_index_count = 1;

    cmd->retain(vertex);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_VERTEX;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(vertex);
    command.m_a = (uint32_t)active_index_count;

    if (vertex_length != 0)
    {
        command.m_data_offset = cmd->append_data(vertices, vertex_length);
        command.m_data_size = (uint32_t)vertex_length;
    }

    cmd->push_command(command);

    // 绘制数量属于 CPU 侧状态，仅录制线程读取
    vertex->m_index_count = active_index_count;
}

void je_graphic_cmd_update_uniform_buffer(
    je_gl_command_buffer_t* cmd,
    je_gl_uniform_buffer_t* buffer,
    const void* data,
    size_t size)
{
    assert(cmd != nullptr);
    assert(buffer != nullptr);
    assert(data != nullptr);
    assert(size != 0);
    assert(size == buffer->m_buffer_size);

    cmd->retain(buffer);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_UNIFORM_BUFFER;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(buffer);
    command.m_data_offset = cmd->append_data(data, size);
    command.m_data_size = (uint32_t)size;
    cmd->push_command(command);
}

void je_graphic_cmd_upload_texture_2d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_2d_t* texture,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(pixels != nullptr);
    assert(width != 0 && height != 0);
    assert(x + width <= texture->m_width);
    assert(y + height <= texture->m_height);

    const size_t byte_size = _jegl_upload_byte_size(texture, width * height);

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_TEXTURE_2D;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    command.m_a = (uint32_t)x;
    command.m_b = (uint32_t)y;
    command.m_c = (uint32_t)width;
    command.m_d = (uint32_t)height;
    command.m_data_offset = cmd->append_data(pixels, byte_size);
    command.m_data_size = (uint32_t)byte_size;
    cmd->push_command(command);
}

void je_graphic_cmd_upload_texture_3d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_3d_t* texture,
    size_t x, size_t y, size_t z,
    size_t width, size_t height, size_t depth,
    const je_gl_texture_pixel_data_t* pixels)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(pixels != nullptr);
    assert(width != 0 && height != 0 && depth != 0);
    assert(x + width <= texture->m_width);
    assert(y + height <= texture->m_height);
    assert(z + depth <= texture->m_depth);

    const size_t byte_size =
        _jegl_upload_byte_size(texture, width * height * depth);

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_TEXTURE_3D;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    command.m_a = (uint32_t)x;
    command.m_b = (uint32_t)y;
    command.m_c = (uint32_t)z;
    command.m_d = (uint32_t)width;
    command.m_e = (uint32_t)height;
    command.m_g = (uint32_t)depth;
    command.m_data_offset = cmd->append_data(pixels, byte_size);
    command.m_data_size = (uint32_t)byte_size;
    cmd->push_command(command);
}

void je_graphic_cmd_upload_texture_array(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_array_t* texture,
    size_t x, size_t y, size_t layer,
    size_t width, size_t height, size_t layers,
    const je_gl_texture_pixel_data_t* pixels)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(pixels != nullptr);
    assert(width != 0 && height != 0 && layers != 0);
    assert(x + width <= texture->m_width);
    assert(y + height <= texture->m_height);
    assert(layer + layers <= texture->m_layers);

    const size_t byte_size =
        _jegl_upload_byte_size(texture, width * height * layers);

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_TEXTURE_ARRAY;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    command.m_a = (uint32_t)x;
    command.m_b = (uint32_t)y;
    command.m_c = (uint32_t)layer;
    command.m_d = (uint32_t)width;
    command.m_e = (uint32_t)height;
    command.m_g = (uint32_t)layers;
    command.m_data_offset = cmd->append_data(pixels, byte_size);
    command.m_data_size = (uint32_t)byte_size;
    cmd->push_command(command);
}

void je_graphic_cmd_upload_texture_cube(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_cube_t* texture,
    size_t face,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(pixels != nullptr);
    assert(face < 6);
    assert(width != 0 && height != 0);
    assert(x + width <= texture->m_width);
    assert(y + height <= texture->m_height);

    const size_t byte_size = _jegl_upload_byte_size(texture, width * height);

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::UPDATE_TEXTURE_CUBE;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    command.m_a = (uint32_t)face;
    command.m_b = (uint32_t)x;
    command.m_c = (uint32_t)y;
    command.m_d = (uint32_t)width;
    command.m_e = (uint32_t)height;
    command.m_data_offset = cmd->append_data(pixels, byte_size);
    command.m_data_size = (uint32_t)byte_size;
    cmd->push_command(command);
}

void je_graphic_cmd_set_uniform(
    je_gl_command_buffer_t* cmd,
    je_gl_uniform_slot_t slot,
    const void* data,
    uint32_t size)
{
    assert(cmd != nullptr);
    assert(data != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JE_GL_UNIFORM_SLOT_NONE);
    assert(slot.m_size > 0);
    assert(size == slot.m_size);
    assert(slot.m_offset + size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        data,
        size);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void je_graphic_cmd_set_uniform_float(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float value)
{
    assert(slot.m_size == sizeof(float));
    je_graphic_cmd_set_uniform(cmd, slot, &value, sizeof(value));
}

void je_graphic_cmd_set_uniform_float2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y)
{
    const float value[2] = { x, y };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_float3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y, float z)
{
    const float value[3] = { x, y, z };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_float4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y, float z, float w)
{
    const float value[4] = { x, y, z, w };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_int(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int value)
{
    assert(slot.m_size == sizeof(int));
    je_graphic_cmd_set_uniform(cmd, slot, &value, sizeof(value));
}

void je_graphic_cmd_set_uniform_int2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y)
{
    const int value[2] = { x, y };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_int3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y, int z)
{
    const int value[3] = { x, y, z };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_int4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, int x, int y, int z, int w)
{
    const int value[4] = { x, y, z, w };
    assert(slot.m_size == sizeof(value));
    je_graphic_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void je_graphic_cmd_set_uniform_float2x2(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[2])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JE_GL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == sizeof(float) * 2 * 2);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        mat,
        sizeof(float) * 2 * 2);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void je_graphic_cmd_set_uniform_float3x3(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[3])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JE_GL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == 48);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    uint8_t* staging = cmd->m_uniform_staging.data() + slot.m_offset;
    for (size_t row = 0; row < 3; ++row)
    {
        memcpy(staging + row * 16, mat[row], sizeof(float) * 3);
        memset(staging + row * 16 + 12, 0, sizeof(float));
    }

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void je_graphic_cmd_set_uniform_float4x4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[4])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JE_GL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == sizeof(float) * 4 * 4);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        mat,
        sizeof(float) * 4 * 4);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void je_graphic_cmd_draw_indexed(
    je_gl_command_buffer_t* cmd,
    uint32_t index_count, uint32_t first_index, uint32_t instance_count)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    assert(index_count > 0);
    assert(instance_count > 0);

    cmd->emit_draw(
        jegl_cmd_op::DRAW_INDEXED,
        index_count,
        first_index,
        instance_count);
}

void je_graphic_cmd_draw(
    je_gl_command_buffer_t* cmd,
    uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    assert(vertex_count > 0);
    assert(instance_count > 0);

    cmd->emit_draw(
        jegl_cmd_op::DRAW,
        vertex_count,
        first_vertex,
        instance_count);
}

void je_graphic_cmd_commit(je_gl_command_buffer_t* cmd)
{
    assert(cmd != nullptr);
    assert(cmd->m_context != nullptr);
    assert(jeecs::graphic::_current_graphic_thread == cmd->m_context);

    // 没有任何命令的命令缓冲不触发后端动作（避免提交空帧）
    if (cmd->m_commands.empty())
        return;

    // A pass begun by je_graphic_branch_new_command_buffer may still be open; close
    // it here so callers can follow the "record, commit" flow.
    if (cmd->m_in_pass)
    {
        jegl_cmd command{};
        command.m_op = jegl_cmd_op::END_PASS;
        cmd->push_command(command);
        cmd->m_in_pass = false;
        cmd->m_target = nullptr;
    }

    const auto* render = cmd->m_context->m_apis;

    je_gl_context_t* const graphic_context = cmd->m_context;
    const auto impl_context = graphic_context->m_graphic_impl_context;
    render->commit_begin(impl_context);

    je_gl_pipeline_t* last_pipeline = nullptr;
    je_gl_vertex_t* last_vertex = nullptr;
    je_gl_uniform_buffer_t* last_ubo = nullptr;
    void* last_texture[32] = {};
    uint32_t last_texture_kind[32] = {};
    je_gl_sampler_t* last_sampler[16] = {};

    for (const auto& command : cmd->m_commands)
    {
        switch (command.m_op)
        {
        case jegl_cmd_op::BEGIN_PASS:
        {
            auto* target =
                reinterpret_cast<je_gl_frame_buffer_t*>(command.m_ptr0);

            if (target != nullptr)
            {
                const bool ready = prepare_framebuffer_resource(target);
                assert(ready);
                (void)ready;
            }

            const je_gl_viewport_t viewport{
                (int32_t)command.m_a,
                (int32_t)command.m_b,
                command.m_c,
                command.m_d,
            };
            render->begin_pass(impl_context, target, &viewport);

            last_pipeline = nullptr;
            last_vertex = nullptr;
            last_ubo = nullptr;
            memset(last_texture, 0, sizeof(last_texture));
            memset(last_texture_kind, 0, sizeof(last_texture_kind));
            memset(last_sampler, 0, sizeof(last_sampler));
            break;
        }
        case jegl_cmd_op::CLEAR_COLOR:
            render->clear_color(impl_context, command.m_a, command.m_f);
            break;
        case jegl_cmd_op::CLEAR_DEPTH:
            render->clear_depth(impl_context, command.m_f[0]);
            break;
        case jegl_cmd_op::END_PASS:
            render->end_pass(impl_context);
            break;

        case jegl_cmd_op::BIND_PIPELINE:
        {
            auto* pipeline =
                reinterpret_cast<je_gl_pipeline_t*>(command.m_ptr0);

            const auto prepare =
                prepare_shader_resource(pipeline->m_shader);
            assert(prepare != ResourcePrepareResult::INVALID);

            const auto pipeline_prepare =
                prepare_pipeline_resource(pipeline);
            assert(pipeline_prepare != ResourcePrepareResult::INVALID);

            if (prepare != ResourcePrepareResult::READY
                || pipeline_prepare != ResourcePrepareResult::READY)
            {
                render->pipeline_init(impl_context, pipeline);
                pipeline->m_backend_initialized = true;
            }

            if (last_pipeline != pipeline)
            {
                render->pipeline_bind(impl_context, pipeline);
                last_pipeline = pipeline;
            }
            break;
        }
        case jegl_cmd_op::BIND_VERTEX:
        {
            auto* vertex =
                reinterpret_cast<je_gl_vertex_t*>(command.m_ptr0);

            const bool ready = prepare_vertex_resource(vertex);
            assert(ready);
            (void)ready;

            if (last_vertex != vertex)
            {
                render->bind_vertex(impl_context, vertex);
                last_vertex = vertex;
            }
            break;
        }
        case jegl_cmd_op::BIND_UNIFORM_BUFFER:
        {
            auto* buffer =
                reinterpret_cast<je_gl_uniform_buffer_t*>(command.m_ptr0);

            const bool ready = prepare_uniform_buffer_resource(buffer);
            assert(ready);
            (void)ready;

            if (last_ubo != buffer)
            {
                render->bind_uniform_buffer(impl_context, buffer);
                last_ubo = buffer;
            }
            break;
        }
        case jegl_cmd_op::BIND_TEXTURE_2D:
        case jegl_cmd_op::BIND_TEXTURE_3D:
        case jegl_cmd_op::BIND_TEXTURE_ARRAY:
        case jegl_cmd_op::BIND_TEXTURE_CUBE:
        {
            const uint32_t pass = command.m_a;
            assert(pass < 32);

            const uint32_t kind =
                command.m_op == jegl_cmd_op::BIND_TEXTURE_2D ? 0u
                : command.m_op == jegl_cmd_op::BIND_TEXTURE_3D ? 1u
                : command.m_op == jegl_cmd_op::BIND_TEXTURE_ARRAY ? 2u
                : 3u;

            void* texture = reinterpret_cast<void*>(command.m_ptr0);
            bool ready = false;

            switch (command.m_op)
            {
            case jegl_cmd_op::BIND_TEXTURE_2D:
                ready = prepare_texture_2d_resource(
                    static_cast<je_gl_texture_2d_t*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_3D:
                ready = prepare_texture_3d_resource(
                    static_cast<je_gl_texture_3d_t*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_ARRAY:
                ready = prepare_texture_array_resource(
                    static_cast<je_gl_texture_array_t*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_CUBE:
                ready = prepare_texture_cube_resource(
                    static_cast<je_gl_texture_cube_t*>(texture));
                break;
            default:
                break;
            }
            assert(ready);
            (void)ready;

            if (last_texture[pass] != texture
                || last_texture_kind[pass] != kind)
            {
                switch (command.m_op)
                {
                case jegl_cmd_op::BIND_TEXTURE_2D:
                    render->bind_texture_2d(
                        impl_context, pass, static_cast<je_gl_texture_2d_t*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_3D:
                    render->bind_texture_3d(
                        impl_context, pass, static_cast<je_gl_texture_3d_t*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_ARRAY:
                    render->bind_texture_array(
                        impl_context, pass, static_cast<je_gl_texture_array_t*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_CUBE:
                    render->bind_texture_cube(
                        impl_context, pass, static_cast<je_gl_texture_cube_t*>(texture));
                    break;
                default:
                    break;
                }

                last_texture[pass] = texture;
                last_texture_kind[pass] = kind;
            }
            break;
        }
        case jegl_cmd_op::BIND_SAMPLER:
        {
            const uint32_t sampler_id = command.m_a;
            auto* sampler =
                reinterpret_cast<je_gl_sampler_t*>(command.m_ptr0);

            const auto sampler_prepare =
                prepare_sampler_resource(sampler);
            assert(sampler_prepare != ResourcePrepareResult::INVALID);

            if (sampler_prepare != ResourcePrepareResult::READY)
            {
                render->sampler_init(impl_context, sampler);
                sampler->m_backend_initialized = true;
            }

            if (last_sampler[sampler_id] != sampler)
            {
                render->sampler_bind(impl_context, sampler_id, sampler);
                last_sampler[sampler_id] = sampler;
            }
            break;
        }

        case jegl_cmd_op::UPDATE_VERTEX:
        {
            auto* vertex =
                reinterpret_cast<je_gl_vertex_t*>(command.m_ptr0);

            const bool ready = prepare_vertex_resource(vertex);
            assert(ready);
            (void)ready;

            const void* data = command.m_data_size != 0
                ? cmd->m_data_arena.data() + command.m_data_offset
                : nullptr;

            render->vertex_update(
                impl_context, vertex, data, command.m_data_size);
            break;
        }
        case jegl_cmd_op::UPDATE_UNIFORM_BUFFER:
        {
            auto* buffer =
                reinterpret_cast<je_gl_uniform_buffer_t*>(command.m_ptr0);

            const bool ready = prepare_uniform_buffer_resource(buffer);
            assert(ready);
            (void)ready;

            render->ubuffer_update(
                impl_context, buffer,
                cmd->m_data_arena.data() + command.m_data_offset,
                command.m_data_size);
            break;
        }
        case jegl_cmd_op::UPDATE_TEXTURE_2D:
        {
            auto* texture =
                reinterpret_cast<je_gl_texture_2d_t*>(command.m_ptr0);

            const bool ready = prepare_texture_2d_resource(texture);
            assert(ready);
            (void)ready;

            render->texture_2d_upload(
                impl_context, texture,
                command.m_a, command.m_b, command.m_c, command.m_d,
                cmd->m_data_arena.data() + command.m_data_offset);
            break;
        }
        case jegl_cmd_op::UPDATE_TEXTURE_3D:
        {
            auto* texture =
                reinterpret_cast<je_gl_texture_3d_t*>(command.m_ptr0);

            const bool ready = prepare_texture_3d_resource(texture);
            assert(ready);
            (void)ready;

            render->texture_3d_upload(
                impl_context, texture,
                command.m_a, command.m_b, command.m_c,
                command.m_d, command.m_e, command.m_g,
                cmd->m_data_arena.data() + command.m_data_offset);
            break;
        }
        case jegl_cmd_op::UPDATE_TEXTURE_ARRAY:
        {
            auto* texture =
                reinterpret_cast<je_gl_texture_array_t*>(command.m_ptr0);

            const bool ready = prepare_texture_array_resource(texture);
            assert(ready);
            (void)ready;

            render->texture_array_upload(
                impl_context, texture,
                command.m_a, command.m_b, command.m_c,
                command.m_d, command.m_e, command.m_g,
                cmd->m_data_arena.data() + command.m_data_offset);
            break;
        }
        case jegl_cmd_op::UPDATE_TEXTURE_CUBE:
        {
            auto* texture =
                reinterpret_cast<je_gl_texture_cube_t*>(command.m_ptr0);

            const bool ready = prepare_texture_cube_resource(texture);
            assert(ready);
            (void)ready;

            render->texture_cube_upload(
                impl_context, texture,
                command.m_a,
                command.m_b, command.m_c, command.m_d, command.m_e,
                cmd->m_data_arena.data() + command.m_data_offset);
            break;
        }

        case jegl_cmd_op::DRAW_INDEXED:
        {
            const void* uniform_data =
                command.m_uniform_size != 0
                ? cmd->m_uniform_arena.data() + command.m_uniform_offset
                : nullptr;

            render->draw_indexed(
                impl_context,
                command.m_a,
                command.m_b,
                command.m_c,
                uniform_data,
                command.m_uniform_size);
            break;
        }
        case jegl_cmd_op::DRAW:
        {
            const void* uniform_data =
                command.m_uniform_size != 0
                ? cmd->m_uniform_arena.data() + command.m_uniform_offset
                : nullptr;

            render->draw(
                impl_context,
                command.m_a,
                command.m_b,
                command.m_c,
                uniform_data,
                command.m_uniform_size);
            break;
        }
        default:
            assert(!"Unknown command.");
            break;
        }
    }

    render->commit_end(impl_context);
}
